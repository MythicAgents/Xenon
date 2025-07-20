#pragma once
#ifndef INJECT_H
#define INJECT_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

typedef struct _Arg {
    char* value;
    size_t size;
    BOOL includeSize;
} Arg;

BOOL InjectCustomKit(_In_ PBYTE buffer, _In_ SIZE_T bufferLen, _In_ PCHAR InjectKit, _In_ SIZE_T kitLen, _Out_ PCHAR* outData);
BOOL InjectDefault(_In_ PBYTE buffer, _In_ SIZE_T bufferLen, _Out_ PCHAR* outData);


#endif // INCLUDE_CMD_EXECUTE_ASSEMBLY

#endif  //INJECT_H
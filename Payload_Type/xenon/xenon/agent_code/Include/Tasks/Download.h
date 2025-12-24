#pragma once
#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_DOWNLOAD
#define MAX_PATH 0x2000

typedef struct _FILE_DOWNLOAD {
    BOOL Initialized;           // Has download been started in Mythic
    HANDLE hFile;               // File handle
    CHAR TaskUuid[37];          // Track task UUID
    CHAR fileUuid[37];          // File UUID (36 + 1 for null terminator)
    CHAR filepath[MAX_PATH];    // Path to the file
    DWORD totalChunks;          // Total number of chunks
    UINT32 currentChunk;        // Current chunk number
    LARGE_INTEGER fileSize;     // Size of the file

    struct FILE_DOWNLOAD* Next;
} FILE_DOWNLOAD, *PFILE_DOWNLOAD;


BOOL DownloadSync(_In_ PCHAR TaskUuid, _In_ PPARSER Response);
VOID DownloadPush();
VOID Download(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

#endif //INCLUDE_CMD_DOWNLOAD

#endif  //DOWNLOAD_H
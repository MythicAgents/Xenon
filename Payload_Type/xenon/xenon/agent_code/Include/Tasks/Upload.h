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
    CHAR filepath[MAX_PATH];    // Path shown in Mythic UI (or UNC path for lateral movement)
    UINT32 totalChunks;         // Total number of chunks
    UINT32 currentChunk;        // Current chunk number
    LARGE_INTEGER fileSize;     // Size of the file

    /* Upload-complete callback - NULL for normal uploads.
     * Set by lateral-movement commands to run SCM/WMI execution after the
     * file is fully written.  Called with the task UUID and this struct; the
     * callback is responsible for sending PackageComplete / PackageError.
     * UploadFree() is called immediately after the callback returns. */
    void (*OnComplete)(PCHAR taskUuid, struct _FILE_UPLOAD* ctx);

    /* Lateral-movement context (populated when OnComplete != NULL). */
    PCHAR  LmTarget;       /* heap-alloc: hostname of the remote target              */
    PCHAR  LmUncPath;      /* heap-alloc: \\target\ADMIN$\Temp\<name>                */
    PCHAR  LmExecPath;     /* heap-alloc: C:\Windows\Temp\<name> [optional args]     */
    PCHAR  LmSvcName;      /* heap-alloc: psexec service name (NULL → random)        */
    PCHAR  LmCredUser;     /* heap-alloc: optional DOMAIN\\User                      */
    PCHAR  LmCredPass;     /* heap-alloc: optional cleartext password                */
    PCHAR  LmCredHash;     /* heap-alloc: optional NT hash for pass-the-hash         */
    HANDLE LmLogonToken;   /* LogonUserA token - REVERTED+CLOSED by UploadFree()     */

    struct FILE_UPLOAD* Next;
} FILE_UPLOAD, *PFILE_UPLOAD;

VOID Upload(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
BOOL UploadSync(_In_ PCHAR TaskUuid, _Inout_ PPARSER Response);
VOID UploadGetChunk(_In_ PFILE_UPLOAD File);
VOID UploadQueue(_In_ PFILE_UPLOAD File);
VOID UploadFree(_In_ PFILE_UPLOAD File);

#endif //INCLUDE_CMD_UPLOAD

#endif  //UPLOAD_H
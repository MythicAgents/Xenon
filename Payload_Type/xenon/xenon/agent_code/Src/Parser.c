/*
Built upon the Talon agent's binary serialization method - type, size, data 
Credits:
    @Cracked5pider
    https://github.com/HavocFramework/Talon/blob/main/Agent/Source/Parser.c
*/
#include "Parser.h"

#include "Xenon.h"
#include "Crypto.h"

VOID ParserNew(PPARSER parser, PBYTE Buffer, UINT32 size) 
{
    if (parser == NULL || Buffer == NULL || size == 0)
        return;

    parser->Original = (PCHAR)LocalAlloc(LPTR, size + 1);

    if (parser->Original == NULL)
        return;

    memcpy(parser->Original, Buffer, size);             // Copies memory from Buffer (so 'Buffer' needs to be freed outside)
    parser->Original[size]  = '\0';                     // Null terminate
    parser->Buffer          = parser->Original;
    parser->Length          = size;
    parser->OriginalSize    = size;
}

PPARSER ParserAlloc(SIZE_T size)
{
    // Allocate memory for the parser structure
    PPARSER parser = (PPARSER)LocalAlloc(LPTR, sizeof(PARSER));
    if (parser == NULL)
    {
        _err("Failed to allocate memory for parser. ERROR: %d", GetLastError());
        return NULL;
    }

    // Allocate memory for the buffer inside the parser
    parser->Original = (PBYTE)LocalAlloc(LPTR, size);
    if (parser->Original == NULL)
    {
        _err("Failed to allocate memory for buffer. ERROR: %d", GetLastError());
        LocalFree(parser); // Clean up the allocated parser if buffer allocation fails
        return NULL;
    }

    // Initialize the buffer to zero and setup the parser
    memset(parser->Original, 0, size);
    parser->Buffer          = parser->Original;
    parser->OriginalSize    = size;
    parser->Length          = size;

    return parser;
}

VOID ParserDataParse(PPARSER parser, char* buffer, int size) 
{
	*parser = (PARSER){ buffer, buffer, size, size };
}

BYTE ParserGetByte(PPARSER parser) 
{
    if (parser->Length < 1) 
        return 0;

    BYTE value = *parser->Buffer;
    parser->Buffer++;
    parser->Length--;

    return value;
}

// Return a 32 bit 4 byte integer and increment buffer and length of parser.
UINT32 ParserGetInt32(PPARSER parser) 
{
    if (parser->Length < 4)
        return 0;

    UINT32 value = 0;
    memcpy(&value, parser->Buffer, 4);
    parser->Buffer += 4;
    parser->Length -= 4;

    return BYTESWAP32(value);
}

// Return a 64 bit 8 byte integer and increment buffer and length of parser.
UINT64 ParserGetInt64(PPARSER parser) 
{
    if (parser->Length < 8)
        return 0;

    UINT64 value = 0;
    memcpy(&value, parser->Buffer, 8);
    parser->Buffer += 8;
    parser->Length -= 8;

    return BYTESWAP64(value);
}

// Read given buffer 
PBYTE ParserGetBytes(PPARSER parser, PUINT32 size) 
{
    UINT32  Length  = 0;
    PBYTE   outdata = NULL;

    if (parser->Length < 4)
        return NULL;

    // Read size of bytes
    if (*size == 0)
    {
        Length = ParserGetInt32(parser);
        *size = Length;
    }
    else
        Length = *size;

    outdata = parser->Buffer;
    if (outdata == NULL)
        return NULL;

    parser->Buffer += Length;
    parser->Length -= Length;

    return outdata;
}

// Return a given number of bytes as a string, and increment buffer and length of parser.
PCHAR ParserGetString(PPARSER parser, PSIZE_T size)
{
    return (PCHAR)ParserGetBytes(parser, size);
}

// Return a given number of bytes as a wide-char string, and increment buffer and length of parser.
PWCHAR ParserGetWString(PPARSER parser, PSIZE_T size)
{
    return (PWCHAR)ParserGetBytes(parser, size);
}

// Get a pointer to a certain sized memory region in the parser buffer
PCHAR ParserGetDataPtr(PPARSER parser, UINT32 size) 
{
    if (parser->Length < size)
        return NULL;

    PCHAR data = (PCHAR)parser->Buffer;
    parser->Buffer += size;
    parser->Length -= size;

    return data;
}

// [Returns char*] Copy a string to a given buffer to be null terminated
PCHAR ParserStringCopy(PPARSER parser, PSIZE_T size)
{
    PCHAR str = ParserGetString(parser, size);
    if (str == NULL)
        return NULL;
	
    PCHAR new = (PCHAR)LocalAlloc(LPTR, *size + 1);
    if (new == NULL)
        return NULL;

    memcpy(new, str, *size);
    new[*size] = '\0';
    
    return new;
}

// Copies parser string into a pre-allocated buffer (null-terminated)
BOOL ParserStringCopySafe(PPARSER parser, char* buffer, PSIZE_T size)
{
	if (parser->Length == 0)
		return FALSE;

	PCHAR ptr = ParserGetString(parser, size);
	if (!ptr)
		return FALSE;

	memcpy(buffer, ptr, *size + 1);
	buffer[*size] = '\0';

    return TRUE;
}

// Base64 decode parser data and adjusts size
BOOL ParserBase64Decode(PPARSER parser)
{
    BOOL success                    = FALSE;
    unsigned char *decoded_buffer   = NULL;
    SIZE_T decoded_length           = 0;

    if (parser == NULL || parser->Buffer == NULL) {
        _err("Invalid input parser");
        goto cleanup;
    }

    ///////////////////////////////////
    //    Base64 Decode Parser //////
    ///////////////////////////////////
    decoded_length = calculate_base64_decoded_size((const char*)parser->Buffer, parser->Length);  // Calculate exact size of decoded data
    
    decoded_buffer = (unsigned char *)malloc(decoded_length);
    if (!decoded_buffer) {
        _err("Memory allocation failed for base64 decoded buffer");
        goto cleanup;
    }

    int status;

    // print_bytes(parser->Buffer, parser->Length);

    status = base64_decode((const char*)parser->Buffer, parser->Length, decoded_buffer, &decoded_length);
    if (status != 0)
    {
        _err("Base64 decoding failed");
        goto cleanup;
    }


// TODO cleanup this
    memset(parser->Original, 0, parser->OriginalSize);
    memcpy(parser->Original, decoded_buffer, decoded_length);
    parser->Buffer          = parser->Original;
    parser->Length          = decoded_length;
    parser->OriginalSize    = decoded_length;

    success = TRUE;

cleanup:
    if (decoded_buffer)
        free(decoded_buffer);

    return success;
}


// BOOL ParserDecrypt(PPARSER parser)
// {
//     _dbg("Decrypting package of %d bytes", parser->Length);

//     if (!ParserBase64Decode(parser)) {
//         _err("Base64 decoding failed");
//         return FALSE;
//     }

//     // Check payload UUID
//     SIZE_T sizeUuid             = TASK_UUID_SIZE;
//     PCHAR receivedPayloadUUID   = NULL; 
//     receivedPayloadUUID         = ParserGetString(parser, &sizeUuid);
//     // Use memcmp to pass a strict size of bytes to compare
//     if (memcmp(receivedPayloadUUID, xenonConfig->agentID, TASK_UUID_SIZE) != 0) {
//         _err("Check-in payload UUID doesn't match what we have. Expected - %s : Received - %s", xenonConfig->agentID, receivedPayloadUUID);
//         return FALSE;
//     }
        
//     // Mythic AES decryption
//     if (xenonConfig->isEncryption)
//     {
//         if (!CryptoMythicDecryptParser(parser))
//             return FALSE;
//     }

//     _dbg("Decrypted Response");
//     print_bytes(parser->Buffer, parser->Length);

//     return TRUE;
// }

// Frees the data held in the parser
VOID ParserDestroy(PPARSER parser) 
{
    if (parser->Original) {
        memset(parser->Original, 0, parser->OriginalSize);
        LocalFree(parser->Original);
        parser->Original = NULL;
    }
}
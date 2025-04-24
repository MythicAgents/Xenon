#include "Utils.h"

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // for rand() and srand()
#include <time.h>    // for time()

#if defined(_MANUAL) || defined(_DEBUG)
void print_bytes(PBYTE buffer, SIZE_T length) {
    for (SIZE_T i = 0; i < length; i++) {
        printf("%02X", buffer[i]);  // Print each byte as two-digit hexadecimal
    }
    printf("\n");
}
#endif

#define HASH_SEED 0x811C9DC5
#define HASH_PRIME 0x01000193

UINT32 custom_hash(const char *data) 
{
    UINT32 hash = HASH_SEED;
    while (*data) {
        hash ^= (UINT8)(*data);
        hash *= HASH_PRIME;
        data++;
    }
    return hash;
}

// Function to generate a random number between min and max
INT RandomInt32(INT min, INT max)
{
    return (rand() % (max - min + 1)) + min;
}

/*
    Ref: https://github.com/vxunderground/VX-API/blob/main/VX-API/CreatePseudoRandomInteger.cpp
*/
INT PseudoRandomIntegerSubroutine(PULONG Context, INT max)
{
	return ((*Context = *Context * 1103515245 + 12345) % ((ULONG)max + 1));
}


BOOL xor_encode(char* data, size_t data_len, char* key, size_t key_len, char* output) {
    if (data == NULL || key == NULL || output == NULL) {
        return FALSE;
    }

    for (size_t i = 0; i < data_len; i++) {
        output[i] = data[i] ^ key[i % key_len]; // Use modulo to loop key
    }
}

size_t calculate_base64_encoded_size(size_t inlen)
{
    size_t ret;

    ret = inlen;
    if (inlen % 3 != 0)
        ret += 3 - (inlen % 3);
    ret /= 3;
    ret *= 4;

    return ret;
}

/*
    Ref: https://github.com/libtom/libtomcrypt/blob/develop/src/misc/base64/base64_encode.c
    LibTomCrypt, modular cryptographic library -- Tom St Denis
    SPDX-License-Identifier: Unlicense
*/

// Define constants for status codes
#define CRYPT_OK 0
#define CRYPT_INVALID_PACKET -1
#define CRYPT_BUFFER_OVERFLOW -2

/* Base64 and Base64 URL encoding tables */
static const char * const codes_base64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char * const codes_base64url =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* Base64 encoding modes */
enum mode {
    nopad = 0,
    pad = 1
};

/* Internal function for Base64 encoding */
static int s_base64_encode_internal(const unsigned char *in, unsigned long inlen,
                                    char *out, unsigned long *outlen,
                                    const char *codes, unsigned int mode) {
    unsigned long i, leven;
    char *p = out;

    leven = 3 * (inlen / 3);  // Process in chunks of 3 bytes
    for (i = 0; i < leven; i += 3) {
        *p++ = codes[(in[0] >> 2) & 0x3F];
        *p++ = codes[((in[0] & 3) << 4) | (in[1] >> 4) & 0x3F];
        *p++ = codes[((in[1] & 0xF) << 2) | (in[2] >> 6) & 0x3F];
        *p++ = codes[in[2] & 0x3F];
        in += 3;
    }

    // Handle remaining bytes and padding if required
    if (i < inlen) {
        unsigned a = in[0];
        unsigned b = (i + 1 < inlen) ? in[1] : 0;

        *p++ = codes[(a >> 2) & 0x3F];
        *p++ = codes[((a & 3) << 4) | (b >> 4) & 0x3F];
        if (mode & pad) {
            *p++ = (i + 1 < inlen) ? codes[(b & 0xF) << 2] : '=';
            *p++ = '=';
        } else {
            if (i + 1 < inlen) *p++ = codes[(b & 0xF) << 2];
        }
    }

    *p = '\0';  // Null-terminate the output
    *outlen = (unsigned long)(p - out);  // Set output length
    return CRYPT_OK;  // Success
}

/* Public function for standard Base64 encoding */
int base64_encode(const unsigned char *in, unsigned long inlen,
                  char *out, unsigned long *outlen) {
    return s_base64_encode_internal(in, inlen, out, outlen, codes_base64, pad);
}

/* Public function for Base64 URL encoding */
int base64url_encode(const unsigned char *in, unsigned long inlen,
                     char *out, unsigned long *outlen) {
    return s_base64_encode_internal(in, inlen, out, outlen, codes_base64url, nopad);
}

/*
    From base64_decode.c @https://github.com/libtom/libtomcrypt
*/

/* LTC_BASE64 */
static const unsigned char map_base64[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 253, 253, 255,
    255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 253, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
    255, 254, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,
      7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,
     19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
     37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255 };


/* LTC_BASE64_URL */
static const unsigned char map_base64url[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 253, 253, 255,
    255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 253, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255,
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
    255, 254, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,
      7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,
     19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255,  63,
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
     37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255
    };

// Internal decode function used for both base64 and base64url
static int s_base64_decode_internal(const char *in, unsigned long inlen,
                                    unsigned char *out, unsigned long *outlen,
                                    const unsigned char *map) {
    unsigned long t = 0, x = 0, y = 0, z = 0;
    unsigned char c;
    int g = 0;  // '=' padding counter

    for (x = 0; x < inlen; x++) {
        c = map[(unsigned char)in[x] & 0xFF];

        if (c == 254) {  // '=' padding
            g++;
            continue;
        }
        if (c == 253) continue;  // Ignore whitespace
        if (c == 255) return CRYPT_INVALID_PACKET;  // Invalid character

        // If padding ('=') appeared earlier, reject further non-padding chars.
        if (g > 0) return CRYPT_INVALID_PACKET;

        t = (t << 6) | c;  // Shift bits and add current value

        if (++y == 4) {
            if (z + 3 > *outlen) return CRYPT_BUFFER_OVERFLOW;  // Check buffer size
            out[z++] = (unsigned char)((t >> 16) & 255);
            out[z++] = (unsigned char)((t >> 8) & 255);
            out[z++] = (unsigned char)(t & 255);
            y = t = 0;  // Reset counters
        }
    }

    // Handle remaining bits if input isn't a multiple of 4
    if (y != 0) {
        if (y == 1) return CRYPT_INVALID_PACKET;  // Incomplete byte group

        t <<= (6 * (4 - y));  // Shift bits to complete group

        if (z + y - 1 > *outlen) return CRYPT_BUFFER_OVERFLOW;  // Check buffer

        if (y >= 2) out[z++] = (unsigned char)((t >> 16) & 255);
        if (y == 3) out[z++] = (unsigned char)((t >> 8) & 255);
    }

    // Verify padding rules (only 0, 1, or 2 '=' allowed at the end)
    if (g > 2 || (g > 0 && (inlen % 4 != 0))) {
        return CRYPT_INVALID_PACKET;
    }

    *outlen = z;  // Set final decoded length
    return CRYPT_OK;
}

// Public API functions for Base64 and Base64URL decoding
int base64_decode(const char *in, unsigned long inlen,
                  unsigned char *out, unsigned long *outlen) {
    return s_base64_decode_internal(in, inlen, out, outlen, map_base64);
}

int base64url_decode(const char *in, unsigned long inlen,
                     unsigned char *out, unsigned long *outlen) {
    return s_base64_decode_internal(in, inlen, out, outlen, map_base64url);
}


SIZE_T calculate_base64_decoded_size(const char *in, SIZE_T inLen)
{
    SIZE_T len;
    SIZE_T ret;
    SIZE_T i;

    if (in == NULL)
        return 0;

    len = inLen;
    ret = len / 4 * 3;

    for (i = len; i-- > 0;)
    {
        if (in[i] == '=')
        {
            ret--;
        }
        else
        {
            break;
        }
    }

    return ret;
}

VOID addInt32ToBuffer_LE(PBYTE buffer, UINT32 value)
{
    buffer[0] = (value) & 0xFF;         
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
}

VOID addInt32ToBuffer(PBYTE buffer, UINT32 value)
{
    (buffer)[0] = (value >> 24) & 0xFF;
    (buffer)[1] = (value >> 16) & 0xFF;
    (buffer)[2] = (value >> 8) & 0xFF;
    (buffer)[3] = (value) & 0xFF;
}

VOID addInt64ToBuffer(PBYTE buffer, UINT64 value)
{
    buffer[7] = value & 0xFF;
    value >>= 8;

    buffer[6] = value & 0xFF;
    value >>= 8;

    buffer[5] = value & 0xFF;
    value >>= 8;

    buffer[4] = value & 0xFF;
    value >>= 8;

    buffer[3] = value & 0xFF;
    value >>= 8;

    buffer[2] = value & 0xFF;
    value >>= 8;

    buffer[1] = value & 0xFF;
    value >>= 8;

    buffer[0] = value & 0xFF;
}

// Function to convert char to wchar_t using Windows API
WCHAR *char_to_wchar(const char *input)
{
    if (input == NULL)
    {
        return NULL;
    }

    // Get the required buffer size for the wide string (including null terminator)
    int len = MultiByteToWideChar(CP_ACP, 0, input, -1, NULL, 0);
    if (len == 0)
    {
        return NULL; // Handle error: conversion failed, possibly due to invalid input
    }

    // Allocate memory for the wide string
    WCHAR *output = (WCHAR *)malloc(len * sizeof(WCHAR));
    if (output == NULL)
    {
        return NULL; // Handle error: memory allocation failed
    }

    // Convert the input string to a wide string
    int result = MultiByteToWideChar(CP_ACP, 0, input, -1, output, len);
    if (result == 0)
    {
        free(output); // Free allocated memory in case of conversion failure
        return NULL;  // Handle error: conversion failed during the actual call
    }

    return output; // Return the converted wide string
}

// Returns the last Win32 error, in string format. Returns an empty string if there is no error.
WCHAR *GetLastErrorAsStringW()
{
    // Get the error message ID, if any.
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0)
    {
        return NULL; // No error message has been recorded
    }

    LPWSTR messageBuffer = NULL;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

    // Copy the error message into a std::string.
    WCHAR *message = messageBuffer;

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

// Returns the last Win32 error, in string format. Returns an empty string if there is no error.
CHAR *GetLastErrorAsStringA()
{
    // Get the error message ID, if any.
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0)
    {
        return NULL; // No error message has been recorded
    }

    LPSTR messageBuffer = NULL;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    // Copy the error message into a std::string.
    CHAR *message = messageBuffer;

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}
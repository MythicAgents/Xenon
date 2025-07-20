#pragma once
#ifndef UTILS_H
#define UTILS_H


#include <windows.h>
#include "Debug.h"

#ifdef __MINGW64__
#define BYTESWAP32(x) __builtin_bswap32(x)
#define BYTESWAP64(x) __builtin_bswap64(x)
#endif
#ifdef _MSC_VER
#define BYTESWAP32(x) _byteswap_ulong(x)
#define BYTESWAP64(x) _byteswap_uint64(x)
#endif

#define SEC_DATA        __attribute__( ( section( ".data" ) ) )

UINT32 custom_hash(const char *data);

INT PseudoRandomIntegerSubroutine(PULONG Context, INT max);

INT RandomInt32(INT min, INT max);

void print_bytes(PBYTE buffer, SIZE_T length);

SIZE_T calculate_base64_decoded_size(const char *in, SIZE_T inLen);
size_t calculate_base64_encoded_size(size_t inlen);


// Functions from @https://github.com/libtom/libtomcrypt
/* ---- LTC_BASE64 Routines ---- */
int base64_encode(const unsigned char *in,  unsigned long inlen,
                                 char *out, unsigned long *outlen);

int base64url_encode(const unsigned char *in,  unsigned long inlen,
                                    char *out, unsigned long *outlen);

int base64_decode(const char *in,  unsigned long inlen,
                        unsigned char *out, unsigned long *outlen);

int base64url_decode(const char *in,  unsigned long inlen,
                        unsigned char *out, unsigned long *outlen);
/*---*/

VOID addInt64ToBuffer(PBYTE buffer, UINT64 value);
VOID addInt32ToBuffer(PBYTE buffer, UINT32 value);
VOID addInt32ToBuffer_LE(PBYTE buffer, UINT32 value);


WCHAR *char_to_wchar(const char *input);

WCHAR *GetLastErrorAsStringW();
CHAR *GetLastErrorAsStringA();

BOOL xor_encode(char* data, size_t data_len, char* key, size_t key_len, char* output);

#endif
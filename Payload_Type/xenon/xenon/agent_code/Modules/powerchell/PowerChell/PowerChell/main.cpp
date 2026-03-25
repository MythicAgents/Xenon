/**
 * @brief Post-ex DLL for PowerChell
 * @ref - Original author @scrt https://github.com/scrt/PowerChell
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "PowerChellLib/powershell.h"

typedef struct _POWERCHELL_ARGUMENTS
{
    UINT32 Length;
    char   Buffer[];
} POWERCHELL_ARGUMENTS, *PPOWERCHELL_ARGUMENTS;

void PowerChellMain(LPVOID lpCmdLine);

#ifdef _WINDLL

void StartPowerShellInNewConsole(LPVOID lpCmdLine);

#pragma warning(disable : 4100)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    HANDLE hThread = NULL;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        /* Run PowerChell in new thread */
        hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StartPowerShellInNewConsole, lpReserved, 0, NULL);
        if (hThread != NULL)
        {
            WaitForSingleObject(hThread, INFINITE);
            CloseHandle(hThread);
        }
        
        break;
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

void StartPowerShellInNewConsole(LPVOID lpCmdLine)
{
    /* Allocate console for printing output */
    AllocConsole();

    /* Execute PowerShell script */
    PowerChellMain(lpCmdLine);

    /* Clean up */
    FreeConsole();
}

#else // _WINDLL
/* Debugging */
int main()
{
    PowerChellMain();

    return 0;
}
#endif

/**
 * @brief PowerChell main function
 * @param lpCmdLine PowerShell Arguments
 */
void PowerChellMain(LPVOID lpCmdLine)
{
    POWERCHELL_ARGUMENTS * Arguments;

    /* Cast lpCmdLine to ARG */
    Arguments = (POWERCHELL_ARGUMENTS *)lpCmdLine;
    
    if (Arguments->Length > 0 )
    {
        /* Convert to wide string for PowerShell */
        int nWide = MultiByteToWideChar(CP_UTF8, 0, Arguments->Buffer, -1, NULL, 0);
        if (nWide > 0)
        {
            LPWSTR pwszScript = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (size_t)nWide * sizeof(WCHAR));
            if (pwszScript != NULL)
            {
                if (MultiByteToWideChar(CP_UTF8, 0, Arguments->Buffer, -1, pwszScript, nWide) > 0) {
                    /* Execute PowerShell script */
                    ExecutePowerShellScript(pwszScript);
                }

                HeapFree(GetProcessHeap(), 0, pwszScript);
            }
        }
    }
}

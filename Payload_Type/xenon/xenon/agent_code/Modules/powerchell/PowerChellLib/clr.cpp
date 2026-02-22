#include "common.h"
#include "clr.h"

#ifdef __MINGW32__
/* Define IID__AppDomain here so the linker sees it (mscorlib.h only declares it; avoid C/C++ link issues) */
extern "C" const IID IID__AppDomain = { 0x05F696DC, 0x2B29, 0x3663, { 0xAD, 0x8B, 0xC4, 0x38, 0x9C, 0xF2, 0xA7, 0x13 } };
#endif

BOOL clr::InitializeCommonLanguageRuntime(PCLR_CONTEXT pClrContext, mscorlib::_AppDomain** ppAppDomain)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    ICLRMetaHost* pMetaHost = NULL;
    ICLRRuntimeInfo* pRuntimeInfo = NULL;
    ICorRuntimeHost* pRuntimeHost = NULL;
    IUnknown* pAppDomainThunk = NULL;
    BOOL bIsLoadable;

    mscorlib::_AppDomain* pAppDomain = NULL;

    hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, reinterpret_cast<PVOID*>(&pMetaHost));
    EXIT_ON_HRESULT_ERROR(L"CLRCreateInstance", hr);

    hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_ICLRRuntimeInfo, reinterpret_cast<PVOID*>(&pRuntimeInfo));
    EXIT_ON_HRESULT_ERROR(L"IMetaHost->GetRuntime", hr);

    hr = pRuntimeInfo->IsLoadable(&bIsLoadable);
    EXIT_ON_HRESULT_ERROR(L"IRuntimeInfo->IsLoadable", hr);

    hr = pRuntimeInfo->GetInterface(CLSID_CorRuntimeHost, IID_ICorRuntimeHost, reinterpret_cast<PVOID*>(&pRuntimeHost));
    EXIT_ON_HRESULT_ERROR(L"IRuntimeInfo->GetInterface", hr);

    hr = pRuntimeHost->Start();
    EXIT_ON_HRESULT_ERROR(L"IRuntimeHost->Start", hr);

    hr = pRuntimeHost->CreateDomain(APP_DOMAIN, nullptr, &pAppDomainThunk);
    EXIT_ON_HRESULT_ERROR(L"IRuntimeHost->CreateDomain", hr);

    hr = pAppDomainThunk->QueryInterface(IID__AppDomain, reinterpret_cast<void**>(&pAppDomain));
    EXIT_ON_HRESULT_ERROR(L"IAppDomainThunk->QueryInterface", hr);

    pClrContext->pMetaHost = pMetaHost;
    pClrContext->pRuntimeInfo = pRuntimeInfo;
    pClrContext->pRuntimeHost = pRuntimeHost;
    pClrContext->pAppDomainThunk = pAppDomainThunk;
    *ppAppDomain = pAppDomain;
    bResult = TRUE;

exit:
    if (!bResult && pAppDomain) pAppDomain->Release();
    if (!bResult && pAppDomainThunk) pAppDomainThunk->Release();
    if (!bResult && pRuntimeHost) pRuntimeHost->Release();
    if (!bResult && pRuntimeInfo) pRuntimeInfo->Release();
    if (!bResult && pMetaHost) pMetaHost->Release();

    return bResult;
}

void clr::DestroyCommonLanguageRuntime(PCLR_CONTEXT pClrContext, mscorlib::_AppDomain* pAppDomain)
{
    if (pAppDomain) pAppDomain->Release();
    if (pClrContext->pAppDomainThunk) pClrContext->pAppDomainThunk->Release();
    if (pClrContext->pRuntimeHost) pClrContext->pRuntimeHost->Release();
    if (pClrContext->pRuntimeInfo) pClrContext->pRuntimeInfo->Release();
    if (pClrContext->pMetaHost) pClrContext->pMetaHost->Release();
}

BOOL clr::FindAssemblyPath(LPCWSTR pwszAssemblyName, LPWSTR* ppwszAssemblyPath)
{
    BOOL bResult = FALSE;
    WIN32_FIND_DATA ffd = { 0 };
    LPCWSTR pwszAssemblyFolderPath = L"C:\\Windows\\Microsoft.NET\\assembly\\GAC_MSIL";
    LPWSTR pwszSearchPath = NULL;
    LPWSTR pwszAssemblyPath = NULL;
    HANDLE hFind = NULL;
    HANDLE hAssemblyFile = NULL;

    pwszSearchPath = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_PATH * sizeof(*pwszSearchPath));
    EXIT_ON_WIN32_ERROR(L"HeapAlloc", pwszSearchPath == NULL);

    pwszAssemblyPath = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_PATH * sizeof(*pwszAssemblyPath));
    EXIT_ON_WIN32_ERROR(L"HeapAlloc", pwszAssemblyPath == NULL);

    swprintf_s(pwszSearchPath, MAX_PATH, L"%ws\\%ws\\*", pwszAssemblyFolderPath, pwszAssemblyName);
    
    hFind = FindFirstFileW(pwszSearchPath, &ffd);
    EXIT_ON_WIN32_ERROR(L"FindFirstFileW", hFind == INVALID_HANDLE_VALUE);

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            swprintf_s(pwszAssemblyPath, MAX_PATH, L"%ws\\%ws\\%ws\\%ws.dll", pwszAssemblyFolderPath, pwszAssemblyName, ffd.cFileName, pwszAssemblyName);
            hAssemblyFile = CreateFileW(pwszAssemblyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hAssemblyFile != INVALID_HANDLE_VALUE)
            {
                bResult = TRUE;
                CloseHandle(hAssemblyFile);
                break;
            }
        }
    } while (FindNextFileW(hFind, &ffd) != 0);

    if (!bResult) goto exit;

    *ppwszAssemblyPath = pwszAssemblyPath;

exit:
    if (hFind && hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
    if (!bResult && pwszAssemblyPath) HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pwszAssemblyPath);
    if (pwszSearchPath) HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pwszSearchPath);

    return bResult;
}

BOOL clr::GetAssembly(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, mscorlib::_Assembly** ppAssembly)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    LONG lAssembliesLowerBound, lAssembliesUpperBound;
    LONG lTargetAssemblyNameLength, lCurrentAssemblyNameLength, lAssemblyNameIndex;
    BSTR bstrAssemblyFullName;
    SAFEARRAY* pLoadedAssembliesArray = NULL;
    mscorlib::_Assembly** ppLoadedAssemblies = NULL;

    lTargetAssemblyNameLength = (LONG)wcslen(pwszAssemblyName);

    hr = pAppDomain->GetAssemblies(&pLoadedAssembliesArray);
    EXIT_ON_HRESULT_ERROR(L"AppDomain->GetAssemblies", hr);

    SafeArrayGetLBound(pLoadedAssembliesArray, 1, &lAssembliesLowerBound);
    SafeArrayGetUBound(pLoadedAssembliesArray, 1, &lAssembliesUpperBound);

    hr = SafeArrayAccessData(pLoadedAssembliesArray, (void**)&ppLoadedAssemblies);
    EXIT_ON_HRESULT_ERROR(L"SafeArrayAccessData", hr);

    for (int i = 0; i < lAssembliesUpperBound - lAssembliesLowerBound + 1; i++)
    {
        bstrAssemblyFullName = NULL;
        hr = ppLoadedAssemblies[i]->get_FullName(&bstrAssemblyFullName);

        if (SUCCEEDED(hr))
        {
            //
            // If the target assembly name is 'Some.Assembly.Name', check whether
            // the current assembly's full name starts with 'Some.Assembly.Name,'.
            // As an example, the full name of the 'System.Management.Automation'
            // assembly would be something like:
            //   System.Management.Automation, Version=3.0.0.0, Culture=neutral, 
            //   PublicKeyToken=31bf3856ad364e35
            //

            lCurrentAssemblyNameLength = (LONG)wcslen(bstrAssemblyFullName);

            if (lCurrentAssemblyNameLength > lTargetAssemblyNameLength)
            {
                for (lAssemblyNameIndex = 0; lAssemblyNameIndex < lTargetAssemblyNameLength; lAssemblyNameIndex++)
                {
                    if (pwszAssemblyName[lAssemblyNameIndex] != bstrAssemblyFullName[lAssemblyNameIndex])
                        break;
                }

                if (lAssemblyNameIndex == lTargetAssemblyNameLength)
                {
                    if (bstrAssemblyFullName[lAssemblyNameIndex] == L',')
                    {
                        bResult = TRUE;
                        *ppAssembly = ppLoadedAssemblies[i];
                        break;
                    }
                }
            }

            SysFreeString(bstrAssemblyFullName);
        }
    }

exit:
    if (pLoadedAssembliesArray) SafeArrayDestroy(pLoadedAssembliesArray);

    return bResult;
}

BOOL clr::LoadAssembly(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, mscorlib::_Assembly** ppAssembly)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    DWORD dwBytesRead;
    HANDLE hAssemblyFile = NULL;
    LPWSTR pwszAssemblyPath = NULL;
    LARGE_INTEGER liAssemblyFileSize = { 0 };
    SAFEARRAYBOUND sab = { 0 };
    SAFEARRAY* pSafeAssembly = NULL;
    mscorlib::_Assembly* pAssembly = NULL;

    // Check if assembly is already loaded
    if (!clr::GetAssembly(pAppDomain, pwszAssemblyName, &pAssembly))
    {
        if (!clr::FindAssemblyPath(pwszAssemblyName, &pwszAssemblyPath))
            goto exit;

        hAssemblyFile = CreateFileW(pwszAssemblyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        EXIT_ON_WIN32_ERROR(L"CreateFileW", hAssemblyFile == INVALID_HANDLE_VALUE);
        EXIT_ON_WIN32_ERROR(L"GetFileSizeEx", GetFileSizeEx(hAssemblyFile, &liAssemblyFileSize) == FALSE);

        sab = { (ULONG)liAssemblyFileSize.QuadPart, 0 };
        pSafeAssembly = SafeArrayCreate(VT_UI1, 1, &sab);

        EXIT_ON_WIN32_ERROR(L"ReadFile", ReadFile(hAssemblyFile, pSafeAssembly->pvData, (DWORD)liAssemblyFileSize.QuadPart, &dwBytesRead, NULL) == FALSE);

        hr = pAppDomain->Load_3(pSafeAssembly, &pAssembly);
        EXIT_ON_HRESULT_ERROR(L"AppDomain->Load_3", hr);
    }

    *ppAssembly = pAssembly;
    bResult = TRUE;

exit:
    if (pSafeAssembly) SafeArrayDestroy(pSafeAssembly);
    if (hAssemblyFile && hAssemblyFile != INVALID_HANDLE_VALUE) CloseHandle(hAssemblyFile);
    if (pwszAssemblyPath) HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, pwszAssemblyPath);

    return bResult;
}

BOOL clr::CreateInstance(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszClassName, VARIANT* pvtInstance)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    BSTR bstrClassName = SysAllocString(pwszClassName);
    VARIANT vtInstance = { 0 };
    mscorlib::_Assembly* pAssembly = NULL;

    if (!clr::LoadAssembly(pAppDomain, pwszAssemblyName, &pAssembly))
        goto exit;

    hr = pAssembly->CreateInstance(bstrClassName, &vtInstance);
    EXIT_ON_HRESULT_ERROR(L"Assembly->CreateInstance", hr);

    memcpy_s(pvtInstance, sizeof(*pvtInstance), &vtInstance, sizeof(vtInstance));
    bResult = TRUE;

exit:
    if (bstrClassName) SysFreeString(bstrClassName);
    if (pAssembly) pAssembly->Release();

    return bResult;
}

BOOL clr::GetType(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszTypeFullName, mscorlib::_Type** ppType)
{
    HRESULT hr;
    BOOL bResult = FALSE;
    BSTR bstrTypeFullName = SysAllocString(pwszTypeFullName);
    mscorlib::_Assembly* pAssembly = NULL;
    mscorlib::_Type* pType = NULL;

    if (!clr::LoadAssembly(pAppDomain, pwszAssemblyName, &pAssembly))
        goto exit;

    hr = pAssembly->GetType_2(bstrTypeFullName, &pType);
    EXIT_ON_HRESULT_ERROR(L"Assembly->GetType_2", hr);
    EXIT_ON_NULL_POINTER(pwszTypeFullName, pType);

    *ppType = pType;
    bResult = TRUE;

exit:
    if (bstrTypeFullName) SysFreeString(bstrTypeFullName);
    if (pAssembly) pAssembly->Release();

    return bResult;
}

BOOL clr::GetProperty(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, LPCWSTR pwszPropertyName, mscorlib::_PropertyInfo** ppPropertyInfo)
{
    HRESULT hr;
    BOOL bResult = FALSE;
    BSTR bstrPropertyName = SysAllocString(pwszPropertyName);
    mscorlib::_PropertyInfo* pPropertyInfo = NULL;

    hr = pType->GetProperty(bstrPropertyName, static_cast<::BindingFlags>(bindingFlags), &pPropertyInfo);
    EXIT_ON_HRESULT_ERROR(L"Type->GetProperty", hr);
    EXIT_ON_NULL_POINTER(pwszPropertyName, pPropertyInfo);

    *ppPropertyInfo = pPropertyInfo;
    bResult = TRUE;

exit:
    if (bstrPropertyName) SysFreeString(bstrPropertyName);

    return bResult;
}

BOOL clr::GetPropertyValue(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, VARIANT vtObject, LPCWSTR pwszPropertyName, VARIANT* pvtPropertyValue)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    VARIANT vtPropertyValue = { 0 };
    mscorlib::_PropertyInfo* pPropertyInfo = NULL;

    if (!clr::GetProperty(pType, bindingFlags, pwszPropertyName, &pPropertyInfo))
        goto exit;

    hr = pPropertyInfo->GetValue(vtObject, NULL, &vtPropertyValue);
    EXIT_ON_HRESULT_ERROR(L"PropertyInfo->GetValue", hr);

    memcpy_s(pvtPropertyValue, sizeof(*pvtPropertyValue), &vtPropertyValue, sizeof(vtPropertyValue));
    bResult = TRUE;

exit:
    if (pPropertyInfo) pPropertyInfo->Release();

    return bResult;
}

BOOL clr::GetField(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, LPCWSTR pwszFieldName, mscorlib::_FieldInfo** ppFieldInfo)
{
    HRESULT hr;
    BOOL bResult = FALSE;
    BSTR bstrFieldName = SysAllocString(pwszFieldName);
    mscorlib::_FieldInfo* pFieldInfo = NULL;

    hr = pType->GetField(bstrFieldName, static_cast<::BindingFlags>(bindingFlags), &pFieldInfo);
    EXIT_ON_HRESULT_ERROR(L"Type->GetField", hr);
    EXIT_ON_NULL_POINTER(pwszFieldName, pFieldInfo);

    *ppFieldInfo = pFieldInfo;
    bResult = TRUE;

exit:
    if (bstrFieldName) SysFreeString(bstrFieldName);

    return bResult;
}

BOOL clr::GetFieldValue(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, VARIANT vtObject, LPCWSTR pwszFieldName, VARIANT* pvtFieldValue)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    VARIANT vtValue = { 0 };
    mscorlib::_FieldInfo* pFieldInfo = NULL;

    if (!clr::GetField(pType, bindingFlags, pwszFieldName, &pFieldInfo))
        goto exit;

    hr = pFieldInfo->GetValue(vtObject, &vtValue);
    EXIT_ON_HRESULT_ERROR(L"FieldInfo->GetValue", hr);

    memcpy_s(pvtFieldValue, sizeof(*pvtFieldValue), &vtValue, sizeof(vtValue));
    bResult = TRUE;

exit:
    if (pFieldInfo) pFieldInfo->Release();

    return bResult;
}

BOOL clr::GetMethod(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, LPCWSTR pwszMethodName, LONG lNbArg, mscorlib::_MethodInfo** ppMethodInfo)
{
    HRESULT hr;
    BOOL bResult = FALSE;
    SAFEARRAY* pMethods = NULL;
    mscorlib::_MethodInfo* pMethodInfo;

    hr = pType->GetMethods(static_cast<::BindingFlags>(bindingFlags), &pMethods);
    EXIT_ON_HRESULT_ERROR(L"Type->GetMethods", hr);

    if (!clr::FindMethodInArray(pMethods, pwszMethodName, lNbArg, &pMethodInfo))
        goto exit;

    *ppMethodInfo = pMethodInfo;
    bResult = TRUE;

exit:
    if (pMethods) SafeArrayDestroy(pMethods);

    return bResult;
}

BOOL clr::InvokeMethod(mscorlib::_MethodInfo* pMethodInfo, VARIANT vtObject, SAFEARRAY* pParameters, VARIANT* pvtResult)
{
    HRESULT hr;
    BOOL bResult = FALSE;
    
    hr = pMethodInfo->Invoke_3(vtObject, pParameters, pvtResult);
    EXIT_ON_HRESULT_ERROR(L"MethodInfo->Invoke_3", hr);

    bResult = TRUE;

exit:
    return bResult;
}

BOOL clr::FindMethodInArray(SAFEARRAY* pMethods, LPCWSTR pwszMethodName, LONG lNbArgs, mscorlib::_MethodInfo** ppMethodInfo)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    LONG lMethodsLowerBound, lMethodsUpperBound;
    LONG lParametersLowerBound, lParametersUpperBound;
    BSTR bstrMethodName;
    SAFEARRAY* pParameters = NULL;
    mscorlib::_MethodInfo** ppMethods = NULL;
    mscorlib::_MethodInfo* pTargetMethodInfo = NULL;

    SafeArrayGetLBound(pMethods, 1, &lMethodsLowerBound);
    SafeArrayGetUBound(pMethods, 1, &lMethodsUpperBound);

    hr = SafeArrayAccessData(pMethods, (void**)&ppMethods);
    EXIT_ON_HRESULT_ERROR(L"SafeArrayAccessData", hr);

    for (int i = 0; i < lMethodsUpperBound - lMethodsLowerBound + 1; i++)
    {
        bstrMethodName = NULL;
        hr = ppMethods[i]->get_name(&bstrMethodName);

        if (SUCCEEDED(hr))
        {
            if (_wcsicmp(bstrMethodName, pwszMethodName) == 0)
            {
                hr = ppMethods[i]->GetParameters(&pParameters);

                if (SUCCEEDED(hr))
                {
                    SafeArrayGetLBound(pParameters, 1, &lParametersLowerBound);
                    SafeArrayGetUBound(pParameters, 1, &lParametersUpperBound);

                    if (lParametersUpperBound - lParametersLowerBound + 1 == lNbArgs)
                    {
                        pTargetMethodInfo = ppMethods[i];
                        break;
                    }

                    SafeArrayDestroy(pParameters);
                }
            }

            SysFreeString(bstrMethodName);
        }
    }

    if (!pTargetMethodInfo)
    {
        PRINT_ERROR(L"Could not find a method named '%ws' with %d arguments.\n", pwszMethodName, lNbArgs);
        goto exit;
    }

    bResult = TRUE;
    *ppMethodInfo = pTargetMethodInfo;

exit:
    return bResult;
}

BOOL clr::PrepareMethod(mscorlib::_AppDomain* pAppDomain, VARIANT* pvtMethodHandle)
{
    BOOL bResult = FALSE;
    LONG lArgumentIndex;
    SAFEARRAY* pPrepareMethodArguments = NULL;
    VARIANT vtEmpty = { 0 };
    VARIANT vtResult = { 0 };
    mscorlib::_Type* pRuntimeHelpersType = NULL;
    mscorlib::_MethodInfo* pPrepareMethod = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_RUNTIME, L"System.Runtime.CompilerServices.RuntimeHelpers", &pRuntimeHelpersType))
        goto exit;

    if (!clr::GetMethod(pRuntimeHelpersType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_STATIC), L"PrepareMethod", 1, &pPrepareMethod))
        goto exit;

    pPrepareMethodArguments = SafeArrayCreateVector(VT_VARIANT, 0, 1);

    lArgumentIndex = 0;
    SafeArrayPutElement(pPrepareMethodArguments, &lArgumentIndex, pvtMethodHandle);

    if (!clr::InvokeMethod(pPrepareMethod, vtEmpty, pPrepareMethodArguments, &vtResult))
        goto exit;

    bResult = TRUE;

exit:
    if (pPrepareMethodArguments) SafeArrayDestroy(pPrepareMethodArguments);

    if (pPrepareMethod) pPrepareMethod->Release();
    if (pRuntimeHelpersType) pRuntimeHelpersType->Release();

    VariantClear(&vtResult);

    return bResult;
}

BOOL clr::GetFunctionPointer(mscorlib::_AppDomain* pAppDomain, VARIANT* pvtMethodHandle, PULONG_PTR pFunctionPointer)
{
    BOOL bResult = FALSE;
    VARIANT vtFunctionPointer = { 0 };
    mscorlib::_Type* pRuntimeMethodHandleType = NULL;
    mscorlib::_MethodInfo* pGetFunctionPointerInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_RUNTIME, L"System.RuntimeMethodHandle", &pRuntimeMethodHandleType))
        goto exit;

    if (!clr::GetMethod(pRuntimeMethodHandleType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"GetFunctionPointer", 0, &pGetFunctionPointerInfo))
        goto exit;

    if (!clr::InvokeMethod(pGetFunctionPointerInfo, *pvtMethodHandle, NULL, &vtFunctionPointer))
        goto exit;

    *pFunctionPointer = vtFunctionPointer.ullVal;
    bResult = TRUE;

exit:
    if (pGetFunctionPointerInfo) pGetFunctionPointerInfo->Release();
    if (pRuntimeMethodHandleType) pRuntimeMethodHandleType->Release();

    return bResult;
}

BOOL clr::GetJustInTimeMethodAddress(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszClassName, LPCWSTR pwszMethodName, DWORD dwNbArgs, PULONG_PTR pMethodAddress)
{
    BOOL bResult = FALSE;
    VARIANT vtMethodHandlePtr = { 0 };
    VARIANT vtMethodHandleVal = { 0 };
    mscorlib::_Type* pType = NULL;
    mscorlib::_Type* pMethodInfoType = NULL;
    mscorlib::_MethodInfo* pTargetMethodInfo = NULL;

    // Here, we include as many binding flags as we can so that we can list
    // ALL the methods of the target class.
    mscorlib::BindingFlags flags = static_cast<mscorlib::BindingFlags>(
        mscorlib::BindingFlags::BindingFlags_Instance |
        mscorlib::BindingFlags::BindingFlags_Static |
        mscorlib::BindingFlags::BindingFlags_Public |
        mscorlib::BindingFlags::BindingFlags_NonPublic |
        mscorlib::BindingFlags::BindingFlags_DeclaredOnly
    );

    if (!clr::GetType(pAppDomain, pwszAssemblyName, pwszClassName, &pType))
        goto exit;

    if (!clr::GetMethod(pType, flags, pwszMethodName, dwNbArgs, &pTargetMethodInfo))
        goto exit;

    //
    // The method for obtaining the MethodHandle from the MethodInfo object is
    // taken from this article.
    // 
    // Credit:
    //   - https://www.outflank.nl/blog/2024/02/01/unmanaged-dotnet-patching/
    //

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_REFLECTION, L"System.Reflection.MethodInfo", &pMethodInfoType))
        goto exit;

    vtMethodHandlePtr.vt = VT_UNKNOWN;
    vtMethodHandlePtr.punkVal = pTargetMethodInfo;

    if (!clr::GetPropertyValue(pMethodInfoType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), vtMethodHandlePtr, L"MethodHandle", &vtMethodHandleVal))
        goto exit;

    //
    // Next, we can invoke 'RuntimeHelpers.PrepareMethod' to make sure the target
    // method is JIT-compiled, and finally get its effective address.
    //
    // Credit:
    //   - https://github.com/calebstewart/bypass-clm
    //   - https://www.mdsec.co.uk/2020/08/massaging-your-clr-preventing-environment-exit-in-in-process-net-assemblies/
    //

    if (!PrepareMethod(pAppDomain, &vtMethodHandleVal))
        goto exit;

    if (!GetFunctionPointer(pAppDomain, &vtMethodHandleVal, pMethodAddress))
        goto exit;

    bResult = TRUE;

exit:
    if (pTargetMethodInfo) pTargetMethodInfo->Release();
    if (pType) pType->Release();
    if (pMethodInfoType) pMethodInfoType->Release();

    VariantClear(&vtMethodHandleVal);
    VariantClear(&vtMethodHandlePtr);

    return bResult;
}

BOOL dotnet::System_Object_GetType(mscorlib::_AppDomain* pAppDomain, VARIANT vtObject, VARIANT* pvtObjectType)
{
    BOOL bResult = FALSE;
    VARIANT vtGetTypeResult = { 0 };
    mscorlib::_Type* pObjectType = NULL;
    mscorlib::_MethodInfo* pGetTypeMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_RUNTIME, L"System.Object", &pObjectType))
        goto exit;

    if (!clr::GetMethod(pObjectType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"GetType", 0, &pGetTypeMethodInfo))
        goto exit;

    if (!clr::InvokeMethod(pGetTypeMethodInfo, vtObject, NULL, &vtGetTypeResult))
        goto exit;

    memcpy_s(pvtObjectType, sizeof(*pvtObjectType), &vtGetTypeResult, sizeof(vtGetTypeResult));
    bResult = TRUE;

exit:
    if (pGetTypeMethodInfo) pGetTypeMethodInfo->Release();
    if (pObjectType) pObjectType->Release();

    return bResult;
}

BOOL dotnet::System_Type_GetProperty(mscorlib::_AppDomain* pAppDomain, VARIANT vtTypeObject, LPCWSTR pwszPropertyName, VARIANT* pvtPropertyInfo)
{
    BOOL bResult = FALSE;
    LONG lArgumentIndex;
    VARIANT vtPropertyName = { 0 };
    VARIANT vtPropertyInfo = { 0 };
    SAFEARRAY* pGetPropertyArguments = NULL;
    mscorlib::_Type* pTypeType = NULL;
    mscorlib::_MethodInfo* pGetPropertyMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_RUNTIME, L"System.Type", &pTypeType))
        goto exit;

    if (!clr::GetMethod(pTypeType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"GetProperty", 1, &pGetPropertyMethodInfo))
        goto exit;

    InitVariantFromString(pwszPropertyName, &vtPropertyName);
    pGetPropertyArguments = SafeArrayCreateVector(VT_VARIANT, 0, 1);

    lArgumentIndex = 0;
    SafeArrayPutElement(pGetPropertyArguments, &lArgumentIndex, &vtPropertyName);

    if (!clr::InvokeMethod(pGetPropertyMethodInfo, vtTypeObject, pGetPropertyArguments, &vtPropertyInfo))
        goto exit;

    memcpy_s(pvtPropertyInfo, sizeof(*pvtPropertyInfo), &vtPropertyInfo, sizeof(vtPropertyInfo));
    bResult = TRUE;

exit:
    if (pGetPropertyArguments) SafeArrayDestroy(pGetPropertyArguments);

    if (pGetPropertyMethodInfo) pGetPropertyMethodInfo->Release();
    if (pTypeType) pTypeType->Release();

    VariantClear(&vtPropertyName);

    return bResult;
}

BOOL dotnet::System_Reflection_PropertyInfo_GetValue(mscorlib::_AppDomain* pAppDomain, VARIANT vtPropertyInfo, VARIANT vtObject, SAFEARRAY* pIndex, VARIANT* pvtValue)
{
    BOOL bResult = FALSE;
    LONG lNbArguments;
    LONG lArgumentIndex;
    VARIANT vtIndexArray = { 0 };
    VARIANT vtValue = { 0 };
    SAFEARRAY* pGetValueArguments = NULL;
    mscorlib::_Type* pPropertyInfoType = NULL;
    mscorlib::_MethodInfo* pGetValueMethodInfo = NULL;

    lNbArguments = pIndex != NULL ? 2 : 1;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_REFLECTION, L"System.Reflection.PropertyInfo", &pPropertyInfoType))
        goto exit;

    if (!clr::GetMethod(pPropertyInfoType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"GetValue", lNbArguments, &pGetValueMethodInfo))
        goto exit;

    pGetValueArguments = SafeArrayCreateVector(VT_VARIANT, 0, lNbArguments);
    
    lArgumentIndex = 0;
    SafeArrayPutElement(pGetValueArguments, &lArgumentIndex, &vtObject);

    if (pIndex != NULL)
    {
        vtIndexArray.vt = VT_ARRAY | VT_VARIANT;
        vtIndexArray.parray = pIndex;

        lArgumentIndex = 1;
        SafeArrayPutElement(pGetValueArguments, &lArgumentIndex, &vtIndexArray);
    }

    if (!clr::InvokeMethod(pGetValueMethodInfo, vtPropertyInfo, pGetValueArguments, &vtValue))
        goto exit;

    memcpy_s(pvtValue, sizeof(*pvtValue), &vtValue, sizeof(vtValue));
    bResult = TRUE;

exit:
    if (pGetValueArguments) SafeArrayDestroy(pGetValueArguments);

    if (pGetValueMethodInfo) pGetValueMethodInfo->Release();
    if (pPropertyInfoType) pPropertyInfoType->Release();

    return bResult;
}

BOOL dotnet::System_Reflection_PropertyInfo_GetValue(mscorlib::_AppDomain* pAppDomain, VARIANT vtPropertyInfo, VARIANT vtObject, VARIANT* pvtValue)
{
    return dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtPropertyInfo, vtObject, NULL, pvtValue);
}

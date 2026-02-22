#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#ifdef __MINGW32__
# include <metahost.h>   /* from execute-assembly/src (add -I../execute-assembly/src) */
# include "clr_guids.h"  /* CLSID_CorRuntimeHost, IID_ICorRuntimeHost (defined in clr_guids.c) */
# include "propvarutil_compat.h"  /* InitVariantFrom* (no propvarutil.h on MinGW) */
#else
# include <metahost.h>
# include <propvarutil.h>
#endif
#pragma comment(lib, "mscoree.lib")
#ifndef __MINGW32__
#pragma comment(lib, "Propsys.lib")
#endif

#ifdef __MINGW32__
# include <mscorlib.h>   /* from execute-assembly/src (types are in global namespace) */
/* Re-export as mscorlib:: so powerpick code compiles unchanged */
namespace mscorlib {
    using ::_AppDomain;
    using ::_Assembly;
    using ::_Type;
    using ::_PropertyInfo;
    using ::_FieldInfo;
    using ::_MethodInfo;
    /* execute-assembly header uses global enum BindingFlags; we need mscorlib::BindingFlags::* */
    enum BindingFlags {
        BindingFlags_Default = 0,
        BindingFlags_IgnoreCase = 1,
        BindingFlags_DeclaredOnly = 2,
        BindingFlags_Instance = 4,
        BindingFlags_Static = 8,
        BindingFlags_Public = 16,
        BindingFlags_NonPublic = 32,
        BindingFlags_FlattenHierarchy = 64,
        BindingFlags_InvokeMethod = 256,
        BindingFlags_CreateInstance = 512,
        BindingFlags_GetField = 1024,
        BindingFlags_SetField = 2048,
        BindingFlags_GetProperty = 4096,
        BindingFlags_SetProperty = 8192,
        BindingFlags_PutDispProperty = 16384,
        BindingFlags_PutRefDispProperty = 32768,
        BindingFlags_ExactBinding = 0x10000,
        BindingFlags_SuppressChangeType = 0x20000,
        BindingFlags_OptionalParamBinding = 0x40000,
        BindingFlags_IgnoreReturn = 0x1000000
    };
}
#else
# import <mscorlib.tlb> raw_interfaces_only \
    high_property_prefixes("_get","_put","_putref") \
    rename("ReportEvent", "InteropServices_ReportEvent") \
    rename("or", "InteropServices_or")
#endif
using namespace mscorlib;

#define APP_DOMAIN L"PowerChell"
#define ASSEMBLY_NAME_SYSTEM_CORE L"System.Core"
#define ASSEMBLY_NAME_SYSTEM_REFLECTION L"System.Reflection"
#define ASSEMBLY_NAME_SYSTEM_RUNTIME L"System.Runtime"
#define ASSEMBLY_NAME_MICROSOFT_POWERSHELL_CONSOLEHOST L"Microsoft.PowerShell.ConsoleHost"
#define ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION L"System.Management.Automation"
#define BINDING_FLAGS_PUBLIC_STATIC mscorlib::BindingFlags::BindingFlags_Public | mscorlib::BindingFlags::BindingFlags_Static
#define BINDING_FLAGS_PUBLIC_INSTANCE mscorlib::BindingFlags::BindingFlags_Public | mscorlib::BindingFlags::BindingFlags_Instance
#define BINDING_FLAGS_NONPUBLIC_STATIC mscorlib::BindingFlags::BindingFlags_NonPublic | mscorlib::BindingFlags::BindingFlags_Static
#define BINDING_FLAGS_NONPUBLIC_INSTANCE mscorlib::BindingFlags::BindingFlags_NonPublic | mscorlib::BindingFlags::BindingFlags_Instance

typedef struct _CLR_CONTEXT
{
    ICLRMetaHost* pMetaHost;
    ICLRRuntimeInfo* pRuntimeInfo;
    ICorRuntimeHost* pRuntimeHost;
    IUnknown* pAppDomainThunk;
} CLR_CONTEXT, * PCLR_CONTEXT;

namespace clr
{
    BOOL InitializeCommonLanguageRuntime(PCLR_CONTEXT pClrContext, mscorlib::_AppDomain** ppAppDomain);
    void DestroyCommonLanguageRuntime(PCLR_CONTEXT pClrContext, mscorlib::_AppDomain* pAppDomain);
    BOOL FindAssemblyPath(LPCWSTR pwszAssemblyName, LPWSTR* ppwszAssemblyPath);
    BOOL GetAssembly(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, mscorlib::_Assembly** ppAssembly);
    BOOL LoadAssembly(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, mscorlib::_Assembly** ppAssembly);
    BOOL CreateInstance(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszClassName, VARIANT* pvtInstance);
    BOOL GetType(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszTypeFullName, mscorlib::_Type** ppType);
    BOOL GetProperty(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, LPCWSTR pwszPropertyName, mscorlib::_PropertyInfo** ppPropertyInfo);
    BOOL GetPropertyValue(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, VARIANT vtObject, LPCWSTR pwszPropertyName, VARIANT* pvtPropertyValue);
    BOOL GetField(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, LPCWSTR pwszFieldName, mscorlib::_FieldInfo** ppFieldInfo);
    BOOL GetFieldValue(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, VARIANT vtObject, LPCWSTR pwszFieldName, VARIANT* pvtFieldValue);
    BOOL GetMethod(mscorlib::_Type* pType, mscorlib::BindingFlags bindingFlags, LPCWSTR pwszMethodName, LONG lNbArg, mscorlib::_MethodInfo** ppMethodInfo);
    BOOL InvokeMethod(mscorlib::_MethodInfo* pMethodInfo, VARIANT vtObject, SAFEARRAY* pParameters, VARIANT* pvtResult);
    BOOL FindMethodInArray(SAFEARRAY* pMethods, LPCWSTR pwszMethodName, LONG lNbArgs, mscorlib::_MethodInfo** ppMethodInfo);
    BOOL PrepareMethod(mscorlib::_AppDomain* pAppDomain, VARIANT* pvtMethodHandle);
    BOOL GetFunctionPointer(mscorlib::_AppDomain* pAppDomain, VARIANT* pvtMethodHandle, PULONG_PTR pFunctionPointer);
    BOOL GetJustInTimeMethodAddress(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszClassName, LPCWSTR pwszMethodName, DWORD dwNbArgs, PULONG_PTR pMethodAddress);
}

namespace dotnet
{
    BOOL System_Object_GetType(mscorlib::_AppDomain* pAppDomain, VARIANT vtObject, VARIANT* pvtObjectType);
    BOOL System_Type_GetProperty(mscorlib::_AppDomain* pAppDomain, VARIANT vtTypeObject, LPCWSTR pwszPropertyName, VARIANT* pvtPropertyInfo);
    BOOL System_Reflection_PropertyInfo_GetValue(mscorlib::_AppDomain* pAppDomain, VARIANT vtPropertyInfo, VARIANT vtObject, SAFEARRAY* pIndex, VARIANT* pvtValue);
    BOOL System_Reflection_PropertyInfo_GetValue(mscorlib::_AppDomain* pAppDomain, VARIANT vtPropertyInfo, VARIANT vtObject, VARIANT* pvtValue);
}
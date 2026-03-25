#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "clr.h"

BOOL PatchAmsiOpenSession();
BOOL PatchAmsiScanBuffer();

BOOL PatchSystemPolicyGetSystemLockdownPolicy(mscorlib::_AppDomain* pAppDomain);
BOOL PatchTranscriptionOptionFlushContentToDisk(mscorlib::_AppDomain* pAppDomain);
BOOL PatchAuthorizationManagerShouldRunInternal(mscorlib::_AppDomain* pAppDomain);

BOOL GetProcedureAddress(LPCWSTR pwszModuleName, LPCSTR pszProcedureName, PULONG_PTR pProcedureAddress);
BOOL PatchProcedure(LPVOID pTargetAddress, LPBYTE pSourceBuffer, DWORD dwSourceBufferSize);

BOOL PatchUnmanagedFunction(LPCWSTR pwszMdoduleName, LPCSTR pszProcedureName, LPBYTE pbPatch, DWORD dwPatchSize, DWORD dwPatchOffset);
BOOL PatchManagedFunction(mscorlib::_AppDomain* pAppDomain, LPCWSTR pwszAssemblyName, LPCWSTR pwszClassName, LPCWSTR pwszMethodName, DWORD dwNbArgs, LPBYTE pbPatch, DWORD dwPatchSize, DWORD dwPatchOffset);

BOOL FindBufferOffset(LPVOID pStartAddress, LPBYTE pBuffer, DWORD dwBufferSize, DWORD dwMaxSize, PDWORD pdwBufferOffset);
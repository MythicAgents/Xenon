#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#define DEBUG TRUE

/* Use L"" prefix + f so GCC and MSVC both accept (callers use L"format" for PRINT_*) */
#define PRINT_INFO(f, ...) wprintf(L"[*] " f, ##__VA_ARGS__)
#define PRINT_WARNING(f, ...) wprintf(L"[!] " f, ##__VA_ARGS__)
#define PRINT_ERROR(f, ...) wprintf(L"[-] " f, ##__VA_ARGS__)
#define EXIT_ON_NULL_POINTER(m, p) if (p == NULL) { PRINT_ERROR(L"Null pointer: %ws\n", m); goto exit; }

#if DEBUG
#define EXIT_ON_WIN32_ERROR(f, c) if (c) { PRINT_ERROR(L"Function %ws failed with error code: %d\n", f, GetLastError()); goto exit; }
#define EXIT_ON_HRESULT_ERROR(f, hr) if (FAILED(hr)) { PRINT_ERROR(L"Function %ws failed with error code: 0x%08x\n", f, hr); goto exit; }
#else
#define EXIT_ON_WIN32_ERROR(f, c) if (c) { PRINT_ERROR(L"A function failed with error code: %d\n", GetLastError()); goto exit; }
#define EXIT_ON_HRESULT_ERROR(f, hr) if (FAILED(hr)) { PRINT_ERROR(L"A function failed with error code: 0x%08x\n", hr); goto exit; }
#endif
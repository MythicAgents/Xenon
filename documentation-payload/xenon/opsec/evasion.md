+++
title = "Evasion Hardening Guide"
chapter = false
weight = 31
+++

This guide covers how to reduce Xenon's detection surface across build-time configuration, runtime behavior, and operational tradecraft. Xenon's defaults are functional but **not OPSEC-safe** - every section below addresses a specific detection vector and how to mitigate it.

---

## 1. Network Layer

### Malleable C2 Profiles

The single highest-impact change an operator can make. The default profile uses generic headers and URIs that stand out in network telemetry (Zeek, Suricata, proxy logs).

**What to do:**
- Use a malleable profile that mimics real traffic for the target environment. Study the target's legitimate outbound traffic (CDN providers, SaaS vendors, update services) and build a profile that matches.
- Set `User-Agent` strings to match the browser or service you're impersonating - **not** a generic string. Match the OS build, browser version, and locale present in the environment.
- Use the `xor` + `base64` transform chain on both client and server messages to avoid plaintext payload signatures in transit.
- Use `prepend` / `append` transforms to wrap C2 traffic in legitimate-looking structures (HTML pages, JSON API responses, binary file headers like `.cab` or `.jpg`).
- Rotate URIs - define multiple URIs per verb to reduce pattern matching on repeated requests to a single path.

**Example:** The included `microsoft.json` profile wraps GET responses in a `.cab` file header (`MSCF`) with `Microsoft-CryptoAPI/10.0` as the User-Agent and IIS response headers. This blends with Windows Update traffic when paired with appropriate infrastructure.

### Domain Rotation Strategy

Xenon supports three strategies: `round-robin`, `failover`, and `random`. The right choice depends on your infrastructure.

| Strategy | When to Use |
|----------|-------------|
| **Failover** | Production ops - primary domain stays stable, fallback only on failure. Minimizes DNS noise. |
| **Random** | Short engagements with expendable domains. Spreads traffic across redirectors. |
| **Round-Robin** | Avoid unless all domains are equally expendable. Consistent rotation is a detectable pattern. |

**Recommendations:**
- Configure at least 3 callback domains behind separate redirectors.
- Set the failover threshold to 3-5 failures - low enough to rotate before the operator notices, high enough to tolerate transient network errors.
- Mark domains as expendable in your infrastructure plan. When a domain is burned, the agent auto-rotates.

### TLS / SSL

- Always enable SSL for HTTPX transports. Unencrypted HTTP C2 traffic is trivially detectable via content inspection.
- Use valid certificates (Let's Encrypt or purchased certs) - self-signed certs generate TLS alerts in enterprise proxies and are flagged by JA3/JA3S fingerprinting tools.
- If the target environment uses TLS inspection (corporate proxy with CA injection), route through a domain-fronting or CDN-based redirector that the proxy's allow-list trusts.

### Proxy Configuration

If the target environment routes traffic through a corporate proxy:
- Enable proxy support at build time and provide the correct proxy URL.
- Ensure the malleable profile's headers (especially `Connection`, `Proxy-Connection`, `Cache-Control`) are consistent with how the impersonated application behaves through a proxy.

---

## 2. Sleep and Jitter

### The Problem

A callback that checks in at a fixed interval - or with insufficient jitter - is trivially detectable via statistical analysis of beacon timing (variance analysis, entropy scoring on connection intervals).

Default sleep with `0` jitter produces a perfectly periodic signal. Even moderate jitter with a low base sleep is noisy.

### Recommendations

- **Minimum sleep**: 30-60 seconds for active operations. Longer (5-15 minutes) for persistence callbacks in low-activity phases.
- **Jitter**: Always set jitter to at least 20-40%. Higher jitter makes statistical detection significantly harder.
- Use the `sleep` command to dynamically adjust during operations - aggressive sleep during active tasking, long sleep during idle periods.
- Be aware: SMB and TCP transports use a **hardcoded 500ms sleep** with no jitter. This is acceptable for internal P2P links but means SMB/TCP agents generate a very consistent polling pattern on the named pipe or socket. Keep this in mind for lateral movement chains.

### Interactive vs. Long-Haul

Maintain two classes of callbacks when possible:
1. **Interactive** - low sleep (10-30s), used for active tasking, burned after the operation phase.
2. **Long-haul** - high sleep (10-60 min), high jitter (40%+), used as persistence. Only wake for check-ins and critical tasks.

---

## 3. Process Injection

This is where most detections fire. The default injection kit uses the classic APC chain:

```
CreateProcess -> VirtualAllocEx -> WriteProcessMemory -> VirtualProtectEx -> QueueUserAPC -> ResumeThread
```

This sequence is **heavily signatured** by every major EDR (CrowdStrike, SentinelOne, Defender for Endpoint, Elastic). Each API call generates ETW telemetry, and the chain as a whole is a known behavioral pattern.

### Replace the Default Kit

Use `register_process_inject_kit` to deploy a custom injection BOF. Priority alternatives:

| Technique | BOF Example | Detection Reduction |
|-----------|-------------|---------------------|
| **Indirect syscalls** | [InjectKit](https://github.com/REDMED-X/InjectKit) (Tartarus' Gate) | Bypasses userland API hooks; avoids ETW from hooked ntdll stubs |
| **Section mapping** | [secinject](https://github.com/apokryptein/secinject) | Uses `NtCreateSection` + `NtMapViewOfSection` - no `VirtualAllocEx`, blends in VAD as image-backed |
| **Syscall-based** | [CB_process_Inject](https://github.com/vgeorgiev90/CB_process_Inject) | Direct/indirect syscalls for the full injection chain |

**Key principles for custom kits:**
- Never allocate RWX memory. Use RW for write, then flip to RX via `NtProtectVirtualMemory`. RWX allocations are a high-confidence indicator.
- Prefer `NtAllocateVirtualMemory` over `VirtualAllocEx` - lower-level, fewer hooks.
- Prefer section-mapping (`NtCreateSection` + `NtMapViewOfSection`) over `WriteProcessMemory` entirely where possible.
- Use indirect syscalls (SSN resolution + `jmp` through ntdll `.text` trampoline) rather than direct `syscall` instructions from your own `.text` section - direct syscalls from non-ntdll memory are detected by CrowdStrike and Elastic.

### Spawnto Selection

The `spawnto` setting controls which sacrificial process is spawned for fork-and-run commands. The default is `svchost.exe`.

**Problems with svchost.exe:**
- `svchost.exe` spawned without `-k` arguments is anomalous.
- `svchost.exe` not spawned by `services.exe` is anomalous.
- Parent-child relationship analysis will flag this immediately.

**Better choices:**
- Match the target environment. Look at what processes are normal for the user context:
  - For standard user: `RuntimeBroker.exe`, `dllhost.exe`, `conhost.exe`
  - For SYSTEM: `WerFault.exe`, `WmiPrvSE.exe`, `taskhostw.exe`
- Use `spawnto` to change this at build time. Use the `spawnto` command to change it at runtime based on context.
- The best spawnto process is one that already exists in the environment, normally runs with similar integrity, and won't look anomalous as a child of your current process.

---

## 4. Reflective Loading (UDRL)

The default Crystal Palace loader works, but has OPSEC gaps:

### RWX Allocation
The default loader in [loader.c](../../Payload_Type/xenon/xenon/agent_code/Loader/default/src/loader.c) allocates memory with `PAGE_EXECUTE_READWRITE`:

```c
char * dll_dst = KERNEL32$VirtualAlloc(NULL, SizeOfDLL(&dll_data),
    MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
```

RWX private memory allocations are a high-confidence indicator for memory scanners (Moneta, pe-sieve, BeaconEye). **This is the single most impactful change you can make in a custom UDRL.**

**Custom UDRL recommendations:**
1. **RW -> RX transition**: Allocate as `PAGE_READWRITE`, write the DLL, then flip to `PAGE_EXECUTE_READ` via `VirtualProtect` / `NtProtectVirtualMemory`.
2. **Module stomping**: Map a legitimate, expendable DLL from disk (e.g., a large, rarely-used system DLL), overwrite its `.text` section with the agent. The memory region shows as image-backed in VAD queries - blends with legitimate modules.
3. **Header erasure**: After loading, zero out the PE headers (DOS header, NT headers) in the allocated region. Memory scanners that look for `MZ` / `PE` signatures at allocation bases will miss it.
4. **Import resolution**: The default loader uses `LoadLibraryA` + `GetProcAddress`. For higher OPSEC, resolve imports via EAT walking with hash comparison - never call `GetProcAddress` with a plaintext function name string.

### Building a Custom UDRL

Follow the UDRL upload process documented in the [OPSEC page](_index.md):
1. Clone `crystal-simple-loader`
2. Implement RW->RX, header erasure, and any additional techniques
3. Zip and upload during the build process

---

## 5. Post-Exploitation Commands

### Fork & Run OPSEC

Commands that use fork-and-run (`mimikatz`, `execute_assembly`, `powerchell`) are inherently noisier than inline execution because they:
- Spawn a child process (visible in process tree)
- Inject into it (triggers injection telemetry)
- Create a named pipe for output capture (pipe name is an IOC)

**Reduce the noise:**
- **Prefer inline execution** (`inline_execute`, `inline_execute_assembly`) over fork-and-run whenever possible. Inline BOFs run in the agent's thread - no child process, no injection, no pipe.
- **Named pipe name**: Change the default pipe name to something that matches legitimate software pipes in the environment. Generic UUID-format pipes are less suspicious than custom strings, but known default pipe names for C2 frameworks are signatured.
- Randomize the pipe name at build time using the randomization option.
- Use `blockdlls` before running fork-and-run commands to prevent EDR DLLs from being loaded into the sacrificial process. This blocks unsigned/non-Microsoft DLLs from loading via `PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON`.

### Mimikatz and Assemblies

- `mimikatz` and `execute_assembly` use Donut shellcode under the hood. Donut-generated shellcode has known signatures - some EDRs detect the Donut loader stub itself.
- Where possible, use BOF equivalents of common post-ex tasks rather than Donut-wrapped assemblies.
- For `inline_execute_assembly`: this patches AMSI and ETW in-process. Be aware that the patching itself (`AmsiScanBuffer` / `EtwEventWrite` patch) is a detectable event if the EDR monitors those functions for integrity.

---

## 6. Named Pipe OPSEC

Named pipes are used for:
1. Fork-and-run output capture
2. SMB P2P linking between agents

### Detection Vectors
- Sysmon Event ID 17 (Pipe Created) and Event ID 18 (Pipe Connected) log pipe names.
- Known C2 pipe name patterns are signatured (e.g., `msagent_*`, `MSSE-*`, `postex_*`, default Cobalt Strike patterns).
- Pipe access from unusual processes triggers behavioral rules.

### Hardening
- Set a plausible pipe name that matches software in the environment. Examples:
  - Chrome: `mojo.{PID}.{TID}.{RANDOM}`
  - VS Code: `vscode-ipc-{UUID}`
  - .NET: `clr_debugger_{PID}`
- Use the `default_pipename` build option with randomization enabled.
- For SMB links, ensure the pipe name matches the expected format on both ends.

---

## 7. Build-Time Hardening Checklist

Before deploying a Xenon payload, verify these settings:

| Setting | Insecure Default | Hardened Value |
|---------|-----------------|----------------|
| **Encryption** | Disabled | Enabled (AES) |
| **Debug** | May be enabled | Disabled - debug builds include console output and symbols |
| **User-Agent** | Generic | Environment-matched (see Malleable Profiles) |
| **Sleep** | Low / no jitter | 30s+ with 20-40% jitter minimum |
| **Spawnto** | `svchost.exe` | Context-appropriate process (see Section 3) |
| **Pipe name** | Default | Randomized or environment-matched |
| **UDRL** | Default (RWX) | Custom with RW->RX, header erasure |
| **Injection Kit** | Default APC chain | Custom syscall-based or section-mapping kit |
| **Output type** | exe | shellcode (via UDRL) or DLL - raw EXEs on disk are high-risk |
| **C2 Profile** | Default | Malleable profile matched to target environment |
| **Block DLLs** | Disabled | Enable before fork-and-run tasks |
| **Domain rotation** | Single domain | 3+ domains with failover strategy |
| **SSL** | May be disabled | Always enabled with valid certs |

---

## 8. Runtime OPSEC Workflow

Once the agent is running, follow this operational sequence:

1. **Initial check-in**: Verify connectivity, confirm sleep/jitter are appropriate.
2. **Set sleep**: `sleep 60 30` - 60 second base, 30% jitter for active ops.
3. **Register injection kit**: `register_process_inject_kit` with your custom BOF before running any fork-and-run commands.
4. **Enable blockdlls**: `blockdlls start` before any fork-and-run.
5. **Change spawnto**: `spawnto <appropriate_process>` for the current user context.
6. **Prefer inline execution**: Use `inline_execute` and `inline_execute_assembly` over fork-and-run equivalents.
7. **Before going idle**: `sleep 900 40` - 15 minute sleep, 40% jitter for long-haul persistence.

---

## 9. What Xenon Does NOT Currently Protect Against

Understanding gaps is as important as hardening what exists:

| Gap | Risk | Mitigation |
|-----|------|------------|
| **No sleep obfuscation** | Memory scanners (BeaconEye, hunt-sleeping-beacons) can scan agent memory during sleep. The agent's strings and code are visible in plaintext. | Implement sleep obfuscation in a custom UDRL (encrypt agent memory during sleep, decrypt on wake). Examples: Ekko, Zilean, Foliage sleep encryption techniques. |
| **No ETW patching** (outside `inline_execute_assembly`) | ETW providers log .NET activity, thread creation, image loads. | Patch `EtwEventWrite` in the agent process early, or use BOFs that handle their own ETW evasion. |
| **No call stack spoofing** | Thread call stack analysis reveals non-legitimate return addresses (e.g., stack frames pointing into private RWX memory). Tools like `ThreadStackSpoofer` detect this. | Implement stack spoofing in the UDRL or injection kit. Spoof the call stack to show legitimate return addresses during sleep. |
| **No module stomping by default** | Agent lives in unbacked private memory - stands out in VAD analysis. | Use a module-stomping UDRL that maps over a legitimate DLL. |
| **Static strings** | Compiled agent may contain identifiable strings. | Strip symbols, use string encryption (compile-time XOR or stack-constructed strings for sensitive values). |
| **No unhooking** | EDR hooks in ntdll are not removed. API calls through hooked functions generate telemetry. | Use a syscall-based injection kit, or implement ntdll unhooking (map fresh ntdll from disk, overwrite `.text` section). |

---

## Summary

The default Xenon configuration prioritizes functionality over stealth. For any engagement against a monitored environment, operators should at minimum:

1. Deploy a malleable C2 profile
2. Replace the default injection kit with a syscall-based alternative
3. Replace the default UDRL with one that avoids RWX and erases headers
4. Configure appropriate sleep, jitter, spawnto, and pipe names
5. Prefer inline execution over fork-and-run
6. Enable encryption and SSL

Each additional hardening step compounds - a single improvement helps, but the combination of all layers is what makes the difference between detection in minutes and sustained access.

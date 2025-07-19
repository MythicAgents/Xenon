+++
title = "Commands"
chapter = false
weight = 20
pre = "<b>1. </b>"
+++

![logo](/agents/xenon/Xenon.png?width=600px)

## Supported Commands

| Command         | Usage                                               | Description |
|----------------|-----------------------------------------------------|-------------|
| `pwd`          | `pwd`                                               | Show present working directory. |
| `ls`           | `ls [path]`                                    | List directory information for `<directory>`. |
| `cd`           | `cd <directory>`                           | Change working directory. |
| `cp`           | `cp <source file> <destination file>`             | Copy a file to a new destination. |
| `rm`           | `rm <path\|file>`                     | Remove a directory or file. |
| `mkdir`        | `mkdir <path>`                            | Create a new directory. |
| `getuid`       | `getuid`                                            | Get the current identity. |
| `make_token`   | `make_token <DOMAIN> <username> <password> [LOGON_TYPE]` | Create a token and impersonate it using plaintext credentials. |
| `steal_token`  | `steal_token <pid>`                                 | Steal and impersonate the token of a target process. |
| `rev2self`     | `rev2self`                                          | Revert identity to the original process's token. |
| `ps`           | `ps`                                                | List host processes. |
| `shell`        | `shell <command>`                                   | Runs `{command}` in a terminal. |
| `sleep`        | `sleep <seconds> [jitter]`                          | Change sleep timer and jitter. |
| `inline_execute` | `inline_execute -BOF [COFF.o] [-Arguments [optional arguments]]` | Execute a Beacon Object File in the current process thread and see output. **Warning:** Incorrect argument types can crash the Agent process. |
| `inline_execute_assembly` | `inline_execute_assembly -Assembly [file] [-Arguments [assembly args] [--patchexit] [--amsi] [--etw]]` | Execute a .NET Assembly in the current process using @EricEsquivel's BOF "Inline-EA" (e.g., inline_execute_assembly -Assembly SharpUp.exe -Arguments "audit" --patchexit --amsi --etw) |
| `execute_assembly` | `execute_assembly -Assembly [SharpUp.exe] [-Arguments [assembly arguments]]` | Execute a .NET Assembly in a remote processes and retrieve the output. |
| `spawnto` | `spawnto -path [C:\Windows\System32\svchost.exe]` | Set the full path of the process to use for spawn & inject commands. |
| `download`     | `download -path <file path>`                           | Download a file off the target system (supports UNC path). |
| `upload`       | `upload (modal)`                                            | Upload a file to the target machine by selecting a file from your computer. |
| `status`         | `status`                                              | List C2 connection hosts and their status. |
| `register_process_inject_kit`       | `register_process_inject_kit (pops modal)`                                            | Register a custom BOF to use for process injection (CS compatible). See documentation for requirements. |
| `exit`         | `exit`                                              | Task the implant to exit. |

---

### Module Commands (BOFs)
These are optional commands that call `inline_execute` under the hood with specific BOFs.

**Some** BOFs from the [CS-Situational-Awareness-BOF](https://github.com/trustedsec/CS-Situational-Awareness-BOF) collection have been added.
Credits to [@trustedsec](https://github.com/trustedsec) for these.

| Command                  | Usage                                                         | Description |
|--------------------------|---------------------------------------------------------------|-------------|
| `sa_adcs_enum`          | `sa_adcs_enum`                                               | **[SituationalAwareness]** Enumerate CAs and templates in the AD using Win32 functions. |
| `sa_arp`                | `sa_arp`                                                    | **[SituationalAwareness]** List ARP table. |
| `sa_driversigs`         | `sa_driversigs`                                            | **[SituationalAwareness]** Enumerate installed services' image paths to check signing certs against known AV/EDR vendors. |
| `sa_get_password_policy` | `sa_get_password_policy [hostname]`                        | **[SituationalAwareness]** Get the configured password policy and lockouts for the target server or domain. |
| `sa_ipconfig`           | `sa_ipconfig`                                              | **[SituationalAwareness]** List IPv4 address, hostname, and DNS server. |
| `sa_ldapsearch`         | `sa_ldapsearch [query] [opt: attribute] [opt: results_limit] [opt: DC hostname or IP] [opt: Distinguished Name]` | **[SituationalAwareness]** Execute LDAP searches. Specify `*,ntsecuritydescriptor` as an attribute parameter for all attributes and base64 encoded ACL of objects (useful for BOFHound). |
| `sa_list_firewall_rules`| `sa_list_firewall_rules`                                   | **[SituationalAwareness]** List Windows firewall rules. |
| `sa_listmods`           | `sa_listmods [opt: pid]`                                   | **[SituationalAwareness]** List process modules (DLLs). Targets the current process if no PID is specified. Complements `sa_driversigs` for AV/EDR injection detection. |
| `sa_netshares`          | `sa_netshares [hostname]`                                 | **[SituationalAwareness]** List shared resources on the local or remote computer. |
| `sa_netstat`            | `sa_netstat`                                             | **[SituationalAwareness]** List active TCP and UDP connections. |
| `sa_netuser`            | `sa_netuser [username] [opt: domain]`                    | **[SituationalAwareness]** Get detailed information about a specific user. |
| `sa_nslookup`           | `sa_nslookup [hostname] [opt:dns server] [opt: record type]` | **[SituationalAwareness]** Perform a DNS query. Supports specifying a custom DNS server and record type (e.g., A, AAAA, ANY). |
| `sa_probe`              | `sa_probe [host] [port]`                                 | **[SituationalAwareness]** Check if a specific port is open. |
| `sa_whoami`             | `sa_whoami`                                             | **[SituationalAwareness]** List `whoami /all`. |
| `mimikatz`          | `mimikatz [args]`                                               | Execute mimikatz on the host. (e.g., mimikatz sekurlsa::logonpasswords) OPSEC Warning: Uses donut shellcode. |

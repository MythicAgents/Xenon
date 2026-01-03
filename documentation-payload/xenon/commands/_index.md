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
| `link`           | `link <target> <named pipe>`                          | Connect to an SMB Link Agent. |
| `unlink`         | `unlink <Display Id>`                                 | Disconnect from an SMB Link Agent. |
| `register_process_inject_kit`       | `register_process_inject_kit (pops modal)`                                            | Register a custom BOF to use for process injection (CS compatible). See documentation for requirements. |
| `exit`         | `exit`                                              | Task the implant to exit. |

---

### Forge
Forge is a command augmentation container that enables Xenon to use a ton of tools from SharpCollections and Sliver Armory BOFs.

Refer to the forge documentation [here](https://github.com/MythicAgents/forge.git)
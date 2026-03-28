+++
title = "Commands"
chapter = true
weight = 10
pre = "<b>1. </b>"
+++

![logo](/agents/xenon/Xenon.png?width=600px)

## Table of Contents

- File Operations
    * [pwd](/agents/xenon/commands/pwd/)
    * [ls](/agents/xenon/commands/ls/)
    * [cd](/agents/xenon/commands/cd/)
    * [cp](/agents/xenon/commands/cp/)
    * [rm](/agents/xenon/commands/rm/)
    * [mkdir](/agents/xenon/commands/mkdir/)
    * [download](/agents/xenon/commands/download/)
    * [upload](/agents/xenon/commands/upload/)
- Credential / Token Commands
    * [getuid](/agents/xenon/commands/getuid/)
    * [make_token](/agents/xenon/commands/make_token/)
    * [steal_token](/agents/xenon/commands/steal_token/)
    * [rev2self](/agents/xenon/commands/rev2self/)
    * [getprivs](/agents/xenon/commands/getprivs/)
- Process Management
    * [ps](/agents/xenon/commands/ps/)
    * [shell](/agents/xenon/commands/shell/)
- Execution
    * [inline_execute](/agents/xenon/commands/inline_execute/)
    * [inline_execute_assembly](/agents/xenon/commands/inline_execute_assembly/)
    * [execute_assembly](/agents/xenon/commands/execute_assembly/)
    * [execute_dll](/agents/xenon/commands/execute_dll/)
    * [inject_shellcode](/agents/xenon/commands/inject_shellcode/)
    * [mimikatz](/agents/xenon/commands/mimikatz/)
- PowerShell
    * [powershell](/agents/xenon/commands/powershell/)
    * [powershell_import](/agents/xenon/commands/powershell_import/)
- Lateral Movement
    * [jump](/agents/xenon/commands/jump/)
    * [sc](/agents/xenon/commands/sc/)
    * [wmiexecute](/agents/xenon/commands/wmiexecute/)
    * [dcomexec](/agents/xenon/commands/dcomexec/)
- Evasion
    * [blockdlls](/agents/xenon/commands/blockdlls/)
    * [spawnto](/agents/xenon/commands/spawnto/)
    * [register_process_inject_kit](/agents/xenon/commands/register_process_inject_kit/)
- Peer-to-Peer / C2
    * [link](/agents/xenon/commands/link/)
    * [unlink](/agents/xenon/commands/unlink/)
    * [socks](/agents/xenon/commands/socks/)
    * [status](/agents/xenon/commands/status/)
- Job Management
    * [job_kill](/agents/xenon/commands/job_kill/)
- Agent Management
    * [sleep](/agents/xenon/commands/sleep/)
    * [exit](/agents/xenon/commands/exit/)

---

### Forge
Forge is a command augmentation container that enables Xenon to use a ton of tools from SharpCollections and Sliver Armory BOFs.

Refer to the forge documentation [here](https://github.com/MythicAgents/forge.git)

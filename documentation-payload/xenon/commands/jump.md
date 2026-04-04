+++
title = "jump"
chapter = false
weight = 103
hidden = false
+++

{{% notice info %}}
`jump` is a `script_only` command - it runs entirely in the Mythic container and orchestrates subtasks (`upload`, `make_token`, `rev2self`, and the selected execution method). No C agent dispatch occurs.
{{% /notice %}}

## Summary
Perform lateral movement to a remote Windows host using one of three techniques:

| Type | Mechanism |
|------|-----------|
| `psexec` | Upload payload over SMB, create and start a Windows service via the Service Control Manager |
| `wmi` | Upload payload over SMB, execute via `Win32_Process::Create` using WMI |
| `dcom` | Upload payload over SMB, execute via DCOM (`MMC20.Application` or `ShellWindows`) |

Optional credentials can be supplied for any technique. If a credential argument is provided, the command calls `make_token` before moving laterally and `rev2self` upon completion.

### Arguments

#### type
The lateral movement technique. One of: `psexec`, `wmi`, `dcom`.

#### target
The hostname or IP address of the remote system.

#### payload
The payload file to deliver and execute on the remote host.

#### command (optional)
The command line to use when creating the remote process (for WMI/DCOM). Defaults to the dropped file path.

#### service_name (optional, psexec only)
The name to use when creating the Windows service. A random name is used if omitted.

#### dcom_method (optional, dcom only)
The DCOM execution method: `mmc20` (default) or `shellwindows`.

#### username (optional)
Username for credential-based lateral movement.

#### domain (optional)
Domain for the supplied username.

#### password (optional)
Plaintext password for the account.

#### hash (optional)
NT hash for pass-the-hash authentication (used instead of password when provided).

## Usage
```
jump -type <psexec|wmi|dcom> -target <host> -payload <file> [options]
```

Example
```
jump -type psexec -target 192.168.1.10 -payload beacon.exe
jump -type wmi -target dc01.corp.local -payload beacon.exe -username jsmith -domain CORP -password P@ssw0rd
jump -type dcom -target 10.0.0.5 -payload beacon.exe -username admin -domain CORP -hash aad3b435b51404eeaad3b435b51404ee:ntlmhashhere
```

## MITRE ATT&CK Mapping

- T1021.002
- T1021.003
- T1021.006
- T1047
- T1569.002

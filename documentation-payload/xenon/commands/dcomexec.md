+++
title = "dcomexec"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute a command on a local or remote host via DCOM using PowerShell (`powerchell`). Supports three DCOM objects with different requirements and detection profiles.

| Method | CLSID | Notes |
|--------|-------|-------|
| `mmc20` (default) | `49B2791A-B1AE-4C90-9B8E-E860BA07F889` | MMC20.Application - widely available, no Explorer dependency |
| `shellwindows` | `9BA05972-F6A8-11CF-A442-00A0C90A8F39` | ShellWindows - requires an open Explorer window on the target |
| `shellbrowserwindow` | `C08AFD90-F2A1-11D1-8455-00A0C91F3880` | ShellBrowserWindow - no Explorer window required |

### Arguments

#### command
The full command line to execute. Wrapped in `cmd.exe /Q /c` before invocation.

#### host (optional)
Remote hostname or IP. Defaults to `127.0.0.1` (local) if omitted.

#### method (optional)
DCOM object to use: `mmc20`, `shellwindows`, or `shellbrowserwindow`. Defaults to `mmc20`.

## Usage
```
dcomexec -command <cmd> [-host <host>] [-method mmc20|shellwindows|shellbrowserwindow]
```

Example
```
dcomexec -command "whoami" -host 192.168.1.10
dcomexec -command "C:\Windows\Temp\beacon.exe" -host dc01 -method shellbrowserwindow
```

## MITRE ATT&CK Mapping

- T1021.003

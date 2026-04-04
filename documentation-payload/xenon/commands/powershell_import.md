+++
title = "powershell_import"
chapter = false
weight = 103
hidden = false
+++

## Summary
Upload a PowerShell script file into the agent's in-memory PowerShell runspace so its functions are available to subsequent `powershell` commands. Pass `--clear` to remove any previously imported script.

### Arguments

#### File
The `.ps1` script file to import into the runspace.

#### --clear (optional)
Clear any previously imported script from the runspace.

## Usage
```
powershell_import -File <script.ps1>
powershell_import --clear
```

Example
```
powershell_import -File PowerView.ps1
powershell -Command "Get-DomainUser"
```

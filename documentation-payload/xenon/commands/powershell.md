+++
title = "powershell"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute a PowerShell command or script block and return the output.

### Arguments

#### Command
The PowerShell command or expression to execute.

## Usage
```
powershell -Command <command>
```

Example
```
powershell -Command "Get-Process"
powershell -Command "Get-ADUser -Filter * | Select SamAccountName"
```

## MITRE ATT&CK Mapping

- T1059
- T1562

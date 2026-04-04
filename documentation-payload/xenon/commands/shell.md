+++
title = "shell"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute a command via `cmd.exe /c` and return the output.

### Arguments

#### command
The command string to pass to `cmd.exe`.

## Usage
```
shell <command>
```

Example
```
shell whoami /all
shell net user /domain
```

## MITRE ATT&CK Mapping

- T1059

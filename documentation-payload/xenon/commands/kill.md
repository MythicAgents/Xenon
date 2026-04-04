+++
title = "kill"
chapter = false
weight = 103
hidden = false
+++

## Summary
Terminate a process by its PID using `OpenProcess` + `TerminateProcess`. Requires `PROCESS_TERMINATE` access to the target process - killing processes owned by other users or system processes typically requires elevated privileges.

### Arguments

#### pid
The numeric PID of the process to terminate.

## Usage
```
kill <pid>
```

Example
```
kill 1234
```

## MITRE ATT&CK Mapping

- T1562.001

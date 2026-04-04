+++
title = "spawnto"
chapter = false
weight = 103
hidden = false
+++

## Summary
Set the full path of the process used as a sacrificial host for spawn-and-inject commands such as `execute_assembly` and `inject_shellcode`. Defaults to `C:\Windows\System32\svchost.exe` if not set.

### Arguments

#### path
The full path to the binary to use as the sacrificial process.

## Usage
```
spawnto -path <full path>
```

Example
```
spawnto -path C:\Windows\System32\svchost.exe
spawnto -path C:\Windows\System32\RuntimeBroker.exe
```

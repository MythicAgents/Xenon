+++
title = "execute_dll"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute a Dynamic Link Library as position-independent code (PIC). The DLL is loaded and its reflective entry point is invoked.

### Arguments

#### File
The DLL file to execute.

#### Arguments (optional)
Arguments to pass to the DLL entry point.

## Usage
```
execute_dll -File <file.dll> [-Arguments <args>]
```

Example
```
execute_dll -File mimikatz.x64.dll
```

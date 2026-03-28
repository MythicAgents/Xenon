+++
title = "inline_execute_assembly"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute a .NET assembly in-process using the "Inline-EA" BOF by Eric Esquivel. Supports optional AMSI/ETW patching and exit-function hooking to prevent the agent from dying when the assembly calls `Environment.Exit`.

### Arguments

#### Assembly
The .NET assembly file to execute.

#### Arguments (optional)
Arguments to pass to the assembly's entry point.

#### --patchexit (optional)
Hook `Environment.Exit` to prevent the assembly from terminating the host process.

#### --amsi (optional)
Patch AMSI before loading the assembly.

#### --etw (optional)
Patch ETW before loading the assembly.

## Usage
```
inline_execute_assembly -Assembly <file.exe> [-Arguments <args>] [--patchexit] [--amsi] [--etw]
```

Example
```
inline_execute_assembly -Assembly SharpUp.exe -Arguments "audit" --patchexit --amsi --etw
```

+++
title = "inline_execute"
chapter = false
weight = 103
hidden = false
+++

{{% notice warning %}}
Incorrect argument types can crash the agent process. Verify BOF argument signatures before executing.
{{% /notice %}}

## Summary
Execute a Beacon Object File (COFF) in-process on the current agent thread and return the output. Compatible with Cobalt Strike BOFs.

### Arguments

#### BOF
The `.o` COFF file to execute.

#### Arguments (optional)
Typed arguments to pass to the BOF entry point. Supported types: `str`, `wstr`, `int`, `short`, `bin`.

## Usage
```
inline_execute -BOF <coff.o> [-Arguments [type:value ...]]
```

Example
```
inline_execute -BOF whoami.o
inline_execute -BOF inject.o -Arguments int:1234 str:calc.exe
```

+++
title = "execute_assembly"
chapter = false
weight = 103
hidden = false
+++

## Summary
Inject and execute a .NET assembly into a remote sacrificial process (configured by `spawnto`) and retrieve the output.

### Arguments

#### Assembly
The .NET assembly file to execute.

#### Arguments (optional)
Arguments to pass to the assembly's entry point.

## Usage
```
execute_assembly -Assembly <file.exe> [-Arguments <args>]
```

Example
```
execute_assembly -Assembly Seatbelt.exe -Arguments "all"
execute_assembly -Assembly Rubeus.exe -Arguments "kerberoast /outfile:hashes.txt"
```

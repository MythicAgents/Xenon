+++
title = "inject_shellcode"
chapter = false
weight = 103
hidden = false
+++

## Summary
Inject shellcode into a target process. Supports the default built-in injection method or a custom inject kit registered via `register_process_inject_kit`.

### Arguments

#### File
The raw shellcode binary to inject.

#### pid
The target process ID to inject into.

#### method (optional)
Injection method to use: `default` (built-in) or `custom` (registered inject kit BOF).

#### kit (optional)
The inject kit BOF file to use when `--method custom` is specified.

## Usage
```
inject_shellcode -File <shellcode.bin> -pid <pid> [--method default|custom] [--kit <file.o>]
```

Example
```
inject_shellcode -File beacon.bin -pid 1234
inject_shellcode -File beacon.bin -pid 1234 --method custom --kit inject_kit.o
```

## MITRE ATT&CK Mapping

- T1055

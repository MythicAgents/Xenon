+++
title = "register_process_inject_kit"
chapter = false
weight = 103
hidden = false
+++

## Summary
Register a custom Beacon Object File (BOF) to use for process injection. The BOF must conform to the Cobalt Strike inject-kit API. Once registered, commands like `inject_shellcode` can use it via `--method custom`.

## Usage
```
register_process_inject_kit (opens modal)
```

## Detailed Summary

The inject kit BOF receives the shellcode buffer and a target PID as arguments. It is responsible for allocating memory, writing shellcode, and executing it in the target process. This allows operators to swap in custom or evasive allocation/execution primitives without modifying the agent itself.

+++
title = "blockdlls"
chapter = false
weight = 103
hidden = false
+++

## Summary
Enable or disable blocking of non-Microsoft-signed DLLs in spawned child processes. When enabled, the agent applies a `PROCESS_CREATION_MITIGATION_POLICY` to each sacrificial process created for injection commands, preventing security product DLLs from loading into those processes.

### Arguments

#### state
`start` to enable DLL blocking, `stop` to disable it.

## Usage
```
blockdlls <start|stop>
```

Example
```
blockdlls start
blockdlls stop
```

## MITRE ATT&CK Mapping

- T1562

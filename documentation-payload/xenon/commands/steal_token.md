+++
title = "steal_token"
chapter = false
weight = 103
hidden = false
+++

## Summary
Open a target process, duplicate its primary token, and impersonate it on the current thread. Requires `SeDebugPrivilege` or at minimum `PROCESS_QUERY_INFORMATION` access to the target process.

### Arguments

#### pid
The process ID of the target process whose token to steal.

## Usage
```
steal_token <pid>
```

Example
```
steal_token 1234
```

## MITRE ATT&CK Mapping

- T1134.001

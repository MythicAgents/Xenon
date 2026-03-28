+++
title = "getprivs"
chapter = false
weight = 103
hidden = false
+++

## Summary
Attempt to enable every known token privilege on the current process token using `AdjustTokenPrivileges`. Reports which privileges were successfully enabled and which were not held by the token.

## Usage
```
getprivs
```

Example output
```
+ SeDebugPrivilege                         [enabled]
+ SeImpersonatePrivilege                   [enabled]
- SeTcbPrivilege                           [not held]
...
Enabled: 12  Not held: 22
```

## MITRE ATT&CK Mapping

- T1134

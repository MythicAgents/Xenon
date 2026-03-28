+++
title = "socks"
chapter = false
weight = 103
hidden = false
+++

## Summary
Start or stop a SOCKS5 proxy server that tunnels traffic through the agent to the target network.

### Arguments

#### action
`start` to enable the proxy, `stop` to disable it.

#### port
The local port on the Mythic server to listen on (required when starting).

## Usage
```
socks <start|stop> <port>
```

Example
```
socks start 7001
socks stop 7001
```

## MITRE ATT&CK Mapping

- T1090

+++
title = "link"
chapter = false
weight = 103
hidden = false
+++

## Summary
Connect to a linked (peer-to-peer) agent over SMB named pipe or TCP. Once linked, the child agent's tasking flows through the parent agent's C2 channel.

### Arguments

#### target
The hostname or IP address of the system running the linked agent.

#### named pipe / tcp port
The named pipe name (for SMB links) or port number (for TCP links).

## Usage
```
link <target> <named pipe | tcp port>
```

Example
```
link 192.168.1.50 xenon_pipe
link dc01.corp.local 4444
```

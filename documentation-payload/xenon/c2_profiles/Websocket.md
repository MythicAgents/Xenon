+++
title = "websocket"
chapter = false
weight = 20
+++

## Overview

Xenon supports the [Mythic websocket C2 profile](https://github.com/MythicC2Profiles/websocket) in **Push** mode only.

The agent opens a persistent WebSocket (`ws://` or `wss://`), sends `Accept-Type: Push` on upgrade, checks in once, then **blocks waiting for Mythic-pushed frames**. It does not periodically issue `get_tasking` polls. Outbound frames are sent only when the agent has real data (task responses, SOCKS, rportfwd, downloads, etc.).

A dedicated receive thread blocks on `WinHttpWebSocketReceive` so idle callbacks generate no C2 traffic. The main loop wakes on inbound events, or on a short interval when local tunnels/downloads need servicing.

## Push only

If `tasking_type` is set to `Poll` at payload build time, the Xenon builder fails with an error. Select **Push** in the websocket C2 profile parameters.

## Configuration

Use the standard websocket C2 profile parameters:

| Parameter | Notes |
|-----------|--------|
| `callback_host` | Must be `ws://` or `wss://` |
| `callback_port` | Port for the websocket server |
| `ENDPOINT_REPLACE` | Path (e.g. `socket`) |
| `USER_AGENT` | Upgrade request User-Agent |
| `domain_front` | Optional `Host` header value |
| `tasking_type` | Must be `Push` |
| `callback_interval` / `callback_jitter` | Used for reconnect backoff only (not for tasking sleep) |
| AES / encryption | Mythic PSK encryption (same as httpx); no client-side EKE |

## Message format

Outbound and inbound Mythic blobs are wrapped in JSON text frames:

```json
{"client":true,"data":"<base64 Mythic message>","tag":""}
```

## Workflow

{{<mermaid>}}
sequenceDiagram
participant M as Mythic
participant W as websocket
participant A as Xenon
A ->> W: HTTP Upgrade Accept-Type Push
W -->> A: 101 Switching Protocols
A ->> W: checkin frame
W ->> M: forward checkin
M -->> W: checkin response
W -->> A: pushed frame
Note over A: block on receive thread
M -->> W: tasking
W -->> A: pushed frame
A ->> W: responses only when queued
{{< /mermaid >}}

## OPSEC

- Initial connection is an HTTP(S) WebSocket upgrade via WinHTTP, then framed WebSocket traffic on the same connection.
- `wss://` skips certificate validation (typical for Mythic redirectors / self-signed certs).
- No httpx-style malleable transforms or multi-host rotation on this transport.

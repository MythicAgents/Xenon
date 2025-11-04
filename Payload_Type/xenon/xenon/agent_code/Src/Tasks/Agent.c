/*
 * Contains misc agent tasks that do not necessitate their own file.
*/
#include "Tasks/Agent.h"

#include "Xenon.h"
#include "Parser.h"
#include "Strategy.h"
#include "Config.h"

/** 
 * Update the sleep & jitter timers for global Xenon instance.
*/ 
VOID AgentSleep(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);

    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
    {
        return;
    }

    xenonConfig->sleeptime  = ParserGetInt32(arguments);
    xenonConfig->jitter     = ParserGetInt32(arguments);
    
    // Success
    PackageComplete(taskUuid, NULL);
}

/**
 * List Agents Current Connection host info
 */
VOID AgentStatus(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    PPackage data = PackageInit(0, NULL);

#ifdef HTTPX_TRANSPORT

    PCALLBACK_NODE Current = xenonConfig->CallbackDomainHead;  // Start at head
    int count = 0;
    while (Current) {  // Loop while the current node is not NULL
        count++;
        PackageAddFormatPrintf(
            data, 
            FALSE, 
            "%s:%d -> %s%s\n",
            Current->hostname, 
            Current->port,
            Current->isDead ? "DEAD" : "ALIVE",
            Current == xenonConfig->CallbackDomains ? "\t(current)" : ""
        );

        Current = Current->next;  // Move to the next node
    }

#endif

#ifdef SMB_TRANSPORT

    PackageAddFormatPrintf(
        data, 
        FALSE, 
        "%s -> %s\n",
        xenonConfig->SmbPipename,
        xenonConfig->SmbPipe == NULL ? "DEAD" : "ALIVE"
    );

#endif

    // Success
    PackageComplete(taskUuid, data);

    // Cleanup
    PackageDestroy(data);
}




#ifdef INCLUDE_CMD_SPAWNTO
/**
 * Update spawnto process path
 */
VOID AgentSpawnto(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);
    if (nbArg == 0)
    {
        return;
    }

    SIZE_T newLen  = 0;
    PCHAR previous = xenonConfig->spawnto;
    PCHAR new      = ParserStringCopy(arguments, &newLen);
    
    if (new == NULL || newLen == 0)
    {
        _err("Failed to update spawnto process");
        PackageError(taskUuid, 0);
    }
    
    // Update spawnto path
    xenonConfig->spawnto = new;

    _dbg("Updated Xenon SPAWNTO \"%s\"", xenonConfig->spawnto);

    // Cleanup previous spawnto path
    memset(previous, '\0', sizeof(previous));
    LocalFree(previous);
    
    // Success
    PackageComplete(taskUuid, NULL);
}

#endif  //INCLUDE_CMD_SPAWNTO



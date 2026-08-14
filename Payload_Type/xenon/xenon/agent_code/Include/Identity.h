#pragma once
#ifndef IDENTITY_H
#define IDENTITY_H

#include <windows.h>

extern HANDLE gIdentityToken;
extern BOOL gIdentityIsLoggedIn;
extern WCHAR* gIdentityDomain;
// extern WCHAR* gIdentityUsername;
// extern WCHAR* gIdentityPassword;

#define IDENTITY_MAX_WCHARS_DOMAIN 256
#define IDENTITY_MAX_WCHARS_USERNAME 256
#define IDENTITY_MAX_WCHARS_PASSWORD 512

VOID IdentityImpersonateToken(void);
VOID IdentityAgentRevertToken(void);
BOOL IdentityGetUserInfo(HANDLE hToken, char* buffer, int size);
BOOL IdentityIsAdmin(void);
BYTE IdentityGetIntegrityLevel(void);

#endif //IDENTITY_H
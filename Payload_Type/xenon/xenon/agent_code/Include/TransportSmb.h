#pragma once
#include <windows.h>

#include "Xenon.h"

#include "Parser.h"
#include "Package.h"
#include "Config.h"
#include <accctrl.h>

/* This file is the the Mythic SMB profile */
#ifdef SMB_TRANSPORT

// Localhost for little endian
#define PIPE_BUFFER_MAX 0x10000
#define LOCALHOST 0x0100007f
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

typedef struct
{
    PSID Sid;
    PSID SidLow;
    PACL SAcl;

    PSECURITY_DESCRIPTOR SecDec;
} SMB_PIPE_SEC_ATTR, *PSMB_PIPE_SEC_ATTR;


BOOL SmbSend(PPackage package);
BOOL SmbRecieve(PBYTE* ppOutData, SIZE_T* pOutLen);

// VOID SmbSecurityAttrOpen( PSMB_PIPE_SEC_ATTR SmbSecAttr, PSECURITY_ATTRIBUTES SecurityAttr );
//VOID SmbSecurityAttrFree( PSMB_PIPE_SEC_ATTR SmbSecAttr );


#endif // SMB_TRANSPORT





#pragma once
#ifndef LINK_H
#define LINK_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_LINK

#define MAX_SMB_PACKETS_PER_LOOP 30

VOID Link( PCHAR taskUuid, PPARSER arguments );
BOOL LinkAdd( PCHAR TaskUuid, PCHAR PipeName, PVOID* outBuf, SIZE_T* outLen, UINT32* LinkId );
BOOL LinkSync( PCHAR TaskUuid, PPARSER Response );
UINT32 PivotParseLinkId( PVOID buffer, SIZE_T size );
VOID LinkPush();

#endif

#ifdef INCLUDE_CMD_UNLINK

VOID UnLink( PCHAR taskUuid, PPARSER arguments );

#endif

#endif //LINK_H
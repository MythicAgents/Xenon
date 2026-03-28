#pragma once
#ifndef JOBKILL_H
#define JOBKILL_H

#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_JOB_KILL

VOID JobKill(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

#endif // INCLUDE_CMD_JOB_KILL
#endif // JOBKILL_H

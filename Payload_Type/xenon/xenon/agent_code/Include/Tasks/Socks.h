#pragma once
#ifndef SOCKS_H
#define SOCKS_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_SOCKS

VOID Socks(PCHAR taskUuid, PPARSER arguments);

#endif

#endif  //SOCKS_H
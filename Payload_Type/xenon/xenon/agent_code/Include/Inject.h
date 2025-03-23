#pragma once
#ifndef INJECT_H
#define INJECT_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

BOOL InjectProcessViaEarlyBird(_In_ PBYTE buf, _In_ SIZE_T szShellcodeLen, _Out_ PCHAR* outData);

#endif // INCLUDE_CMD_EXECUTE_ASSEMBLY

#endif  //INJECT_H
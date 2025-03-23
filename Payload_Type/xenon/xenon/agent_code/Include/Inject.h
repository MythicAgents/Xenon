#pragma once
#ifndef INJECT_H
#define INJECT_H


// #ifdef defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)

#include <windows.h>
#include "Parser.h"
#include "Config.h"

BOOL InjectProcessViaEarlyBird(PBYTE buf, SIZE_T szShellcodeLen);

// #endif // defined(INCLUDE_CMD_UPLOAD) || defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_EXECUTE_ASSEMBLY)

#endif  //INJECT_H
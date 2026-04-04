#pragma once
#ifndef LATERAL_MOVEMENT_H
#define LATERAL_MOVEMENT_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_JUMP_PSEXEC
/**
 * @brief Lateral movement via Windows Service Control Manager.
 *        Creates a temporary service on the remote host, starts it, and cleans up.
 */
VOID LateralMovementPsexec(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
#endif  // INCLUDE_CMD_JUMP_PSEXEC

#ifdef INCLUDE_CMD_JUMP_WMI
/**
 * @brief Lateral movement via WMI Win32_Process::Create.
 *        Uses COM/WMI DCOM to execute a command on the remote host.
 */
VOID LateralMovementWmi(_In_ PCHAR taskUuid, _In_ PPARSER arguments);
#endif  // INCLUDE_CMD_JUMP_WMI

#endif  // LATERAL_MOVEMENT_H

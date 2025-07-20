#pragma once

#ifndef TASK_H
#define TASK_H

#include <windows.h>
#include "Parser.h"

#define ERROR_MYTHIC_DOWNLOAD   1111
#define ERROR_MYTHIC_UPLOAD     1112
#define ERROR_MYTHIC_BOF        1113

#define NUMBER_OF_TASKS     2       // Per request

// Mythic C2 tasks
#define GET_TASKING         0x00
#define POST_RESPONSE       0x01
#define CHECKIN             0xf1
// Special
#define DOWNLOAD_INIT       0x02
#define DOWNLOAD_CONTINUE   0x03
#define UPLOAD_CHUNKED      0x04

// General
#define STATUS_CMD      0x37
#define SLEEP_CMD       0x38
#define EXAMPLE_CMD     0x40
// File system
#define RM_CMD          0x39
#define LS_CMD          0x41
#define CD_CMD          0x42
#define PWD_CMD         0x43
#define MKDIR_CMD       0x44
#define CP_CMD          0x45
#define CAT_CMD         0x46        // TODO: Might not do
// Special
#define UPLOAD_CMD      0x50
#define DOWNLOAD_CMD    0x51
#define INLINE_EXECUTE_CMD 0x53
// #define EXECUTE_ASSEMBLY_CMD 0x54
#define SPAWNTO_CMD     0x55
#define INJECT_SHELLCODE_CMD     0x56
// #define REGISTER_PROCESS_INJECT_KIT_CMD 0x57

// System enumeration
#define PS_CMD          0x52
// MISC
#define SHELL_CMD       0x60
#define PWSH_CMD        0x61        // TODO
// Token/Identity
#define GETUID_CMD      0x70
#define STEAL_TOKEN_CMD 0x71
#define MAKE_TOKEN_CMD  0x72
#define REV2SELF_CMD    0x73
// Agent
#define EXIT_CMD        0x80



typedef struct _TASK_PARAMETER {
    char* TaskUuid;        // UUID of the task
    PPARSER TaskParser;    // Task parser object
} TASK_PARAMETER, *PTASK_PARAMETER;

BOOL TaskCheckin(PPARSER checkinResponseData);
VOID TaskRoutine();

#endif //TASK_H
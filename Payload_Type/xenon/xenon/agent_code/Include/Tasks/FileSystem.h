#pragma once
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_CD
VOID FileSystemCd(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_PWD
VOID FileSystemPwd(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_MKDIR
VOID FileSystemMkdir(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_CP
VOID FileSystemCopy(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_LS
VOID FileSystemList(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_RM
VOID FileSystemRemove(PCHAR taskUuid, PPARSER arguments);
#endif

#ifdef INCLUDE_CMD_CAT
VOID FileSystemCat(PCHAR taskUuid, PPARSER arguments);
#endif

#endif //FILESYSTEM_H
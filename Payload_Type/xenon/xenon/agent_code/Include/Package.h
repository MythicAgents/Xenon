#pragma once

#ifndef PACKAGE_H
#define PACKAGE_H

#include "Parser.h"

#define TASK_COMPLETE		0x95
#define TASK_FAILED			0x99

typedef struct
{
	PVOID buffer;
	SIZE_T length;
} Package, *PPackage;

PPackage PackageInit(BYTE commandId, BOOL init);
BOOL PackageAddByte(PPackage package, BYTE byte);
BOOL PackageAddShort(PPackage package, USHORT value);
BOOL PackageAddInt32(PPackage package, UINT32 value);
BOOL PackageAddInt32_LE(PPackage package, UINT32 value);
BOOL PackageAddInt64(PPackage package, UINT64 value);
BOOL PackageAddBytes(PPackage package, PBYTE data, SIZE_T size, BOOL copySize);
BOOL PackageAddString(PPackage package, PCHAR data, BOOL copySize);
BOOL PackageAddWString(PPackage package, PWCHAR data, BOOL copySize);
BOOL PackageAddFormatPrintf(PPackage package, BOOL copySize, char *fmt, ...);
BOOL PackageSend(PPackage package, PPARSER response);
VOID PackageError(PCHAR taskUuid, UINT32 errorCode);
VOID PackageComplete(PCHAR taskUuid, PPackage package);
VOID PackageDestroy(PPackage package);

#endif
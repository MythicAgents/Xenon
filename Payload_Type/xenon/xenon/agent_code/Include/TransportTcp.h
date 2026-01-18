#pragma once
#include <windows.h>

#include "Xenon.h"

#include "Parser.h"
#include "Package.h"
#include "Config.h"

/* This file is the the Mythic TCP profile */
#ifdef TCP_TRANSPORT

// Localhost for little endian
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )


BOOL TcpSend(PPackage package);
BOOL TcpRecieve(PBYTE* ppOutData, SIZE_T* pOutLen);


#endif // TCP_TRANSPORT





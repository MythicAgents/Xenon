#pragma once

#ifndef NETWORK_H
#define NETWORK_H

#include "Package.h"
#include "Parser.h"


BOOL NetworkRequest(PPackage package, PBYTE* ppOutData, SIZE_T* pOutLen, BOOL IsGetResponse);


#endif
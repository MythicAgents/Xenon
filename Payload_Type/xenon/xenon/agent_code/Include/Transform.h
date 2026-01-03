#pragma once
#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "Xenon.h"
#include "Config.h"

#ifdef HTTPX_TRANSPORT
#include "Parser.h"
#include <windows.h>
 

typedef struct TRANSFORM
{
	const char* headers;
	const char* cookies;
	const char* uriParams;
	const char* uri;
	void* body;
	DWORD bodyLength;
	unsigned int outputLength;
	const char* transformed;
	char* temp;
	PPARSER parser;
} TRANSFORM;


BOOL TransformInit(TRANSFORM* transform, SIZE_T size);

BOOL TransformApply(TRANSFORM* transform, PBYTE bufferIn, UINT32 bufferLen, unsigned char* reqProfile);

BOOL TransformReverse(char* recoverable, DWORD recoverableLength, SIZE_T* recoveredDataLen, unsigned char* resProfile, int maxGet);

void TransformDestroy(TRANSFORM* transform);

#endif // HTTPX_TRANSPORT

#endif
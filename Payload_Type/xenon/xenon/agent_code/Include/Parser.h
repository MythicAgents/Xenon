#pragma once
#ifndef PARSER_H
#define PARSER_H

#include "Utils.h"
#include <windows.h>

#define TASK_UUID_SIZE 36

typedef struct {
    PCHAR   Original;           // Holds pointer to start of original buffer
    PCHAR   Buffer;             // Holds pointer to current area of buffer
    UINT32  OriginalSize;       // Keep track of total size
    UINT32  Length;             // Used to keep track of where we are when parsing buffer
    BOOL    Endian;
} PARSER, *PPARSER;


VOID ParserNew(PPARSER parser, PBYTE Buffer, UINT32 size);
PPARSER ParserAlloc(SIZE_T size);
VOID ParserDataParse(PPARSER parser, char* buffer, int size);
BYTE ParserGetByte(PPARSER parser);
UINT32 ParserGetInt32(PPARSER parser);
UINT64 ParserGetInt64(PPARSER parser);
PBYTE ParserGetBytes( PPARSER parser, PUINT32 size );
PCHAR ParserGetString(PPARSER parser, PSIZE_T size);
PWCHAR ParserGetWString(PPARSER parser, PSIZE_T size);
PCHAR ParserGetDataPtr(PPARSER parser, UINT32 size);
BOOL ParserStringCopySafe(PPARSER parser, char* buffer, PSIZE_T size);
PCHAR ParserStringCopy(PPARSER parser, PSIZE_T size);
BOOL ParserBase64Decode(PPARSER parser);
VOID ParserDestroy( PPARSER parser );

// BOOL ParserDecrypt(PPARSER parser);

#endif
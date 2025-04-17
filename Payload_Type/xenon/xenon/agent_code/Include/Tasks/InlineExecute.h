#pragma once
#ifndef INLINEEXECUTE_H
#define INLINEEXECUTE_H

#include <windows.h>
#include "Parser.h"
#include "Config.h"

#ifdef INCLUDE_CMD_INLINE_EXECUTE

#if defined(__x86_64__) || defined(_WIN64)
#define PREPENDSYMBOLVALUE "__imp_"
#else
#define PREPENDSYMBOLVALUE "__imp__"
#endif

#define STR_EQUALS(str, substr) (strncmp(str, substr, strlen(substr)) == 0)


typedef struct _BOF_UPLOAD {
    CHAR fileUuid[37];          // File UUID (36 + 1 for null terminator)
    UINT32 totalChunks;          // Total number of chunks
    UINT32 currentChunk;        // Current chunk number
    SIZE_T size;                // Size of the file
    PVOID buffer;               // Pointer to BOF buffer data
} BOF_UPLOAD, *PBOF_UPLOAD;


//https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-file-header-object-and-image
typedef struct coff_file_header {
    UINT16 Machine;		            
    UINT16 NumberOfSections;          
    UINT32 TimeDateStamp;             
    UINT32 PointerToSymbolTable;      
    UINT32 NumberOfSymbols;           
    UINT16 SizeOfOptionalHeader;      
    UINT16 Characteristics;           
} FileHeader_t;

#pragma pack(push,1)

//https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-table-section-headers
typedef struct coff_sections_table {
    char Name[8];
    UINT32 VirtualSize;
    UINT32 VirtualAddress;
    UINT32 SizeOfRawData;
    UINT32 PointerToRawData;
    UINT32 PointerToRelocations;
    UINT32 PointerToLineNumbers;
    UINT16 NumberOfRelocations;
    UINT16 NumberOfLinenumbers;
    UINT32 Characteristics;
} Section_t;

//https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-relocations-object-only
typedef struct coff_relocations {
    UINT32 VirtualAddress;
    UINT32 SymbolTableIndex;
    UINT16 Type;
} Relocation_t;

//https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-symbol-table
typedef struct coff_symbols_table {
    union {
        char Name[8];
        UINT32 value[2];
    } first;
    UINT32 Value;
    UINT16 SectionNumber;
    UINT16 Type;
    UINT8 StorageClass;
    UINT8 NumberOfAuxSymbols;
} Symbol_t;


typedef struct COFF {
    char* FileBase;
    FileHeader_t* FileHeader;
    Relocation_t* Relocation;
    Symbol_t* SymbolTable;

    void* RawTextData;
    char* RelocationsTextPTR;
    int RelocationsCount;
    int FunctionMappingCount;
} COFF_t;


VOID InlineExecute(PCHAR taskUuid, PPARSER arguments);

BOOL RunCOFF(char* FileData, DWORD* DataSize, char* EntryName, char* argumentdata, unsigned long argumentsize);

#endif //INCLUDE_CMD_INLINE_EXECUTE

#endif  //INLINEEXECUTE_H
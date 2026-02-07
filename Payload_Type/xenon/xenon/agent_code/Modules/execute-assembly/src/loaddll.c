/*
 * Copyright 2025 Raphael Mudge, Adversary Fan Fiction Writers Guild
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "loaderdefs.h"
#include "tcg.h"

typedef struct {
	WORD	offset:12;
	WORD	type:4;
} __IMAGE_RELOC, *__PIMAGE_RELOC;

IMAGE_DATA_DIRECTORY * GetDataDirectory(DLLDATA * dll, UINT entry) {
	return dll->OptionalHeader->DataDirectory + entry;
}

void ProcessRelocation(DLLDATA * dll, char * src, char * dst, IMAGE_BASE_RELOCATION * relocation, ULONG_PTR newBaseAddress) {
	void *          relocAddr    = PTR_OFFSET(dst, relocation->VirtualAddress);
	DWORD           relocEntries = (relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(__IMAGE_RELOC);
	__IMAGE_RELOC * relocEntry   = (__IMAGE_RELOC *)PTR_OFFSET( relocation, sizeof(IMAGE_BASE_RELOCATION) );

	for (int x = 0; x < relocEntries; x++) {
		if (relocEntry->type == IMAGE_REL_BASED_DIR64) {
			*(ULONG_PTR *)(relocAddr + relocEntry->offset) += newBaseAddress;
		}
		else if (relocEntry->type == IMAGE_REL_BASED_HIGHLOW) {
			*(DWORD *)(relocAddr + relocEntry->offset) += (DWORD)newBaseAddress;
		}
		else if (relocEntry->type == IMAGE_REL_BASED_HIGH) {
			*(WORD *)(relocAddr + relocEntry->offset) += HIWORD(newBaseAddress);
		}
		else if (relocEntry->type == IMAGE_REL_BASED_LOW) {
			*(WORD *)(relocAddr + relocEntry->offset) += LOWORD(newBaseAddress);
		}

		relocEntry++;
	}
}

void ProcessRelocations(DLLDATA * dll, char * src, char * dst) {
	IMAGE_DATA_DIRECTORY  * relocationData;
	ULONG_PTR               newBaseAddress;
	IMAGE_BASE_RELOCATION * relocation;

	relocationData = GetDataDirectory(dll, IMAGE_DIRECTORY_ENTRY_BASERELOC);

	// calculate the base address delta and perform relocations (even if we load at desired image base)
	newBaseAddress = (ULONG_PTR)dst - (ULONG_PTR)dll->OptionalHeader->ImageBase;

	/* check if there are relocations present */
	if (relocationData->Size) {
		relocation = (IMAGE_BASE_RELOCATION *)( dst + relocationData->VirtualAddress );

		while (relocation->SizeOfBlock) {
			/* process this next relocation */
			ProcessRelocation(dll, src, dst, relocation, newBaseAddress);

			/* go on to our next relocation */
			relocation = (IMAGE_BASE_RELOCATION *)PTR_OFFSET(relocation, relocation->SizeOfBlock);
		}
	}
}

void ProcessImport(IMPORTFUNCS * funcs, DLLDATA * dll, char * dst, IMAGE_IMPORT_DESCRIPTOR * importDesc) {
	void                    * hLib;
	IMAGE_THUNK_DATA        * firstThunk;
	IMAGE_THUNK_DATA        * originalFirstThunk;
	IMAGE_IMPORT_BY_NAME    * importByName;
	ULONG_PTR                 importByOrdinal;

	/* load whatever library we need here */
	hLib = (void *)funcs->LoadLibraryA((char *)PTR_OFFSET(dst, importDesc->Name));

	/* get our thunks */
	firstThunk         = (IMAGE_THUNK_DATA *)PTR_OFFSET( dst, importDesc->FirstThunk );
	originalFirstThunk = (IMAGE_THUNK_DATA *)PTR_OFFSET( dst, importDesc->OriginalFirstThunk );

	/* NOTE: IMAGE_THUNK_DATA has one union member, u1. All of the fields are the same size.
	 * The different member names seem more for semantics than anything else. We're skipping the
	 * field names in the union and just stomping over whatever is in this pointer-sized structure */

	/* https://devblogs.microsoft.com/oldnewthing/20231129-00/?p=109077 */

	while ( DEREF(firstThunk) ) {
		if ( originalFirstThunk && (originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) ) {
			/*
			 * I'm OK passing a ULONG_PTR with our ordinal. Windows (will likely) just check
			 * that our pointer is < MAXUSHORT ala ReactOS:
			 * https://doxygen.reactos.org/de/de3/dll_2win32_2kernel32_2client_2loader_8c.html#a0f3819de0cdab6061ec9e3432a85bf85
			 */
			importByOrdinal = IMAGE_ORDINAL(originalFirstThunk->u1.Ordinal);
			DEREF(firstThunk) = (ULONG_PTR)funcs->GetProcAddress(hLib, (char *)importByOrdinal);
		}
		/* OK, we are doing an import by name. */
		else {
			importByName      = (IMAGE_IMPORT_BY_NAME *)PTR_OFFSET( dst, firstThunk->u1.AddressOfData );
			DEREF(firstThunk) = (ULONG_PTR)funcs->GetProcAddress(hLib, (char *)importByName->Name);
		}

		/* increment our pointers, to look at next import option */
		firstThunk++;
		if (originalFirstThunk)
			originalFirstThunk++;
	}
}

void ProcessImports(IMPORTFUNCS * funcs, DLLDATA * dll, char * dst) {
	IMAGE_DATA_DIRECTORY    * importTableHdr;
	IMAGE_IMPORT_DESCRIPTOR * importDesc;

	/* grab our header for the import table */
	importTableHdr = GetDataDirectory(dll, IMAGE_DIRECTORY_ENTRY_IMPORT);

	/* start with the first function of our import table, we're working solely from our destination memory now */
	importDesc = (IMAGE_IMPORT_DESCRIPTOR *)PTR_OFFSET(dst, importTableHdr->VirtualAddress);

	/* walk our import table and process each of the entries */
	while (importDesc->Name) {
		ProcessImport(funcs, dll, dst, importDesc);
		importDesc++;
	}
}

void LoadSections(DLLDATA * dll, char * src, char * dst) {
	DWORD                   numberOfSections = dll->NtHeaders->FileHeader.NumberOfSections;
	IMAGE_SECTION_HEADER  * sectionHdr       = NULL;
	void                  * sectionDst       = NULL;
	void                  * sectionSrc       = NULL;

	/* our first section! */
	sectionHdr = (IMAGE_SECTION_HEADER *)PTR_OFFSET(dll->OptionalHeader, dll->NtHeaders->FileHeader.SizeOfOptionalHeader);

	for (int x = 0; x < numberOfSections; x++) {
		/* our source data to copy from */
		sectionSrc = src + sectionHdr->PointerToRawData;

		/* our destination data */
		sectionDst = dst + sectionHdr->VirtualAddress;

		/* copy our section data over */
		__movsb((unsigned char *)sectionDst, (unsigned char *)sectionSrc, sectionHdr->SizeOfRawData);
		//__builtin_memcpy(sectionDst, sectionSrc, sectionHdr->SizeOfRawData);

		/* advance to our next section */
		sectionHdr++;
	}
}

void ParseDLL(char * src, DLLDATA * data) {
	data->DosHeader      = (IMAGE_DOS_HEADER *)src;
	data->NtHeaders      = (IMAGE_NT_HEADERS *)(src + data->DosHeader->e_lfanew);
	data->OptionalHeader = (IMAGE_OPTIONAL_HEADER *)&(data->NtHeaders->OptionalHeader);
}

DLLMAIN_FUNC EntryPoint(DLLDATA * dll, void * base) {
	return (DLLMAIN_FUNC)PTR_OFFSET(base, dll->OptionalHeader->AddressOfEntryPoint);
}

DWORD SizeOfDLL(DLLDATA * data) {
	return data->OptionalHeader->SizeOfImage;
}

void LoadDLL(DLLDATA * dll, char * src, char * dst) {
	/* copy our headers over to the destination address, if we wish */
	__movsb((unsigned char *)dst, (unsigned char *)src, dll->OptionalHeader->SizeOfHeaders);

	/* load our section data */
	LoadSections(dll, src, dst);

	/* process our relocations */
	ProcessRelocations(dll, src, dst);
}

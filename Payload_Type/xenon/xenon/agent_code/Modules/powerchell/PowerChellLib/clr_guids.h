#pragma once
/* MinGW: declare CLR GUIDs defined in clr_guids.c (not in execute-assembly metahost.h) */
#ifdef __MINGW32__
#include <windows.h>
extern const GUID CLSID_CorRuntimeHost;
extern const GUID IID_ICorRuntimeHost;
/* IID__AppDomain is declared in mscorlib.h; we only define it in clr_guids.c */
#endif

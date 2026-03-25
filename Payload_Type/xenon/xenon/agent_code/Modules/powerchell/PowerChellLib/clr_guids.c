/* Define CLR/mscorlib GUIDs that MinGW doesn't provide (from mscoree/cor.h and mscorlib). */
#include "clr_guids.h"

#if defined(__MINGW32__)
#define DEFINE_CLR_GUID(name, a, b, c, d1, d2, d3, d4, d5, d6, d7, d8) \
    const GUID name = { a, b, c, { d1, d2, d3, d4, d5, d6, d7, d8 } }

DEFINE_CLR_GUID(CLSID_CorRuntimeHost, 0xCB2F6723, 0xAB3A, 0x11D2, 0x9C, 0x40, 0x00, 0xC0, 0x4F, 0xA3, 0x0A, 0x3E);
DEFINE_CLR_GUID(IID_ICorRuntimeHost,  0xCB2F6722, 0xAB3A, 0x11D2, 0x9C, 0x40, 0x00, 0xC0, 0x4F, 0xA3, 0x0A, 0x3E);
/* IID__AppDomain is defined in clr.cpp (C++ TU) to avoid link-order issues */
#endif

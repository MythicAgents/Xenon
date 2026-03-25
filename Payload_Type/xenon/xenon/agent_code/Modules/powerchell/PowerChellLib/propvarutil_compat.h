#pragma once
/* MinGW compat: propvarutil.h InitVariantFrom* for VARIANT (used when __MINGW32__) */
#include <oleauto.h>

static inline HRESULT InitVariantFromInt32(LONG lVal, VARIANT *pvar) {
    if (!pvar) return E_POINTER;
    VariantInit(pvar);
    pvar->vt = VT_I4;
    pvar->lVal = lVal;
    return S_OK;
}

static inline HRESULT InitVariantFromBoolean(BOOL bVal, VARIANT *pvar) {
    if (!pvar) return E_POINTER;
    VariantInit(pvar);
    pvar->vt = VT_BOOL;
    pvar->boolVal = bVal ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

static inline HRESULT InitVariantFromString(PCWSTR psz, VARIANT *pvar) {
    if (!pvar) return E_POINTER;
    VariantInit(pvar);
    pvar->vt = VT_BSTR;
    pvar->bstrVal = SysAllocString(psz ? psz : L"");
    return pvar->bstrVal ? S_OK : E_OUTOFMEMORY;
}

static inline HRESULT InitVariantFromStringArray(PCWSTR *ppwsz, ULONG cElems, VARIANT *pvar) {
    SAFEARRAY *psa = NULL;
    if (!pvar) return E_POINTER;
    VariantInit(pvar);
    if (cElems == 0) {
        psa = SafeArrayCreateVector(VT_BSTR, 0, 0);
    } else {
        psa = SafeArrayCreateVector(VT_BSTR, 0, cElems);
        if (psa && ppwsz) {
            for (ULONG i = 0; i < cElems; i++) {
                BSTR b = SysAllocString(ppwsz[i] ? ppwsz[i] : L"");
                SafeArrayPutElement(psa, (LONG*)&i, &b);
                if (b) SysFreeString(b);
            }
        }
    }
    if (!psa) return E_OUTOFMEMORY;
    pvar->vt = VT_ARRAY | VT_BSTR;
    pvar->parray = psa;
    return S_OK;
}

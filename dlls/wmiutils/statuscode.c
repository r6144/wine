/*
 * Copyright 2009 Hans Leidekker for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include "config.h"
#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "initguid.h"
#include "ole2.h"
#include "wbemcli.h"

#include "wine/debug.h"
#include "wine/unicode.h"
#include "wmiutils_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(wmiutils);

typedef struct status_code
{
    const IWbemStatusCodeTextVtbl *vtbl;
    LONG refs;
} status_code;

static inline status_code *impl_from_IWbemStatusCodeText( IWbemStatusCodeText *iface )
{
    return (status_code *)((char *)iface - FIELD_OFFSET( status_code, vtbl ));
}

static ULONG WINAPI status_code_AddRef(
    IWbemStatusCodeText *iface )
{
    status_code *status_code = impl_from_IWbemStatusCodeText( iface );
    return InterlockedIncrement( &status_code->refs );
}

static ULONG WINAPI status_code_Release(
    IWbemStatusCodeText *iface )
{
    status_code *status_code = impl_from_IWbemStatusCodeText( iface );
    LONG refs = InterlockedDecrement( &status_code->refs );
    if (!refs)
    {
        TRACE("destroying %p\n", status_code);
        HeapFree( GetProcessHeap(), 0, status_code );
    }
    return refs;
}

static HRESULT WINAPI status_code_QueryInterface(
    IWbemStatusCodeText *iface,
    REFIID riid,
    void **ppvObject )
{
    status_code *This = impl_from_IWbemStatusCodeText( iface );

    TRACE("%p %s %p\n", This, debugstr_guid( riid ), ppvObject );

    if ( IsEqualGUID( riid, &IID_IWbemStatusCodeText ) ||
         IsEqualGUID( riid, &IID_IUnknown ) )
    {
        *ppvObject = iface;
    }
    else
    {
        FIXME("interface %s not implemented\n", debugstr_guid(riid));
        return E_NOINTERFACE;
    }
    IWbemStatusCodeText_AddRef( iface );
    return S_OK;
}

static HRESULT WINAPI status_code_GetErrorCodeText(
    IWbemStatusCodeText *iface,
    HRESULT res,
    LCID lcid,
    LONG flags,
    BSTR *text )
{
    static const WCHAR fmt[] =
        {'E','r','r','o','r',' ','c','o','d','e',':',' ','0','x','%','0','8','x',0};
    WCHAR msg[32];

    FIXME("%p, 0x%08x, 0x%04x, 0x%08x, %p\n", iface, res, lcid, flags, text);

    sprintfW(msg, fmt, res);
    *text = SysAllocString(msg);
    return WBEM_S_NO_ERROR;
}

static HRESULT WINAPI status_code_GetFacilityCodeText(
    IWbemStatusCodeText *iface,
    HRESULT res,
    LCID lcid,
    LONG flags,
    BSTR *text )
{
    static const WCHAR fmt[] =
        {'F','a','c','i','l','i','t','y',' ','c','o','d','e',':',' ','0','x','%','0','8','x',0};
    WCHAR msg[32];

    FIXME("%p, 0x%08x, 0x%04x, 0x%08x, %p\n", iface, res, lcid, flags, text);

    sprintfW(msg, fmt, res);
    *text = SysAllocString(msg);
    return WBEM_S_NO_ERROR;
}

static const struct IWbemStatusCodeTextVtbl status_code_vtbl =
{
    status_code_QueryInterface,
    status_code_AddRef,
    status_code_Release,
    status_code_GetErrorCodeText,
    status_code_GetFacilityCodeText
};

HRESULT WbemStatusCodeText_create( IUnknown *pUnkOuter, LPVOID *ppObj )
{
    status_code *sc;

    TRACE("(%p,%p)\n", pUnkOuter, ppObj);

    sc = HeapAlloc( GetProcessHeap(), 0, sizeof(*sc) );
    if (!sc) return E_OUTOFMEMORY;

    sc->vtbl = &status_code_vtbl;
    sc->refs = 1;

    *ppObj = &sc->vtbl;

    TRACE("returning iface %p\n", *ppObj);
    return S_OK;
}

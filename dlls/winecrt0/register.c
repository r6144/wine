/*
 * Support functions for Wine dll registrations
 *
 * Copyright (c) 2010 Alexandre Julliard
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

#include "config.h"
#include <stdarg.h>

#define COBJMACROS
#define ATL_INITGUID
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "ole2.h"
#include "rpcproxy.h"
#include "atliface.h"

static const WCHAR ole32W[] = {'o','l','e','3','2','.','d','l','l',0};
static const WCHAR oleaut32W[] = {'o','l','e','a','u','t','3','2','.','d','l','l',0};
static const WCHAR regtypeW[] = {'W','I','N','E','_','R','E','G','I','S','T','R','Y',0};
static const WCHAR moduleW[] = {'M','O','D','U','L','E',0};
static const WCHAR clsidW[] = {'C','L','S','I','D','_','P','S','F','a','c','t','o','r','y','B','u','f','f','e','r',0};
static const WCHAR typelibW[] = {'T','Y','P','E','L','I','B',0};

struct reg_info
{
    IRegistrar  *registrar;
    const CLSID *clsid;
    BOOL         do_register;
    BOOL         uninit;
    HRESULT      result;
};

static HMODULE ole32;
static HMODULE oleaut32;
static HRESULT (WINAPI *pCoInitialize)(LPVOID);
static void (WINAPI *pCoUninitialize)(void);
static HRESULT (WINAPI *pCoCreateInstance)(REFCLSID,LPUNKNOWN,DWORD,REFIID,LPVOID*);
static INT (WINAPI *pStringFromGUID2)(REFGUID,LPOLESTR,INT);
static HRESULT (WINAPI *pLoadTypeLibEx)(LPCOLESTR,REGKIND,ITypeLib**);
static HRESULT (WINAPI *pUnRegisterTypeLib)(REFGUID,WORD,WORD,LCID,SYSKIND);

static IRegistrar *create_registrar( HMODULE inst, struct reg_info *info )
{
    if (!pStringFromGUID2)
    {
        if (!(ole32 = LoadLibraryW( ole32W )) ||
            !(pCoInitialize = (void *)GetProcAddress( ole32, "CoInitialize" )) ||
            !(pCoUninitialize = (void *)GetProcAddress( ole32, "CoUninitialize" )) ||
            !(pCoCreateInstance = (void *)GetProcAddress( ole32, "CoCreateInstance" )) ||
            !(pStringFromGUID2 = (void *)GetProcAddress( ole32, "StringFromGUID2" )))
        {
            info->result = E_NOINTERFACE;
            return NULL;
        }
    }
    info->uninit = SUCCEEDED( pCoInitialize( NULL ));

    info->result = pCoCreateInstance( &CLSID_Registrar, NULL, CLSCTX_INPROC_SERVER,
                                      &IID_IRegistrar, (void **)&info->registrar );
    if (SUCCEEDED( info->result ))
    {
        WCHAR str[MAX_PATH];

        GetModuleFileNameW( inst, str, MAX_PATH );
        IRegistrar_AddReplacement( info->registrar, moduleW, str );
        if (info->clsid)
        {
            pStringFromGUID2( info->clsid, str, MAX_PATH );
            IRegistrar_AddReplacement( info->registrar, clsidW, str );
        }
    }
    return info->registrar;
}

static BOOL CALLBACK register_resource( HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR arg )
{
    struct reg_info *info = (struct reg_info *)arg;
    WCHAR *buffer;
    HRSRC rsrc = FindResourceW( module, name, type );
    char *str = LoadResource( module, rsrc );
    DWORD lenW, lenA = SizeofResource( module, rsrc );

    if (!str) return FALSE;
    if (!info->registrar && !create_registrar( module, info )) return FALSE;
    lenW = MultiByteToWideChar( CP_UTF8, 0, str, lenA, NULL, 0 ) + 1;
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, lenW * sizeof(WCHAR) )))
    {
        info->result = E_OUTOFMEMORY;
        return FALSE;
    }
    MultiByteToWideChar( CP_UTF8, 0, str, lenA, buffer, lenW );
    buffer[lenW - 1] = 0;

    if (info->do_register)
        info->result = IRegistrar_StringRegister( info->registrar, buffer );
    else
        info->result = IRegistrar_StringUnregister( info->registrar, buffer );

    HeapFree( GetProcessHeap(), 0, buffer );
    return SUCCEEDED(info->result);
}

static HRESULT register_typelib( HMODULE module, BOOL do_register )
{
    WCHAR name[MAX_PATH];
    ITypeLib *typelib;
    HRESULT ret;

    if (!pUnRegisterTypeLib)
    {
        if (!(oleaut32 = LoadLibraryW( oleaut32W )) ||
            !(pLoadTypeLibEx = (void *)GetProcAddress( oleaut32, "LoadTypeLibEx" )) ||
            !(pUnRegisterTypeLib = (void *)GetProcAddress( oleaut32, "UnRegisterTypeLib" )))
            return E_FAIL;
    }
    GetModuleFileNameW( module, name, MAX_PATH );
    if (do_register)
    {
        ret = pLoadTypeLibEx( name, REGKIND_REGISTER, &typelib );
    }
    else
    {
        ret = pLoadTypeLibEx( name, REGKIND_NONE, &typelib );
        if (SUCCEEDED( ret ))
        {
            TLIBATTR *attr;
            ITypeLib_GetLibAttr( typelib, &attr );
            ret = pUnRegisterTypeLib( &attr->guid, attr->wMajorVerNum, attr->wMinorVerNum,
                                      attr->lcid, attr->syskind );
            ITypeLib_ReleaseTLibAttr( typelib, attr );
        }
    }
    if (SUCCEEDED( ret )) ITypeLib_Release( typelib );
    return ret;
}

HRESULT __wine_register_resources( HMODULE module, const CLSID *clsid )
{
    struct reg_info info;

    info.registrar = NULL;
    info.clsid = clsid;
    info.do_register = TRUE;
    info.uninit = FALSE;
    info.result = S_OK;
    EnumResourceNamesW( module, regtypeW, register_resource, (LONG_PTR)&info );
    if (info.registrar) IRegistrar_Release( info.registrar );
    if (SUCCEEDED(info.result) && FindResourceW( module, MAKEINTRESOURCEW(1), typelibW ))
        info.result = register_typelib( module, TRUE );
    if (info.uninit) pCoUninitialize();
    return info.result;
}

HRESULT __wine_unregister_resources( HMODULE module, const CLSID *clsid )
{
    struct reg_info info;

    info.registrar = NULL;
    info.clsid = clsid;
    info.do_register = FALSE;
    info.uninit = FALSE;
    info.result = S_OK;
    EnumResourceNamesW( module, regtypeW, register_resource, (LONG_PTR)&info );
    if (info.registrar) IRegistrar_Release( info.registrar );
    if (SUCCEEDED(info.result) && FindResourceW( module, MAKEINTRESOURCEW(1), typelibW ))
        info.result = register_typelib( module, FALSE );
    if (info.uninit) pCoUninitialize();
    return info.result;
}

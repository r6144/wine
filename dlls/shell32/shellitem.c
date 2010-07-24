/*
 * IShellItem implementation
 *
 * Copyright 2008 Vincent Povirk for CodeWeavers
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
#include "wine/port.h"

#include <stdio.h>
#include <stdarg.h>

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

#include "pidl.h"
#include "shell32_main.h"
#include "debughlp.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

typedef struct _ShellItem {
    const IShellItemVtbl    *lpIShellItemVtbl;
    LONG                    ref;
    LPITEMIDLIST            pidl;
    const IPersistIDListVtbl *lpIPersistIDListVtbl;
} ShellItem;


static inline ShellItem *impl_from_IPersistIDList( IPersistIDList *iface )
{
    return (ShellItem*)((char*)iface - FIELD_OFFSET(ShellItem, lpIPersistIDListVtbl));
}


static HRESULT WINAPI ShellItem_QueryInterface(IShellItem *iface, REFIID riid,
    void **ppv)
{
    ShellItem *This = (ShellItem*)iface;

    TRACE("(%p,%p,%p)\n", iface, riid, ppv);

    if (!ppv) return E_INVALIDARG;

    if (IsEqualIID(&IID_IUnknown, riid) || IsEqualIID(&IID_IShellItem, riid))
    {
        *ppv = This;
    }
    else if (IsEqualIID(&IID_IPersist, riid) || IsEqualIID(&IID_IPersistIDList, riid))
    {
        *ppv = &(This->lpIPersistIDListVtbl);
    }
    else {
        FIXME("not implemented for %s\n", shdebugstr_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ShellItem_AddRef(IShellItem *iface)
{
    ShellItem *This = (ShellItem*)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p), new refcount=%i\n", iface, ref);

    return ref;
}

static ULONG WINAPI ShellItem_Release(IShellItem *iface)
{
    ShellItem *This = (ShellItem*)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p), new refcount=%i\n", iface, ref);

    if (ref == 0)
    {
        ILFree(This->pidl);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT ShellItem_get_parent_pidl(ShellItem *This, LPITEMIDLIST *parent_pidl)
{
    *parent_pidl = ILClone(This->pidl);
    if (*parent_pidl)
    {
        if (ILRemoveLastID(*parent_pidl))
            return S_OK;
        else
        {
            ILFree(*parent_pidl);
            *parent_pidl = NULL;
            return E_INVALIDARG;
        }
    }
    else
    {
        *parent_pidl = NULL;
        return E_OUTOFMEMORY;
    }
}

static HRESULT ShellItem_get_parent_shellfolder(ShellItem *This, IShellFolder **ppsf)
{
    LPITEMIDLIST parent_pidl;
    IShellFolder *desktop;
    HRESULT ret;

    ret = ShellItem_get_parent_pidl(This, &parent_pidl);
    if (SUCCEEDED(ret))
    {
        ret = SHGetDesktopFolder(&desktop);
        if (SUCCEEDED(ret))
        {
            ret = IShellFolder_BindToObject(desktop, parent_pidl, NULL, &IID_IShellFolder, (void**)ppsf);
            IShellFolder_Release(desktop);
        }
        ILFree(parent_pidl);
    }

    return ret;
}

static HRESULT WINAPI ShellItem_BindToHandler(IShellItem *iface, IBindCtx *pbc,
    REFGUID rbhid, REFIID riid, void **ppvOut)
{
    FIXME("(%p,%p,%s,%p,%p)\n", iface, pbc, shdebugstr_guid(rbhid), riid, ppvOut);

    *ppvOut = NULL;

    return E_NOTIMPL;
}

static HRESULT WINAPI ShellItem_GetParent(IShellItem *iface, IShellItem **ppsi)
{
    ShellItem *This = (ShellItem*)iface;
    LPITEMIDLIST parent_pidl;
    HRESULT ret;

    TRACE("(%p,%p)\n", iface, ppsi);

    ret = ShellItem_get_parent_pidl(This, &parent_pidl);
    if (SUCCEEDED(ret))
    {
        ret = SHCreateShellItem(NULL, NULL, parent_pidl, ppsi);
        ILFree(parent_pidl);
    }

    return ret;
}

static HRESULT WINAPI ShellItem_GetDisplayName(IShellItem *iface, SIGDN sigdnName,
    LPWSTR *ppszName)
{
    ShellItem *This = (ShellItem*)iface;
    TRACE("(%p,%x,%p)\n", iface, sigdnName, ppszName);

    return SHGetNameFromIDList(This->pidl, sigdnName, ppszName);
}

static HRESULT WINAPI ShellItem_GetAttributes(IShellItem *iface, SFGAOF sfgaoMask,
    SFGAOF *psfgaoAttribs)
{
    ShellItem *This = (ShellItem*)iface;
    IShellFolder *parent_folder;
    LPITEMIDLIST child_pidl;
    HRESULT ret;

    TRACE("(%p,%x,%p)\n", iface, sfgaoMask, psfgaoAttribs);

    ret = ShellItem_get_parent_shellfolder(This, &parent_folder);
    if (SUCCEEDED(ret))
    {
        child_pidl = ILFindLastID(This->pidl);
        *psfgaoAttribs = sfgaoMask;
        ret = IShellFolder_GetAttributesOf(parent_folder, 1, (LPCITEMIDLIST*)&child_pidl, psfgaoAttribs);
        IShellFolder_Release(parent_folder);
    }

    return ret;
}

static HRESULT WINAPI ShellItem_Compare(IShellItem *iface, IShellItem *oth,
    SICHINTF hint, int *piOrder)
{
    FIXME("(%p,%p,%x,%p)\n", iface, oth, hint, piOrder);

    return E_NOTIMPL;
}

static const IShellItemVtbl ShellItem_Vtbl = {
    ShellItem_QueryInterface,
    ShellItem_AddRef,
    ShellItem_Release,
    ShellItem_BindToHandler,
    ShellItem_GetParent,
    ShellItem_GetDisplayName,
    ShellItem_GetAttributes,
    ShellItem_Compare
};


static HRESULT ShellItem_GetClassID(ShellItem* This, CLSID *pClassID)
{
    TRACE("(%p,%p)\n", This, pClassID);

    *pClassID = CLSID_ShellItem;
    return S_OK;
}


static HRESULT WINAPI ShellItem_IPersistIDList_QueryInterface(IPersistIDList *iface,
    REFIID riid, void **ppv)
{
    ShellItem *This = impl_from_IPersistIDList(iface);
    return ShellItem_QueryInterface((IShellItem*)This, riid, ppv);
}

static ULONG WINAPI ShellItem_IPersistIDList_AddRef(IPersistIDList *iface)
{
    ShellItem *This = impl_from_IPersistIDList(iface);
    return ShellItem_AddRef((IShellItem*)This);
}

static ULONG WINAPI ShellItem_IPersistIDList_Release(IPersistIDList *iface)
{
    ShellItem *This = impl_from_IPersistIDList(iface);
    return ShellItem_Release((IShellItem*)This);
}

static HRESULT WINAPI ShellItem_IPersistIDList_GetClassID(IPersistIDList* iface,
    CLSID *pClassID)
{
    ShellItem *This = impl_from_IPersistIDList(iface);

    return ShellItem_GetClassID(This, pClassID);
}

static HRESULT WINAPI ShellItem_IPersistIDList_SetIDList(IPersistIDList* iface,
    LPCITEMIDLIST pidl)
{
    ShellItem *This = impl_from_IPersistIDList(iface);
    LPITEMIDLIST new_pidl;

    TRACE("(%p,%p)\n", This, pidl);

    new_pidl = ILClone(pidl);

    if (new_pidl)
    {
        ILFree(This->pidl);
        This->pidl = new_pidl;
        return S_OK;
    }
    else
        return E_OUTOFMEMORY;
}

static HRESULT WINAPI ShellItem_IPersistIDList_GetIDList(IPersistIDList* iface,
    LPITEMIDLIST *ppidl)
{
    ShellItem *This = impl_from_IPersistIDList(iface);

    TRACE("(%p,%p)\n", This, ppidl);

    *ppidl = ILClone(This->pidl);
    if (*ppidl)
        return S_OK;
    else
        return E_OUTOFMEMORY;
}

static const IPersistIDListVtbl ShellItem_IPersistIDList_Vtbl = {
    ShellItem_IPersistIDList_QueryInterface,
    ShellItem_IPersistIDList_AddRef,
    ShellItem_IPersistIDList_Release,
    ShellItem_IPersistIDList_GetClassID,
    ShellItem_IPersistIDList_SetIDList,
    ShellItem_IPersistIDList_GetIDList
};


HRESULT WINAPI IShellItem_Constructor(IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
    ShellItem *This;
    HRESULT ret;

    TRACE("(%p,%s)\n",pUnkOuter, debugstr_guid(riid));

    *ppv = NULL;

    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    This = HeapAlloc(GetProcessHeap(), 0, sizeof(ShellItem));
    This->lpIShellItemVtbl = &ShellItem_Vtbl;
    This->ref = 1;
    This->pidl = NULL;
    This->lpIPersistIDListVtbl = &ShellItem_IPersistIDList_Vtbl;

    ret = ShellItem_QueryInterface((IShellItem*)This, riid, ppv);
    ShellItem_Release((IShellItem*)This);

    return ret;
}

HRESULT WINAPI SHCreateShellItem(LPCITEMIDLIST pidlParent,
    IShellFolder *psfParent, LPCITEMIDLIST pidl, IShellItem **ppsi)
{
    ShellItem *This;
    LPITEMIDLIST new_pidl;
    HRESULT ret;

    TRACE("(%p,%p,%p,%p)\n", pidlParent, psfParent, pidl, ppsi);

    if (!pidl)
    {
        return E_INVALIDARG;
    }
    else if (pidlParent || psfParent)
    {
        LPITEMIDLIST temp_parent=NULL;
        if (!pidlParent)
        {
            IPersistFolder2* ppf2Parent;

            if (FAILED(IPersistFolder2_QueryInterface(psfParent, &IID_IPersistFolder2, (void**)&ppf2Parent)))
            {
                FIXME("couldn't get IPersistFolder2 interface of parent\n");
                return E_NOINTERFACE;
            }

            if (FAILED(IPersistFolder2_GetCurFolder(ppf2Parent, &temp_parent)))
            {
                FIXME("couldn't get parent PIDL\n");
                IPersistFolder2_Release(ppf2Parent);
                return E_NOINTERFACE;
            }

            pidlParent = temp_parent;
            IPersistFolder2_Release(ppf2Parent);
        }

        new_pidl = ILCombine(pidlParent, pidl);
        ILFree(temp_parent);

        if (!new_pidl)
            return E_OUTOFMEMORY;
    }
    else
    {
        new_pidl = ILClone(pidl);
        if (!new_pidl)
            return E_OUTOFMEMORY;
    }

    ret = IShellItem_Constructor(NULL, &IID_IShellItem, (void**)&This);
    if (This)
    {
        *ppsi = (IShellItem*)This;
        This->pidl = new_pidl;
    }
    else
    {
        *ppsi = NULL;
        ILFree(new_pidl);
    }
    return ret;
}

HRESULT WINAPI SHCreateItemFromParsingName(PCWSTR pszPath,
    IBindCtx *pbc, REFIID riid, void **ppv)
{
    LPITEMIDLIST pidl;
    HRESULT ret;

    *ppv = NULL;

    ret = SHParseDisplayName(pszPath, pbc, &pidl, 0, NULL);
    if(SUCCEEDED(ret))
    {
        ShellItem *This;
        ret = IShellItem_Constructor(NULL, riid, (void**)&This);

        if(SUCCEEDED(ret))
        {
            This->pidl = pidl;
            *ppv = (void*)This;
        }
        else
        {
            ILFree(pidl);
        }
    }
    return ret;
}

HRESULT WINAPI SHCreateItemFromIDList(PCIDLIST_ABSOLUTE pidl, REFIID riid, void **ppv)
{
    ShellItem *psiimpl;
    HRESULT ret;

    if(!pidl)
        return E_INVALIDARG;

    ret = IShellItem_Constructor(NULL, riid, ppv);
    if(SUCCEEDED(ret))
    {
        psiimpl = (ShellItem*)*ppv;
        psiimpl->pidl = ILClone(pidl);
    }

    return ret;
}

HRESULT WINAPI SHGetItemFromDataObject(IDataObject *pdtobj,
    DATAOBJ_GET_ITEM_FLAGS dwFlags, REFIID riid, void **ppv)
{
    FORMATETC fmt;
    STGMEDIUM medium;
    HRESULT ret;

    TRACE("%p, %x, %s, %p\n", pdtobj, dwFlags, debugstr_guid(riid), ppv);

    if(!pdtobj)
        return E_INVALIDARG;

    fmt.cfFormat = RegisterClipboardFormatW(CFSTR_SHELLIDLISTW);
    fmt.ptd = NULL;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    ret = IDataObject_GetData(pdtobj, &fmt, &medium);
    if(SUCCEEDED(ret))
    {
        LPIDA pida = GlobalLock(medium.u.hGlobal);

        if((pida->cidl > 1 && !(dwFlags & DOGIF_ONLY_IF_ONE)) ||
           pida->cidl == 1)
        {
            LPITEMIDLIST pidl;

            /* Get the first pidl (parent + child1) */
            pidl = ILCombine((LPCITEMIDLIST) ((LPBYTE)pida+pida->aoffset[0]),
                             (LPCITEMIDLIST) ((LPBYTE)pida+pida->aoffset[1]));

            ret = SHCreateItemFromIDList(pidl, riid, ppv);
            ILFree(pidl);
        }
        else
        {
            ret = E_FAIL;
        }

        GlobalUnlock(medium.u.hGlobal);
        GlobalFree(medium.u.hGlobal);
    }

    if(FAILED(ret) && !(dwFlags & DOGIF_NO_HDROP))
    {
        TRACE("Attempting to fall back on CF_HDROP.\n");

        fmt.cfFormat = CF_HDROP;
        fmt.ptd = NULL;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex = -1;
        fmt.tymed = TYMED_HGLOBAL;

        ret = IDataObject_GetData(pdtobj, &fmt, &medium);
        if(SUCCEEDED(ret))
        {
            DROPFILES *df = GlobalLock(medium.u.hGlobal);
            LPBYTE files = (LPBYTE)df + df->pFiles;
            BOOL multiple_files = FALSE;

            ret = E_FAIL;
            if(!df->fWide)
            {
                WCHAR filename[MAX_PATH];
                PCSTR first_file = (PCSTR)files;
                if(*(files + lstrlenA(first_file) + 1) != 0)
                    multiple_files = TRUE;

                if( !(multiple_files && (dwFlags & DOGIF_ONLY_IF_ONE)) )
                {
                    MultiByteToWideChar(CP_ACP, 0, first_file, -1, filename, MAX_PATH);
                    ret = SHCreateItemFromParsingName(filename, NULL, riid, ppv);
                }
            }
            else
            {
                PCWSTR first_file = (PCWSTR)files;
                if(*((PCWSTR)files + lstrlenW(first_file) + 1) != 0)
                    multiple_files = TRUE;

                if( !(multiple_files && (dwFlags & DOGIF_ONLY_IF_ONE)) )
                    ret = SHCreateItemFromParsingName(first_file, NULL, riid, ppv);
            }

            GlobalUnlock(medium.u.hGlobal);
            GlobalFree(medium.u.hGlobal);
        }
    }

    if(FAILED(ret) && !(dwFlags & DOGIF_NO_URL))
    {
        FIXME("Failed to create item, should try CF_URL.\n");
    }

    return ret;
}

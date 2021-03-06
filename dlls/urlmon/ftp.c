/*
 * Copyright 2005-2009 Jacek Caban for CodeWeavers
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

#include "urlmon_main.h"

#define NO_SHLWAPI_REG
#include "shlwapi.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(urlmon);

typedef struct {
    Protocol base;

    const IInternetProtocolExVtbl*lpIInternetProtocolExVtbl;
    const IInternetPriorityVtbl  *lpInternetPriorityVtbl;
    const IWinInetHttpInfoVtbl   *lpWinInetHttpInfoVtbl;

    LONG ref;
} FtpProtocol;

#define PROTOCOLEX(x)    ((IInternetProtocolEx*)&(x)->lpIInternetProtocolExVtbl)
#define PRIORITY(x)      ((IInternetPriority*)  &(x)->lpInternetPriorityVtbl)
#define INETHTTPINFO(x)  ((IWinInetHttpInfo*)   &(x)->lpWinInetHttpInfoVtbl)

#define ASYNCPROTOCOL_THIS(iface) DEFINE_THIS2(FtpProtocol, base, iface)

static HRESULT FtpProtocol_open_request(Protocol *prot, IUri *uri, DWORD request_flags,
        HINTERNET internet_session, IInternetBindInfo *bind_info)
{
    FtpProtocol *This = ASYNCPROTOCOL_THIS(prot);
    BSTR url;
    HRESULT hres;

    hres = IUri_GetAbsoluteUri(uri, &url);
    if(FAILED(hres))
        return hres;

    This->base.request = InternetOpenUrlW(internet_session, url, NULL, 0,
            request_flags|INTERNET_FLAG_EXISTING_CONNECT|INTERNET_FLAG_PASSIVE,
            (DWORD_PTR)&This->base);
    SysFreeString(url);
    if (!This->base.request && GetLastError() != ERROR_IO_PENDING) {
        WARN("InternetOpenUrl failed: %d\n", GetLastError());
        return INET_E_RESOURCE_NOT_FOUND;
    }

    return S_OK;
}

static HRESULT FtpProtocol_end_request(Protocol *prot)
{
    return E_NOTIMPL;
}

static HRESULT FtpProtocol_start_downloading(Protocol *prot)
{
    FtpProtocol *This = ASYNCPROTOCOL_THIS(prot);
    DWORD size;
    BOOL res;

    res = FtpGetFileSize(This->base.request, &size);
    if(res)
        This->base.content_length = size;
    else
        WARN("FtpGetFileSize failed: %d\n", GetLastError());

    return S_OK;
}

static void FtpProtocol_close_connection(Protocol *prot)
{
}

#undef ASYNCPROTOCOL_THIS

static const ProtocolVtbl AsyncProtocolVtbl = {
    FtpProtocol_open_request,
    FtpProtocol_end_request,
    FtpProtocol_start_downloading,
    FtpProtocol_close_connection
};

#define PROTOCOL_THIS(iface) DEFINE_THIS(FtpProtocol, IInternetProtocolEx, iface)

static HRESULT WINAPI FtpProtocol_QueryInterface(IInternetProtocolEx *iface, REFIID riid, void **ppv)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    *ppv = NULL;
    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = PROTOCOLEX(This);
    }else if(IsEqualGUID(&IID_IInternetProtocolRoot, riid)) {
        TRACE("(%p)->(IID_IInternetProtocolRoot %p)\n", This, ppv);
        *ppv = PROTOCOLEX(This);
    }else if(IsEqualGUID(&IID_IInternetProtocol, riid)) {
        TRACE("(%p)->(IID_IInternetProtocol %p)\n", This, ppv);
        *ppv = PROTOCOLEX(This);
    }else if(IsEqualGUID(&IID_IInternetProtocolEx, riid)) {
        TRACE("(%p)->(IID_IInternetProtocolEx %p)\n", This, ppv);
        *ppv = PROTOCOLEX(This);
    }else if(IsEqualGUID(&IID_IInternetPriority, riid)) {
        TRACE("(%p)->(IID_IInternetPriority %p)\n", This, ppv);
        *ppv = PRIORITY(This);
    }else if(IsEqualGUID(&IID_IWinInetInfo, riid)) {
        TRACE("(%p)->(IID_IWinInetInfo %p)\n", This, ppv);
        *ppv = INETHTTPINFO(This);
    }else if(IsEqualGUID(&IID_IWinInetHttpInfo, riid)) {
        TRACE("(%p)->(IID_IWinInetHttpInfo %p)\n", This, ppv);
        *ppv = INETHTTPINFO(This);
    }

    if(*ppv) {
        IInternetProtocol_AddRef(iface);
        return S_OK;
    }

    WARN("not supported interface %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI FtpProtocol_AddRef(IInternetProtocolEx *iface)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ref=%d\n", This, ref);
    return ref;
}

static ULONG WINAPI FtpProtocol_Release(IInternetProtocolEx *iface)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        protocol_close_connection(&This->base);
        heap_free(This);

        URLMON_UnlockModule();
    }

    return ref;
}

static HRESULT WINAPI FtpProtocol_Start(IInternetProtocolEx *iface, LPCWSTR szUrl,
        IInternetProtocolSink *pOIProtSink, IInternetBindInfo *pOIBindInfo,
        DWORD grfPI, HANDLE_PTR dwReserved)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%s %p %p %08x %lx)\n", This, debugstr_w(szUrl), pOIProtSink,
          pOIBindInfo, grfPI, dwReserved);

    hres = CreateUri(szUrl, 0, 0, &uri);
    if(FAILED(hres))
        return hres;

    hres = IInternetProtocolEx_StartEx(PROTOCOLEX(This), uri, pOIProtSink, pOIBindInfo,
            grfPI, (HANDLE*)dwReserved);

    IUri_Release(uri);
    return hres;
}

static HRESULT WINAPI FtpProtocol_Continue(IInternetProtocolEx *iface, PROTOCOLDATA *pProtocolData)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pProtocolData);

    return protocol_continue(&This->base, pProtocolData);
}

static HRESULT WINAPI FtpProtocol_Abort(IInternetProtocolEx *iface, HRESULT hrReason,
        DWORD dwOptions)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    TRACE("(%p)->(%08x %08x)\n", This, hrReason, dwOptions);

    return protocol_abort(&This->base, hrReason);
}

static HRESULT WINAPI FtpProtocol_Terminate(IInternetProtocolEx *iface, DWORD dwOptions)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    TRACE("(%p)->(%08x)\n", This, dwOptions);

    protocol_close_connection(&This->base);
    return S_OK;
}

static HRESULT WINAPI FtpProtocol_Suspend(IInternetProtocolEx *iface)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI FtpProtocol_Resume(IInternetProtocolEx *iface)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI FtpProtocol_Read(IInternetProtocolEx *iface, void *pv,
        ULONG cb, ULONG *pcbRead)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    TRACE("(%p)->(%p %u %p)\n", This, pv, cb, pcbRead);

    return protocol_read(&This->base, pv, cb, pcbRead);
}

static HRESULT WINAPI FtpProtocol_Seek(IInternetProtocolEx *iface, LARGE_INTEGER dlibMove,
        DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    FIXME("(%p)->(%d %d %p)\n", This, dlibMove.u.LowPart, dwOrigin, plibNewPosition);
    return E_NOTIMPL;
}

static HRESULT WINAPI FtpProtocol_LockRequest(IInternetProtocolEx *iface, DWORD dwOptions)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    TRACE("(%p)->(%08x)\n", This, dwOptions);

    return protocol_lock_request(&This->base);
}

static HRESULT WINAPI FtpProtocol_UnlockRequest(IInternetProtocolEx *iface)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);

    TRACE("(%p)\n", This);

    return protocol_unlock_request(&This->base);
}

static HRESULT WINAPI FtpProtocol_StartEx(IInternetProtocolEx *iface, IUri *pUri,
        IInternetProtocolSink *pOIProtSink, IInternetBindInfo *pOIBindInfo,
        DWORD grfPI, HANDLE *dwReserved)
{
    FtpProtocol *This = PROTOCOL_THIS(iface);
    DWORD scheme = 0;
    HRESULT hres;

    TRACE("(%p)->(%p %p %p %08x %p)\n", This, pUri, pOIProtSink,
            pOIBindInfo, grfPI, dwReserved);

    hres = IUri_GetScheme(pUri, &scheme);
    if(FAILED(hres))
        return hres;
    if(scheme != URL_SCHEME_FTP)
        return MK_E_SYNTAX;

    return protocol_start(&This->base, (IInternetProtocol*)PROTOCOLEX(This), pUri, pOIProtSink, pOIBindInfo);
}
#undef PROTOCOL_THIS

static const IInternetProtocolExVtbl FtpProtocolVtbl = {
    FtpProtocol_QueryInterface,
    FtpProtocol_AddRef,
    FtpProtocol_Release,
    FtpProtocol_Start,
    FtpProtocol_Continue,
    FtpProtocol_Abort,
    FtpProtocol_Terminate,
    FtpProtocol_Suspend,
    FtpProtocol_Resume,
    FtpProtocol_Read,
    FtpProtocol_Seek,
    FtpProtocol_LockRequest,
    FtpProtocol_UnlockRequest,
    FtpProtocol_StartEx
};

#define PRIORITY_THIS(iface) DEFINE_THIS(FtpProtocol, InternetPriority, iface)

static HRESULT WINAPI FtpPriority_QueryInterface(IInternetPriority *iface, REFIID riid, void **ppv)
{
    FtpProtocol *This = PRIORITY_THIS(iface);
    return IInternetProtocolEx_QueryInterface(PROTOCOLEX(This), riid, ppv);
}

static ULONG WINAPI FtpPriority_AddRef(IInternetPriority *iface)
{
    FtpProtocol *This = PRIORITY_THIS(iface);
    return IInternetProtocolEx_AddRef(PROTOCOLEX(This));
}

static ULONG WINAPI FtpPriority_Release(IInternetPriority *iface)
{
    FtpProtocol *This = PRIORITY_THIS(iface);
    return IInternetProtocolEx_Release(PROTOCOLEX(This));
}

static HRESULT WINAPI FtpPriority_SetPriority(IInternetPriority *iface, LONG nPriority)
{
    FtpProtocol *This = PRIORITY_THIS(iface);

    TRACE("(%p)->(%d)\n", This, nPriority);

    This->base.priority = nPriority;
    return S_OK;
}

static HRESULT WINAPI FtpPriority_GetPriority(IInternetPriority *iface, LONG *pnPriority)
{
    FtpProtocol *This = PRIORITY_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pnPriority);

    *pnPriority = This->base.priority;
    return S_OK;
}

#undef PRIORITY_THIS

static const IInternetPriorityVtbl FtpPriorityVtbl = {
    FtpPriority_QueryInterface,
    FtpPriority_AddRef,
    FtpPriority_Release,
    FtpPriority_SetPriority,
    FtpPriority_GetPriority
};

#define INETINFO_THIS(iface) DEFINE_THIS(FtpProtocol, WinInetHttpInfo, iface)

static HRESULT WINAPI HttpInfo_QueryInterface(IWinInetHttpInfo *iface, REFIID riid, void **ppv)
{
    FtpProtocol *This = INETINFO_THIS(iface);
    return IInternetProtocolEx_QueryInterface(PROTOCOLEX(This), riid, ppv);
}

static ULONG WINAPI HttpInfo_AddRef(IWinInetHttpInfo *iface)
{
    FtpProtocol *This = INETINFO_THIS(iface);
    return IInternetProtocolEx_AddRef(PROTOCOLEX(This));
}

static ULONG WINAPI HttpInfo_Release(IWinInetHttpInfo *iface)
{
    FtpProtocol *This = INETINFO_THIS(iface);
    return IInternetProtocolEx_Release(PROTOCOLEX(This));
}

static HRESULT WINAPI HttpInfo_QueryOption(IWinInetHttpInfo *iface, DWORD dwOption,
        void *pBuffer, DWORD *pcbBuffer)
{
    FtpProtocol *This = INETINFO_THIS(iface);
    FIXME("(%p)->(%x %p %p)\n", This, dwOption, pBuffer, pcbBuffer);
    return E_NOTIMPL;
}

static HRESULT WINAPI HttpInfo_QueryInfo(IWinInetHttpInfo *iface, DWORD dwOption,
        void *pBuffer, DWORD *pcbBuffer, DWORD *pdwFlags, DWORD *pdwReserved)
{
    FtpProtocol *This = INETINFO_THIS(iface);
    FIXME("(%p)->(%x %p %p %p %p)\n", This, dwOption, pBuffer, pcbBuffer, pdwFlags, pdwReserved);
    return E_NOTIMPL;
}

#undef INETINFO_THIS

static const IWinInetHttpInfoVtbl WinInetHttpInfoVtbl = {
    HttpInfo_QueryInterface,
    HttpInfo_AddRef,
    HttpInfo_Release,
    HttpInfo_QueryOption,
    HttpInfo_QueryInfo
};

HRESULT FtpProtocol_Construct(IUnknown *pUnkOuter, LPVOID *ppobj)
{
    FtpProtocol *ret;

    TRACE("(%p %p)\n", pUnkOuter, ppobj);

    URLMON_LockModule();

    ret = heap_alloc_zero(sizeof(FtpProtocol));

    ret->base.vtbl = &AsyncProtocolVtbl;
    ret->lpIInternetProtocolExVtbl = &FtpProtocolVtbl;
    ret->lpInternetPriorityVtbl    = &FtpPriorityVtbl;
    ret->lpWinInetHttpInfoVtbl     = &WinInetHttpInfoVtbl;
    ret->ref = 1;

    *ppobj = PROTOCOLEX(ret);
    
    return S_OK;
}

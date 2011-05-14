/*
 * Copyright 2006-2010 Jacek Caban for CodeWeavers
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
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"
#include "hlguids.h"
#include "shlguid.h"
#include "wininet.h"
#include "shlwapi.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define CONTENT_LENGTH "Content-Length"
#define UTF16_STR "utf-16"

typedef struct {
    nsIInputStream nsIInputStream_iface;

    LONG ref;

    char buf[1024];
    DWORD buf_size;
} nsProtocolStream;

typedef struct {
    void (*destroy)(BSCallback*);
    HRESULT (*init_bindinfo)(BSCallback*);
    HRESULT (*start_binding)(BSCallback*);
    HRESULT (*stop_binding)(BSCallback*,HRESULT);
    HRESULT (*read_data)(BSCallback*,IStream*);
    HRESULT (*on_progress)(BSCallback*,ULONG,LPCWSTR);
    HRESULT (*on_response)(BSCallback*,DWORD,LPCWSTR);
    HRESULT (*beginning_transaction)(BSCallback*,WCHAR**);
} BSCallbackVtbl;

struct BSCallback {
    IBindStatusCallback IBindStatusCallback_iface;
    IServiceProvider    IServiceProvider_iface;
    IHttpNegotiate2     IHttpNegotiate2_iface;
    IInternetBindInfo   IInternetBindInfo_iface;

    const BSCallbackVtbl          *vtbl;

    LONG ref;

    LPWSTR headers;
    HGLOBAL post_data;
    ULONG post_data_len;
    ULONG readed;
    DWORD bindf;
    BOOL bindinfo_ready;

    IMoniker *mon;
    IBinding *binding;

    HTMLDocumentNode *doc;

    struct list entry;
};

static inline nsProtocolStream *impl_from_nsIInputStream(nsIInputStream *iface)
{
    return CONTAINING_RECORD(iface, nsProtocolStream, nsIInputStream_iface);
}

static nsresult NSAPI nsInputStream_QueryInterface(nsIInputStream *iface, nsIIDRef riid,
        void **result)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);

    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result  = &This->nsIInputStream_iface;
    }else if(IsEqualGUID(&IID_nsIInputStream, riid)) {
        TRACE("(%p)->(IID_nsIInputStream %p)\n", This, result);
        *result  = &This->nsIInputStream_iface;
    }

    if(*result) {
        nsIInputStream_AddRef(&This->nsIInputStream_iface);
        return NS_OK;
    }

    WARN("unsupported interface %s\n", debugstr_guid(riid));
    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsInputStream_AddRef(nsIInputStream *iface)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}


static nsrefcnt NSAPI nsInputStream_Release(nsIInputStream *iface)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref)
        heap_free(This);

    return ref;
}

static nsresult NSAPI nsInputStream_Close(nsIInputStream *iface)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    FIXME("(%p)\n", This);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsInputStream_Available(nsIInputStream *iface, PRUint32 *_retval)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    FIXME("(%p)->(%p)\n", This, _retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsInputStream_Read(nsIInputStream *iface, char *aBuf, PRUint32 aCount,
                                         PRUint32 *_retval)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    DWORD read = aCount;

    TRACE("(%p)->(%p %d %p)\n", This, aBuf, aCount, _retval);

    if(read > This->buf_size)
        read = This->buf_size;

    if(read) {
        memcpy(aBuf, This->buf, read);
        if(read < This->buf_size)
            memmove(This->buf, This->buf+read, This->buf_size-read);
        This->buf_size -= read;
    }

    *_retval = read;
    return NS_OK;
}

static nsresult NSAPI nsInputStream_ReadSegments(nsIInputStream *iface,
        nsresult (WINAPI *aWriter)(nsIInputStream*,void*,const char*,PRUint32,PRUint32,PRUint32*),
        void *aClousure, PRUint32 aCount, PRUint32 *_retval)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    PRUint32 written = 0;
    nsresult nsres;

    TRACE("(%p)->(%p %p %d %p)\n", This, aWriter, aClousure, aCount, _retval);

    if(!This->buf_size)
        return S_OK;

    if(aCount > This->buf_size)
        aCount = This->buf_size;

    nsres = aWriter(&This->nsIInputStream_iface, aClousure, This->buf, 0, aCount, &written);
    if(NS_FAILED(nsres))
        TRACE("aWritter failed: %08x\n", nsres);
    else if(written != This->buf_size)
        FIXME("written %d != buf_size %d\n", written, This->buf_size);

    This->buf_size -= written; 

    *_retval = written;
    return nsres;
}

static nsresult NSAPI nsInputStream_IsNonBlocking(nsIInputStream *iface, PRBool *_retval)
{
    nsProtocolStream *This = impl_from_nsIInputStream(iface);
    FIXME("(%p)->(%p)\n", This, _retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static const nsIInputStreamVtbl nsInputStreamVtbl = {
    nsInputStream_QueryInterface,
    nsInputStream_AddRef,
    nsInputStream_Release,
    nsInputStream_Close,
    nsInputStream_Available,
    nsInputStream_Read,
    nsInputStream_ReadSegments,
    nsInputStream_IsNonBlocking
};

static nsProtocolStream *create_nsprotocol_stream(void)
{
    nsProtocolStream *ret = heap_alloc(sizeof(nsProtocolStream));

    ret->nsIInputStream_iface.lpVtbl = &nsInputStreamVtbl;
    ret->ref = 1;
    ret->buf_size = 0;

    return ret;
}

static inline BSCallback *impl_from_IBindStatusCallback(IBindStatusCallback *iface)
{
    return CONTAINING_RECORD(iface, BSCallback, IBindStatusCallback_iface);
}

static HRESULT WINAPI BindStatusCallback_QueryInterface(IBindStatusCallback *iface,
        REFIID riid, void **ppv)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);

    *ppv = NULL;
    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown, %p)\n", This, ppv);
        *ppv = &This->IBindStatusCallback_iface;
    }else if(IsEqualGUID(&IID_IBindStatusCallback, riid)) {
        TRACE("(%p)->(IID_IBindStatusCallback, %p)\n", This, ppv);
        *ppv = &This->IBindStatusCallback_iface;
    }else if(IsEqualGUID(&IID_IServiceProvider, riid)) {
        TRACE("(%p)->(IID_IServiceProvider %p)\n", This, ppv);
        *ppv = &This->IServiceProvider_iface;
    }else if(IsEqualGUID(&IID_IHttpNegotiate, riid)) {
        TRACE("(%p)->(IID_IHttpNegotiate %p)\n", This, ppv);
        *ppv = &This->IHttpNegotiate2_iface;
    }else if(IsEqualGUID(&IID_IHttpNegotiate2, riid)) {
        TRACE("(%p)->(IID_IHttpNegotiate2 %p)\n", This, ppv);
        *ppv = &This->IHttpNegotiate2_iface;
    }else if(IsEqualGUID(&IID_IInternetBindInfo, riid)) {
        TRACE("(%p)->(IID_IInternetBindInfo %p)\n", This, ppv);
        *ppv = &This->IInternetBindInfo_iface;
    }

    if(*ppv) {
        IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
        return S_OK;
    }

    TRACE("Unsupported riid = %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI BindStatusCallback_AddRef(IBindStatusCallback *iface)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref = %d\n", This, ref);

    return ref;
}

static ULONG WINAPI BindStatusCallback_Release(IBindStatusCallback *iface)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %d\n", This, ref);

    if(!ref) {
        if(This->post_data)
            GlobalFree(This->post_data);
        if(This->mon)
            IMoniker_Release(This->mon);
        if(This->binding)
            IBinding_Release(This->binding);
        list_remove(&This->entry);
        list_init(&This->entry);
        heap_free(This->headers);

        This->vtbl->destroy(This);
    }

    return ref;
}

static HRESULT WINAPI BindStatusCallback_OnStartBinding(IBindStatusCallback *iface,
        DWORD dwReserved, IBinding *pbind)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%d %p)\n", This, dwReserved, pbind);

    IBinding_AddRef(pbind);
    This->binding = pbind;

    if(This->doc)
        list_add_head(&This->doc->bindings, &This->entry);

    return This->vtbl->start_binding(This);
}

static HRESULT WINAPI BindStatusCallback_GetPriority(IBindStatusCallback *iface, LONG *pnPriority)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    FIXME("(%p)->(%p)\n", This, pnPriority);
    return E_NOTIMPL;
}

static HRESULT WINAPI BindStatusCallback_OnLowResource(IBindStatusCallback *iface, DWORD reserved)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    FIXME("(%p)->(%d)\n", This, reserved);
    return E_NOTIMPL;
}

static HRESULT WINAPI BindStatusCallback_OnProgress(IBindStatusCallback *iface, ULONG ulProgress,
        ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("%p)->(%u %u %u %s)\n", This, ulProgress, ulProgressMax, ulStatusCode,
            debugstr_w(szStatusText));

    return This->vtbl->on_progress(This, ulStatusCode, szStatusText);
}

static HRESULT WINAPI BindStatusCallback_OnStopBinding(IBindStatusCallback *iface,
        HRESULT hresult, LPCWSTR szError)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    HRESULT hres;

    TRACE("(%p)->(%08x %s)\n", This, hresult, debugstr_w(szError));

    /* NOTE: IE7 calls GetBindResult here */

    hres = This->vtbl->stop_binding(This, hresult);

    if(This->binding) {
        IBinding_Release(This->binding);
        This->binding = NULL;
    }

    list_remove(&This->entry);
    list_init(&This->entry);
    This->doc = NULL;

    return hres;
}

static HRESULT WINAPI BindStatusCallback_GetBindInfo(IBindStatusCallback *iface,
        DWORD *grfBINDF, BINDINFO *pbindinfo)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    DWORD size;

    TRACE("(%p)->(%p %p)\n", This, grfBINDF, pbindinfo);

    if(!This->bindinfo_ready) {
        HRESULT hres;

        hres = This->vtbl->init_bindinfo(This);
        if(FAILED(hres))
            return hres;

        This->bindinfo_ready = TRUE;
    }

    *grfBINDF = This->bindf;

    size = pbindinfo->cbSize;
    memset(pbindinfo, 0, size);
    pbindinfo->cbSize = size;

    pbindinfo->cbstgmedData = This->post_data_len;
    pbindinfo->dwCodePage = CP_UTF8;
    pbindinfo->dwOptions = 0x80000;

    if(This->post_data) {
        pbindinfo->dwBindVerb = BINDVERB_POST;

        pbindinfo->stgmedData.tymed = TYMED_HGLOBAL;
        pbindinfo->stgmedData.u.hGlobal = This->post_data;
        pbindinfo->stgmedData.pUnkForRelease = (IUnknown*)&This->IBindStatusCallback_iface;
        IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
    }

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnDataAvailable(IBindStatusCallback *iface,
        DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);

    TRACE("(%p)->(%08x %d %p %p)\n", This, grfBSCF, dwSize, pformatetc, pstgmed);

    return This->vtbl->read_data(This, pstgmed->u.pstm);
}

static HRESULT WINAPI BindStatusCallback_OnObjectAvailable(IBindStatusCallback *iface,
        REFIID riid, IUnknown *punk)
{
    BSCallback *This = impl_from_IBindStatusCallback(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_guid(riid), punk);
    return E_NOTIMPL;
}

static const IBindStatusCallbackVtbl BindStatusCallbackVtbl = {
    BindStatusCallback_QueryInterface,
    BindStatusCallback_AddRef,
    BindStatusCallback_Release,
    BindStatusCallback_OnStartBinding,
    BindStatusCallback_GetPriority,
    BindStatusCallback_OnLowResource,
    BindStatusCallback_OnProgress,
    BindStatusCallback_OnStopBinding,
    BindStatusCallback_GetBindInfo,
    BindStatusCallback_OnDataAvailable,
    BindStatusCallback_OnObjectAvailable
};

static inline BSCallback *impl_from_IHttpNegotiate2(IHttpNegotiate2 *iface)
{
    return CONTAINING_RECORD(iface, BSCallback, IHttpNegotiate2_iface);
}

static HRESULT WINAPI HttpNegotiate_QueryInterface(IHttpNegotiate2 *iface,
                                                   REFIID riid, void **ppv)
{
    BSCallback *This = impl_from_IHttpNegotiate2(iface);
    return IBindStatusCallback_QueryInterface(&This->IBindStatusCallback_iface, riid, ppv);
}

static ULONG WINAPI HttpNegotiate_AddRef(IHttpNegotiate2 *iface)
{
    BSCallback *This = impl_from_IHttpNegotiate2(iface);
    return IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
}

static ULONG WINAPI HttpNegotiate_Release(IHttpNegotiate2 *iface)
{
    BSCallback *This = impl_from_IHttpNegotiate2(iface);
    return IBindStatusCallback_Release(&This->IBindStatusCallback_iface);
}

static HRESULT WINAPI HttpNegotiate_BeginningTransaction(IHttpNegotiate2 *iface,
        LPCWSTR szURL, LPCWSTR szHeaders, DWORD dwReserved, LPWSTR *pszAdditionalHeaders)
{
    BSCallback *This = impl_from_IHttpNegotiate2(iface);
    HRESULT hres;

    TRACE("(%p)->(%s %s %d %p)\n", This, debugstr_w(szURL), debugstr_w(szHeaders),
          dwReserved, pszAdditionalHeaders);

    *pszAdditionalHeaders = NULL;

    hres = This->vtbl->beginning_transaction(This, pszAdditionalHeaders);
    if(hres != S_FALSE)
        return hres;

    if(This->headers) {
        DWORD size;

        size = (strlenW(This->headers)+1)*sizeof(WCHAR);
        *pszAdditionalHeaders = CoTaskMemAlloc(size);
        if(!*pszAdditionalHeaders)
            return E_OUTOFMEMORY;
        memcpy(*pszAdditionalHeaders, This->headers, size);
    }

    return S_OK;
}

static HRESULT WINAPI HttpNegotiate_OnResponse(IHttpNegotiate2 *iface, DWORD dwResponseCode,
        LPCWSTR szResponseHeaders, LPCWSTR szRequestHeaders, LPWSTR *pszAdditionalRequestHeaders)
{
    BSCallback *This = impl_from_IHttpNegotiate2(iface);

    TRACE("(%p)->(%d %s %s %p)\n", This, dwResponseCode, debugstr_w(szResponseHeaders),
          debugstr_w(szRequestHeaders), pszAdditionalRequestHeaders);

    return This->vtbl->on_response(This, dwResponseCode, szResponseHeaders);
}

static HRESULT WINAPI HttpNegotiate_GetRootSecurityId(IHttpNegotiate2 *iface,
        BYTE *pbSecurityId, DWORD *pcbSecurityId, DWORD_PTR dwReserved)
{
    BSCallback *This = impl_from_IHttpNegotiate2(iface);
    FIXME("(%p)->(%p %p %ld)\n", This, pbSecurityId, pcbSecurityId, dwReserved);
    return E_NOTIMPL;
}

static const IHttpNegotiate2Vtbl HttpNegotiate2Vtbl = {
    HttpNegotiate_QueryInterface,
    HttpNegotiate_AddRef,
    HttpNegotiate_Release,
    HttpNegotiate_BeginningTransaction,
    HttpNegotiate_OnResponse,
    HttpNegotiate_GetRootSecurityId
};

static inline BSCallback *impl_from_IInternetBindInfo(IInternetBindInfo *iface)
{
    return CONTAINING_RECORD(iface, BSCallback, IInternetBindInfo_iface);
}

static HRESULT WINAPI InternetBindInfo_QueryInterface(IInternetBindInfo *iface,
                                                      REFIID riid, void **ppv)
{
    BSCallback *This = impl_from_IInternetBindInfo(iface);
    return IBindStatusCallback_QueryInterface(&This->IBindStatusCallback_iface, riid, ppv);
}

static ULONG WINAPI InternetBindInfo_AddRef(IInternetBindInfo *iface)
{
    BSCallback *This = impl_from_IInternetBindInfo(iface);
    return IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
}

static ULONG WINAPI InternetBindInfo_Release(IInternetBindInfo *iface)
{
    BSCallback *This = impl_from_IInternetBindInfo(iface);
    return IBindStatusCallback_Release(&This->IBindStatusCallback_iface);
}

static HRESULT WINAPI InternetBindInfo_GetBindInfo(IInternetBindInfo *iface,
                                                   DWORD *grfBINDF, BINDINFO *pbindinfo)
{
    BSCallback *This = impl_from_IInternetBindInfo(iface);
    FIXME("(%p)->(%p %p)\n", This, grfBINDF, pbindinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetBindInfo_GetBindString(IInternetBindInfo *iface,
        ULONG ulStringType, LPOLESTR *ppwzStr, ULONG cEl, ULONG *pcElFetched)
{
    BSCallback *This = impl_from_IInternetBindInfo(iface);
    FIXME("(%p)->(%u %p %u %p)\n", This, ulStringType, ppwzStr, cEl, pcElFetched);
    return E_NOTIMPL;
}

static const IInternetBindInfoVtbl InternetBindInfoVtbl = {
    InternetBindInfo_QueryInterface,
    InternetBindInfo_AddRef,
    InternetBindInfo_Release,
    InternetBindInfo_GetBindInfo,
    InternetBindInfo_GetBindString
};

static inline BSCallback *impl_from_IServiceProvider(IServiceProvider *iface)
{
    return CONTAINING_RECORD(iface, BSCallback, IServiceProvider_iface);
}

static HRESULT WINAPI BSCServiceProvider_QueryInterface(IServiceProvider *iface,
                                                        REFIID riid, void **ppv)
{
    BSCallback *This = impl_from_IServiceProvider(iface);
    return IBindStatusCallback_QueryInterface(&This->IBindStatusCallback_iface, riid, ppv);
}

static ULONG WINAPI BSCServiceProvider_AddRef(IServiceProvider *iface)
{
    BSCallback *This = impl_from_IServiceProvider(iface);
    return IBindStatusCallback_AddRef(&This->IBindStatusCallback_iface);
}

static ULONG WINAPI BSCServiceProvider_Release(IServiceProvider *iface)
{
    BSCallback *This = impl_from_IServiceProvider(iface);
    return IBindStatusCallback_Release(&This->IBindStatusCallback_iface);
}

static HRESULT WINAPI BSCServiceProvider_QueryService(IServiceProvider *iface,
        REFGUID guidService, REFIID riid, void **ppv)
{
    BSCallback *This = impl_from_IServiceProvider(iface);
    TRACE("(%p)->(%s %s %p)\n", This, debugstr_guid(guidService), debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static const IServiceProviderVtbl ServiceProviderVtbl = {
    BSCServiceProvider_QueryInterface,
    BSCServiceProvider_AddRef,
    BSCServiceProvider_Release,
    BSCServiceProvider_QueryService
};

static void init_bscallback(BSCallback *This, const BSCallbackVtbl *vtbl, IMoniker *mon, DWORD bindf)
{
    This->IBindStatusCallback_iface.lpVtbl = &BindStatusCallbackVtbl;
    This->IServiceProvider_iface.lpVtbl = &ServiceProviderVtbl;
    This->IHttpNegotiate2_iface.lpVtbl = &HttpNegotiate2Vtbl;
    This->IInternetBindInfo_iface.lpVtbl = &InternetBindInfoVtbl;
    This->vtbl = vtbl;
    This->ref = 1;
    This->bindf = bindf;

    list_init(&This->entry);

    if(mon)
        IMoniker_AddRef(mon);
    This->mon = mon;
}

/* Calls undocumented 84 cmd of CGID_ShellDocView */
static void call_docview_84(HTMLDocumentObj *doc)
{
    IOleCommandTarget *olecmd;
    VARIANT var;
    HRESULT hres;

    if(!doc->client)
        return;

    hres = IOleClientSite_QueryInterface(doc->client, &IID_IOleCommandTarget, (void**)&olecmd);
    if(FAILED(hres))
        return;

    VariantInit(&var);
    hres = IOleCommandTarget_Exec(olecmd, &CGID_ShellDocView, 84, 0, NULL, &var);
    IOleCommandTarget_Release(olecmd);
    if(SUCCEEDED(hres) && V_VT(&var) != VT_NULL)
        FIXME("handle result\n");
}

static HRESULT parse_headers(const WCHAR *headers, struct list *headers_list)
{
    const WCHAR *header, *header_end, *colon, *value;
    HRESULT hres;

    header = headers;
    while(*header) {
        if(header[0] == '\r' && header[1] == '\n' && !header[2])
            break;
        for(colon = header; *colon && *colon != ':' && *colon != '\r'; colon++);
        if(*colon != ':')
            return E_FAIL;

        value = colon+1;
        while(*value == ' ')
            value++;
        if(!*value)
            return E_FAIL;

        for(header_end = value+1; *header_end && *header_end != '\r'; header_end++);

        hres = set_http_header(headers_list, header, colon-header, value, header_end-value);
        if(FAILED(hres))
            return hres;

        header = header_end;
        if(header[0] == '\r' && header[1] == '\n')
            header += 2;
    }

    return S_OK;
}

HRESULT start_binding(HTMLWindow *window, HTMLDocumentNode *doc, BSCallback *bscallback, IBindCtx *bctx)
{
    IStream *str = NULL;
    HRESULT hres;

    bscallback->doc = doc;

    /* NOTE: IE7 calls IsSystemMoniker here*/

    if(window) {
        if(bscallback->mon != window->mon)
            set_current_mon(window, bscallback->mon);
        call_docview_84(window->doc_obj);
    }

    if(bctx) {
        RegisterBindStatusCallback(bctx, &bscallback->IBindStatusCallback_iface, NULL, 0);
        IBindCtx_AddRef(bctx);
    }else {
        hres = CreateAsyncBindCtx(0, &bscallback->IBindStatusCallback_iface, NULL, &bctx);
        if(FAILED(hres)) {
            WARN("CreateAsyncBindCtx failed: %08x\n", hres);
            bscallback->vtbl->stop_binding(bscallback, hres);
            return hres;
        }
    }

    hres = IMoniker_BindToStorage(bscallback->mon, bctx, NULL, &IID_IStream, (void**)&str);
    IBindCtx_Release(bctx);
    if(FAILED(hres)) {
        WARN("BindToStorage failed: %08x\n", hres);
        bscallback->vtbl->stop_binding(bscallback, hres);
        return hres;
    }

    if(str)
        IStream_Release(str);

    IMoniker_Release(bscallback->mon);
    bscallback->mon = NULL;

    return S_OK;
}

typedef struct {
    BSCallback bsc;

    DWORD size;
    BYTE *buf;
    HRESULT hres;
} BufferBSC;

static inline BufferBSC *BufferBSC_from_BSCallback(BSCallback *iface)
{
    return CONTAINING_RECORD(iface, BufferBSC, bsc);
}

static void BufferBSC_destroy(BSCallback *bsc)
{
    BufferBSC *This = BufferBSC_from_BSCallback(bsc);

    heap_free(This->buf);
    heap_free(This);
}

static HRESULT BufferBSC_init_bindinfo(BSCallback *bsc)
{
    return S_OK;
}

static HRESULT BufferBSC_start_binding(BSCallback *bsc)
{
    return S_OK;
}

static HRESULT BufferBSC_stop_binding(BSCallback *bsc, HRESULT result)
{
    BufferBSC *This = BufferBSC_from_BSCallback(bsc);

    This->hres = result;

    if(FAILED(result)) {
        heap_free(This->buf);
        This->buf = NULL;
        This->size = 0;
    }

    return S_OK;
}

static HRESULT BufferBSC_read_data(BSCallback *bsc, IStream *stream)
{
    BufferBSC *This = BufferBSC_from_BSCallback(bsc);
    DWORD readed;
    HRESULT hres;

    if(!This->buf) {
        This->size = 128;
        This->buf = heap_alloc(This->size);
    }

    do {
        if(This->bsc.readed == This->size) {
            This->size <<= 1;
            This->buf = heap_realloc(This->buf, This->size);
        }

        readed = 0;
        hres = IStream_Read(stream, This->buf+This->bsc.readed, This->size-This->bsc.readed, &readed);
        This->bsc.readed += readed;
    }while(hres == S_OK);

    return S_OK;
}

static HRESULT BufferBSC_on_progress(BSCallback *bsc, ULONG status_code, LPCWSTR status_text)
{
    return S_OK;
}

static HRESULT BufferBSC_on_response(BSCallback *bsc, DWORD response_code,
        LPCWSTR response_headers)
{
    return S_OK;
}

static HRESULT BufferBSC_beginning_transaction(BSCallback *bsc, WCHAR **additional_headers)
{
    return S_FALSE;
}

static const BSCallbackVtbl BufferBSCVtbl = {
    BufferBSC_destroy,
    BufferBSC_init_bindinfo,
    BufferBSC_start_binding,
    BufferBSC_stop_binding,
    BufferBSC_read_data,
    BufferBSC_on_progress,
    BufferBSC_on_response,
    BufferBSC_beginning_transaction
};


static BufferBSC *create_bufferbsc(IMoniker *mon)
{
    BufferBSC *ret = heap_alloc_zero(sizeof(*ret));

    init_bscallback(&ret->bsc, &BufferBSCVtbl, mon, 0);
    ret->hres = E_FAIL;

    return ret;
}

HRESULT bind_mon_to_buffer(HTMLDocumentNode *doc, IMoniker *mon, void **buf, DWORD *size)
{
    BufferBSC *bsc = create_bufferbsc(mon);
    HRESULT hres;

    *buf = NULL;

    hres = start_binding(NULL, doc, &bsc->bsc, NULL);
    if(SUCCEEDED(hres)) {
        hres = bsc->hres;
        if(SUCCEEDED(hres)) {
            *buf = bsc->buf;
            bsc->buf = NULL;
            *size = bsc->bsc.readed;
            bsc->size = 0;
        }
    }

    IBindStatusCallback_Release(&bsc->bsc.IBindStatusCallback_iface);

    return hres;
}

struct nsChannelBSC {
    BSCallback bsc;

    HTMLWindow *window;

    nsChannel *nschannel;
    nsIStreamListener *nslistener;
    nsISupports *nscontext;

    nsProtocolStream *nsstream;
};

static HRESULT read_post_data_stream(nsChannelBSC *This, nsChannel *nschannel)
{
    PRUint32 data_len = 0, available = 0;
    char *data, *post_data;
    nsresult nsres;
    HRESULT hres = S_OK;

    if(!nschannel->post_data_stream)
        return S_OK;

    nsres =  nsIInputStream_Available(nschannel->post_data_stream, &available);
    if(NS_FAILED(nsres))
        return E_FAIL;

    post_data = data = GlobalAlloc(0, available);
    if(!data)
        return E_OUTOFMEMORY;

    nsres = nsIInputStream_Read(nschannel->post_data_stream, data, available, &data_len);
    if(NS_FAILED(nsres)) {
        GlobalFree(data);
        return E_FAIL;
    }

    if(nschannel->post_data_contains_headers) {
        if(data_len >= 2 && data[0] == '\r' && data[1] == '\n') {
            post_data = data+2;
            data_len -= 2;
        }else {
            WCHAR *headers;
            DWORD size;
            char *ptr;

            post_data += data_len;
            for(ptr = data; ptr+4 < data+data_len; ptr++) {
                if(!memcmp(ptr, "\r\n\r\n", 4)) {
                    post_data = ptr+4;
                    break;
                }
            }

            data_len -= post_data-data;

            size = MultiByteToWideChar(CP_ACP, 0, data, post_data-data, NULL, 0);
            headers = heap_alloc((size+1)*sizeof(WCHAR));
            if(headers) {
                MultiByteToWideChar(CP_ACP, 0, data, post_data-data, headers, size);
                headers[size] = 0;
                hres = parse_headers(headers , &nschannel->request_headers);
                if(SUCCEEDED(hres))
                    This->bsc.headers = headers;
                else
                    heap_free(headers);
            }else {
                hres = E_OUTOFMEMORY;
            }
        }
    }

    if(FAILED(hres)) {
        GlobalFree(data);
        return hres;
    }

    if(!data_len) {
        GlobalFree(data);
        post_data = NULL;
    }else if(post_data != data) {
        char *new_data;

        new_data = GlobalAlloc(0, data_len);
        if(new_data)
            memcpy(new_data, post_data, data_len);
        GlobalFree(data);
        if(!new_data)
            return E_OUTOFMEMORY;
        post_data = new_data;
    }

    This->bsc.post_data = post_data;
    This->bsc.post_data_len = data_len;
    TRACE("post_data = %s\n", debugstr_a(This->bsc.post_data));
    return S_OK;
}

static HRESULT on_start_nsrequest(nsChannelBSC *This)
{
    nsresult nsres;

    /* FIXME: it's needed for http connections from BindToObject. */
    if(!This->nschannel->response_status)
        This->nschannel->response_status = 200;

    nsres = nsIStreamListener_OnStartRequest(This->nslistener,
            (nsIRequest*)&This->nschannel->nsIHttpChannel_iface, This->nscontext);
    if(NS_FAILED(nsres)) {
        FIXME("OnStartRequest failed: %08x\n", nsres);
        return E_FAIL;
    }

    if(This->window) {
        update_window_doc(This->window);
        if(This->window->doc != This->bsc.doc)
            This->bsc.doc = This->window->doc;
        if(This->window->readystate != READYSTATE_LOADING)
            set_ready_state(This->window, READYSTATE_LOADING);
    }

    return S_OK;
}

static void on_stop_nsrequest(nsChannelBSC *This, HRESULT result)
{
    nsresult nsres, request_result;

    switch(result) {
    case S_OK:
        request_result = NS_OK;
        break;
    case E_ABORT:
        request_result = NS_BINDING_ABORTED;
        break;
    default:
        request_result = NS_ERROR_FAILURE;
    }

    if(!This->bsc.readed && SUCCEEDED(result)) {
        TRACE("No data read! Calling OnStartRequest\n");
        on_start_nsrequest(This);
    }

    if(This->nslistener) {
        nsres = nsIStreamListener_OnStopRequest(This->nslistener,
                 (nsIRequest*)&This->nschannel->nsIHttpChannel_iface, This->nscontext,
                 request_result);
        if(NS_FAILED(nsres))
            WARN("OnStopRequet failed: %08x\n", nsres);
    }

    if(This->nschannel->load_group) {
        nsres = nsILoadGroup_RemoveRequest(This->nschannel->load_group,
                (nsIRequest*)&This->nschannel->nsIHttpChannel_iface, NULL, request_result);
        if(NS_FAILED(nsres))
            ERR("RemoveRequest failed: %08x\n", nsres);
    }
}

static HRESULT read_stream_data(nsChannelBSC *This, IStream *stream)
{
    DWORD read;
    nsresult nsres;
    HRESULT hres;

    if(!This->nslistener) {
        BYTE buf[1024];

        do {
            read = 0;
            hres = IStream_Read(stream, buf, sizeof(buf), &read);
        }while(hres == S_OK && read);

        return S_OK;
    }

    if(!This->nsstream)
        This->nsstream = create_nsprotocol_stream();

    do {
        read = 0;
        hres = IStream_Read(stream, This->nsstream->buf+This->nsstream->buf_size,
                sizeof(This->nsstream->buf)-This->nsstream->buf_size, &read);
        if(!read)
            break;

        This->nsstream->buf_size += read;

        if(!This->bsc.readed) {
            if(This->nsstream->buf_size >= 2
               && (BYTE)This->nsstream->buf[0] == 0xff
               && (BYTE)This->nsstream->buf[1] == 0xfe)
                This->nschannel->charset = heap_strdupA(UTF16_STR);

            if(!This->nschannel->content_type) {
                WCHAR *mime;

                hres = FindMimeFromData(NULL, NULL, This->nsstream->buf, This->nsstream->buf_size, NULL, 0, &mime, 0);
                if(FAILED(hres))
                    return hres;

                TRACE("Found MIME %s\n", debugstr_w(mime));

                This->nschannel->content_type = heap_strdupWtoA(mime);
                CoTaskMemFree(mime);
                if(!This->nschannel->content_type)
                    return E_OUTOFMEMORY;
            }

            on_start_nsrequest(This);
        }

        This->bsc.readed += This->nsstream->buf_size;

        nsres = nsIStreamListener_OnDataAvailable(This->nslistener,
                (nsIRequest*)&This->nschannel->nsIHttpChannel_iface, This->nscontext,
                &This->nsstream->nsIInputStream_iface, This->bsc.readed-This->nsstream->buf_size,
                This->nsstream->buf_size);
        if(NS_FAILED(nsres))
            ERR("OnDataAvailable failed: %08x\n", nsres);

        if(This->nsstream->buf_size == sizeof(This->nsstream->buf)) {
            ERR("buffer is full\n");
            break;
        }
    }while(hres == S_OK);

    return S_OK;
}

typedef struct {
    nsIAsyncVerifyRedirectCallback nsIAsyncVerifyRedirectCallback_iface;

    LONG ref;

    nsChannel *nschannel;
    nsChannelBSC *bsc;
} nsRedirectCallback;

static nsRedirectCallback *impl_from_nsIAsyncVerifyRedirectCallback(nsIAsyncVerifyRedirectCallback *iface)
{
    return CONTAINING_RECORD(iface, nsRedirectCallback, nsIAsyncVerifyRedirectCallback_iface);
}

static nsresult NSAPI nsAsyncVerifyRedirectCallback_QueryInterface(nsIAsyncVerifyRedirectCallback *iface,
        nsIIDRef riid, void **result)
{
    nsRedirectCallback *This = impl_from_nsIAsyncVerifyRedirectCallback(iface);

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISuports %p)\n", This, result);
        *result = &This->nsIAsyncVerifyRedirectCallback_iface;
    }else if(IsEqualGUID(&IID_nsIAsyncVerifyRedirectCallback, riid)) {
        TRACE("(%p)->(IID_nsIAsyncVerifyRedirectCallback %p)\n", This, result);
        *result = &This->nsIAsyncVerifyRedirectCallback_iface;
    }else {
        *result = NULL;
        WARN("unimplemented iface %s\n", debugstr_guid(riid));
        return NS_NOINTERFACE;
    }

    nsISupports_AddRef((nsISupports*)*result);
    return NS_OK;
}

static nsrefcnt NSAPI nsAsyncVerifyRedirectCallback_AddRef(nsIAsyncVerifyRedirectCallback *iface)
{
    nsRedirectCallback *This = impl_from_nsIAsyncVerifyRedirectCallback(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsAsyncVerifyRedirectCallback_Release(nsIAsyncVerifyRedirectCallback *iface)
{
    nsRedirectCallback *This = impl_from_nsIAsyncVerifyRedirectCallback(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        IBindStatusCallback_Release(&This->bsc->bsc.IBindStatusCallback_iface);
        nsIChannel_Release(&This->nschannel->nsIHttpChannel_iface);
        heap_free(This);
    }

    return ref;
}

static nsresult NSAPI nsAsyncVerifyRedirectCallback_AsyncOnChannelRedirect(nsIAsyncVerifyRedirectCallback *iface, nsresult result)
{
    nsRedirectCallback *This = impl_from_nsIAsyncVerifyRedirectCallback(iface);

    TRACE("(%p)->(%08x)\n", This, result);

    if(This->bsc->nschannel)
        nsIHttpChannel_Release(&This->bsc->nschannel->nsIHttpChannel_iface);
    nsIChannel_AddRef(&This->nschannel->nsIHttpChannel_iface);
    This->bsc->nschannel = This->nschannel;

    if(This->nschannel->load_group) {
        nsresult nsres;

        nsres = nsILoadGroup_AddRequest(This->nschannel->load_group, (nsIRequest*)&This->nschannel->nsIHttpChannel_iface,
                NULL);
        if(NS_FAILED(nsres))
            ERR("AddRequest failed: %08x\n", nsres);
    }

    return NS_OK;
}

static const nsIAsyncVerifyRedirectCallbackVtbl nsAsyncVerifyRedirectCallbackVtbl = {
    nsAsyncVerifyRedirectCallback_QueryInterface,
    nsAsyncVerifyRedirectCallback_AddRef,
    nsAsyncVerifyRedirectCallback_Release,
    nsAsyncVerifyRedirectCallback_AsyncOnChannelRedirect
};

static HRESULT create_redirect_callback(nsChannel *nschannel, nsChannelBSC *bsc, nsRedirectCallback **ret)
{
    nsRedirectCallback *callback;

    callback = heap_alloc(sizeof(*callback));
    if(!callback)
        return E_OUTOFMEMORY;

    callback->nsIAsyncVerifyRedirectCallback_iface.lpVtbl = &nsAsyncVerifyRedirectCallbackVtbl;
    callback->ref = 1;

    nsIChannel_AddRef(&nschannel->nsIHttpChannel_iface);
    callback->nschannel = nschannel;

    IBindStatusCallback_AddRef(&bsc->bsc.IBindStatusCallback_iface);
    callback->bsc = bsc;

    *ret = callback;
    return S_OK;
}

static inline nsChannelBSC *nsChannelBSC_from_BSCallback(BSCallback *iface)
{
    return CONTAINING_RECORD(iface, nsChannelBSC, bsc);
}

static void nsChannelBSC_destroy(BSCallback *bsc)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);

    if(This->nschannel)
        nsIChannel_Release(&This->nschannel->nsIHttpChannel_iface);
    if(This->nslistener)
        nsIStreamListener_Release(This->nslistener);
    if(This->nscontext)
        nsISupports_Release(This->nscontext);
    if(This->nsstream)
        nsIInputStream_Release(&This->nsstream->nsIInputStream_iface);
    heap_free(This);
}

static HRESULT nsChannelBSC_start_binding(BSCallback *bsc)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);

    if(This->window)
        This->window->doc->skip_mutation_notif = FALSE;

    return S_OK;
}

static HRESULT nsChannelBSC_init_bindinfo(BSCallback *bsc)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);
    HRESULT hres;

    if(This->nschannel && This->nschannel->post_data_stream) {
        hres = read_post_data_stream(This, This->nschannel);
        if(FAILED(hres))
            return hres;
    }

    return S_OK;
}

typedef struct {
    task_t header;
    nsChannelBSC *bsc;
} stop_request_task_t;

static void stop_request_proc(task_t *_task)
{
    stop_request_task_t *task = (stop_request_task_t*)_task;

    TRACE("(%p)\n", task->bsc);

    list_remove(&task->bsc->bsc.entry);
    list_init(&task->bsc->bsc.entry);
    on_stop_nsrequest(task->bsc, S_OK);
    IBindStatusCallback_Release(&task->bsc->bsc.IBindStatusCallback_iface);
}

static HRESULT async_stop_request(nsChannelBSC *This)
{
    stop_request_task_t *task;

    task = heap_alloc(sizeof(*task));
    if(!task)
        return E_OUTOFMEMORY;

    IBindStatusCallback_AddRef(&This->bsc.IBindStatusCallback_iface);
    task->bsc = This;
    push_task(&task->header, stop_request_proc, This->bsc.doc->basedoc.doc_obj->basedoc.task_magic);
    return S_OK;
}

static void handle_navigation_error(nsChannelBSC *This, DWORD result)
{
    HTMLDocumentObj *doc;
    IOleCommandTarget *olecmd;
    BOOL is_error_url;
    SAFEARRAY *sa;
    SAFEARRAYBOUND bound;
    VARIANT var, varOut;
    LONG ind;
    BSTR url, unk;
    HRESULT hres;

    if(!This->window)
        return;

    doc = This->window->doc_obj;
    if(!doc || !doc->doc_object_service || !doc->client)
        return;

    hres = IDocObjectService_IsErrorUrl(doc->doc_object_service,
            This->window->url, &is_error_url);
    if(FAILED(hres) || is_error_url)
        return;

    hres = IOleClientSite_QueryInterface(doc->client,
            &IID_IOleCommandTarget, (void**)&olecmd);
    if(FAILED(hres))
        return;

    bound.lLbound = 0;
    bound.cElements = 8;
    sa = SafeArrayCreate(VT_VARIANT, 1, &bound);
    if(!sa) {
        IOleCommandTarget_Release(olecmd);
        return;
    }

    ind = 0;
    V_VT(&var) = VT_I4;
    V_I4(&var) = result;
    SafeArrayPutElement(sa, &ind, &var);

    ind = 1;
    V_VT(&var) = VT_BSTR;
    url = SysAllocString(This->window->url);
    V_BSTR(&var) = url;
    SafeArrayPutElement(sa, &ind, &var);

    ind = 3;
    V_VT(&var) = VT_UNKNOWN;
    V_UNKNOWN(&var) = (IUnknown*)&This->window->IHTMLWindow2_iface;
    SafeArrayPutElement(sa, &ind, &var);

    /* FIXME: what are the following fields for? */
    ind = 2;
    V_VT(&var) = VT_UNKNOWN;
    V_UNKNOWN(&var) = NULL;
    SafeArrayPutElement(sa, &ind, &var);

    ind = 4;
    V_VT(&var) = VT_BOOL;
    V_BOOL(&var) = FALSE;
    SafeArrayPutElement(sa, &ind, &var);

    ind = 5;
    V_VT(&var) = VT_BOOL;
    V_BOOL(&var) = FALSE;
    SafeArrayPutElement(sa, &ind, &var);

    ind = 6;
    V_VT(&var) = VT_BSTR;
    unk = SysAllocString(NULL);
    V_BSTR(&var) = unk;
    SafeArrayPutElement(sa, &ind, &var);

    ind = 7;
    V_VT(&var) = VT_UNKNOWN;
    V_UNKNOWN(&var) = NULL;
    SafeArrayPutElement(sa, &ind, &var);

    V_VT(&var) = VT_ARRAY;
    V_ARRAY(&var) = sa;
    V_VT(&varOut) = VT_BOOL;
    V_BOOL(&varOut) = VARIANT_TRUE;
    IOleCommandTarget_Exec(olecmd, &CGID_DocHostCmdPriv, 1, 0, &var, FAILED(hres)?NULL:&varOut);

    SysFreeString(url);
    SysFreeString(unk);
    SafeArrayDestroy(sa);
    IOleCommandTarget_Release(olecmd);
}

static HRESULT nsChannelBSC_stop_binding(BSCallback *bsc, HRESULT result)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);

    if(FAILED(result))
        handle_navigation_error(This, result);
    else if(This->window) {
        result = async_stop_request(This);
        if(SUCCEEDED(result))
           return S_OK;
    }

    on_stop_nsrequest(This, result);
    return S_OK;
}

static HRESULT nsChannelBSC_read_data(BSCallback *bsc, IStream *stream)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);

    return read_stream_data(This, stream);
}

static HRESULT handle_redirect(nsChannelBSC *This, const WCHAR *new_url)
{
    nsRedirectCallback *callback;
    nsIChannelEventSink *sink;
    nsChannel *new_channel;
    nsresult nsres;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_w(new_url));

    if(!This->nschannel || !This->nschannel->notif_callback)
        return S_OK;

    nsres = nsIInterfaceRequestor_GetInterface(This->nschannel->notif_callback, &IID_nsIChannelEventSink, (void**)&sink);
    if(NS_FAILED(nsres))
        return S_OK;

    hres = create_redirect_nschannel(new_url, This->nschannel, &new_channel);
    if(SUCCEEDED(hres)) {
        hres = create_redirect_callback(new_channel, This, &callback);
        nsIChannel_Release(&new_channel->nsIHttpChannel_iface);
    }

    if(SUCCEEDED(hres)) {
        nsres = nsIChannelEventSink_AsyncOnChannelRedirect(sink, (nsIChannel*)&This->nschannel->nsIHttpChannel_iface,
                (nsIChannel*)&callback->nschannel->nsIHttpChannel_iface, REDIRECT_TEMPORARY, /* FIXME */
                &callback->nsIAsyncVerifyRedirectCallback_iface);

        if(NS_FAILED(nsres))
            FIXME("AsyncOnChannelRedirect failed: %08x\n", hres);
        else if(This->nschannel != callback->nschannel)
            FIXME("nschannel not updated\n");

        nsIAsyncVerifyRedirectCallback_Release(&callback->nsIAsyncVerifyRedirectCallback_iface);
    }

    nsIChannelEventSink_Release(sink);
    return hres;
}

static HRESULT nsChannelBSC_on_progress(BSCallback *bsc, ULONG status_code, LPCWSTR status_text)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);

    switch(status_code) {
    case BINDSTATUS_MIMETYPEAVAILABLE:
        if(!This->nschannel)
            return S_OK;

        heap_free(This->nschannel->content_type);
        This->nschannel->content_type = heap_strdupWtoA(status_text);
        break;
    case BINDSTATUS_REDIRECTING:
        return handle_redirect(This, status_text);
    case BINDSTATUS_BEGINDOWNLOADDATA: {
        IWinInetHttpInfo *http_info;
        DWORD status, size = sizeof(DWORD);
        HRESULT hres;

        if(!This->bsc.binding)
            break;

        hres = IBinding_QueryInterface(This->bsc.binding, &IID_IWinInetHttpInfo, (void**)&http_info);
        if(FAILED(hres))
            break;

        hres = IWinInetHttpInfo_QueryInfo(http_info,
                HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER, &status, &size, NULL, NULL);
        IWinInetHttpInfo_Release(http_info);
        if(FAILED(hres) || status == HTTP_STATUS_OK)
            break;

        handle_navigation_error(This, status);
    }
    }

    return S_OK;
}

static HRESULT nsChannelBSC_on_response(BSCallback *bsc, DWORD response_code,
        LPCWSTR response_headers)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);
    HRESULT hres;

    This->nschannel->response_status = response_code;

    if(response_headers) {
        const WCHAR *headers;

        headers = strchrW(response_headers, '\r');
        if(headers && headers[1] == '\n') {
            headers += 2;
            hres = parse_headers(headers, &This->nschannel->response_headers);
            if(FAILED(hres)) {
                WARN("parsing headers failed: %08x\n", hres);
                return hres;
            }
        }
    }

    return S_OK;
}

static HRESULT nsChannelBSC_beginning_transaction(BSCallback *bsc, WCHAR **additional_headers)
{
    nsChannelBSC *This = nsChannelBSC_from_BSCallback(bsc);
    http_header_t *iter;
    DWORD len = 0;
    WCHAR *ptr;

    static const WCHAR content_lengthW[] =
        {'C','o','n','t','e','n','t','-','L','e','n','g','t','h',0};

    if(!This->nschannel)
        return S_FALSE;

    LIST_FOR_EACH_ENTRY(iter, &This->nschannel->request_headers, http_header_t, entry) {
        if(strcmpW(iter->header, content_lengthW))
            len += strlenW(iter->header) + 2 /* ": " */ + strlenW(iter->data) + 2 /* "\r\n" */;
    }

    if(!len)
        return S_OK;

    *additional_headers = ptr = CoTaskMemAlloc((len+1)*sizeof(WCHAR));
    if(!ptr)
        return E_OUTOFMEMORY;

    LIST_FOR_EACH_ENTRY(iter, &This->nschannel->request_headers, http_header_t, entry) {
        if(!strcmpW(iter->header, content_lengthW))
            continue;

        len = strlenW(iter->header);
        memcpy(ptr, iter->header, len*sizeof(WCHAR));
        ptr += len;

        *ptr++ = ':';
        *ptr++ = ' ';

        len = strlenW(iter->data);
        memcpy(ptr, iter->data, len*sizeof(WCHAR));
        ptr += len;

        *ptr++ = '\r';
        *ptr++ = '\n';
    }

    *ptr = 0;

    return S_OK;
}

static const BSCallbackVtbl nsChannelBSCVtbl = {
    nsChannelBSC_destroy,
    nsChannelBSC_init_bindinfo,
    nsChannelBSC_start_binding,
    nsChannelBSC_stop_binding,
    nsChannelBSC_read_data,
    nsChannelBSC_on_progress,
    nsChannelBSC_on_response,
    nsChannelBSC_beginning_transaction
};

HRESULT create_channelbsc(IMoniker *mon, WCHAR *headers, BYTE *post_data, DWORD post_data_size, nsChannelBSC **retval)
{
    nsChannelBSC *ret;

    ret = heap_alloc_zero(sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    init_bscallback(&ret->bsc, &nsChannelBSCVtbl, mon, BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA);

    if(headers) {
        ret->bsc.headers = heap_strdupW(headers);
        if(!ret->bsc.headers) {
            IBindStatusCallback_Release(&ret->bsc.IBindStatusCallback_iface);
            return E_OUTOFMEMORY;
        }
    }

    if(post_data) {
        ret->bsc.post_data = GlobalAlloc(0, post_data_size);
        if(!ret->bsc.post_data) {
            heap_free(ret->bsc.headers);
            IBindStatusCallback_Release(&ret->bsc.IBindStatusCallback_iface);
            return E_OUTOFMEMORY;
        }

        memcpy(ret->bsc.post_data, post_data, post_data_size);
        ret->bsc.post_data_len = post_data_size;
    }

    *retval = ret;
    return S_OK;
}

void set_window_bscallback(HTMLWindow *window, nsChannelBSC *callback)
{
    if(window->bscallback) {
        if(window->bscallback->bsc.binding)
            IBinding_Abort(window->bscallback->bsc.binding);
        window->bscallback->bsc.doc = NULL;
        window->bscallback->window = NULL;
        IBindStatusCallback_Release(&window->bscallback->bsc.IBindStatusCallback_iface);
    }

    window->bscallback = callback;

    if(callback) {
        callback->window = window;
        IBindStatusCallback_AddRef(&callback->bsc.IBindStatusCallback_iface);
        callback->bsc.doc = window->doc;
    }
}

typedef struct {
    task_t header;
    HTMLWindow *window;
    nsChannelBSC *bscallback;
} start_doc_binding_task_t;

static void start_doc_binding_proc(task_t *_task)
{
    start_doc_binding_task_t *task = (start_doc_binding_task_t*)_task;

    start_binding(task->window, NULL, (BSCallback*)task->bscallback, NULL);
    IBindStatusCallback_Release(&task->bscallback->bsc.IBindStatusCallback_iface);
}

HRESULT async_start_doc_binding(HTMLWindow *window, nsChannelBSC *bscallback)
{
    start_doc_binding_task_t *task;

    task = heap_alloc(sizeof(start_doc_binding_task_t));
    if(!task)
        return E_OUTOFMEMORY;

    task->window = window;
    task->bscallback = bscallback;
    IBindStatusCallback_AddRef(&bscallback->bsc.IBindStatusCallback_iface);

    push_task(&task->header, start_doc_binding_proc, window->task_magic);
    return S_OK;
}

void abort_document_bindings(HTMLDocumentNode *doc)
{
    BSCallback *iter, *next;

    LIST_FOR_EACH_ENTRY_SAFE(iter, next, &doc->bindings, BSCallback, entry) {
        if(iter->doc)
            remove_target_tasks(iter->doc->basedoc.task_magic);

        if(iter->binding)
            IBinding_Abort(iter->binding);
        else {
            list_remove(&iter->entry);
            list_init(&iter->entry);
            iter->vtbl->stop_binding(iter, S_OK);
        }

        iter->doc = NULL;
    }
}

HRESULT channelbsc_load_stream(nsChannelBSC *bscallback, IStream *stream)
{
    HRESULT hres = S_OK;

    if(!bscallback->nschannel) {
        ERR("NULL nschannel\n");
        return E_FAIL;
    }

    bscallback->nschannel->content_type = heap_strdupA("text/html");
    if(!bscallback->nschannel->content_type)
        return E_OUTOFMEMORY;

    list_add_head(&bscallback->bsc.doc->bindings, &bscallback->bsc.entry);
    if(stream)
        hres = read_stream_data(bscallback, stream);
    if(SUCCEEDED(hres))
        hres = async_stop_request(bscallback);
    if(FAILED(hres))
        IBindStatusCallback_OnStopBinding(&bscallback->bsc.IBindStatusCallback_iface, hres,
                ERROR_SUCCESS);

    return hres;
}

void channelbsc_set_channel(nsChannelBSC *This, nsChannel *channel, nsIStreamListener *listener, nsISupports *context)
{
    nsIChannel_AddRef(&channel->nsIHttpChannel_iface);
    This->nschannel = channel;

    nsIStreamListener_AddRef(listener);
    This->nslistener = listener;

    if(context) {
        nsISupports_AddRef(context);
        This->nscontext = context;
    }

    if(This->bsc.headers) {
        HRESULT hres;

        hres = parse_headers(This->bsc.headers, &channel->request_headers);
        heap_free(This->bsc.headers);
        This->bsc.headers = NULL;
        if(FAILED(hres))
            WARN("parse_headers failed: %08x\n", hres);
    }
}

HRESULT hlink_frame_navigate(HTMLDocument *doc, LPCWSTR url, nsChannel *nschannel, DWORD hlnf, BOOL *cancel)
{
    IHlinkFrame *hlink_frame;
    nsChannelBSC *callback;
    IServiceProvider *sp;
    IBindCtx *bindctx;
    IMoniker *mon;
    IHlink *hlink;
    HRESULT hres;

    *cancel = FALSE;

    hres = IOleClientSite_QueryInterface(doc->doc_obj->client, &IID_IServiceProvider,
            (void**)&sp);
    if(FAILED(hres))
        return S_OK;

    hres = IServiceProvider_QueryService(sp, &IID_IHlinkFrame, &IID_IHlinkFrame,
            (void**)&hlink_frame);
    IServiceProvider_Release(sp);
    if(FAILED(hres))
        return S_OK;

    hres = create_channelbsc(NULL, NULL, NULL, 0, &callback);
    if(FAILED(hres)) {
        IHlinkFrame_Release(hlink_frame);
        return hres;
    }

    if(nschannel)
        read_post_data_stream(callback, nschannel);

    hres = CreateAsyncBindCtx(0, &callback->bsc.IBindStatusCallback_iface, NULL, &bindctx);
    if(SUCCEEDED(hres))
        hres = CoCreateInstance(&CLSID_StdHlink, NULL, CLSCTX_INPROC_SERVER,
                &IID_IHlink, (LPVOID*)&hlink);

    if(SUCCEEDED(hres))
        hres = CreateURLMoniker(NULL, url, &mon);

    if(SUCCEEDED(hres)) {
        IHlink_SetMonikerReference(hlink, HLINKSETF_TARGET, mon, NULL);

        if(hlnf & HLNF_OPENINNEWWINDOW) {
            static const WCHAR wszBlank[] = {'_','b','l','a','n','k',0};
            IHlink_SetTargetFrameName(hlink, wszBlank); /* FIXME */
        }

        hres = IHlinkFrame_Navigate(hlink_frame, hlnf, bindctx,
                &callback->bsc.IBindStatusCallback_iface, hlink);
        IMoniker_Release(mon);
        *cancel = hres == S_OK;
        hres = S_OK;
    }

    IHlinkFrame_Release(hlink_frame);
    IBindCtx_Release(bindctx);
    IBindStatusCallback_Release(&callback->bsc.IBindStatusCallback_iface);
    return hres;
}

HRESULT navigate_url(HTMLWindow *window, const WCHAR *new_url, const WCHAR *base_url)
{
    WCHAR url[INTERNET_MAX_URL_LENGTH];
    nsWineURI *uri;
    HRESULT hres;

    if(!new_url) {
        *url = 0;
    }else if(base_url) {
        DWORD len = 0;

        hres = CoInternetCombineUrl(base_url, new_url, URL_ESCAPE_SPACES_ONLY|URL_DONT_ESCAPE_EXTRA_INFO,
                url, sizeof(url)/sizeof(WCHAR), &len, 0);
        if(FAILED(hres))
            return hres;
    }else {
        strcpyW(url, new_url);
    }

    if(window->doc_obj && window->doc_obj->hostui) {
        OLECHAR *translated_url = NULL;

        hres = IDocHostUIHandler_TranslateUrl(window->doc_obj->hostui, 0, url,
                &translated_url);
        if(hres == S_OK) {
            TRACE("%08x %s -> %s\n", hres, debugstr_w(url), debugstr_w(translated_url));
            strcpyW(url, translated_url);
            CoTaskMemFree(translated_url);
        }
    }

    if(window->doc_obj && window == window->doc_obj->basedoc.window) {
        BOOL cancel;

        hres = hlink_frame_navigate(&window->doc->basedoc, url, NULL, 0, &cancel);
        if(FAILED(hres))
            return hres;

        if(cancel) {
            TRACE("Navigation handled by hlink frame\n");
            return S_OK;
        }
    }

    hres = create_doc_uri(window, url, &uri);
    if(FAILED(hres))
        return hres;

    hres = load_nsuri(window, uri, NULL, LOAD_FLAGS_NONE);
    nsISupports_Release((nsISupports*)uri);
    return hres;
}

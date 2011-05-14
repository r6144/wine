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

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"
#include "shlguid.h"
#include "wininet.h"
#include "shlwapi.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define NS_IOSERVICE_CLASSNAME "nsIOService"
#define NS_IOSERVICE_CONTRACTID "@mozilla.org/network/io-service;1"

static const IID NS_IOSERVICE_CID =
    {0x9ac9e770, 0x18bc, 0x11d3, {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40}};
static const IID IID_nsWineURI =
    {0x5088272e, 0x900b, 0x11da, {0xc6,0x87, 0x00,0x0f,0xea,0x57,0xf2,0x1a}};

static nsIIOService *nsio = NULL;
static nsINetUtil *net_util;

static const WCHAR about_blankW[] = {'a','b','o','u','t',':','b','l','a','n','k',0};

static const char *request_method_strings[] = {"GET", "PUT", "POST"};

struct  nsWineURI {
    nsIURL nsIURL_iface;

    LONG ref;

    nsIURI *nsuri;
    nsIURL *nsurl;
    NSContainer *container;
    windowref_t *window_ref;
    nsChannelBSC *channel_bsc;
    BSTR wine_url;
    IUri *uri;
    IUriBuilder *uri_builder;
    BOOL is_doc_uri;
};

static nsresult create_uri(nsIURI*,HTMLWindow*,NSContainer*,nsWineURI**);

static const char *debugstr_nsacstr(const nsACString *nsstr)
{
    const char *data;

    nsACString_GetData(nsstr, &data);
    return debugstr_a(data);
}

HRESULT nsuri_to_url(LPCWSTR nsuri, BOOL ret_empty, BSTR *ret)
{
    const WCHAR *ptr = nsuri;

    static const WCHAR wine_prefixW[] = {'w','i','n','e',':'};

    if(!strncmpW(nsuri, wine_prefixW, sizeof(wine_prefixW)/sizeof(WCHAR)))
        ptr += sizeof(wine_prefixW)/sizeof(WCHAR);

    if(*ptr || ret_empty) {
        *ret = SysAllocString(ptr);
        if(!*ret)
            return E_OUTOFMEMORY;
    }else {
        *ret = NULL;
    }

    TRACE("%s -> %s\n", debugstr_w(nsuri), debugstr_w(*ret));
    return S_OK;
}

static BOOL exec_shldocvw_67(HTMLDocumentObj *doc, LPCWSTR url)
{
    IOleCommandTarget *cmdtrg = NULL;
    HRESULT hres;

    hres = IOleClientSite_QueryInterface(doc->client, &IID_IOleCommandTarget, (void**)&cmdtrg);
    if(SUCCEEDED(hres)) {
        VARIANT varUrl, varRes;

        V_VT(&varUrl) = VT_BSTR;
        V_BSTR(&varUrl) = SysAllocString(url);
        V_VT(&varRes) = VT_BOOL;

        hres = IOleCommandTarget_Exec(cmdtrg, &CGID_ShellDocView, 67, 0, &varUrl, &varRes);

        IOleCommandTarget_Release(cmdtrg);
        SysFreeString(V_BSTR(&varUrl));

        if(SUCCEEDED(hres) && !V_BOOL(&varRes)) {
            TRACE("got VARIANT_FALSE, do not load\n");
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL before_async_open(nsChannel *channel, NSContainer *container)
{
    HTMLDocumentObj *doc = container->doc;
    DWORD hlnf = 0;
    BOOL cancel;
    HRESULT hres;

    if(!doc) {
        NSContainer *container_iter = container;

        hlnf = HLNF_OPENINNEWWINDOW;
        while(!container_iter->doc)
            container_iter = container_iter->parent;
        doc = container_iter->doc;
    }

    if(!doc->client)
        return TRUE;

    if(!hlnf && !exec_shldocvw_67(doc, channel->uri->wine_url))
        return FALSE;

    hres = hlink_frame_navigate(&doc->basedoc, channel->uri->wine_url, channel, hlnf, &cancel);
    return FAILED(hres) || cancel;
}

HRESULT load_nsuri(HTMLWindow *window, nsWineURI *uri, nsChannelBSC *channelbsc, DWORD flags)
{
    nsIWebNavigation *web_navigation;
    nsIDocShell *doc_shell;
    HTMLDocumentNode *doc;
    nsresult nsres;

    nsres = get_nsinterface((nsISupports*)window->nswindow, &IID_nsIWebNavigation, (void**)&web_navigation);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIWebNavigation interface: %08x\n", nsres);
        return E_FAIL;
    }

    nsres = nsIWebNavigation_QueryInterface(web_navigation, &IID_nsIDocShell, (void**)&doc_shell);
    nsIWebNavigation_Release(web_navigation);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDocShell: %08x\n", nsres);
        return E_FAIL;
    }

    uri->channel_bsc = channelbsc;
    doc = window->doc;
    doc->skip_mutation_notif = TRUE;
    nsres = nsIDocShell_LoadURI(doc_shell, (nsIURI*)&uri->nsIURL_iface, NULL, flags, FALSE);
    if(doc == window->doc)
        doc->skip_mutation_notif = FALSE;
    uri->channel_bsc = NULL;
    nsIDocShell_Release(doc_shell);
    if(NS_FAILED(nsres)) {
        WARN("LoadURI failed: %08x\n", nsres);
        return E_FAIL;
    }

    return S_OK;
}

static HRESULT set_wine_url(nsWineURI *This, LPCWSTR url)
{
    TRACE("(%p)->(%s)\n", This, debugstr_w(url));

    if(url) {
        BSTR new_url;

        new_url = SysAllocString(url);
        if(!new_url)
            return E_OUTOFMEMORY;
        SysFreeString(This->wine_url);
        This->wine_url = new_url;
    }else {
        heap_free(This->wine_url);
        This->wine_url = NULL;
    }

    return S_OK;
}

static void set_uri_nscontainer(nsWineURI *This, NSContainer *nscontainer)
{
    if(This->container) {
        if(This->container == nscontainer)
            return;
        TRACE("Changing %p -> %p\n", This->container, nscontainer);
        nsIWebBrowserChrome_Release(&This->container->nsIWebBrowserChrome_iface);
    }

    if(nscontainer)
        nsIWebBrowserChrome_AddRef(&nscontainer->nsIWebBrowserChrome_iface);
    This->container = nscontainer;
}

static void set_uri_window(nsWineURI *This, HTMLWindow *window)
{
    if(This->window_ref) {
        if(This->window_ref->window == window)
            return;
        TRACE("Changing %p -> %p\n", This->window_ref->window, window);
        windowref_release(This->window_ref);
    }

    if(window) {
        windowref_addref(window->window_ref);
        This->window_ref = window->window_ref;

        if(window->doc_obj)
            set_uri_nscontainer(This, window->doc_obj->nscontainer);
    }else {
        This->window_ref = NULL;
    }
}

static inline BOOL is_http_channel(nsChannel *This)
{
    return This->url_scheme == URL_SCHEME_HTTP || This->url_scheme == URL_SCHEME_HTTPS;
}

static http_header_t *find_http_header(struct list *headers, const WCHAR *name, int len)
{
    http_header_t *iter;

    LIST_FOR_EACH_ENTRY(iter, headers, http_header_t, entry) {
        if(!strcmpiW(iter->header, name))
            return iter;
    }

    return NULL;
}

static nsresult get_channel_http_header(struct list *headers, const nsACString *header_name_str,
        nsACString *_retval)
{
    const char *header_namea;
    http_header_t *header;
    WCHAR *header_name;
    char *data;

    nsACString_GetData(header_name_str, &header_namea);
    header_name = heap_strdupAtoW(header_namea);
    if(!header_name)
        return NS_ERROR_UNEXPECTED;

    header = find_http_header(headers, header_name, strlenW(header_name));
    heap_free(header_name);
    if(!header)
        return NS_ERROR_NOT_AVAILABLE;

    data = heap_strdupWtoA(header->data);
    if(!data)
        return NS_ERROR_UNEXPECTED;

    nsACString_SetData(_retval, data);
    heap_free(data);
    return NS_OK;
}

HRESULT set_http_header(struct list *headers, const WCHAR *name, int name_len,
        const WCHAR *value, int value_len)
{
    http_header_t *header;

    TRACE("%s: %s\n", debugstr_wn(name, name_len), debugstr_wn(value, value_len));

    header = find_http_header(headers, name, name_len);
    if(header) {
        WCHAR *new_data;

        new_data = heap_strndupW(value, value_len);
        if(!new_data)
            return E_OUTOFMEMORY;

        heap_free(header->data);
        header->data = new_data;
    }else {
        header = heap_alloc(sizeof(http_header_t));
        if(!header)
            return E_OUTOFMEMORY;

        header->header = heap_strndupW(name, name_len);
        header->data = heap_strndupW(value, value_len);
        if(!header->header || !header->data) {
            heap_free(header->header);
            heap_free(header->data);
            heap_free(header);
            return E_OUTOFMEMORY;
        }

        list_add_tail(headers, &header->entry);
    }

    return S_OK;
}

static nsresult set_channel_http_header(struct list *headers, const nsACString *name_str,
        const nsACString *value_str)
{
    const char *namea, *valuea;
    WCHAR *name, *value;
    HRESULT hres;

    nsACString_GetData(name_str, &namea);
    name = heap_strdupAtoW(namea);
    if(!name)
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(value_str, &valuea);
    value = heap_strdupAtoW(valuea);
    if(!value) {
        heap_free(name);
        return NS_ERROR_UNEXPECTED;
    }

    hres = set_http_header(headers, name, strlenW(name), value, strlenW(value));

    heap_free(name);
    heap_free(value);
    return SUCCEEDED(hres) ? NS_OK : NS_ERROR_UNEXPECTED;
}

static nsresult visit_http_headers(struct list *headers, nsIHttpHeaderVisitor *visitor)
{
    nsACString header_str, value_str;
    char *header, *value;
    http_header_t *iter;
    nsresult nsres;

    LIST_FOR_EACH_ENTRY(iter, headers, http_header_t, entry) {
        header = heap_strdupWtoA(iter->header);
        if(!header)
            return NS_ERROR_OUT_OF_MEMORY;

        value = heap_strdupWtoA(iter->data);
        if(!value) {
            heap_free(header);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        nsACString_InitDepend(&header_str, header);
        nsACString_InitDepend(&value_str, value);
        nsres = nsIHttpHeaderVisitor_VisitHeader(visitor, &header_str, &value_str);
        nsACString_Finish(&header_str);
        nsACString_Finish(&value_str);
        heap_free(header);
        heap_free(value);
        if(NS_FAILED(nsres))
            break;
    }

    return NS_OK;
}

static void free_http_headers(struct list *list)
{
    http_header_t *iter, *iter_next;

    LIST_FOR_EACH_ENTRY_SAFE(iter, iter_next, list, http_header_t, entry) {
        list_remove(&iter->entry);
        heap_free(iter->header);
        heap_free(iter->data);
        heap_free(iter);
    }
}

static inline nsChannel *impl_from_nsIHttpChannel(nsIHttpChannel *iface)
{
    return CONTAINING_RECORD(iface, nsChannel, nsIHttpChannel_iface);
}

static nsresult NSAPI nsChannel_QueryInterface(nsIHttpChannel *iface, nsIIDRef riid, void **result)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result = &This->nsIHttpChannel_iface;
    }else if(IsEqualGUID(&IID_nsIRequest, riid)) {
        TRACE("(%p)->(IID_nsIRequest %p)\n", This, result);
        *result = &This->nsIHttpChannel_iface;
    }else if(IsEqualGUID(&IID_nsIChannel, riid)) {
        TRACE("(%p)->(IID_nsIChannel %p)\n", This, result);
        *result = &This->nsIHttpChannel_iface;
    }else if(IsEqualGUID(&IID_nsIHttpChannel, riid)) {
        TRACE("(%p)->(IID_nsIHttpChannel %p)\n", This, result);
        *result = is_http_channel(This) ? &This->nsIHttpChannel_iface : NULL;
    }else if(IsEqualGUID(&IID_nsIUploadChannel, riid)) {
        TRACE("(%p)->(IID_nsIUploadChannel %p)\n", This, result);
        *result = &This->nsIUploadChannel_iface;
    }else if(IsEqualGUID(&IID_nsIHttpChannelInternal, riid)) {
        TRACE("(%p)->(IID_nsIHttpChannelInternal %p)\n", This, result);
        *result = is_http_channel(This) ? &This->nsIHttpChannelInternal_iface : NULL;
    }else {
        TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), result);
        *result = NULL;
    }

    if(*result) {
        nsIChannel_AddRef(&This->nsIHttpChannel_iface);
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsChannel_AddRef(nsIHttpChannel *iface)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);
    nsrefcnt ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsChannel_Release(nsIHttpChannel *iface)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    if(!ref) {
        nsIURI_Release(&This->uri->nsIURL_iface);
        if(This->owner)
            nsISupports_Release(This->owner);
        if(This->post_data_stream)
            nsIInputStream_Release(This->post_data_stream);
        if(This->load_group)
            nsILoadGroup_Release(This->load_group);
        if(This->notif_callback)
            nsIInterfaceRequestor_Release(This->notif_callback);
        if(This->original_uri)
            nsIURI_Release(This->original_uri);
        if(This->referrer)
            nsIURI_Release(This->referrer);

        free_http_headers(&This->response_headers);
        free_http_headers(&This->request_headers);

        heap_free(This->content_type);
        heap_free(This->charset);
        heap_free(This);
    }

    return ref;
}

static nsresult NSAPI nsChannel_GetName(nsIHttpChannel *iface, nsACString *aName)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aName);

    return nsIURI_GetSpec(&This->uri->nsIURL_iface, aName);
}

static nsresult NSAPI nsChannel_IsPending(nsIHttpChannel *iface, PRBool *_retval)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetStatus(nsIHttpChannel *iface, nsresult *aStatus)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    WARN("(%p)->(%p) returning NS_OK\n", This, aStatus);

    return *aStatus = NS_OK;
}

static nsresult NSAPI nsChannel_Cancel(nsIHttpChannel *iface, nsresult aStatus)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%08x)\n", This, aStatus);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_Suspend(nsIHttpChannel *iface)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_Resume(nsIHttpChannel *iface)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetLoadGroup(nsIHttpChannel *iface, nsILoadGroup **aLoadGroup)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aLoadGroup);

    if(This->load_group)
        nsILoadGroup_AddRef(This->load_group);

    *aLoadGroup = This->load_group;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetLoadGroup(nsIHttpChannel *iface, nsILoadGroup *aLoadGroup)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aLoadGroup);

    if(This->load_group)
        nsILoadGroup_Release(This->load_group);
    if(aLoadGroup)
        nsILoadGroup_AddRef(aLoadGroup);
    This->load_group = aLoadGroup;

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetLoadFlags(nsIHttpChannel *iface, nsLoadFlags *aLoadFlags)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aLoadFlags);

    *aLoadFlags = This->load_flags;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetLoadFlags(nsIHttpChannel *iface, nsLoadFlags aLoadFlags)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%08x)\n", This, aLoadFlags);

    This->load_flags = aLoadFlags;
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetOriginalURI(nsIHttpChannel *iface, nsIURI **aOriginalURI)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aOriginalURI);

    if(This->original_uri)
        nsIURI_AddRef(This->original_uri);

    *aOriginalURI = This->original_uri;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetOriginalURI(nsIHttpChannel *iface, nsIURI *aOriginalURI)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aOriginalURI);

    if(This->original_uri)
        nsIURI_Release(This->original_uri);

    nsIURI_AddRef(aOriginalURI);
    This->original_uri = aOriginalURI;
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetURI(nsIHttpChannel *iface, nsIURI **aURI)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aURI);

    nsIURI_AddRef(&This->uri->nsIURL_iface);
    *aURI = (nsIURI*)This->uri;

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetOwner(nsIHttpChannel *iface, nsISupports **aOwner)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aOwner);

    if(This->owner)
        nsISupports_AddRef(This->owner);
    *aOwner = This->owner;

    return NS_OK;
}

static nsresult NSAPI nsChannel_SetOwner(nsIHttpChannel *iface, nsISupports *aOwner)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aOwner);

    if(aOwner)
        nsISupports_AddRef(aOwner);
    if(This->owner)
        nsISupports_Release(This->owner);
    This->owner = aOwner;

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetNotificationCallbacks(nsIHttpChannel *iface,
        nsIInterfaceRequestor **aNotificationCallbacks)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aNotificationCallbacks);

    if(This->notif_callback)
        nsIInterfaceRequestor_AddRef(This->notif_callback);
    *aNotificationCallbacks = This->notif_callback;

    return NS_OK;
}

static nsresult NSAPI nsChannel_SetNotificationCallbacks(nsIHttpChannel *iface,
        nsIInterfaceRequestor *aNotificationCallbacks)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aNotificationCallbacks);

    if(This->notif_callback)
        nsIInterfaceRequestor_Release(This->notif_callback);
    if(aNotificationCallbacks)
        nsIInterfaceRequestor_AddRef(aNotificationCallbacks);

    This->notif_callback = aNotificationCallbacks;

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetSecurityInfo(nsIHttpChannel *iface, nsISupports **aSecurityInfo)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aSecurityInfo);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetContentType(nsIHttpChannel *iface, nsACString *aContentType)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aContentType);

    if(This->content_type) {
        nsACString_SetData(aContentType, This->content_type);
        return S_OK;
    }

    WARN("unknown type\n");
    return NS_ERROR_FAILURE;
}

static nsresult NSAPI nsChannel_SetContentType(nsIHttpChannel *iface,
                                               const nsACString *aContentType)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);
    const char *content_type;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aContentType));

    nsACString_GetData(aContentType, &content_type);
    heap_free(This->content_type);
    This->content_type = heap_strdupA(content_type);

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetContentCharset(nsIHttpChannel *iface,
                                                  nsACString *aContentCharset)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aContentCharset);

    if(This->charset) {
        nsACString_SetData(aContentCharset, This->charset);
        return NS_OK;
    }

    nsACString_SetData(aContentCharset, "");
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetContentCharset(nsIHttpChannel *iface,
                                                  const nsACString *aContentCharset)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%s)\n", This, debugstr_nsacstr(aContentCharset));

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetContentLength(nsIHttpChannel *iface, PRInt32 *aContentLength)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, aContentLength);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetContentLength(nsIHttpChannel *iface, PRInt32 aContentLength)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%d)\n", This, aContentLength);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_Open(nsIHttpChannel *iface, nsIInputStream **_retval)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static HTMLWindow *get_window_from_load_group(nsChannel *This)
{
    HTMLWindow *window;
    nsIChannel *channel;
    nsIRequest *req;
    nsWineURI *wine_uri;
    nsIURI *uri;
    nsresult nsres;

    nsres = nsILoadGroup_GetDefaultLoadRequest(This->load_group, &req);
    if(NS_FAILED(nsres)) {
        ERR("GetDefaultLoadRequest failed: %08x\n", nsres);
        return NULL;
    }

    if(!req)
        return NULL;

    nsres = nsIRequest_QueryInterface(req, &IID_nsIChannel, (void**)&channel);
    nsIRequest_Release(req);
    if(NS_FAILED(nsres)) {
        WARN("Could not get nsIChannel interface: %08x\n", nsres);
        return NULL;
    }

    nsres = nsIChannel_GetURI(channel, &uri);
    nsIChannel_Release(channel);
    if(NS_FAILED(nsres)) {
        ERR("GetURI failed: %08x\n", nsres);
        return NULL;
    }

    nsres = nsIURI_QueryInterface(uri, &IID_nsWineURI, (void**)&wine_uri);
    nsIURI_Release(uri);
    if(NS_FAILED(nsres)) {
        TRACE("Could not get nsWineURI: %08x\n", nsres);
        return NULL;
    }

    window = wine_uri->window_ref ? wine_uri->window_ref->window : NULL;
    if(window)
        IHTMLWindow2_AddRef(&window->IHTMLWindow2_iface);
    nsIURI_Release(&wine_uri->nsIURL_iface);

    return window;
}

static HTMLWindow *get_channel_window(nsChannel *This)
{
    nsIWebProgress *web_progress;
    nsIDOMWindow *nswindow;
    HTMLWindow *window;
    nsresult nsres;

    if(This->load_group) {
        nsIRequestObserver *req_observer;

        nsres = nsILoadGroup_GetGroupObserver(This->load_group, &req_observer);
        if(NS_FAILED(nsres) || !req_observer) {
            ERR("GetGroupObserver failed: %08x\n", nsres);
            return NULL;
        }

        nsres = nsIRequestObserver_QueryInterface(req_observer, &IID_nsIWebProgress, (void**)&web_progress);
        nsIRequestObserver_Release(req_observer);
        if(NS_FAILED(nsres)) {
            ERR("Could not get nsIWebProgress iface: %08x\n", nsres);
            return NULL;
        }
    }else if(This->notif_callback) {
        nsres = nsIInterfaceRequestor_GetInterface(This->notif_callback, &IID_nsIWebProgress, (void**)&web_progress);
        if(NS_FAILED(nsres)) {
            ERR("GetInterface(IID_nsIWebProgress failed: %08x\n", nsres);
            return NULL;
        }
    }else {
        ERR("no load group nor notif callback\n");
        return NULL;
    }

    nsres = nsIWebProgress_GetDOMWindow(web_progress, &nswindow);
    nsIWebProgress_Release(web_progress);
    if(NS_FAILED(nsres) || !nswindow) {
        ERR("GetDOMWindow failed: %08x\n", nsres);
        return NULL;
    }

    window = nswindow_to_window(nswindow);
    nsIDOMWindow_Release(nswindow);

    if(window)
        IHTMLWindow2_AddRef(&window->IHTMLWindow2_iface);
    else
        FIXME("NULL window for %p\n", nswindow);
    return window;
}

typedef struct {
    task_t header;
    HTMLDocumentNode *doc;
    nsChannelBSC *bscallback;
} start_binding_task_t;

static void start_binding_proc(task_t *_task)
{
    start_binding_task_t *task = (start_binding_task_t*)_task;

    start_binding(NULL, task->doc, (BSCallback*)task->bscallback, NULL);

    IUnknown_Release((IUnknown*)task->bscallback);
}

static nsresult async_open(nsChannel *This, HTMLWindow *window, BOOL is_doc_channel, nsIStreamListener *listener,
        nsISupports *context)
{
    nsChannelBSC *bscallback;
    IMoniker *mon = NULL;
    HRESULT hres;

    hres = CreateURLMoniker(NULL, This->uri->wine_url, &mon);
    if(FAILED(hres)) {
        WARN("CreateURLMoniker failed: %08x\n", hres);
        return NS_ERROR_UNEXPECTED;
    }

    if(is_doc_channel)
        set_current_mon(window, mon);

    hres = create_channelbsc(mon, NULL, NULL, 0, &bscallback);
    IMoniker_Release(mon);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    channelbsc_set_channel(bscallback, This, listener, context);

    if(is_doc_channel) {
        set_window_bscallback(window, bscallback);
        async_start_doc_binding(window, bscallback);
        IUnknown_Release((IUnknown*)bscallback);
    }else {
        start_binding_task_t *task = heap_alloc(sizeof(start_binding_task_t));

        task->doc = window->doc;
        task->bscallback = bscallback;
        push_task(&task->header, start_binding_proc, window->doc->basedoc.task_magic);
    }

    return NS_OK;
}

static nsresult NSAPI nsChannel_AsyncOpen(nsIHttpChannel *iface, nsIStreamListener *aListener,
                                          nsISupports *aContext)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);
    HTMLWindow *window = NULL;
    BOOL open = TRUE;
    nsresult nsres = NS_OK;

    TRACE("(%p)->(%p %p) opening %s\n", This, aListener, aContext, debugstr_w(This->uri->wine_url));

    if(This->uri->is_doc_uri) {
        window = get_channel_window(This);
        if(window) {
            set_uri_window(This->uri, window);
        }else if(This->uri->container) {
            BOOL b;

            /* nscontainer->doc should be NULL which means navigation to a new window */
            if(This->uri->container->doc)
                FIXME("nscontainer->doc = %p\n", This->uri->container->doc);

            b = before_async_open(This, This->uri->container);
            if(b)
                FIXME("Navigation not cancelled\n");
            return NS_ERROR_UNEXPECTED;
        }
    }

    if(!window) {
        if(This->uri->window_ref && This->uri->window_ref->window) {
            window = This->uri->window_ref->window;
            IHTMLWindow2_AddRef(&window->IHTMLWindow2_iface);
        }else {
            /* FIXME: Analyze removing get_window_from_load_group call */
            if(This->load_group)
                window = get_window_from_load_group(This);
            if(!window)
                window = get_channel_window(This);
            if(window)
                set_uri_window(This->uri, window);
        }
    }

    if(!window) {
        ERR("window = NULL\n");
        return NS_ERROR_UNEXPECTED;
    }

    if(This->uri->is_doc_uri && window == window->doc_obj->basedoc.window) {
        if(This->uri->channel_bsc) {
            channelbsc_set_channel(This->uri->channel_bsc, This, aListener, aContext);

            if(window->doc_obj->mime) {
                heap_free(This->content_type);
                This->content_type = heap_strdupWtoA(window->doc_obj->mime);
            }

            open = FALSE;
        }else {
            open = !before_async_open(This, window->doc_obj->nscontainer);
            if(!open) {
                TRACE("canceled\n");
                nsres = NS_ERROR_UNEXPECTED;
            }
        }
    }

    if(open)
        nsres = async_open(This, window, This->uri->is_doc_uri, aListener, aContext);

    if(NS_SUCCEEDED(nsres) && This->load_group) {
        nsres = nsILoadGroup_AddRequest(This->load_group, (nsIRequest*)&This->nsIHttpChannel_iface,
                aContext);
        if(NS_FAILED(nsres))
            ERR("AddRequest failed: %08x\n", nsres);
    }

    IHTMLWindow2_Release(&window->IHTMLWindow2_iface);
    return nsres;
}

static nsresult NSAPI nsChannel_GetRequestMethod(nsIHttpChannel *iface, nsACString *aRequestMethod)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aRequestMethod);

    nsACString_SetData(aRequestMethod, request_method_strings[This->request_method]);
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetRequestMethod(nsIHttpChannel *iface,
                                                 const nsACString *aRequestMethod)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);
    const char *method;
    unsigned i;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aRequestMethod));

    nsACString_GetData(aRequestMethod, &method);
    for(i=0; i < sizeof(request_method_strings)/sizeof(*request_method_strings); i++) {
        if(!strcasecmp(method, request_method_strings[i])) {
            This->request_method = i;
            return NS_OK;
        }
    }

    ERR("Invalid method %s\n", debugstr_a(method));
    return NS_ERROR_UNEXPECTED;
}

static nsresult NSAPI nsChannel_GetReferrer(nsIHttpChannel *iface, nsIURI **aReferrer)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aReferrer);

    if(This->referrer)
        nsIURI_AddRef(This->referrer);
    *aReferrer = This->referrer;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetReferrer(nsIHttpChannel *iface, nsIURI *aReferrer)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aReferrer);

    if(aReferrer)
        nsIURI_AddRef(aReferrer);
    if(This->referrer)
        nsIURI_Release(This->referrer);
    This->referrer = aReferrer;
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetRequestHeader(nsIHttpChannel *iface,
         const nsACString *aHeader, nsACString *_retval)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_nsacstr(aHeader), _retval);

    return get_channel_http_header(&This->request_headers, aHeader, _retval);
}

static nsresult NSAPI nsChannel_SetRequestHeader(nsIHttpChannel *iface,
         const nsACString *aHeader, const nsACString *aValue, PRBool aMerge)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%s %s %x)\n", This, debugstr_nsacstr(aHeader), debugstr_nsacstr(aValue), aMerge);

    if(aMerge)
        FIXME("aMerge not supported\n");

    return set_channel_http_header(&This->request_headers, aHeader, aValue);
}

static nsresult NSAPI nsChannel_VisitRequestHeaders(nsIHttpChannel *iface,
                                                    nsIHttpHeaderVisitor *aVisitor)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, aVisitor);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetAllowPipelining(nsIHttpChannel *iface, PRBool *aAllowPipelining)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, aAllowPipelining);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetAllowPipelining(nsIHttpChannel *iface, PRBool aAllowPipelining)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%x)\n", This, aAllowPipelining);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetRedirectionLimit(nsIHttpChannel *iface, PRUint32 *aRedirectionLimit)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, aRedirectionLimit);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetRedirectionLimit(nsIHttpChannel *iface, PRUint32 aRedirectionLimit)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%u)\n", This, aRedirectionLimit);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetResponseStatus(nsIHttpChannel *iface, PRUint32 *aResponseStatus)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aResponseStatus);

    if(This->response_status) {
        *aResponseStatus = This->response_status;
        return NS_OK;
    }

    WARN("No response status\n");
    return NS_ERROR_UNEXPECTED;
}

static nsresult NSAPI nsChannel_GetResponseStatusText(nsIHttpChannel *iface,
                                                      nsACString *aResponseStatusText)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, aResponseStatusText);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetRequestSucceeded(nsIHttpChannel *iface,
                                                    PRBool *aRequestSucceeded)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aRequestSucceeded);

    if(!This->response_status)
        return NS_ERROR_NOT_AVAILABLE;

    *aRequestSucceeded = This->response_status/100 == 2;

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetResponseHeader(nsIHttpChannel *iface,
         const nsACString *header, nsACString *_retval)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_nsacstr(header), _retval);

    return get_channel_http_header(&This->response_headers, header, _retval);
}

static nsresult NSAPI nsChannel_SetResponseHeader(nsIHttpChannel *iface,
        const nsACString *header, const nsACString *value, PRBool merge)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%s %s %x)\n", This, debugstr_nsacstr(header), debugstr_nsacstr(value), merge);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_VisitResponseHeaders(nsIHttpChannel *iface,
        nsIHttpHeaderVisitor *aVisitor)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    TRACE("(%p)->(%p)\n", This, aVisitor);

    return visit_http_headers(&This->response_headers, aVisitor);
}

static nsresult NSAPI nsChannel_IsNoStoreResponse(nsIHttpChannel *iface, PRBool *_retval)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);
    http_header_t *header;

    static const WCHAR cache_controlW[] = {'C','a','c','h','e','-','C','o','n','t','r','o','l'};
    static const WCHAR no_storeW[] = {'n','o','-','s','t','o','r','e',0};

    TRACE("(%p)->(%p)\n", This, _retval);

    header = find_http_header(&This->response_headers, cache_controlW, sizeof(cache_controlW)/sizeof(WCHAR));
    *_retval = header && !strcmpiW(header->data, no_storeW);
    return NS_OK;
}

static nsresult NSAPI nsChannel_IsNoCacheResponse(nsIHttpChannel *iface, PRBool *_retval)
{
    nsChannel *This = impl_from_nsIHttpChannel(iface);

    FIXME("(%p)->(%p)\n", This, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static const nsIHttpChannelVtbl nsChannelVtbl = {
    nsChannel_QueryInterface,
    nsChannel_AddRef,
    nsChannel_Release,
    nsChannel_GetName,
    nsChannel_IsPending,
    nsChannel_GetStatus,
    nsChannel_Cancel,
    nsChannel_Suspend,
    nsChannel_Resume,
    nsChannel_GetLoadGroup,
    nsChannel_SetLoadGroup,
    nsChannel_GetLoadFlags,
    nsChannel_SetLoadFlags,
    nsChannel_GetOriginalURI,
    nsChannel_SetOriginalURI,
    nsChannel_GetURI,
    nsChannel_GetOwner,
    nsChannel_SetOwner,
    nsChannel_GetNotificationCallbacks,
    nsChannel_SetNotificationCallbacks,
    nsChannel_GetSecurityInfo,
    nsChannel_GetContentType,
    nsChannel_SetContentType,
    nsChannel_GetContentCharset,
    nsChannel_SetContentCharset,
    nsChannel_GetContentLength,
    nsChannel_SetContentLength,
    nsChannel_Open,
    nsChannel_AsyncOpen,
    nsChannel_GetRequestMethod,
    nsChannel_SetRequestMethod,
    nsChannel_GetReferrer,
    nsChannel_SetReferrer,
    nsChannel_GetRequestHeader,
    nsChannel_SetRequestHeader,
    nsChannel_VisitRequestHeaders,
    nsChannel_GetAllowPipelining,
    nsChannel_SetAllowPipelining,
    nsChannel_GetRedirectionLimit,
    nsChannel_SetRedirectionLimit,
    nsChannel_GetResponseStatus,
    nsChannel_GetResponseStatusText,
    nsChannel_GetRequestSucceeded,
    nsChannel_GetResponseHeader,
    nsChannel_SetResponseHeader,
    nsChannel_VisitResponseHeaders,
    nsChannel_IsNoStoreResponse,
    nsChannel_IsNoCacheResponse
};

static inline nsChannel *impl_from_nsIUploadChannel(nsIUploadChannel *iface)
{
    return CONTAINING_RECORD(iface, nsChannel, nsIUploadChannel_iface);
}

static nsresult NSAPI nsUploadChannel_QueryInterface(nsIUploadChannel *iface, nsIIDRef riid,
        void **result)
{
    nsChannel *This = impl_from_nsIUploadChannel(iface);
    return nsIChannel_QueryInterface(&This->nsIHttpChannel_iface, riid, result);
}

static nsrefcnt NSAPI nsUploadChannel_AddRef(nsIUploadChannel *iface)
{
    nsChannel *This = impl_from_nsIUploadChannel(iface);
    return nsIChannel_AddRef(&This->nsIHttpChannel_iface);
}

static nsrefcnt NSAPI nsUploadChannel_Release(nsIUploadChannel *iface)
{
    nsChannel *This = impl_from_nsIUploadChannel(iface);
    return nsIChannel_Release(&This->nsIHttpChannel_iface);
}

static nsresult NSAPI nsUploadChannel_SetUploadStream(nsIUploadChannel *iface,
        nsIInputStream *aStream, const nsACString *aContentType, PRInt32 aContentLength)
{
    nsChannel *This = impl_from_nsIUploadChannel(iface);
    const char *content_type;

    static const WCHAR content_typeW[] =
        {'C','o','n','t','e','n','t','-','T','y','p','e',0};

    TRACE("(%p)->(%p %s %d)\n", This, aStream, debugstr_nsacstr(aContentType), aContentLength);

    This->post_data_contains_headers = TRUE;

    if(aContentType) {
        nsACString_GetData(aContentType, &content_type);
        if(*content_type) {
            WCHAR *ct;

            ct = heap_strdupAtoW(content_type);
            if(!ct)
                return NS_ERROR_UNEXPECTED;

            set_http_header(&This->request_headers, content_typeW,
                    sizeof(content_typeW)/sizeof(WCHAR), ct, strlenW(ct));
            heap_free(ct);
            This->post_data_contains_headers = FALSE;
        }
    }

    if(This->post_data_stream)
        nsIInputStream_Release(This->post_data_stream);

    if(aContentLength != -1)
        FIXME("Unsupported acontentLength = %d\n", aContentLength);

    if(This->post_data_stream)
        nsIInputStream_Release(This->post_data_stream);
    This->post_data_stream = aStream;
    if(aStream)
        nsIInputStream_AddRef(aStream);

    This->request_method = METHOD_POST;
    return NS_OK;
}

static nsresult NSAPI nsUploadChannel_GetUploadStream(nsIUploadChannel *iface,
        nsIInputStream **aUploadStream)
{
    nsChannel *This = impl_from_nsIUploadChannel(iface);

    TRACE("(%p)->(%p)\n", This, aUploadStream);

    if(This->post_data_stream)
        nsIInputStream_AddRef(This->post_data_stream);

    *aUploadStream = This->post_data_stream;
    return NS_OK;
}

static const nsIUploadChannelVtbl nsUploadChannelVtbl = {
    nsUploadChannel_QueryInterface,
    nsUploadChannel_AddRef,
    nsUploadChannel_Release,
    nsUploadChannel_SetUploadStream,
    nsUploadChannel_GetUploadStream
};

static inline nsChannel *impl_from_nsIHttpChannelInternal(nsIHttpChannelInternal *iface)
{
    return CONTAINING_RECORD(iface, nsChannel, nsIHttpChannelInternal_iface);
}

static nsresult NSAPI nsHttpChannelInternal_QueryInterface(nsIHttpChannelInternal *iface, nsIIDRef riid,
        void **result)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);
    return nsIChannel_QueryInterface(&This->nsIHttpChannel_iface, riid, result);
}

static nsrefcnt NSAPI nsHttpChannelInternal_AddRef(nsIHttpChannelInternal *iface)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);
    return nsIChannel_AddRef(&This->nsIHttpChannel_iface);
}

static nsrefcnt NSAPI nsHttpChannelInternal_Release(nsIHttpChannelInternal *iface)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);
    return nsIChannel_Release(&This->nsIHttpChannel_iface);
}

static nsresult NSAPI nsHttpChannelInternal_GetDocumentURI(nsIHttpChannelInternal *iface, nsIURI **aDocumentURI)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_SetDocumentURI(nsIHttpChannelInternal *iface, nsIURI *aDocumentURI)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_GetRequestVersion(nsIHttpChannelInternal *iface, PRUint32 *major, PRUint32 *minor)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_GetResponseVersion(nsIHttpChannelInternal *iface, PRUint32 *major, PRUint32 *minor)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_SetCookie(nsIHttpChannelInternal *iface, const char *aCookieHeader)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_SetupFallbackChannel(nsIHttpChannelInternal *iface, const char *aFallbackKey)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_GetForceAllowThirdPartyCookie(nsIHttpChannelInternal *iface, PRBool *aForceThirdPartyCookie)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_SetForceAllowThirdPartyCookie(nsIHttpChannelInternal *iface, PRBool aForceThirdPartyCookie)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->()\n", This);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_GetCanceled(nsIHttpChannelInternal *iface, PRBool *aCanceled)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->(%p)\n", This, aCanceled);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_GetChannelIsForDownload(nsIHttpChannelInternal *iface, PRBool *aCanceled)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->(%p)\n", This, aCanceled);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsHttpChannelInternal_SetChannelIsForDownload(nsIHttpChannelInternal *iface, PRBool aCanceled)
{
    nsChannel *This = impl_from_nsIHttpChannelInternal(iface);

    FIXME("(%p)->(%x)\n", This, aCanceled);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static const nsIHttpChannelInternalVtbl nsHttpChannelInternalVtbl = {
    nsHttpChannelInternal_QueryInterface,
    nsHttpChannelInternal_AddRef,
    nsHttpChannelInternal_Release,
    nsHttpChannelInternal_GetDocumentURI,
    nsHttpChannelInternal_SetDocumentURI,
    nsHttpChannelInternal_GetRequestVersion,
    nsHttpChannelInternal_GetResponseVersion,
    nsHttpChannelInternal_SetCookie,
    nsHttpChannelInternal_SetupFallbackChannel,
    nsHttpChannelInternal_GetForceAllowThirdPartyCookie,
    nsHttpChannelInternal_SetForceAllowThirdPartyCookie,
    nsHttpChannelInternal_GetCanceled,
    nsHttpChannelInternal_GetChannelIsForDownload,
    nsHttpChannelInternal_SetChannelIsForDownload
};

static BOOL ensure_uri(nsWineURI *This)
{
    HRESULT hres;

    if(This->uri)
        return TRUE;

    if(This->uri_builder) {
        hres = IUriBuilder_CreateUriSimple(This->uri_builder, 0, 0, &This->uri);
        if(FAILED(hres)) {
            WARN("CreateUriSimple failed: %08x\n", hres);
            return FALSE;
        }

        return TRUE;
    }

    hres = CreateUri(This->wine_url, 0, 0, &This->uri);
    if(FAILED(hres)) {
        WARN("CreateUri failed: %08x\n", hres);
        return FALSE;
    }

    return TRUE;
}

static void invalidate_uri(nsWineURI *This)
{
    if(This->uri) {
        IUri_Release(This->uri);
        This->uri = NULL;
    }
}

static BOOL ensure_uri_builder(nsWineURI *This)
{
    if(!This->uri_builder) {
        HRESULT hres;

        if(!ensure_uri(This))
            return FALSE;

        hres = CreateIUriBuilder(This->uri, 0, 0, &This->uri_builder);
        if(FAILED(hres)) {
            WARN("CreateIUriBulder failed: %08x\n", hres);
            return FALSE;
        }
    }

    invalidate_uri(This);
    return TRUE;
}

/* Temporary helper */
static void sync_wine_url(nsWineURI *This)
{
    BSTR new_url;
    HRESULT hres;

    if(!ensure_uri(This))
        return;

    hres = IUri_GetDisplayUri(This->uri, &new_url);
    if(FAILED(hres))
        return;

    SysFreeString(This->wine_url);
    This->wine_url = new_url;
}

static nsresult get_uri_string(nsWineURI *This, Uri_PROPERTY prop, nsACString *ret)
{
    char *vala;
    BSTR val;
    HRESULT hres;

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    hres = IUri_GetPropertyBSTR(This->uri, prop, &val, 0);
    if(FAILED(hres)) {
        WARN("GetPropertyBSTR failed: %08x\n", hres);
        return NS_ERROR_UNEXPECTED;
    }

    vala = heap_strdupWtoA(val);
    SysFreeString(val);
    if(!vala)
        return NS_ERROR_OUT_OF_MEMORY;

    TRACE("ret %s\n", debugstr_a(vala));
    nsACString_SetData(ret, vala);
    heap_free(vala);
    return NS_OK;
}

static inline nsWineURI *impl_from_nsIURL(nsIURL *iface)
{
    return CONTAINING_RECORD(iface, nsWineURI, nsIURL_iface);
}

static nsresult NSAPI nsURI_QueryInterface(nsIURL *iface, nsIIDRef riid, void **result)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result = &This->nsIURL_iface;
    }else if(IsEqualGUID(&IID_nsIURI, riid)) {
        TRACE("(%p)->(IID_nsIURI %p)\n", This, result);
        *result = &This->nsIURL_iface;
    }else if(IsEqualGUID(&IID_nsIURL, riid)) {
        TRACE("(%p)->(IID_nsIURL %p)\n", This, result);
        *result = &This->nsIURL_iface;
    }else if(IsEqualGUID(&IID_nsWineURI, riid)) {
        TRACE("(%p)->(IID_nsWineURI %p)\n", This, result);
        *result = This;
    }

    if(*result) {
        nsIURI_AddRef(&This->nsIURL_iface);
        return NS_OK;
    }

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), result);
    return This->nsuri ? nsIURI_QueryInterface(This->nsuri, riid, result) : NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsURI_AddRef(nsIURL *iface)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsURI_Release(nsIURL *iface)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        if(This->window_ref)
            windowref_release(This->window_ref);
        if(This->container)
            nsIWebBrowserChrome_Release(&This->container->nsIWebBrowserChrome_iface);
        if(This->nsurl)
            nsIURL_Release(This->nsurl);
        if(This->nsuri)
            nsIURI_Release(This->nsuri);
        if(This->uri)
            IUri_Release(This->uri);
        SysFreeString(This->wine_url);
        heap_free(This);
    }

    return ref;
}

static nsresult NSAPI nsURI_GetSpec(nsIURL *iface, nsACString *aSpec)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aSpec);

    return get_uri_string(This, Uri_PROPERTY_DISPLAY_URI, aSpec);
}

static nsresult NSAPI nsURI_SetSpec(nsIURL *iface, const nsACString *aSpec)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *speca;
    WCHAR *spec;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aSpec));

    nsACString_GetData(aSpec, &speca);
    spec = heap_strdupAtoW(speca);
    if(!spec)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = CreateUri(spec, 0, 0, &uri);
    heap_free(spec);
    if(FAILED(hres)) {
        WARN("CreateUri failed: %08x\n", hres);
        return NS_ERROR_FAILURE;
    }

    invalidate_uri(This);
    if(This->uri_builder) {
        IUriBuilder_Release(This->uri_builder);
        This->uri_builder = NULL;
    }

    This->uri = uri;
    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURI_GetPrePath(nsIURL *iface, nsACString *aPrePath)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aPrePath);

    if(This->nsuri)
        return nsIURI_GetPrePath(This->nsuri, aPrePath);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetScheme(nsIURL *iface, nsACString *aScheme)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    DWORD scheme;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, aScheme);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    hres = IUri_GetScheme(This->uri, &scheme);
    if(FAILED(hres)) {
        WARN("GetScheme failed: %08x\n", hres);
        return NS_ERROR_UNEXPECTED;
    }

    if(scheme == URL_SCHEME_ABOUT) {
        nsACString_SetData(aScheme, "wine");
        return NS_OK;
    }

    return get_uri_string(This, Uri_PROPERTY_SCHEME_NAME, aScheme);
}

static nsresult NSAPI nsURI_SetScheme(nsIURL *iface, const nsACString *aScheme)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *schemea;
    WCHAR *scheme;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aScheme));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aScheme, &schemea);
    scheme = heap_strdupAtoW(schemea);
    if(!scheme)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetSchemeName(This->uri_builder, scheme);
    heap_free(scheme);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURI_GetUserPass(nsIURL *iface, nsACString *aUserPass)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aUserPass);

    if(This->nsuri)
        return nsIURI_GetUserPass(This->nsuri, aUserPass);

    return get_uri_string(This, Uri_PROPERTY_USER_INFO, aUserPass);
}

static nsresult NSAPI nsURI_SetUserPass(nsIURL *iface, const nsACString *aUserPass)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aUserPass));

    if(This->nsuri) {
        invalidate_uri(This);
        return nsIURI_SetUserPass(This->nsuri, aUserPass);
    }

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetUsername(nsIURL *iface, nsACString *aUsername)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aUsername);

    return get_uri_string(This, Uri_PROPERTY_USER_NAME, aUsername);
}

static nsresult NSAPI nsURI_SetUsername(nsIURL *iface, const nsACString *aUsername)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *usera;
    WCHAR *user;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aUsername));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aUsername, &usera);
    user = heap_strdupAtoW(usera);
    if(!user)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetUserName(This->uri_builder, user);
    heap_free(user);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURI_GetPassword(nsIURL *iface, nsACString *aPassword)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aPassword);

    return get_uri_string(This, Uri_PROPERTY_PASSWORD, aPassword);
}

static nsresult NSAPI nsURI_SetPassword(nsIURL *iface, const nsACString *aPassword)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *passa;
    WCHAR *pass;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aPassword));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aPassword, &passa);
    pass = heap_strdupAtoW(passa);
    if(!pass)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetPassword(This->uri_builder, pass);
    heap_free(pass);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURI_GetHostPort(nsIURL *iface, nsACString *aHostPort)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const WCHAR *ptr;
    char *vala;
    BSTR val;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, aHostPort);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    hres = IUri_GetAuthority(This->uri, &val);
    if(FAILED(hres)) {
        WARN("GetAuthority failed: %08x\n", hres);
        return NS_ERROR_UNEXPECTED;
    }

    ptr = strchrW(val, '@');
    if(!ptr)
        ptr = val;

    vala = heap_strdupWtoA(ptr);
    SysFreeString(val);
    if(!vala)
        return NS_ERROR_OUT_OF_MEMORY;

    TRACE("ret %s\n", debugstr_a(vala));
    nsACString_SetData(aHostPort, vala);
    heap_free(vala);
    return NS_OK;
}

static nsresult NSAPI nsURI_SetHostPort(nsIURL *iface, const nsACString *aHostPort)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    WARN("(%p)->(%s)\n", This, debugstr_nsacstr(aHostPort));

    /* Not implemented by Gecko */
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetHost(nsIURL *iface, nsACString *aHost)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aHost);

    return get_uri_string(This, Uri_PROPERTY_HOST, aHost);
}

static nsresult NSAPI nsURI_SetHost(nsIURL *iface, const nsACString *aHost)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *hosta;
    WCHAR *host;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aHost));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aHost, &hosta);
    host = heap_strdupAtoW(hosta);
    if(!host)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetHost(This->uri_builder, host);
    heap_free(host);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURI_GetPort(nsIURL *iface, PRInt32 *aPort)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    DWORD port;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, aPort);

    if(This->nsuri)
        return nsIURI_GetPort(This->nsuri, aPort);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    hres = IUri_GetPort(This->uri, &port);
    if(FAILED(hres)) {
        WARN("GetPort failed: %08x\n", hres);
        return NS_ERROR_UNEXPECTED;
    }

    *aPort = port ? port : -1;
    return NS_OK;
}

static nsresult NSAPI nsURI_SetPort(nsIURL *iface, PRInt32 aPort)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%d)\n", This, aPort);

    if(This->nsuri) {
        invalidate_uri(This);
        return nsIURI_SetPort(This->nsuri, aPort);
    }

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetPath(nsIURL *iface, nsACString *aPath)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aPath);

    return get_uri_string(This, Uri_PROPERTY_PATH, aPath);
}

static nsresult NSAPI nsURI_SetPath(nsIURL *iface, const nsACString *aPath)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *patha;
    WCHAR *path;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aPath));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aPath, &patha);
    path = heap_strdupAtoW(patha);
    if(!path)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetPath(This->uri_builder, path);
    heap_free(path);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURI_Equals(nsIURL *iface, nsIURI *other, PRBool *_retval)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    nsWineURI *other_obj;
    nsresult nsres;
    HRESULT hres;

    TRACE("(%p)->(%p %p)\n", This, other, _retval);

    nsres = nsIURI_QueryInterface(other, &IID_nsWineURI, (void**)&other_obj);
    if(NS_FAILED(nsres)) {
        TRACE("Could not get nsWineURI interface\n");
        *_retval = FALSE;
        return NS_OK;
    }

    if(ensure_uri(This) && ensure_uri(other_obj)) {
        hres = IUri_IsEqual(This->uri, other_obj->uri, _retval);
        nsres = SUCCEEDED(hres) ? NS_OK : NS_ERROR_FAILURE;
    }else {
        nsres = NS_ERROR_UNEXPECTED;
    }

    nsIURI_Release(&other_obj->nsIURL_iface);
    return nsres;
}

static nsresult NSAPI nsURI_SchemeIs(nsIURL *iface, const char *scheme, PRBool *_retval)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    WCHAR buf[INTERNET_MAX_SCHEME_LENGTH];
    BSTR scheme_name;
    HRESULT hres;

    TRACE("(%p)->(%s %p)\n", This, debugstr_a(scheme), _retval);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    hres = IUri_GetSchemeName(This->uri, &scheme_name);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    MultiByteToWideChar(CP_ACP, 0, scheme, -1, buf, sizeof(buf)/sizeof(WCHAR));
    *_retval = !strcmpW(scheme_name, buf);
    SysFreeString(scheme_name);
    return NS_OK;
}

static nsresult NSAPI nsURI_Clone(nsIURL *iface, nsIURI **_retval)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    nsIURI *nsuri = NULL;
    nsWineURI *wine_uri;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, _retval);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    if(This->nsuri) {
        nsres = nsIURI_Clone(This->nsuri, &nsuri);
        if(NS_FAILED(nsres)) {
            WARN("Clone failed: %08x\n", nsres);
            return nsres;
        }
    }

    nsres = create_uri(nsuri, This->window_ref ? This->window_ref->window : NULL, This->container, &wine_uri);
    if(NS_FAILED(nsres)) {
        WARN("create_uri failed: %08x\n", nsres);
        return nsres;
    }

    wine_uri->uri = This->uri;
    IUri_AddRef(wine_uri->uri);
    sync_wine_url(wine_uri);

    *_retval = (nsIURI*)&wine_uri->nsIURL_iface;
    return NS_OK;
}

static nsresult NSAPI nsURI_Resolve(nsIURL *iface, const nsACString *aRelativePath,
        nsACString *_retval)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *patha;
    IUri *new_uri;
    WCHAR *path;
    char *reta;
    BSTR ret;
    HRESULT hres;

    TRACE("(%p)->(%s %p)\n", This, debugstr_nsacstr(aRelativePath), _retval);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aRelativePath, &patha);
    path = heap_strdupAtoW(patha);
    if(!path)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = CoInternetCombineUrlEx(This->uri, path, URL_ESCAPE_SPACES_ONLY|URL_DONT_ESCAPE_EXTRA_INFO, &new_uri, 0);
    heap_free(path);
    if(FAILED(hres)) {
        ERR("CoIntenetCombineUrlEx failed: %08x\n", hres);
        return NS_ERROR_FAILURE;
    }

    hres = IUri_GetDisplayUri(new_uri, &ret);
    IUri_Release(new_uri);
    if(FAILED(hres))
        return NS_ERROR_FAILURE;

    reta = heap_strdupWtoA(ret);
    SysFreeString(ret);
    if(!reta)
        return NS_ERROR_OUT_OF_MEMORY;

    TRACE("returning %s\n", debugstr_a(reta));
    nsACString_SetData(_retval, reta);
    heap_free(reta);
    return NS_OK;
}

static nsresult NSAPI nsURI_GetAsciiSpec(nsIURL *iface, nsACString *aAsciiSpec)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aAsciiSpec);

    return nsIURI_GetSpec(&This->nsIURL_iface, aAsciiSpec);
}

static nsresult NSAPI nsURI_GetAsciiHost(nsIURL *iface, nsACString *aAsciiHost)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aAsciiHost);

    if(This->nsuri)
        return nsIURI_GetAsciiHost(This->nsuri, aAsciiHost);

    FIXME("Use Uri_PUNYCODE_IDN_HOST flag\n");
    return get_uri_string(This, Uri_PROPERTY_HOST, aAsciiHost);
}

static nsresult NSAPI nsURI_GetOriginCharset(nsIURL *iface, nsACString *aOriginCharset)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aOriginCharset);

    if(This->nsuri)
        return nsIURI_GetOriginCharset(This->nsuri, aOriginCharset);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetFilePath(nsIURL *iface, nsACString *aFilePath)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aFilePath);

    return nsIURL_GetPath(&This->nsIURL_iface, aFilePath);
}

static nsresult NSAPI nsURL_SetFilePath(nsIURL *iface, const nsACString *aFilePath)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aFilePath));

    return nsIURL_SetPath(&This->nsIURL_iface, aFilePath);
}

static nsresult NSAPI nsURL_GetParam(nsIURL *iface, nsACString *aParam)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aParam);

    if(This->nsurl)
        return nsIURL_GetParam(This->nsurl, aParam);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_SetParam(nsIURL *iface, const nsACString *aParam)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    WARN("(%p)->(%s)\n", This, debugstr_nsacstr(aParam));

    /* Not implemented by Gecko */
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetQuery(nsIURL *iface, nsACString *aQuery)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aQuery);

    return get_uri_string(This, Uri_PROPERTY_QUERY, aQuery);
}

static nsresult NSAPI nsURL_SetQuery(nsIURL *iface, const nsACString *aQuery)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *querya;
    WCHAR *query;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aQuery));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aQuery, &querya);
    query = heap_strdupAtoW(querya);
    if(!query)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetQuery(This->uri_builder, query);
    heap_free(query);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURL_GetRef(nsIURL *iface, nsACString *aRef)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    char *refa = NULL;
    BSTR ref;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, aRef);

    if(!ensure_uri(This))
        return NS_ERROR_UNEXPECTED;

    hres = IUri_GetFragment(This->uri, &ref);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    refa = heap_strdupWtoA(ref);
    SysFreeString(ref);
    if(ref && !refa)
        return NS_ERROR_OUT_OF_MEMORY;

    nsACString_SetData(aRef, refa && *refa == '#' ? refa+1 : refa);
    heap_free(refa);
    return NS_OK;
}

static nsresult NSAPI nsURL_SetRef(nsIURL *iface, const nsACString *aRef)
{
    nsWineURI *This = impl_from_nsIURL(iface);
    const char *refa;
    WCHAR *ref;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aRef));

    if(!ensure_uri_builder(This))
        return NS_ERROR_UNEXPECTED;

    nsACString_GetData(aRef, &refa);
    ref = heap_strdupAtoW(refa);
    if(!ref)
        return NS_ERROR_OUT_OF_MEMORY;

    hres = IUriBuilder_SetFragment(This->uri_builder, ref);
    heap_free(ref);
    if(FAILED(hres))
        return NS_ERROR_UNEXPECTED;

    sync_wine_url(This);
    return NS_OK;
}

static nsresult NSAPI nsURL_GetDirectory(nsIURL *iface, nsACString *aDirectory)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aDirectory);

    if(This->nsurl)
        return nsIURL_GetDirectory(This->nsurl, aDirectory);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_SetDirectory(nsIURL *iface, const nsACString *aDirectory)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    WARN("(%p)->(%s)\n", This, debugstr_nsacstr(aDirectory));

    /* Not implemented by Gecko */
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetFileName(nsIURL *iface, nsACString *aFileName)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aFileName);

    if(This->nsurl)
        return nsIURL_GetFileName(This->nsurl, aFileName);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_SetFileName(nsIURL *iface, const nsACString *aFileName)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aFileName));

    if(This->nsurl) {
        invalidate_uri(This);
        return nsIURL_SetFileName(This->nsurl, aFileName);
    }

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetFileBaseName(nsIURL *iface, nsACString *aFileBaseName)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aFileBaseName);

    if(This->nsurl)
        return nsIURL_GetFileBaseName(This->nsurl, aFileBaseName);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_SetFileBaseName(nsIURL *iface, const nsACString *aFileBaseName)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aFileBaseName));

    if(This->nsurl) {
        invalidate_uri(This);
        return nsIURL_SetFileBaseName(This->nsurl, aFileBaseName);
    }

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetFileExtension(nsIURL *iface, nsACString *aFileExtension)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p)\n", This, aFileExtension);

    if(This->nsurl)
        return nsIURL_GetFileExtension(This->nsurl, aFileExtension);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_SetFileExtension(nsIURL *iface, const nsACString *aFileExtension)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_nsacstr(aFileExtension));

    if(This->nsurl) {
        invalidate_uri(This);
        return nsIURL_SetFileExtension(This->nsurl, aFileExtension);
    }

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetCommonBaseSpec(nsIURL *iface, nsIURI *aURIToCompare, nsACString *_retval)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p %p)\n", This, aURIToCompare, _retval);

    if(This->nsurl)
        return nsIURL_GetCommonBaseSpec(This->nsurl, aURIToCompare, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURL_GetRelativeSpec(nsIURL *iface, nsIURI *aURIToCompare, nsACString *_retval)
{
    nsWineURI *This = impl_from_nsIURL(iface);

    TRACE("(%p)->(%p %p)\n", This, aURIToCompare, _retval);

    if(This->nsurl)
        return nsIURL_GetRelativeSpec(This->nsurl, aURIToCompare, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static const nsIURLVtbl nsURLVtbl = {
    nsURI_QueryInterface,
    nsURI_AddRef,
    nsURI_Release,
    nsURI_GetSpec,
    nsURI_SetSpec,
    nsURI_GetPrePath,
    nsURI_GetScheme,
    nsURI_SetScheme,
    nsURI_GetUserPass,
    nsURI_SetUserPass,
    nsURI_GetUsername,
    nsURI_SetUsername,
    nsURI_GetPassword,
    nsURI_SetPassword,
    nsURI_GetHostPort,
    nsURI_SetHostPort,
    nsURI_GetHost,
    nsURI_SetHost,
    nsURI_GetPort,
    nsURI_SetPort,
    nsURI_GetPath,
    nsURI_SetPath,
    nsURI_Equals,
    nsURI_SchemeIs,
    nsURI_Clone,
    nsURI_Resolve,
    nsURI_GetAsciiSpec,
    nsURI_GetAsciiHost,
    nsURI_GetOriginCharset,
    nsURL_GetFilePath,
    nsURL_SetFilePath,
    nsURL_GetParam,
    nsURL_SetParam,
    nsURL_GetQuery,
    nsURL_SetQuery,
    nsURL_GetRef,
    nsURL_SetRef,
    nsURL_GetDirectory,
    nsURL_SetDirectory,
    nsURL_GetFileName,
    nsURL_SetFileName,
    nsURL_GetFileBaseName,
    nsURL_SetFileBaseName,
    nsURL_GetFileExtension,
    nsURL_SetFileExtension,
    nsURL_GetCommonBaseSpec,
    nsURL_GetRelativeSpec
};

static nsresult create_uri(nsIURI *nsuri, HTMLWindow *window, NSContainer *container, nsWineURI **_retval)
{
    nsWineURI *ret = heap_alloc_zero(sizeof(nsWineURI));

    ret->nsIURL_iface.lpVtbl = &nsURLVtbl;
    ret->ref = 1;
    ret->nsuri = nsuri;

    set_uri_nscontainer(ret, container);
    set_uri_window(ret, window);

    if(nsuri)
        nsIURI_QueryInterface(nsuri, &IID_nsIURL, (void**)&ret->nsurl);

    TRACE("retval=%p\n", ret);
    *_retval = ret;
    return NS_OK;
}

HRESULT create_doc_uri(HTMLWindow *window, WCHAR *url, nsWineURI **ret)
{
    nsWineURI *uri;
    nsresult nsres;

    nsres = create_uri(NULL, window, window->doc_obj->nscontainer, &uri);
    if(NS_FAILED(nsres))
        return E_FAIL;

    set_wine_url(uri, url);
    uri->is_doc_uri = TRUE;

    *ret = uri;
    return S_OK;
}

static nsresult create_nschannel(nsWineURI *uri, nsChannel **ret)
{
    nsChannel *channel;
    HRESULT hres;

    if(!ensure_uri(uri))
        return NS_ERROR_UNEXPECTED;

    channel = heap_alloc_zero(sizeof(nsChannel));
    if(!channel)
        return NS_ERROR_OUT_OF_MEMORY;

    channel->nsIHttpChannel_iface.lpVtbl = &nsChannelVtbl;
    channel->nsIUploadChannel_iface.lpVtbl = &nsUploadChannelVtbl;
    channel->nsIHttpChannelInternal_iface.lpVtbl = &nsHttpChannelInternalVtbl;
    channel->ref = 1;
    channel->request_method = METHOD_GET;
    list_init(&channel->response_headers);
    list_init(&channel->request_headers);

    nsIURL_AddRef(&uri->nsIURL_iface);
    channel->uri = uri;

    hres = IUri_GetScheme(uri->uri, &channel->url_scheme);
    if(FAILED(hres))
        channel->url_scheme = URL_SCHEME_UNKNOWN;

    *ret = channel;
    return NS_OK;
}

HRESULT create_redirect_nschannel(const WCHAR *url, nsChannel *orig_channel, nsChannel **ret)
{
    HTMLWindow *window = NULL;
    nsChannel *channel;
    nsWineURI *uri;
    nsresult nsres;
    HRESULT hres;

    if(orig_channel->uri->window_ref)
        window = orig_channel->uri->window_ref->window;
    nsres = create_uri(NULL, window, NULL, &uri);
    if(NS_FAILED(nsres))
        return E_FAIL;

    hres = CreateUri(url, 0, 0, &uri->uri);
    if(SUCCEEDED(hres))
        nsres = create_nschannel(uri, &channel);
    sync_wine_url(uri);
    nsIURL_Release(&uri->nsIURL_iface);
    if(FAILED(hres))
        return hres;
    if(NS_FAILED(nsres))
        return E_FAIL;

    if(orig_channel->load_group) {
        nsILoadGroup_AddRef(orig_channel->load_group);
        channel->load_group = orig_channel->load_group;
    }

    if(orig_channel->notif_callback) {
        nsIInterfaceRequestor_AddRef(orig_channel->notif_callback);
        channel->notif_callback = orig_channel->notif_callback;
    }

    channel->load_flags = orig_channel->load_flags | LOAD_REPLACE;

    if(orig_channel->request_method == METHOD_POST)
        FIXME("unsupported POST method\n");

    if(orig_channel->original_uri) {
        nsIURI_AddRef(orig_channel->original_uri);
        channel->original_uri = orig_channel->original_uri;
    }

    if(orig_channel->referrer) {
        nsIURI_AddRef(orig_channel->referrer);
        channel->referrer = orig_channel->referrer;
    }

    *ret = channel;
    return S_OK;
}

typedef struct {
    nsIProtocolHandler nsIProtocolHandler_iface;

    LONG ref;

    nsIProtocolHandler *nshandler;
} nsProtocolHandler;

static inline nsProtocolHandler *impl_from_nsIProtocolHandler(nsIProtocolHandler *iface)
{
    return CONTAINING_RECORD(iface, nsProtocolHandler, nsIProtocolHandler_iface);
}

static nsresult NSAPI nsProtocolHandler_QueryInterface(nsIProtocolHandler *iface, nsIIDRef riid,
        void **result)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result = &This->nsIProtocolHandler_iface;
    }else if(IsEqualGUID(&IID_nsIProtocolHandler, riid)) {
        TRACE("(%p)->(IID_nsIProtocolHandler %p)\n", This, result);
        *result = &This->nsIProtocolHandler_iface;
    }else if(IsEqualGUID(&IID_nsIExternalProtocolHandler, riid)) {
        TRACE("(%p)->(IID_nsIExternalProtocolHandler %p), returning NULL\n", This, result);
        return NS_NOINTERFACE;
    }

    if(*result) {
        nsISupports_AddRef((nsISupports*)*result);
        return NS_OK;
    }

    WARN("(%s %p)\n", debugstr_guid(riid), result);
    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsProtocolHandler_AddRef(nsIProtocolHandler *iface)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsProtocolHandler_Release(nsIProtocolHandler *iface)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        if(This->nshandler)
            nsIProtocolHandler_Release(This->nshandler);
        heap_free(This);
    }

    return ref;
}

static nsresult NSAPI nsProtocolHandler_GetScheme(nsIProtocolHandler *iface, nsACString *aScheme)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    TRACE("(%p)->(%p)\n", This, aScheme);

    if(This->nshandler)
        return nsIProtocolHandler_GetScheme(This->nshandler, aScheme);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsProtocolHandler_GetDefaultPort(nsIProtocolHandler *iface,
        PRInt32 *aDefaultPort)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    TRACE("(%p)->(%p)\n", This, aDefaultPort);

    if(This->nshandler)
        return nsIProtocolHandler_GetDefaultPort(This->nshandler, aDefaultPort);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsProtocolHandler_GetProtocolFlags(nsIProtocolHandler *iface,
                                                         PRUint32 *aProtocolFlags)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    TRACE("(%p)->(%p)\n", This, aProtocolFlags);

    if(This->nshandler)
        return nsIProtocolHandler_GetProtocolFlags(This->nshandler, aProtocolFlags);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsProtocolHandler_NewURI(nsIProtocolHandler *iface,
        const nsACString *aSpec, const char *aOriginCharset, nsIURI *aBaseURI, nsIURI **_retval)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    TRACE("((%p)->%s %s %p %p)\n", This, debugstr_nsacstr(aSpec), debugstr_a(aOriginCharset),
          aBaseURI, _retval);

    if(This->nshandler)
        return nsIProtocolHandler_NewURI(This->nshandler, aSpec, aOriginCharset, aBaseURI, _retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsProtocolHandler_NewChannel(nsIProtocolHandler *iface,
        nsIURI *aURI, nsIChannel **_retval)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    TRACE("(%p)->(%p %p)\n", This, aURI, _retval);

    if(This->nshandler)
        return nsIProtocolHandler_NewChannel(This->nshandler, aURI, _retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsProtocolHandler_AllowPort(nsIProtocolHandler *iface,
        PRInt32 port, const char *scheme, PRBool *_retval)
{
    nsProtocolHandler *This = impl_from_nsIProtocolHandler(iface);

    TRACE("(%p)->(%d %s %p)\n", This, port, debugstr_a(scheme), _retval);

    if(This->nshandler)
        return nsIProtocolHandler_AllowPort(This->nshandler, port, scheme, _retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

static const nsIProtocolHandlerVtbl nsProtocolHandlerVtbl = {
    nsProtocolHandler_QueryInterface,
    nsProtocolHandler_AddRef,
    nsProtocolHandler_Release,
    nsProtocolHandler_GetScheme,
    nsProtocolHandler_GetDefaultPort,
    nsProtocolHandler_GetProtocolFlags,
    nsProtocolHandler_NewURI,
    nsProtocolHandler_NewChannel,
    nsProtocolHandler_AllowPort
};

static nsIProtocolHandler *create_protocol_handler(nsIProtocolHandler *nshandler)
{
    nsProtocolHandler *ret = heap_alloc(sizeof(nsProtocolHandler));

    ret->nsIProtocolHandler_iface.lpVtbl = &nsProtocolHandlerVtbl;
    ret->ref = 1;
    ret->nshandler = nshandler;

    return &ret->nsIProtocolHandler_iface;
}

static nsresult NSAPI nsIOService_QueryInterface(nsIIOService*,nsIIDRef,void**);

static nsrefcnt NSAPI nsIOService_AddRef(nsIIOService *iface)
{
    return 2;
}

static nsrefcnt NSAPI nsIOService_Release(nsIIOService *iface)
{
    return 1;
}

static nsresult NSAPI nsIOService_GetProtocolHandler(nsIIOService *iface, const char *aScheme,
                                                     nsIProtocolHandler **_retval)
{
    nsIExternalProtocolHandler *nsexthandler;
    nsIProtocolHandler *nshandler;
    nsresult nsres;

    TRACE("(%s %p)\n", debugstr_a(aScheme), _retval);

    nsres = nsIIOService_GetProtocolHandler(nsio, aScheme, &nshandler);
    if(NS_FAILED(nsres)) {
        WARN("GetProtocolHandler failed: %08x\n", nsres);
        return nsres;
    }

    nsres = nsIProtocolHandler_QueryInterface(nshandler, &IID_nsIExternalProtocolHandler,
                                              (void**)&nsexthandler);
    if(NS_FAILED(nsres)) {
        *_retval = nshandler;
        return NS_OK;
    }

    nsIExternalProtocolHandler_Release(nsexthandler);
    *_retval = create_protocol_handler(nshandler);
    TRACE("return %p\n", *_retval);
    return NS_OK;
}

static nsresult NSAPI nsIOService_GetProtocolFlags(nsIIOService *iface, const char *aScheme,
                                                    PRUint32 *_retval)
{
    TRACE("(%s %p)\n", debugstr_a(aScheme), _retval);
    return nsIIOService_GetProtocolFlags(nsio, aScheme, _retval);
}

static BOOL is_gecko_special_uri(const char *spec)
{
    static const char *special_schemes[] = {"chrome:", "jar:", "moz-safe-about", "resource:", "javascript:", "wyciwyg:"};
    int i;

    for(i=0; i < sizeof(special_schemes)/sizeof(*special_schemes); i++) {
        if(!strncasecmp(spec, special_schemes[i], strlen(special_schemes[i])))
            return TRUE;
    }

    if(!strncasecmp(spec, "file:", 5)) {
        const char *ptr = spec+5;
        while(*ptr == '/')
            ptr++;
        return is_gecko_path(ptr);
    }

    return FALSE;
}

static nsresult NSAPI nsIOService_NewURI(nsIIOService *iface, const nsACString *aSpec,
        const char *aOriginCharset, nsIURI *aBaseURI, nsIURI **_retval)
{
    nsWineURI *wine_uri, *base_wine_uri = NULL;
    WCHAR new_spec[INTERNET_MAX_URL_LENGTH];
    HTMLWindow *window = NULL;
    const char *spec = NULL;
    nsIURI *uri = NULL;
    IUri *urlmon_uri;
    nsresult nsres;
    HRESULT hres;

    TRACE("(%s %s %p %p)\n", debugstr_nsacstr(aSpec), debugstr_a(aOriginCharset),
          aBaseURI, _retval);

    nsACString_GetData(aSpec, &spec);
    if(is_gecko_special_uri(spec))
        return nsIIOService_NewURI(nsio, aSpec, aOriginCharset, aBaseURI, _retval);

    if(!strncmp(spec, "wine:", 5))
        spec += 5;

    if(aBaseURI) {
        nsres = nsIURI_QueryInterface(aBaseURI, &IID_nsWineURI, (void**)&base_wine_uri);
        if(NS_SUCCEEDED(nsres)) {
            if(!ensure_uri(base_wine_uri))
                return NS_ERROR_UNEXPECTED;
            if(base_wine_uri->window_ref)
                window = base_wine_uri->window_ref->window;
        }else {
            WARN("Could not get base nsWineURI: %08x\n", nsres);
        }
    }

    MultiByteToWideChar(CP_ACP, 0, spec, -1, new_spec, sizeof(new_spec)/sizeof(WCHAR));

    if(base_wine_uri) {
        hres = CoInternetCombineUrlEx(base_wine_uri->uri, new_spec, URL_ESCAPE_SPACES_ONLY|URL_DONT_ESCAPE_EXTRA_INFO,
                &urlmon_uri, 0);
        if(FAILED(hres))
            WARN("CoInternetCombineUrlEx failed: %08x\n", hres);
    }else {
        hres = CreateUri(new_spec, 0, 0, &urlmon_uri);
        if(FAILED(hres))
            WARN("CreateUri failed: %08x\n", hres);
    }

    nsres = nsIIOService_NewURI(nsio, aSpec, aOriginCharset, aBaseURI, &uri);
    if(NS_FAILED(nsres))
        TRACE("NewURI failed: %08x\n", nsres);

    if(FAILED(hres)) {
        *_retval = uri;
        return nsres;
    }

    nsres = create_uri(uri, window, NULL, &wine_uri);
    if(base_wine_uri)
        nsIURI_Release(&base_wine_uri->nsIURL_iface);
    if(NS_FAILED(nsres))
        return nsres;

    wine_uri->uri = urlmon_uri;

    sync_wine_url(wine_uri);
    *_retval = (nsIURI*)wine_uri;
    return nsres;
}

static nsresult NSAPI nsIOService_NewFileURI(nsIIOService *iface, nsIFile *aFile,
                                             nsIURI **_retval)
{
    TRACE("(%p %p)\n", aFile, _retval);
    return nsIIOService_NewFileURI(nsio, aFile, _retval);
}

static nsresult NSAPI nsIOService_NewChannelFromURI(nsIIOService *iface, nsIURI *aURI,
                                                     nsIChannel **_retval)
{
    nsWineURI *wine_uri;
    nsChannel *ret;
    nsresult nsres;

    TRACE("(%p %p)\n", aURI, _retval);

    nsres = nsIURI_QueryInterface(aURI, &IID_nsWineURI, (void**)&wine_uri);
    if(NS_FAILED(nsres)) {
        TRACE("Could not get nsWineURI: %08x\n", nsres);
        return nsIIOService_NewChannelFromURI(nsio, aURI, _retval);
    }

    nsres = create_nschannel(wine_uri, &ret);
    nsIURL_Release(&wine_uri->nsIURL_iface);
    if(NS_FAILED(nsres))
        return nsres;

    nsIURI_AddRef(aURI);
    ret->original_uri = aURI;

    *_retval = (nsIChannel*)&ret->nsIHttpChannel_iface;
    return NS_OK;
}

static nsresult NSAPI nsIOService_NewChannel(nsIIOService *iface, const nsACString *aSpec,
        const char *aOriginCharset, nsIURI *aBaseURI, nsIChannel **_retval)
{
    TRACE("(%s %s %p %p)\n", debugstr_nsacstr(aSpec), debugstr_a(aOriginCharset), aBaseURI, _retval);
    return nsIIOService_NewChannel(nsio, aSpec, aOriginCharset, aBaseURI, _retval);
}

static nsresult NSAPI nsIOService_GetOffline(nsIIOService *iface, PRBool *aOffline)
{
    TRACE("(%p)\n", aOffline);
    return nsIIOService_GetOffline(nsio, aOffline);
}

static nsresult NSAPI nsIOService_SetOffline(nsIIOService *iface, PRBool aOffline)
{
    TRACE("(%x)\n", aOffline);
    return nsIIOService_SetOffline(nsio, aOffline);
}

static nsresult NSAPI nsIOService_AllowPort(nsIIOService *iface, PRInt32 aPort,
                                             const char *aScheme, PRBool *_retval)
{
    TRACE("(%d %s %p)\n", aPort, debugstr_a(aScheme), _retval);
    return nsIIOService_AllowPort(nsio, aPort, debugstr_a(aScheme), _retval);
}

static nsresult NSAPI nsIOService_ExtractScheme(nsIIOService *iface, const nsACString *urlString,
                                                 nsACString * _retval)
{
    TRACE("(%s %p)\n", debugstr_nsacstr(urlString), _retval);
    return nsIIOService_ExtractScheme(nsio, urlString, _retval);
}

static const nsIIOServiceVtbl nsIOServiceVtbl = {
    nsIOService_QueryInterface,
    nsIOService_AddRef,
    nsIOService_Release,
    nsIOService_GetProtocolHandler,
    nsIOService_GetProtocolFlags,
    nsIOService_NewURI,
    nsIOService_NewFileURI,
    nsIOService_NewChannelFromURI,
    nsIOService_NewChannel,
    nsIOService_GetOffline,
    nsIOService_SetOffline,
    nsIOService_AllowPort,
    nsIOService_ExtractScheme
};

static nsIIOService nsIOService = { &nsIOServiceVtbl };

static nsresult NSAPI nsNetUtil_QueryInterface(nsINetUtil *iface, nsIIDRef riid,
        void **result)
{
    return nsIIOService_QueryInterface(&nsIOService, riid, result);
}

static nsrefcnt NSAPI nsNetUtil_AddRef(nsINetUtil *iface)
{
    return 2;
}

static nsrefcnt NSAPI nsNetUtil_Release(nsINetUtil *iface)
{
    return 1;
}

static nsresult NSAPI nsNetUtil_ParseContentType(nsINetUtil *iface, const nsACString *aTypeHeader,
        nsACString *aCharset, PRBool *aHadCharset, nsACString *aContentType)
{
    TRACE("(%s %p %p %p)\n", debugstr_nsacstr(aTypeHeader), aCharset, aHadCharset, aContentType);

    return nsINetUtil_ParseContentType(net_util, aTypeHeader, aCharset, aHadCharset, aContentType);
}

static nsresult NSAPI nsNetUtil_ProtocolHasFlags(nsINetUtil *iface, nsIURI *aURI, PRUint32 aFlags, PRBool *_retval)
{
    TRACE("()\n");

    return nsINetUtil_ProtocolHasFlags(net_util, aURI, aFlags, _retval);
}

static nsresult NSAPI nsNetUtil_URIChainHasFlags(nsINetUtil *iface, nsIURI *aURI, PRUint32 aFlags, PRBool *_retval)
{
    TRACE("(%p %08x %p)\n", aURI, aFlags, _retval);

    if(aFlags == (1<<11)) {
        *_retval = FALSE;
        return NS_OK;
    }

    return nsINetUtil_URIChainHasFlags(net_util, aURI, aFlags, _retval);
}

static nsresult NSAPI nsNetUtil_ToImmutableURI(nsINetUtil *iface, nsIURI *aURI, nsIURI **_retval)
{
    TRACE("(%p %p)\n", aURI, _retval);

    return nsINetUtil_ToImmutableURI(net_util, aURI, _retval);
}

static nsresult NSAPI nsNetUtil_NewSimpleNestedURI(nsINetUtil *iface, nsIURI *aURI, nsIURI **_retval)
{
    TRACE("(%p %p)\n", aURI, _retval);

    return nsINetUtil_NewSimpleNestedURI(net_util, aURI, _retval);
}

static nsresult NSAPI nsNetUtil_EscapeString(nsINetUtil *iface, const nsACString *aString,
                                             PRUint32 aEscapeType, nsACString *_retval)
{
    TRACE("(%s %x %p)\n", debugstr_nsacstr(aString), aEscapeType, _retval);

    return nsINetUtil_EscapeString(net_util, aString, aEscapeType, _retval);
}

static nsresult NSAPI nsNetUtil_EscapeURL(nsINetUtil *iface, const nsACString *aStr, PRUint32 aFlags,
                                          nsACString *_retval)
{
    TRACE("(%s %08x %p)\n", debugstr_nsacstr(aStr), aFlags, _retval);

    return nsINetUtil_EscapeURL(net_util, aStr, aFlags, _retval);
}

static nsresult NSAPI nsNetUtil_UnescapeString(nsINetUtil *iface, const nsACString *aStr,
                                               PRUint32 aFlags, nsACString *_retval)
{
    TRACE("(%s %08x %p)\n", debugstr_nsacstr(aStr), aFlags, _retval);

    return nsINetUtil_UnescapeString(net_util, aStr, aFlags, _retval);
}

static nsresult NSAPI nsNetUtil_ExtractCharsetFromContentType(nsINetUtil *iface, const nsACString *aTypeHeader,
        nsACString *aCharset, PRInt32 *aCharsetStart, PRInt32 *aCharsetEnd, PRBool *_retval)
{
    TRACE("(%s %p %p %p %p)\n", debugstr_nsacstr(aTypeHeader), aCharset, aCharsetStart,
          aCharsetEnd, _retval);

    return nsINetUtil_ExtractCharsetFromContentType(net_util, aTypeHeader, aCharset, aCharsetStart, aCharsetEnd, _retval);
}

static const nsINetUtilVtbl nsNetUtilVtbl = {
    nsNetUtil_QueryInterface,
    nsNetUtil_AddRef,
    nsNetUtil_Release,
    nsNetUtil_ParseContentType,
    nsNetUtil_ProtocolHasFlags,
    nsNetUtil_URIChainHasFlags,
    nsNetUtil_ToImmutableURI,
    nsNetUtil_NewSimpleNestedURI,
    nsNetUtil_EscapeString,
    nsNetUtil_EscapeURL,
    nsNetUtil_UnescapeString,
    nsNetUtil_ExtractCharsetFromContentType
};

static nsINetUtil nsNetUtil = { &nsNetUtilVtbl };

static nsresult NSAPI nsIOService_QueryInterface(nsIIOService *iface, nsIIDRef riid,
        void **result)
{
    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid))
        *result = &nsIOService;
    else if(IsEqualGUID(&IID_nsIIOService, riid))
        *result = &nsIOService;
    else if(IsEqualGUID(&IID_nsINetUtil, riid))
        *result = &nsNetUtil;

    if(*result) {
        nsISupports_AddRef((nsISupports*)*result);
        return NS_OK;
    }

    FIXME("(%s %p)\n", debugstr_guid(riid), result);
    return NS_NOINTERFACE;
}

static nsresult NSAPI nsIOServiceFactory_QueryInterface(nsIFactory *iface, nsIIDRef riid,
        void **result)
{
    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(IID_nsISupports %p)\n", result);
        *result = iface;
    }else if(IsEqualGUID(&IID_nsIFactory, riid)) {
        TRACE("(IID_nsIFactory %p)\n", result);
        *result = iface;
    }

    if(*result) {
        nsIFactory_AddRef(iface);
        return NS_OK;
    }

    WARN("(%s %p)\n", debugstr_guid(riid), result);
    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsIOServiceFactory_AddRef(nsIFactory *iface)
{
    return 2;
}

static nsrefcnt NSAPI nsIOServiceFactory_Release(nsIFactory *iface)
{
    return 1;
}

static nsresult NSAPI nsIOServiceFactory_CreateInstance(nsIFactory *iface,
        nsISupports *aOuter, const nsIID *iid, void **result)
{
    return nsIIOService_QueryInterface(&nsIOService, iid, result);
}

static nsresult NSAPI nsIOServiceFactory_LockFactory(nsIFactory *iface, PRBool lock)
{
    WARN("(%x)\n", lock);
    return NS_OK;
}

static const nsIFactoryVtbl nsIOServiceFactoryVtbl = {
    nsIOServiceFactory_QueryInterface,
    nsIOServiceFactory_AddRef,
    nsIOServiceFactory_Release,
    nsIOServiceFactory_CreateInstance,
    nsIOServiceFactory_LockFactory
};

static nsIFactory nsIOServiceFactory = { &nsIOServiceFactoryVtbl };

static BOOL translate_url(HTMLDocumentObj *doc, nsWineURI *uri)
{
    OLECHAR *new_url = NULL;
    WCHAR *url;
    BOOL ret = FALSE;
    HRESULT hres;

    if(!doc->hostui || !ensure_uri(uri))
        return FALSE;

    hres = IUri_GetDisplayUri(uri->uri, &url);
    if(FAILED(hres))
        return FALSE;

    hres = IDocHostUIHandler_TranslateUrl(doc->hostui, 0, url, &new_url);
    if(hres == S_OK && new_url) {
        if(strcmpW(url, new_url)) {
            FIXME("TranslateUrl returned new URL %s -> %s\n", debugstr_w(url), debugstr_w(new_url));
            ret = TRUE;
        }
        CoTaskMemFree(new_url);
    }

    SysFreeString(url);
    return ret;
}

nsresult on_start_uri_open(NSContainer *nscontainer, nsIURI *uri, PRBool *_retval)
{
    nsWineURI *wine_uri;
    nsresult nsres;

    *_retval = FALSE;

    nsres = nsIURI_QueryInterface(uri, &IID_nsWineURI, (void**)&wine_uri);
    if(NS_FAILED(nsres)) {
        WARN("Could not get nsWineURI: %08x\n", nsres);
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    if(!wine_uri->is_doc_uri) {
        wine_uri->is_doc_uri = TRUE;

        if(!wine_uri->container) {
            nsIWebBrowserChrome_AddRef(&nscontainer->nsIWebBrowserChrome_iface);
            wine_uri->container = nscontainer;
        }

        if(nscontainer->doc)
            *_retval = translate_url(nscontainer->doc, wine_uri);
    }

    nsIURI_Release(&wine_uri->nsIURL_iface);
    return NS_OK;
}

void init_nsio(nsIComponentManager *component_manager, nsIComponentRegistrar *registrar)
{
    nsIFactory *old_factory = NULL;
    nsresult nsres;

    nsres = nsIComponentManager_GetClassObject(component_manager, &NS_IOSERVICE_CID,
                                               &IID_nsIFactory, (void**)&old_factory);
    if(NS_FAILED(nsres)) {
        ERR("Could not get factory: %08x\n", nsres);
        return;
    }

    nsres = nsIFactory_CreateInstance(old_factory, NULL, &IID_nsIIOService, (void**)&nsio);
    if(NS_FAILED(nsres)) {
        ERR("Couldn not create nsIOService instance %08x\n", nsres);
        nsIFactory_Release(old_factory);
        return;
    }

    nsres = nsIIOService_QueryInterface(nsio, &IID_nsINetUtil, (void**)&net_util);
    if(NS_FAILED(nsres)) {
        WARN("Could not get nsINetUtil interface: %08x\n", nsres);
        nsIIOService_Release(nsio);
        return;
    }

    nsres = nsIComponentRegistrar_UnregisterFactory(registrar, &NS_IOSERVICE_CID, old_factory);
    nsIFactory_Release(old_factory);
    if(NS_FAILED(nsres))
        ERR("UnregisterFactory failed: %08x\n", nsres);

    nsres = nsIComponentRegistrar_RegisterFactory(registrar, &NS_IOSERVICE_CID,
            NS_IOSERVICE_CLASSNAME, NS_IOSERVICE_CONTRACTID, &nsIOServiceFactory);
    if(NS_FAILED(nsres))
        ERR("RegisterFactory failed: %08x\n", nsres);
}

void release_nsio(void)
{
    if(net_util) {
        nsINetUtil_Release(net_util);
        net_util = NULL;
    }

    if(nsio) {
        nsIIOService_Release(nsio);
        nsio = NULL;
    }
}

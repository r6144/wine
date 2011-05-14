/*
 * Header includes for shdocvw.dll
 *
 * Copyright 2001 John R. Sheets (for CodeWeavers)
 * Copyright 2005-2006 Jacek Caban for CodeWeavers
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

#ifndef __WINE_SHDOCVW_H
#define __WINE_SHDOCVW_H

#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "ole2.h"
#include "olectl.h"
#include "shlobj.h"
#include "exdisp.h"
#include "mshtmhst.h"
#include "hlink.h"
#include "htiframe.h"

#include "wine/unicode.h"
#include "wine/list.h"
#include "resource.h"


#define WM_UPDATEADDRBAR    (WM_APP+1)

/**********************************************************************
 * Shell Instance Objects
 */
extern HRESULT SHDOCVW_GetShellInstanceObjectClassObject(REFCLSID rclsid, 
    REFIID riid, LPVOID *ppvClassObj);

/**********************************************************************
 * WebBrowser declaration for SHDOCVW.DLL
 */

typedef struct ConnectionPoint ConnectionPoint;
typedef struct DocHost DocHost;

typedef struct {
    IConnectionPointContainer IConnectionPointContainer_iface;

    ConnectionPoint *wbe2;
    ConnectionPoint *wbe;
    ConnectionPoint *pns;

    IUnknown *impl;
} ConnectionPointContainer;

typedef struct {
    IHlinkFrame    IHlinkFrame_iface;
    ITargetFrame2  ITargetFrame2_iface;

    IUnknown *outer;
    DocHost *doc_host;
} HlinkFrame;

struct _task_header_t;

typedef void (*task_proc_t)(DocHost*, struct _task_header_t*);

typedef struct _task_header_t {
    task_proc_t proc;
} task_header_t;

typedef struct _IDocHostContainerVtbl
{
    ULONG (*addref)(DocHost*);
    ULONG (*release)(DocHost*);
    void (WINAPI* GetDocObjRect)(DocHost*,RECT*);
    HRESULT (WINAPI* SetStatusText)(DocHost*,LPCWSTR);
    void (WINAPI* SetURL)(DocHost*,LPCWSTR);
    HRESULT (*exec)(DocHost*,const GUID*,DWORD,DWORD,VARIANT*,VARIANT*);
} IDocHostContainerVtbl;

struct DocHost {
    IOleClientSite      IOleClientSite_iface;
    IOleInPlaceSite     IOleInPlaceSite_iface;
    IDocHostUIHandler2  IDocHostUIHandler2_iface;
    IOleDocumentSite    IOleDocumentSite_iface;
    IOleControlSite     IOleControlSite_iface;
    IOleCommandTarget   IOleCommandTarget_iface;
    IDispatch           IDispatch_iface;
    IPropertyNotifySink IPropertyNotifySink_iface;
    IServiceProvider    IServiceProvider_iface;

    /* Interfaces of InPlaceFrame object */
    IOleInPlaceFrame  IOleInPlaceFrame_iface;

    IDispatch *disp;

    IDispatch *client_disp;
    IDocHostUIHandler *hostui;
    IOleInPlaceFrame *frame;

    IUnknown *document;
    IOleDocumentView *view;
    IUnknown *doc_navigate;

    const IDocHostContainerVtbl *container_vtbl;

    HWND hwnd;
    HWND frame_hwnd;

    LPOLESTR url;

    VARIANT_BOOL silent;
    VARIANT_BOOL offline;
    VARIANT_BOOL busy;

    READYSTATE ready_state;
    READYSTATE doc_state;
    DWORD prop_notif_cookie;
    BOOL is_prop_notif;

    ConnectionPointContainer cps;
};

struct WebBrowser {
    IWebBrowser2             IWebBrowser2_iface;
    IOleObject               IOleObject_iface;
    IOleInPlaceObject        IOleInPlaceObject_iface;
    IOleControl              IOleControl_iface;
    IPersistStorage          IPersistStorage_iface;
    IPersistMemory           IPersistMemory_iface;
    IPersistStreamInit       IPersistStreamInit_iface;
    IProvideClassInfo2       IProvideClassInfo2_iface;
    IViewObject2             IViewObject2_iface;
    IOleInPlaceActiveObject  IOleInPlaceActiveObject_iface;
    IOleCommandTarget        IOleCommandTarget_iface;
    IServiceProvider         IServiceProvider_iface;
    IDataObject              IDataObject_iface;
    HlinkFrame hlink_frame;

    LONG ref;

    INT version;

    IOleClientSite *client;
    IOleContainer *container;
    IOleInPlaceSite *inplace;

    /* window context */

    HWND frame_hwnd;
    IOleInPlaceUIWindow *uiwindow;
    RECT pos_rect;
    RECT clip_rect;
    OLEINPLACEFRAMEINFO frameinfo;
    SIZEL extent;

    HWND shell_embedding_hwnd;

    VARIANT_BOOL register_browser;
    VARIANT_BOOL visible;
    VARIANT_BOOL menu_bar;
    VARIANT_BOOL address_bar;
    VARIANT_BOOL status_bar;
    VARIANT_BOOL tool_bar;
    VARIANT_BOOL full_screen;
    VARIANT_BOOL theater_mode;

    DocHost doc_host;
};

typedef struct {
    DocHost doc_host;

    LONG ref;

    InternetExplorer *ie;
} IEDocHost;

struct InternetExplorer {
    IWebBrowser2 IWebBrowser2_iface;
    HlinkFrame hlink_frame;

    LONG ref;

    HWND frame_hwnd;
    HWND status_hwnd;
    HMENU menu;
    BOOL nohome;

    struct list entry;
    IEDocHost *doc_host;
};

void WebBrowser_OleObject_Init(WebBrowser*);
void WebBrowser_ViewObject_Init(WebBrowser*);
void WebBrowser_Persist_Init(WebBrowser*);
void WebBrowser_ClassInfo_Init(WebBrowser*);

void WebBrowser_OleObject_Destroy(WebBrowser*);

void DocHost_Init(DocHost*,IDispatch*,const IDocHostContainerVtbl*);
void DocHost_ClientSite_Init(DocHost*);
void DocHost_Frame_Init(DocHost*);
void release_dochost_client(DocHost*);

void DocHost_Release(DocHost*);
void DocHost_ClientSite_Release(DocHost*);

void ConnectionPointContainer_Init(ConnectionPointContainer*,IUnknown*);
void ConnectionPointContainer_Destroy(ConnectionPointContainer*);

void HlinkFrame_Init(HlinkFrame*,IUnknown*,DocHost*);
BOOL HlinkFrame_QI(HlinkFrame*,REFIID,void**);

HRESULT ShellBrowser_Create(IShellBrowser**);
HRESULT WebBrowserV1_Create(IUnknown*,REFIID,void**);
HRESULT WebBrowserV2_Create(IUnknown*,REFIID,void**);

void create_doc_view_hwnd(DocHost*);
void deactivate_document(DocHost*);
HRESULT dochost_object_available(DocHost*,IUnknown*);
void call_sink(ConnectionPoint*,DISPID,DISPPARAMS*);
HRESULT navigate_url(DocHost*,LPCWSTR,const VARIANT*,const VARIANT*,VARIANT*,VARIANT*);
HRESULT go_home(DocHost*);
void set_doc_state(DocHost*,READYSTATE);
HRESULT get_location_url(DocHost*,BSTR*);
void handle_navigation_error(DocHost*,HRESULT,BSTR,IHTMLWindow2*);

#define WM_DOCHOSTTASK (WM_USER+0x300)
void push_dochost_task(DocHost*,task_header_t*,task_proc_t,BOOL);
LRESULT  process_dochost_task(DocHost*,LPARAM);

HRESULT InternetExplorer_Create(IUnknown*,REFIID,void**);
void InternetExplorer_WebBrowser_Init(InternetExplorer*);

void released_obj(void);

HRESULT CUrlHistory_Create(IUnknown*,REFIID,void**);

HRESULT InternetShortcut_Create(IUnknown*,REFIID,void**);

HRESULT TaskbarList_Create(IUnknown*,REFIID,void**);

/**********************************************************************
 * Dll lifetime tracking declaration for shdocvw.dll
 */
extern LONG SHDOCVW_refCount;
static inline void SHDOCVW_LockModule(void) { InterlockedIncrement( &SHDOCVW_refCount ); }
static inline void SHDOCVW_UnlockModule(void) { InterlockedDecrement( &SHDOCVW_refCount ); }

extern HINSTANCE shdocvw_hinstance;
extern void register_iewindow_class(void);
extern void unregister_iewindow_class(void);
extern HRESULT update_ie_statustext(InternetExplorer*, LPCWSTR);

HRESULT register_class_object(BOOL);
HRESULT get_typeinfo(ITypeInfo**);
DWORD register_iexplore(BOOL);

const char *debugstr_variant(const VARIANT*);

/* memory allocation functions */

static inline void *heap_alloc(size_t len)
{
    return HeapAlloc(GetProcessHeap(), 0, len);
}

static inline void *heap_alloc_zero(size_t len)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}

static inline void *heap_realloc(void *mem, size_t len)
{
    return HeapReAlloc(GetProcessHeap(), 0, mem, len);
}

static inline BOOL heap_free(void *mem)
{
    return HeapFree(GetProcessHeap(), 0, mem);
}

static inline LPWSTR heap_strdupW(LPCWSTR str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD size;

        size = (strlenW(str)+1)*sizeof(WCHAR);
        ret = heap_alloc(size);
        memcpy(ret, str, size);
    }

    return ret;
}

static inline LPWSTR co_strdupW(LPCWSTR str)
{
    WCHAR *ret = CoTaskMemAlloc((strlenW(str) + 1)*sizeof(WCHAR));
    if (ret)
        lstrcpyW(ret, str);
    return ret;
}

static inline LPWSTR co_strdupAtoW(LPCSTR str)
{
    INT len;
    WCHAR *ret;
    len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    ret = CoTaskMemAlloc(len*sizeof(WCHAR));
    if (ret)
        MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    return ret;
}

static inline LPSTR co_strdupWtoA(LPCWSTR str)
{
    INT len;
    CHAR *ret;
    len = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, 0, 0);
    ret = CoTaskMemAlloc(len);
    if (ret)
        WideCharToMultiByte(CP_ACP, 0, str, -1, ret, len, 0, 0);
    return ret;
}

#endif /* __WINE_SHDOCVW_H */

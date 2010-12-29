/*
 *    XPath/XSLPattern query result node list implementation
 *
 * Copyright 2005 Mike McCormack
 * Copyright 2007 Mikolaj Zalewski
 * Copyright 2010 Adam Martinson for CodeWeavers
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
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "msxml6.h"

#include "msxml_private.h"

#include "wine/debug.h"

/* This file implements the object returned by a XPath query. Note that this is
 * not the IXMLDOMNodeList returned by childNodes - it's implemented in nodelist.c.
 * They are different because the list returned by XPath queries:
 *  - is static - gives the results for the XML tree as it existed during the
 *    execution of the query
 *  - supports IXMLDOMSelection (TODO)
 *
 */

WINE_DEFAULT_DEBUG_CHANNEL(msxml);

#ifdef HAVE_LIBXML2

#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

int registerNamespaces(xmlXPathContextPtr ctxt);
xmlChar* XSLPattern_to_XPath(xmlXPathContextPtr ctxt, xmlChar const* xslpat_str);

typedef struct _queryresult
{
    DispatchEx dispex;
    const struct IXMLDOMNodeListVtbl *lpVtbl;
    LONG ref;
    xmlNodePtr node;
    xmlXPathObjectPtr result;
    int resultPos;
} queryresult;

static inline queryresult *impl_from_IXMLDOMNodeList( IXMLDOMNodeList *iface )
{
    return (queryresult *)((char*)iface - FIELD_OFFSET(queryresult, lpVtbl));
}

#define XMLQUERYRES(x)  ((IXMLDOMNodeList*)&(x)->lpVtbl)

static HRESULT WINAPI queryresult_QueryInterface(
    IXMLDOMNodeList *iface,
    REFIID riid,
    void** ppvObject )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );

    TRACE("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppvObject);

    if(!ppvObject)
        return E_INVALIDARG;

    if ( IsEqualGUID( riid, &IID_IUnknown ) ||
         IsEqualGUID( riid, &IID_IXMLDOMNodeList ) )
    {
        *ppvObject = iface;
    }
    else if(dispex_query_interface(&This->dispex, riid, ppvObject))
    {
        return *ppvObject ? S_OK : E_NOINTERFACE;
    }
    else
    {
        FIXME("interface %s not implemented\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IXMLDOMNodeList_AddRef( iface );

    return S_OK;
}

static ULONG WINAPI queryresult_AddRef(
    IXMLDOMNodeList *iface )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );
    return InterlockedIncrement( &This->ref );
}

static ULONG WINAPI queryresult_Release(
    IXMLDOMNodeList *iface )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );
    ULONG ref;

    ref = InterlockedDecrement(&This->ref);
    if ( ref == 0 )
    {
        xmlXPathFreeObject(This->result);
        xmldoc_release(This->node->doc);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI queryresult_GetTypeInfoCount(
    IXMLDOMNodeList *iface,
    UINT* pctinfo )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;

    return S_OK;
}

static HRESULT WINAPI queryresult_GetTypeInfo(
    IXMLDOMNodeList *iface,
    UINT iTInfo,
    LCID lcid,
    ITypeInfo** ppTInfo )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );
    HRESULT hr;

    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);

    hr = get_typeinfo(IXMLDOMNodeList_tid, ppTInfo);

    return hr;
}

static HRESULT WINAPI queryresult_GetIDsOfNames(
    IXMLDOMNodeList *iface,
    REFIID riid,
    LPOLESTR* rgszNames,
    UINT cNames,
    LCID lcid,
    DISPID* rgDispId )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);

    if(!rgszNames || cNames == 0 || !rgDispId)
        return E_INVALIDARG;

    hr = get_typeinfo(IXMLDOMNodeList_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI queryresult_Invoke(
    IXMLDOMNodeList *iface,
    DISPID dispIdMember,
    REFIID riid,
    LCID lcid,
    WORD wFlags,
    DISPPARAMS* pDispParams,
    VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo,
    UINT* puArgErr )
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IXMLDOMNodeList_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, &(This->lpVtbl), dispIdMember, wFlags, pDispParams,
                pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI queryresult_get_item(
        IXMLDOMNodeList* iface,
        LONG index,
        IXMLDOMNode** listItem)
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );

    TRACE("(%p)->(%d %p)\n", This, index, listItem);

    if(!listItem)
        return E_INVALIDARG;

    *listItem = NULL;

    if (index < 0 || index >= xmlXPathNodeSetGetLength(This->result->nodesetval))
        return S_FALSE;

    *listItem = create_node(xmlXPathNodeSetItem(This->result->nodesetval, index));
    This->resultPos = index + 1;

    return S_OK;
}

static HRESULT WINAPI queryresult_get_length(
        IXMLDOMNodeList* iface,
        LONG* listLength)
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );

    TRACE("(%p)->(%p)\n", This, listLength);

    if(!listLength)
        return E_INVALIDARG;

    *listLength = xmlXPathNodeSetGetLength(This->result->nodesetval);
    return S_OK;
}

static HRESULT WINAPI queryresult_nextNode(
        IXMLDOMNodeList* iface,
        IXMLDOMNode** nextItem)
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );

    TRACE("(%p)->(%p)\n", This, nextItem );

    if(!nextItem)
        return E_INVALIDARG;

    *nextItem = NULL;

    if (This->resultPos >= xmlXPathNodeSetGetLength(This->result->nodesetval))
        return S_FALSE;

    *nextItem = create_node(xmlXPathNodeSetItem(This->result->nodesetval, This->resultPos));
    This->resultPos++;
    return S_OK;
}

static HRESULT WINAPI queryresult_reset(
        IXMLDOMNodeList* iface)
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );

    TRACE("%p\n", This);
    This->resultPos = 0;
    return S_OK;
}

static HRESULT WINAPI queryresult__newEnum(
        IXMLDOMNodeList* iface,
        IUnknown** ppUnk)
{
    queryresult *This = impl_from_IXMLDOMNodeList( iface );
    FIXME("(%p)->(%p)\n", This, ppUnk);
    return E_NOTIMPL;
}


static const struct IXMLDOMNodeListVtbl queryresult_vtbl =
{
    queryresult_QueryInterface,
    queryresult_AddRef,
    queryresult_Release,
    queryresult_GetTypeInfoCount,
    queryresult_GetTypeInfo,
    queryresult_GetIDsOfNames,
    queryresult_Invoke,
    queryresult_get_item,
    queryresult_get_length,
    queryresult_nextNode,
    queryresult_reset,
    queryresult__newEnum,
};

static HRESULT queryresult_get_dispid(IUnknown *iface, BSTR name, DWORD flags, DISPID *dispid)
{
    queryresult *This = impl_from_IXMLDOMNodeList( (IXMLDOMNodeList*)iface );
    WCHAR *ptr;
    int idx = 0;

    for(ptr = name; *ptr && isdigitW(*ptr); ptr++)
        idx = idx*10 + (*ptr-'0');
    if(*ptr)
        return DISP_E_UNKNOWNNAME;

    if(idx >= xmlXPathNodeSetGetLength(This->result->nodesetval))
        return DISP_E_UNKNOWNNAME;

    *dispid = MSXML_DISPID_CUSTOM_MIN + idx;
    TRACE("ret %x\n", *dispid);
    return S_OK;
}

static HRESULT queryresult_invoke(IUnknown *iface, DISPID id, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei)
{
    queryresult *This = impl_from_IXMLDOMNodeList( (IXMLDOMNodeList*)iface );

    TRACE("(%p)->(%x %x %x %p %p %p)\n", This, id, lcid, flags, params, res, ei);

    V_VT(res) = VT_DISPATCH;
    V_DISPATCH(res) = NULL;

    switch(flags)
    {
        case INVOKE_PROPERTYGET:
        {
            IXMLDOMNode *disp = NULL;

            queryresult_get_item(XMLQUERYRES(This), id - MSXML_DISPID_CUSTOM_MIN, &disp);
            V_DISPATCH(res) = (IDispatch*)disp;
            break;
        }
        default:
        {
            FIXME("unimplemented flags %x\n", flags);
            break;
        }
    }

    TRACE("ret %p\n", V_DISPATCH(res));

    return S_OK;
}

static const dispex_static_data_vtbl_t queryresult_dispex_vtbl = {
    queryresult_get_dispid,
    queryresult_invoke
};

static const tid_t queryresult_iface_tids[] = {
    IXMLDOMNodeList_tid,
    0
};
static dispex_static_data_t queryresult_dispex = {
    &queryresult_dispex_vtbl,
    IXMLDOMSelection_tid,
    NULL,
    queryresult_iface_tids
};

#define XSLPATTERN_CHECK_ARGS(n) \
    if (nargs != n) { \
        FIXME("XSLPattern syntax error: Expected %i arguments, got %i\n", n, nargs); \
        xmlXPathSetArityError(pctx); \
        return; \
    }


void XSLPattern_index(xmlXPathParserContextPtr pctx, int nargs)
{
    XSLPATTERN_CHECK_ARGS(0);

    xmlXPathPositionFunction(pctx, 0);
    xmlXPathReturnNumber(pctx, xmlXPathPopNumber(pctx) - 1.0);
}

void XSLPattern_end(xmlXPathParserContextPtr pctx, int nargs)
{
    double pos, last;
    XSLPATTERN_CHECK_ARGS(0);

    xmlXPathPositionFunction(pctx, 0);
    pos = xmlXPathPopNumber(pctx);
    xmlXPathLastFunction(pctx, 0);
    last = xmlXPathPopNumber(pctx);
    xmlXPathReturnBoolean(pctx, pos == last);
}

void XSLPattern_nodeType(xmlXPathParserContextPtr pctx, int nargs)
{
    XSLPATTERN_CHECK_ARGS(0);
    xmlXPathReturnNumber(pctx, pctx->context->node->type);
}

void XSLPattern_OP_IEq(xmlXPathParserContextPtr pctx, int nargs)
{
    xmlChar *arg1, *arg2;
    XSLPATTERN_CHECK_ARGS(2);

    arg2 = xmlXPathPopString(pctx);
    arg1 = xmlXPathPopString(pctx);
    xmlXPathReturnBoolean(pctx, xmlStrcasecmp(arg1, arg2) == 0);
    xmlFree(arg1);
    xmlFree(arg2);
}

void XSLPattern_OP_INEq(xmlXPathParserContextPtr pctx, int nargs)
{
    xmlChar *arg1, *arg2;
    XSLPATTERN_CHECK_ARGS(2);

    arg2 = xmlXPathPopString(pctx);
    arg1 = xmlXPathPopString(pctx);
    xmlXPathReturnBoolean(pctx, xmlStrcasecmp(arg1, arg2) != 0);
    xmlFree(arg1);
    xmlFree(arg2);
}

void XSLPattern_OP_ILt(xmlXPathParserContextPtr pctx, int nargs)
{
    xmlChar *arg1, *arg2;
    XSLPATTERN_CHECK_ARGS(2);

    arg2 = xmlXPathPopString(pctx);
    arg1 = xmlXPathPopString(pctx);
    xmlXPathReturnBoolean(pctx, xmlStrcasecmp(arg1, arg2) < 0);
    xmlFree(arg1);
    xmlFree(arg2);
}

void XSLPattern_OP_ILEq(xmlXPathParserContextPtr pctx, int nargs)
{
    xmlChar *arg1, *arg2;
    XSLPATTERN_CHECK_ARGS(2);

    arg2 = xmlXPathPopString(pctx);
    arg1 = xmlXPathPopString(pctx);
    xmlXPathReturnBoolean(pctx, xmlStrcasecmp(arg1, arg2) <= 0);
    xmlFree(arg1);
    xmlFree(arg2);
}

void XSLPattern_OP_IGt(xmlXPathParserContextPtr pctx, int nargs)
{
    xmlChar *arg1, *arg2;
    XSLPATTERN_CHECK_ARGS(2);

    arg2 = xmlXPathPopString(pctx);
    arg1 = xmlXPathPopString(pctx);
    xmlXPathReturnBoolean(pctx, xmlStrcasecmp(arg1, arg2) > 0);
    xmlFree(arg1);
    xmlFree(arg2);
}

void XSLPattern_OP_IGEq(xmlXPathParserContextPtr pctx, int nargs)
{
    xmlChar *arg1, *arg2;
    XSLPATTERN_CHECK_ARGS(2);

    arg2 = xmlXPathPopString(pctx);
    arg1 = xmlXPathPopString(pctx);
    xmlXPathReturnBoolean(pctx, xmlStrcasecmp(arg1, arg2) >= 0);
    xmlFree(arg1);
    xmlFree(arg2);
}

static void query_serror(void* ctx, xmlErrorPtr err)
{
    LIBXML2_CALLBACK_SERROR(queryresult_create, err);
}

HRESULT queryresult_create(xmlNodePtr node, xmlChar* szQuery, IXMLDOMNodeList **out)
{
    queryresult *This = heap_alloc_zero(sizeof(queryresult));
    xmlXPathContextPtr ctxt = xmlXPathNewContext(node->doc);
    HRESULT hr;

    TRACE("(%p, %s, %p)\n", node, wine_dbgstr_a((char const*)szQuery), out);

    *out = NULL;
    if (This == NULL || ctxt == NULL || szQuery == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }

    This->lpVtbl = &queryresult_vtbl;
    This->ref = 1;
    This->resultPos = 0;
    This->node = node;
    xmldoc_add_ref(This->node->doc);

    ctxt->error = query_serror;
    ctxt->node = node;
    registerNamespaces(ctxt);

    if (is_xpathmode(This->node->doc))
    {
        xmlXPathRegisterAllFunctions(ctxt);
        This->result = xmlXPathEvalExpression(szQuery, ctxt);
    }
    else
    {
        xmlChar* xslpQuery = XSLPattern_to_XPath(ctxt, szQuery);

        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"not", xmlXPathNotFunction);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"boolean", xmlXPathBooleanFunction);

        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"index", XSLPattern_index);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"end", XSLPattern_end);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"nodeType", XSLPattern_nodeType);

        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"OP_IEq", XSLPattern_OP_IEq);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"OP_INEq", XSLPattern_OP_INEq);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"OP_ILt", XSLPattern_OP_ILt);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"OP_ILEq", XSLPattern_OP_ILEq);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"OP_IGt", XSLPattern_OP_IGt);
        xmlXPathRegisterFunc(ctxt, (xmlChar const*)"OP_IGEq", XSLPattern_OP_IGEq);

        This->result = xmlXPathEvalExpression(xslpQuery, ctxt);
        xmlFree(xslpQuery);
    }

    if (!This->result || This->result->type != XPATH_NODESET)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    init_dispex(&This->dispex, (IUnknown*)&This->lpVtbl, &queryresult_dispex);

    *out = (IXMLDOMNodeList *) &This->lpVtbl;
    hr = S_OK;
    TRACE("found %d matches\n", xmlXPathNodeSetGetLength(This->result->nodesetval));

cleanup:
    if (This != NULL && FAILED(hr))
        IXMLDOMNodeList_Release( (IXMLDOMNodeList*) &This->lpVtbl );
    xmlXPathFreeContext(ctxt);
    return hr;
}

#endif

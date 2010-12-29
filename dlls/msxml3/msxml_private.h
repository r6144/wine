/*
 *    Common definitions
 *
 * Copyright 2005 Mike McCormack
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

#ifndef __MSXML_PRIVATE__
#define __MSXML_PRIVATE__

#include "dispex.h"

#include "wine/unicode.h"

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

typedef enum {
    MSXML_DEFAULT = 0,
    MSXML3 = 30,
    MSXML4 = 40,
    MSXML6 = 60
} MSXML_VERSION;

/* typelibs */
typedef enum tid_t {
    IXMLDOMAttribute_tid,
    IXMLDOMCDATASection_tid,
    IXMLDOMComment_tid,
    IXMLDOMDocument_tid,
    IXMLDOMDocument2_tid,
    IXMLDOMDocumentFragment_tid,
    IXMLDOMDocumentType_tid,
    IXMLDOMElement_tid,
    IXMLDOMEntityReference_tid,
    IXMLDOMImplementation_tid,
    IXMLDOMNamedNodeMap_tid,
    IXMLDOMNode_tid,
    IXMLDOMNodeList_tid,
    IXMLDOMParseError_tid,
    IXMLDOMProcessingInstruction_tid,
    IXMLDOMSchemaCollection_tid,
    IXMLDOMSelection_tid,
    IXMLDOMText_tid,
    IXMLElement_tid,
    IXMLDocument_tid,
    IXMLHTTPRequest_tid,
    IVBSAXAttributes_tid,
    IVBSAXContentHandler_tid,
    IVBSAXDeclHandler_tid,
    IVBSAXDTDHandler_tid,
    IVBSAXEntityResolver_tid,
    IVBSAXErrorHandler_tid,
    IVBSAXLexicalHandler_tid,
    IVBSAXLocator_tid,
    IVBSAXXMLFilter_tid,
    IVBSAXXMLReader_tid,
    IMXAttributes_tid,
    IMXReaderControl_tid,
    IMXWriter_tid,
    LAST_tid
} tid_t;

/* The XDR datatypes (urn:schemas-microsoft-com:datatypes)
 * These are actually valid for XSD schemas as well
 * See datatypes.xsd
 */
typedef enum _XDR_DT {
    DT_INVALID = -1,
    DT_BIN_BASE64,
    DT_BIN_HEX,
    DT_BOOLEAN,
    DT_CHAR,
    DT_DATE,
    DT_DATE_TZ,
    DT_DATETIME,
    DT_DATETIME_TZ,
    DT_ENTITY,
    DT_ENTITIES,
    DT_ENUMERATION,
    DT_FIXED_14_4,
    DT_FLOAT,
    DT_I1,
    DT_I2,
    DT_I4,
    DT_I8,
    DT_ID,
    DT_IDREF,
    DT_IDREFS,
    DT_INT,
    DT_NMTOKEN,
    DT_NMTOKENS,
    DT_NOTATION,
    DT_NUMBER,
    DT_R4,
    DT_R8,
    DT_STRING,
    DT_TIME,
    DT_TIME_TZ,
    DT_UI1,
    DT_UI2,
    DT_UI4,
    DT_UI8,
    DT_URI,
    DT_UUID
} XDR_DT;
#define DT__N_TYPES  (DT_UUID+1)

extern HRESULT get_typeinfo(tid_t tid, ITypeInfo **typeinfo);
extern void release_typelib(void);

typedef struct dispex_data_t dispex_data_t;
typedef struct dispex_dynamic_data_t dispex_dynamic_data_t;

#define MSXML_DISPID_CUSTOM_MIN 0x60000000
#define MSXML_DISPID_CUSTOM_MAX 0x6fffffff

typedef struct {
    HRESULT (*get_dispid)(IUnknown*,BSTR,DWORD,DISPID*);
    HRESULT (*invoke)(IUnknown*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*);
} dispex_static_data_vtbl_t;

typedef struct {
    const dispex_static_data_vtbl_t *vtbl;
    const tid_t disp_tid;
    dispex_data_t *data;
    const tid_t* const iface_tids;
} dispex_static_data_t;

typedef struct {
    const IDispatchExVtbl  *lpIDispatchExVtbl;

    IUnknown *outer;

    dispex_static_data_t *data;
    dispex_dynamic_data_t *dynamic_data;
} DispatchEx;

extern HINSTANCE MSXML_hInstance;

void init_dispex(DispatchEx*,IUnknown*,dispex_static_data_t*);
BOOL dispex_query_interface(DispatchEx*,REFIID,void**);

#ifdef HAVE_LIBXML2

#ifdef HAVE_LIBXML_PARSER_H
#include <libxml/parser.h>
#endif



#include <libxml/xmlerror.h>

extern void schemasInit(void);
extern void schemasCleanup(void);

#ifndef HAVE_XMLFIRSTELEMENTCHILD
static inline xmlNodePtr wine_xmlFirstElementChild(xmlNodePtr parent)
{
    xmlNodePtr child;
    for (child = parent->children; child != NULL; child = child->next)
        if (child->type == XML_ELEMENT_NODE)
            break;

    return child;
}
#define xmlFirstElementChild wine_xmlFirstElementChild
#endif

/* constructors */
extern IUnknown         *create_domdoc( xmlNodePtr document );
extern IUnknown         *create_xmldoc( void );
extern IXMLDOMNode      *create_node( xmlNodePtr node );
extern IUnknown         *create_element( xmlNodePtr element );
extern IUnknown         *create_attribute( xmlNodePtr attribute );
extern IUnknown         *create_text( xmlNodePtr text );
extern IUnknown         *create_pi( xmlNodePtr pi );
extern IUnknown         *create_comment( xmlNodePtr comment );
extern IUnknown         *create_cdata( xmlNodePtr text );
extern IXMLDOMNodeList  *create_children_nodelist( xmlNodePtr );
extern IXMLDOMNamedNodeMap *create_nodemap( IXMLDOMNode *node );
extern IUnknown         *create_doc_Implementation(void);
extern IUnknown         *create_doc_fragment( xmlNodePtr fragment );
extern IUnknown         *create_doc_entity_ref( xmlNodePtr entity );
extern IUnknown         *create_doc_type( xmlNodePtr doctype );

extern HRESULT queryresult_create( xmlNodePtr node, xmlChar* szQuery, IXMLDOMNodeList** out );

/* data accessors */
xmlNodePtr xmlNodePtr_from_domnode( IXMLDOMNode *iface, xmlElementType type );

/* helpers */
extern xmlChar *xmlChar_from_wchar( LPCWSTR str );

extern void xmldoc_init( xmlDocPtr doc, const GUID *clsid );
extern LONG xmldoc_add_ref( xmlDocPtr doc );
extern LONG xmldoc_release( xmlDocPtr doc );
extern HRESULT xmldoc_add_orphan( xmlDocPtr doc, xmlNodePtr node );
extern HRESULT xmldoc_remove_orphan( xmlDocPtr doc, xmlNodePtr node );
extern void xmldoc_link_xmldecl(xmlDocPtr doc, xmlNodePtr node);
extern xmlNodePtr xmldoc_unlink_xmldecl(xmlDocPtr doc);

extern HRESULT XMLElement_create( IUnknown *pUnkOuter, xmlNodePtr node, LPVOID *ppObj, BOOL own );

extern void wineXmlCallbackLog(char const* caller, xmlErrorLevel lvl, char const* msg, va_list ap);
extern void wineXmlCallbackError(char const* caller, xmlErrorPtr err);

#define LIBXML2_LOG_CALLBACK __WINE_PRINTF_ATTR(2,3)

#define LIBXML2_CALLBACK_TRACE(caller, msg, ap) \
        wineXmlCallbackLog(#caller, XML_ERR_NONE, msg, ap)

#define LIBXML2_CALLBACK_WARN(caller, msg, ap) \
        wineXmlCallbackLog(#caller, XML_ERR_WARNING, msg, ap)

#define LIBXML2_CALLBACK_ERR(caller, msg, ap) \
        wineXmlCallbackLog(#caller, XML_ERR_ERROR, msg, ap)

#define LIBXML2_CALLBACK_SERROR(caller, err) wineXmlCallbackError(#caller, err)

extern BOOL is_preserving_whitespace(xmlNodePtr node);
extern BOOL is_xpathmode(const xmlDocPtr doc);
extern void set_xpathmode(xmlDocPtr doc, BOOL xpath);

/* IXMLDOMNode Internal Structure */
typedef struct _xmlnode
{
    DispatchEx dispex;
    const struct IXMLDOMNodeVtbl *lpVtbl;
    IXMLDOMNode *iface;
    xmlNodePtr node;
} xmlnode;

static inline IXMLDOMNode *IXMLDOMNode_from_impl(xmlnode *This)
{
    return (IXMLDOMNode*)&This->lpVtbl;
}

extern void init_xmlnode(xmlnode*,xmlNodePtr,IXMLDOMNode*,dispex_static_data_t*);
extern void destroy_xmlnode(xmlnode*);
extern BOOL node_query_interface(xmlnode*,REFIID,void**);
extern xmlnode *get_node_obj(IXMLDOMNode*);

extern HRESULT node_get_nodeName(xmlnode*,BSTR*);
extern HRESULT node_get_content(xmlnode*,VARIANT*);
extern HRESULT node_set_content(xmlnode*,LPCWSTR);
extern HRESULT node_put_value(xmlnode*,VARIANT*);
extern HRESULT node_put_value_escaped(xmlnode*,VARIANT*);
extern HRESULT node_get_parent(xmlnode*,IXMLDOMNode**);
extern HRESULT node_get_child_nodes(xmlnode*,IXMLDOMNodeList**);
extern HRESULT node_get_first_child(xmlnode*,IXMLDOMNode**);
extern HRESULT node_get_last_child(xmlnode*,IXMLDOMNode**);
extern HRESULT node_get_previous_sibling(xmlnode*,IXMLDOMNode**);
extern HRESULT node_get_next_sibling(xmlnode*,IXMLDOMNode**);
extern HRESULT node_insert_before(xmlnode*,IXMLDOMNode*,const VARIANT*,IXMLDOMNode**);
extern HRESULT node_replace_child(xmlnode*,IXMLDOMNode*,IXMLDOMNode*,IXMLDOMNode**);
extern HRESULT node_put_text(xmlnode*,BSTR);
extern HRESULT node_get_xml(xmlnode*,BOOL,BOOL,BSTR*);
extern HRESULT node_clone(xmlnode*,VARIANT_BOOL,IXMLDOMNode**);
extern HRESULT node_get_prefix(xmlnode*,BSTR*);
extern HRESULT node_get_base_name(xmlnode*,BSTR*);

extern HRESULT DOMDocument_create_from_xmldoc(xmlDocPtr xmldoc, IXMLDOMDocument3 **document);
extern HRESULT SchemaCache_validate_tree(IXMLDOMSchemaCollection2* iface, xmlNodePtr tree);
extern XDR_DT  SchemaCache_get_node_dt(IXMLDOMSchemaCollection2* iface, xmlNodePtr node);

extern XDR_DT str_to_dt(xmlChar const* str, int len /* calculated if -1 */);
extern XDR_DT bstr_to_dt(OLECHAR const* bstr, int len /* calculated if -1 */);
extern xmlChar const* dt_to_str(XDR_DT dt);
extern OLECHAR const* dt_to_bstr(XDR_DT dt);
extern XDR_DT element_get_dt(xmlNodePtr node);
extern HRESULT dt_validate(XDR_DT dt, xmlChar const* content);

extern BSTR EnsureCorrectEOL(BSTR);

extern xmlChar* tagName_to_XPath(const BSTR tagName);

static inline BSTR bstr_from_xmlChar(const xmlChar *str)
{
    BSTR ret = NULL;

    if(str) {
        DWORD len = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)str, -1, NULL, 0);
        ret = SysAllocStringLen(NULL, len-1);
        if(ret)
            MultiByteToWideChar( CP_UTF8, 0, (LPCSTR)str, -1, ret, len);
    }
    else
        ret = SysAllocStringLen(NULL, 0);

    return ret;
}

static inline HRESULT return_bstr(const WCHAR *value, BSTR *p)
{
    if(!p)
        return E_INVALIDARG;

    if(value) {
        *p = SysAllocString(value);
        if(!*p)
            return E_OUTOFMEMORY;
    }else {
        *p = NULL;
    }

    return S_OK;
}

static inline HRESULT return_null_node(IXMLDOMNode **p)
{
    if(!p)
        return E_INVALIDARG;
    *p = NULL;
    return S_FALSE;
}

static inline HRESULT return_null_ptr(void **p)
{
    if(!p)
        return E_INVALIDARG;
    *p = NULL;
    return S_FALSE;
}

static inline HRESULT return_null_var(VARIANT *p)
{
    if(!p)
        return E_INVALIDARG;

    V_VT(p) = VT_NULL;
    return S_FALSE;
}

static inline HRESULT return_null_bstr(BSTR *p)
{
    if(!p)
        return E_INVALIDARG;

    *p = NULL;
    return S_FALSE;
}

#endif

extern void* libxslt_handle;
#ifdef SONAME_LIBXSLT
# ifdef HAVE_LIBXSLT_PATTERN_H
#  include <libxslt/pattern.h>
# endif
# ifdef HAVE_LIBXSLT_TRANSFORM_H
#  include <libxslt/transform.h>
# endif
# include <libxslt/xsltutils.h>
# include <libxslt/xsltInternals.h>

# define MAKE_FUNCPTR(f) extern typeof(f) * p##f
MAKE_FUNCPTR(xsltApplyStylesheet);
MAKE_FUNCPTR(xsltCleanupGlobals);
MAKE_FUNCPTR(xsltFreeStylesheet);
MAKE_FUNCPTR(xsltParseStylesheetDoc);
# undef MAKE_FUNCPTR
#endif

extern IXMLDOMParseError *create_parseError( LONG code, BSTR url, BSTR reason, BSTR srcText,
                                             LONG line, LONG linepos, LONG filepos );
extern HRESULT DOMDocument_create( const GUID *clsid, IUnknown *pUnkOuter, void **ppObj );
extern HRESULT SchemaCache_create( const GUID *clsid, IUnknown *pUnkOuter, void **ppObj );
extern HRESULT XMLDocument_create( IUnknown *pUnkOuter, void **pObj );
extern HRESULT SAXXMLReader_create(IUnknown *pUnkOuter, void **pObj );
extern HRESULT XMLHTTPRequest_create(IUnknown *pUnkOuter, void **pObj);

static inline const CLSID* DOMDocument_version(MSXML_VERSION v)
{
    switch (v)
    {
    default:
    case MSXML_DEFAULT: return &CLSID_DOMDocument;
    case MSXML3: return &CLSID_DOMDocument30;
    case MSXML4: return &CLSID_DOMDocument40;
    case MSXML6: return &CLSID_DOMDocument60;
    }
}

static inline const CLSID* SchemaCache_version(MSXML_VERSION v)
{
    switch (v)
    {
    default:
    case MSXML_DEFAULT: return &CLSID_XMLSchemaCache;
    case MSXML3: return &CLSID_XMLSchemaCache30;
    case MSXML4: return &CLSID_XMLSchemaCache40;
    case MSXML6: return &CLSID_XMLSchemaCache60;
    }
}

typedef struct bsc_t bsc_t;

HRESULT bind_url(LPCWSTR, HRESULT (*onDataAvailable)(void*,char*,DWORD), void*, bsc_t**);
void detach_bsc(bsc_t*);

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

/* Error Codes - not defined anywhere in the public headers */
#define E_XML_ELEMENT_UNDECLARED            0xC00CE00D
#define E_XML_ELEMENT_ID_NOT_FOUND          0xC00CE00E
/* ... */
#define E_XML_EMPTY_NOT_ALLOWED             0xC00CE011
#define E_XML_ELEMENT_NOT_COMPLETE          0xC00CE012
#define E_XML_ROOT_NAME_MISMATCH            0xC00CE013
#define E_XML_INVALID_CONTENT               0xC00CE014
#define E_XML_ATTRIBUTE_NOT_DEFINED         0xC00CE015
#define E_XML_ATTRIBUTE_FIXED               0xC00CE016
#define E_XML_ATTRIBUTE_VALUE               0xC00CE017
#define E_XML_ILLEGAL_TEXT                  0xC00CE018
/* ... */
#define E_XML_REQUIRED_ATTRIBUTE_MISSING    0xC00CE020

#endif /* __MSXML_PRIVATE__ */

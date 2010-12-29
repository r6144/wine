/*
 * XML test
 *
 * Copyright 2005 Mike McCormack for CodeWeavers
 * Copyright 2007-2008 Alistair Leslie-Hughes
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

#include "windows.h"
#include "ole2.h"
#include "objsafe.h"
#include "msxml2.h"
#include "msxml2did.h"
#include "dispex.h"
#include <stdio.h>
#include <assert.h>

#include "wine/test.h"

#include "initguid.h"

DEFINE_GUID(IID_IObjectSafety, 0xcb5bdc81, 0x93c1, 0x11cf, 0x8f,0x20, 0x00,0x80,0x5f,0x2c,0xd0,0x64);

static int g_unexpectedcall, g_expectedcall;

typedef struct
{
    const struct IDispatchVtbl *lpVtbl;
    LONG ref;
} dispevent;

static inline dispevent *impl_from_IDispatch( IDispatch *iface )
{
    return (dispevent *)((char*)iface - FIELD_OFFSET(dispevent, lpVtbl));
}

static HRESULT WINAPI dispevent_QueryInterface(IDispatch *iface, REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if ( IsEqualGUID( riid, &IID_IDispatch) ||
         IsEqualGUID( riid, &IID_IUnknown) )
    {
        *ppvObject = iface;
    }
    else
        return E_NOINTERFACE;

    IDispatch_AddRef( iface );

    return S_OK;
}

static ULONG WINAPI dispevent_AddRef(IDispatch *iface)
{
    dispevent *This = impl_from_IDispatch( iface );
    return InterlockedIncrement( &This->ref );
}

static ULONG WINAPI dispevent_Release(IDispatch *iface)
{
    dispevent *This = impl_from_IDispatch( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    if (ref == 0)
        HeapFree(GetProcessHeap(), 0, This);

    return ref;
}

static HRESULT WINAPI dispevent_GetTypeInfoCount(IDispatch *iface, UINT *pctinfo)
{
    g_unexpectedcall++;
    *pctinfo = 0;
    return S_OK;
}

static HRESULT WINAPI dispevent_GetTypeInfo(IDispatch *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    g_unexpectedcall++;
    return S_OK;
}

static HRESULT WINAPI dispevent_GetIDsOfNames(IDispatch *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    g_unexpectedcall++;
    return S_OK;
}

static HRESULT WINAPI dispevent_Invoke(IDispatch *iface, DISPID member, REFIID riid,
        LCID lcid, WORD flags, DISPPARAMS *params, VARIANT *result,
        EXCEPINFO *excepInfo, UINT *argErr)
{
    ok(member == 0, "expected 0 member, got %d\n", member);
    ok(lcid == LOCALE_SYSTEM_DEFAULT, "expected LOCALE_SYSTEM_DEFAULT, got lcid %x\n", lcid);
    ok(flags == DISPATCH_METHOD, "expected DISPATCH_METHOD, got %d\n", flags);

    ok(params->cArgs == 0, "got %d\n", params->cArgs);
    ok(params->cNamedArgs == 0, "got %d\n", params->cNamedArgs);
    ok(params->rgvarg == NULL, "got %p\n", params->rgvarg);
    ok(params->rgdispidNamedArgs == NULL, "got %p\n", params->rgdispidNamedArgs);

    ok(result == NULL, "got %p\n", result);
    ok(excepInfo == NULL, "got %p\n", excepInfo);
    ok(argErr == NULL, "got %p\n", argErr);

    g_expectedcall++;
    return E_FAIL;
}

static const IDispatchVtbl dispeventVtbl =
{
    dispevent_QueryInterface,
    dispevent_AddRef,
    dispevent_Release,
    dispevent_GetTypeInfoCount,
    dispevent_GetTypeInfo,
    dispevent_GetIDsOfNames,
    dispevent_Invoke
};

static IDispatch* create_dispevent(void)
{
    dispevent *event = HeapAlloc(GetProcessHeap(), 0, sizeof(*event));

    event->lpVtbl = &dispeventVtbl;
    event->ref = 1;

    return (IDispatch*)&event->lpVtbl;
}


static const WCHAR szEmpty[] = { 0 };
static const WCHAR szIncomplete[] = {
    '<','?','x','m','l',' ',
    'v','e','r','s','i','o','n','=','\'','1','.','0','\'','?','>','\n',0
};
static const WCHAR szComplete1[] = {
    '<','?','x','m','l',' ',
    'v','e','r','s','i','o','n','=','\'','1','.','0','\'','?','>','\n',
    '<','o','p','e','n','>','<','/','o','p','e','n','>','\n',0
};
static const WCHAR szComplete2[] = {
    '<','?','x','m','l',' ',
    'v','e','r','s','i','o','n','=','\'','1','.','0','\'','?','>','\n',
    '<','o','>','<','/','o','>','\n',0
};
static const WCHAR szComplete3[] = {
    '<','?','x','m','l',' ',
    'v','e','r','s','i','o','n','=','\'','1','.','0','\'','?','>','\n',
    '<','a','>','<','/','a','>','\n',0
};
static const WCHAR szComplete4[] = {
    '<','?','x','m','l',' ','v','e','r','s','i','o','n','=','\'','1','.','0','\'','?','>','\n',
    '<','l','c',' ','d','l','=','\'','s','t','r','1','\'','>','\n',
        '<','b','s',' ','v','r','=','\'','s','t','r','2','\'',' ','s','z','=','\'','1','2','3','4','\'','>',
            'f','n','1','.','t','x','t','\n',
        '<','/','b','s','>','\n',
        '<','p','r',' ','i','d','=','\'','s','t','r','3','\'',' ','v','r','=','\'','1','.','2','.','3','\'',' ',
                    'p','n','=','\'','w','i','n','e',' ','2','0','0','5','0','8','0','4','\'','>','\n',
            'f','n','2','.','t','x','t','\n',
        '<','/','p','r','>','\n',
        '<','e','m','p','t','y','>','<','/','e','m','p','t','y','>','\n',
        '<','f','o','>','\n',
            '<','b','a','>','\n',
                'f','1','\n',
            '<','/','b','a','>','\n',
        '<','/','f','o','>','\n',
    '<','/','l','c','>','\n',0
};
static const WCHAR szComplete5[] = {
    '<','S',':','s','e','a','r','c','h',' ','x','m','l','n','s',':','D','=','"','D','A','V',':','"',' ',
    'x','m','l','n','s',':','C','=','"','u','r','n',':','s','c','h','e','m','a','s','-','m','i','c','r','o','s','o','f','t','-','c','o','m',':','o','f','f','i','c','e',':','c','l','i','p','g','a','l','l','e','r','y','"',
    ' ','x','m','l','n','s',':','S','=','"','u','r','n',':','s','c','h','e','m','a','s','-','m','i','c','r','o','s','o','f','t','-','c','o','m',':','o','f','f','i','c','e',':','c','l','i','p','g','a','l','l','e','r','y',':','s','e','a','r','c','h','"','>',
        '<','S',':','s','c','o','p','e','>',
            '<','S',':','d','e','e','p','>','/','<','/','S',':','d','e','e','p','>',
        '<','/','S',':','s','c','o','p','e','>',
        '<','S',':','c','o','n','t','e','n','t','f','r','e','e','t','e','x','t','>',
            '<','C',':','t','e','x','t','o','r','p','r','o','p','e','r','t','y','/','>',
            'c','o','m','p','u','t','e','r',
        '<','/','S',':','c','o','n','t','e','n','t','f','r','e','e','t','e','x','t','>',
    '<','/','S',':','s','e','a','r','c','h','>',0
};

static const WCHAR szComplete6[] = {
    '<','?','x','m','l',' ','v','e','r','s','i','o','n','=','\'','1','.','0','\'',' ',
    'e','n','c','o','d','i','n','g','=','\'','W','i','n','d','o','w','s','-','1','2','5','2','\'','?','>','\n',
    '<','o','p','e','n','>','<','/','o','p','e','n','>','\n',0
};

static const CHAR szNonUnicodeXML[] =
"<?xml version='1.0' encoding='Windows-1252'?>\n"
"<open></open>\n";

static const CHAR szExampleXML[] =
"<?xml version='1.0' encoding='utf-8'?>\n"
"<root xmlns:foo='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29'>\n"
"    <elem>\n"
"        <a>A1 field</a>\n"
"        <b>B1 field</b>\n"
"        <c>C1 field</c>\n"
"        <description xmlns:foo='http://www.winehq.org' xmlns:bar='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29'>\n"
"            <html xmlns='http://www.w3.org/1999/xhtml'>\n"
"                This is <strong>a</strong> <i>description</i>. <bar:x/>\n"
"            </html>\n"
"            <html xml:space='preserve' xmlns='http://www.w3.org/1999/xhtml'>\n"
"                This is <strong>a</strong> <i>description</i> with preserved whitespace. <bar:x/>\n"
"            </html>\n"
"        </description>\n"
"    </elem>\n"
"\n"
"    <elem>\n"
"        <a>A2 field</a>\n"
"        <b>B2 field</b>\n"
"        <c type=\"old\">C2 field</c>\n"
"    </elem>\n"
"\n"
"    <elem xmlns='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29'>\n"
"        <a>A3 field</a>\n"
"        <b>B3 field</b>\n"
"        <c>C3 field</c>\n"
"    </elem>\n"
"\n"
"    <elem>\n"
"        <a>A4 field</a>\n"
"        <b>B4 field</b>\n"
"        <foo:c>C4 field</foo:c>\n"
"    </elem>\n"
"</root>\n";

static const CHAR szNodeTypesXML[] =
"<?xml version='1.0'?>"
"<!-- comment node 0 -->"
"<root id='0' depth='0'>"
"   <!-- comment node 1 -->"
"   text node 0"
"   <x id='1' depth='1'>"
"       <?foo value='PI for x'?>"
"       <!-- comment node 2 -->"
"       text node 1"
"       <a id='3' depth='2'/>"
"       <b id='4' depth='2'/>"
"       <c id='5' depth='2'/>"
"   </x>"
"   <y id='2' depth='1'>"
"       <?bar value='PI for y'?>"
"       <!-- comment node 3 -->"
"       text node 2"
"       <a id='6' depth='2'/>"
"       <b id='7' depth='2'/>"
"       <c id='8' depth='2'/>"
"   </y>"
"</root>";

static const CHAR szTransformXML[] =
"<?xml version=\"1.0\"?>\n"
"<greeting>\n"
"Hello World\n"
"</greeting>";

static  const CHAR szTransformSSXML[] =
"<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\">\n"
"   <xsl:output method=\"html\"/>\n"
"   <xsl:template match=\"/\">\n"
"       <xsl:apply-templates select=\"greeting\"/>\n"
"   </xsl:template>\n"
"   <xsl:template match=\"greeting\">\n"
"       <html>\n"
"           <body>\n"
"               <h1>\n"
"                   <xsl:value-of select=\".\"/>\n"
"               </h1>\n"
"           </body>\n"
"       </html>\n"
"   </xsl:template>\n"
"</xsl:stylesheet>";

static  const CHAR szTransformOutput[] =
"<html><body><h1>"
"Hello World"
"</h1></body></html>";

static const CHAR szTypeValueXML[] =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<root xmlns:dt=\"urn:schemas-microsoft-com:datatypes\">\n"
"   <string>Wine</string>\n"
"   <string2 dt:dt=\"string\">String</string2>\n"
"   <number dt:dt=\"number\">12.44</number>\n"
"   <number2 dt:dt=\"NUMbEr\">-3.71e3</number2>\n"
"   <int dt:dt=\"int\">-13</int>\n"
"   <fixed dt:dt=\"fixed.14.4\">7322.9371</fixed>\n"
"   <bool dt:dt=\"boolean\">1</bool>\n"
"   <datetime dt:dt=\"datetime\">2009-11-18T03:21:33.12</datetime>\n"
"   <datetimetz dt:dt=\"datetime.tz\">2003-07-11T11:13:57+03:00</datetimetz>\n"
"   <date dt:dt=\"date\">3721-11-01</date>\n"
"   <time dt:dt=\"time\">13:57:12.31321</time>\n"
"   <timetz dt:dt=\"time.tz\">23:21:01.13+03:21</timetz>\n"
"   <i1 dt:dt=\"i1\">-13</i1>\n"
"   <i2 dt:dt=\"i2\">31915</i2>\n"
"   <i4 dt:dt=\"i4\">-312232</i4>\n"
"   <ui1 dt:dt=\"ui1\">123</ui1>\n"
"   <ui2 dt:dt=\"ui2\">48282</ui2>\n"
"   <ui4 dt:dt=\"ui4\">949281</ui4>\n"
"   <r4 dt:dt=\"r4\">213124.0</r4>\n"
"   <r8 dt:dt=\"r8\">0.412</r8>\n"
"   <float dt:dt=\"float\">41221.421</float>\n"
"   <uuid dt:dt=\"uuid\">333C7BC4-460F-11D0-BC04-0080C7055a83</uuid>\n"
"   <binhex dt:dt=\"bin.hex\">fffca012003c</binhex>\n"
"   <binbase64 dt:dt=\"bin.base64\">YmFzZTY0IHRlc3Q=</binbase64>\n"
"</root>";

static const CHAR szBasicTransformSSXMLPart1[] =
"<?xml version=\"1.0\"?>"
"<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" >"
"<xsl:output method=\"html\"/>\n"
"<xsl:template match=\"/\">"
"<HTML><BODY><TABLE>"
"        <xsl:apply-templates select='document(\"";

static const CHAR szBasicTransformSSXMLPart2[] =
"\")/bottle/wine'>"
"           <xsl:sort select=\"cost\"/><xsl:sort select=\"name\"/>"
"        </xsl:apply-templates>"
"</TABLE></BODY></HTML>"
"</xsl:template>"
"<xsl:template match=\"bottle\">"
"   <TR><xsl:apply-templates select=\"name\" /><xsl:apply-templates select=\"cost\" /></TR>"
"</xsl:template>"
"<xsl:template match=\"name\">"
"   <TD><xsl:apply-templates /></TD>"
"</xsl:template>"
"<xsl:template match=\"cost\">"
"   <TD><xsl:apply-templates /></TD>"
"</xsl:template>"
"</xsl:stylesheet>";

static const CHAR szBasicTransformXML[] =
"<?xml version=\"1.0\"?><bottle><wine><name>Wine</name><cost>$25.00</cost></wine></bottle>";

static const CHAR szBasicTransformOutput[] =
"<HTML><BODY><TABLE><TD>Wine</TD><TD>$25.00</TD></TABLE></BODY></HTML>";

#define SZ_EMAIL_DTD \
"<!DOCTYPE email ["\
"   <!ELEMENT email         (recipients,from,reply-to?,subject,body,attachment*)>"\
"       <!ATTLIST email attachments IDREFS #REQUIRED>"\
"       <!ATTLIST email sent (yes|no) \"no\">"\
"   <!ELEMENT recipients    (to+,cc*)>"\
"   <!ELEMENT to            (#PCDATA)>"\
"       <!ATTLIST to name CDATA #IMPLIED>"\
"   <!ELEMENT cc            (#PCDATA)>"\
"       <!ATTLIST cc name CDATA #IMPLIED>"\
"   <!ELEMENT from          (#PCDATA)>"\
"       <!ATTLIST from name CDATA #IMPLIED>"\
"   <!ELEMENT reply-to      (#PCDATA)>"\
"       <!ATTLIST reply-to name CDATA #IMPLIED>"\
"   <!ELEMENT subject       ANY>"\
"   <!ELEMENT body          ANY>"\
"       <!ATTLIST body enc CDATA #FIXED \"UTF-8\">"\
"   <!ELEMENT attachment    (#PCDATA)>"\
"       <!ATTLIST attachment id ID #REQUIRED>"\
"]>"

static const CHAR szEmailXML[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 87)</subject>"
"   <body>"
"       It no longer causes spontaneous combustion..."
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_0D[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 88)</subject>"
"   <body>"
"       <undecl />"
"       XML_ELEMENT_UNDECLARED 0xC00CE00D"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_0E[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 89)</subject>"
"   <body>"
"       XML_ELEMENT_ID_NOT_FOUND 0xC00CE00E"
"   </body>"
"   <attachment id=\"patch\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_11[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   <recipients>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 90)</subject>"
"   <body>"
"       XML_EMPTY_NOT_ALLOWED 0xC00CE011"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_13[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<msg attachments=\"patch1\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 91)</subject>"
"   <body>"
"       XML_ROOT_NAME_MISMATCH 0xC00CE013"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</msg>";

static const CHAR szEmailXML_14[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   <to>wine-patches@winehq.org</to>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 92)</subject>"
"   <body>"
"       XML_INVALID_CONTENT 0xC00CE014"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_15[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\" ip=\"127.0.0.1\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 93)</subject>"
"   <body>"
"       XML_ATTRIBUTE_NOT_DEFINED 0xC00CE015"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_16[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 94)</subject>"
"   <body enc=\"ASCII\">"
"       XML_ATTRIBUTE_FIXED 0xC00CE016"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_17[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\" sent=\"true\">"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 95)</subject>"
"   <body>"
"       XML_ATTRIBUTE_VALUE 0xC00CE017"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_18[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email attachments=\"patch1\">"
"   oops"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 96)</subject>"
"   <body>"
"       XML_ILLEGAL_TEXT 0xC00CE018"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const CHAR szEmailXML_20[] =
"<?xml version=\"1.0\"?>"
SZ_EMAIL_DTD
"<email>"
"   <recipients>"
"       <to>wine-patches@winehq.org</to>"
"   </recipients>"
"   <from name=\"Anonymous\">user@localhost</from>"
"   <subject>msxml3/tests: DTD validation (try 97)</subject>"
"   <body>"
"       XML_REQUIRED_ATTRIBUTE_MISSING 0xC00CE020"
"   </body>"
"   <attachment id=\"patch1\">0001-msxml3-tests-DTD-validation.patch</attachment>"
"</email>";

static const WCHAR szNonExistentFile[] = {
    'c', ':', '\\', 'N', 'o', 'n', 'e', 'x', 'i', 's', 't', 'e', 'n', 't', '.', 'x', 'm', 'l', 0
};
static const WCHAR szNonExistentAttribute[] = {
    'n','o','n','E','x','i','s','i','t','i','n','g','A','t','t','r','i','b','u','t','e',0
};
static const WCHAR szDocument[] = {
    '#', 'd', 'o', 'c', 'u', 'm', 'e', 'n', 't', 0
};

static const WCHAR szOpen[] = { 'o','p','e','n',0 };
static WCHAR szdl[] = { 'd','l',0 };
static const WCHAR szvr[] = { 'v','r',0 };
static const WCHAR szlc[] = { 'l','c',0 };
static WCHAR szbs[] = { 'b','s',0 };
static const WCHAR szstr1[] = { 's','t','r','1',0 };
static const WCHAR szstr2[] = { 's','t','r','2',0 };
static const WCHAR szstar[] = { '*',0 };
static const WCHAR szfn1_txt[] = {'f','n','1','.','t','x','t',0};

static WCHAR szComment[] = {'A',' ','C','o','m','m','e','n','t',0 };
static WCHAR szCommentXML[] = {'<','!','-','-','A',' ','C','o','m','m','e','n','t','-','-','>',0 };
static WCHAR szCommentNodeText[] = {'#','c','o','m','m','e','n','t',0 };

static WCHAR szElement[] = {'E','l','e','T','e','s','t', 0 };
static WCHAR szElementXML[]  = {'<','E','l','e','T','e','s','t','/','>',0 };
static WCHAR szElementXML2[] = {'<','E','l','e','T','e','s','t',' ','A','t','t','r','=','"','"','/','>',0 };
static WCHAR szElementXML3[] = {'<','E','l','e','T','e','s','t',' ','A','t','t','r','=','"','"','>',
                                'T','e','s','t','i','n','g','N','o','d','e','<','/','E','l','e','T','e','s','t','>',0 };
static WCHAR szElementXML4[] = {'<','E','l','e','T','e','s','t',' ','A','t','t','r','=','"','"','>',
                                '&','a','m','p',';','x',' ',0x2103,'<','/','E','l','e','T','e','s','t','>',0 };

static WCHAR szAttribute[] = {'A','t','t','r',0 };
static WCHAR szAttributeXML[] = {'A','t','t','r','=','"','"',0 };

static WCHAR szCData[] = {'[','1',']','*','2','=','3',';',' ','&','g','e','e',' ','t','h','a','t','s',
                          ' ','n','o','t',' ','r','i','g','h','t','!', 0};
static WCHAR szCDataXML[] = {'<','!','[','C','D','A','T','A','[','[','1',']','*','2','=','3',';',' ','&',
                             'g','e','e',' ','t','h','a','t','s',' ','n','o','t',' ','r','i','g','h','t',
                             '!',']',']','>',0};
static WCHAR szCDataNodeText[] = {'#','c','d','a','t','a','-','s','e','c','t','i','o','n',0 };
static WCHAR szDocFragmentText[] = {'#','d','o','c','u','m','e','n','t','-','f','r','a','g','m','e','n','t',0 };

static WCHAR szEntityRef[] = {'e','n','t','i','t','y','r','e','f',0 };
static WCHAR szEntityRefXML[] = {'&','e','n','t','i','t','y','r','e','f',';',0 };
static WCHAR szStrangeChars[] = {'&','x',' ',0x2103, 0};

#define expect_bstr_eq_and_free(bstr, expect) { \
    BSTR bstrExp = alloc_str_from_narrow(expect); \
    ok(lstrcmpW(bstr, bstrExp) == 0, "String differs\n"); \
    SysFreeString(bstr); \
    SysFreeString(bstrExp); \
}

#define expect_eq(expr, value, type, format) { type ret = (expr); ok((value) == ret, #expr " expected " format " got " format "\n", value, ret); }

#define ole_check(expr) { \
    HRESULT r = expr; \
    ok(r == S_OK, #expr " returned %x\n", r); \
}

#define ole_expect(expr, expect) { \
    HRESULT r = expr; \
    ok(r == (expect), #expr " returned %x, expected %x\n", r, expect); \
}

#define double_eq(x, y) ok((x)-(y)<=1e-14*(x) && (x)-(y)>=-1e-14*(x), "expected %.16g, got %.16g\n", x, y)

static void* _create_object(const GUID *clsid, const char *name, const IID *iid, int line)
{
    void *obj = NULL;
    HRESULT hr;

    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, iid, &obj);
    if (hr != S_OK)
        win_skip_(__FILE__,line)("failed to create %s instance: 0x%08x\n", name, hr);

    return obj;
}

#define _create(cls) cls, #cls

#define create_document(iid) _create_object(&_create(CLSID_DOMDocument), iid, __LINE__)

#define create_document_version(v, iid) _create_object(&_create(CLSID_DOMDocument ## v), iid, __LINE__)

#define create_cache(iid) _create_object(&_create(CLSID_XMLSchemaCache), iid, __LINE__)

#define create_cache_version(v, iid) _create_object(&_create(CLSID_XMLSchemaCache ## v), iid, __LINE__)

static BSTR alloc_str_from_narrow(const char *str)
{
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    BSTR ret = SysAllocStringLen(NULL, len - 1);  /* NUL character added automatically */
    MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    return ret;
}

static BSTR alloced_bstrs[256];
static int alloced_bstrs_count;

static BSTR _bstr_(const char *str)
{
    assert(alloced_bstrs_count < sizeof(alloced_bstrs)/sizeof(alloced_bstrs[0]));
    alloced_bstrs[alloced_bstrs_count] = alloc_str_from_narrow(str);
    return alloced_bstrs[alloced_bstrs_count++];
}

static void free_bstrs(void)
{
    int i;
    for (i = 0; i < alloced_bstrs_count; i++)
        SysFreeString(alloced_bstrs[i]);
    alloced_bstrs_count = 0;
}

static VARIANT _variantbstr_(const char *str)
{
    VARIANT v;
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = _bstr_(str);
    return v;
}

static BOOL compareIgnoreReturns(BSTR sLeft, BSTR sRight)
{
    for (;;)
    {
        while (*sLeft == '\r' || *sLeft == '\n') sLeft++;
        while (*sRight == '\r' || *sRight == '\n') sRight++;
        if (*sLeft != *sRight) return FALSE;
        if (!*sLeft) return TRUE;
        sLeft++;
        sRight++;
    }
}

static void get_str_for_type(DOMNodeType type, char *buf)
{
    switch (type)
    {
        case NODE_ATTRIBUTE:
            strcpy(buf, "A");
            break;
        case NODE_ELEMENT:
            strcpy(buf, "E");
            break;
        case NODE_DOCUMENT:
            strcpy(buf, "D");
            break;
        case NODE_TEXT:
            strcpy(buf, "T");
            break;
        case NODE_COMMENT:
            strcpy(buf, "C");
            break;
        case NODE_PROCESSING_INSTRUCTION:
            strcpy(buf, "P");
            break;
        default:
            wsprintfA(buf, "[%d]", type);
    }
}

#define test_disp(u) _test_disp(__LINE__,u)
static void _test_disp(unsigned line, IUnknown *unk)
{
    DISPID dispid = DISPID_XMLDOM_NODELIST_RESET;
    IDispatchEx *dispex;
    DWORD dwProps = 0;
    BSTR sName;
    UINT ticnt;
    IUnknown *pUnk;
    HRESULT hres;

    hres = IUnknown_QueryInterface(unk, &IID_IDispatchEx, (void**)&dispex);
    ok_(__FILE__,line) (hres == S_OK, "Could not get IDispatch: %08x\n", hres);
    if(FAILED(hres))
        return;

    ticnt = 0xdeadbeef;
    hres = IDispatchEx_GetTypeInfoCount(dispex, &ticnt);
    ok_(__FILE__,line) (hres == S_OK, "GetTypeInfoCount failed: %08x\n", hres);
    ok_(__FILE__,line) (ticnt == 1, "ticnt=%u\n", ticnt);

    sName = SysAllocString( szstar );
    hres = IDispatchEx_DeleteMemberByName(dispex, sName, fdexNameCaseSensitive);
    ok(hres == E_NOTIMPL, "expected E_NOTIMPL got %08x\n", hres);
    SysFreeString( sName );

    hres = IDispatchEx_DeleteMemberByDispID(dispex, dispid);
    ok(hres == E_NOTIMPL, "expected E_NOTIMPL got %08x\n", hres);

    hres = IDispatchEx_GetMemberProperties(dispex, dispid, grfdexPropCanAll, &dwProps);
    ok(hres == E_NOTIMPL, "expected E_NOTIMPL got %08x\n", hres);
    ok(dwProps == 0, "expected 0 got %d\n", dwProps);

    hres = IDispatchEx_GetMemberName(dispex, dispid, &sName);
    ok(hres == E_NOTIMPL, "expected E_NOTIMPL got %08x\n", hres);
    if(SUCCEEDED(hres))
        SysFreeString(sName);

    hres = IDispatchEx_GetNextDispID(dispex, fdexEnumDefault, DISPID_XMLDOM_NODELIST_RESET, &dispid);
    ok(hres == E_NOTIMPL, "expected E_NOTIMPL got %08x\n", hres);

    hres = IDispatchEx_GetNameSpaceParent(dispex, &pUnk);
    ok(hres == E_NOTIMPL, "expected E_NOTIMPL got %08x\n", hres);
    if(hres == S_OK && pUnk)
        IUnknown_Release(pUnk);

    IDispatchEx_Release(dispex);
}

static int get_node_position(IXMLDOMNode *node)
{
    HRESULT r;
    int pos = 0;

    IXMLDOMNode_AddRef(node);
    do
    {
        IXMLDOMNode *new_node;

        pos++;
        r = IXMLDOMNode_get_previousSibling(node, &new_node);
        ok(SUCCEEDED(r), "get_previousSibling failed\n");
        IXMLDOMNode_Release(node);
        node = new_node;
    } while (r == S_OK);
    return pos;
}

static void node_to_string(IXMLDOMNode *node, char *buf)
{
    HRESULT r = S_OK;
    DOMNodeType type;

    if (node == NULL)
    {
        lstrcpyA(buf, "(null)");
        return;
    }

    IXMLDOMNode_AddRef(node);
    while (r == S_OK)
    {
        IXMLDOMNode *new_node;

        ole_check(IXMLDOMNode_get_nodeType(node, &type));
        get_str_for_type(type, buf);
        buf+=strlen(buf);

        if (type == NODE_ATTRIBUTE)
        {
            BSTR bstr;
            ole_check(IXMLDOMNode_get_nodeName(node, &bstr));
            *(buf++) = '\'';
            wsprintfA(buf, "%ws", bstr);
            buf += strlen(buf);
            *(buf++) = '\'';
            SysFreeString(bstr);

            r = IXMLDOMNode_selectSingleNode(node, _bstr_(".."), &new_node);
        }
        else
        {
            r = IXMLDOMNode_get_parentNode(node, &new_node);
            wsprintf(buf, "%d", get_node_position(node));
            buf += strlen(buf);
        }

        ok(SUCCEEDED(r), "get_parentNode failed (%08x)\n", r);
        IXMLDOMNode_Release(node);
        node = new_node;
        if (r == S_OK)
            *(buf++) = '.';
    }

    *buf = 0;
}

static char *list_to_string(IXMLDOMNodeList *list)
{
    static char buf[4096];
    char *pos = buf;
    LONG len = 0;
    int i;

    if (list == NULL)
    {
        lstrcpyA(buf, "(null)");
        return buf;
    }
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    for (i = 0; i < len; i++)
    {
        IXMLDOMNode *node;
        if (i > 0)
            *(pos++) = ' ';
        ole_check(IXMLDOMNodeList_nextNode(list, &node));
        node_to_string(node, pos);
        pos += strlen(pos);
        IXMLDOMNode_Release(node);
    }
    *pos = 0;
    return buf;
}

#define expect_node(node, expstr) { char str[4096]; node_to_string(node, str); ok(strcmp(str, expstr)==0, "Invalid node: %s, expected %s\n", str, expstr); }
#define expect_list_and_release(list, expstr) { char *str = list_to_string(list); ok(strcmp(str, expstr)==0, "Invalid node list: %s, expected %s\n", str, expstr); if (list) IXMLDOMNodeList_Release(list); }

static void test_domdoc( void )
{
    HRESULT r;
    IXMLDOMDocument *doc;
    IXMLDOMParseError *error;
    IXMLDOMElement *element = NULL;
    IXMLDOMNode *node;
    IXMLDOMText *nodetext = NULL;
    IXMLDOMComment *node_comment = NULL;
    IXMLDOMAttribute *node_attr = NULL;
    IXMLDOMNode *nodeChild = NULL;
    IXMLDOMProcessingInstruction *nodePI = NULL;
    ISupportErrorInfo *support_error = NULL;
    VARIANT_BOOL b;
    VARIANT var;
    BSTR str;
    LONG code;
    LONG nLength = 0;
    WCHAR buff[100];

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    test_disp((IUnknown*)doc);

if (0)
{
    /* crashes on native */
    r = IXMLDOMDocument_loadXML( doc, (BSTR)0x1, NULL );
}

    /* try some stupid things */
    r = IXMLDOMDocument_loadXML( doc, NULL, NULL );
    ok( r == S_FALSE, "loadXML succeeded\n");

    b = VARIANT_TRUE;
    r = IXMLDOMDocument_loadXML( doc, NULL, &b );
    ok( r == S_FALSE, "loadXML succeeded\n");
    ok( b == VARIANT_FALSE, "failed to load XML string\n");

    /* try to load a document from a nonexistent file */
    b = VARIANT_TRUE;
    str = SysAllocString( szNonExistentFile );
    VariantInit(&var);
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = str;

    r = IXMLDOMDocument_load( doc, var, &b);
    ok( r == S_FALSE, "loadXML succeeded\n");
    ok( b == VARIANT_FALSE, "succeeded in loading XML string\n");
    SysFreeString( str );

    /* try load an empty document */
    b = VARIANT_TRUE;
    str = SysAllocString( szEmpty );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_FALSE, "loadXML succeeded\n");
    ok( b == VARIANT_FALSE, "succeeded in loading XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_async( doc, &b );
    ok( r == S_OK, "get_async failed (%08x)\n", r);
    ok( b == VARIANT_TRUE, "Wrong default value\n");

    /* check that there's no document element */
    element = NULL;
    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_FALSE, "should be no document element\n");

    /* try finding a node */
    node = NULL;
    str = SysAllocString( szstr1 );
    r = IXMLDOMDocument_selectSingleNode( doc, str, &node );
    ok( r == S_FALSE, "ret %08x\n", r );
    SysFreeString( str );

    b = VARIANT_TRUE;
    str = SysAllocString( szIncomplete );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_FALSE, "loadXML succeeded\n");
    ok( b == VARIANT_FALSE, "succeeded in loading XML string\n");
    SysFreeString( str );

    /* check that there's no document element */
    element = (IXMLDOMElement*)1;
    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_FALSE, "should be no document element\n");
    ok( element == NULL, "Element should be NULL\n");

    /* test for BSTR handling, pass broken BSTR */
    memcpy(&buff[2], szComplete1, sizeof(szComplete1));
    /* just a big length */
    *(DWORD*)buff = 0xf0f0;
    b = VARIANT_FALSE;
    r = IXMLDOMDocument_loadXML( doc, &buff[2], &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");

    /* loadXML ignores the encoding attribute and always expects Unicode */
    b = VARIANT_FALSE;
    str = SysAllocString( szComplete6 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    /* try a BSTR containing a Windows-1252 document */
    b = VARIANT_TRUE;
    str = SysAllocStringByteLen( szNonUnicodeXML, sizeof(szNonUnicodeXML) - 1 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_FALSE, "loadXML succeeded\n");
    ok( b == VARIANT_FALSE, "succeeded in loading XML string\n");
    SysFreeString( str );

    /* try to load something valid */
    b = VARIANT_FALSE;
    str = SysAllocString( szComplete1 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    /* check if nodename is correct */
    r = IXMLDOMDocument_get_nodeName( doc, NULL );
    ok ( r == E_INVALIDARG, "get_nodeName (NULL) wrong code\n");

    str = (BSTR)0xdeadbeef;
    r = IXMLDOMDocument_get_baseName( doc, &str );
    ok ( r == S_FALSE, "got 0x%08x\n", r);
    ok (str == NULL, "got %p\n", str);

    /* content doesn't matter here */
    str = NULL;
    r = IXMLDOMDocument_get_nodeName( doc, &str );
    ok ( r == S_OK, "get_nodeName wrong code\n");
    ok ( str != NULL, "str is null\n");
    ok( !lstrcmpW( str, szDocument ), "incorrect nodeName\n");
    SysFreeString( str );

    /* test put_text */
    r = IXMLDOMDocument_put_text( doc, _bstr_("Should Fail") );
    ok( r == E_FAIL, "ret %08x\n", r );

    /* check that there's a document element */
    element = NULL;
    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "should be a document element\n");
    if( element )
    {
        IObjectIdentity *ident;
        BSTR tag = NULL;

        test_disp((IUnknown*)element);

        r = IXMLDOMElement_QueryInterface( element, &IID_IObjectIdentity, (void**)&ident );
        ok( r == E_NOINTERFACE, "ret %08x\n", r);

        r = IXMLDOMElement_get_tagName( element, NULL );
        ok( r == E_INVALIDARG, "ret %08x\n", r);

        /* check if the tag is correct */
        r = IXMLDOMElement_get_tagName( element, &tag );
        ok( r == S_OK, "couldn't get tag name\n");
        ok( tag != NULL, "tag was null\n");
        ok( !lstrcmpW( tag, szOpen ), "incorrect tag name\n");
        SysFreeString( tag );

        /* figure out what happens if we try to reload the document */
        str = SysAllocString( szComplete2 );
        r = IXMLDOMDocument_loadXML( doc, str, &b );
        ok( r == S_OK, "loadXML failed\n");
        ok( b == VARIANT_TRUE, "failed to load XML string\n");
        SysFreeString( str );

        /* check if the tag is still correct */
        tag = NULL;
        r = IXMLDOMElement_get_tagName( element, &tag );
        ok( r == S_OK, "couldn't get tag name\n");
        ok( tag != NULL, "tag was null\n");
        ok( !lstrcmpW( tag, szOpen ), "incorrect tag name\n");
        SysFreeString( tag );

        IXMLDOMElement_Release( element );
        element = NULL;
    }

    /* as soon as we call loadXML again, the document element will disappear */
    b = 2;
    r = IXMLDOMDocument_loadXML( doc, NULL, NULL );
    ok( r == S_FALSE, "loadXML failed\n");
    ok( b == 2, "variant modified\n");
    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_FALSE, "should be no document element\n");

    /* try to load something else simple and valid */
    b = VARIANT_FALSE;
    str = SysAllocString( szComplete3 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    /* try something a little more complicated */
    b = FALSE;
    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_parseError( doc, &error );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMParseError_get_errorCode( error, &code );
    ok( r == S_FALSE, "returns %08x\n", r );
    ok( code == 0, "code %d\n", code );
    IXMLDOMParseError_Release( error );

    /* test createTextNode */
    r = IXMLDOMDocument_createTextNode(doc, _bstr_(""), &nodetext);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMText_Release(nodetext);

    str = SysAllocString( szOpen );
    r = IXMLDOMDocument_createTextNode(doc, str, NULL);
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    r = IXMLDOMDocument_createTextNode(doc, str, &nodetext);
    ok( r == S_OK, "returns %08x\n", r );
    SysFreeString( str );
    if(nodetext)
    {
        IXMLDOMNamedNodeMap *pAttribs;

        r = IXMLDOMText_QueryInterface(nodetext, &IID_IXMLDOMElement, (void**)&element);
        ok(r == E_NOINTERFACE, "ret %08x\n", r );

        /* Text Last Child Checks */
        r = IXMLDOMText_get_lastChild(nodetext, NULL);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        nodeChild = (IXMLDOMNode*)0x1;
        r = IXMLDOMText_get_lastChild(nodetext, &nodeChild);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok(nodeChild == NULL, "nodeChild not NULL\n");

        /* test get_attributes */
        r = IXMLDOMText_get_attributes( nodetext, NULL );
        ok( r == E_INVALIDARG, "get_attributes returned wrong code\n");

        pAttribs = (IXMLDOMNamedNodeMap*)0x1;
        r = IXMLDOMText_get_attributes( nodetext, &pAttribs);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok( pAttribs == NULL, "pAttribs not NULL\n");

        /* test get_dataType */
        r = IXMLDOMText_get_dataType(nodetext, &var);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok( V_VT(&var) == VT_NULL, "incorrect dataType type\n");
        VariantClear(&var);

        /* test length property */
        r = IXMLDOMText_get_length(nodetext, NULL);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        r = IXMLDOMText_get_length(nodetext, &nLength);
        ok(r == S_OK, "ret %08x\n", r );
        ok(nLength == 4, "expected 4 got %d\n", nLength);

        /* test nodeTypeString */
        r = IXMLDOMText_get_nodeTypeString(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("text") ), "incorrect nodeTypeString string\n");
        SysFreeString(str);

        /* put data Tests */
        r = IXMLDOMText_put_data(nodetext, _bstr_("This &is a ; test <>\\"));
        ok(r == S_OK, "ret %08x\n", r );

        /* get data Tests */
        r = IXMLDOMText_get_data(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\") ), "incorrect put_data string\n");
        SysFreeString(str);

        /* Confirm XML text is good */
        r = IXMLDOMText_get_xml(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This &amp;is a ; test &lt;&gt;\\") ), "incorrect xml string\n");
        SysFreeString(str);

        /* Confirm we get the put_data Text back */
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\") ), "incorrect xml string\n");
        SysFreeString(str);

        /* test substringData */
        r = IXMLDOMText_substringData(nodetext, 0, 4, NULL);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        /* test substringData - Invalid offset */
        str = (BSTR)&szElement;
        r = IXMLDOMText_substringData(nodetext, -1, 4, &str);
        ok(r == E_INVALIDARG, "ret %08x\n", r );
        ok( str == NULL, "incorrect string\n");

        /* test substringData - Invalid offset */
        str = (BSTR)&szElement;
        r = IXMLDOMText_substringData(nodetext, 30, 0, &str);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok( str == NULL, "incorrect string\n");

        /* test substringData - Invalid size */
        str = (BSTR)&szElement;
        r = IXMLDOMText_substringData(nodetext, 0, -1, &str);
        ok(r == E_INVALIDARG, "ret %08x\n", r );
        ok( str == NULL, "incorrect string\n");

        /* test substringData - Invalid size */
        str = (BSTR)&szElement;
        r = IXMLDOMText_substringData(nodetext, 2, 0, &str);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok( str == NULL, "incorrect string\n");

        /* test substringData - Start of string */
        r = IXMLDOMText_substringData(nodetext, 0, 4, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This") ), "incorrect substringData string\n");
        SysFreeString(str);

        /* test substringData - Middle of string */
        r = IXMLDOMText_substringData(nodetext, 13, 4, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("test") ), "incorrect substringData string\n");
        SysFreeString(str);

        /* test substringData - End of string */
        r = IXMLDOMText_substringData(nodetext, 20, 4, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("\\") ), "incorrect substringData string\n");
        SysFreeString(str);

        /* test appendData */
        r = IXMLDOMText_appendData(nodetext, NULL);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_appendData(nodetext, _bstr_(""));
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_appendData(nodetext, _bstr_("Append"));
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\Append") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* test insertData */
        str = SysAllocStringLen(NULL, 0);
        r = IXMLDOMText_insertData(nodetext, -1, str);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, -1, NULL);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 1000, str);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 1000, NULL);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 0, NULL);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 0, str);
        ok(r == S_OK, "ret %08x\n", r );
        SysFreeString(str);

        r = IXMLDOMText_insertData(nodetext, -1, _bstr_("Inserting"));
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 1000, _bstr_("Inserting"));
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 0, _bstr_("Begin "));
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 17, _bstr_("Middle"));
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 39, _bstr_(" End"));
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("Begin This &is a Middle; test <>\\Append End") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* delete data */
        /* invalid arguments */
        r = IXMLDOMText_deleteData(nodetext, -1, 1);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        r = IXMLDOMText_deleteData(nodetext, 0, 0);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_deleteData(nodetext, 0, -1);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        r = IXMLDOMText_get_length(nodetext, &nLength);
        ok(r == S_OK, "ret %08x\n", r );
        ok(nLength == 43, "expected 43 got %d\n", nLength);

        r = IXMLDOMText_deleteData(nodetext, nLength, 1);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_deleteData(nodetext, nLength+1, 1);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        /* delete from start */
        r = IXMLDOMText_deleteData(nodetext, 0, 5);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_get_length(nodetext, &nLength);
        ok(r == S_OK, "ret %08x\n", r );
        ok(nLength == 38, "expected 38 got %d\n", nLength);

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This &is a Middle; test <>\\Append End") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* delete from end */
        r = IXMLDOMText_deleteData(nodetext, 35, 3);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_get_length(nodetext, &nLength);
        ok(r == S_OK, "ret %08x\n", r );
        ok(nLength == 35, "expected 35 got %d\n", nLength);

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("This &is a Middle; test <>\\Append") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* delete from inside */
        r = IXMLDOMText_deleteData(nodetext, 1, 33);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_get_length(nodetext, &nLength);
        ok(r == S_OK, "ret %08x\n", r );
        ok(nLength == 2, "expected 2 got %d\n", nLength);

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* delete whole data ... */
        r = IXMLDOMText_get_length(nodetext, &nLength);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_deleteData(nodetext, 0, nLength);
        ok(r == S_OK, "ret %08x\n", r );
        /* ... and try again with empty string */
        r = IXMLDOMText_deleteData(nodetext, 0, nLength);
        ok(r == S_OK, "ret %08x\n", r );

        /* test put_data */
        V_VT(&var) = VT_BSTR;
        V_BSTR(&var) = SysAllocString(szstr1);
        r = IXMLDOMText_put_nodeValue(nodetext, var);
        ok(r == S_OK, "ret %08x\n", r );
        VariantClear(&var);

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, szstr1 ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* test put_data */
        V_VT(&var) = VT_I4;
        V_I4(&var) = 99;
        r = IXMLDOMText_put_nodeValue(nodetext, var);
        ok(r == S_OK, "ret %08x\n", r );
        VariantClear(&var);

        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("99") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* ::replaceData() */
        V_VT(&var) = VT_BSTR;
        V_BSTR(&var) = SysAllocString(szstr1);
        r = IXMLDOMText_put_nodeValue(nodetext, var);
        ok(r == S_OK, "ret %08x\n", r );
        VariantClear(&var);

        r = IXMLDOMText_replaceData(nodetext, 6, 0, NULL);
        ok(r == E_INVALIDARG, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("str1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        r = IXMLDOMText_replaceData(nodetext, 0, 0, NULL);
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("str1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* NULL pointer means delete */
        r = IXMLDOMText_replaceData(nodetext, 0, 1, NULL);
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("tr1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* empty string means delete */
        r = IXMLDOMText_replaceData(nodetext, 0, 1, _bstr_(""));
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("r1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* zero count means insert */
        r = IXMLDOMText_replaceData(nodetext, 0, 0, _bstr_("a"));
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("ar1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        r = IXMLDOMText_replaceData(nodetext, 0, 2, NULL);
        ok(r == S_OK, "ret %08x\n", r );

        r = IXMLDOMText_insertData(nodetext, 0, _bstr_("m"));
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("m1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* nonempty string, count greater than its length */
        r = IXMLDOMText_replaceData(nodetext, 0, 2, _bstr_("a1.2"));
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("a1.2") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        /* nonempty string, count less than its length */
        r = IXMLDOMText_replaceData(nodetext, 0, 1, _bstr_("wine"));
        ok(r == S_OK, "ret %08x\n", r );
        r = IXMLDOMText_get_text(nodetext, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("wine1.2") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
        SysFreeString(str);

        IXMLDOMText_Release( nodetext );
    }

    /* test Create Comment */
    r = IXMLDOMDocument_createComment(doc, NULL, NULL);
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    node_comment = (IXMLDOMComment*)0x1;

    /* empty comment */
    r = IXMLDOMDocument_createComment(doc, _bstr_(""), &node_comment);
    ok( r == S_OK, "returns %08x\n", r );
    str = (BSTR)0x1;
    r = IXMLDOMComment_get_data(node_comment, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty string data\n");
    IXMLDOMComment_Release(node_comment);
    SysFreeString(str);

    r = IXMLDOMDocument_createComment(doc, NULL, &node_comment);
    ok( r == S_OK, "returns %08x\n", r );
    str = (BSTR)0x1;
    r = IXMLDOMComment_get_data(node_comment, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && (SysStringLen(str) == 0), "expected empty string data\n");
    IXMLDOMComment_Release(node_comment);
    SysFreeString(str);

    str = SysAllocString(szComment);
    r = IXMLDOMDocument_createComment(doc, str, &node_comment);
    SysFreeString(str);
    ok( r == S_OK, "returns %08x\n", r );
    if(node_comment)
    {
        /* Last Child Checks */
        r = IXMLDOMComment_get_lastChild(node_comment, NULL);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        nodeChild = (IXMLDOMNode*)0x1;
        r = IXMLDOMComment_get_lastChild(node_comment, &nodeChild);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok(nodeChild == NULL, "pLastChild not NULL\n");

        /* baseName */
        str = (BSTR)0xdeadbeef;
        IXMLDOMComment_get_baseName(node_comment, &str);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok(str == NULL, "Expected NULL\n");

        IXMLDOMComment_Release( node_comment );
    }

    /* test Create Attribute */
    str = SysAllocString(szAttribute);
    r = IXMLDOMDocument_createAttribute(doc, NULL, NULL);
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    r = IXMLDOMDocument_createAttribute(doc, str, &node_attr);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMText_Release( node_attr);
    SysFreeString(str);

    /* test Processing Instruction */
    str = SysAllocStringLen(NULL, 0);
    r = IXMLDOMDocument_createProcessingInstruction(doc, str, str, NULL);
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    r = IXMLDOMDocument_createProcessingInstruction(doc, NULL, str, &nodePI);
    ok( r == E_FAIL, "returns %08x\n", r );
    r = IXMLDOMDocument_createProcessingInstruction(doc, str, str, &nodePI);
    ok( r == E_FAIL, "returns %08x\n", r );
    SysFreeString(str);

    r = IXMLDOMDocument_createProcessingInstruction(doc, _bstr_("xml"), _bstr_("version=\"1.0\""), &nodePI);
    ok( r == S_OK, "returns %08x\n", r );
    if(nodePI)
    {
        /* Last Child Checks */
        r = IXMLDOMProcessingInstruction_get_lastChild(nodePI, NULL);
        ok(r == E_INVALIDARG, "ret %08x\n", r );

        nodeChild = (IXMLDOMNode*)0x1;
        r = IXMLDOMProcessingInstruction_get_lastChild(nodePI, &nodeChild);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok(nodeChild == NULL, "nodeChild not NULL\n");

        r = IXMLDOMProcessingInstruction_get_dataType(nodePI, &var);
        ok(r == S_FALSE, "ret %08x\n", r );
        ok( V_VT(&var) == VT_NULL, "incorrect dataType type\n");
        VariantClear(&var);

        /* test nodeName */
        r = IXMLDOMProcessingInstruction_get_nodeName(nodePI, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("xml") ), "incorrect nodeName string\n");
        SysFreeString(str);

        /* test baseName */
        str = (BSTR)0x1;
        r = IXMLDOMProcessingInstruction_get_baseName(nodePI, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("xml") ), "incorrect nodeName string\n");
        SysFreeString(str);

        /* test Target */
        r = IXMLDOMProcessingInstruction_get_target(nodePI, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("xml") ), "incorrect target string\n");
        SysFreeString(str);

        /* test nodeTypeString */
        r = IXMLDOMProcessingInstruction_get_nodeTypeString(nodePI, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("processinginstruction") ), "incorrect nodeTypeString string\n");
        SysFreeString(str);

        /* test get_nodeValue */
        r = IXMLDOMProcessingInstruction_get_nodeValue(nodePI, &var);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( V_BSTR(&var), _bstr_("version=\"1.0\"") ), "incorrect data string\n");
        VariantClear(&var);

        /* test get_data */
        r = IXMLDOMProcessingInstruction_get_data(nodePI, &str);
        ok(r == S_OK, "ret %08x\n", r );
        ok( !lstrcmpW( str, _bstr_("version=\"1.0\"") ), "incorrect data string\n");
        SysFreeString(str);

        /* test put_data */
        r = IXMLDOMProcessingInstruction_put_data(nodePI, _bstr_("version=\"1.0\" encoding=\"UTF-8\""));
        ok(r == E_FAIL, "ret %08x\n", r );

        /* test put_data */
        V_VT(&var) = VT_BSTR;
        V_BSTR(&var) = SysAllocString(szOpen);  /* Doesn't matter what the string is, cannot set an xml node. */
        r = IXMLDOMProcessingInstruction_put_nodeValue(nodePI, var);
        ok(r == E_FAIL, "ret %08x\n", r );
        VariantClear(&var);

        /* test get nodeName */
        r = IXMLDOMProcessingInstruction_get_nodeName(nodePI, &str);
        ok( !lstrcmpW( str, _bstr_("xml") ), "incorrect nodeName string\n");
        ok(r == S_OK, "ret %08x\n", r );
        SysFreeString(str);

        IXMLDOMProcessingInstruction_Release(nodePI);
    }

    r = IXMLDOMDocument_QueryInterface( doc, &IID_ISupportErrorInfo, (void**)&support_error );
    ok( r == S_OK, "ret %08x\n", r );
    if(r == S_OK)
    {
        r = ISupportErrorInfo_InterfaceSupportsErrorInfo( support_error, &IID_IXMLDOMDocument );
        todo_wine ok( r == S_OK, "ret %08x\n", r );
        ISupportErrorInfo_Release( support_error );
    }

    r = IXMLDOMDocument_Release( doc );
    ok( r == 0, "document ref count incorrect\n");

    free_bstrs();
}

static void test_persiststreaminit(void)
{
    IXMLDOMDocument *doc;
    IPersistStreamInit *streaminit;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&streaminit);
    ok( hr == S_OK, "failed with 0x%08x\n", hr );

    hr = IPersistStreamInit_InitNew(streaminit);
    ok( hr == S_OK, "failed with 0x%08x\n", hr );

    IXMLDOMDocument_Release(doc);
}

static void test_domnode( void )
{
    HRESULT r;
    IXMLDOMDocument *doc, *owner = NULL;
    IXMLDOMElement *element = NULL;
    IXMLDOMNamedNodeMap *map = NULL;
    IXMLDOMNode *node = NULL, *next = NULL;
    IXMLDOMNodeList *list = NULL;
    IXMLDOMAttribute *attr = NULL;
    DOMNodeType type = NODE_INVALID;
    VARIANT_BOOL b;
    BSTR str;
    VARIANT var;
    LONG count;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    b = FALSE;
    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    b = 1;
    r = IXMLDOMNode_hasChildNodes( doc, &b );
    ok( r == S_OK, "hasChildNoes bad return\n");
    ok( b == VARIANT_TRUE, "hasChildNoes wrong result\n");

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "should be a document element\n");
    ok( element != NULL, "should be an element\n");

    VariantInit(&var);
    ok( V_VT(&var) == VT_EMPTY, "variant init failed\n");

    r = IXMLDOMNode_get_nodeValue( doc, NULL );
    ok(r == E_INVALIDARG, "get_nodeValue ret %08x\n", r );

    r = IXMLDOMNode_get_nodeValue( doc, &var );
    ok( r == S_FALSE, "nextNode returned wrong code\n");
    ok( V_VT(&var) == VT_NULL, "variant wasn't empty\n");
    ok( V_BSTR(&var) == NULL, "variant value wasn't null\n");

    if (element)
    {
        owner = NULL;
        r = IXMLDOMNode_get_ownerDocument( element, &owner );
        ok( r == S_OK, "get_ownerDocument return code\n");
        ok( owner != doc, "get_ownerDocument return\n");
        IXMLDOMDocument_Release(owner);

        type = NODE_INVALID;
        r = IXMLDOMNode_get_nodeType( element, &type);
        ok( r == S_OK, "got %08x\n", r);
        ok( type == NODE_ELEMENT, "node not an element\n");

        str = NULL;
        r = IXMLDOMNode_get_baseName( element, &str );
        ok( r == S_OK, "get_baseName returned wrong code\n");
        ok( lstrcmpW(str,szlc) == 0, "basename was wrong\n");
        SysFreeString(str);

        /* check if nodename is correct */
        r = IXMLDOMElement_get_nodeName( element, NULL );
        ok ( r == E_INVALIDARG, "get_nodeName (NULL) wrong code\n");

        /* content doesn't matter here */
        str = NULL;
        r = IXMLDOMElement_get_nodeName( element, &str );
        ok ( r == S_OK, "get_nodeName wrong code\n");
        ok ( str != NULL, "str is null\n");
        ok( !lstrcmpW( str, szlc ), "incorrect nodeName\n");
        SysFreeString( str );

        str = SysAllocString( szNonExistentFile );	
        V_VT(&var) = VT_I4;
        V_I4(&var) = 0x1234;
        r = IXMLDOMElement_getAttribute( element, str, &var );
        ok( r == E_FAIL, "getAttribute ret %08x\n", r );
        ok( V_VT(&var) == VT_NULL || V_VT(&var) == VT_EMPTY, "vt = %x\n", V_VT(&var));
        VariantClear(&var);

        r = IXMLDOMElement_getAttributeNode( element, str, NULL);
        ok( r == E_FAIL, "getAttributeNode ret %08x\n", r );

        attr = (IXMLDOMAttribute*)0xdeadbeef;
        r = IXMLDOMElement_getAttributeNode( element, str, &attr);
        ok( r == E_FAIL, "getAttributeNode ret %08x\n", r );
        ok( attr == NULL, "getAttributeNode ret %p, expected NULL\n", attr );
        SysFreeString( str );

        attr = (IXMLDOMAttribute*)0xdeadbeef;
        str = SysAllocString( szNonExistentAttribute );
        r = IXMLDOMElement_getAttributeNode( element, str, &attr);
        ok( r == S_FALSE, "getAttributeNode ret %08x\n", r );
        ok( attr == NULL, "getAttributeNode ret %p, expected NULL\n", attr );
        SysFreeString( str );

        str = SysAllocString( szdl );	
        V_VT(&var) = VT_I4;
        V_I4(&var) = 0x1234;
        r = IXMLDOMElement_getAttribute( element, str, &var );
        ok( r == S_OK, "getAttribute ret %08x\n", r );
        ok( V_VT(&var) == VT_BSTR, "vt = %x\n", V_VT(&var));
        ok( !lstrcmpW(V_BSTR(&var), szstr1), "wrong attr value\n");
        VariantClear( &var );

        r = IXMLDOMElement_getAttribute( element, NULL, &var );
        ok( r == E_INVALIDARG, "getAttribute ret %08x\n", r );

        r = IXMLDOMElement_getAttribute( element, str, NULL );
        ok( r == E_INVALIDARG, "getAttribute ret %08x\n", r );

        attr = NULL;
        r = IXMLDOMElement_getAttributeNode( element, str, &attr);
        ok( r == S_OK, "GetAttributeNode ret %08x\n", r );
        ok( attr != NULL, "getAttributeNode returned NULL\n" );
        if (attr)
        {
            r = IXMLDOMAttribute_get_parentNode( attr, NULL );
            ok( r == E_INVALIDARG, "Expected E_INVALIDARG, ret %08x\n", r );

            /* attribute doesn't have a parent in msxml interpretation */
            node = (IXMLDOMNode*)0xdeadbeef;
            r = IXMLDOMAttribute_get_parentNode( attr, &node );
            ok( r == S_FALSE, "Expected S_FALSE, ret %08x\n", r );
            ok( node == NULL, "Expected NULL, got %p\n", node );

            IXMLDOMAttribute_Release(attr);
        }

        SysFreeString( str );

        r = IXMLDOMElement_get_attributes( element, &map );
        ok( r == S_OK, "get_attributes returned wrong code\n");
        ok( map != NULL, "should be attributes\n");

        b = 1;
        r = IXMLDOMNode_hasChildNodes( element, &b );
        ok( r == S_OK, "hasChildNoes bad return\n");
        ok( b == VARIANT_TRUE, "hasChildNoes wrong result\n");
    }
    else
        ok( FALSE, "no element\n");

    if (map)
    {
        ISupportErrorInfo *support_error;
        r = IXMLDOMNamedNodeMap_QueryInterface( map, &IID_ISupportErrorInfo, (void**)&support_error );
        ok( r == S_OK, "ret %08x\n", r );

        r = ISupportErrorInfo_InterfaceSupportsErrorInfo( support_error, &IID_IXMLDOMNamedNodeMap );
todo_wine
{
        ok( r == S_OK, "ret %08x\n", r );
}
        ISupportErrorInfo_Release( support_error );

        str = SysAllocString( szdl );
        r = IXMLDOMNamedNodeMap_getNamedItem( map, str, &node );
        ok( r == S_OK, "getNamedItem returned wrong code\n");
        ok( node != NULL, "should be attributes\n");
        IXMLDOMNode_Release(node);
        SysFreeString( str );

        str = SysAllocString( szdl );
        r = IXMLDOMNamedNodeMap_getNamedItem( map, str, NULL );
        ok( r == E_INVALIDARG, "getNamedItem should return E_INVALIDARG\n");
        SysFreeString( str );

        /* something that isn't in szComplete4 */
        str = SysAllocString( szOpen );
        node = (IXMLDOMNode *) 1;
        r = IXMLDOMNamedNodeMap_getNamedItem( map, str, &node );
        ok( r == S_FALSE, "getNamedItem found a node that wasn't there\n");
        ok( node == NULL, "getNamedItem should have returned NULL\n");
        SysFreeString( str );

	/* test indexed access of attributes */
        r = IXMLDOMNamedNodeMap_get_length( map, NULL );
        ok ( r == E_INVALIDARG, "get_length should return E_INVALIDARG\n");

        r = IXMLDOMNamedNodeMap_get_length( map, &count );
        ok ( r == S_OK, "get_length wrong code\n");
        ok ( count == 1, "get_length != 1\n");

        node = NULL;
        r = IXMLDOMNamedNodeMap_get_item( map, -1, &node);
        ok ( r == S_FALSE, "get_item (-1) wrong code\n");
        ok ( node == NULL, "there is no node\n");

        node = NULL;
        r = IXMLDOMNamedNodeMap_get_item( map, 1, &node);
        ok ( r == S_FALSE, "get_item (1) wrong code\n");
        ok ( node == NULL, "there is no attribute\n");

        node = NULL;
        r = IXMLDOMNamedNodeMap_get_item( map, 0, &node);
        ok ( r == S_OK, "get_item (0) wrong code\n");
        ok ( node != NULL, "should be attribute\n");

        r = IXMLDOMNode_get_nodeName( node, NULL );
        ok ( r == E_INVALIDARG, "get_nodeName (NULL) wrong code\n");

        /* content doesn't matter here */
        str = NULL;
        r = IXMLDOMNode_get_nodeName( node, &str );
        ok ( r == S_OK, "get_nodeName wrong code\n");
        ok ( str != NULL, "str is null\n");
        ok( !lstrcmpW( str, szdl ), "incorrect node name\n");
        SysFreeString( str );

        /* test sequential access of attributes */
        node = NULL;
        r = IXMLDOMNamedNodeMap_nextNode( map, &node );
        ok ( r == S_OK, "nextNode (first time) wrong code\n");
        ok ( node != NULL, "nextNode, should be attribute\n");

        r = IXMLDOMNamedNodeMap_nextNode( map, &node );
        ok ( r != S_OK, "nextNode (second time) wrong code\n");
        ok ( node == NULL, "nextNode, there is no attribute\n");

        r = IXMLDOMNamedNodeMap_reset( map );
        ok ( r == S_OK, "reset should return S_OK\n");

        r = IXMLDOMNamedNodeMap_nextNode( map, &node );
        ok ( r == S_OK, "nextNode (third time) wrong code\n");
        ok ( node != NULL, "nextNode, should be attribute\n");
    }
    else
        ok( FALSE, "no map\n");

    if (node)
    {
        type = NODE_INVALID;
        r = IXMLDOMNode_get_nodeType( node, &type);
        ok( r == S_OK, "getNamedItem returned wrong code\n");
        ok( type == NODE_ATTRIBUTE, "node not an attribute\n");

        str = NULL;
        r = IXMLDOMNode_get_baseName( node, NULL );
        ok( r == E_INVALIDARG, "get_baseName returned wrong code\n");

        str = NULL;
        r = IXMLDOMNode_get_baseName( node, &str );
        ok( r == S_OK, "get_baseName returned wrong code\n");
        ok( lstrcmpW(str,szdl) == 0, "basename was wrong\n");
        SysFreeString( str );

        r = IXMLDOMNode_get_nodeValue( node, &var );
        ok( r == S_OK, "returns %08x\n", r );
        ok( V_VT(&var) == VT_BSTR, "vt %x\n", V_VT(&var));
        ok( !lstrcmpW(V_BSTR(&var), szstr1), "nodeValue incorrect\n");
        VariantClear(&var);

        r = IXMLDOMNode_get_childNodes( node, NULL );
        ok( r == E_INVALIDARG, "get_childNodes returned wrong code\n");

        r = IXMLDOMNode_get_childNodes( node, &list );
        ok( r == S_OK, "get_childNodes returned wrong code\n");

        if (list)
        {
            r = IXMLDOMNodeList_nextNode( list, &next );
            ok( r == S_OK, "nextNode returned wrong code\n");
        }
        else
            ok( FALSE, "no childlist\n");

        if (next)
        {
            b = 1;
            r = IXMLDOMNode_hasChildNodes( next, &b );
            ok( r == S_FALSE, "hasChildNoes bad return\n");
            ok( b == VARIANT_FALSE, "hasChildNoes wrong result\n");

            type = NODE_INVALID;
            r = IXMLDOMNode_get_nodeType( next, &type);
            ok( r == S_OK, "getNamedItem returned wrong code\n");
            ok( type == NODE_TEXT, "node not text\n");

            str = (BSTR) 1;
            r = IXMLDOMNode_get_baseName( next, &str );
            ok( r == S_FALSE, "get_baseName returned wrong code\n");
            ok( str == NULL, "basename was wrong\n");
            SysFreeString(str);
        }
        else
            ok( FALSE, "no next\n");

        if (next)
            IXMLDOMNode_Release( next );
        next = NULL;
        if (list)
            IXMLDOMNodeList_Release( list );
        list = NULL;
        if (node)
            IXMLDOMNode_Release( node );
    }
    else
        ok( FALSE, "no node\n");
    node = NULL;

    if (map)
        IXMLDOMNamedNodeMap_Release( map );

    /* now traverse the tree from the root element */
    if (element)
    {
        r = IXMLDOMNode_get_childNodes( element, &list );
        ok( r == S_OK, "get_childNodes returned wrong code\n");

        /* using get_item for child list doesn't advance the position */
        ole_check(IXMLDOMNodeList_get_item(list, 1, &node));
        expect_node(node, "E2.E2.D1");
        IXMLDOMNode_Release(node);
        ole_check(IXMLDOMNodeList_nextNode(list, &node));
        expect_node(node, "E1.E2.D1");
        IXMLDOMNode_Release(node);
        ole_check(IXMLDOMNodeList_reset(list));

        IXMLDOMNodeList_AddRef(list);
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1 E3.E2.D1 E4.E2.D1");
        ole_check(IXMLDOMNodeList_reset(list));

        node = (void*)0xdeadbeef;
        str = SysAllocString(szdl);
        r = IXMLDOMNode_selectSingleNode( element, str, &node );
        SysFreeString(str);
        ok( r == S_FALSE, "ret %08x\n", r );
        ok( node == NULL, "node %p\n", node );

        str = SysAllocString(szbs);
        r = IXMLDOMNode_selectSingleNode( element, str, &node );
        SysFreeString(str);
        ok( r == S_OK, "ret %08x\n", r );
        r = IXMLDOMNode_Release( node );
        ok( r == 0, "ret %08x\n", r );
    }
    else
        ok( FALSE, "no element\n");

    if (list)
    {
        r = IXMLDOMNodeList_QueryInterface(list, &IID_IDispatch, NULL);
        ok( r == E_INVALIDARG || r == E_POINTER, "ret %08x\n", r );

        r = IXMLDOMNodeList_get_item(list, 0, NULL);
        ok(r == E_INVALIDARG, "Exected E_INVALIDARG got %08x\n", r);

        r = IXMLDOMNodeList_get_length(list, NULL);
        ok(r == E_INVALIDARG, "Exected E_INVALIDARG got %08x\n", r);

        r = IXMLDOMNodeList_get_length( list, &count );
        ok( r == S_OK, "get_length returns %08x\n", r );
        ok( count == 4, "get_length got %d\n", count );

        r = IXMLDOMNodeList_nextNode(list, NULL);
        ok(r == E_INVALIDARG, "Exected E_INVALIDARG got %08x\n", r);

        r = IXMLDOMNodeList_nextNode( list, &node );
        ok( r == S_OK, "nextNode returned wrong code\n");
    }
    else
        ok( FALSE, "no list\n");

    if (node)
    {
        type = NODE_INVALID;
        r = IXMLDOMNode_get_nodeType( node, &type);
        ok( r == S_OK, "getNamedItem returned wrong code\n");
        ok( type == NODE_ELEMENT, "node not text\n");

        VariantInit(&var);
        ok( V_VT(&var) == VT_EMPTY, "variant init failed\n");
        r = IXMLDOMNode_get_nodeValue( node, &var );
        ok( r == S_FALSE, "nextNode returned wrong code\n");
        ok( V_VT(&var) == VT_NULL, "variant wasn't empty\n");
        ok( V_BSTR(&var) == NULL, "variant value wasn't null\n");

        r = IXMLDOMNode_hasChildNodes( node, NULL );
        ok( r == E_INVALIDARG, "hasChildNoes bad return\n");

        b = 1;
        r = IXMLDOMNode_hasChildNodes( node, &b );
        ok( r == S_OK, "hasChildNoes bad return\n");
        ok( b == VARIANT_TRUE, "hasChildNoes wrong result\n");

        str = NULL;
        r = IXMLDOMNode_get_baseName( node, &str );
        ok( r == S_OK, "get_baseName returned wrong code\n");
        ok( lstrcmpW(str,szbs) == 0, "basename was wrong\n");
        SysFreeString(str);
    }
    else
        ok( FALSE, "no node\n");

    if (node)
        IXMLDOMNode_Release( node );
    if (list)
        IXMLDOMNodeList_Release( list );
    if (element)
        IXMLDOMElement_Release( element );

    b = FALSE;
    str = SysAllocString( szComplete5 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    b = 1;
    r = IXMLDOMNode_hasChildNodes( doc, &b );
    ok( r == S_OK, "hasChildNoes bad return\n");
    ok( b == VARIANT_TRUE, "hasChildNoes wrong result\n");

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "should be a document element\n");
    ok( element != NULL, "should be an element\n");

    if (element)
    {
        static const WCHAR szSSearch[] = {'S',':','s','e','a','r','c','h',0};
        BSTR tag = NULL;

        /* check if the tag is correct */
        r = IXMLDOMElement_get_tagName( element, &tag );
        ok( r == S_OK, "couldn't get tag name\n");
        ok( tag != NULL, "tag was null\n");
        ok( !lstrcmpW( tag, szSSearch ), "incorrect tag name\n");
        SysFreeString( tag );
    }

    if (element)
        IXMLDOMElement_Release( element );
    ok(IXMLDOMDocument_Release( doc ) == 0, "document is not destroyed\n");
}

static void test_refs(void)
{
    HRESULT r;
    BSTR str;
    VARIANT_BOOL b;
    IXMLDOMDocument *doc;
    IXMLDOMElement *element = NULL;
    IXMLDOMNode *node = NULL, *node2;
    IXMLDOMNodeList *node_list = NULL;
    LONG ref;
    IUnknown *unk, *unk2;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    ref = IXMLDOMDocument_Release(doc);
    ok( ref == 0, "ref %d\n", ref);

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    ref = IXMLDOMDocument_AddRef( doc );
    ok( ref == 2, "ref %d\n", ref );
    ref = IXMLDOMDocument_AddRef( doc );
    ok( ref == 3, "ref %d\n", ref );
    IXMLDOMDocument_Release( doc );
    IXMLDOMDocument_Release( doc );

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "should be a document element\n");
    ok( element != NULL, "should be an element\n");

    ref = IXMLDOMDocument_AddRef( doc );
    ok( ref == 2, "ref %d\n", ref );
    IXMLDOMDocument_Release( doc );

    r = IXMLDOMElement_get_childNodes( element, &node_list );
    ok( r == S_OK, "rets %08x\n", r);

    ref = IXMLDOMNodeList_AddRef( node_list );
    ok( ref == 2, "ref %d\n", ref );
    IXMLDOMNodeList_Release( node_list );

    IXMLDOMNodeList_get_item( node_list, 0, &node );
    ok( r == S_OK, "rets %08x\n", r);

    IXMLDOMNodeList_get_item( node_list, 0, &node2 );
    ok( r == S_OK, "rets %08x\n", r);

    ref = IXMLDOMNode_AddRef( node );
    ok( ref == 2, "ref %d\n", ref );
    IXMLDOMNode_Release( node );

    ref = IXMLDOMNode_Release( node );
    ok( ref == 0, "ref %d\n", ref );
    ref = IXMLDOMNode_Release( node2 );
    ok( ref == 0, "ref %d\n", ref );

    ref = IXMLDOMNodeList_Release( node_list );
    ok( ref == 0, "ref %d\n", ref );

    ok( node != node2, "node %p node2 %p\n", node, node2 );

    ref = IXMLDOMDocument_Release( doc );
    ok( ref == 0, "ref %d\n", ref );

    ref = IXMLDOMElement_AddRef( element );
    todo_wine {
    ok( ref == 3, "ref %d\n", ref );
    }
    IXMLDOMElement_Release( element );

    /* IUnknown must be unique however we obtain it */
    r = IXMLDOMElement_QueryInterface( element, &IID_IUnknown, (void**)&unk );
    ok( r == S_OK, "rets %08x\n", r );
    r = IXMLDOMElement_QueryInterface( element, &IID_IXMLDOMNode, (void**)&node );
    ok( r == S_OK, "rets %08x\n", r );
    r = IXMLDOMNode_QueryInterface( node, &IID_IUnknown, (void**)&unk2 );
    ok( r == S_OK, "rets %08x\n", r );
    ok( unk == unk2, "unk %p unk2 %p\n", unk, unk2 );

    IUnknown_Release( unk2 );
    IUnknown_Release( unk );
    IXMLDOMNode_Release( node );

    IXMLDOMElement_Release( element );

}

static void test_create(void)
{
    static const WCHAR szOne[] = {'1',0};
    static const WCHAR szOneGarbage[] = {'1','G','a','r','b','a','g','e',0};
    HRESULT r;
    VARIANT var;
    BSTR str, name;
    IXMLDOMDocument *doc;
    IXMLDOMElement *element;
    IXMLDOMComment *comment;
    IXMLDOMText *text;
    IXMLDOMCDATASection *cdata;
    IXMLDOMNode *root, *node, *child;
    IXMLDOMNamedNodeMap *attr_map;
    IUnknown *unk;
    LONG ref;
    LONG num;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    /* types not supported for creation */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_DOCUMENT;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_DOCUMENT_TYPE;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ENTITY;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_NOTATION;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    /* NODE_COMMENT */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_COMMENT;
    node = NULL;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    ok( node != NULL, "\n");

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMComment, (void**)&comment);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMComment_get_data(comment, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMComment_Release(comment);
    SysFreeString(str);

    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMComment, (void**)&comment);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMComment_get_data(comment, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMComment_Release(comment);
    SysFreeString(str);

    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_("blah"), NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMComment, (void**)&comment);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMComment_get_data(comment, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMComment_Release(comment);
    SysFreeString(str);

    /* NODE_TEXT */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_TEXT;
    node = NULL;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    ok( node != NULL, "\n");

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMText, (void**)&text);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMText_get_data(text, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMText_Release(text);
    SysFreeString(str);

    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMText, (void**)&text);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMText_get_data(text, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMText_Release(text);
    SysFreeString(str);

    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_("blah"), NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMText, (void**)&text);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMText_get_data(text, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMText_Release(text);
    SysFreeString(str);

    /* NODE_CDATA_SECTION */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_CDATA_SECTION;
    node = NULL;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    ok( node != NULL, "\n");

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMCDATASection, (void**)&cdata);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMCDATASection_get_data(cdata, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMCDATASection_Release(cdata);
    SysFreeString(str);

    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMCDATASection, (void**)&cdata);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMCDATASection_get_data(cdata, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMCDATASection_Release(cdata);
    SysFreeString(str);

    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_("blah"), NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMCDATASection, (void**)&cdata);
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release(node);

    str = NULL;
    r = IXMLDOMCDATASection_get_data(cdata, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( str && SysStringLen(str) == 0, "expected empty comment, %p\n", str);
    IXMLDOMCDATASection_Release(cdata);
    SysFreeString(str);

    /* NODE_ATTRIBUTE */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ATTRIBUTE;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ATTRIBUTE;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ATTRIBUTE;
    str = SysAllocString( szlc );
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    if( SUCCEEDED(r) ) IXMLDOMNode_Release( node );
    SysFreeString(str);

    /* a name is required for attribute, try a BSTR with first null wchar */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ATTRIBUTE;
    str = SysAllocString( szstr1 );
    str[0] = 0;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);
    SysFreeString(str);

    /* NODE_PROCESSING_INSTRUCTION */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_PROCESSING_INSTRUCTION;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_PROCESSING_INSTRUCTION;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_PROCESSING_INSTRUCTION;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_("pi"), NULL, NULL );
    ok( r == E_INVALIDARG, "returns %08x\n", r );

    /* NODE_ENTITY_REFERENCE */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ENTITY_REFERENCE;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ENTITY_REFERENCE;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    /* NODE_ELEMENT */
    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ELEMENT;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, NULL, NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ELEMENT;
    node = (IXMLDOMNode*)0x1;
    r = IXMLDOMDocument_createNode( doc, var, _bstr_(""), NULL, &node );
    ok( r == E_FAIL, "returns %08x\n", r );
    ok( node == (void*)0x1, "expected same ptr, got %p\n", node);

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ELEMENT;
    str = SysAllocString( szlc );
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    if( SUCCEEDED(r) ) IXMLDOMNode_Release( node );

    V_VT(&var) = VT_I1;
    V_I1(&var) = NODE_ELEMENT;
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, NULL );
    ok( r == E_INVALIDARG, "returns %08x\n", r );

    V_VT(&var) = VT_R4;
    V_R4(&var) = NODE_ELEMENT;
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    if( SUCCEEDED(r) ) IXMLDOMNode_Release( node );

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString( szOne );
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    if( SUCCEEDED(r) ) IXMLDOMNode_Release( node );
    VariantClear(&var);

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString( szOneGarbage );
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == E_INVALIDARG, "returns %08x\n", r );
    if( SUCCEEDED(r) ) IXMLDOMNode_Release( node );
    VariantClear(&var);

    V_VT(&var) = VT_I4;
    V_I4(&var) = NODE_ELEMENT;
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMDocument_appendChild( doc, node, &root );
    ok( r == S_OK, "returns %08x\n", r );
    ok( node == root, "%p %p\n", node, root );

    ref = IXMLDOMNode_AddRef( node );
    ok(ref == 3, "ref %d\n", ref);
    IXMLDOMNode_Release( node );

    ref = IXMLDOMNode_Release( node );
    ok(ref == 1, "ref %d\n", ref);
    SysFreeString( str );

    V_VT(&var) = VT_I4;
    V_I4(&var) = NODE_ELEMENT;
    str = SysAllocString( szbs );
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    SysFreeString( str );

    ref = IXMLDOMNode_AddRef( node );
    ok(ref == 2, "ref = %d\n", ref);
    IXMLDOMNode_Release( node );

    r = IXMLDOMNode_QueryInterface( node, &IID_IUnknown, (void**)&unk );
    ok( r == S_OK, "returns %08x\n", r );

    ref = IXMLDOMNode_AddRef( unk );
    ok(ref == 3, "ref = %d\n", ref);
    IXMLDOMNode_Release( unk );

    V_VT(&var) = VT_EMPTY;
    r = IXMLDOMNode_insertBefore( root, (IXMLDOMNode*)unk, var, &child );
    ok( r == S_OK, "returns %08x\n", r );
    ok( unk == (IUnknown*)child, "%p %p\n", unk, child );
    IXMLDOMNode_Release( child );
    IUnknown_Release( unk );


    V_VT(&var) = VT_NULL;
    V_DISPATCH(&var) = (IDispatch*)node;
    r = IXMLDOMNode_insertBefore( root, node, var, &child );
    ok( r == S_OK, "returns %08x\n", r );
    ok( node == child, "%p %p\n", node, child );
    IXMLDOMNode_Release( child );


    V_VT(&var) = VT_NULL;
    V_DISPATCH(&var) = (IDispatch*)node;
    r = IXMLDOMNode_insertBefore( root, node, var, NULL );
    ok( r == S_OK, "returns %08x\n", r );
    IXMLDOMNode_Release( node );

    r = IXMLDOMNode_QueryInterface( root, &IID_IXMLDOMElement, (void**)&element );
    ok( r == S_OK, "returns %08x\n", r );

    r = IXMLDOMElement_get_attributes( element, &attr_map );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMNamedNodeMap_get_length( attr_map, &num );
    ok( r == S_OK, "returns %08x\n", r );
    ok( num == 0, "num %d\n", num );
    IXMLDOMNamedNodeMap_Release( attr_map );

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString( szstr1 );
    name = SysAllocString( szdl );
    r = IXMLDOMElement_setAttribute( element, name, var );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMElement_get_attributes( element, &attr_map );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMNamedNodeMap_get_length( attr_map, &num );
    ok( r == S_OK, "returns %08x\n", r );
    ok( num == 1, "num %d\n", num );
    IXMLDOMNamedNodeMap_Release( attr_map );
    VariantClear(&var);

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString( szstr2 );
    r = IXMLDOMElement_setAttribute( element, name, var );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMElement_get_attributes( element, &attr_map );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMNamedNodeMap_get_length( attr_map, &num );
    ok( r == S_OK, "returns %08x\n", r );
    ok( num == 1, "num %d\n", num );
    IXMLDOMNamedNodeMap_Release( attr_map );
    VariantClear(&var);
    r = IXMLDOMElement_getAttribute( element, name, &var );
    ok( r == S_OK, "returns %08x\n", r );
    ok( !lstrcmpW(V_BSTR(&var), szstr2), "wrong attr value\n");
    VariantClear(&var);
    SysFreeString(name);

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString( szstr1 );
    name = SysAllocString( szlc );
    r = IXMLDOMElement_setAttribute( element, name, var );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMElement_get_attributes( element, &attr_map );
    ok( r == S_OK, "returns %08x\n", r );
    r = IXMLDOMNamedNodeMap_get_length( attr_map, &num );
    ok( r == S_OK, "returns %08x\n", r );
    ok( num == 2, "num %d\n", num );
    IXMLDOMNamedNodeMap_Release( attr_map );
    VariantClear(&var);
    SysFreeString(name);

    V_VT(&var) = VT_I4;
    V_I4(&var) = 10;
    name = SysAllocString( szbs );
    r = IXMLDOMElement_setAttribute( element, name, var );
    ok( r == S_OK, "returns %08x\n", r );
    VariantClear(&var);
    r = IXMLDOMElement_getAttribute( element, name, &var );
    ok( r == S_OK, "returns %08x\n", r );
    ok( V_VT(&var) == VT_BSTR, "variant type %x\n", V_VT(&var));
    VariantClear(&var);
    SysFreeString(name);

    /* Create an Attribute */
    V_VT(&var) = VT_I4;
    V_I4(&var) = NODE_ATTRIBUTE;
    str = SysAllocString( szAttribute );
    r = IXMLDOMDocument_createNode( doc, var, str, NULL, &node );
    ok( r == S_OK, "returns %08x\n", r );
    ok( node != NULL, "node was null\n");
    SysFreeString(str);

    r = IXMLDOMNode_get_nodeTypeString(node, &str);
    ok( r == S_OK, "returns %08x\n", r );
    ok( !lstrcmpW( str, _bstr_("attribute") ), "incorrect nodeTypeString string\n");
    SysFreeString(str);
    IXMLDOMNode_Release( node );

    IXMLDOMElement_Release( element );
    IXMLDOMNode_Release( root );
    IXMLDOMDocument_Release( doc );
}

static void test_getElementsByTagName(void)
{
    IXMLDOMNodeList *node_list;
    IXMLDOMDocument *doc;
    IXMLDOMElement *elem;
    WCHAR buff[100];
    VARIANT_BOOL b;
    HRESULT r;
    LONG len;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    str = SysAllocString( szstar );

    /* null arguments cases */
    r = IXMLDOMDocument_getElementsByTagName(doc, NULL, &node_list);
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    r = IXMLDOMDocument_getElementsByTagName(doc, str, NULL);
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    r = IXMLDOMDocument_getElementsByTagName(doc, str, &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 6, "len %d\n", len );

    test_disp((IUnknown*)node_list);

    IXMLDOMNodeList_Release( node_list );
    SysFreeString( str );

    /* broken query BSTR */
    memcpy(&buff[2], szstar, sizeof(szstar));
    /* just a big length */
    *(DWORD*)buff = 0xf0f0;
    r = IXMLDOMDocument_getElementsByTagName(doc, &buff[2], &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 6, "len %d\n", len );
    IXMLDOMNodeList_Release( node_list );

    str = SysAllocString( szbs );
    r = IXMLDOMDocument_getElementsByTagName(doc, str, &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 1, "len %d\n", len );
    IXMLDOMNodeList_Release( node_list );
    SysFreeString( str );

    str = SysAllocString( szdl );
    r = IXMLDOMDocument_getElementsByTagName(doc, str, &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 0, "len %d\n", len );
    IXMLDOMNodeList_Release( node_list );
    SysFreeString( str );

    str = SysAllocString( szstr1 );
    r = IXMLDOMDocument_getElementsByTagName(doc, str, &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 0, "len %d\n", len );
    IXMLDOMNodeList_Release( node_list );
    SysFreeString( str );

    /* test for element */
    r = IXMLDOMDocument_get_documentElement(doc, &elem);
    ok( r == S_OK, "ret %08x\n", r );

    str = SysAllocString( szstar );

    /* null arguments cases */
    r = IXMLDOMElement_getElementsByTagName(elem, NULL, &node_list);
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    r = IXMLDOMElement_getElementsByTagName(elem, str, NULL);
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    r = IXMLDOMElement_getElementsByTagName(elem, str, &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 5, "len %d\n", len );
    expect_list_and_release(node_list, "E1.E2.D1 E2.E2.D1 E3.E2.D1 E4.E2.D1 E1.E4.E2.D1");
    SysFreeString( str );

    /* broken query BSTR */
    memcpy(&buff[2], szstar, sizeof(szstar));
    /* just a big length */
    *(DWORD*)buff = 0xf0f0;
    r = IXMLDOMElement_getElementsByTagName(elem, &buff[2], &node_list);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 5, "len %d\n", len );
    IXMLDOMNodeList_Release( node_list );

    IXMLDOMElement_Release(elem);

    IXMLDOMDocument_Release( doc );
}

static void test_get_text(void)
{
    HRESULT r;
    BSTR str;
    VARIANT_BOOL b;
    IXMLDOMDocument *doc;
    IXMLDOMNode *node, *node2, *node3;
    IXMLDOMNode *nodeRoot;
    IXMLDOMNodeList *node_list;
    IXMLDOMNamedNodeMap *node_map;
    LONG len;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    str = SysAllocString( szbs );
    r = IXMLDOMDocument_getElementsByTagName( doc, str, &node_list );
    ok( r == S_OK, "ret %08x\n", r );
    SysFreeString(str);

    /* Test to get all child node text. */
    r = IXMLDOMDocument_QueryInterface(doc, &IID_IXMLDOMNode, (void**)&nodeRoot);
    ok( r == S_OK, "ret %08x\n", r );
    if(r == S_OK)
    {
        r = IXMLDOMNode_get_text( nodeRoot, &str );
        ok( r == S_OK, "ret %08x\n", r );
        ok( compareIgnoreReturns(str, _bstr_("fn1.txt\n\n fn2.txt \n\nf1\n")), "wrong get_text: %s\n", wine_dbgstr_w(str));
        SysFreeString(str);

        IXMLDOMNode_Release(nodeRoot);
    }

    if (0) {
    /* this test crashes on win9x */
    r = IXMLDOMNodeList_QueryInterface(node_list, &IID_IDispatch, NULL);
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    }

    r = IXMLDOMNodeList_get_length( node_list, NULL );
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 1, "expect 1 got %d\n", len );

    r = IXMLDOMNodeList_get_item( node_list, 0, NULL );
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    r = IXMLDOMNodeList_nextNode( node_list, NULL );
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    r = IXMLDOMNodeList_get_item( node_list, 0, &node );
    ok( r == S_OK, "ret %08x\n", r );
    IXMLDOMNodeList_Release( node_list );

    /* Invalid output parameter*/
    r = IXMLDOMNode_get_text( node, NULL );
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    r = IXMLDOMNode_get_text( node, &str );
    ok( r == S_OK, "ret %08x\n", r );
    ok( !memcmp(str, szfn1_txt, lstrlenW(szfn1_txt) ), "wrong string\n" );
    SysFreeString(str);

    r = IXMLDOMNode_get_attributes( node, &node_map );
    ok( r == S_OK, "ret %08x\n", r );

    str = SysAllocString( szvr );
    r = IXMLDOMNamedNodeMap_getNamedItem( node_map, str, &node2 );
    ok( r == S_OK, "ret %08x\n", r );
    SysFreeString(str);

    r = IXMLDOMNode_get_text( node2, &str );
    ok( r == S_OK, "ret %08x\n", r );
    ok( !memcmp(str, szstr2, sizeof(szstr2)), "wrong string\n" );
    SysFreeString(str);

    r = IXMLDOMNode_get_firstChild( node2, &node3 );
    ok( r == S_OK, "ret %08x\n", r );

    r = IXMLDOMNode_get_text( node3, &str );
    ok( r == S_OK, "ret %08x\n", r );
    ok( !memcmp(str, szstr2, sizeof(szstr2)), "wrong string\n" );
    SysFreeString(str);


    IXMLDOMNode_Release( node3 );
    IXMLDOMNode_Release( node2 );
    IXMLDOMNamedNodeMap_Release( node_map );
    IXMLDOMNode_Release( node );
    IXMLDOMDocument_Release( doc );
}

static void test_get_childNodes(void)
{
    HRESULT r;
    BSTR str;
    VARIANT_BOOL b;
    IXMLDOMDocument *doc;
    IXMLDOMElement *element;
    IXMLDOMNode *node, *node2;
    IXMLDOMNodeList *node_list, *node_list2;
    LONG len;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMElement_get_childNodes( element, &node_list );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_length( node_list, &len );
    ok( r == S_OK, "ret %08x\n", r);
    ok( len == 4, "len %d\n", len);

    r = IXMLDOMNodeList_get_item( node_list, 2, &node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_childNodes( node, &node_list2 );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_length( node_list2, &len );
    ok( r == S_OK, "ret %08x\n", r);
    ok( len == 0, "len %d\n", len);

    r = IXMLDOMNodeList_get_item( node_list2, 0, &node2);
    ok( r == S_FALSE, "ret %08x\n", r);

    IXMLDOMNodeList_Release( node_list2 );
    IXMLDOMNode_Release( node );
    IXMLDOMNodeList_Release( node_list );
    IXMLDOMElement_Release( element );
    IXMLDOMDocument_Release( doc );
}

static void test_get_firstChild(void)
{
    static WCHAR xmlW[] = {'x','m','l',0};
    IXMLDOMDocument *doc;
    IXMLDOMNode *node;
    VARIANT_BOOL b;
    HRESULT r;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_firstChild( doc, &node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_nodeName( node, &str );
    ok( r == S_OK, "ret %08x\n", r);

    ok(memcmp(str, xmlW, sizeof(xmlW)) == 0, "expected \"xml\" node name\n");

    SysFreeString(str);
    IXMLDOMNode_Release( node );
    IXMLDOMDocument_Release( doc );
}

static void test_get_lastChild(void)
{
    static WCHAR lcW[] = {'l','c',0};
    static WCHAR foW[] = {'f','o',0};
    IXMLDOMDocument *doc;
    IXMLDOMNode *node, *child;
    VARIANT_BOOL b;
    HRESULT r;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_lastChild( doc, &node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_nodeName( node, &str );
    ok( r == S_OK, "ret %08x\n", r);

    ok(memcmp(str, lcW, sizeof(lcW)) == 0, "expected \"lc\" node name\n");
    SysFreeString(str);

    r = IXMLDOMNode_get_lastChild( node, &child );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_nodeName( child, &str );
    ok( r == S_OK, "ret %08x\n", r);

    ok(memcmp(str, foW, sizeof(foW)) == 0, "expected \"fo\" node name\n");
    SysFreeString(str);

    IXMLDOMNode_Release( child );
    IXMLDOMNode_Release( node );
    IXMLDOMDocument_Release( doc );
}

static void test_removeChild(void)
{
    HRESULT r;
    BSTR str;
    VARIANT_BOOL b;
    IXMLDOMDocument *doc;
    IXMLDOMElement *element, *lc_element;
    IXMLDOMNode *fo_node, *ba_node, *removed_node, *temp_node, *lc_node;
    IXMLDOMNodeList *root_list, *fo_list;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMElement_get_childNodes( element, &root_list );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_item( root_list, 3, &fo_node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_childNodes( fo_node, &fo_list );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_item( fo_list, 0, &ba_node );
    ok( r == S_OK, "ret %08x\n", r);

    /* invalid parameter: NULL ptr */
    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMElement_removeChild( element, NULL, &removed_node );
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    ok( removed_node == (void*)0xdeadbeef, "%p\n", removed_node );

    /* ba_node is a descendant of element, but not a direct child. */
    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMElement_removeChild( element, ba_node, &removed_node );
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    ok( removed_node == NULL, "%p\n", removed_node );

    r = IXMLDOMElement_removeChild( element, fo_node, &removed_node );
    ok( r == S_OK, "ret %08x\n", r);
    ok( fo_node == removed_node, "node %p node2 %p\n", fo_node, removed_node );

    /* try removing already removed child */
    temp_node = (void*)0xdeadbeef;
    r = IXMLDOMElement_removeChild( element, fo_node, &temp_node );
    ok( r == E_INVALIDARG, "ret %08x\n", r);
    ok( temp_node == NULL, "%p\n", temp_node );

    /* the removed node has no parent anymore */
    r = IXMLDOMNode_get_parentNode( removed_node, &temp_node );
    ok( r == S_FALSE, "ret %08x\n", r);
    ok( temp_node == NULL, "%p\n", temp_node );

    IXMLDOMNode_Release( removed_node );
    IXMLDOMNode_Release( ba_node );
    IXMLDOMNodeList_Release( fo_list );

    r = IXMLDOMNodeList_get_item( root_list, 0, &lc_node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMElement_QueryInterface( lc_node, &IID_IXMLDOMElement, (void**)&lc_element );
    ok( r == S_OK, "ret %08x\n", r);

    /* MS quirk: passing wrong interface pointer works, too */
    r = IXMLDOMElement_removeChild( element, (IXMLDOMNode*)lc_element, NULL );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_parentNode( lc_node, &temp_node );
    ok( r == S_FALSE, "ret %08x\n", r);
    ok( temp_node == NULL, "%p\n", temp_node );

    IXMLDOMNode_Release( lc_node );
    IXMLDOMNodeList_Release( root_list );
    IXMLDOMElement_Release( element );
    IXMLDOMDocument_Release( doc );
}

static void test_replaceChild(void)
{
    HRESULT r;
    BSTR str;
    VARIANT_BOOL b;
    IXMLDOMDocument *doc;
    IXMLDOMElement *element, *ba_element;
    IXMLDOMNode *fo_node, *ba_node, *lc_node, *removed_node, *temp_node;
    IXMLDOMNodeList *root_list, *fo_list;
    IUnknown * unk1, *unk2;
    LONG len;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMElement_get_childNodes( element, &root_list );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_item( root_list, 0, &lc_node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_item( root_list, 3, &fo_node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_childNodes( fo_node, &fo_list );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_item( fo_list, 0, &ba_node );
    ok( r == S_OK, "ret %08x\n", r);

    IXMLDOMNodeList_Release( fo_list );

    /* invalid parameter: NULL ptr for element to remove */
    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMElement_replaceChild( element, ba_node, NULL, &removed_node );
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    ok( removed_node == (void*)0xdeadbeef, "%p\n", removed_node );

    /* invalid parameter: NULL for replacement element. (Sic!) */
    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMElement_replaceChild( element, NULL, fo_node, &removed_node );
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    ok( removed_node == (void*)0xdeadbeef, "%p\n", removed_node );

    /* invalid parameter: OldNode is not a child */
    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMElement_replaceChild( element, lc_node, ba_node, &removed_node );
    ok( r == E_INVALIDARG, "ret %08x\n", r );
    ok( removed_node == NULL, "%p\n", removed_node );

    /* invalid parameter: would create loop */
    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMNode_replaceChild( fo_node, fo_node, ba_node, &removed_node );
    ok( r == E_FAIL, "ret %08x\n", r );
    ok( removed_node == NULL, "%p\n", removed_node );

    r = IXMLDOMElement_replaceChild( element, ba_node, fo_node, NULL );
    ok( r == S_OK, "ret %08x\n", r );

    r = IXMLDOMNodeList_get_item( root_list, 3, &temp_node );
    ok( r == S_OK, "ret %08x\n", r );

    /* ba_node and temp_node refer to the same node, yet they
       are different interface pointers */
    ok( ba_node != temp_node, "ba_node %p temp_node %p\n", ba_node, temp_node);
    r = IXMLDOMNode_QueryInterface( temp_node, &IID_IUnknown, (void**)&unk1);
    ok( r == S_OK, "ret %08x\n", r );
    r = IXMLDOMNode_QueryInterface( ba_node, &IID_IUnknown, (void**)&unk2);
    ok( r == S_OK, "ret %08x\n", r );
    todo_wine ok( unk1 == unk2, "unk1 %p unk2 %p\n", unk1, unk2);

    IUnknown_Release( unk1 );
    IUnknown_Release( unk2 );

    /* ba_node should have been removed from below fo_node */
    r = IXMLDOMNode_get_childNodes( fo_node, &fo_list );
    ok( r == S_OK, "ret %08x\n", r );

    /* MS quirk: replaceChild also accepts elements instead of nodes */
    r = IXMLDOMNode_QueryInterface( ba_node, &IID_IXMLDOMElement, (void**)&ba_element);
    ok( r == S_OK, "ret %08x\n", r );

    r = IXMLDOMElement_replaceChild( element, ba_node, (IXMLDOMNode*)ba_element, &removed_node );
    ok( r == S_OK, "ret %08x\n", r );

    r = IXMLDOMNodeList_get_length( fo_list, &len);
    ok( r == S_OK, "ret %08x\n", r );
    ok( len == 0, "len %d\n", len);

    IXMLDOMNodeList_Release( fo_list );

    IXMLDOMNode_Release(ba_node);
    IXMLDOMNode_Release(fo_node);
    IXMLDOMNode_Release(temp_node);
    IXMLDOMNodeList_Release( root_list );
    IXMLDOMElement_Release( element );
    IXMLDOMDocument_Release( doc );
}

static void test_removeNamedItem(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *element;
    IXMLDOMNode *pr_node, *removed_node, *removed_node2;
    IXMLDOMNodeList *root_list;
    IXMLDOMNamedNodeMap * pr_attrs;
    VARIANT_BOOL b;
    BSTR str;
    LONG len;
    HRESULT r;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    r = IXMLDOMDocument_get_documentElement( doc, &element );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMElement_get_childNodes( element, &root_list );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNodeList_get_item( root_list, 1, &pr_node );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNode_get_attributes( pr_node, &pr_attrs );
    ok( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNamedNodeMap_get_length( pr_attrs, &len );
    ok( r == S_OK, "ret %08x\n", r);
    ok( len == 3, "length %d\n", len);

    removed_node = (void*)0xdeadbeef;
    r = IXMLDOMNamedNodeMap_removeNamedItem( pr_attrs, NULL, &removed_node);
    ok ( r == E_INVALIDARG, "ret %08x\n", r);
    ok ( removed_node == (void*)0xdeadbeef, "got %p\n", removed_node);

    removed_node = (void*)0xdeadbeef;
    str = SysAllocString(szvr);
    r = IXMLDOMNamedNodeMap_removeNamedItem( pr_attrs, str, &removed_node);
    ok ( r == S_OK, "ret %08x\n", r);

    removed_node2 = (void*)0xdeadbeef;
    r = IXMLDOMNamedNodeMap_removeNamedItem( pr_attrs, str, &removed_node2);
    ok ( r == S_FALSE, "ret %08x\n", r);
    ok ( removed_node2 == NULL, "got %p\n", removed_node2 );

    r = IXMLDOMNamedNodeMap_get_length( pr_attrs, &len );
    ok( r == S_OK, "ret %08x\n", r);
    ok( len == 2, "length %d\n", len);

    r = IXMLDOMNamedNodeMap_setNamedItem( pr_attrs, removed_node, NULL);
    ok ( r == S_OK, "ret %08x\n", r);
    IXMLDOMNode_Release(removed_node);

    r = IXMLDOMNamedNodeMap_get_length( pr_attrs, &len );
    ok( r == S_OK, "ret %08x\n", r);
    ok( len == 3, "length %d\n", len);

    r = IXMLDOMNamedNodeMap_removeNamedItem( pr_attrs, str, NULL);
    ok ( r == S_OK, "ret %08x\n", r);

    r = IXMLDOMNamedNodeMap_get_length( pr_attrs, &len );
    ok( r == S_OK, "ret %08x\n", r);
    ok( len == 2, "length %d\n", len);

    r = IXMLDOMNamedNodeMap_removeNamedItem( pr_attrs, str, NULL);
    ok ( r == S_FALSE, "ret %08x\n", r);

    SysFreeString(str);

    IXMLDOMNamedNodeMap_Release( pr_attrs );
    IXMLDOMNode_Release( pr_node );
    IXMLDOMNodeList_Release( root_list );
    IXMLDOMElement_Release( element );
    IXMLDOMDocument_Release( doc );
}

static void test_XMLHTTP(void)
{
    static const WCHAR wszBody[] = {'m','o','d','e','=','T','e','s','t',0};
    static const WCHAR wszPOST[] = {'P','O','S','T',0};
    static const WCHAR wszUrl[] = {'h','t','t','p',':','/','/',
        'c','r','o','s','s','o','v','e','r','.','c','o','d','e','w','e','a','v','e','r','s','.','c','o','m','/',
        'p','o','s','t','t','e','s','t','.','p','h','p',0};
    static const WCHAR xmltestW[] = {'h','t','t','p',':','/','/',
        'c','r','o','s','s','o','v','e','r','.','c','o','d','e','w','e','a','v','e','r','s','.','c','o','m','/',
        'x','m','l','t','e','s','t','.','x','m','l',0};
    static const WCHAR wszExpectedResponse[] = {'F','A','I','L','E','D',0};
    static const CHAR xmltestbodyA[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<a>TEST</a>\n";

    IXMLHttpRequest *pXMLHttpRequest;
    BSTR bstrResponse, method, url;
    VARIANT dummy;
    VARIANT async;
    VARIANT varbody;
    LONG state, status, ref, bound;
    void *ptr;
    IDispatch *event;
    HRESULT hr = CoCreateInstance(&CLSID_XMLHTTPRequest, NULL,
                                  CLSCTX_INPROC_SERVER, &IID_IXMLHttpRequest,
                                  (void **)&pXMLHttpRequest);
    if (FAILED(hr))
    {
        win_skip("IXMLHTTPRequest is not available (0x%08x)\n", hr);
        return;
    }

    VariantInit(&dummy);
    V_VT(&dummy) = VT_ERROR;
    V_ERROR(&dummy) = DISP_E_MEMBERNOTFOUND;
    VariantInit(&async);
    V_VT(&async) = VT_BOOL;
    V_BOOL(&async) = VARIANT_FALSE;
    V_VT(&varbody) = VT_BSTR;
    V_BSTR(&varbody) = SysAllocString(wszBody);

    method = SysAllocString(wszPOST);
    url = SysAllocString(wszUrl);

if (0)
{
    /* crashes on win98 */
    hr = IXMLHttpRequest_put_onreadystatechange(pXMLHttpRequest, NULL);
    ok(hr == S_OK, "got 0x%08x\n", hr);
}

    hr = IXMLHttpRequest_abort(pXMLHttpRequest);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    /* send before open */
    hr = IXMLHttpRequest_send(pXMLHttpRequest, dummy);
    ok(hr == E_FAIL || broken(hr == E_UNEXPECTED) /* win9x, win2k */, "got 0x%08x\n", hr);

    /* initial status code */
    hr = IXMLHttpRequest_get_status(pXMLHttpRequest, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    status = 0xdeadbeef;
    hr = IXMLHttpRequest_get_status(pXMLHttpRequest, &status);
    ok(hr == E_FAIL || broken(hr == E_UNEXPECTED) /* win9x, win2k */, "got 0x%08x\n", hr);
    ok(status == 0xdeadbeef, "got %d\n", status);

    /* invalid parameters */
    hr = IXMLHttpRequest_open(pXMLHttpRequest, NULL, NULL, async, dummy, dummy);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_open(pXMLHttpRequest, method, NULL, async, dummy, dummy);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_open(pXMLHttpRequest, NULL, url, async, dummy, dummy);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, NULL, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, _bstr_("header1"), NULL);
    ok(hr == E_FAIL || broken(hr == E_UNEXPECTED) /* win9x, win2k */, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, NULL, _bstr_("value1"));
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, _bstr_("header1"), _bstr_("value1"));
    ok(hr == E_FAIL || broken(hr == E_UNEXPECTED) /* win9x, win2k */, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_get_readyState(pXMLHttpRequest, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    state = -1;
    hr = IXMLHttpRequest_get_readyState(pXMLHttpRequest, &state);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(state == READYSTATE_UNINITIALIZED, "got %d, expected READYSTATE_UNINITIALIZED\n", state);

    event = create_dispevent();
    ref = IDispatch_AddRef(event);
    ok(ref == 2, "got %d\n", ref);
    IDispatch_Release(event);

    hr = IXMLHttpRequest_put_onreadystatechange(pXMLHttpRequest, event);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    ref = IDispatch_AddRef(event);
    ok(ref == 3, "got %d\n", ref);
    IDispatch_Release(event);

    g_unexpectedcall = g_expectedcall = 0;

    hr = IXMLHttpRequest_open(pXMLHttpRequest, method, url, async, dummy, dummy);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    ok(g_unexpectedcall == 0, "unexpected disp event call\n");
    ok(g_expectedcall == 1 || broken(g_expectedcall == 0) /* win2k */, "no expected disp event call\n");

    /* status code after ::open() */
    status = 0xdeadbeef;
    hr = IXMLHttpRequest_get_status(pXMLHttpRequest, &status);
    ok(hr == E_FAIL || broken(hr == E_UNEXPECTED) /* win9x, win2k */, "got 0x%08x\n", hr);
    ok(status == 0xdeadbeef, "got %d\n", status);

    state = -1;
    hr = IXMLHttpRequest_get_readyState(pXMLHttpRequest, &state);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(state == READYSTATE_LOADING, "got %d, expected READYSTATE_LOADING\n", state);

    hr = IXMLHttpRequest_abort(pXMLHttpRequest);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    state = -1;
    hr = IXMLHttpRequest_get_readyState(pXMLHttpRequest, &state);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(state == READYSTATE_UNINITIALIZED || broken(state == READYSTATE_LOADING) /* win98, win2k */,
        "got %d, expected READYSTATE_UNINITIALIZED\n", state);

    hr = IXMLHttpRequest_open(pXMLHttpRequest, method, url, async, dummy, dummy);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, _bstr_("header1"), _bstr_("value1"));
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, NULL, _bstr_("value1"));
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_setRequestHeader(pXMLHttpRequest, _bstr_(""), _bstr_("value1"));
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    SysFreeString(method);
    SysFreeString(url);

    hr = IXMLHttpRequest_send(pXMLHttpRequest, varbody);
    if (hr == INET_E_RESOURCE_NOT_FOUND)
    {
        skip("No connection could be made with crossover.codeweavers.com\n");
        IXMLHttpRequest_Release(pXMLHttpRequest);
        return;
    }
    ok(hr == S_OK, "got 0x%08x\n", hr);

    /* status code after ::send() */
    status = 0xdeadbeef;
    hr = IXMLHttpRequest_get_status(pXMLHttpRequest, &status);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(status == 200, "got %d\n", status);

    /* another ::send() after completed request */
    hr = IXMLHttpRequest_send(pXMLHttpRequest, varbody);
    ok(hr == E_FAIL || broken(hr == E_UNEXPECTED) /* win9x, win2k */, "got 0x%08x\n", hr);

    VariantClear(&varbody);

    hr = IXMLHttpRequest_get_responseText(pXMLHttpRequest, &bstrResponse);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    /* the server currently returns "FAILED" because the Content-Type header is
     * not what the server expects */
    if(hr == S_OK)
    {
        ok(!memcmp(bstrResponse, wszExpectedResponse, sizeof(wszExpectedResponse)),
            "expected %s, got %s\n", wine_dbgstr_w(wszExpectedResponse), wine_dbgstr_w(bstrResponse));
        SysFreeString(bstrResponse);
    }

    /* GET request */
    url = SysAllocString(xmltestW);

    hr = IXMLHttpRequest_open(pXMLHttpRequest, _bstr_("GET"), url, async, dummy, dummy);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    V_VT(&varbody) = VT_EMPTY;

    hr = IXMLHttpRequest_send(pXMLHttpRequest, varbody);
    if (hr == INET_E_RESOURCE_NOT_FOUND)
    {
        skip("No connection could be made with crossover.codeweavers.com\n");
        IXMLHttpRequest_Release(pXMLHttpRequest);
        return;
    }
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_get_responseText(pXMLHttpRequest, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLHttpRequest_get_responseText(pXMLHttpRequest, &bstrResponse);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    if(hr == S_OK)
    {
        ok(!memcmp(bstrResponse, _bstr_(xmltestbodyA), sizeof(xmltestbodyA)*sizeof(WCHAR)),
            "expected %s, got %s\n", xmltestbodyA, wine_dbgstr_w(bstrResponse));
        SysFreeString(bstrResponse);
    }

    hr = IXMLHttpRequest_get_responseBody(pXMLHttpRequest, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    V_VT(&varbody) = VT_EMPTY;
    hr = IXMLHttpRequest_get_responseBody(pXMLHttpRequest, &varbody);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&varbody) == (VT_ARRAY|VT_UI1), "got type %d, expected %d\n", V_VT(&varbody), VT_ARRAY|VT_UI1);
    ok(SafeArrayGetDim(V_ARRAY(&varbody)) == 1, "got %d, expected one dimension\n", SafeArrayGetDim(V_ARRAY(&varbody)));

    bound = -1;
    hr = SafeArrayGetLBound(V_ARRAY(&varbody), 1, &bound);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(bound == 0, "got %d, expected zero bound\n", bound);

    hr = SafeArrayAccessData(V_ARRAY(&varbody), &ptr);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(memcmp(ptr, xmltestbodyA, sizeof(xmltestbodyA)-1) == 0, "got wrond body data\n");
    SafeArrayUnaccessData(V_ARRAY(&varbody));

    VariantClear(&varbody);

    SysFreeString(url);

    IDispatch_Release(event);
    IXMLHttpRequest_Release(pXMLHttpRequest);
    free_bstrs();
}

static void test_IXMLDOMDocument2(void)
{
    static const WCHAR emptyW[] = {0};
    IXMLDOMDocument2 *doc2, *dtddoc2;
    IXMLDOMDocument *doc;
    IXMLDOMParseError* err;
    IDispatchEx *dispex;
    VARIANT_BOOL b;
    VARIANT var;
    HRESULT r;
    LONG ref, res;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    dtddoc2 = create_document(&IID_IXMLDOMDocument2);
    if (!dtddoc2)
    {
        IXMLDOMDocument_Release(doc);
        return;
    }

    r = IXMLDOMDocument_QueryInterface( doc, &IID_IXMLDOMDocument2, (void**)&doc2 );
    ok( r == S_OK, "ret %08x\n", r );
    ok( doc == (IXMLDOMDocument*)doc2, "interfaces differ\n");

    ole_expect(IXMLDOMDocument2_get_readyState(doc2, NULL), E_INVALIDARG);
    ole_check(IXMLDOMDocument2_get_readyState(doc2, &res));
    ok(res == READYSTATE_COMPLETE, "expected READYSTATE_COMPLETE (4), got %i\n", res);

    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(doc2, NULL), S_FALSE);
    ole_expect(IXMLDOMDocument2_validate(doc2, &err), S_FALSE);
    ok(err != NULL, "expected a pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_E_NOTWF */
        ok(res == E_XML_NOTWF, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    str = SysAllocString( szComplete4 );
    r = IXMLDOMDocument_loadXML( doc2, str, &b );
    ok( r == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    ole_check(IXMLDOMDocument2_get_readyState(doc, &res));
    ok(res == READYSTATE_COMPLETE, "expected READYSTATE_COMPLETE (4), got %i\n", res);

    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(doc2, &err), S_FALSE);
    ok(err != NULL, "expected a pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_E_NODTD */
        ok(res == E_XML_NODTD, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    r = IXMLDOMDocument_QueryInterface( doc, &IID_IDispatchEx, (void**)&dispex );
    ok( r == S_OK, "ret %08x\n", r );
    if(r == S_OK)
    {
        IDispatchEx_Release(dispex);
    }

    /* we will check if the variant got cleared */
    ref = IXMLDOMDocument2_AddRef(doc2);
    expect_eq(ref, 3, int, "%d");  /* doc, doc2, AddRef*/
    V_VT(&var) = VT_UNKNOWN;
    V_UNKNOWN(&var) = (IUnknown *)doc2;

    /* invalid calls */
    ole_expect(IXMLDOMDocument2_getProperty(doc2, _bstr_("askldhfaklsdf"), &var), E_FAIL);
    expect_eq(V_VT(&var), VT_UNKNOWN, int, "%x");
    ole_expect(IXMLDOMDocument2_getProperty(doc2, _bstr_("SelectionLanguage"), NULL), E_INVALIDARG);

    /* valid call */
    ole_check(IXMLDOMDocument2_getProperty(doc2, _bstr_("SelectionLanguage"), &var));
    expect_eq(V_VT(&var), VT_BSTR, int, "%x");
    expect_bstr_eq_and_free(V_BSTR(&var), "XSLPattern");
    V_VT(&var) = VT_R4;

    /* the variant didn't get cleared*/
    expect_eq(IXMLDOMDocument2_Release(doc2), 2, int, "%d");

    /* setProperty tests */
    ole_expect(IXMLDOMDocument2_setProperty(doc2, _bstr_("askldhfaklsdf"), var), E_FAIL);
    ole_expect(IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionLanguage"), var), E_FAIL);
    ole_expect(IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionLanguage"), _variantbstr_("alskjdh faklsjd hfk")), E_FAIL);
    ole_check(IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionLanguage"), _variantbstr_("XSLPattern")));
    ole_check(IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionLanguage"), _variantbstr_("XPath")));
    ole_check(IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionLanguage"), _variantbstr_("XSLPattern")));

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = SysAllocString(emptyW);
    r = IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionNamespaces"), var);
    ok(r == S_OK, "got 0x%08x\n", r);
    VariantClear(&var);

    V_VT(&var) = VT_I2;
    V_I2(&var) = 0;
    r = IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionNamespaces"), var);
    ok(r == E_FAIL, "got 0x%08x\n", r);

    /* contrary to what MSDN claims you can switch back from XPath to XSLPattern */
    ole_check(IXMLDOMDocument2_getProperty(doc2, _bstr_("SelectionLanguage"), &var));
    expect_eq(V_VT(&var), VT_BSTR, int, "%x");
    expect_bstr_eq_and_free(V_BSTR(&var), "XSLPattern");

    IXMLDOMDocument2_Release( doc2 );
    IXMLDOMDocument_Release( doc );

    /* DTD validation */
    ole_check(IXMLDOMDocument2_put_validateOnParse(dtddoc2, VARIANT_FALSE));
    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_check(IXMLDOMDocument2_validate(dtddoc2, &err));
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_expect(IXMLDOMParseError_get_errorCode(err, &res), S_FALSE);
        ok(res == 0, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_0D), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ELEMENT_UNDECLARED */
        todo_wine ok(res == 0xC00CE00D, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_0E), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ELEMENT_ID_NOT_FOUND */
        todo_wine ok(res == 0xC00CE00E, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_11), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_EMPTY_NOT_ALLOWED */
        todo_wine ok(res == 0xC00CE011, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_13), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ROOT_NAME_MISMATCH */
        todo_wine ok(res == 0xC00CE013, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_14), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_INVALID_CONTENT */
        todo_wine ok(res == 0xC00CE014, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_15), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ATTRIBUTE_NOT_DEFINED */
        todo_wine ok(res == 0xC00CE015, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_16), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ATTRIBUTE_FIXED */
        todo_wine ok(res == 0xC00CE016, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_17), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ATTRIBUTE_VALUE */
        todo_wine ok(res == 0xC00CE017, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_18), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_ILLEGAL_TEXT */
        todo_wine ok(res == 0xC00CE018, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    ole_check(IXMLDOMDocument2_loadXML(dtddoc2, _bstr_(szEmailXML_20), &b));
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    err = NULL;
    ole_expect(IXMLDOMDocument2_validate(dtddoc2, &err), S_FALSE);
    ok(err != NULL, "expected pointer\n");
    if (err)
    {
        res = 0;
        ole_check(IXMLDOMParseError_get_errorCode(err, &res));
        /* XML_REQUIRED_ATTRIBUTE_MISSING */
        todo_wine ok(res == 0xC00CE020, "got %08x\n", res);
        IXMLDOMParseError_Release(err);
    }

    IXMLDOMDocument2_Release( dtddoc2 );
    free_bstrs();
}

#define helper_ole_check(expr) { \
    HRESULT r = expr; \
    ok_(__FILE__, line)(r == S_OK, "=> %i: " #expr " returned %08x\n", __LINE__, r); \
}

#define helper_expect_list_and_release(list, expstr) { \
    char *str = list_to_string(list); \
    ok_(__FILE__, line)(strcmp(str, expstr)==0, "=> %i: Invalid node list: %s, expected %s\n", __LINE__, str, expstr); \
    if (list) IXMLDOMNodeList_Release(list); \
}

#define helper_expect_bstr_and_release(bstr, str) { \
    ok_(__FILE__, line)(lstrcmpW(bstr, _bstr_(str)) == 0, \
       "=> %i: got %s\n", __LINE__, wine_dbgstr_w(bstr)); \
    SysFreeString(bstr); \
}

#define check_ws_ignored(doc, str) _check_ws_ignored(__LINE__, doc, str)
static inline void _check_ws_ignored(int line, IXMLDOMDocument2* doc, char const* str)
{
    IXMLDOMNode *node1, *node2;
    IXMLDOMNodeList *list;
    BSTR bstr;

    helper_ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//*[local-name()='html']"), &list));
    helper_ole_check(IXMLDOMNodeList_get_item(list, 0, &node1));
    helper_ole_check(IXMLDOMNodeList_get_item(list, 1, &node2));
    helper_ole_check(IXMLDOMNodeList_reset(list));
    helper_expect_list_and_release(list, "E1.E4.E1.E2.D1 E2.E4.E1.E2.D1");

    helper_ole_check(IXMLDOMNode_get_childNodes(node1, &list));
    helper_expect_list_and_release(list, "T1.E1.E4.E1.E2.D1 E2.E1.E4.E1.E2.D1 E3.E1.E4.E1.E2.D1 T4.E1.E4.E1.E2.D1 E5.E1.E4.E1.E2.D1");
    helper_ole_check(IXMLDOMNode_get_text(node1, &bstr));
    if (str)
    {
        helper_expect_bstr_and_release(bstr, str);
    }
    else
    {
        helper_expect_bstr_and_release(bstr, "This is a description.");
    }
    IXMLDOMNode_Release(node1);

    helper_ole_check(IXMLDOMNode_get_childNodes(node2, &list));
    helper_expect_list_and_release(list, "T1.E2.E4.E1.E2.D1 E2.E2.E4.E1.E2.D1 T3.E2.E4.E1.E2.D1 E4.E2.E4.E1.E2.D1 T5.E2.E4.E1.E2.D1 E6.E2.E4.E1.E2.D1 T7.E2.E4.E1.E2.D1");
    helper_ole_check(IXMLDOMNode_get_text(node2, &bstr));
    helper_expect_bstr_and_release(bstr, "\n                This is a description with preserved whitespace. \n            ");
    IXMLDOMNode_Release(node2);
}

#define check_ws_preserved(doc, str) _check_ws_preserved(__LINE__, doc, str)
static inline void _check_ws_preserved(int line, IXMLDOMDocument2* doc, char const* str)
{
    IXMLDOMNode *node1, *node2;
    IXMLDOMNodeList *list;
    BSTR bstr;

    helper_ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//*[local-name()='html']"), &list));
    helper_ole_check(IXMLDOMNodeList_get_item(list, 0, &node1));
    helper_ole_check(IXMLDOMNodeList_get_item(list, 1, &node2));
    helper_ole_check(IXMLDOMNodeList_reset(list));
    helper_expect_list_and_release(list, "E2.E8.E2.E2.D1 E4.E8.E2.E2.D1");

    helper_ole_check(IXMLDOMNode_get_childNodes(node1, &list));
    helper_expect_list_and_release(list, "T1.E2.E8.E2.E2.D1 E2.E2.E8.E2.E2.D1 T3.E2.E8.E2.E2.D1 E4.E2.E8.E2.E2.D1 T5.E2.E8.E2.E2.D1 E6.E2.E8.E2.E2.D1 T7.E2.E8.E2.E2.D1");
    helper_ole_check(IXMLDOMNode_get_text(node1, &bstr));
    if (str)
    {
        helper_expect_bstr_and_release(bstr, str);
    }
    else
    {
        helper_expect_bstr_and_release(bstr, "\n                This is a description. \n            ");
    }
    IXMLDOMNode_Release(node1);

    helper_ole_check(IXMLDOMNode_get_childNodes(node2, &list));
    helper_expect_list_and_release(list, "T1.E4.E8.E2.E2.D1 E2.E4.E8.E2.E2.D1 T3.E4.E8.E2.E2.D1 E4.E4.E8.E2.E2.D1 T5.E4.E8.E2.E2.D1 E6.E4.E8.E2.E2.D1 T7.E4.E8.E2.E2.D1");
    helper_ole_check(IXMLDOMNode_get_text(node2, &bstr));
    helper_expect_bstr_and_release(bstr, "\n                This is a description with preserved whitespace. \n            ");
    IXMLDOMNode_Release(node2);
}

static void test_whitespace(void)
{
    VARIANT_BOOL b;
    IXMLDOMDocument2 *doc1, *doc2, *doc3, *doc4;

    doc1 = create_document(&IID_IXMLDOMDocument2);
    doc2 = create_document(&IID_IXMLDOMDocument2);
    if (!doc1 || !doc2) return;

    ole_check(IXMLDOMDocument2_put_preserveWhiteSpace(doc2, VARIANT_TRUE));
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc1, &b));
    ok(b == VARIANT_FALSE, "expected false\n");
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc2, &b));
    ok(b == VARIANT_TRUE, "expected true\n");

    ole_check(IXMLDOMDocument2_loadXML(doc1, _bstr_(szExampleXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");
    ole_check(IXMLDOMDocument2_loadXML(doc2, _bstr_(szExampleXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");

    /* switch to XPath */
    ole_check(IXMLDOMDocument2_setProperty(doc1, _bstr_("SelectionLanguage"), _variantbstr_("XPath")));
    ole_check(IXMLDOMDocument2_setProperty(doc2, _bstr_("SelectionLanguage"), _variantbstr_("XPath")));

    check_ws_ignored(doc1, NULL);
    check_ws_preserved(doc2, NULL);

    /* new instances copy the property */
    ole_check(IXMLDOMDocument2_QueryInterface(doc1, &IID_IXMLDOMDocument2, (void**) &doc3));
    ole_check(IXMLDOMDocument2_QueryInterface(doc2, &IID_IXMLDOMDocument2, (void**) &doc4));

    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc3, &b));
    ok(b == VARIANT_FALSE, "expected false\n");
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc4, &b));
    ok(b == VARIANT_TRUE, "expected true\n");

    check_ws_ignored(doc3, NULL);
    check_ws_preserved(doc4, NULL);

    /* setting after loading xml affects trimming of leading/trailing ws only */
    ole_check(IXMLDOMDocument2_put_preserveWhiteSpace(doc1, VARIANT_TRUE));
    ole_check(IXMLDOMDocument2_put_preserveWhiteSpace(doc2, VARIANT_FALSE));

    /* the trailing "\n            " isn't there, because it was ws-only node */
    check_ws_ignored(doc1, "\n                This is a description. ");
    check_ws_preserved(doc2, "This is a description.");

    /* it takes effect on reload */
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc1, &b));
    ok(b == VARIANT_TRUE, "expected true\n");
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc2, &b));
    ok(b == VARIANT_FALSE, "expected false\n");

    ole_check(IXMLDOMDocument2_loadXML(doc1, _bstr_(szExampleXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");
    ole_check(IXMLDOMDocument2_loadXML(doc2, _bstr_(szExampleXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");

    check_ws_preserved(doc1, NULL);
    check_ws_ignored(doc2, NULL);

    /* other instances follow suit */
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc3, &b));
    ok(b == VARIANT_TRUE, "expected true\n");
    ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc4, &b));
    ok(b == VARIANT_FALSE, "expected false\n");

    check_ws_preserved(doc3, NULL);
    check_ws_ignored(doc4, NULL);

    IXMLDOMDocument_Release(doc1);
    IXMLDOMDocument_Release(doc2);
    IXMLDOMDocument_Release(doc3);
    IXMLDOMDocument_Release(doc4);
    free_bstrs();
}

static void test_XPath(void)
{
    VARIANT var;
    VARIANT_BOOL b;
    IXMLDOMDocument2 *doc;
    IXMLDOMNode *rootNode;
    IXMLDOMNode *elem1Node;
    IXMLDOMNode *node;
    IXMLDOMNodeList *list;

    doc = create_document(&IID_IXMLDOMDocument2);
    if (!doc) return;

    ole_check(IXMLDOMDocument_loadXML(doc, _bstr_(szExampleXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");

    /* switch to XPath */
    ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionLanguage"), _variantbstr_("XPath")));

    /* some simple queries*/
    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("root"), &list));
    ole_check(IXMLDOMNodeList_get_item(list, 0, &rootNode));
    ole_check(IXMLDOMNodeList_reset(list));
    expect_list_and_release(list, "E2.D1");
    if (rootNode == NULL)
        return;

    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("root//c"), &list));
    expect_list_and_release(list, "E3.E1.E2.D1 E3.E2.E2.D1");

    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("//c[@type]"), &list));
    expect_list_and_release(list, "E3.E2.E2.D1");

    ole_check(IXMLDOMNode_selectNodes(rootNode, _bstr_("elem"), &list));
    /* using get_item for query results advances the position */
    ole_check(IXMLDOMNodeList_get_item(list, 1, &node));
    expect_node(node, "E2.E2.D1");
    IXMLDOMNode_Release(node);
    ole_check(IXMLDOMNodeList_nextNode(list, &node));
    expect_node(node, "E4.E2.D1");
    IXMLDOMNode_Release(node);
    ole_check(IXMLDOMNodeList_reset(list));
    expect_list_and_release(list, "E1.E2.D1 E2.E2.D1 E4.E2.D1");

    ole_check(IXMLDOMNode_selectNodes(rootNode, _bstr_("."), &list));
    expect_list_and_release(list, "E2.D1");

    ole_check(IXMLDOMNode_selectNodes(rootNode, _bstr_("elem[3]/preceding-sibling::*"), &list));
    ole_check(IXMLDOMNodeList_get_item(list, 0, &elem1Node));
    ole_check(IXMLDOMNodeList_reset(list));
    expect_list_and_release(list, "E1.E2.D1 E2.E2.D1 E3.E2.D1");

    /* select an attribute */
    ole_check(IXMLDOMNode_selectNodes(rootNode, _bstr_(".//@type"), &list));
    expect_list_and_release(list, "A'type'.E3.E2.E2.D1");

    /* would evaluate to a number */
    ole_expect(IXMLDOMNode_selectNodes(rootNode, _bstr_("count(*)"), &list), E_FAIL);
    /* would evaluate to a boolean */
    ole_expect(IXMLDOMNode_selectNodes(rootNode, _bstr_("position()>0"), &list), E_FAIL);
    /* would evaluate to a string */
    ole_expect(IXMLDOMNode_selectNodes(rootNode, _bstr_("name()"), &list), E_FAIL);

    /* no results */
    ole_check(IXMLDOMNode_selectNodes(rootNode, _bstr_("c"), &list));
    expect_list_and_release(list, "");
    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("elem//c"), &list));
    expect_list_and_release(list, "");
    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("//elem[4]"), &list));
    expect_list_and_release(list, "");
    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("root//elem[0]"), &list));
    expect_list_and_release(list, "");

    /* foo undeclared in document node */
    ole_expect(IXMLDOMDocument_selectNodes(doc, _bstr_("root//foo:c"), &list), E_FAIL);
    /* undeclared in <root> node */
    ole_expect(IXMLDOMNode_selectNodes(rootNode, _bstr_(".//foo:c"), &list), E_FAIL);
    /* undeclared in <elem> node */
    ole_expect(IXMLDOMNode_selectNodes(elem1Node, _bstr_("//foo:c"), &list), E_FAIL);
    /* but this trick can be used */
    ole_check(IXMLDOMNode_selectNodes(elem1Node, _bstr_("//*[name()='foo:c']"), &list));
    expect_list_and_release(list, "E3.E4.E2.D1");

    /* it has to be declared in SelectionNamespaces */
    ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"),
        _variantbstr_("xmlns:test='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29'")));

    /* now the namespace can be used */
    ole_check(IXMLDOMDocument_selectNodes(doc, _bstr_("root//test:c"), &list));
    expect_list_and_release(list, "E3.E3.E2.D1 E3.E4.E2.D1");
    ole_check(IXMLDOMNode_selectNodes(rootNode, _bstr_(".//test:c"), &list));
    expect_list_and_release(list, "E3.E3.E2.D1 E3.E4.E2.D1");
    ole_check(IXMLDOMNode_selectNodes(elem1Node, _bstr_("//test:c"), &list));
    expect_list_and_release(list, "E3.E3.E2.D1 E3.E4.E2.D1");
    ole_check(IXMLDOMNode_selectNodes(elem1Node, _bstr_(".//test:x"), &list));
    expect_list_and_release(list, "E5.E1.E4.E1.E2.D1 E6.E2.E4.E1.E2.D1");

    /* SelectionNamespaces syntax error - the namespaces doesn't work anymore but the value is stored */
    ole_expect(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"),
        _variantbstr_("xmlns:test='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29' xmlns:foo=###")), E_FAIL);

    ole_expect(IXMLDOMDocument_selectNodes(doc, _bstr_("root//foo:c"), &list), E_FAIL);

    VariantInit(&var);
    ole_check(IXMLDOMDocument2_getProperty(doc, _bstr_("SelectionNamespaces"), &var));
    expect_eq(V_VT(&var), VT_BSTR, int, "%x");
    if (V_VT(&var) == VT_BSTR)
        expect_bstr_eq_and_free(V_BSTR(&var), "xmlns:test='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29' xmlns:foo=###");

    /* extra attributes - same thing*/
    ole_expect(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"),
        _variantbstr_("xmlns:test='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29' param='test'")), E_FAIL);
    ole_expect(IXMLDOMDocument_selectNodes(doc, _bstr_("root//foo:c"), &list), E_FAIL);

    IXMLDOMNode_Release(rootNode);
    IXMLDOMNode_Release(elem1Node);
    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_cloneNode(void )
{
    IXMLDOMDocument *doc;
    VARIANT_BOOL b;
    IXMLDOMNodeList *pList;
    IXMLDOMNamedNodeMap *mapAttr;
    LONG nLength = 0, nLength1 = 0;
    LONG nAttrCnt = 0, nAttrCnt1 = 0;
    IXMLDOMNode *node;
    IXMLDOMNode *node_clone;
    IXMLDOMNode *node_first;
    HRESULT r;
    BSTR str;
    static const WCHAR szSearch[] = { 'l', 'c', '/', 'p', 'r', 0 };

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    ole_check(IXMLDOMDocument_loadXML(doc, str, &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString(str);

    if(!b)
        return;

    str = SysAllocString( szSearch);
    r = IXMLDOMNode_selectSingleNode(doc, str, &node);
    ok( r == S_OK, "ret %08x\n", r );
    ok( node != NULL, "node %p\n", node );
    SysFreeString(str);

    if(!node)
    {
        IXMLDOMDocument_Release(doc);
        return;
    }

    /* Check invalid parameter */
    r = IXMLDOMNode_cloneNode(node, VARIANT_TRUE, NULL);
    ok( r == E_INVALIDARG, "ret %08x\n", r );

    /* All Children */
    r = IXMLDOMNode_cloneNode(node, VARIANT_TRUE, &node_clone);
    ok( r == S_OK, "ret %08x\n", r );
    ok( node_clone != NULL, "node %p\n", node );

    if(!node_clone)
    {
        IXMLDOMDocument_Release(doc);
        IXMLDOMNode_Release(node);
        return;
    }

    r = IXMLDOMNode_get_firstChild(node_clone, &node_first);
    ok( r == S_OK, "ret %08x\n", r );
    if(r == S_OK)
    {
        IXMLDOMDocument *doc2;

        r = IXMLDOMNode_get_ownerDocument(node_clone, &doc2);
        ok( r == S_OK, "ret %08x\n", r );
        if(r == S_OK)
            IXMLDOMDocument_Release(doc2);

        IXMLDOMNode_Release(node_first);
    }

    r = IXMLDOMNode_get_childNodes(node, &pList);
    ok( r == S_OK, "ret %08x\n", r );
    if (pList)
	{
		IXMLDOMNodeList_get_length(pList, &nLength);
		IXMLDOMNodeList_Release(pList);
	}

    r = IXMLDOMNode_get_attributes(node, &mapAttr);
    ok( r == S_OK, "ret %08x\n", r );
    if(mapAttr)
    {
        IXMLDOMNamedNodeMap_get_length(mapAttr, &nAttrCnt);
        IXMLDOMNamedNodeMap_Release(mapAttr);
    }

    r = IXMLDOMNode_get_childNodes(node_clone, &pList);
    ok( r == S_OK, "ret %08x\n", r );
    if (pList)
	{
		IXMLDOMNodeList_get_length(pList, &nLength1);
		IXMLDOMNodeList_Release(pList);
	}

    r = IXMLDOMNode_get_attributes(node_clone, &mapAttr);
    ok( r == S_OK, "ret %08x\n", r );
    if(mapAttr)
    {
        IXMLDOMNamedNodeMap_get_length(mapAttr, &nAttrCnt1);
        IXMLDOMNamedNodeMap_Release(mapAttr);
    }

    ok(nLength == nLength1, "wrong Child count (%d, %d)\n", nLength, nLength1);
    ok(nAttrCnt == nAttrCnt1, "wrong Attribute count (%d, %d)\n", nAttrCnt, nAttrCnt1);
    IXMLDOMNode_Release(node_clone);

    /* No Children */
    r = IXMLDOMNode_cloneNode(node, VARIANT_FALSE, &node_clone);
    ok( r == S_OK, "ret %08x\n", r );
    ok( node_clone != NULL, "node %p\n", node );

    if(!node_clone)
    {
        IXMLDOMDocument_Release(doc);
        IXMLDOMNode_Release(node);
        return;
    }

    r = IXMLDOMNode_get_firstChild(node_clone, &node_first);
    ok( r == S_FALSE, "ret %08x\n", r );
    if(r == S_OK)
    {
        IXMLDOMDocument *doc2;

        r = IXMLDOMNode_get_ownerDocument(node_clone, &doc2);
        ok( r == S_OK, "ret %08x\n", r );
        if(r == S_OK)
            IXMLDOMDocument_Release(doc2);

        IXMLDOMNode_Release(node_first);
    }

    r = IXMLDOMNode_get_childNodes(node_clone, &pList);
    ok( r == S_OK, "ret %08x\n", r );
    if (pList)
    {
        IXMLDOMNodeList_get_length(pList, &nLength1);
        ok( nLength1 == 0, "Length should be 0 (%d)\n", nLength1);
        IXMLDOMNodeList_Release(pList);
    }

    r = IXMLDOMNode_get_attributes(node_clone, &mapAttr);
    ok( r == S_OK, "ret %08x\n", r );
    if(mapAttr)
    {
        IXMLDOMNamedNodeMap_get_length(mapAttr, &nAttrCnt1);
        ok( nAttrCnt1 == 3, "Attribute count should be 3 (%d)\n", nAttrCnt1);
        IXMLDOMNamedNodeMap_Release(mapAttr);
    }

    ok(nLength != nLength1, "wrong Child count (%d, %d)\n", nLength, nLength1);
    ok(nAttrCnt == nAttrCnt1, "wrong Attribute count (%d, %d)\n", nAttrCnt, nAttrCnt1);
    IXMLDOMNode_Release(node_clone);


    IXMLDOMNode_Release(node);
    IXMLDOMDocument_Release(doc);
}

static void test_xmlTypes(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *pRoot;
    HRESULT hr;
    IXMLDOMComment *pComment;
    IXMLDOMElement *pElement;
    IXMLDOMAttribute *pAttribute;
    IXMLDOMNamedNodeMap *pAttribs;
    IXMLDOMCDATASection *pCDataSec;
    IXMLDOMImplementation *pIXMLDOMImplementation = NULL;
    IXMLDOMDocumentFragment *pDocFrag = NULL;
    IXMLDOMEntityReference *pEntityRef = NULL;
    BSTR str;
    IXMLDOMNode *pNextChild;
    VARIANT v;
    LONG len = 0;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    pNextChild = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_get_nextSibling(doc, NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    pNextChild = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_get_nextSibling(doc, &pNextChild);
    ok(hr == S_FALSE, "ret %08x\n", hr );
    ok(pNextChild == NULL, "pDocChild not NULL\n");

    /* test previous Sibling */
    hr = IXMLDOMDocument_get_previousSibling(doc, NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    pNextChild = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_get_previousSibling(doc, &pNextChild);
    ok(hr == S_FALSE, "ret %08x\n", hr );
    ok(pNextChild == NULL, "pNextChild not NULL\n");

    /* test get_attributes */
    hr = IXMLDOMDocument_get_attributes( doc, NULL );
    ok( hr == E_INVALIDARG, "get_attributes returned wrong code\n");

    pAttribs = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_get_attributes( doc, &pAttribs);
    ok(hr == S_FALSE, "ret %08x\n", hr );
    ok( pAttribs == NULL, "pAttribs not NULL\n");

    /* test get_dataType */
    V_VT(&v) = VT_EMPTY;
    hr = IXMLDOMDocument_get_dataType(doc, &v);
    ok(hr == S_FALSE, "ret %08x\n", hr );
    ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
    VariantClear(&v);

    /* test nodeTypeString */
    str = NULL;
    hr = IXMLDOMDocument_get_nodeTypeString(doc, &str);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok( !lstrcmpW( str, _bstr_("document") ), "incorrect nodeTypeString string\n");
    SysFreeString(str);

    /* test implementation */
    hr = IXMLDOMDocument_get_implementation(doc, NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    hr = IXMLDOMDocument_get_implementation(doc, &pIXMLDOMImplementation);
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        VARIANT_BOOL hasFeature = VARIANT_TRUE;
        BSTR sEmpty = SysAllocStringLen(NULL, 0);

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, NULL, sEmpty, &hasFeature);
        ok(hr == E_INVALIDARG, "ret %08x\n", hr );

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, sEmpty, sEmpty, NULL);
        ok(hr == E_INVALIDARG, "ret %08x\n", hr );

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("DOM"), sEmpty, &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_FALSE, "hasFeature returned false\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, sEmpty, sEmpty, &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_FALSE, "hasFeature returned true\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("DOM"), NULL, &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_TRUE, "hasFeature returned false\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("DOM"), sEmpty, &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_FALSE, "hasFeature returned false\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("DOM"), _bstr_("1.0"), &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_TRUE, "hasFeature returned true\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("XML"), _bstr_("1.0"), &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_TRUE, "hasFeature returned true\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("MS-DOM"), _bstr_("1.0"), &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_TRUE, "hasFeature returned true\n");

        hr = IXMLDOMImplementation_hasFeature(pIXMLDOMImplementation, _bstr_("SSS"), NULL, &hasFeature);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(hasFeature == VARIANT_FALSE, "hasFeature returned false\n");

        SysFreeString(sEmpty);
        IXMLDOMImplementation_Release(pIXMLDOMImplementation);
    }

    pRoot = (IXMLDOMElement*)0x1;
    hr = IXMLDOMDocument_createElement(doc, NULL, &pRoot);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );
    ok(pRoot == (void*)0x1, "Expect same ptr, got %p\n", pRoot);

    pRoot = (IXMLDOMElement*)0x1;
    hr = IXMLDOMDocument_createElement(doc, _bstr_(""), &pRoot);
    ok(hr == E_FAIL, "ret %08x\n", hr );
    ok(pRoot == (void*)0x1, "Expect same ptr, got %p\n", pRoot);

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing"), &pRoot);
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)pRoot, NULL);
        ok(hr == S_OK, "ret %08x\n", hr );
        if(hr == S_OK)
        {
            /* Comment */
            str = SysAllocString(szComment);
            hr = IXMLDOMDocument_createComment(doc, str, &pComment);
            SysFreeString(str);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                /* test get_attributes */
                hr = IXMLDOMComment_get_attributes( pComment, NULL );
                ok( hr == E_INVALIDARG, "get_attributes returned wrong code\n");

                pAttribs = (IXMLDOMNamedNodeMap*)0x1;
                hr = IXMLDOMComment_get_attributes( pComment, &pAttribs);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( pAttribs == NULL, "pAttribs not NULL\n");

                /* test nodeTypeString */
                hr = IXMLDOMComment_get_nodeTypeString(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("comment") ), "incorrect nodeTypeString string\n");
                SysFreeString(str);

                hr = IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pComment, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_nodeName(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szCommentNodeText ), "incorrect comment node Name\n");
                SysFreeString(str);

                hr = IXMLDOMComment_get_xml(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szCommentXML ), "incorrect comment xml\n");
                SysFreeString(str);

                hr = IXMLDOMComment_get_dataType(pComment, &v);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
                VariantClear(&v);

                /* put data Tests */
                hr = IXMLDOMComment_put_data(pComment, _bstr_("This &is a ; test <>\\"));
                ok(hr == S_OK, "ret %08x\n", hr );

                /* get data Tests */
                hr = IXMLDOMComment_get_data(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\") ), "incorrect get_data string\n");
                SysFreeString(str);

                /* get data Tests */
                hr = IXMLDOMComment_get_nodeValue(pComment, &v);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( V_VT(&v) == VT_BSTR, "incorrect dataType type\n");
                ok( !lstrcmpW( V_BSTR(&v), _bstr_("This &is a ; test <>\\") ), "incorrect get_nodeValue string\n");
                VariantClear(&v);

                /* Confirm XML text is good */
                hr = IXMLDOMComment_get_xml(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("<!--This &is a ; test <>\\-->") ), "incorrect xml string\n");
                SysFreeString(str);

                /* Confirm we get the put_data Text back */
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\") ), "incorrect xml string\n");
                SysFreeString(str);

                /* test length property */
                hr = IXMLDOMComment_get_length(pComment, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 21, "expected 21 got %d\n", len);

                /* test substringData */
                hr = IXMLDOMComment_substringData(pComment, 0, 4, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                /* test substringData - Invalid offset */
                str = (BSTR)&szElement;
                hr = IXMLDOMComment_substringData(pComment, -1, 4, &str);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Invalid offset */
                str = (BSTR)&szElement;
                hr = IXMLDOMComment_substringData(pComment, 30, 0, &str);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Invalid size */
                str = (BSTR)&szElement;
                hr = IXMLDOMComment_substringData(pComment, 0, -1, &str);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Invalid size */
                str = (BSTR)&szElement;
                hr = IXMLDOMComment_substringData(pComment, 2, 0, &str);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Start of string */
                hr = IXMLDOMComment_substringData(pComment, 0, 4, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This") ), "incorrect substringData string\n");
                SysFreeString(str);

                /* test substringData - Middle of string */
                hr = IXMLDOMComment_substringData(pComment, 13, 4, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("test") ), "incorrect substringData string\n");
                SysFreeString(str);

                /* test substringData - End of string */
                hr = IXMLDOMComment_substringData(pComment, 20, 4, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("\\") ), "incorrect substringData string\n");
                SysFreeString(str);

                /* test appendData */
                hr = IXMLDOMComment_appendData(pComment, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_appendData(pComment, _bstr_(""));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_appendData(pComment, _bstr_("Append"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\Append") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* test insertData */
                str = SysAllocStringLen(NULL, 0);
                hr = IXMLDOMComment_insertData(pComment, -1, str);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, -1, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 1000, str);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 1000, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 0, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 0, str);
                ok(hr == S_OK, "ret %08x\n", hr );
                SysFreeString(str);

                hr = IXMLDOMComment_insertData(pComment, -1, _bstr_("Inserting"));
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 1000, _bstr_("Inserting"));
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 0, _bstr_("Begin "));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 17, _bstr_("Middle"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 39, _bstr_(" End"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("Begin This &is a Middle; test <>\\Append End") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete data */
                /* invalid arguments */
                hr = IXMLDOMComment_deleteData(pComment, -1, 1);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMComment_deleteData(pComment, 0, 0);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_deleteData(pComment, 0, -1);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_length(pComment, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 43, "expected 43 got %d\n", len);

                hr = IXMLDOMComment_deleteData(pComment, len, 1);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_deleteData(pComment, len+1, 1);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                /* delete from start */
                hr = IXMLDOMComment_deleteData(pComment, 0, 5);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_length(pComment, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 38, "expected 38 got %d\n", len);

                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_(" This &is a Middle; test <>\\Append End") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete from end */
                hr = IXMLDOMComment_deleteData(pComment, 35, 3);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_length(pComment, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 35, "expected 35 got %d\n", len);

                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_(" This &is a Middle; test <>\\Append ") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete from inside */
                hr = IXMLDOMComment_deleteData(pComment, 1, 33);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_get_length(pComment, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 2, "expected 2 got %d\n", len);

                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("  ") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete whole data ... */
                hr = IXMLDOMComment_get_length(pComment, &len);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_deleteData(pComment, 0, len);
                ok(hr == S_OK, "ret %08x\n", hr );
                /* ... and try again with empty string */
                hr = IXMLDOMComment_deleteData(pComment, 0, len);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* ::replaceData() */
                V_VT(&v) = VT_BSTR;
                V_BSTR(&v) = SysAllocString(szstr1);
                hr = IXMLDOMComment_put_nodeValue(pComment, v);
                ok(hr == S_OK, "ret %08x\n", hr );
                VariantClear(&v);

                hr = IXMLDOMComment_replaceData(pComment, 6, 0, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("str1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                hr = IXMLDOMComment_replaceData(pComment, 0, 0, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("str1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* NULL pointer means delete */
                hr = IXMLDOMComment_replaceData(pComment, 0, 1, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("tr1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* empty string means delete */
                hr = IXMLDOMComment_replaceData(pComment, 0, 1, _bstr_(""));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("r1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* zero count means insert */
                hr = IXMLDOMComment_replaceData(pComment, 0, 0, _bstr_("a"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("ar1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                hr = IXMLDOMComment_replaceData(pComment, 0, 2, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMComment_insertData(pComment, 0, _bstr_("m"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("m1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* nonempty string, count greater than its length */
                hr = IXMLDOMComment_replaceData(pComment, 0, 2, _bstr_("a1.2"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("a1.2") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* nonempty string, count less than its length */
                hr = IXMLDOMComment_replaceData(pComment, 0, 1, _bstr_("wine"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMComment_get_text(pComment, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("wine1.2") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                IXMLDOMComment_Release(pComment);
            }

            /* Element */
            str = SysAllocString(szElement);
            hr = IXMLDOMDocument_createElement(doc, str, &pElement);
            SysFreeString(str);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                hr = IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* test nodeTypeString */
                hr = IXMLDOMDocument_get_nodeTypeString(pElement, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("element") ), "incorrect nodeTypeString string\n");
                SysFreeString(str);

                hr = IXMLDOMElement_get_nodeName(pElement, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szElement ), "incorrect element node Name\n");
                SysFreeString(str);

                hr = IXMLDOMElement_get_xml(pElement, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szElementXML ), "incorrect element xml\n");
                SysFreeString(str);

                hr = IXMLDOMElement_get_dataType(pElement, &v);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
                VariantClear(&v);

                /* Attribute */
                pAttribute = (IXMLDOMAttribute*)0x1;
                hr = IXMLDOMDocument_createAttribute(doc, NULL, &pAttribute);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                ok(pAttribute == (void*)0x1, "Expect same ptr, got %p\n", pAttribute);

                pAttribute = (IXMLDOMAttribute*)0x1;
                hr = IXMLDOMDocument_createAttribute(doc, _bstr_(""), &pAttribute);
                ok(hr == E_FAIL, "ret %08x\n", hr );
                ok(pAttribute == (void*)0x1, "Expect same ptr, got %p\n", pAttribute);

                str = SysAllocString(szAttribute);
                hr = IXMLDOMDocument_createAttribute(doc, str, &pAttribute);
                SysFreeString(str);
                ok(hr == S_OK, "ret %08x\n", hr );
                if(hr == S_OK)
                {
                    IXMLDOMNode *pNewChild = (IXMLDOMNode *)0x1;

                    hr = IXMLDOMAttribute_get_nextSibling(pAttribute, NULL);
                    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                    pNextChild = (IXMLDOMNode *)0x1;
                    hr = IXMLDOMAttribute_get_nextSibling(pAttribute, &pNextChild);
                    ok(hr == S_FALSE, "ret %08x\n", hr );
                    ok(pNextChild == NULL, "pNextChild not NULL\n");

                    /* test Previous Sibling*/
                    hr = IXMLDOMAttribute_get_previousSibling(pAttribute, NULL);
                    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                    pNextChild = (IXMLDOMNode *)0x1;
                    hr = IXMLDOMAttribute_get_previousSibling(pAttribute, &pNextChild);
                    ok(hr == S_FALSE, "ret %08x\n", hr );
                    ok(pNextChild == NULL, "pNextChild not NULL\n");

                    /* test get_attributes */
                    hr = IXMLDOMAttribute_get_attributes( pAttribute, NULL );
                    ok( hr == E_INVALIDARG, "get_attributes returned wrong code\n");

                    pAttribs = (IXMLDOMNamedNodeMap*)0x1;
                    hr = IXMLDOMAttribute_get_attributes( pAttribute, &pAttribs);
                    ok(hr == S_FALSE, "ret %08x\n", hr );
                    ok( pAttribs == NULL, "pAttribs not NULL\n");

                    hr = IXMLDOMElement_appendChild(pElement, (IXMLDOMNode*)pAttribute, &pNewChild);
                    ok(hr == E_FAIL, "ret %08x\n", hr );
                    ok(pNewChild == NULL, "pNewChild not NULL\n");

                    hr = IXMLDOMElement_get_attributes(pElement, &pAttribs);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    if ( hr == S_OK )
                    {
                        hr = IXMLDOMNamedNodeMap_setNamedItem(pAttribs, (IXMLDOMNode*)pAttribute, NULL );
                        ok(hr == S_OK, "ret %08x\n", hr );

                        IXMLDOMNamedNodeMap_Release(pAttribs);
                    }

                    hr = IXMLDOMAttribute_get_nodeName(pAttribute, &str);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    ok( !lstrcmpW( str, szAttribute ), "incorrect attribute node Name\n");
                    SysFreeString(str);

                    /* test nodeTypeString */
                    hr = IXMLDOMAttribute_get_nodeTypeString(pAttribute, &str);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    ok( !lstrcmpW( str, _bstr_("attribute") ), "incorrect nodeTypeString string\n");
                    SysFreeString(str);

                    /* test nodeName */
                    hr = IXMLDOMAttribute_get_nodeName(pAttribute, &str);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    ok( !lstrcmpW( str, szAttribute ), "incorrect nodeName string\n");
                    SysFreeString(str);

                    /* test name property */
                    hr = IXMLDOMAttribute_get_name(pAttribute, &str);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    ok( !lstrcmpW( str, szAttribute ), "incorrect name string\n");
                    SysFreeString(str);

                    hr = IXMLDOMAttribute_get_xml(pAttribute, &str);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    ok( !lstrcmpW( str, szAttributeXML ), "incorrect attribute xml\n");
                    SysFreeString(str);

                    hr = IXMLDOMAttribute_get_dataType(pAttribute, &v);
                    ok(hr == S_FALSE, "ret %08x\n", hr );
                    ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
                    VariantClear(&v);

                    IXMLDOMAttribute_Release(pAttribute);

                    /* Check Element again with the Add Attribute*/
                    hr = IXMLDOMElement_get_xml(pElement, &str);
                    ok(hr == S_OK, "ret %08x\n", hr );
                    ok( !lstrcmpW( str, szElementXML2 ), "incorrect element xml\n");
                    SysFreeString(str);
                }

                hr = IXMLDOMElement_put_text(pElement, _bstr_("TestingNode"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMElement_get_xml(pElement, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szElementXML3 ), "incorrect element xml\n");
                SysFreeString(str);

                /* Test for reversible escaping */
                str = SysAllocString( szStrangeChars );
                hr = IXMLDOMElement_put_text(pElement, str);
                ok(hr == S_OK, "ret %08x\n", hr );
                SysFreeString( str );

                hr = IXMLDOMElement_get_xml(pElement, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szElementXML4 ), "incorrect element xml\n");
                SysFreeString(str);

                hr = IXMLDOMElement_get_text(pElement, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szStrangeChars ), "incorrect element text\n");
                SysFreeString(str);

                IXMLDOMElement_Release(pElement);
            }

            /* CData Section */
            str = SysAllocString(szCData);
            hr = IXMLDOMDocument_createCDATASection(doc, str, NULL);
            ok(hr == E_INVALIDARG, "ret %08x\n", hr );

            hr = IXMLDOMDocument_createCDATASection(doc, str, &pCDataSec);
            SysFreeString(str);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMNode *pNextChild = (IXMLDOMNode *)0x1;
                VARIANT var;

                VariantInit(&var);

                hr = IXMLDOMCDATASection_QueryInterface(pCDataSec, &IID_IXMLDOMElement, (void**)&pElement);
                ok(hr == E_NOINTERFACE, "ret %08x\n", hr);

                hr = IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pCDataSec, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* get Attribute Tests */
                hr = IXMLDOMCDATASection_get_attributes(pCDataSec, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                pAttribs = (IXMLDOMNamedNodeMap*)0x1;
                hr = IXMLDOMCDATASection_get_attributes(pCDataSec, &pAttribs);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(pAttribs == NULL, "pAttribs != NULL\n");

                hr = IXMLDOMCDATASection_get_nodeName(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szCDataNodeText ), "incorrect cdata node Name\n");
                SysFreeString(str);

                hr = IXMLDOMCDATASection_get_xml(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szCDataXML ), "incorrect cdata xml\n");
                SysFreeString(str);

                /* test lastChild */
                pNextChild = (IXMLDOMNode*)0x1;
                hr = IXMLDOMCDATASection_get_lastChild(pCDataSec, &pNextChild);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(pNextChild == NULL, "pNextChild not NULL\n");

                /* test get_dataType */
                hr = IXMLDOMCDATASection_get_dataType(pCDataSec, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_dataType(pCDataSec, &v);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
                VariantClear(&v);

                /* test nodeTypeString */
                hr = IXMLDOMCDATASection_get_nodeTypeString(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("cdatasection") ), "incorrect nodeTypeString string\n");
                SysFreeString(str);

                /* put data Tests */
                hr = IXMLDOMCDATASection_put_data(pCDataSec, _bstr_("This &is a ; test <>\\"));
                ok(hr == S_OK, "ret %08x\n", hr );

                /* Confirm XML text is good */
                hr = IXMLDOMCDATASection_get_xml(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("<![CDATA[This &is a ; test <>\\]]>") ), "incorrect xml string\n");
                SysFreeString(str);

                /* Confirm we get the put_data Text back */
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\") ), "incorrect text string\n");
                SysFreeString(str);

                /* test length property */
                hr = IXMLDOMCDATASection_get_length(pCDataSec, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 21, "expected 21 got %d\n", len);

                /* test get nodeValue */
                hr = IXMLDOMCDATASection_get_nodeValue(pCDataSec, &var);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(V_VT(&var) == VT_BSTR, "got vt %04x\n", V_VT(&var));
                ok( !lstrcmpW( V_BSTR(&var), _bstr_("This &is a ; test <>\\") ), "incorrect text string\n");
                VariantClear(&var);

                /* test get data */
                hr = IXMLDOMCDATASection_get_data(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\") ), "incorrect text string\n");
                SysFreeString(str);

                /* test substringData */
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 0, 4, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                /* test substringData - Invalid offset */
                str = (BSTR)&szElement;
                hr = IXMLDOMCDATASection_substringData(pCDataSec, -1, 4, &str);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Invalid offset */
                str = (BSTR)&szElement;
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 30, 0, &str);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Invalid size */
                str = (BSTR)&szElement;
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 0, -1, &str);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Invalid size */
                str = (BSTR)&szElement;
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 2, 0, &str);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( str == NULL, "incorrect string\n");

                /* test substringData - Start of string */
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 0, 4, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This") ), "incorrect substringData string\n");
                SysFreeString(str);

                /* test substringData - Middle of string */
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 13, 4, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("test") ), "incorrect substringData string\n");
                SysFreeString(str);

                /* test substringData - End of string */
                hr = IXMLDOMCDATASection_substringData(pCDataSec, 20, 4, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("\\") ), "incorrect substringData string\n");
                SysFreeString(str);

                /* test appendData */
                hr = IXMLDOMCDATASection_appendData(pCDataSec, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_appendData(pCDataSec, _bstr_(""));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_appendData(pCDataSec, _bstr_("Append"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("This &is a ; test <>\\Append") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* test insertData */
                str = SysAllocStringLen(NULL, 0);
                hr = IXMLDOMCDATASection_insertData(pCDataSec, -1, str);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, -1, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 1000, str);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 1000, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 0, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 0, str);
                ok(hr == S_OK, "ret %08x\n", hr );
                SysFreeString(str);

                hr = IXMLDOMCDATASection_insertData(pCDataSec, -1, _bstr_("Inserting"));
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 1000, _bstr_("Inserting"));
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 0, _bstr_("Begin "));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 17, _bstr_("Middle"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 39, _bstr_(" End"));
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("Begin This &is a Middle; test <>\\Append End") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete data */
                /* invalid arguments */
                hr = IXMLDOMCDATASection_deleteData(pCDataSec, -1, 1);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 0, 0);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 0, -1);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_length(pCDataSec, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 43, "expected 43 got %d\n", len);

                hr = IXMLDOMCDATASection_deleteData(pCDataSec, len, 1);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_deleteData(pCDataSec, len+1, 1);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                /* delete from start */
                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 0, 5);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_length(pCDataSec, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 38, "expected 38 got %d\n", len);

                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_(" This &is a Middle; test <>\\Append End") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete from end */
                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 35, 3);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_length(pCDataSec, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 35, "expected 35 got %d\n", len);

                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_(" This &is a Middle; test <>\\Append ") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete from inside */
                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 1, 33);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_get_length(pCDataSec, &len);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok(len == 2, "expected 2 got %d\n", len);

                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("  ") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* delete whole data ... */
                hr = IXMLDOMCDATASection_get_length(pCDataSec, &len);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 0, len);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* ... and try again with empty string */
                hr = IXMLDOMCDATASection_deleteData(pCDataSec, 0, len);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* ::replaceData() */
                V_VT(&v) = VT_BSTR;
                V_BSTR(&v) = SysAllocString(szstr1);
                hr = IXMLDOMCDATASection_put_nodeValue(pCDataSec, v);
                ok(hr == S_OK, "ret %08x\n", hr );
                VariantClear(&v);

                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 6, 0, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("str1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 0, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("str1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* NULL pointer means delete */
                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 1, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("tr1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* empty string means delete */
                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 1, _bstr_(""));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("r1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* zero count means insert */
                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 0, _bstr_("a"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("ar1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 2, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMCDATASection_insertData(pCDataSec, 0, _bstr_("m"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("m1") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* nonempty string, count greater than its length */
                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 2, _bstr_("a1.2"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("a1.2") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                /* nonempty string, count less than its length */
                hr = IXMLDOMCDATASection_replaceData(pCDataSec, 0, 1, _bstr_("wine"));
                ok(hr == S_OK, "ret %08x\n", hr );
                hr = IXMLDOMCDATASection_get_text(pCDataSec, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("wine1.2") ), "incorrect get_text string, got '%s'\n", wine_dbgstr_w(str) );
                SysFreeString(str);

                IXMLDOMCDATASection_Release(pCDataSec);
            }

            /* Document Fragments */
            hr = IXMLDOMDocument_createDocumentFragment(doc, NULL);
            ok(hr == E_INVALIDARG, "ret %08x\n", hr );

            hr = IXMLDOMDocument_createDocumentFragment(doc, &pDocFrag);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMNode *node;

                hr = IXMLDOMDocumentFragment_get_parentNode(pDocFrag, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                node = (IXMLDOMNode *)0x1;
                hr = IXMLDOMDocumentFragment_get_parentNode(pDocFrag, &node);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(node == NULL, "expected NULL, got %p\n", node);

                hr = IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pDocFrag, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* get Attribute Tests */
                hr = IXMLDOMDocumentFragment_get_attributes(pDocFrag, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                pAttribs = (IXMLDOMNamedNodeMap*)0x1;
                hr = IXMLDOMDocumentFragment_get_attributes(pDocFrag, &pAttribs);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(pAttribs == NULL, "pAttribs != NULL\n");

                hr = IXMLDOMDocumentFragment_get_nodeName(pDocFrag, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szDocFragmentText ), "incorrect docfragment node Name\n");
                SysFreeString(str);

                /* test next Sibling*/
                hr = IXMLDOMDocumentFragment_get_nextSibling(pDocFrag, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                node = (IXMLDOMNode *)0x1;
                hr = IXMLDOMDocumentFragment_get_nextSibling(pDocFrag, &node);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(node == NULL, "next sibling not NULL\n");

                /* test Previous Sibling*/
                hr = IXMLDOMDocumentFragment_get_previousSibling(pDocFrag, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                node = (IXMLDOMNode *)0x1;
                hr = IXMLDOMDocumentFragment_get_previousSibling(pDocFrag, &node);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(node == NULL, "previous sibling not NULL\n");

                /* test get_dataType */
                hr = IXMLDOMDocumentFragment_get_dataType(pDocFrag, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                hr = IXMLDOMDocumentFragment_get_dataType(pDocFrag, &v);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
                VariantClear(&v);

                /* test nodeTypeString */
                hr = IXMLDOMDocumentFragment_get_nodeTypeString(pDocFrag, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("documentfragment") ), "incorrect nodeTypeString string\n");
                SysFreeString(str);

                IXMLDOMDocumentFragment_Release(pDocFrag);
            }

            /* Entity References */
            hr = IXMLDOMDocument_createEntityReference(doc, NULL, &pEntityRef);
            ok(hr == E_FAIL, "ret %08x\n", hr );
            hr = IXMLDOMDocument_createEntityReference(doc, _bstr_(""), &pEntityRef);
            ok(hr == E_FAIL, "ret %08x\n", hr );

            str = SysAllocString(szEntityRef);
            hr = IXMLDOMDocument_createEntityReference(doc, str, NULL);
            ok(hr == E_INVALIDARG, "ret %08x\n", hr );

            hr = IXMLDOMDocument_createEntityReference(doc, str, &pEntityRef);
            SysFreeString(str);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                hr = IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pEntityRef, NULL);
                ok(hr == S_OK, "ret %08x\n", hr );

                /* get Attribute Tests */
                hr = IXMLDOMEntityReference_get_attributes(pEntityRef, NULL);
                ok(hr == E_INVALIDARG, "ret %08x\n", hr );

                pAttribs = (IXMLDOMNamedNodeMap*)0x1;
                hr = IXMLDOMEntityReference_get_attributes(pEntityRef, &pAttribs);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok(pAttribs == NULL, "pAttribs != NULL\n");

                /* test dataType */
                hr = IXMLDOMEntityReference_get_dataType(pEntityRef, &v);
                ok(hr == S_FALSE, "ret %08x\n", hr );
                ok( V_VT(&v) == VT_NULL, "incorrect dataType type\n");
                VariantClear(&v);

                /* test nodeTypeString */
                hr = IXMLDOMEntityReference_get_nodeTypeString(pEntityRef, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, _bstr_("entityreference") ), "incorrect nodeTypeString string\n");
                SysFreeString(str);

                /* test get_xml*/
                hr = IXMLDOMEntityReference_get_xml(pEntityRef, &str);
                ok(hr == S_OK, "ret %08x\n", hr );
                ok( !lstrcmpW( str, szEntityRefXML ), "incorrect xml string\n");
                SysFreeString(str);

                IXMLDOMEntityReference_Release(pEntityRef);
            }

            IXMLDOMElement_Release( pRoot );
        }
    }

    IXMLDOMDocument_Release(doc);

    free_bstrs();
}

static void test_nodeTypeTests( void )
{
    IXMLDOMDocument *doc = NULL;
    IXMLDOMElement *pRoot;
    IXMLDOMElement *pElement;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing"), NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing"), &pRoot);
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)pRoot, NULL);
        ok(hr == S_OK, "ret %08x\n", hr );
        if(hr == S_OK)
        {
            hr = IXMLDOMElement_put_dataType(pRoot, NULL);
            ok(hr == E_INVALIDARG, "ret %08x\n", hr );

            /* Invalid Value */
            hr = IXMLDOMElement_put_dataType(pRoot, _bstr_("abcdefg") );
            ok(hr == E_FAIL, "ret %08x\n", hr );

            /* NOTE:
             *   The name passed into put_dataType is case-insensitive. So many of the names
             *     have been changed to reflect this.
             */
            /* Boolean */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Boolean"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("Boolean") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* String */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_String"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("String") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Number */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Number"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("number") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Int */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Int"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("InT") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Fixed */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Fixed"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("fixed.14.4") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* DateTime */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_DateTime"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("DateTime") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* DateTime TZ */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_DateTime_tz"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("DateTime.tz") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Date */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Date"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("Date") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Time */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Time"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("Time") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Time.tz */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Time_TZ"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("Time.tz") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* I1 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_I1"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("I1") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* I2 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_I2"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("I2") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* I4 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_I4"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("I4") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* UI1 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_UI1"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("UI1") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* UI2 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_UI2"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("UI2") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* UI4 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_UI4"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("UI4") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* r4 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_r4"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("r4") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* r8 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_r8"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("r8") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* float */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_float"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("float") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* uuid */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_uuid"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("UuId") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* bin.hex */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_bin_hex"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("bin.hex") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* bin.base64 */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_bin_base64"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("bin.base64") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            /* Check changing types */
            hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing_Change"), &pElement);
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                IXMLDOMElement_appendChild(pRoot, (IXMLDOMNode*)pElement, NULL);

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("DateTime.tz") );
                ok(hr == S_OK, "ret %08x\n", hr );

                hr = IXMLDOMElement_put_dataType(pElement, _bstr_("string") );
                ok(hr == S_OK, "ret %08x\n", hr );

                IXMLDOMElement_Release(pElement);
            }

            IXMLDOMElement_Release(pRoot);
        }
    }

    IXMLDOMDocument_Release(doc);

    free_bstrs();
}

static void test_DocumentSaveToDocument(void)
{
    IXMLDOMDocument *doc, *doc2;
    IXMLDOMElement *pRoot;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    doc2 = create_document(&IID_IXMLDOMDocument);
    if (!doc2)
    {
        IXMLDOMDocument_Release(doc);
        return;
    }

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing"), &pRoot);
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)pRoot, NULL);
        ok(hr == S_OK, "ret %08x\n", hr );
        if(hr == S_OK)
        {
            VARIANT vDoc;
            BSTR sOrig;
            BSTR sNew;

            V_VT(&vDoc) = VT_UNKNOWN;
            V_UNKNOWN(&vDoc) = (IUnknown*)doc2;

            hr = IXMLDOMDocument_save(doc, vDoc);
            ok(hr == S_OK, "ret %08x\n", hr );

            hr = IXMLDOMDocument_get_xml(doc, &sOrig);
            ok(hr == S_OK, "ret %08x\n", hr );

            hr = IXMLDOMDocument_get_xml(doc2, &sNew);
            ok(hr == S_OK, "ret %08x\n", hr );

            ok( !lstrcmpW( sOrig, sNew ), "New document is not the same as origial\n");

            SysFreeString(sOrig);
            SysFreeString(sNew);
        }
    }

    IXMLDOMDocument_Release(doc2);
    IXMLDOMDocument_Release(doc);
}

static void test_DocumentSaveToFile(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *pRoot;
    HANDLE file;
    char buffer[100];
    DWORD read = 0;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Testing"), &pRoot);
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        hr = IXMLDOMDocument_appendChild(doc, (IXMLDOMNode*)pRoot, NULL);
        ok(hr == S_OK, "ret %08x\n", hr );
        if(hr == S_OK)
        {
            VARIANT vFile;

            V_VT(&vFile) = VT_BSTR;
            V_BSTR(&vFile) = _bstr_("test.xml");

            hr = IXMLDOMDocument_save(doc, vFile);
            ok(hr == S_OK, "ret %08x\n", hr );
        }
    }

    IXMLDOMElement_Release(pRoot);
    IXMLDOMDocument_Release(doc);

    file = CreateFile("test.xml", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    ok(file != INVALID_HANDLE_VALUE, "Could not open file: %u\n", GetLastError());
    if(file == INVALID_HANDLE_VALUE)
        return;

    ReadFile(file, buffer, sizeof(buffer), &read, NULL);
    ok(read != 0, "could not read file\n");
    ok(buffer[0] != '<' || buffer[1] != '?', "File contains processing instruction\n");

    CloseHandle(file);
    DeleteFile("test.xml");
}

static void test_testTransforms(void)
{
    IXMLDOMDocument *doc, *docSS;
    IXMLDOMNode *pNode;
    VARIANT_BOOL bSucc;

    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    docSS = create_document(&IID_IXMLDOMDocument);
    if (!docSS)
    {
        IXMLDOMDocument_Release(doc);
        return;
    }

    hr = IXMLDOMDocument_loadXML(doc, _bstr_(szTransformXML), &bSucc);
    ok(hr == S_OK, "ret %08x\n", hr );

    hr = IXMLDOMDocument_loadXML(docSS, _bstr_(szTransformSSXML), &bSucc);
    ok(hr == S_OK, "ret %08x\n", hr );

    hr = IXMLDOMDocument_QueryInterface(docSS, &IID_IXMLDOMNode, (void**)&pNode );
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        BSTR bOut;

        hr = IXMLDOMDocument_transformNode(doc, pNode, &bOut);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok( compareIgnoreReturns( bOut, _bstr_(szTransformOutput)), "Stylesheet output not correct\n");
        SysFreeString(bOut);

        IXMLDOMNode_Release(pNode);
    }

    IXMLDOMDocument_Release(docSS);
    IXMLDOMDocument_Release(doc);

    free_bstrs();
}

static void test_Namespaces(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMNode *pNode;
    IXMLDOMNode *pNode2 = NULL;
    VARIANT_BOOL bSucc;
    HRESULT hr;
    BSTR str;
    static  const CHAR szNamespacesXML[] =
"<?xml version=\"1.0\"?>\n"
"<root xmlns:WEB='http://www.winehq.org'>\n"
"<WEB:Site version=\"1.0\" />\n"
"</root>";

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_loadXML(doc, _bstr_(szNamespacesXML), &bSucc);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(bSucc == VARIANT_TRUE, "Expected VARIANT_TRUE got VARIANT_FALSE\n");

    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("root"), &pNode );
    ok(hr == S_OK, "ret %08x\n", hr );
    if(hr == S_OK)
    {
        hr = IXMLDOMNode_get_firstChild( pNode, &pNode2 );
        ok( hr == S_OK, "ret %08x\n", hr );
        ok( pNode2 != NULL, "pNode2 == NULL\n");

        /* Test get_prefix */
        hr = IXMLDOMNode_get_prefix(pNode2, NULL);
        ok( hr == E_INVALIDARG, "ret %08x\n", hr );
        /* NOTE: Need to test that arg2 gets cleared on Error. */

        hr = IXMLDOMNode_get_prefix(pNode2, &str);
        ok( hr == S_OK, "ret %08x\n", hr );
        ok( !lstrcmpW( str, _bstr_("WEB")), "incorrect prefix string\n");
        SysFreeString(str);

        /* Test get_namespaceURI */
        hr = IXMLDOMNode_get_namespaceURI(pNode2, NULL);
        ok( hr == E_INVALIDARG, "ret %08x\n", hr );
        /* NOTE: Need to test that arg2 gets cleared on Error. */

        hr = IXMLDOMNode_get_namespaceURI(pNode2, &str);
        ok( hr == S_OK, "ret %08x\n", hr );
        ok( !lstrcmpW( str, _bstr_("http://www.winehq.org")), "incorrect namespaceURI string\n");
        SysFreeString(str);

        IXMLDOMNode_Release(pNode2);
        IXMLDOMNode_Release(pNode);
    }

    IXMLDOMDocument_Release(doc);

    free_bstrs();
}

static void test_FormattingXML(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *pElement;
    VARIANT_BOOL bSucc;
    HRESULT hr;
    BSTR str;
    static const CHAR szLinefeedXML[] = "<?xml version=\"1.0\"?>\n<Root>\n\t<Sub val=\"A\" />\n</Root>";
    static const CHAR szLinefeedRootXML[] = "<Root>\r\n\t<Sub val=\"A\"/>\r\n</Root>";

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_loadXML(doc, _bstr_(szLinefeedXML), &bSucc);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(bSucc == VARIANT_TRUE, "Expected VARIANT_TRUE got VARIANT_FALSE\n");

    if(bSucc == VARIANT_TRUE)
    {
        hr = IXMLDOMDocument_get_documentElement(doc, &pElement);
        ok(hr == S_OK, "ret %08x\n", hr );
        if(hr == S_OK)
        {
            hr = IXMLDOMElement_get_xml(pElement, &str);
            ok(hr == S_OK, "ret %08x\n", hr );
            ok( !lstrcmpW( str, _bstr_(szLinefeedRootXML) ), "incorrect element xml\n");
            SysFreeString(str);

            IXMLDOMElement_Release(pElement);
        }
    }

    IXMLDOMDocument_Release(doc);

    free_bstrs();
}

typedef struct _nodetypedvalue_t {
    const char *name;
    VARTYPE type;
    const char *value; /* value in string format */
} nodetypedvalue_t;

static const nodetypedvalue_t get_nodetypedvalue[] = {
    { "root/string",    VT_BSTR, "Wine" },
    { "root/string2",   VT_BSTR, "String" },
    { "root/number",    VT_BSTR, "12.44" },
    { "root/number2",   VT_BSTR, "-3.71e3" },
    { "root/int",       VT_I4,   "-13" },
    { "root/fixed",     VT_CY,   "7322.9371" },
    { "root/bool",      VT_BOOL, "-1" },
    { "root/datetime",  VT_DATE, "40135.14" },
    { "root/datetimetz",VT_DATE, "37813.59" },
    { "root/date",      VT_DATE, "665413" },
    { "root/time",      VT_DATE, "0.5813889" },
    { "root/timetz",    VT_DATE, "1.112512" },
    { "root/i1",        VT_I1,   "-13" },
    { "root/i2",        VT_I2,   "31915" },
    { "root/i4",        VT_I4,   "-312232" },
    { "root/ui1",       VT_UI1,  "123" },
    { "root/ui2",       VT_UI2,  "48282" },
    { "root/ui4",       VT_UI4,  "949281" },
    { "root/r4",        VT_R4,   "213124" },
    { "root/r8",        VT_R8,   "0.412" },
    { "root/float",     VT_R8,   "41221.421" },
    { "root/uuid",      VT_BSTR, "333C7BC4-460F-11D0-BC04-0080C7055a83" },
    { 0 }
};

static void test_nodeTypedValue(void)
{
    const nodetypedvalue_t *entry = get_nodetypedvalue;
    IXMLDOMDocument *doc;
    IXMLDOMNode *node;
    VARIANT_BOOL b;
    VARIANT value;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    b = VARIANT_FALSE;
    hr = IXMLDOMDocument_loadXML(doc, _bstr_(szTypeValueXML), &b);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(b == VARIANT_TRUE, "got %d\n", b);

    hr = IXMLDOMDocument_get_nodeValue(doc, NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    V_VT(&value) = VT_BSTR;
    V_BSTR(&value) = NULL;
    hr = IXMLDOMDocument_get_nodeValue(doc, &value);
    ok(hr == S_FALSE, "ret %08x\n", hr );
    ok(V_VT(&value) == VT_NULL, "expect VT_NULL got %d\n", V_VT(&value));

    hr = IXMLDOMDocument_get_nodeTypedValue(doc, NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    hr = IXMLDOMDocument_get_nodeTypedValue(doc, &value);
    ok(hr == S_FALSE, "ret %08x\n", hr );

    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("root/string"), &node);
    ok(hr == S_OK, "ret %08x\n", hr );

    V_VT(&value) = VT_BSTR;
    V_BSTR(&value) = NULL;
    hr = IXMLDOMNode_get_nodeValue(node, &value);
    ok(hr == S_FALSE, "ret %08x\n", hr );
    ok(V_VT(&value) == VT_NULL, "expect VT_NULL got %d\n", V_VT(&value));

    hr = IXMLDOMNode_get_nodeTypedValue(node, NULL);
    ok(hr == E_INVALIDARG, "ret %08x\n", hr );

    IXMLDOMNode_Release(node);

    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("root/binhex"), &node);
    ok(hr == S_OK, "ret %08x\n", hr );
    {
        BYTE bytes[] = {0xff,0xfc,0xa0,0x12,0x00,0x3c};

        hr = IXMLDOMNode_get_nodeTypedValue(node, &value);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(V_VT(&value) == (VT_ARRAY|VT_UI1), "incorrect type\n");
        ok(V_ARRAY(&value)->rgsabound[0].cElements == 6, "incorrect array size\n");
        if(V_ARRAY(&value)->rgsabound[0].cElements == 6)
            ok(!memcmp(bytes, V_ARRAY(&value)->pvData, sizeof(bytes)), "incorrect value\n");
        VariantClear(&value);
        IXMLDOMNode_Release(node);
    }

    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("root/binbase64"), &node);
    ok(hr == S_OK, "ret %08x\n", hr );
    {
        BYTE bytes[] = {0x62,0x61,0x73,0x65,0x36,0x34,0x20,0x74,0x65,0x73,0x74};

        hr = IXMLDOMNode_get_nodeTypedValue(node, &value);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(V_VT(&value) == (VT_ARRAY|VT_UI1), "incorrect type\n");
        ok(V_ARRAY(&value)->rgsabound[0].cElements == 11, "incorrect array size\n");
        if(V_ARRAY(&value)->rgsabound[0].cElements == 11)
            ok(!memcmp(bytes, V_ARRAY(&value)->pvData, sizeof(bytes)), "incorrect value\n");
        VariantClear(&value);
        IXMLDOMNode_Release(node);
    }

    while (entry->name)
    {
        hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_(entry->name), &node);
        ok(hr == S_OK, "ret %08x\n", hr );

        hr = IXMLDOMNode_get_nodeTypedValue(node, &value);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(V_VT(&value) == entry->type, "incorrect type, expected %d, got %d\n", entry->type, V_VT(&value));

        if (entry->type != VT_BSTR)
        {
           if (entry->type == VT_DATE ||
               entry->type == VT_R8 ||
               entry->type == VT_CY)
           {
               if (entry->type == VT_DATE)
               {
                   hr = VariantChangeType(&value, &value, 0, VT_R4);
                   ok(hr == S_OK, "ret %08x\n", hr );
               }
               hr = VariantChangeTypeEx(&value, &value,
                                        MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT),
                                        VARIANT_NOUSEROVERRIDE, VT_BSTR);
               ok(hr == S_OK, "ret %08x\n", hr );
           }
           else
           {
               hr = VariantChangeType(&value, &value, 0, VT_BSTR);
               ok(hr == S_OK, "ret %08x\n", hr );
           }

           ok(lstrcmpW( V_BSTR(&value), _bstr_(entry->value)) == 0,
              "expected %s, got %s\n", entry->value, wine_dbgstr_w(V_BSTR(&value)));
        }
        else
           ok(lstrcmpW( V_BSTR(&value), _bstr_(entry->value)) == 0,
               "expected %s, got %s\n", entry->value, wine_dbgstr_w(V_BSTR(&value)));

        VariantClear( &value );
        IXMLDOMNode_Release(node);

        entry++;
    }

    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_TransformWithLoadingLocalFile(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMDocument *xsl;
    IXMLDOMNode *pNode;
    VARIANT_BOOL bSucc;
    HRESULT hr;
    HANDLE file;
    DWORD dwWritten;
    char lpPathBuffer[MAX_PATH];
    int i;

    /* Create a Temp File. */
    GetTempPathA(MAX_PATH, lpPathBuffer);
    strcat(lpPathBuffer, "customers.xml" );

    file = CreateFile(lpPathBuffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    ok(file != INVALID_HANDLE_VALUE, "Could not create file: %u\n", GetLastError());
    if(file == INVALID_HANDLE_VALUE)
        return;

    WriteFile(file, szBasicTransformXML, strlen(szBasicTransformXML), &dwWritten, NULL);
    CloseHandle(file);

    /* Correct path to not include a escape character. */
    for(i=0; i < strlen(lpPathBuffer); i++)
    {
        if(lpPathBuffer[i] == '\\')
            lpPathBuffer[i] = '/';
    }

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    xsl = create_document(&IID_IXMLDOMDocument);
    if (!xsl)
    {
        IXMLDOMDocument2_Release(doc);
        return;
    }

    hr = IXMLDOMDocument_loadXML(doc, _bstr_(szTypeValueXML), &bSucc);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(bSucc == VARIANT_TRUE, "Expected VARIANT_TRUE got VARIANT_FALSE\n");
    if(bSucc == VARIANT_TRUE)
    {
        BSTR sXSL;
        BSTR sPart1 = _bstr_(szBasicTransformSSXMLPart1);
        BSTR sPart2 = _bstr_(szBasicTransformSSXMLPart2);
        BSTR sFileName = _bstr_(lpPathBuffer);
        int nLegnth = lstrlenW(sPart1) + lstrlenW(sPart2) + lstrlenW(sFileName) + 1;

        sXSL = SysAllocStringLen(NULL, nLegnth);
        lstrcpyW(sXSL, sPart1);
        lstrcatW(sXSL, sFileName);
        lstrcatW(sXSL, sPart2);

        hr = IXMLDOMDocument_loadXML(xsl, sXSL, &bSucc);
        ok(hr == S_OK, "ret %08x\n", hr );
        ok(bSucc == VARIANT_TRUE, "Expected VARIANT_TRUE got VARIANT_FALSE\n");
        if(bSucc == VARIANT_TRUE)
        {
            BSTR sResult;

            hr = IXMLDOMDocument_QueryInterface(xsl, &IID_IXMLDOMNode, (void**)&pNode );
            ok(hr == S_OK, "ret %08x\n", hr );
            if(hr == S_OK)
            {
                /* This will load the temp file via the XSL */
                hr = IXMLDOMDocument_transformNode(doc, pNode, &sResult);
                ok(hr == S_OK, "ret %08x\n", hr );
                if(hr == S_OK)
                {
                    ok( compareIgnoreReturns( sResult, _bstr_(szBasicTransformOutput)), "Stylesheet output not correct\n");
                    SysFreeString(sResult);
                }

                IXMLDOMNode_Release(pNode);
            }
        }

        SysFreeString(sXSL);
    }

    IXMLDOMDocument_Release(doc);
    IXMLDOMDocument_Release(xsl);

    DeleteFile(lpPathBuffer);
    free_bstrs();
}

static void test_put_nodeValue(void)
{
    static const WCHAR jeevesW[] = {'J','e','e','v','e','s',' ','&',' ','W','o','o','s','t','e','r',0};
    IXMLDOMDocument *doc;
    IXMLDOMText *text;
    IXMLDOMEntityReference *entityref;
    IXMLDOMAttribute *attr;
    IXMLDOMNode *node;
    HRESULT hr;
    VARIANT data, type;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    /* test for unsupported types */
    /* NODE_DOCUMENT */
    hr = IXMLDOMDocument_QueryInterface(doc, &IID_IXMLDOMNode, (void**)&node);
    ok(hr == S_OK, "ret %08x\n", hr );
    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("one two three");
    hr = IXMLDOMNode_put_nodeValue(node, data);
    ok(hr == E_FAIL, "ret %08x\n", hr );
    IXMLDOMNode_Release(node);

    /* NODE_DOCUMENT_FRAGMENT */
    V_VT(&type) = VT_I1;
    V_I1(&type) = NODE_DOCUMENT_FRAGMENT;
    hr = IXMLDOMDocument_createNode(doc, type, _bstr_("test"), NULL, &node);
    ok(hr == S_OK, "ret %08x\n", hr );
    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("one two three");
    hr = IXMLDOMNode_put_nodeValue(node, data);
    ok(hr == E_FAIL, "ret %08x\n", hr );
    IXMLDOMNode_Release(node);

    /* NODE_ELEMENT */
    V_VT(&type) = VT_I1;
    V_I1(&type) = NODE_ELEMENT;
    hr = IXMLDOMDocument_createNode(doc, type, _bstr_("test"), NULL, &node);
    ok(hr == S_OK, "ret %08x\n", hr );
    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("one two three");
    hr = IXMLDOMNode_put_nodeValue(node, data);
    ok(hr == E_FAIL, "ret %08x\n", hr );
    IXMLDOMNode_Release(node);

    /* NODE_ENTITY_REFERENCE */
    hr = IXMLDOMDocument_createEntityReference(doc, _bstr_("ref"), &entityref);
    ok(hr == S_OK, "ret %08x\n", hr );

    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("one two three");
    hr = IXMLDOMEntityReference_put_nodeValue(entityref, data);
    ok(hr == E_FAIL, "ret %08x\n", hr );

    hr = IXMLDOMEntityReference_QueryInterface(entityref, &IID_IXMLDOMNode, (void**)&node);
    ok(hr == S_OK, "ret %08x\n", hr );
    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("one two three");
    hr = IXMLDOMNode_put_nodeValue(node, data);
    ok(hr == E_FAIL, "ret %08x\n", hr );
    IXMLDOMNode_Release(node);
    IXMLDOMEntityReference_Release(entityref);

    /* supported types */
    hr = IXMLDOMDocument_createTextNode(doc, _bstr_(""), &text);
    ok(hr == S_OK, "ret %08x\n", hr );
    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("Jeeves & Wooster");
    hr = IXMLDOMText_put_nodeValue(text, data);
    ok(hr == S_OK, "ret %08x\n", hr );
    IXMLDOMText_Release(text);

    hr = IXMLDOMDocument_createAttribute(doc, _bstr_("attr"), &attr);
    ok(hr == S_OK, "ret %08x\n", hr );
    V_VT(&data) = VT_BSTR;
    V_BSTR(&data) = _bstr_("Jeeves & Wooster");
    hr = IXMLDOMAttribute_put_nodeValue(attr, data);
    ok(hr == S_OK, "ret %08x\n", hr );
    hr = IXMLDOMAttribute_get_nodeValue(attr, &data);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(memcmp(V_BSTR(&data), jeevesW, sizeof(jeevesW)) == 0, "got %s\n",
        wine_dbgstr_w(V_BSTR(&data)));
    VariantClear(&data);
    IXMLDOMAttribute_Release(attr);

    free_bstrs();

    IXMLDOMDocument_Release(doc);
}

static void test_IObjectSafety_set(IObjectSafety *safety, HRESULT result, HRESULT result2, DWORD set, DWORD mask, DWORD expected, DWORD expected2)
{
    DWORD enabled, supported;
    HRESULT hr;

    hr = IObjectSafety_SetInterfaceSafetyOptions(safety, NULL, set, mask);
    if (result == result2)
        ok(hr == result, "SetInterfaceSafetyOptions: expected %08x, returned %08x\n", result, hr );
    else
        ok(broken(hr == result) || hr == result2,
           "SetInterfaceSafetyOptions: expected %08x, got %08x\n", result2, hr );

    supported = enabled = 0xCAFECAFE;
    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, &supported, &enabled);
    ok(hr == S_OK, "ret %08x\n", hr );
    if (expected == expected2)
        ok(enabled == expected, "Expected %08x, got %08x\n", expected, enabled);
    else
        ok(broken(enabled == expected) || enabled == expected2,
           "Expected %08x, got %08x\n", expected2, enabled);

    /* reset the safety options */

    hr = IObjectSafety_SetInterfaceSafetyOptions(safety, NULL,
            INTERFACESAFE_FOR_UNTRUSTED_CALLER|INTERFACESAFE_FOR_UNTRUSTED_DATA|INTERFACE_USES_SECURITY_MANAGER,
            0);
    ok(hr == S_OK, "ret %08x\n", hr );

    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, &supported, &enabled);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(enabled == 0, "Expected 0, got %08x\n", enabled);
}

static void test_document_IObjectSafety(void)
{
    IXMLDOMDocument *doc;
    IObjectSafety *safety;
    DWORD enabled = 0, supported = 0;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_QueryInterface(doc, &IID_IObjectSafety, (void**)&safety);
    ok(hr == S_OK, "ret %08x\n", hr );

    /* get */
    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, NULL, &enabled);
    ok(hr == E_POINTER, "ret %08x\n", hr );
    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, &supported, NULL);
    ok(hr == E_POINTER, "ret %08x\n", hr );

    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, &supported, &enabled);
    ok(hr == S_OK, "ret %08x\n", hr );
    ok(broken(supported == (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA)) ||
       supported == (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA | INTERFACE_USES_SECURITY_MANAGER) /* msxml3 SP8+ */,
        "Expected (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA | INTERFACE_USES_SECURITY_MANAGER), "
             "got %08x\n", supported);
    ok(enabled == 0, "Expected 0, got %08x\n", enabled);

    /* set -- individual flags */

    test_IObjectSafety_set(safety, S_OK, S_OK,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER, INTERFACESAFE_FOR_UNTRUSTED_CALLER,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER, INTERFACESAFE_FOR_UNTRUSTED_CALLER);

    test_IObjectSafety_set(safety, S_OK, S_OK,
        INTERFACESAFE_FOR_UNTRUSTED_DATA, INTERFACESAFE_FOR_UNTRUSTED_DATA,
        INTERFACESAFE_FOR_UNTRUSTED_DATA, INTERFACESAFE_FOR_UNTRUSTED_DATA);

    test_IObjectSafety_set(safety, S_OK, S_OK,
        INTERFACE_USES_SECURITY_MANAGER, INTERFACE_USES_SECURITY_MANAGER,
        0, INTERFACE_USES_SECURITY_MANAGER /* msxml3 SP8+ */);

    /* set INTERFACE_USES_DISPEX  */

    test_IObjectSafety_set(safety, S_OK, E_FAIL /* msxml3 SP8+ */,
        INTERFACE_USES_DISPEX, INTERFACE_USES_DISPEX,
        0, 0);

    test_IObjectSafety_set(safety, S_OK, E_FAIL /* msxml3 SP8+ */,
        INTERFACE_USES_DISPEX, 0,
        0, 0);

    test_IObjectSafety_set(safety, S_OK, S_OK /* msxml3 SP8+ */,
        0, INTERFACE_USES_DISPEX,
        0, 0);

    /* set option masking */

    test_IObjectSafety_set(safety, S_OK, S_OK,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER|INTERFACESAFE_FOR_UNTRUSTED_DATA,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER);

    test_IObjectSafety_set(safety, S_OK, S_OK,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER|INTERFACESAFE_FOR_UNTRUSTED_DATA,
        INTERFACESAFE_FOR_UNTRUSTED_DATA,
        INTERFACESAFE_FOR_UNTRUSTED_DATA,
        INTERFACESAFE_FOR_UNTRUSTED_DATA);

    test_IObjectSafety_set(safety, S_OK, S_OK,
        INTERFACESAFE_FOR_UNTRUSTED_CALLER|INTERFACESAFE_FOR_UNTRUSTED_DATA,
        INTERFACE_USES_SECURITY_MANAGER,
        0,
        0);

    /* set -- inheriting previous settings */

    hr = IObjectSafety_SetInterfaceSafetyOptions(safety, NULL,
                                                         INTERFACESAFE_FOR_UNTRUSTED_CALLER,
                                                         INTERFACESAFE_FOR_UNTRUSTED_CALLER);
    ok(hr == S_OK, "ret %08x\n", hr );
    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, &supported, &enabled);
    ok(hr == S_OK, "ret %08x\n", hr );
    todo_wine
    ok(broken(enabled == INTERFACESAFE_FOR_UNTRUSTED_CALLER) ||
       enabled == (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACE_USES_SECURITY_MANAGER) /* msxml3 SP8+ */,
         "Expected (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACE_USES_SECURITY_MANAGER), "
         "got %08x\n", enabled);

    hr = IObjectSafety_SetInterfaceSafetyOptions(safety, NULL,
                                                         INTERFACESAFE_FOR_UNTRUSTED_DATA,
                                                         INTERFACESAFE_FOR_UNTRUSTED_DATA);
    ok(hr == S_OK, "ret %08x\n", hr );
    hr = IObjectSafety_GetInterfaceSafetyOptions(safety, NULL, &supported, &enabled);
    ok(hr == S_OK, "ret %08x\n", hr );
    todo_wine
    ok(broken(enabled == INTERFACESAFE_FOR_UNTRUSTED_DATA) ||
       enabled == (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA) /* msxml3 SP8+ */,
        "Expected (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA), "
        "got %08x\n", enabled);

    IObjectSafety_Release(safety);

    IXMLDOMDocument_Release(doc);
}

typedef struct _property_test_t {
    const GUID *guid;
    const char *clsid;
    const char *property;
    const char *value;
} property_test_t;

static const property_test_t properties_test_data[] = {
    { &CLSID_DOMDocument,  "CLSID_DOMDocument" , "SelectionLanguage", "XSLPattern" },
    { &CLSID_DOMDocument2,  "CLSID_DOMDocument2" , "SelectionLanguage", "XSLPattern" },
    { &CLSID_DOMDocument30, "CLSID_DOMDocument30", "SelectionLanguage", "XSLPattern" },
    { &CLSID_DOMDocument40, "CLSID_DOMDocument40", "SelectionLanguage", "XPath" },
    { &CLSID_DOMDocument60, "CLSID_DOMDocument60", "SelectionLanguage", "XPath" },
    { 0 }
};

static void test_default_properties(void)
{
    const property_test_t *entry = properties_test_data;
    IXMLDOMDocument2 *doc;
    VARIANT var;
    HRESULT hr;

    while (entry->guid)
    {
        hr = CoCreateInstance(entry->guid, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void**)&doc);
        if (hr != S_OK)
        {
            win_skip("can't create %s instance\n", entry->clsid);
            entry++;
            continue;
        }

        hr = IXMLDOMDocument2_getProperty(doc, _bstr_(entry->property), &var);
        ok(hr == S_OK, "got 0x%08x\n", hr);
        ok(lstrcmpW(V_BSTR(&var), _bstr_(entry->value)) == 0, "expected %s, for %s\n",
           entry->value, entry->clsid);
        VariantClear(&var);

        IXMLDOMDocument2_Release(doc);

        entry++;
    }
}

static void test_XSLPattern(void)
{
    IXMLDOMDocument2 *doc;
    IXMLDOMNodeList *list;
    VARIANT_BOOL b;
    LONG len;

    doc = create_document(&IID_IXMLDOMDocument2);
    if (!doc) return;

    ole_check(IXMLDOMDocument2_loadXML(doc, _bstr_(szExampleXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");

    /* switch to XSLPattern */
    ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionLanguage"), _variantbstr_("XSLPattern")));

    /* XPath doesn't select elements with non-null default namespace with unqualified selectors, XSLPattern does */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//elem/c"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    /* should select <elem><c> and <elem xmlns='...'><c> but not <elem><foo:c> */
    ok(len == 3, "expected 3 entries in list, got %d\n", len);
    IXMLDOMNodeList_Release(list);

    /* for XSLPattern start index is 0, for XPath it's 1 */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[0]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1");

    /* index() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()=1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1");

    /* $eq$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() $eq$ 1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1");

    /* end() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E4.E2.D1");

    /* $not$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[$not$ end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1 E3.E2.D1");

    /* !=/$ne$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() != 0]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1 E3.E2.D1 E4.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() $ne$ 0]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1 E3.E2.D1 E4.E2.D1");

    /* </$lt$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() < 2]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() $lt$ 2]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1");

    /* <=/$le$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() <= 1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() $le$ 1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1");

    /* >/$gt$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() > 1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E2.D1 E4.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() $gt$ 1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E2.D1 E4.E2.D1");

    /* >=/$ge$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() >= 2]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E2.D1 E4.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index() $ge$ 2]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E2.D1 E4.E2.D1");

    /* $ieq$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[a $ieq$ 'a2 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1");

    /* $ine$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[a $ine$ 'a2 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E3.E2.D1 E4.E2.D1");

    /* $ilt$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[a $ilt$ 'a3 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1");

    /* $ile$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[a $ile$ 'a2 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E2.E2.D1");

    /* $igt$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[a $igt$ 'a2 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E2.D1 E4.E2.D1");

    /* $ige$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[a $ige$ 'a3 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E2.D1 E4.E2.D1");

    /* $any$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[$any$ *='B2 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1");

    /* $all$ */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[$all$ *!='B2 field']"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E3.E2.D1 E4.E2.D1");

    /* or/$or$/|| */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()=0 or end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E4.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()=0 $or$ end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E4.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()=0 || end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E1.E2.D1 E4.E2.D1");

    /* and/$and$/&& */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()>0 and $not$ end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1 E3.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()>0 $and$ $not$ end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1 E3.E2.D1");
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root//elem[index()>0 && $not$ end()]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E2.E2.D1 E3.E2.D1");

    /* namespace handling */
    /* no registered namespaces */
    ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"), _variantbstr_("")));
    list = NULL;

    /* prefixes don't need to be registered, you may use them as they are in the doc */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//bar:x"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E5.E1.E4.E1.E2.D1 E6.E2.E4.E1.E2.D1");

    /* prefixes must be explicitly specified in the name */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//foo:elem"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len == 0, "expected empty list\n");
    if (len)
        IXMLDOMNodeList_Release(list);

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//foo:c"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E4.E2.D1");

    /* explicitly register prefix foo */
    ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"), _variantbstr_("xmlns:foo='urn:uuid:86B2F87F-ACB6-45cd-8B77-9BDB92A01A29'")));

    /* now we get the same behavior as XPath */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//foo:c"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.E3.E2.D1 E3.E4.E2.D1");

    /* set prefix foo to some nonexistent namespace */
    ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"), _variantbstr_("xmlns:foo='urn:nonexistent-foo'")));

    /* the registered prefix takes precedence */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//foo:c"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len == 0, "expected empty list\n");
    if (len)
        IXMLDOMNodeList_Release(list);

    IXMLDOMDocument2_Release(doc);

    doc = create_document(&IID_IXMLDOMDocument2);
    if (!doc) return;

    ole_check(IXMLDOMDocument2_loadXML(doc, _bstr_(szNodeTypesXML), &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");
    list = NULL;

    /* attribute() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("attribute()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len == 0, "expected empty list\n");
    if (len)
        IXMLDOMNodeList_Release(list);

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("attribute('depth')"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len == 0, "expected empty list\n");
    if (len)
        IXMLDOMNodeList_Release(list);

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root/attribute('depth')"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "A'depth'.E3.D1");

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x/attribute()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "A'id'.E3.E3.D1 A'depth'.E3.E3.D1");

    list = NULL;
    ole_expect(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x//attribute(id)"), &list), E_FAIL);
    if (list)
        IXMLDOMNodeList_Release(list);
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x//attribute('id')"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "A'id'.E3.E3.D1 A'id'.E4.E3.E3.D1 A'id'.E5.E3.E3.D1 A'id'.E6.E3.E3.D1");

    /* comment() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("comment()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "C2.D1");

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//comment()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "C2.D1 C1.E3.D1 C2.E3.E3.D1 C2.E4.E3.D1");

    /* element() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("element()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E3.D1");

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root/y/element()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E4.E4.E3.D1 E5.E4.E3.D1 E6.E4.E3.D1");

    list = NULL;
    ole_expect(IXMLDOMDocument2_selectNodes(doc, _bstr_("//element(a)"), &list), E_FAIL);
    if (list)
        IXMLDOMNodeList_Release(list);
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//element('a')"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E4.E3.E3.D1 E4.E4.E3.D1");

    /* node() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("node()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "P1.D1 C2.D1 E3.D1");

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x/node()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "P1.E3.E3.D1 C2.E3.E3.D1 T3.E3.E3.D1 E4.E3.E3.D1 E5.E3.E3.D1 E6.E3.E3.D1");

    /* nodeType() */
    /* XML_ELEMENT_NODE */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x/node()[nodeType()=1]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "E4.E3.E3.D1 E5.E3.E3.D1 E6.E3.E3.D1");
    /* XML_TEXT_NODE */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x/node()[nodeType()=3]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "T3.E3.E3.D1");
    /* XML_PI_NODE */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x/node()[nodeType()=7]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "P1.E3.E3.D1");
    /* XML_COMMENT_NODE */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//x/node()[nodeType()=8]"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "C2.E3.E3.D1");

    /* pi() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("pi()"), &list));
    if (list)
    {
        len = 0;
        ole_check(IXMLDOMNodeList_get_length(list, &len));
        ok(len != 0, "expected filled list\n");
        if (len)
            expect_list_and_release(list, "P1.D1");
    }

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("//y/pi()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "P1.E4.E3.D1");

    /* textnode() */
    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root/textnode()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "T2.E3.D1");

    ole_check(IXMLDOMDocument2_selectNodes(doc, _bstr_("root/element()/textnode()"), &list));
    len = 0;
    ole_check(IXMLDOMNodeList_get_length(list, &len));
    ok(len != 0, "expected filled list\n");
    if (len)
        expect_list_and_release(list, "T3.E3.E3.D1 T3.E4.E3.D1");

    IXMLDOMDocument2_Release(doc);
    free_bstrs();
}

static void test_splitText(void)
{
    IXMLDOMCDATASection *cdata;
    IXMLDOMElement *root;
    IXMLDOMDocument *doc;
    IXMLDOMText *text, *text2;
    IXMLDOMNode *node;
    VARIANT var;
    VARIANT_BOOL success;
    LONG length;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_loadXML(doc, _bstr_("<root></root>"), &success);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_get_documentElement(doc, &root);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_createCDATASection(doc, _bstr_("beautiful plumage"), &cdata);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    V_VT(&var) = VT_EMPTY;
    hr = IXMLDOMElement_appendChild(root, (IXMLDOMNode*)cdata, NULL);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    length = 0;
    hr = IXMLDOMCDATASection_get_length(cdata, &length);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(length > 0, "got %d\n", length);

    hr = IXMLDOMCDATASection_splitText(cdata, 0, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    text = (void*)0xdeadbeef;
    /* negative offset */
    hr = IXMLDOMCDATASection_splitText(cdata, -1, &text);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);
    ok(text == (void*)0xdeadbeef, "got %p\n", text);

    text = (void*)0xdeadbeef;
    /* offset outside data */
    hr = IXMLDOMCDATASection_splitText(cdata, length + 1, &text);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);
    ok(text == 0, "got %p\n", text);

    text = (void*)0xdeadbeef;
    /* offset outside data */
    hr = IXMLDOMCDATASection_splitText(cdata, length, &text);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(text == 0, "got %p\n", text);

    /* no empty node created */
    node = (void*)0xdeadbeef;
    hr = IXMLDOMCDATASection_get_nextSibling(cdata, &node);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(node == 0, "got %p\n", text);

    hr = IXMLDOMCDATASection_splitText(cdata, 10, &text);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    length = 0;
    hr = IXMLDOMText_get_length(text, &length);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(length == 7, "got %d\n", length);

    hr = IXMLDOMCDATASection_get_nextSibling(cdata, &node);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IXMLDOMNode_Release(node);

    /* split new text node */
    hr = IXMLDOMText_get_length(text, &length);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    node = (void*)0xdeadbeef;
    hr = IXMLDOMText_get_nextSibling(text, &node);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(node == 0, "got %p\n", text);

    hr = IXMLDOMText_splitText(text, 0, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    text2 = (void*)0xdeadbeef;
    /* negative offset */
    hr = IXMLDOMText_splitText(text, -1, &text2);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);
    ok(text2 == (void*)0xdeadbeef, "got %p\n", text2);

    text2 = (void*)0xdeadbeef;
    /* offset outside data */
    hr = IXMLDOMText_splitText(text, length + 1, &text2);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);
    ok(text2 == 0, "got %p\n", text2);

    text2 = (void*)0xdeadbeef;
    /* offset outside data */
    hr = IXMLDOMText_splitText(text, length, &text2);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(text2 == 0, "got %p\n", text);

    text2 = 0;
    hr = IXMLDOMText_splitText(text, 4, &text2);
    todo_wine ok(hr == S_OK, "got 0x%08x\n", hr);
    if (text2) IXMLDOMText_Release(text2);

    node = 0;
    hr = IXMLDOMText_get_nextSibling(text, &node);
    todo_wine ok(hr == S_OK, "got 0x%08x\n", hr);
    if (node) IXMLDOMNode_Release(node);

    IXMLDOMText_Release(text);
    IXMLDOMElement_Release(root);
    IXMLDOMCDATASection_Release(cdata);
    free_bstrs();
}

static void test_getQualifiedItem(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *element;
    IXMLDOMNode *pr_node, *node;
    IXMLDOMNodeList *root_list;
    IXMLDOMNamedNodeMap *map;
    VARIANT_BOOL b;
    BSTR str;
    LONG len;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    hr = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( hr == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    hr = IXMLDOMDocument_get_documentElement(doc, &element);
    ok( hr == S_OK, "ret %08x\n", hr);

    hr = IXMLDOMElement_get_childNodes(element, &root_list);
    ok( hr == S_OK, "ret %08x\n", hr);

    hr = IXMLDOMNodeList_get_item(root_list, 1, &pr_node);
    ok( hr == S_OK, "ret %08x\n", hr);
    IXMLDOMNodeList_Release(root_list);

    hr = IXMLDOMNode_get_attributes(pr_node, &map);
    ok( hr == S_OK, "ret %08x\n", hr);
    IXMLDOMNode_Release(pr_node);

    hr = IXMLDOMNamedNodeMap_get_length(map, &len);
    ok( hr == S_OK, "ret %08x\n", hr);
    ok( len == 3, "length %d\n", len);

    hr = IXMLDOMNamedNodeMap_getQualifiedItem(map, NULL, NULL, NULL);
    ok( hr == E_INVALIDARG, "ret %08x\n", hr);

    node = (void*)0xdeadbeef;
    hr = IXMLDOMNamedNodeMap_getQualifiedItem(map, NULL, NULL, &node);
    ok( hr == E_INVALIDARG, "ret %08x\n", hr);
    ok( node == (void*)0xdeadbeef, "got %p\n", node);

    hr = IXMLDOMNamedNodeMap_getQualifiedItem(map, _bstr_("id"), NULL, NULL);
    ok( hr == E_INVALIDARG, "ret %08x\n", hr);

    hr = IXMLDOMNamedNodeMap_getQualifiedItem(map, _bstr_("id"), NULL, &node);
    ok( hr == S_OK, "ret %08x\n", hr);
    IXMLDOMNode_Release(node);

    IXMLDOMNamedNodeMap_Release( map );
    IXMLDOMElement_Release( element );
    IXMLDOMDocument_Release( doc );
    free_bstrs();
}

static void test_removeQualifiedItem(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *element;
    IXMLDOMNode *pr_node, *node;
    IXMLDOMNodeList *root_list;
    IXMLDOMNamedNodeMap *map;
    VARIANT_BOOL b;
    BSTR str;
    LONG len;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    hr = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( hr == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    hr = IXMLDOMDocument_get_documentElement(doc, &element);
    ok( hr == S_OK, "ret %08x\n", hr);

    hr = IXMLDOMElement_get_childNodes(element, &root_list);
    ok( hr == S_OK, "ret %08x\n", hr);

    hr = IXMLDOMNodeList_get_item(root_list, 1, &pr_node);
    ok( hr == S_OK, "ret %08x\n", hr);
    IXMLDOMNodeList_Release(root_list);

    hr = IXMLDOMNode_get_attributes(pr_node, &map);
    ok( hr == S_OK, "ret %08x\n", hr);
    IXMLDOMNode_Release(pr_node);

    hr = IXMLDOMNamedNodeMap_get_length(map, &len);
    ok( hr == S_OK, "ret %08x\n", hr);
    ok( len == 3, "length %d\n", len);

    hr = IXMLDOMNamedNodeMap_removeQualifiedItem(map, NULL, NULL, NULL);
    ok( hr == E_INVALIDARG, "ret %08x\n", hr);

    node = (void*)0xdeadbeef;
    hr = IXMLDOMNamedNodeMap_removeQualifiedItem(map, NULL, NULL, &node);
    ok( hr == E_INVALIDARG, "ret %08x\n", hr);
    ok( node == (void*)0xdeadbeef, "got %p\n", node);

    /* out pointer is optional */
    hr = IXMLDOMNamedNodeMap_removeQualifiedItem(map, _bstr_("id"), NULL, NULL);
    ok( hr == S_OK, "ret %08x\n", hr);

    /* already removed */
    hr = IXMLDOMNamedNodeMap_removeQualifiedItem(map, _bstr_("id"), NULL, NULL);
    ok( hr == S_FALSE, "ret %08x\n", hr);

    hr = IXMLDOMNamedNodeMap_removeQualifiedItem(map, _bstr_("vr"), NULL, &node);
    ok( hr == S_OK, "ret %08x\n", hr);
    IXMLDOMNode_Release(node);

    IXMLDOMNamedNodeMap_Release( map );
    IXMLDOMElement_Release( element );
    IXMLDOMDocument_Release( doc );
    free_bstrs();
}

#define check_default_props(doc) _check_default_props(__LINE__, doc)
static inline void _check_default_props(int line, IXMLDOMDocument2* doc)
{
    VARIANT_BOOL b;
    VARIANT var;
    HRESULT hr;

    VariantInit(&var);
    helper_ole_check(IXMLDOMDocument2_getProperty(doc, _bstr_("SelectionLanguage"), &var));
    ok_(__FILE__, line)(lstrcmpW(V_BSTR(&var), _bstr_("XSLPattern")) == 0, "expected XSLPattern\n");
    VariantClear(&var);

    helper_ole_check(IXMLDOMDocument2_getProperty(doc, _bstr_("SelectionNamespaces"), &var));
    ok_(__FILE__, line)(lstrcmpW(V_BSTR(&var), _bstr_("")) == 0, "expected empty string\n");
    VariantClear(&var);

    helper_ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc, &b));
    ok_(__FILE__, line)(b == VARIANT_FALSE, "expected FALSE\n");

    hr = IXMLDOMDocument2_get_schemas(doc, &var);
    ok_(__FILE__, line)(hr == S_FALSE, "got %08x\n", hr);
    VariantClear(&var);
}

#define check_set_props(doc) _check_set_props(__LINE__, doc)
static inline void _check_set_props(int line, IXMLDOMDocument2* doc)
{
    VARIANT_BOOL b;
    VARIANT var;

    VariantInit(&var);
    helper_ole_check(IXMLDOMDocument2_getProperty(doc, _bstr_("SelectionLanguage"), &var));
    ok_(__FILE__, line)(lstrcmpW(V_BSTR(&var), _bstr_("XPath")) == 0, "expected XPath\n");
    VariantClear(&var);

    helper_ole_check(IXMLDOMDocument2_getProperty(doc, _bstr_("SelectionNamespaces"), &var));
    ok_(__FILE__, line)(lstrcmpW(V_BSTR(&var), _bstr_("xmlns:wi=\'www.winehq.org\'")) == 0, "got %s\n", wine_dbgstr_w(V_BSTR(&var)));
    VariantClear(&var);

    helper_ole_check(IXMLDOMDocument2_get_preserveWhiteSpace(doc, &b));
    ok_(__FILE__, line)(b == VARIANT_TRUE, "expected TRUE\n");

    helper_ole_check(IXMLDOMDocument2_get_schemas(doc, &var));
    ok_(__FILE__, line)(V_VT(&var) != VT_NULL, "expected pointer\n");
    VariantClear(&var);
}

#define set_props(doc, cache) _set_props(__LINE__, doc, cache)
static inline void _set_props(int line, IXMLDOMDocument2* doc, IXMLDOMSchemaCollection* cache)
{
    VARIANT var;

    VariantInit(&var);
    helper_ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionLanguage"), _variantbstr_("XPath")));
    helper_ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"), _variantbstr_("xmlns:wi=\'www.winehq.org\'")));
    helper_ole_check(IXMLDOMDocument2_put_preserveWhiteSpace(doc, VARIANT_TRUE));
    V_VT(&var) = VT_DISPATCH;
    V_DISPATCH(&var) = NULL;
    helper_ole_check(IXMLDOMSchemaCollection_QueryInterface(cache, &IID_IDispatch, (void**)&V_DISPATCH(&var)));
    ok_(__FILE__, line)(V_DISPATCH(&var) != NULL, "expected pointer\n");
    helper_ole_check(IXMLDOMDocument2_putref_schemas(doc, var));
    VariantClear(&var);
}

#define unset_props(doc) _unset_props(__LINE__, doc)
static inline void _unset_props(int line, IXMLDOMDocument2* doc)
{
    VARIANT var;

    VariantInit(&var);
    helper_ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionLanguage"), _variantbstr_("XSLPattern")));
    helper_ole_check(IXMLDOMDocument2_setProperty(doc, _bstr_("SelectionNamespaces"), _variantbstr_("")));
    helper_ole_check(IXMLDOMDocument2_put_preserveWhiteSpace(doc, VARIANT_FALSE));
    V_VT(&var) = VT_NULL;
    helper_ole_check(IXMLDOMDocument2_putref_schemas(doc, var));
    VariantClear(&var);
}

static void test_get_ownerDocument(void)
{
    IXMLDOMDocument *doc1, *doc2, *doc3;
    IXMLDOMDocument2 *doc, *doc_owner;
    IXMLDOMNode *node;
    IXMLDOMSchemaCollection *cache;
    VARIANT_BOOL b;
    VARIANT var;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument2);
    cache = create_cache(&IID_IXMLDOMSchemaCollection);
    if (!doc || !cache)
    {
        if (doc) IXMLDOMDocument2_Release(doc);
        if (cache) IXMLDOMSchemaCollection_Release(cache);
        return;
    }

    VariantInit(&var);

    str = SysAllocString(szComplete4);
    ole_check(IXMLDOMDocument_loadXML(doc, str, &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString(str);

    check_default_props(doc);

    /* set properties and check that new instances use them */
    set_props(doc, cache);
    check_set_props(doc);

    ole_check(IXMLDOMDocument2_get_firstChild(doc, &node));
    ole_check(IXMLDOMNode_get_ownerDocument(node, &doc1));

    /* new interface keeps props */
    ole_check(IXMLDOMDocument_QueryInterface(doc1, &IID_IXMLDOMDocument2, (void**)&doc_owner));
    ok( doc_owner != doc, "got %p, doc %p\n", doc_owner, doc);
    check_set_props(doc_owner);
    IXMLDOMDocument2_Release(doc_owner);

    ole_check(IXMLDOMNode_get_ownerDocument(node, &doc2));
    IXMLDOMNode_Release(node);

    ok(doc1 != doc2, "got %p, expected %p. original %p\n", doc2, doc1, doc);

    /* reload */
    str = SysAllocString(szComplete4);
    ole_check(IXMLDOMDocument2_loadXML(doc, str, &b));
    ok(b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString(str);

    /* properties retained even after reload */
    check_set_props(doc);

    ole_check(IXMLDOMDocument2_get_firstChild(doc, &node));
    ole_check(IXMLDOMNode_get_ownerDocument(node, &doc3));
    IXMLDOMNode_Release(node);

    ole_check(IXMLDOMDocument_QueryInterface(doc3, &IID_IXMLDOMDocument2, (void**)&doc_owner));
    ok(doc3 != doc1 && doc3 != doc2 && doc_owner != doc, "got %p, (%p, %p, %p)\n", doc3, doc, doc1, doc2);
    check_set_props(doc_owner);

    /* changing properties for one instance changes them for all */
    unset_props(doc_owner);
    check_default_props(doc_owner);
    check_default_props(doc);


    IXMLDOMDocument_Release(doc1);
    IXMLDOMDocument_Release(doc2);
    IXMLDOMDocument_Release(doc3);
    IXMLDOMDocument2_Release(doc);
    IXMLDOMDocument2_Release(doc_owner);
    free_bstrs();
}

static void test_setAttributeNode(void)
{
    IXMLDOMDocument *doc, *doc2;
    IXMLDOMElement *elem;
    IXMLDOMAttribute *attr, *attr2, *ret_attr;
    VARIANT_BOOL b;
    HRESULT hr;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    hr = IXMLDOMDocument2_loadXML( doc, str, &b );
    ok( hr == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    hr = IXMLDOMDocument_get_documentElement(doc, &elem);
    ok( hr == S_OK, "got 0x%08x\n", hr);

    ret_attr = (void*)0xdeadbeef;
    hr = IXMLDOMElement_setAttributeNode(elem, NULL, &ret_attr);
    ok( hr == E_INVALIDARG, "got 0x%08x\n", hr);
    ok( ret_attr == (void*)0xdeadbeef, "got %p\n", ret_attr);

    hr = IXMLDOMDocument_createAttribute(doc, _bstr_("attr"), &attr);
    ok( hr == S_OK, "got 0x%08x\n", hr);

    ret_attr = (void*)0xdeadbeef;
    hr = IXMLDOMElement_setAttributeNode(elem, attr, &ret_attr);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    ok( ret_attr == NULL, "got %p\n", ret_attr);

    attr2 = NULL;
    hr = IXMLDOMElement_getAttributeNode(elem, _bstr_("attr"), &attr2);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    IXMLDOMAttribute_Release(attr2);

    /* try to add it another time */
    ret_attr = (void*)0xdeadbeef;
    hr = IXMLDOMElement_setAttributeNode(elem, attr, &ret_attr);
    todo_wine ok( hr == E_FAIL, "got 0x%08x\n", hr);
    todo_wine ok( ret_attr == (void*)0xdeadbeef, "got %p\n", ret_attr);

    IXMLDOMElement_Release(elem);

    /* initialy used element is released, attribute still 'has' a container */
    hr = IXMLDOMDocument_get_documentElement(doc, &elem);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    ret_attr = (void*)0xdeadbeef;
    hr = IXMLDOMElement_setAttributeNode(elem, attr, &ret_attr);
    todo_wine ok( hr == E_FAIL, "got 0x%08x\n", hr);
    todo_wine ok( ret_attr == (void*)0xdeadbeef, "got %p\n", ret_attr);
    IXMLDOMElement_Release(elem);

    /* add attribute already attached to another document */
    doc2 = create_document(&IID_IXMLDOMDocument);

    str = SysAllocString( szComplete4 );
    hr = IXMLDOMDocument_loadXML( doc2, str, &b );
    ok( hr == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    hr = IXMLDOMDocument_get_documentElement(doc2, &elem);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    hr = IXMLDOMElement_setAttributeNode(elem, attr, NULL);
    todo_wine ok( hr == E_FAIL, "got 0x%08x\n", hr);
    IXMLDOMElement_Release(elem);

    IXMLDOMDocument_Release(doc2);

    IXMLDOMAttribute_Release(attr);
    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_put_dataType(void)
{
    IXMLDOMCDATASection *cdata;
    IXMLDOMDocument *doc;
    VARIANT_BOOL b;
    HRESULT hr;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    str = SysAllocString( szComplete4 );
    hr = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( hr == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    hr = IXMLDOMDocument_createCDATASection(doc, _bstr_("test"), &cdata);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    hr = IXMLDOMCDATASection_put_dataType(cdata, _bstr_("number"));
    ok( hr == E_FAIL, "got 0x%08x\n", hr);
    hr = IXMLDOMCDATASection_put_dataType(cdata, _bstr_("string"));
    ok( hr == E_FAIL, "got 0x%08x\n", hr);
    IXMLDOMCDATASection_Release(cdata);

    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_createNode(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *elem;
    IXMLDOMNode *node;
    VARIANT v, var;
    BSTR prefix, str;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    /* NODE_ELEMENT nodes */
    /* 1. specified namespace */
    V_VT(&v) = VT_I4;
    V_I4(&v) = NODE_ELEMENT;

    hr = IXMLDOMDocument_createNode(doc, v, _bstr_("ns1:test"), _bstr_("http://winehq.org"), &node);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    prefix = NULL;
    hr = IXMLDOMNode_get_prefix(node, &prefix);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    ok(lstrcmpW(prefix, _bstr_("ns1")) == 0, "wrong prefix\n");
    SysFreeString(prefix);
    IXMLDOMNode_Release(node);

    /* 2. default namespace */
    hr = IXMLDOMDocument_createNode(doc, v, _bstr_("test"), _bstr_("http://winehq.org/default"), &node);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    prefix = (void*)0xdeadbeef;
    hr = IXMLDOMNode_get_prefix(node, &prefix);
    ok( hr == S_FALSE, "got 0x%08x\n", hr);
    ok(prefix == 0, "expected empty prefix, got %p\n", prefix);

    hr = IXMLDOMNode_QueryInterface(node, &IID_IXMLDOMElement, (void**)&elem);
    ok( hr == S_OK, "got 0x%08x\n", hr);

    V_VT(&var) = VT_BSTR;
    hr = IXMLDOMElement_getAttribute(elem, _bstr_("xmlns"), &var);
    ok( hr == S_FALSE, "got 0x%08x\n", hr);
    ok( V_VT(&var) == VT_NULL, "got %d\n", V_VT(&var));

    str = NULL;
    hr = IXMLDOMElement_get_namespaceURI(elem, &str);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    ok( lstrcmpW(str, _bstr_("http://winehq.org/default")) == 0, "expected default namespace\n");
    SysFreeString(str);

    IXMLDOMElement_Release(elem);
    IXMLDOMNode_Release(node);

    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_get_prefix(void)
{
    IXMLDOMDocumentFragment *fragment;
    IXMLDOMCDATASection *cdata;
    IXMLDOMElement *element;
    IXMLDOMComment *comment;
    IXMLDOMDocument *doc;
    HRESULT hr;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    /* nodes that can't support prefix */
    /* 1. document */
    str = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_get_prefix(doc, &str);
    ok( hr == S_FALSE, "got 0x%08x\n", hr);
    ok( str == 0, "got %p\n", str);

    hr = IXMLDOMDocument_get_prefix(doc, NULL);
    ok( hr == E_INVALIDARG, "got 0x%08x\n", hr);

    /* 2. cdata */
    hr = IXMLDOMDocument_createCDATASection(doc, NULL, &cdata);
    ok(hr == S_OK, "got %08x\n", hr );

    str = (void*)0xdeadbeef;
    hr = IXMLDOMCDATASection_get_prefix(cdata, &str);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok( str == 0, "got %p\n", str);

    hr = IXMLDOMCDATASection_get_prefix(cdata, NULL);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);
    IXMLDOMCDATASection_Release(cdata);

    /* 3. comment */
    hr = IXMLDOMDocument_createComment(doc, NULL, &comment);
    ok(hr == S_OK, "got %08x\n", hr );

    str = (void*)0xdeadbeef;
    hr = IXMLDOMComment_get_prefix(comment, &str);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok( str == 0, "got %p\n", str);

    hr = IXMLDOMComment_get_prefix(comment, NULL);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);
    IXMLDOMComment_Release(comment);

    /* 4. fragment */
    hr = IXMLDOMDocument_createDocumentFragment(doc, &fragment);
    ok(hr == S_OK, "got %08x\n", hr );

    str = (void*)0xdeadbeef;
    hr = IXMLDOMDocumentFragment_get_prefix(fragment, &str);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok( str == 0, "got %p\n", str);

    hr = IXMLDOMDocumentFragment_get_prefix(fragment, NULL);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);
    IXMLDOMDocumentFragment_Release(fragment);

    /* no prefix */
    hr = IXMLDOMDocument_createElement(doc, _bstr_("elem"), &element);
    ok( hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLDOMElement_get_prefix(element, NULL);
    ok( hr == E_INVALIDARG, "got 0x%08x\n", hr);

    str = (void*)0xdeadbeef;
    hr = IXMLDOMElement_get_prefix(element, &str);
    ok( hr == S_FALSE, "got 0x%08x\n", hr);
    ok( str == 0, "got %p\n", str);

    IXMLDOMElement_Release(element);

    /* with prefix */
    hr = IXMLDOMDocument_createElement(doc, _bstr_("a:elem"), &element);
    ok( hr == S_OK, "got 0x%08x\n", hr);

    str = (void*)0xdeadbeef;
    hr = IXMLDOMElement_get_prefix(element, &str);
    ok( hr == S_OK, "got 0x%08x\n", hr);
    ok( lstrcmpW(str, _bstr_("a")) == 0, "expected prefix \"a\"\n");
    SysFreeString(str);

    str = (void*)0xdeadbeef;
    hr = IXMLDOMElement_get_namespaceURI(element, &str);
    ok( hr == S_FALSE, "got 0x%08x\n", hr);
    ok( str == 0, "got %p\n", str);

    IXMLDOMElement_Release(element);

    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_selectSingleNode(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMNodeList *list;
    IXMLDOMNode *node;
    VARIANT_BOOL b;
    HRESULT hr;
    LONG len;
    BSTR str;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_selectSingleNode(doc, NULL, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_selectNodes(doc, NULL, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    str = SysAllocString( szComplete4 );
    hr = IXMLDOMDocument_loadXML( doc, str, &b );
    ok( hr == S_OK, "loadXML failed\n");
    ok( b == VARIANT_TRUE, "failed to load XML string\n");
    SysFreeString( str );

    hr = IXMLDOMDocument_selectSingleNode(doc, NULL, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_selectNodes(doc, NULL, NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("lc"), NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_selectNodes(doc, _bstr_("lc"), NULL);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("lc"), &node);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IXMLDOMNode_Release(node);

    hr = IXMLDOMDocument_selectNodes(doc, _bstr_("lc"), &list);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IXMLDOMNodeList_Release(list);

    list = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_selectNodes(doc, NULL, &list);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);
    ok(list == (void*)0xdeadbeef, "got %p\n", list);

    node = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_selectSingleNode(doc, _bstr_("nonexistent"), &node);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(node == 0, "got %p\n", node);

    list = (void*)0xdeadbeef;
    hr = IXMLDOMDocument_selectNodes(doc, _bstr_("nonexistent"), &list);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    len = 1;
    hr = IXMLDOMNodeList_get_length(list, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == 0, "got %d\n", len);
    IXMLDOMNodeList_Release(list);

    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_events(void)
{
    IConnectionPointContainer *conn;
    IConnectionPoint *point;
    IXMLDOMDocument *doc;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_QueryInterface(doc, &IID_IConnectionPointContainer, (void**)&conn);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IConnectionPointContainer_FindConnectionPoint(conn, &IID_IDispatch, &point);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IConnectionPoint_Release(point);
    hr = IConnectionPointContainer_FindConnectionPoint(conn, &IID_IPropertyNotifySink, &point);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IConnectionPoint_Release(point);
    hr = IConnectionPointContainer_FindConnectionPoint(conn, &DIID_XMLDOMDocumentEvents, &point);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IConnectionPoint_Release(point);

    IConnectionPointContainer_Release(conn);

    IXMLDOMDocument_Release(doc);
}

static void test_createProcessingInstruction(void)
{
    static const WCHAR bodyW[] = {'t','e','s','t',0};
    IXMLDOMProcessingInstruction *pi;
    IXMLDOMDocument *doc;
    WCHAR buff[10];
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    /* test for BSTR handling, pass broken BSTR */
    memcpy(&buff[2], bodyW, sizeof(bodyW));
    /* just a big length */
    *(DWORD*)buff = 0xf0f0;
    hr = IXMLDOMDocument_createProcessingInstruction(doc, _bstr_("test"), &buff[2], &pi);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    IXMLDOMProcessingInstruction_Release(pi);
    IXMLDOMDocument_Release(doc);
}

static void test_put_nodeTypedValue(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *elem;
    VARIANT type;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    hr = IXMLDOMDocument_createElement(doc, _bstr_("Element"), &elem);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    V_VT(&type) = VT_EMPTY;
    hr = IXMLDOMElement_get_dataType(elem, &type);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(V_VT(&type) == VT_NULL, "got %d, expected VT_NULL\n", V_VT(&type));

    /* set typed value for untyped node */
    V_VT(&type) = VT_I1;
    V_I1(&type) = 1;
    hr = IXMLDOMElement_put_nodeTypedValue(elem, type);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    V_VT(&type) = VT_EMPTY;
    hr = IXMLDOMElement_get_dataType(elem, &type);
    ok(hr == S_FALSE, "got 0x%08x\n", hr);
    ok(V_VT(&type) == VT_NULL, "got %d, expected VT_NULL\n", V_VT(&type));

    /* no type info stored */
    V_VT(&type) = VT_EMPTY;
    hr = IXMLDOMElement_get_nodeTypedValue(elem, &type);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&type) == VT_BSTR, "got %d, expected VT_BSTR\n", V_VT(&type));
    ok(memcmp(V_BSTR(&type), _bstr_("1"), 2*sizeof(WCHAR)) == 0,
       "got %s, expected \"1\"\n", wine_dbgstr_w(V_BSTR(&type)));
    VariantClear(&type);

    IXMLDOMDocument_Release(doc);
    free_bstrs();
}

static void test_get_xml(void)
{
    static const char xmlA[] = "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n<a>test</a>\r\n";
    IXMLDOMProcessingInstruction *pi;
    IXMLDOMNode *first;
    IXMLDOMDocument *doc;
    VARIANT_BOOL b;
    VARIANT v;
    BSTR xml;
    HRESULT hr;

    doc = create_document(&IID_IXMLDOMDocument);
    if (!doc) return;

    b = VARIANT_TRUE;
    hr = IXMLDOMDocument_loadXML( doc, _bstr_("<a>test</a>"), &b );
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok( b == VARIANT_TRUE, "got %d\n", b);

    hr = IXMLDOMDocument_createProcessingInstruction(doc, _bstr_("xml"),
                             _bstr_("version=\"1.0\" encoding=\"UTF-16\""), &pi);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXMLDOMDocument_get_firstChild(doc, &first);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    V_UNKNOWN(&v) = (IUnknown*)first;
    V_VT(&v) = VT_UNKNOWN;

    hr = IXMLDOMDocument_insertBefore(doc, (IXMLDOMNode*)pi, v, NULL);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    IXMLDOMProcessingInstruction_Release(pi);
    IXMLDOMNode_Release(first);

    hr = IXMLDOMDocument_get_xml(doc, &xml);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    ok(memcmp(xml, _bstr_(xmlA), sizeof(xmlA)*sizeof(WCHAR)) == 0,
        "got %s, expected %s\n", wine_dbgstr_w(xml), xmlA);
    SysFreeString(xml);

    IXMLDOMDocument_Release(doc);
}

START_TEST(domdoc)
{
    IXMLDOMDocument *doc;
    HRESULT hr;

    hr = CoInitialize( NULL );
    ok( hr == S_OK, "failed to init com\n");
    if (hr != S_OK)
        return;

    test_XMLHTTP();

    hr = CoCreateInstance( &CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void**)&doc );
    if (hr != S_OK)
    {
        win_skip("IXMLDOMDocument is not available (0x%08x)\n", hr);
        return;
    }

    IXMLDOMDocument_Release(doc);

    test_domdoc();
    test_persiststreaminit();
    test_domnode();
    test_refs();
    test_create();
    test_getElementsByTagName();
    test_get_text();
    test_get_childNodes();
    test_get_firstChild();
    test_get_lastChild();
    test_removeChild();
    test_replaceChild();
    test_removeNamedItem();
    test_IXMLDOMDocument2();
    test_whitespace();
    test_XPath();
    test_XSLPattern();
    test_cloneNode();
    test_xmlTypes();
    test_nodeTypeTests();
    test_DocumentSaveToDocument();
    test_DocumentSaveToFile();
    test_testTransforms();
    test_Namespaces();
    test_FormattingXML();
    test_nodeTypedValue();
    test_TransformWithLoadingLocalFile();
    test_put_nodeValue();
    test_document_IObjectSafety();
    test_splitText();
    test_getQualifiedItem();
    test_removeQualifiedItem();
    test_get_ownerDocument();
    test_setAttributeNode();
    test_put_dataType();
    test_createNode();
    test_get_prefix();
    test_default_properties();
    test_selectSingleNode();
    test_events();
    test_createProcessingInstruction();
    test_put_nodeTypedValue();
    test_get_xml();

    CoUninitialize();
}

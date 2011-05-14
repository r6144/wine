/*
 * Copyright 2011 Jacek Caban for CodeWeavers
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

#include <stdio.h>

#define COBJMACROS
#define CONST_VTABLE

#include <initguid.h>
#include <windows.h>
#include <oaidl.h>

#include "wine/test.h"

#define DEFINE_EXPECT(func) \
    static BOOL expect_ ## func = FALSE, called_ ## func = FALSE

#define SET_EXPECT(func) \
    expect_ ## func = TRUE

#define SET_CALLED(func) \
    called_ ## func = TRUE

#define CHECK_EXPECT2(func) \
    do { \
        ok(expect_ ##func, "unexpected call " #func "\n"); \
        called_ ## func = TRUE; \
    }while(0)

#define CHECK_EXPECT(func) \
    do { \
        CHECK_EXPECT2(func); \
        expect_ ## func = FALSE; \
    }while(0)

#define CHECK_CALLED(func) \
    do { \
        ok(called_ ## func, "expected " #func "\n"); \
        expect_ ## func = called_ ## func = FALSE; \
    }while(0)

DEFINE_EXPECT(reportSuccess);

#define DISPID_TESTOBJ_OK             10000
#define DISPID_TESTOBJ_TRACE          10001
#define DISPID_TESTOBJ_REPORTSUCCESS  10002

#define TESTOBJ_CLSID "{178fc166-f585-4e24-9c13-4bb7faf80646}"

static const GUID CLSID_TestObj =
    {0x178fc166,0xf585,0x4e24,{0x9c,0x13,0x4b,0xb7,0xfa,0xf8,0x06,0x46}};

static const char *script_name;

static int strcmp_wa(LPCWSTR strw, const char *stra)
{
    WCHAR buf[512];
    MultiByteToWideChar(CP_ACP, 0, stra, -1, buf, sizeof(buf)/sizeof(WCHAR));
    return lstrcmpW(strw, buf);
}

static HRESULT WINAPI Dispatch_QueryInterface(IDispatch *iface, REFIID riid, void **ppv)
{
    if(IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch)) {
        *ppv = iface;
	return S_OK;
    }

    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI Dispatch_AddRef(IDispatch *iface)
{
    return 2;
}

static ULONG WINAPI Dispatch_Release(IDispatch *iface)
{
    return 1;
}

static HRESULT WINAPI Dispatch_GetTypeInfoCount(IDispatch *iface, UINT *pctinfo)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI Dispatch_GetTypeInfo(IDispatch *iface, UINT iTInfo,
	LCID lcid, ITypeInfo **ppTInfo)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI Dispatch_GetIDsOfNames(IDispatch *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    unsigned i;

    for(i=0; i<cNames; i++) {
        if(!strcmp_wa(rgszNames[i], "ok")) {
            rgDispId[i] = DISPID_TESTOBJ_OK;
        }else if(!strcmp_wa(rgszNames[i], "trace")) {
            rgDispId[i] = DISPID_TESTOBJ_TRACE;
        }else if(!strcmp_wa(rgszNames[i], "reportSuccess")) {
            rgDispId[i] = DISPID_TESTOBJ_REPORTSUCCESS;
        }else {
            ok(0, "unexpected name %s\n", wine_dbgstr_w(rgszNames[i]));
            return DISP_E_UNKNOWNNAME;
        }
    }

    return S_OK;
}

static HRESULT WINAPI Dispatch_Invoke(IDispatch *iface, DISPID dispIdMember, REFIID riid, LCID lcid,
				      WORD wFlags, DISPPARAMS *pdp, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    switch(dispIdMember) {
    case DISPID_TESTOBJ_OK:
        ok(wFlags == INVOKE_FUNC, "wFlags = %x\n", wFlags);
        ok(pdp->cArgs == 2, "cArgs = %d\n", pdp->cArgs);
        ok(!pdp->cNamedArgs, "cNamedArgs = %d\n", pdp->cNamedArgs);
        ok(V_VT(pdp->rgvarg) == VT_BSTR, "V_VT(psp->rgvargs) = %d\n", V_VT(pdp->rgvarg));
        ok(V_VT(pdp->rgvarg+1) == VT_BOOL, "V_VT(psp->rgvargs+1) = %d\n", V_VT(pdp->rgvarg));
        ok(V_BOOL(pdp->rgvarg+1), "%s: %s\n", script_name, wine_dbgstr_w(V_BSTR(pdp->rgvarg)));
        if(pVarResult)
            V_VT(pVarResult) = VT_EMPTY;
        break;
    case DISPID_TESTOBJ_TRACE:
        ok(wFlags == INVOKE_FUNC, "wFlags = %x\n", wFlags);
        ok(pdp->cArgs == 1, "cArgs = %d\n", pdp->cArgs);
        ok(!pdp->cNamedArgs, "cNamedArgs = %d\n", pdp->cNamedArgs);
        ok(V_VT(pdp->rgvarg) == VT_BSTR, "V_VT(psp->rgvargs) = %d\n", V_VT(pdp->rgvarg));
        trace("%s: %s\n", script_name, wine_dbgstr_w(V_BSTR(pdp->rgvarg)));
        if(pVarResult)
            V_VT(pVarResult) = VT_EMPTY;
        break;
    case DISPID_TESTOBJ_REPORTSUCCESS:
        CHECK_EXPECT(reportSuccess);

        ok(wFlags == INVOKE_FUNC, "wFlags = %x\n", wFlags);
        ok(pdp->cArgs == 0, "cArgs = %d\n", pdp->cArgs);
        ok(!pdp->cNamedArgs, "cNamedArgs = %d\n", pdp->cNamedArgs);
        if(pVarResult)
            V_VT(pVarResult) = VT_EMPTY;
        break;
    default:
        ok(0, "unexpected dispIdMember %d\n", dispIdMember);
        return E_NOTIMPL;
    }

    return S_OK;
}

static IDispatchVtbl testobj_vtbl = {
    Dispatch_QueryInterface,
    Dispatch_AddRef,
    Dispatch_Release,
    Dispatch_GetTypeInfoCount,
    Dispatch_GetTypeInfo,
    Dispatch_GetIDsOfNames,
    Dispatch_Invoke
};

static IDispatch testobj = { &testobj_vtbl };

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IClassFactory, riid)) {
        *ppv = iface;
        return S_OK;
    }

    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    ok(!outer, "outer = %p\n", outer);
    return IDispatch_QueryInterface(&testobj, riid, ppv);
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL dolock)
{
    return S_OK;
}

static const IClassFactoryVtbl ClassFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ClassFactory_CreateInstance,
    ClassFactory_LockServer
};

static IClassFactory testobj_cf = { &ClassFactoryVtbl };

static void run_test(const char *file_name)
{
    SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
    char command[MAX_PATH];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    BOOL bres;

    script_name = file_name;
    sprintf(command, "wscript.exe %s", file_name);

    SET_EXPECT(reportSuccess);

    bres = CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if(!bres) {
        win_skip("script.exe is not available\n");
        SET_CALLED(reportSuccess);
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    CHECK_CALLED(reportSuccess);
}

static BOOL WINAPI test_enum_proc(HMODULE module, LPCTSTR type, LPSTR name, LONG_PTR param)
{
    const char *script_data, *ext;
    DWORD script_size, size;
    char file_name[MAX_PATH];
    HANDLE file;
    HRSRC src;
    BOOL res;

    trace("running %s test...\n", name);

    src = FindResourceA(NULL, name, type);
    ok(src != NULL, "Could not find resource %s: %u\n", name, GetLastError());
    if(!src)
        return TRUE;

    script_data = LoadResource(NULL, src);
    script_size = SizeofResource(NULL, src);
    while(script_size && !script_data[script_size-1])
        script_size--;

    ext = strrchr(name, '.');
    ok(ext != NULL, "no script extension\n");
    if(!ext)
      return TRUE;

    sprintf(file_name, "test%s", ext);

    file = CreateFileA(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
    ok(file != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());
    if(file == INVALID_HANDLE_VALUE)
        return TRUE;

    res = WriteFile(file, script_data, script_size, &size, NULL);
    CloseHandle(file);
    ok(res, "Could not write to file: %u\n", GetLastError());
    if(!res)
        return TRUE;

    run_test(file_name);

    DeleteFileA(file_name);
    return TRUE;
}

static BOOL init_key(const char *key_name, const char *def_value, BOOL init)
{
    HKEY hkey;
    DWORD res;

    if(!init) {
        RegDeleteKey(HKEY_CLASSES_ROOT, key_name);
        return TRUE;
    }

    res = RegCreateKeyA(HKEY_CLASSES_ROOT, key_name, &hkey);
    if(res != ERROR_SUCCESS)
        return FALSE;

    if(def_value)
        res = RegSetValueA(hkey, NULL, REG_SZ, def_value, strlen(def_value));

    RegCloseKey(hkey);
    return res == ERROR_SUCCESS;
}

static BOOL init_registry(BOOL init)
{
    return init_key("Wine.Test\\CLSID", TESTOBJ_CLSID, init);
}

static BOOL register_activex(void)
{
    DWORD regid;
    HRESULT hres;

    if(!init_registry(TRUE)) {
        init_registry(FALSE);
        return FALSE;
    }

    hres = CoRegisterClassObject(&CLSID_TestObj, (IUnknown *)&testobj_cf,
            CLSCTX_SERVER, REGCLS_MULTIPLEUSE, &regid);
    ok(hres == S_OK, "Could not register screipt engine: %08x\n", hres);
    return TRUE;
}

START_TEST(run)
{
    char **argv;
    int argc;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    register_activex();

    argc = winetest_get_mainargs(&argv);
    if(argc > 2)
        run_test(argv[2]);
    else
        EnumResourceNamesA(NULL, "TESTSCRIPT", test_enum_proc, 0);

    init_registry(FALSE);
    CoUninitialize();
}

/*
 * Unit test suite for GDI objects
 *
 * Copyright 2002 Mike McCormack
 * Copyright 2004 Dmitry Timoshkov
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

#include <stdarg.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/test.h"

static void test_gdi_objects(void)
{
    BYTE buff[256];
    HDC hdc = GetDC(NULL);
    HPEN hp;
    int i;
    BOOL ret;

    /* SelectObject() with a NULL DC returns 0 and sets ERROR_INVALID_HANDLE.
     * Note: Under XP at least invalid ptrs can also be passed, not just NULL;
     *       Don't test that here in case it crashes earlier win versions.
     */
    SetLastError(0);
    hp = SelectObject(NULL, GetStockObject(BLACK_PEN));
    ok(!hp && (GetLastError() == ERROR_INVALID_HANDLE || broken(!GetLastError())),
       "SelectObject(NULL DC) expected 0, ERROR_INVALID_HANDLE, got %p, %u\n",
       hp, GetLastError());

    /* With a valid DC and a NULL object, the call returns 0 but does not SetLastError() */
    SetLastError(0);
    hp = SelectObject(hdc, NULL);
    ok(!hp && !GetLastError(),
       "SelectObject(NULL obj) expected 0, NO_ERROR, got %p, %u\n",
       hp, GetLastError());

    /* The DC is unaffected by the NULL SelectObject */
    SetLastError(0);
    hp = SelectObject(hdc, GetStockObject(BLACK_PEN));
    ok(hp && !GetLastError(),
       "SelectObject(post NULL) expected non-null, NO_ERROR, got %p, %u\n",
       hp, GetLastError());

    /* GetCurrentObject does not SetLastError() on a null object */
    SetLastError(0);
    hp = GetCurrentObject(NULL, OBJ_PEN);
    ok(!hp && !GetLastError(),
       "GetCurrentObject(NULL DC) expected 0, NO_ERROR, got %p, %u\n",
       hp, GetLastError());

    /* DeleteObject does not SetLastError() on a null object */
    ret = DeleteObject(NULL);
    ok( !ret && !GetLastError(),
       "DeleteObject(NULL obj), expected 0, NO_ERROR, got %d, %u\n",
       ret, GetLastError());

    /* GetObject does not SetLastError() on a null object */
    SetLastError(0);
    i = GetObjectA(NULL, sizeof(buff), buff);
    ok (!i && (GetLastError() == 0 || GetLastError() == ERROR_INVALID_PARAMETER),
        "GetObject(NULL obj), expected 0, NO_ERROR, got %d, %u\n",
	i, GetLastError());

    /* GetObject expects ERROR_NOACCESS when passed an invalid buffer */
    hp = SelectObject(hdc, GetStockObject(BLACK_PEN));
    SetLastError(0);
    i = GetObjectA(hp, (INT_PTR)buff, (LPVOID)sizeof(buff));
    ok (!i && (GetLastError() == 0 || GetLastError() == ERROR_NOACCESS),
        "GetObject(invalid buff), expected 0, ERROR_NOACCESS, got %d, %u\n",
    i, GetLastError());

    /* GetObjectType does SetLastError() on a null object */
    SetLastError(0);
    i = GetObjectType(NULL);
    ok (!i && GetLastError() == ERROR_INVALID_HANDLE,
        "GetObjectType(NULL obj), expected 0, ERROR_INVALID_HANDLE, got %d, %u\n",
        i, GetLastError());

    /* UnrealizeObject does not SetLastError() on a null object */
    SetLastError(0);
    i = UnrealizeObject(NULL);
    ok (!i && !GetLastError(),
        "UnrealizeObject(NULL obj), expected 0, NO_ERROR, got %d, %u\n",
        i, GetLastError());

    ReleaseDC(NULL, hdc);
}

struct hgdiobj_event
{
    HDC hdc;
    HGDIOBJ hgdiobj1;
    HGDIOBJ hgdiobj2;
    HANDLE stop_event;
    HANDLE ready_event;
};

static DWORD WINAPI thread_proc(void *param)
{
    LOGPEN lp;
    struct hgdiobj_event *hgdiobj_event = param;

    hgdiobj_event->hdc = CreateDC("display", NULL, NULL, NULL);
    ok(hgdiobj_event->hdc != NULL, "CreateDC error %u\n", GetLastError());

    hgdiobj_event->hgdiobj1 = CreatePen(PS_DASHDOTDOT, 17, RGB(1, 2, 3));
    ok(hgdiobj_event->hgdiobj1 != 0, "Failed to create pen\n");

    hgdiobj_event->hgdiobj2 = CreateRectRgn(0, 1, 12, 17);
    ok(hgdiobj_event->hgdiobj2 != 0, "Failed to create pen\n");

    SetEvent(hgdiobj_event->ready_event);
    ok(WaitForSingleObject(hgdiobj_event->stop_event, INFINITE) == WAIT_OBJECT_0,
       "WaitForSingleObject error %u\n", GetLastError());

    ok(!GetObject(hgdiobj_event->hgdiobj1, sizeof(lp), &lp), "GetObject should fail\n");

    ok(!GetDeviceCaps(hgdiobj_event->hdc, TECHNOLOGY), "GetDeviceCaps(TECHNOLOGY) should fail\n");

    return 0;
}

static void test_thread_objects(void)
{
    LOGPEN lp;
    DWORD tid, type;
    HANDLE hthread;
    struct hgdiobj_event hgdiobj_event;
    INT ret;

    hgdiobj_event.stop_event = CreateEvent(NULL, 0, 0, NULL);
    ok(hgdiobj_event.stop_event != NULL, "CreateEvent error %u\n", GetLastError());
    hgdiobj_event.ready_event = CreateEvent(NULL, 0, 0, NULL);
    ok(hgdiobj_event.ready_event != NULL, "CreateEvent error %u\n", GetLastError());

    hthread = CreateThread(NULL, 0, thread_proc, &hgdiobj_event, 0, &tid);
    ok(hthread != NULL, "CreateThread error %u\n", GetLastError());

    ok(WaitForSingleObject(hgdiobj_event.ready_event, INFINITE) == WAIT_OBJECT_0,
       "WaitForSingleObject error %u\n", GetLastError());

    ok(GetObject(hgdiobj_event.hgdiobj1, sizeof(lp), &lp) == sizeof(lp),
       "GetObject error %u\n", GetLastError());
    ok(lp.lopnStyle == PS_DASHDOTDOT, "wrong pen style %d\n", lp.lopnStyle);
    ok(lp.lopnWidth.x == 17, "wrong pen width.y %d\n", lp.lopnWidth.x);
    ok(lp.lopnWidth.y == 0, "wrong pen width.y %d\n", lp.lopnWidth.y);
    ok(lp.lopnColor == RGB(1, 2, 3), "wrong pen width.y %08x\n", lp.lopnColor);

    ret = GetDeviceCaps(hgdiobj_event.hdc, TECHNOLOGY);
    ok(ret == DT_RASDISPLAY, "GetDeviceCaps(TECHNOLOGY) should return DT_RASDISPLAY not %d\n", ret);

    ok(DeleteObject(hgdiobj_event.hgdiobj1), "DeleteObject error %u\n", GetLastError());
    ok(DeleteDC(hgdiobj_event.hdc), "DeleteDC error %u\n", GetLastError());

    type = GetObjectType(hgdiobj_event.hgdiobj2);
    ok(type == OBJ_REGION, "GetObjectType returned %u\n", type);

    SetEvent(hgdiobj_event.stop_event);
    ok(WaitForSingleObject(hthread, INFINITE) == WAIT_OBJECT_0,
       "WaitForSingleObject error %u\n", GetLastError());
    CloseHandle(hthread);

    type = GetObjectType(hgdiobj_event.hgdiobj2);
    ok(type == OBJ_REGION, "GetObjectType returned %u\n", type);
    ok(DeleteObject(hgdiobj_event.hgdiobj2), "DeleteObject error %u\n", GetLastError());

    CloseHandle(hgdiobj_event.stop_event);
    CloseHandle(hgdiobj_event.ready_event);
}

static void test_GetCurrentObject(void)
{
    DWORD type;
    HPEN hpen;
    HBRUSH hbrush;
    HPALETTE hpal;
    HFONT hfont;
    HBITMAP hbmp;
    HRGN hrgn;
    HDC hdc;
    HCOLORSPACE hcs;
    HGDIOBJ hobj;
    LOGBRUSH lb;
    LOGCOLORSPACEA lcs;

    hdc = CreateCompatibleDC(0);
    assert(hdc != 0);

    type = GetObjectType(hdc);
    ok(type == OBJ_MEMDC, "GetObjectType returned %u\n", type);

    hpen = CreatePen(PS_SOLID, 10, RGB(10, 20, 30));
    assert(hpen != 0);
    SelectObject(hdc, hpen);
    hobj = GetCurrentObject(hdc, OBJ_PEN);
    ok(hobj == hpen, "OBJ_PEN is wrong: %p\n", hobj);
    hobj = GetCurrentObject(hdc, OBJ_EXTPEN);
    ok(hobj == hpen || broken(hobj == 0) /* win9x */, "OBJ_EXTPEN is wrong: %p\n", hobj);

    hbrush = CreateSolidBrush(RGB(10, 20, 30));
    assert(hbrush != 0);
    SelectObject(hdc, hbrush);
    hobj = GetCurrentObject(hdc, OBJ_BRUSH);
    ok(hobj == hbrush, "OBJ_BRUSH is wrong: %p\n", hobj);

    hpal = CreateHalftonePalette(hdc);
    assert(hpal != 0);
    SelectPalette(hdc, hpal, FALSE);
    hobj = GetCurrentObject(hdc, OBJ_PAL);
    ok(hobj == hpal, "OBJ_PAL is wrong: %p\n", hobj);

    hfont = CreateFontA(10, 5, 0, 0, FW_DONTCARE, 0, 0, 0, ANSI_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                        DEFAULT_PITCH, "MS Sans Serif");
    assert(hfont != 0);
    SelectObject(hdc, hfont);
    hobj = GetCurrentObject(hdc, OBJ_FONT);
    ok(hobj == hfont, "OBJ_FONT is wrong: %p\n", hobj);

    hbmp = CreateBitmap(100, 100, 1, 1, NULL);
    assert(hbmp != 0);
    SelectObject(hdc, hbmp);
    hobj = GetCurrentObject(hdc, OBJ_BITMAP);
    ok(hobj == hbmp, "OBJ_BITMAP is wrong: %p\n", hobj);

    assert(GetObject(hbrush, sizeof(lb), &lb) == sizeof(lb));
    hpen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_SQUARE | PS_JOIN_BEVEL,
                        10, &lb, 0, NULL);
    assert(hpen != 0);
    SelectObject(hdc, hpen);
    hobj = GetCurrentObject(hdc, OBJ_PEN);
    ok(hobj == hpen, "OBJ_PEN is wrong: %p\n", hobj);
    hobj = GetCurrentObject(hdc, OBJ_EXTPEN);
    ok(hobj == hpen || broken(hobj == 0) /* win9x */, "OBJ_EXTPEN is wrong: %p\n", hobj);

    hcs = GetColorSpace(hdc);
    if (hcs)
    {
        trace("current color space is not NULL\n");
        ok(GetLogColorSpaceA(hcs, &lcs, sizeof(lcs)), "GetLogColorSpace failed\n");
        hcs = CreateColorSpaceA(&lcs);
        ok(hcs != 0, "CreateColorSpace failed\n");
        SelectObject(hdc, hcs);
        hobj = GetCurrentObject(hdc, OBJ_COLORSPACE);
        ok(hobj == hcs || broken(hobj == 0) /* win9x */, "OBJ_COLORSPACE is wrong: %p\n", hobj);
    }

    hrgn = CreateRectRgn(1, 1, 100, 100);
    assert(hrgn != 0);
    SelectObject(hdc, hrgn);
    hobj = GetCurrentObject(hdc, OBJ_REGION);
    ok(!hobj, "OBJ_REGION is wrong: %p\n", hobj);

    DeleteDC(hdc);
}

static void test_region(void)
{
    HRGN hrgn = CreateRectRgn(10, 10, 20, 20);
    RECT rc = { 5, 5, 15, 15 };
    BOOL ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap left and right */
    SetRect( &rc, 15, 5, 5, 15 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap top and bottom */
    SetRect( &rc, 5, 15, 15, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap both */
    SetRect( &rc, 15, 15, 5, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    DeleteObject(hrgn);
    /* swap left and right in the region */
    hrgn = CreateRectRgn(20, 10, 10, 20);
    SetRect( &rc, 5, 5, 15, 15 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap left and right */
    SetRect( &rc, 15, 5, 5, 15 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap top and bottom */
    SetRect( &rc, 5, 15, 15, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    /* swap both */
    SetRect( &rc, 15, 15, 5, 5 );
    ret = RectInRegion( hrgn, &rc);
    ok( ret, "RectInRegion should return TRUE\n");
    DeleteObject(hrgn);
}

START_TEST(gdiobj)
{
    test_gdi_objects();
    test_thread_objects();
    test_GetCurrentObject();
    test_region();
}

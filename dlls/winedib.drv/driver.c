/*
 * Access to display driver
 *
 * Copyright 2009 Massimo Del Fedele
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

#include "dibdrv.h"
#include <stdio.h>

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/* CreateDriver
 * Allocate and fill the function pointer structure for a given module. */
static DC_FUNCTIONS *CreateDriver( HMODULE module )
{
    DC_FUNCTIONS *funcs;

    if (!(funcs = HeapAlloc( GetProcessHeap(), 0, sizeof(*funcs))))
        return NULL;

    /* fill the function table */
    if (module)
    {
#define GET_FUNC(name) funcs->p##name = (void*)GetProcAddress( module, #name )
        GET_FUNC(AbortDoc);
        GET_FUNC(AbortPath);
        GET_FUNC(AlphaBlend);
        GET_FUNC(AngleArc);
        GET_FUNC(Arc);
        GET_FUNC(ArcTo);
        GET_FUNC(BeginPath);
        GET_FUNC(BitBlt);
        GET_FUNC(ChoosePixelFormat);
        GET_FUNC(Chord);
        GET_FUNC(CloseFigure);
        GET_FUNC(CreateBitmap);
        GET_FUNC(CreateDC);
        GET_FUNC(CreateDIBSection);
        GET_FUNC(DeleteBitmap);
        GET_FUNC(DeleteDC);
        GET_FUNC(DescribePixelFormat);
        GET_FUNC(DeviceCapabilities);
        GET_FUNC(Ellipse);
        GET_FUNC(EndDoc);
        GET_FUNC(EndPage);
        GET_FUNC(EndPath);
        GET_FUNC(EnumDeviceFonts);
        GET_FUNC(ExcludeClipRect);
        GET_FUNC(ExtDeviceMode);
        GET_FUNC(ExtEscape);
        GET_FUNC(ExtFloodFill);
        GET_FUNC(ExtSelectClipRgn);
        GET_FUNC(ExtTextOut);
        GET_FUNC(FillPath);
        GET_FUNC(FillRgn);
        GET_FUNC(FlattenPath);
        GET_FUNC(FrameRgn);
        GET_FUNC(GdiComment);
        GET_FUNC(GetBitmapBits);
        GET_FUNC(GetCharWidth);
        GET_FUNC(GetDCOrgEx);
        GET_FUNC(GetDIBColorTable);
        GET_FUNC(GetDIBits);
        GET_FUNC(GetDeviceCaps);
        GET_FUNC(GetDeviceGammaRamp);
        GET_FUNC(GetICMProfile);
        GET_FUNC(GetNearestColor);
        GET_FUNC(GetPixel);
        GET_FUNC(GetPixelFormat);
        GET_FUNC(GetSystemPaletteEntries);
        GET_FUNC(GetTextExtentExPoint);
        GET_FUNC(GetTextMetrics);
        GET_FUNC(IntersectClipRect);
        GET_FUNC(InvertRgn);
        GET_FUNC(LineTo);
        GET_FUNC(MoveTo);
        GET_FUNC(ModifyWorldTransform);
        GET_FUNC(OffsetClipRgn);
        GET_FUNC(OffsetViewportOrg);
        GET_FUNC(OffsetWindowOrg);
        GET_FUNC(PaintRgn);
        GET_FUNC(PatBlt);
        GET_FUNC(Pie);
        GET_FUNC(PolyBezier);
        GET_FUNC(PolyBezierTo);
        GET_FUNC(PolyDraw);
        GET_FUNC(PolyPolygon);
        GET_FUNC(PolyPolyline);
        GET_FUNC(Polygon);
        GET_FUNC(Polyline);
        GET_FUNC(PolylineTo);
        GET_FUNC(RealizeDefaultPalette);
        GET_FUNC(RealizePalette);
        GET_FUNC(Rectangle);
        GET_FUNC(ResetDC);
        GET_FUNC(RestoreDC);
        GET_FUNC(RoundRect);
        GET_FUNC(SaveDC);
        GET_FUNC(ScaleViewportExt);
        GET_FUNC(ScaleWindowExt);
        GET_FUNC(SelectBitmap);
        GET_FUNC(SelectBrush);
        GET_FUNC(SelectClipPath);
        GET_FUNC(SelectFont);
        GET_FUNC(SelectPalette);
        GET_FUNC(SelectPen);
        GET_FUNC(SetArcDirection);
        GET_FUNC(SetBitmapBits);
        GET_FUNC(SetBkColor);
        GET_FUNC(SetBkMode);
        GET_FUNC(SetDCBrushColor);
        GET_FUNC(SetDCOrg);
        GET_FUNC(SetDCPenColor);
        GET_FUNC(SetDIBColorTable);
        GET_FUNC(SetDIBits);
        GET_FUNC(SetDIBitsToDevice);
        GET_FUNC(SetDeviceClipping);
        GET_FUNC(SetDeviceGammaRamp);
        GET_FUNC(SetMapMode);
        GET_FUNC(SetMapperFlags);
        GET_FUNC(SetPixel);
        GET_FUNC(SetPixelFormat);
        GET_FUNC(SetPolyFillMode);
        GET_FUNC(SetROP2);
        GET_FUNC(SetRelAbs);
        GET_FUNC(SetStretchBltMode);
        GET_FUNC(SetTextAlign);
        GET_FUNC(SetTextCharacterExtra);
        GET_FUNC(SetTextColor);
        GET_FUNC(SetTextJustification);
        GET_FUNC(SetViewportExt);
        GET_FUNC(SetViewportOrg);
        GET_FUNC(SetWindowExt);
        GET_FUNC(SetWindowOrg);
        GET_FUNC(SetWorldTransform);
        GET_FUNC(StartDoc);
        GET_FUNC(StartPage);
        GET_FUNC(StretchBlt);
        GET_FUNC(StretchDIBits);
        GET_FUNC(StrokeAndFillPath);
        GET_FUNC(StrokePath);
        GET_FUNC(SwapBuffers);
        GET_FUNC(UnrealizePalette);
        GET_FUNC(WidenPath);

        /* OpenGL32 */
        GET_FUNC(wglCreateContext);
        GET_FUNC(wglDeleteContext);
        GET_FUNC(wglGetProcAddress);
        GET_FUNC(wglGetPbufferDCARB);
        GET_FUNC(wglMakeContextCurrentARB);
        GET_FUNC(wglMakeCurrent);
        GET_FUNC(wglSetPixelFormatWINE);
        GET_FUNC(wglShareLists);
        GET_FUNC(wglUseFontBitmapsA);
        GET_FUNC(wglUseFontBitmapsW);
#undef GET_FUNC
    }
    else
        memset( funcs, 0, sizeof(*funcs) );

    /* add it to the list */
    return funcs;
}


/* LoadDisplayDriver
 * Loads display driver - partially grabbed from gdi32 */
static DC_FUNCTIONS *X11DrvFuncs = NULL;
static HMODULE X11DrvModule = 0;
DC_FUNCTIONS *_DIBDRV_LoadDisplayDriver(void)
{
    char buffer[MAX_PATH], libname[32], *name, *next;
    HMODULE module = 0;
    HKEY hkey;

    if (X11DrvFuncs)  /* already loaded */
        return X11DrvFuncs;
    
    strcpy( buffer, "x11" );  /* default value */
    /* @@ Wine registry key: HKCU\Software\Wine\Drivers */
    if (!RegOpenKeyA( HKEY_CURRENT_USER, "Software\\Wine\\Drivers", &hkey ))
    {
        DWORD type, count = sizeof(buffer);
        RegQueryValueExA( hkey, "Graphics", 0, &type, (LPBYTE) buffer, &count );
        RegCloseKey( hkey );
    }

    name = buffer;
    while (name)
    {
        next = strchr( name, ',' );
        if (next)
            *next++ = 0;

        snprintf( libname, sizeof(libname), "wine%s.drv", name );
        if ((module = LoadLibraryA( libname )) != 0)
            break;
        name = next;
    }

    if (!(X11DrvFuncs = CreateDriver(module)))
    {
        ERR( "Could not create graphics driver '%s'\n", buffer );
        FreeLibrary( module );
        ExitProcess(1);
    }

    X11DrvModule = module;
    return X11DrvFuncs;
}

/* FreeDisplayDriver
   Frees resources allocated by Display driver */
void _DIBDRV_FreeDisplayDriver(void)
{
    /* frees function table */
    if(X11DrvFuncs)
    {
        HeapFree(GetProcessHeap(), 0, X11DrvFuncs);
        X11DrvFuncs = 0;
    }
    /* and unload the module */
    if(X11DrvModule)
    {
        FreeLibrary(X11DrvModule);
        X11DrvModule = 0;
    }
}

/* GetDisplayDriver
   Gets a pointer to display drives'function table */
inline DC_FUNCTIONS *_DIBDRV_GetDisplayDriver(void)
{
    return X11DrvFuncs;

}

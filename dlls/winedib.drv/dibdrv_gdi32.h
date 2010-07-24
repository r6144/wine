/*
 * GDI structures - grabbed from dlls/gdi32/gdi_private.h
 * Those are needed to access some opaque gdi32 pointers
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

#ifndef __WINE_DIBDRV_GDI32_H
#define __WINE_DIBDRV_GDI32_H


/* driver function pointers - used here to forward calls to X11 driver when needed */
typedef struct { int opaque; } *PHYSDEV;  /* PHYSDEV is an opaque pointer */
typedef struct tagDC_FUNCS
{
    INT      (CDECL *pAbortDoc)(PHYSDEV);
    BOOL     (CDECL *pAbortPath)(PHYSDEV);
    BOOL     (CDECL *pAlphaBlend)(PHYSDEV,INT,INT,INT,INT,PHYSDEV,INT,INT,INT,INT,BLENDFUNCTION);
    BOOL     (CDECL *pAngleArc)(PHYSDEV,INT,INT,DWORD,FLOAT,FLOAT);
    BOOL     (CDECL *pArc)(PHYSDEV,INT,INT,INT,INT,INT,INT,INT,INT);
    BOOL     (CDECL *pArcTo)(PHYSDEV,INT,INT,INT,INT,INT,INT,INT,INT);
    BOOL     (CDECL *pBeginPath)(PHYSDEV);
    BOOL     (CDECL *pBitBlt)(PHYSDEV,INT,INT,INT,INT,PHYSDEV,INT,INT,DWORD);
    INT      (CDECL *pChoosePixelFormat)(PHYSDEV,const PIXELFORMATDESCRIPTOR *);
    BOOL     (CDECL *pChord)(PHYSDEV,INT,INT,INT,INT,INT,INT,INT,INT);
    BOOL     (CDECL *pCloseFigure)(PHYSDEV);
    BOOL     (CDECL *pCreateBitmap)(PHYSDEV,HBITMAP,LPVOID);
    BOOL     (CDECL *pCreateDC)(HDC,PHYSDEV *,LPCWSTR,LPCWSTR,LPCWSTR,const DEVMODEW*);
    HBITMAP  (CDECL *pCreateDIBSection)(PHYSDEV,HBITMAP,const BITMAPINFO *,UINT);
    BOOL     (CDECL *pDeleteBitmap)(HBITMAP);
    BOOL     (CDECL *pDeleteDC)(PHYSDEV);
    BOOL     (CDECL *pDeleteObject)(PHYSDEV,HGDIOBJ);
    INT      (CDECL *pDescribePixelFormat)(PHYSDEV,INT,UINT,PIXELFORMATDESCRIPTOR *);
    DWORD    (CDECL *pDeviceCapabilities)(LPSTR,LPCSTR,LPCSTR,WORD,LPSTR,LPDEVMODEA);
    BOOL     (CDECL *pEllipse)(PHYSDEV,INT,INT,INT,INT);
    INT      (CDECL *pEndDoc)(PHYSDEV);
    INT      (CDECL *pEndPage)(PHYSDEV);
    BOOL     (CDECL *pEndPath)(PHYSDEV);
    BOOL     (CDECL *pEnumDeviceFonts)(PHYSDEV,LPLOGFONTW,FONTENUMPROCW,LPARAM);
    INT      (CDECL *pExcludeClipRect)(PHYSDEV,INT,INT,INT,INT);
    INT      (CDECL *pExtDeviceMode)(LPSTR,HWND,LPDEVMODEA,LPSTR,LPSTR,LPDEVMODEA,LPSTR,DWORD);
    INT      (CDECL *pExtEscape)(PHYSDEV,INT,INT,LPCVOID,INT,LPVOID);
    BOOL     (CDECL *pExtFloodFill)(PHYSDEV,INT,INT,COLORREF,UINT);
    INT      (CDECL *pExtSelectClipRgn)(PHYSDEV,HRGN,INT);
    BOOL     (CDECL *pExtTextOut)(PHYSDEV,INT,INT,UINT,const RECT*,LPCWSTR,UINT,const INT*);
    BOOL     (CDECL *pFillPath)(PHYSDEV);
    BOOL     (CDECL *pFillRgn)(PHYSDEV,HRGN,HBRUSH);
    BOOL     (CDECL *pFlattenPath)(PHYSDEV);
    BOOL     (CDECL *pFrameRgn)(PHYSDEV,HRGN,HBRUSH,INT,INT);
    BOOL     (CDECL *pGdiComment)(PHYSDEV,UINT,CONST BYTE*);
    LONG     (CDECL *pGetBitmapBits)(HBITMAP,void*,LONG);
    BOOL     (CDECL *pGetCharWidth)(PHYSDEV,UINT,UINT,LPINT);
    BOOL     (CDECL *pGetDCOrgEx)(PHYSDEV,LPPOINT);
    UINT     (CDECL *pGetDIBColorTable)(PHYSDEV,UINT,UINT,RGBQUAD*);
    INT      (CDECL *pGetDIBits)(PHYSDEV,HBITMAP,UINT,UINT,LPCVOID,const BITMAPINFO*,UINT);
    INT      (CDECL *pGetDeviceCaps)(PHYSDEV,INT);
    BOOL     (CDECL *pGetDeviceGammaRamp)(PHYSDEV,LPVOID);
    BOOL     (CDECL *pGetICMProfile)(PHYSDEV,LPDWORD,LPWSTR);
    COLORREF (CDECL *pGetNearestColor)(PHYSDEV,COLORREF);
    COLORREF (CDECL *pGetPixel)(PHYSDEV,INT,INT);
    INT      (CDECL *pGetPixelFormat)(PHYSDEV);
    UINT     (CDECL *pGetSystemPaletteEntries)(PHYSDEV,UINT,UINT,LPPALETTEENTRY);
    BOOL     (CDECL *pGetTextExtentExPoint)(PHYSDEV,LPCWSTR,INT,INT,LPINT,LPINT,LPSIZE);
    BOOL     (CDECL *CDECL pGetTextMetrics)(PHYSDEV,TEXTMETRICW*);
    INT      (CDECL *pIntersectClipRect)(PHYSDEV,INT,INT,INT,INT);
    BOOL     (CDECL *pInvertRgn)(PHYSDEV,HRGN);
    BOOL     (CDECL *pLineTo)(PHYSDEV,INT,INT);
    BOOL     (CDECL *pModifyWorldTransform)(PHYSDEV,const XFORM*,INT);
    BOOL     (CDECL *pMoveTo)(PHYSDEV,INT,INT);
    INT      (CDECL *pOffsetClipRgn)(PHYSDEV,INT,INT);
    INT      (CDECL *pOffsetViewportOrg)(PHYSDEV,INT,INT);
    INT      (CDECL *pOffsetWindowOrg)(PHYSDEV,INT,INT);
    BOOL     (CDECL *pPaintRgn)(PHYSDEV,HRGN);
    BOOL     (CDECL *pPatBlt)(PHYSDEV,INT,INT,INT,INT,DWORD);
    BOOL     (CDECL *pPie)(PHYSDEV,INT,INT,INT,INT,INT,INT,INT,INT);
    BOOL     (CDECL *pPolyBezier)(PHYSDEV,const POINT*,DWORD);
    BOOL     (CDECL *pPolyBezierTo)(PHYSDEV,const POINT*,DWORD);
    BOOL     (CDECL *pPolyDraw)(PHYSDEV,const POINT*,const BYTE *,DWORD);
    BOOL     (CDECL *pPolyPolygon)(PHYSDEV,const POINT*,const INT*,UINT);
    BOOL     (CDECL *pPolyPolyline)(PHYSDEV,const POINT*,const DWORD*,DWORD);
    BOOL     (CDECL *pPolygon)(PHYSDEV,const POINT*,INT);
    BOOL     (CDECL *pPolyline)(PHYSDEV,const POINT*,INT);
    BOOL     (CDECL *pPolylineTo)(PHYSDEV,const POINT*,INT);
    UINT     (CDECL *pRealizeDefaultPalette)(PHYSDEV);
    UINT     (CDECL *pRealizePalette)(PHYSDEV,HPALETTE,BOOL);
    BOOL     (CDECL *pRectangle)(PHYSDEV,INT,INT,INT,INT);
    HDC      (CDECL *pResetDC)(PHYSDEV,const DEVMODEW*);
    BOOL     (CDECL *pRestoreDC)(PHYSDEV,INT);
    BOOL     (CDECL *pRoundRect)(PHYSDEV,INT,INT,INT,INT,INT,INT);
    INT      (CDECL *pSaveDC)(PHYSDEV);
    INT      (CDECL *pScaleViewportExt)(PHYSDEV,INT,INT,INT,INT);
    INT      (CDECL *pScaleWindowExt)(PHYSDEV,INT,INT,INT,INT);
    HBITMAP  (CDECL *pSelectBitmap)(PHYSDEV,HBITMAP);
    HBRUSH   (CDECL *pSelectBrush)(PHYSDEV,HBRUSH);
    BOOL     (CDECL *pSelectClipPath)(PHYSDEV,INT);
    HFONT    (CDECL *pSelectFont)(PHYSDEV,HFONT,HANDLE);
    HPALETTE (CDECL *pSelectPalette)(PHYSDEV,HPALETTE,BOOL);
    HPEN     (CDECL *pSelectPen)(PHYSDEV,HPEN);
    INT      (CDECL *pSetArcDirection)(PHYSDEV,INT);
    LONG     (CDECL *pSetBitmapBits)(HBITMAP,const void*,LONG);
    COLORREF (CDECL *pSetBkColor)(PHYSDEV,COLORREF);
    INT      (CDECL *pSetBkMode)(PHYSDEV,INT);
    COLORREF (CDECL *pSetDCBrushColor)(PHYSDEV, COLORREF);
    DWORD    (CDECL *pSetDCOrg)(PHYSDEV,INT,INT);
    COLORREF (CDECL *pSetDCPenColor)(PHYSDEV, COLORREF);
    UINT     (CDECL *pSetDIBColorTable)(PHYSDEV,UINT,UINT,const RGBQUAD*);
    INT      (CDECL *pSetDIBits)(PHYSDEV,HBITMAP,UINT,UINT,LPCVOID,const BITMAPINFO*,UINT);
    INT      (CDECL *pSetDIBitsToDevice)(PHYSDEV,INT,INT,DWORD,DWORD,INT,INT,UINT,UINT,LPCVOID,
                                           const BITMAPINFO*,UINT);
    VOID     (CDECL *pSetDeviceClipping)(PHYSDEV,HRGN,HRGN);
    BOOL     (CDECL *pSetDeviceGammaRamp)(PHYSDEV,LPVOID);
    INT      (CDECL *pSetMapMode)(PHYSDEV,INT);
    DWORD    (CDECL *pSetMapperFlags)(PHYSDEV,DWORD);
    COLORREF (CDECL *pSetPixel)(PHYSDEV,INT,INT,COLORREF);
    BOOL     (CDECL *pSetPixelFormat)(PHYSDEV,INT,const PIXELFORMATDESCRIPTOR *);
    INT      (CDECL *pSetPolyFillMode)(PHYSDEV,INT);
    INT      (CDECL *pSetROP2)(PHYSDEV,INT);
    INT      (CDECL *pSetRelAbs)(PHYSDEV,INT);
    INT      (CDECL *pSetStretchBltMode)(PHYSDEV,INT);
    UINT     (CDECL *pSetTextAlign)(PHYSDEV,UINT);
    INT      (CDECL *pSetTextCharacterExtra)(PHYSDEV,INT);
    DWORD    (CDECL *pSetTextColor)(PHYSDEV,DWORD);
    INT      (CDECL *pSetTextJustification)(PHYSDEV,INT,INT);
    INT      (CDECL *pSetViewportExt)(PHYSDEV,INT,INT);
    INT      (CDECL *pSetViewportOrg)(PHYSDEV,INT,INT);
    INT      (CDECL *pSetWindowExt)(PHYSDEV,INT,INT);
    INT      (CDECL *pSetWindowOrg)(PHYSDEV,INT,INT);
    BOOL     (CDECL *pSetWorldTransform)(PHYSDEV,const XFORM*);
    INT      (CDECL *pStartDoc)(PHYSDEV,const DOCINFOW*);
    INT      (CDECL *pStartPage)(PHYSDEV);
    BOOL     (CDECL *pStretchBlt)(PHYSDEV,INT,INT,INT,INT,PHYSDEV,INT,INT,INT,INT,DWORD);
    INT      (CDECL *pStretchDIBits)(PHYSDEV,INT,INT,INT,INT,INT,INT,INT,INT,const void *,
                                       const BITMAPINFO*,UINT,DWORD);
    BOOL     (CDECL *pStrokeAndFillPath)(PHYSDEV);
    BOOL     (CDECL *pStrokePath)(PHYSDEV);
    BOOL     (CDECL *pSwapBuffers)(PHYSDEV);
    BOOL     (CDECL *pUnrealizePalette)(HPALETTE);
    BOOL     (CDECL *pWidenPath)(PHYSDEV);

    /* OpenGL32 */
    BOOL     (CDECL *pwglCopyContext)(HGLRC, HGLRC, UINT);
    HGLRC    (CDECL *pwglCreateContext)(PHYSDEV);
    BOOL     (CDECL *pwglDeleteContext)(HGLRC);
    PROC     (CDECL *pwglGetProcAddress)(LPCSTR);
    HDC      (CDECL *pwglGetPbufferDCARB)(PHYSDEV, void*);
    BOOL     (CDECL *pwglMakeCurrent)(PHYSDEV, HGLRC);
    BOOL     (CDECL *pwglMakeContextCurrentARB)(PHYSDEV, PHYSDEV, HGLRC);
    BOOL     (CDECL *pwglSetPixelFormatWINE)(PHYSDEV,INT,const PIXELFORMATDESCRIPTOR *);
    BOOL     (CDECL *pwglShareLists)(HGLRC hglrc1, HGLRC hglrc2);
    BOOL     (CDECL *pwglUseFontBitmapsA)(PHYSDEV, DWORD, DWORD, DWORD);
    BOOL     (CDECL *pwglUseFontBitmapsW)(PHYSDEV, DWORD, DWORD, DWORD);
} DC_FUNCTIONS;

#endif

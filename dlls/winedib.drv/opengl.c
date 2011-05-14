/*
 * DIBDRV OpenGL functions
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

#define HPBUFFERARB void *

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

int DIBDRV_ChoosePixelFormat( DIBDRVPHYSDEV *physDev,
                              const PIXELFORMATDESCRIPTOR *ppfd )
{
    int res;
    
    TRACE("physDev:%p, ppfd:%p\n", physDev, ppfd);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pChoosePixelFormat(physDev->X11PhysDev, ppfd);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pChoosePixelFormat(physDev->X11PhysDev, ppfd);
    }
    return res;
}

int DIBDRV_DescribePixelFormat( DIBDRVPHYSDEV *physDev,
                                int iPixelFormat,
                                UINT nBytes,
                                PIXELFORMATDESCRIPTOR *ppfd )
{
    int res;
    
    TRACE("physDev:%p, iPixelFormat:%d, nBytes:%d, ppfd:%p\n", physDev, iPixelFormat, nBytes, ppfd);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pDescribePixelFormat(physDev->X11PhysDev, iPixelFormat, nBytes, ppfd);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pDescribePixelFormat(physDev->X11PhysDev, iPixelFormat, nBytes, ppfd);
    }
    return res;
}

int DIBDRV_GetPixelFormat( DIBDRVPHYSDEV *physDev)
{
    int res;
    
    TRACE("physDev:%p\n", physDev);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetPixelFormat(physDev->X11PhysDev);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetPixelFormat(physDev->X11PhysDev);
    }
    return res;
}

BOOL DIBDRV_SetPixelFormat( DIBDRVPHYSDEV *physDev,
                            int iPixelFormat,
                            const PIXELFORMATDESCRIPTOR *ppfd )
{
    BOOL res;
    
    TRACE("physDev:%p, iPixelFormat:%d, ppfd:%p\n", physDev, iPixelFormat, ppfd);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetPixelFormat(physDev->X11PhysDev, iPixelFormat, ppfd);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetPixelFormat(physDev->X11PhysDev, iPixelFormat, ppfd);
    }
    return res;
}

BOOL DIBDRV_SwapBuffers( DIBDRVPHYSDEV *physDev )
{
    BOOL res;
    
    TRACE("physDev:%p\n", physDev);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSwapBuffers(physDev->X11PhysDev);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSwapBuffers(physDev->X11PhysDev);
    }
    return res;
}

/**
 * DIBDRV_wglCopyContext
 *
 * For OpenGL32 wglCopyContext.
 */
BOOL CDECL DIBDRV_wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
    BOOL res;
    
    TRACE("hglrcSrc:%p, hglrcDst:%p, mask:%x\n", hglrcSrc, hglrcDst, mask);

    ONCE(FIXME("stub\n"));
    res = _DIBDRV_GetDisplayDriver()->pwglCopyContext(hglrcSrc, hglrcDst, mask);

    return res;
}

/**
 * DIBDRV_wglCreateContext
 *
 * For OpenGL32 wglCreateContext.
 */
HGLRC CDECL DIBDRV_wglCreateContext(DIBDRVPHYSDEV *physDev)
{
    HGLRC res;
    
    TRACE("physDev:%p\n", physDev);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglCreateContext(physDev->X11PhysDev);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pwglCreateContext(physDev->X11PhysDev);
    }
    return res;
}

/**
 * DIBDRV_wglDeleteContext
 *
 * For OpenGL32 wglDeleteContext.
 */
BOOL CDECL DIBDRV_wglDeleteContext(HGLRC hglrc)
{
    BOOL res;
    
    TRACE("hglrc:%p\n", hglrc);

    ONCE(FIXME("stub\n"));
    res = _DIBDRV_GetDisplayDriver()->pwglDeleteContext(hglrc);
    return res;
}

/**
 * DIBDRV_wglGetProcAddress
 *
 * For OpenGL32 wglGetProcAddress.
 */
PROC CDECL DIBDRV_wglGetProcAddress(LPCSTR lpszProc)
{
    PROC res;
    
    TRACE("lpszProc:%p\n", lpszProc);

    ONCE(FIXME("stub\n"));
    res = _DIBDRV_GetDisplayDriver()->pwglGetProcAddress(lpszProc);

    return res;
}

/**
 * DIBDRV_wglGetPbufferDCARB
 *
 * WGL_ARB_pbuffer: wglGetPbufferDCARB
 * The function wglGetPbufferDCARB returns a device context for a pbuffer.
 * Gdi32 implements the part of this function which creates a device context.
 * This part associates the physDev with the X drawable of the pbuffer.
 */
HDC CDECL DIBDRV_wglGetPbufferDCARB(DIBDRVPHYSDEV *physDev, HPBUFFERARB hPbuffer)
{
    HDC res;
    
    TRACE("physDev:%p, hPbuffer:%p\n", physDev, hPbuffer);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglGetPbufferDCARB(physDev->X11PhysDev, hPbuffer);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pwglGetPbufferDCARB(physDev->X11PhysDev, hPbuffer);
    }
    return res;
}

/**
 * DIBDRV_wglMakeContextCurrentARB
 *
 * For OpenGL32 wglMakeContextCurrentARB
 */
BOOL CDECL DIBDRV_wglMakeContextCurrentARB(DIBDRVPHYSDEV* pDrawDev, DIBDRVPHYSDEV* pReadDev, HGLRC hglrc)
{
    BOOL res;
    
    TRACE("pDrawDev:%p, pReadDev:%p, hglrc:%p\n", pDrawDev, pReadDev, hglrc);

    if(pDrawDev->hasDIB && pReadDev->hasDIB)
    {
        /* DIB section selected both in source and dest DCs, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglMakeContextCurrentARB(pDrawDev->X11PhysDev, pReadDev->X11PhysDev, hglrc);
    }
    if(!pDrawDev->hasDIB && !pReadDev->hasDIB)
    {
        /* DDB selected both in source and dest DCs, use X11 Driver */
        res = _DIBDRV_GetDisplayDriver()->pwglMakeContextCurrentARB(pDrawDev->X11PhysDev, pReadDev->X11PhysDev, hglrc);
    }
    else if(pDrawDev->hasDIB)
    {
        /* DIB selected in pDrawDev, must convert pReadDev to DIB and use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglMakeContextCurrentARB(pDrawDev->X11PhysDev, pReadDev->X11PhysDev, hglrc);
    }
    else /* if(pReadDev->hasDIB) */
    {
        /* DIB selected in pReadDev, must convert pReadDev to DDB and use X11 Driver */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglMakeContextCurrentARB(pDrawDev->X11PhysDev, pReadDev->X11PhysDev, hglrc);
    }
    return res;
}

/**
 * DIBDRV_wglMakeCurrent
 *
 * For OpenGL32 wglMakeCurrent.
 */
BOOL CDECL DIBDRV_wglMakeCurrent(DIBDRVPHYSDEV *physDev, HGLRC hglrc)
{
    BOOL res;
    
    TRACE("physDev:%p, hglrc:%p\n", physDev, hglrc);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglMakeCurrent(physDev->X11PhysDev, hglrc);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pwglMakeCurrent(physDev->X11PhysDev, hglrc);
    }
    return res;
}

/**
 * DIBDRV_wglSetPixelFormatWINE
 *
 * WGL_WINE_pixel_format_passthrough: wglSetPixelFormatWINE
 * This is a WINE-specific wglSetPixelFormat which can set the pixel format multiple times.
 */
BOOL CDECL DIBDRV_wglSetPixelFormatWINE(DIBDRVPHYSDEV *physDev, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd)
{
    BOOL res;
    
    TRACE("physDev:%p, iPixelFormat:%d, ppfd:%p\n", physDev, iPixelFormat, ppfd);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglSetPixelFormatWINE(physDev->X11PhysDev, iPixelFormat, ppfd);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pwglSetPixelFormatWINE(physDev->X11PhysDev, iPixelFormat, ppfd);
    }
    return res;
}

/**
 * DIBDRV_wglShareLists
 *
 * For OpenGL32 wglShareLists.
 */
BOOL CDECL DIBDRV_wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
    BOOL res;
    
    TRACE("hglrc1:%p, hglrc2:%p\n", hglrc1, hglrc2);

    ONCE(FIXME("stub\n"));
    res = _DIBDRV_GetDisplayDriver()->pwglShareLists(hglrc1, hglrc2);

    return res;
}

/**
 * DIBDRV_wglUseFontBitmapsA
 *
 * For OpenGL32 wglUseFontBitmapsA.
 */
BOOL CDECL DIBDRV_wglUseFontBitmapsA(DIBDRVPHYSDEV *physDev, DWORD first, DWORD count, DWORD listBase)
{
    BOOL res;
    
    TRACE("physDev:%p, first:%d, count:%d, listBase:%d\n", physDev, first, count, listBase);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglUseFontBitmapsA(physDev->X11PhysDev, first, count, listBase);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pwglUseFontBitmapsA(physDev->X11PhysDev, first, count, listBase);
    }
    return res;
}

/**
 * DIBDRV_wglUseFontBitmapsW
 *
 * For OpenGL32 wglUseFontBitmapsW.
 */
BOOL CDECL DIBDRV_wglUseFontBitmapsW(DIBDRVPHYSDEV *physDev, DWORD first, DWORD count, DWORD listBase)
{
    BOOL res;
    
    TRACE("physDev:%p, first:%d, count:%d, listBase:%d\n", physDev, first, count, listBase);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pwglUseFontBitmapsW(physDev->X11PhysDev, first, count, listBase);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pwglUseFontBitmapsW(physDev->X11PhysDev, first, count, listBase);
    }
    return res;
}

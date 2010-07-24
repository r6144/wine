/*
 * DIBDRV font objects
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
 *
 */

#include "config.h"
#include "wine/port.h"

#include "dibdrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/**********************************************************************
 *          DIBDRV_SetTextColor
 */
COLORREF DIBDRV_SetTextColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    COLORREF res;
    
    TRACE("physDev:%p, color:%08x\n", physDev, color);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetTextColor(physDev->X11PhysDev, color);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetTextColor(physDev->X11PhysDev, color);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SelectFont
 */
HFONT DIBDRV_SelectFont( DIBDRVPHYSDEV *physDev, HFONT hfont, HANDLE gdiFont )
{
    HFONT res;
    
    TRACE("physDev:%p, hfont:%p, gdiFont:%p\n", physDev, hfont, gdiFont);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSelectFont(physDev->X11PhysDev, hfont, gdiFont);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSelectFont(physDev->X11PhysDev, hfont, gdiFont);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_EnumDeviceFonts
 */
BOOL DIBDRV_EnumDeviceFonts( DIBDRVPHYSDEV *physDev, LPLOGFONTW plf,
                             FONTENUMPROCW proc, LPARAM lp )
{
    BOOL res;
    
    TRACE("physDev:%p, plf:%p, proc:%p, lp:%lx\n", physDev, plf, proc, lp);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pEnumDeviceFonts(physDev->X11PhysDev, plf, proc, lp);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pEnumDeviceFonts(physDev->X11PhysDev, plf, proc, lp);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetTextMetrics
 */
BOOL DIBDRV_GetTextMetrics( DIBDRVPHYSDEV *physDev, TEXTMETRICW *metrics )
{
    BOOL res;
    
    TRACE("physDev:%p, metrics:%p\n", physDev, metrics);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetTextMetrics(physDev->X11PhysDev, metrics);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetTextMetrics(physDev->X11PhysDev, metrics);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetCharWidth
 */
BOOL DIBDRV_GetCharWidth( DIBDRVPHYSDEV *physDev, UINT firstChar, UINT lastChar,
                          LPINT buffer )
{
    BOOL res;
    
    TRACE("physDev:%p, firstChar:%d, lastChar:%d, buffer:%pn", physDev, firstChar, lastChar, buffer);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetCharWidth(physDev->X11PhysDev, firstChar, lastChar, buffer);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetCharWidth(physDev->X11PhysDev, firstChar, lastChar, buffer);
    }
    return res;
}

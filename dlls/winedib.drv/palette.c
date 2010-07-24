/*
 * DIBDRV palette objects
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

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/***********************************************************************
 *              DIBDRV_RealizePalette
 */
UINT DIBDRV_RealizePalette( DIBDRVPHYSDEV *physDev, HPALETTE hpal, BOOL primary )
{
    UINT res;
    
    TRACE("physDev:%p, hpal:%p, primary:%s\n", physDev, hpal, (primary ? "TRUE" : "FALSE"));
    
    /* we should in any case call X11 function, as UnrealizePalette() doesn't
     * take a physDev parameter */
    res = _DIBDRV_GetDisplayDriver()->pRealizePalette(physDev->X11PhysDev, hpal, primary);
    
    if(physDev->hasDIB)
    {
        /* DIB section selected in, additional (if needed) engine code */
        ONCE(FIXME("STUB\n"));
    }

    return res;
}

/***********************************************************************
 *              DIBDRV_UnrealizePalette
 */
BOOL DIBDRV_UnrealizePalette( HPALETTE hpal )
{
    BOOL res;
    
    TRACE("hpal:%p\n", hpal);

    /* we should in any case call X11 function, as UnrealizePalette() doesn't
     * take a physDev parameter */
    res = _DIBDRV_GetDisplayDriver()->pUnrealizePalette(hpal);
    
    /* additional Engine code here, if needed */
    ONCE(FIXME("STUB\n"));

    return res;
}

/***********************************************************************
 *              DIBDRV_GetSystemPaletteEntries
 */
UINT DIBDRV_GetSystemPaletteEntries( DIBDRVPHYSDEV *physDev, UINT start, UINT count,
                                     LPPALETTEENTRY entries )
{
    UINT res;
    
    TRACE("physDev:%p, start:%d, count:%d, entries:%p\n", physDev, start, count, entries);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetSystemPaletteEntries(physDev->X11PhysDev, start, count, entries);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetSystemPaletteEntries(physDev->X11PhysDev, start, count, entries);
    }
    return res;
}

/***********************************************************************
 *              DIBDRV_GetNearestColor
 */
COLORREF DIBDRV_GetNearestColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    COLORREF res;
    
    TRACE("physDev:%p, color:%x\n", physDev, color);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetNearestColor(physDev->X11PhysDev, color);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetNearestColor(physDev->X11PhysDev, color);
    }
    return res;
}

/***********************************************************************
 *              DIBDRV_RealizeDefaultPalette
 */
UINT DIBDRV_RealizeDefaultPalette( DIBDRVPHYSDEV *physDev )
{
    UINT res;
    
    TRACE("physDev:%p\n", physDev);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pRealizeDefaultPalette(physDev->X11PhysDev);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pRealizeDefaultPalette(physDev->X11PhysDev);
    }
    return res;
}

BOOL DIBDRV_GetICMProfile(DIBDRVPHYSDEV *physDev, LPDWORD lpcbName, LPWSTR lpszFilename)
{
    BOOL res;
    
    TRACE("physDev:%p, lpcpName:%p, lpszFilename:%p\n", physDev, lpcbName, lpszFilename);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetICMProfile(physDev->X11PhysDev, lpcbName, lpszFilename);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetICMProfile(physDev->X11PhysDev, lpcbName, lpszFilename);
    }
    return res;
}

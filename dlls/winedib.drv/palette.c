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
    UINT res = 0;
    
    MAYBE(TRACE("physDev:%p, hpal:%p, primary:%s\n", physDev, hpal, (primary ? "TRUE" : "FALSE")));
    
    if(physDev && physDev->hasDIB)
    {
        /* DIB section selected in, additional (if needed) engine code */
        ONCE(FIXME("STUB\n"));
        res = 0;
    }
    else
    {
        /* we should in any case call X11 function, as UnrealizePalette() doesn't
         * take a physDev parameter */
        res = _DIBDRV_GetDisplayDriver()->pRealizePalette(physDev ? physDev->X11PhysDev : NULL, hpal, primary);
    
    }

    return res;
}

/***********************************************************************
 *              DIBDRV_UnrealizePalette
 */
BOOL DIBDRV_UnrealizePalette( HPALETTE hpal )
{
    BOOL res;
    
    MAYBE(TRACE("hpal:%p\n", hpal));

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
    
    MAYBE(TRACE("physDev:%p, start:%d, count:%d, entries:%p\n", physDev, start, count, entries));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = 0;
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
    
    MAYBE(TRACE("physDev:%p, color:%x\n", physDev, color));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = 0;
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
#ifdef DIBDRV_ENABLE_MAYBE
    int i;
    RGBQUAD *q;
#endif
    
    MAYBE(TRACE("physDev:%p\n", physDev));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        /* HACK - we can't get the dib color table during SelectBitmap since it hasn't
           been initialized yet.  This is called from DC_InitDC so it's a convenient place
           to grab the color table. */
        MAYBE(TRACE("Color table size = %d, Color table = %p\n", physDev->physBitmap.colorTableSize, physDev->physBitmap.colorTable));
        if(!physDev->physBitmap.colorTableGrabbed)
        {
            MAYBE(TRACE("Grabbing palette\n"));
            physDev->physBitmap.colorTable = HeapAlloc(GetProcessHeap(), 0, sizeof(physDev->physBitmap.colorTable[0]) * physDev->physBitmap.colorTableSize);
            GetDIBColorTable(physDev->hdc, 0, physDev->physBitmap.colorTableSize, physDev->physBitmap.colorTable);
#ifdef DIBDRV_ENABLE_MAYBE 
           for(i = 0; i < physDev->physBitmap.colorTableSize; i++)
            {
                q = physDev->physBitmap.colorTable + i;
                TRACE("  %03d : R%03d G%03d B%03d\n", i, q->rgbRed, q->rgbGreen, q->rgbBlue);
            }
#endif
            physDev->physBitmap.colorTableGrabbed = TRUE;
        }
        res = 0;
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
    
    MAYBE(TRACE("physDev:%p, lpcpName:%p, lpszFilename:%p\n", physDev, lpcbName, lpszFilename));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));

        res = 0;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetICMProfile(physDev->X11PhysDev, lpcbName, lpszFilename);
    }
    return res;
}

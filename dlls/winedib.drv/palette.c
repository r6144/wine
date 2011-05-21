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

/* maps a colorref to actual color */
COLORREF _DIBDRV_MapColor(DIBDRVPHYSDEV *physDev, COLORREF color)
{
    WORD index;
    RGBQUAD *palColor;
    HPALETTE hPal;
    PALETTEENTRY paletteEntry;
    
    switch(color >> 24)
    {
        case 0x10 : /* DIBINDEX */
            MAYBE(TRACE("DIBINDEX Color is %08x\n", color));
            index =  color & 0xffff;
            if(index >= physDev->physBitmap->colorTableSize)
            {
                WARN("DIBINDEX color %d out of range, color table size is %d\n", index, physDev->physBitmap->colorTableSize);
                return 0;
            }
            palColor = physDev->physBitmap->colorTable + index;
            MAYBE(TRACE("Returning color %08x\n", RGB(palColor->rgbRed, palColor->rgbGreen, palColor->rgbBlue)));
            return RGB(palColor->rgbRed, palColor->rgbGreen, palColor->rgbBlue);
            
        case 0x01: /* PALETTEINDEX */
            MAYBE(TRACE("PALETTEINDEX Color is %08x\n", color));
            index =  color & 0xffff;
            if(!(hPal = GetCurrentObject(physDev->hdc, OBJ_PAL)))
            {
                ERR("Couldn't get palette\n");
                return 0;
            }
            if (!GetPaletteEntries(hPal, index, 1, &paletteEntry))
            {
                WARN("PALETTEINDEX(%x) : index %d is out of bounds, assuming black\n", color, index);
                return 0;
            }
            MAYBE(TRACE("Returning color %08x\n", RGB(paletteEntry.peRed, paletteEntry.peGreen, paletteEntry.peBlue)));
            return RGB(paletteEntry.peRed, paletteEntry.peGreen, paletteEntry.peBlue);
        
        case 0x02: /* PALETTERGB */
            return _DIBDRV_GetNearestColor(physDev->physBitmap, color & 0xffffff);
        
        default:
            /* RGB color -- we must process special case for monochrome bitmaps */
            if(physDev->physBitmap->bitCount == 1)
            {
                RGBQUAD *back = physDev->physBitmap->colorTable;
                RGBQUAD *fore = back+1;
                COLORREF lightColorref, darkColorref;
                
                /* lightest color is considered to be 'foreground' one, i.e. associated to white color */
                if(physDev->physBitmap->lightColor == 1)
                {
                    darkColorref = RGB(back->rgbRed, back->rgbGreen, back->rgbBlue);
                    lightColorref = RGB(fore->rgbRed, fore->rgbGreen, fore->rgbBlue);
                }
                else
                {
                    darkColorref = RGB(fore->rgbRed, fore->rgbGreen, fore->rgbBlue);
                    lightColorref = RGB(back->rgbRed, back->rgbGreen, back->rgbBlue);
                }
                
                /* tested on Windows XP -- if present in colortable, maps to corresponding color
                   if not, if white maps to the lightest color, otherwise darkest one. */
                if(color == lightColorref || color == darkColorref)
                    return color;
                else if (color == 0x00ffffff)
                    return lightColorref;
                else
                    return darkColorref;
            }
            else
                return color;
    }
}

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
	TRACE("hasDIB:%d, physBitmap:%p\n", (int) physDev->hasDIB, physDev->physBitmap);
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        /* HACK - we can't get the dib color table during SelectBitmap since it hasn't
           been initialized yet.  This is called from DC_InitDC so it's a convenient place
           to grab the color table. */
        MAYBE(TRACE("Color table size = %d, Color table = %p\n", physDev->physBitmap->colorTableSize, physDev->physBitmap->colorTable));
        if(!physDev->physBitmap->colorTableGrabbed)
        {
            MAYBE(TRACE("Grabbing palette\n"));
            physDev->physBitmap->colorTable = HeapAlloc(GetProcessHeap(), 0, sizeof(physDev->physBitmap->colorTable[0]) * physDev->physBitmap->colorTableSize);
            GetDIBColorTable(physDev->hdc, 0, physDev->physBitmap->colorTableSize, physDev->physBitmap->colorTable);
#ifdef DIBDRV_ENABLE_MAYBE 
           for(i = 0; i < physDev->physBitmap->colorTableSize; i++)
            {
                q = physDev->physBitmap->colorTable + i;
                TRACE("  %03d : R%03d G%03d B%03d\n", i, q->rgbRed, q->rgbGreen, q->rgbBlue);
            }
#endif
            physDev->physBitmap->colorTableGrabbed = TRUE;

            /* for monochrome bitmaps, we need the 'lightest' color */
            _DIBDRVBITMAP_GetLightestColorIndex(physDev->physBitmap);
    
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

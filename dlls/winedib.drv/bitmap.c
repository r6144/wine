/*
 * DIB driver bitmap objects
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


/****************************************************************************
 *       SelectBitmap   (WINEDIB.DRV.@)
 */
HBITMAP DIBDRV_SelectBitmap( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap )
{
    DIBSECTION dibSection;
    HBITMAP res;
    
    /* try to get the DIBSECTION data from the bitmap */
    if(GetObjectW(hbitmap, sizeof(DIBSECTION), &dibSection) == sizeof(BITMAP))
    {
        /* not a DIB section, sets it on physDev and use X11 behaviour */

        MAYBE(TRACE("physDev:%p, hbitmap:%p\n", physDev, hbitmap));
        physDev->hasDIB = FALSE;
        res = _DIBDRV_GetDisplayDriver()->pSelectBitmap(physDev->X11PhysDev, hbitmap);
        if(res)
            physDev->hbitmap = hbitmap;
    }
    else
    {
        /* it's a DIB section, sets it on physDev and use DIB Engine behaviour */
        
        MAYBE(TRACE("physDev:%p, hbitmap:%p, physBitmap=%p\n", physDev, hbitmap, &physDev->physBitmap));

        /* frees any previously physical bitmap */
        _DIBDRVBITMAP_Free(&physDev->physBitmap);

        /* WARNING : the color table can't be grabbed here, since it's still
           not initialized. It'll be grabbed on RealizeDefaultPalette(),
           which is presumably the first call made after palette initialization.
           So, by now we just set up palette size and leave NULL the palette pointer */
        if(_DIBDRVBITMAP_InitFromBMIH(&physDev->physBitmap, &dibSection.dsBmih, dibSection.dsBitfields, NULL, dibSection.dsBm.bmBits))
        {
            /* stores the active bitmap */
            res = physDev->hbitmap;
            physDev->hbitmap = hbitmap;
            
            /* remember there's a DIB selected in */
            physDev->hasDIB = TRUE;
        }
        else
        {
            ERR("Failed to initialize physical bitmap\n");
            res = 0;
        }
    }
    return res;
    
}

/****************************************************************************
 *        DIBDRV_CreateBitmap
 */
BOOL DIBDRV_CreateBitmap( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, LPVOID bmBits )
{
    DIBSECTION dibSection;
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, hbitmap:%p, bmBits:%p\n", physDev, hbitmap, bmBits));

    /* try to get the DIBSECTION data from the bitmap */
    if(GetObjectW(hbitmap, sizeof(DIBSECTION), &dibSection) == sizeof(BITMAP))
    {
        /* not a DIB section, use X11 behaviour */
        res = _DIBDRV_GetDisplayDriver()->pCreateBitmap(physDev->X11PhysDev, hbitmap, bmBits);
    }
    else
    {
        /* it's a DIB section, use DIB Engine behaviour - should not happen, but.... */
        ONCE(FIXME("CreateBitmap() called for a DIB section - shouldn't happen\n"));
        res = TRUE;
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_DeleteBitmap
 */
BOOL DIBDRV_DeleteBitmap( HBITMAP hbitmap )
{
    DIBSECTION dibSection;
    BOOL res;
    
    MAYBE(TRACE("hbitmap:%p\n", hbitmap));

    /* try to get the DIBSECTION data from the bitmap */
    if(GetObjectW(hbitmap, sizeof(DIBSECTION), &dibSection) == sizeof(BITMAP))
    {
        /* not a DIB section, use X11 behaviour */
        res = _DIBDRV_GetDisplayDriver()->pDeleteBitmap(hbitmap);
    }
    else
    {
        /* it's a DIB section, use DIB Engine behaviour */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetBitmapBits
 */
LONG DIBDRV_GetBitmapBits( HBITMAP hbitmap, void *buffer, LONG count )
{
    LONG res;
    
    MAYBE(TRACE("hbitmap:%p, buffer:%p, count:%d\n", hbitmap, buffer, count));

    /* GetBitmapBits is only valid for DDBs, so use X11 driver */
    res =  _DIBDRV_GetDisplayDriver()->pGetBitmapBits(hbitmap, buffer, count);
    
    return res;
}

/******************************************************************************
 *             DIBDRV_SetBitmapBits
 */
LONG DIBDRV_SetBitmapBits( HBITMAP hbitmap, const void *bits, LONG count )
{
    LONG res;
    
    MAYBE(TRACE("hbitmap:%p, bits:%p, count:%d\n", hbitmap, bits, count));

    /* SetBitmapBits is only valid for DDBs, so use X11 driver */
    res = _DIBDRV_GetDisplayDriver()->pSetBitmapBits(hbitmap, bits, count);

    return res;
}

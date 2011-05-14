/*
 * DIBDRV device-independent bitmaps
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
 *           DIBDRV_CreateDIBSection
 */
HBITMAP DIBDRV_CreateDIBSection( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap,
                                 const BITMAPINFO *bmi, UINT usage )
{
    HBITMAP res;
    
    MAYBE(TRACE("physDev:%p, hbitmap:%p, bmi:%p, usage:%d\n", physDev, hbitmap, bmi, usage));

    /* createDIBSection is only DIB-related, so we just use the engine */
    ONCE(FIXME("STUB\n"));
    res = hbitmap;

    return res;
}

/***********************************************************************
 *           DIBDRV_GetDIBits
*/
INT DIBDRV_GetDIBits( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, UINT startscan,
                      UINT lines, LPCVOID bits, const BITMAPINFO *bmi, UINT coloruse )
{
    INT res;
    
    MAYBE(TRACE("physDev:%p, hbitmap:%p, startscan:%d, lines:%d, bits:%p, bmi:%p, coloruse:%d\n",
        physDev, hbitmap, startscan, lines, bits, bmi, coloruse));

    /* GetDIBits reads bits from a DDB, so we should use the X11 driver */
    res = _DIBDRV_GetDisplayDriver()->pGetDIBits(physDev->X11PhysDev, hbitmap, startscan, lines, bits, bmi, coloruse);
    
    return res;
}

/***********************************************************************
 *           DIBDRV_SetDIBColorTable
 */
UINT DIBDRV_SetDIBColorTable( DIBDRVPHYSDEV *physDev, UINT start, UINT count,
                              const RGBQUAD *colors )
{
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    
    MAYBE(TRACE("physDev:%p, start:%d, count:%d, colors:%p\n", physDev, start, count, colors));

    /* SetDIBColorTable operates on a DIB, so we use the engine */
    
    /* if bpp > 8, some error occurred... */
    if(dib->bitCount > 8)
    {
        ERR("Called for BPP > 8\n");
        return 0;
    }
    
    /* if dib hasn't a color table, or has a small one, we must before
       create/extend it */
    if(!(dib->colorTable))
    {
        dib->colorTableSize = (1 << dib->bitCount);
        dib->colorTable = HeapAlloc(GetProcessHeap(), 0, sizeof(RGBQUAD) * dib->colorTableSize);
    }
    else if(dib->colorTableSize < (1 << dib->bitCount))
    {
        int newSize = (1 << dib->bitCount);
        RGBQUAD *newTable = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RGBQUAD) * newSize);
        memcpy(newTable, dib->colorTable, sizeof(RGBQUAD) * dib->colorTableSize);
        HeapFree(GetProcessHeap(), 0, dib->colorTable);
        dib->colorTable = newTable;
        dib->colorTableSize = newSize;
    }
    
    /* sanity check */
    if(start + count > dib->colorTableSize)
    {
        ERR("Out of range setting color table, size is %d, requested is %d\n", dib->colorTableSize, start+count);
        return 0;
    }
    memcpy(dib->colorTable + start, colors, sizeof(RGBQUAD) * count);
    dib->colorTableGrabbed = TRUE;

    return TRUE;
}

/***********************************************************************
 *           DIBDRV_SetDIBits
 */
INT DIBDRV_SetDIBits( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, UINT startscan,
                      UINT lines, LPCVOID bits, const BITMAPINFO *info, UINT coloruse )
{
    INT res;
    
    MAYBE(TRACE("physDev:%p, hbitmap:%p, startscan:%d, lines:%d, bits:%p, bmi:%p, coloruse:%d\n",
        physDev, hbitmap, startscan, lines, bits, info, coloruse));

    /* SetDIBits writes bits to a DDB, so we should use the X11 driver */
    res =  _DIBDRV_GetDisplayDriver()->pSetDIBits(physDev->X11PhysDev, hbitmap, startscan, lines, bits, info, coloruse);

    return res;
}

/*************************************************************************
 *              DIBDRV_SetDIBitsToDevice
 */
INT DIBDRV_SetDIBitsToDevice( DIBDRVPHYSDEV *physDev, INT xDest, INT yDest, DWORD cx,
                              DWORD cy, INT xSrc, INT ySrc,
                              UINT startscan, UINT lines, LPCVOID bits,
                              const BITMAPINFO *info, UINT coloruse )
{
    INT res;
    
    MAYBE(TRACE("physDev:%p, xDest:%d, yDest:%d, cx:%x, cy:%x, xSrc:%d, ySrc:%d, startscan:%d, lines:%d, bits:%p, info:%p, coloruse:%d\n",
        physDev, xDest, yDest, cx, cy, xSrc, ySrc, startscan, lines, bits, info, coloruse));

    /* SetDIBitsToDevice operates on a physical device, so we should use the X11 driver */
    res = _DIBDRV_GetDisplayDriver()->pSetDIBitsToDevice(physDev->X11PhysDev, xDest, yDest, cx, cy, xSrc, ySrc,
                                                          startscan, lines, bits, info, coloruse);

    return res;
}

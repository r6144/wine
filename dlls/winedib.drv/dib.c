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
    TRACE("physDev:%p, hbitmap:%p, bmi:%p, usage:%d\n", physDev, hbitmap, bmi, usage);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pCreateDIBSection(physDev->X11PhysDev, hbitmap, bmi, usage);
}

/***********************************************************************
 *           DIBDRV_GetDIBits
*/
INT DIBDRV_GetDIBits( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, UINT startscan,
                      UINT lines, LPCVOID bits, const BITMAPINFO *bmi, UINT coloruse )
{
    TRACE("physDev:%p, hbitmap:%p, startscan:%d, lines:%d, bits:%p, bmi:%p, coloruse:%d\n",
        physDev, hbitmap, startscan, lines, bits, bmi, coloruse);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetDIBits(physDev->X11PhysDev, hbitmap, startscan, lines, bits, bmi, coloruse);
}

/***********************************************************************
 *           DIBDRV_SetDIBColorTable
 */
UINT DIBDRV_SetDIBColorTable( DIBDRVPHYSDEV *physDev, UINT start, UINT count,
                              const RGBQUAD *colors )
{
    TRACE("physDev:%p, start:%d, count:%d, colors:%p\n", physDev, start, count, colors);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetDIBColorTable(physDev->X11PhysDev, start, count, colors);
}

/***********************************************************************
 *           DIBDRV_SetDIBits
 */
INT DIBDRV_SetDIBits( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, UINT startscan,
                      UINT lines, LPCVOID bits, const BITMAPINFO *info, UINT coloruse )
{
    TRACE("physDev:%p, hbitmap:%p, startscan:%d, lines:%d, bits:%p, bmi:%p, coloruse:%d\n",
        physDev, hbitmap, startscan, lines, bits, info, coloruse);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetDIBits(physDev->X11PhysDev, hbitmap, startscan, lines, bits, info, coloruse);
}

/*************************************************************************
 *              DIBDRV_SetDIBitsToDevice
 */
INT DIBDRV_SetDIBitsToDevice( DIBDRVPHYSDEV *physDev, INT xDest, INT yDest, DWORD cx,
                              DWORD cy, INT xSrc, INT ySrc,
                              UINT startscan, UINT lines, LPCVOID bits,
                              const BITMAPINFO *info, UINT coloruse )
{
    TRACE("physDev:%p, xDest:%d, yDest:%d, cx:%x, cy:%x, xSrc:%d, ySrc:%d, startscan:%d, lines:%d, bits:%p, info:%p, coloruse:%d\n",
        physDev, xDest, yDest, cx, cy, xSrc, ySrc, startscan, lines, bits, info, coloruse);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetDIBitsToDevice(physDev->X11PhysDev, xDest, yDest, cx, cy, xSrc, ySrc,
                                                          startscan, lines, bits, info, coloruse);
}

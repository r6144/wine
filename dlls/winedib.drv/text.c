/*
 * DIBDRV text functions
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
 *           DIBDRV_ExtTextOut
 */
BOOL DIBDRV_ExtTextOut( DIBDRVPHYSDEV *physDev, INT x, INT y, UINT flags,
                        const RECT *lprect, LPCWSTR wstr, UINT count,
                        const INT *lpDx )
{
    BOOL res;
    
    TRACE("physDev:%p, x:%d, y:%d, flags:%x, lprect:%p, wstr:%s, count:%d, lpDx:%p\n",
          physDev, x, y, flags, lprect, debugstr_w(wstr), count, lpDx);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pExtTextOut(physDev->X11PhysDev, x, y, flags, lprect,
                                                   wstr, count, lpDx);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pExtTextOut(physDev->X11PhysDev, x, y, flags, lprect,
                                                   wstr, count, lpDx);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetTextExtentExPoint
 */
BOOL DIBDRV_GetTextExtentExPoint( DIBDRVPHYSDEV *physDev, LPCWSTR str, INT count,
                                  INT maxExt, LPINT lpnFit, LPINT alpDx, LPSIZE size )
{
    BOOL res;
    
    TRACE("physDev:%p, str:%s, count:%d, maxExt:%d, lpnFit:%p, alpDx:%p, size:%p\n",
        physDev, debugstr_w(str), count, maxExt, lpnFit, alpDx, size);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetTextExtentExPoint(physDev->X11PhysDev, str, count, maxExt,
                                                                lpnFit, alpDx, size);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetTextExtentExPoint(physDev->X11PhysDev, str, count, maxExt,
                                                                lpnFit, alpDx, size);
    }
    return res;
}

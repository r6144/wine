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
    TRACE("physDev:%p, hpal:%p, primary:%s\n", physDev, hpal, (primary ? "TRUE" : "FALSE"));
    
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pRealizePalette(physDev->X11PhysDev, hpal, primary);
}

/***********************************************************************
 *              DIBDRV_UnrealizePalette
 */
BOOL DIBDRV_UnrealizePalette( HPALETTE hpal )
{
    TRACE("hpal:%p\n", hpal);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pUnrealizePalette(hpal);
}

/***********************************************************************
 *              DIBDRV_GetSystemPaletteEntries
 */
UINT DIBDRV_GetSystemPaletteEntries( DIBDRVPHYSDEV *physDev, UINT start, UINT count,
                                     LPPALETTEENTRY entries )
{
    TRACE("physDev:%p, start:%d, count:%d, entries:%p\n", physDev, start, count, entries);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetSystemPaletteEntries(physDev->X11PhysDev, start, count, entries);
}

/***********************************************************************
 *              DIBDRV_GetNearestColor
 */
COLORREF DIBDRV_GetNearestColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    TRACE("physDev:%p, color:%x\n", physDev, color);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetNearestColor(physDev->X11PhysDev, color);
}

/***********************************************************************
 *              DIBDRV_RealizeDefaultPalette
 */
UINT DIBDRV_RealizeDefaultPalette( DIBDRVPHYSDEV *physDev )
{
    TRACE("physDev:%p\n", physDev);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pRealizeDefaultPalette(physDev->X11PhysDev);
}

BOOL DIBDRV_GetICMProfile(DIBDRVPHYSDEV *physDev, LPDWORD lpcbName, LPWSTR lpszFilename)
{
    TRACE("physDev:%p, lpcpName:%p, lpszFilename:%p\n", physDev, lpcbName, lpszFilename);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetICMProfile(physDev->X11PhysDev, lpcbName, lpszFilename);
}

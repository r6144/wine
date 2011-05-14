/*
 * DIBDRV pen objects
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
 *           DIBDRV_SelectPen
 */
HPEN DIBDRV_SelectPen( DIBDRVPHYSDEV *physDev, HPEN hpen )
{
    TRACE("physDev:%p, hpen:%p\n", physDev, hpen);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSelectPen(physDev->X11PhysDev, hpen);
}

/***********************************************************************
 *           DIBDRV_SetDCPenColor
 */
COLORREF DIBDRV_SetDCPenColor( DIBDRVPHYSDEV *physDev, COLORREF crColor )
{
    TRACE("physDev:%p, crColor:%x\n", physDev, crColor);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetDCPenColor(physDev->X11PhysDev, crColor);
}

/***********************************************************************
 *           DIBDRV_SelectBrush
 */
HBRUSH DIBDRV_SelectBrush( DIBDRVPHYSDEV *physDev, HBRUSH hbrush )
{
    TRACE("physDev:%p, hbrush:%p\n", physDev, hbrush);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSelectBrush(physDev->X11PhysDev, hbrush);
}

/***********************************************************************
 *           DIBDRV_SetDCBrushColor
 */
COLORREF DIBDRV_SetDCBrushColor( DIBDRVPHYSDEV *physDev, COLORREF crColor )
{
    TRACE("physDev:%p, crColor:%x\n", physDev, crColor);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetDCBrushColor(physDev->X11PhysDev, crColor);
}

/***********************************************************************
 *           SetROP2
 */
INT DIBDRV_SetROP2( DIBDRVPHYSDEV *physDev, INT rop )
{
    INT prevRop;
    
    TRACE("physDev:%p, rop:%x\n", physDev, rop);

    ONCE(FIXME("stub\n"));
    prevRop = physDev->rop2;
    physDev->rop2 = rop;
    return prevRop;
    /* note : X11 Driver don't have SetROP2() function exported */
}

/***********************************************************************
 *           SetBkColor
 */
COLORREF DIBDRV_SetBkColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    TRACE("physDev:%p, color:%x\n", physDev, color);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetBkColor(physDev->X11PhysDev, color);
}

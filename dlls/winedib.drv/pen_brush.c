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
    HPEN res;
    
    TRACE("physDev:%p, hpen:%p\n", physDev, hpen);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSelectPen(physDev->X11PhysDev, hpen);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSelectPen(physDev->X11PhysDev, hpen);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SetDCPenColor
 */
COLORREF DIBDRV_SetDCPenColor( DIBDRVPHYSDEV *physDev, COLORREF crColor )
{
    COLORREF res;
    
    TRACE("physDev:%p, crColor:%x\n", physDev, crColor);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetDCPenColor(physDev->X11PhysDev, crColor);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetDCPenColor(physDev->X11PhysDev, crColor);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SelectBrush
 */
HBRUSH DIBDRV_SelectBrush( DIBDRVPHYSDEV *physDev, HBRUSH hbrush )
{
    HBRUSH res;
    
    TRACE("physDev:%p, hbrush:%p\n", physDev, hbrush);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSelectBrush(physDev->X11PhysDev, hbrush);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSelectBrush(physDev->X11PhysDev, hbrush);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SetDCBrushColor
 */
COLORREF DIBDRV_SetDCBrushColor( DIBDRVPHYSDEV *physDev, COLORREF crColor )
{
    COLORREF res;
    
    TRACE("physDev:%p, crColor:%x\n", physDev, crColor);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetDCBrushColor(physDev->X11PhysDev, crColor);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetDCBrushColor(physDev->X11PhysDev, crColor);
    }
    return res;
}

/***********************************************************************
 *           SetROP2
 */
INT DIBDRV_SetROP2( DIBDRVPHYSDEV *physDev, INT rop )
{
    INT prevRop;
    
    TRACE("physDev:%p, rop:%x\n", physDev, rop);

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
    COLORREF res;
    
    TRACE("physDev:%p, color:%x\n", physDev, color);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetBkColor(physDev->X11PhysDev, color);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetBkColor(physDev->X11PhysDev, color);
    }
    return res;
}

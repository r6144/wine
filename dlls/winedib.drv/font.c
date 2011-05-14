/*
 * DIBDRV font objects
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
 *
 */

#include "config.h"
#include "wine/port.h"

#include "dibdrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/**********************************************************************
 *          DIBDRV_SetTextColor
 */
COLORREF DIBDRV_SetTextColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    TRACE("physDev:%p, color:%08x\n", physDev, color);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetTextColor(physDev->X11PhysDev, color);
}

/***********************************************************************
 *           DIBDRV_SelectFont
 */
HFONT DIBDRV_SelectFont( DIBDRVPHYSDEV *physDev, HFONT hfont, HANDLE gdiFont )
{
    TRACE("physDev:%p, hfont:%p, gdiFont:%p\n", physDev, hfont, gdiFont);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSelectFont(physDev->X11PhysDev, hfont, gdiFont);
}

/***********************************************************************
 *           DIBDRV_EnumDeviceFonts
 */
BOOL DIBDRV_EnumDeviceFonts( DIBDRVPHYSDEV *physDev, LPLOGFONTW plf,
                             FONTENUMPROCW proc, LPARAM lp )
{
    TRACE("physDev:%p, plf:%p, proc:%p, lp:%lx\n", physDev, plf, proc, lp);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pEnumDeviceFonts(physDev->X11PhysDev, plf, proc, lp);
}

/***********************************************************************
 *           DIBDRV_GetTextMetrics
 */
BOOL DIBDRV_GetTextMetrics( DIBDRVPHYSDEV *physDev, TEXTMETRICW *metrics )
{
    TRACE("physDev:%p, metrics:%p\n", physDev, metrics);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetTextMetrics(physDev->X11PhysDev, metrics);
}

/***********************************************************************
 *           DIBDRV_GetCharWidth
 */
BOOL DIBDRV_GetCharWidth( DIBDRVPHYSDEV *physDev, UINT firstChar, UINT lastChar,
                          LPINT buffer )
{
    TRACE("physDev:%p, firstChar:%d, lastChar:%d, buffer:%pn", physDev, firstChar, lastChar, buffer);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetCharWidth(physDev->X11PhysDev, firstChar, lastChar, buffer);
}

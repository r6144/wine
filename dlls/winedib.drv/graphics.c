/*
 * DIBDRV implementation of GDI driver graphics functions
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
 *           DIBDRV_Arc
 */
BOOL DIBDRV_Arc( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom,
            INT xstart, INT ystart, INT xend, INT yend )
{
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pArc(physDev->X11PhysDev, left, top, right, bottom,
                                            xstart, ystart, xend, yend);
}

/***********************************************************************
 *           DIBDRV_Chord
 */
BOOL DIBDRV_Chord( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom,
              INT xstart, INT ystart, INT xend, INT yend )
{
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pChord(physDev->X11PhysDev, left, top, right, bottom,
                                              xstart, ystart, xend, yend);
}

/***********************************************************************
 *           DIBDRV_Ellipse
 */
BOOL DIBDRV_Ellipse( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom )
{
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n",
          physDev, left, top, right, bottom);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pEllipse(physDev->X11PhysDev, left, top, right, bottom);
}

/**********************************************************************
 *          DIBDRV_ExtFloodFill
 */
BOOL DIBDRV_ExtFloodFill( DIBDRVPHYSDEV *physDev, INT x, INT y, COLORREF color,
                     UINT fillType )
{
    TRACE("physDev:%p, x:%d, y:%d, color:%x, fillType:%d\n",
          physDev, x, y, color, fillType);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pExtFloodFill(physDev->X11PhysDev, x, y, color, fillType);
}

/***********************************************************************
 *           DIBDRV_GetDCOrgEx
 */
BOOL DIBDRV_GetDCOrgEx( DIBDRVPHYSDEV *physDev, LPPOINT lpp )
{
    TRACE("physDev:%p, lpp:%p\n", physDev, lpp);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetDCOrgEx(physDev->X11PhysDev, lpp);
}

/***********************************************************************
 *           DIBDRV_GetPixel
 */
COLORREF DIBDRV_GetPixel( DIBDRVPHYSDEV *physDev, INT x, INT y )
{
    TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetPixel(physDev->X11PhysDev, x, y);
}

/***********************************************************************
 *           DIBDRV_LineTo
 */
BOOL DIBDRV_LineTo( DIBDRVPHYSDEV *physDev, INT x, INT y )
{
    TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pLineTo(physDev->X11PhysDev, x, y);
}

/***********************************************************************
 *           DIBDRV_PaintRgn
 */
BOOL DIBDRV_PaintRgn( DIBDRVPHYSDEV *physDev, HRGN hrgn )
{
    TRACE("physDev:%p, hrgn:%p\n", physDev, hrgn);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPaintRgn(physDev->X11PhysDev, hrgn);
}

/***********************************************************************
 *           DIBDRV_Pie
 */
BOOL DIBDRV_Pie( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom,
            INT xstart, INT ystart, INT xend, INT yend )
{
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPie(physDev->X11PhysDev, left, top, right, bottom,
                                            xstart, ystart, xend, yend);
}

/**********************************************************************
 *          DIBDRV_Polygon
 */
BOOL DIBDRV_Polygon( DIBDRVPHYSDEV *physDev, const POINT* pt, INT count )
{
    TRACE("physDev:%p, pt:%p, count:%d\n", physDev, pt, count);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPolygon(physDev->X11PhysDev, pt, count);
}

/**********************************************************************
 *          DIBDRV_Polyline
 */
BOOL DIBDRV_Polyline( DIBDRVPHYSDEV *physDev, const POINT* pt, INT count )
{
    TRACE("physDev:%p, pt:%p, count:%d\n", physDev, pt, count);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPolyline(physDev->X11PhysDev, pt, count);
}

/**********************************************************************
 *          DIBDRV_PolyPolygon
 */
BOOL DIBDRV_PolyPolygon( DIBDRVPHYSDEV *physDev, const POINT* pt, const INT* counts, UINT polygons)
{
    TRACE("physDev:%p, pt:%p, counts:%p, polygons:%d\n", physDev, pt, counts, polygons);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPolyPolygon(physDev->X11PhysDev, pt, counts, polygons);
}

/**********************************************************************
 *          DIBDRV_PolyPolyline
 */
BOOL DIBDRV_PolyPolyline( DIBDRVPHYSDEV *physDev, const POINT* pt, const DWORD* counts,
                     DWORD polylines )
{
    TRACE("physDev:%p, pt:%p, counts:%p, polylines:%d\n", physDev, pt, counts, polylines);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPolyPolyline(physDev->X11PhysDev, pt, counts, polylines);
}

/***********************************************************************
 *           DIBDRV_Rectangle
 */
BOOL DIBDRV_Rectangle( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom)
{
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n",
          physDev, left, top, right, bottom);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pRectangle(physDev->X11PhysDev, left, top, right, bottom);
}

/***********************************************************************
 *           DIBDRV_RoundRect
 */
BOOL DIBDRV_RoundRect( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right,
                  INT bottom, INT ell_width, INT ell_height )
{
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, ell_width:%d, ell_height:%d\n",
          physDev, left, top, right, bottom, ell_width, ell_height);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pRoundRect(physDev->X11PhysDev, left, top, right, bottom,
                                                  ell_width, ell_height);
}

/***********************************************************************
 *           DIBDRV_SetPixel
 */
COLORREF DIBDRV_SetPixel( DIBDRVPHYSDEV *physDev, INT x, INT y, COLORREF color )
{
    TRACE("physDev:%p, x:%d, y:%d, color:%x\n", physDev, x, y, color);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetPixel(physDev->X11PhysDev, x, y, color);
}

/***********************************************************************
 *           DIBDRV_SetDCOrg
 */
DWORD DIBDRV_SetDCOrg( DIBDRVPHYSDEV *physDev, INT x, INT y )
{
    TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetDCOrg(physDev->X11PhysDev, x, y);
}

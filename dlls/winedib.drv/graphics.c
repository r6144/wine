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
    BOOL res;
    
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pArc(physDev->X11PhysDev, left, top, right, bottom,
                                               xstart, ystart, xend, yend);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pArc(physDev->X11PhysDev, left, top, right, bottom,
                                               xstart, ystart, xend, yend);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_Chord
 */
BOOL DIBDRV_Chord( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom,
              INT xstart, INT ystart, INT xend, INT yend )
{
    BOOL res;
    
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pChord(physDev->X11PhysDev, left, top, right, bottom,
                                                 xstart, ystart, xend, yend);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pChord(physDev->X11PhysDev, left, top, right, bottom,
                                                 xstart, ystart, xend, yend);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_Ellipse
 */
BOOL DIBDRV_Ellipse( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom )
{
    BOOL res;
    
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n",
          physDev, left, top, right, bottom);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pEllipse(physDev->X11PhysDev, left, top, right, bottom);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pEllipse(physDev->X11PhysDev, left, top, right, bottom);
    }
    return res;
}

/**********************************************************************
 *          DIBDRV_ExtFloodFill
 */
BOOL DIBDRV_ExtFloodFill( DIBDRVPHYSDEV *physDev, INT x, INT y, COLORREF color,
                     UINT fillType )
{
    BOOL res;
    
    TRACE("physDev:%p, x:%d, y:%d, color:%x, fillType:%d\n",
          physDev, x, y, color, fillType);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pExtFloodFill(physDev->X11PhysDev, x, y, color, fillType);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pExtFloodFill(physDev->X11PhysDev, x, y, color, fillType);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetDCOrgEx
 */
BOOL DIBDRV_GetDCOrgEx( DIBDRVPHYSDEV *physDev, LPPOINT lpp )
{
    BOOL res;
    
    TRACE("physDev:%p, lpp:%p\n", physDev, lpp);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetDCOrgEx(physDev->X11PhysDev, lpp);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetDCOrgEx(physDev->X11PhysDev, lpp);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetPixel
 */
COLORREF DIBDRV_GetPixel( DIBDRVPHYSDEV *physDev, INT x, INT y )
{
    COLORREF res;
    
    TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetPixel(physDev->X11PhysDev, x, y);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetPixel(physDev->X11PhysDev, x, y);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_LineTo
 */
BOOL DIBDRV_LineTo( DIBDRVPHYSDEV *physDev, INT x, INT y )
{
    BOOL res;
    
    TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pLineTo(physDev->X11PhysDev, x, y);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pLineTo(physDev->X11PhysDev, x, y);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_PaintRgn
 */
BOOL DIBDRV_PaintRgn( DIBDRVPHYSDEV *physDev, HRGN hrgn )
{
    BOOL res;
    
    TRACE("physDev:%p, hrgn:%p\n", physDev, hrgn);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pPaintRgn(physDev->X11PhysDev, hrgn);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPaintRgn(physDev->X11PhysDev, hrgn);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_Pie
 */
BOOL DIBDRV_Pie( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom,
            INT xstart, INT ystart, INT xend, INT yend )
{
    BOOL res;
    
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pPie(physDev->X11PhysDev, left, top, right, bottom,
                                               xstart, ystart, xend, yend);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPie(physDev->X11PhysDev, left, top, right, bottom,
                                               xstart, ystart, xend, yend);
    }
    return res;
}

/**********************************************************************
 *          DIBDRV_Polygon
 */
BOOL DIBDRV_Polygon( DIBDRVPHYSDEV *physDev, const POINT* pt, INT count )
{
    BOOL res;
    
    TRACE("physDev:%p, pt:%p, count:%d\n", physDev, pt, count);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pPolygon(physDev->X11PhysDev, pt, count);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPolygon(physDev->X11PhysDev, pt, count);
    }
    return res;
}

/**********************************************************************
 *          DIBDRV_Polyline
 */
BOOL DIBDRV_Polyline( DIBDRVPHYSDEV *physDev, const POINT* pt, INT count )
{
    BOOL res;
    
    TRACE("physDev:%p, pt:%p, count:%d\n", physDev, pt, count);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pPolyline(physDev->X11PhysDev, pt, count);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPolyline(physDev->X11PhysDev, pt, count);
    }
    return res;
}

/**********************************************************************
 *          DIBDRV_PolyPolygon
 */
BOOL DIBDRV_PolyPolygon( DIBDRVPHYSDEV *physDev, const POINT* pt, const INT* counts, UINT polygons)
{
    BOOL res;
    
    TRACE("physDev:%p, pt:%p, counts:%p, polygons:%d\n", physDev, pt, counts, polygons);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pPolyPolygon(physDev->X11PhysDev, pt, counts, polygons);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPolyPolygon(physDev->X11PhysDev, pt, counts, polygons);
    }
    return res;
}

/**********************************************************************
 *          DIBDRV_PolyPolyline
 */
BOOL DIBDRV_PolyPolyline( DIBDRVPHYSDEV *physDev, const POINT* pt, const DWORD* counts,
                     DWORD polylines )
{
    BOOL res;
    
    TRACE("physDev:%p, pt:%p, counts:%p, polylines:%d\n", physDev, pt, counts, polylines);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pPolyPolyline(physDev->X11PhysDev, pt, counts, polylines);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPolyPolyline(physDev->X11PhysDev, pt, counts, polylines);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_Rectangle
 */
BOOL DIBDRV_Rectangle( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right, INT bottom)
{
    BOOL res;
    
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n",
          physDev, left, top, right, bottom);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pRectangle(physDev->X11PhysDev, left, top, right, bottom);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pRectangle(physDev->X11PhysDev, left, top, right, bottom);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_RoundRect
 */
BOOL DIBDRV_RoundRect( DIBDRVPHYSDEV *physDev, INT left, INT top, INT right,
                  INT bottom, INT ell_width, INT ell_height )
{
    BOOL res;
    
    TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, ell_width:%d, ell_height:%d\n",
          physDev, left, top, right, bottom, ell_width, ell_height);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pRoundRect(physDev->X11PhysDev, left, top, right, bottom,
                                                     ell_width, ell_height);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pRoundRect(physDev->X11PhysDev, left, top, right, bottom,
                                                     ell_width, ell_height);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SetPixel
 */
COLORREF DIBDRV_SetPixel( DIBDRVPHYSDEV *physDev, INT x, INT y, COLORREF color )
{
    COLORREF res;
    
    TRACE("physDev:%p, x:%d, y:%d, color:%x\n", physDev, x, y, color);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetPixel(physDev->X11PhysDev, x, y, color);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetPixel(physDev->X11PhysDev, x, y, color);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SetDCOrg
 */
DWORD DIBDRV_SetDCOrg( DIBDRVPHYSDEV *physDev, INT x, INT y )
{
    DWORD res;
    
    TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pSetDCOrg(physDev->X11PhysDev, x, y);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetDCOrg(physDev->X11PhysDev, x, y);
    }
    return res;
}

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

static inline void OrderInt(int *i1, int *i2)
{
    if(*i1 > *i2)
    {
        int tmp;
        tmp = *i1;
        *i1 = *i2;
        *i2 = tmp;
    }
}

/***********************************************************************
 *           DIBDRV_Arc
 */
BOOL DIBDRV_Arc( DIBDRVPHYSDEV *physDev, int left, int top, int right, int bottom,
            int xstart, int ystart, int xend, int yend )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_Chord( DIBDRVPHYSDEV *physDev, int left, int top, int right, int bottom,
              int xstart, int ystart, int xend, int yend )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_Ellipse( DIBDRVPHYSDEV *physDev, int left, int top, int right, int bottom )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n",
          physDev, left, top, right, bottom));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_ExtFloodFill( DIBDRVPHYSDEV *physDev, int x, int y, COLORREF color,
                     UINT fillType )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d, color:%x, fillType:%d\n",
          physDev, x, y, color, fillType));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
    
    MAYBE(TRACE("physDev:%p, lpp:%p\n", physDev, lpp));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
COLORREF DIBDRV_GetPixel( DIBDRVPHYSDEV *physDev, int x, int y )
{
    COLORREF res;
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y));

    if(physDev->hasDIB)
    {
        res = physDev->physBitmap.funcs->GetPixel(&physDev->physBitmap, x, y);
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
BOOL DIBDRV_LineTo( DIBDRVPHYSDEV *physDev, int x, int y )
{
    BOOL res;
    POINT curPos;
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y));

    if(physDev->hasDIB)
    {
        GetCurrentPositionEx(physDev->hdc, &curPos);

        _DIBDRV_ResetDashOrigin(physDev);

        if(curPos.y == y)
            physDev->penHLine(physDev, curPos.x, x, y);
        else if(curPos.x == x)
            physDev->penVLine(physDev, x, curPos.y, y);
        else
            physDev->penLine(physDev, curPos.x, curPos.y, x, y);
        res = TRUE;
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
BOOL DIBDRV_Rectangle( DIBDRVPHYSDEV *physDev, int left, int top, int right, int bottom);
BOOL DIBDRV_PaintRgn( DIBDRVPHYSDEV *physDev, HRGN hrgn )
{
    BOOL res = FALSE;
    int i;
    RECT *rect;
    RGNDATA *data;
    int size;
    
    MAYBE(TRACE("physDev:%p, hrgn:%p\n", physDev, hrgn));

    if(physDev->hasDIB)
    {
        /* gets needed region data size */
        if(!(size = GetRegionData(hrgn, 0, NULL)))
            goto fin;
        
        /* allocates buffer and gets actual region data */
        if(!(data = HeapAlloc(GetProcessHeap(), 0, size)))
            goto fin;
        if(!GetRegionData(hrgn, size, data))
        {
            HeapFree(GetProcessHeap(), 0, data);
            goto fin;
        }

        /* paints the filled rectangles */
        rect = (RECT *)data->Buffer;
        for(i = 0; i < data->rdh.nCount; i++)
        {
            DIBDRV_Rectangle( physDev, rect->left, rect->top, rect->right, rect->bottom);
            rect++;
        }
        HeapFree( GetProcessHeap(), 0, data );
        res = TRUE;
fin:
        ;
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
BOOL DIBDRV_Pie( DIBDRVPHYSDEV *physDev, int left, int top, int right, int bottom,
            int xstart, int ystart, int xend, int yend )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, xstart:%d, ystart:%d, xend:%d, yend:%d\n",
          physDev, left, top, right, bottom, xstart, ystart, xend, yend));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_Polygon( DIBDRVPHYSDEV *physDev, const POINT* pt, int count )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, pt:%p, count:%d\n", physDev, pt, count));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_Polyline( DIBDRVPHYSDEV *physDev, const POINT* pt, int count )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, pt:%p, count:%d\n", physDev, pt, count));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_PolyPolygon( DIBDRVPHYSDEV *physDev, const POINT* pt, const int* counts, UINT polygons)
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, pt:%p, counts:%p, polygons:%d\n", physDev, pt, counts, polygons));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
    
    MAYBE(TRACE("physDev:%p, pt:%p, counts:%p, polylines:%d\n", physDev, pt, counts, polylines));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
BOOL DIBDRV_Rectangle( DIBDRVPHYSDEV *physDev, int x1, int y1, int x2, int y2)
{
    BOOL res = TRUE;
    int i;
    DIBDRVBITMAP *bmp = &physDev->physBitmap;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n", physDev, x1, y1, x2, y2));

    if(physDev->hasDIB)
    {
        OrderInt(&x1, &x2);
        OrderInt(&y1, &y2);

        /* temporary fix.... should be done by clipping */
        if(x1 < 0) x1 = 0;
        if(x2 < 0) x2 = 0;
        if(y1 < 0) y1 = 0;
        if(y2 < 0) y2 = 0;
        if(x1 >= bmp->width)  x1 = bmp->width  - 1;
        if(y1 >= bmp->height) y1 = bmp->height - 1;
        if(x2 >  bmp->width)  x2 = bmp->width;
        if(y2 >  bmp->height) y2 = bmp->height ;
        if(x1 >= x2 || y1 >= y2)
            goto fin;

        _DIBDRV_ResetDashOrigin(physDev);
        
        /* particular case where the rectangle
           degenerates to a line or a point */
        if(x1 >= x2 - 1)
        {
            physDev->penVLine(physDev, x1, y1, y2);
            goto fin;
        }
        else if (y1 >= y2 -1)
        {
            physDev->penHLine(physDev, x1, x2, y1);
            goto fin;
        }
        
        /* Draw the perimeter */
        physDev->penHLine(physDev, x1    , x2    , y1   );
        physDev->penHLine(physDev, x1    , x2    , y2 - 1);
        physDev->penVLine(physDev, x1    , y1 + 1, y2 - 1);
        physDev->penVLine(physDev, x2 - 1, y1 + 1, y2 - 1);

        /* fill the inside */
        if(x2 >= x1 + 2)
            for (i = y1 + 1; i < y2 - 1; i++)
                physDev->brushHLine(physDev, x1 + 1, x2 - 1, i);

        res = TRUE;
fin:
        ;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pRectangle(physDev->X11PhysDev, x1, y1, x2, y2);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_RoundRect
 */
BOOL DIBDRV_RoundRect( DIBDRVPHYSDEV *physDev, int left, int top, int right,
                  int bottom, int ell_width, int ell_height )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d, ell_width:%d, ell_height:%d\n",
          physDev, left, top, right, bottom, ell_width, ell_height));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
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
COLORREF DIBDRV_SetPixel( DIBDRVPHYSDEV *physDev, int x, int y, COLORREF color )
{
    COLORREF res;
    DWORD and, xor;
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d, color:%x\n", physDev, x, y, color));

    if(physDev->hasDIB)
    {
        /* gets previous pixel */
        res = physDev->physBitmap.funcs->GetPixel(&physDev->physBitmap, x, y);
     
        /* calculates AND and XOR from color */
        _DIBDRV_CalcAndXorMasks(GetROP2(physDev->hdc), color, &and, &xor);
        
        /* sets the pixel */
        physDev->physBitmap.funcs->SetPixel(&physDev->physBitmap, x, y, and, xor);
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
DWORD DIBDRV_SetDCOrg( DIBDRVPHYSDEV *physDev, int x, int y )
{
    DWORD res;
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = 0;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetDCOrg(physDev->X11PhysDev, x, y);
    }
    return res;
}

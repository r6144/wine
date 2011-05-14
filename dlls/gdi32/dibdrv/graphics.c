/*
 * DIB driver graphics operations.
 *
 * Copyright 2011 Huw Davies
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

#include "gdi_private.h"
#include "dibdrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dib);

static RECT get_device_rect( HDC hdc, int left, int top, int right, int bottom )
{
    RECT rect;

    rect.left   = left;
    rect.top    = top;
    rect.right  = right;
    rect.bottom = bottom;
    if (GetLayout( hdc ) & LAYOUT_RTL)
    {
        /* shift the rectangle so that the right border is included after mirroring */
        /* it would be more correct to do this after LPtoDP but that's not what Windows does */
        rect.left--;
        rect.right--;
    }
    LPtoDP( hdc, (POINT *)&rect, 2 );
    if (rect.left > rect.right)
    {
        int tmp = rect.left;
        rect.left = rect.right;
        rect.right = tmp;
    }
    if (rect.top > rect.bottom)
    {
        int tmp = rect.top;
        rect.top = rect.bottom;
        rect.bottom = tmp;
    }
    return rect;
}

/***********************************************************************
 *           dibdrv_LineTo
 */
BOOL CDECL dibdrv_LineTo( PHYSDEV dev, INT x, INT y )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pLineTo );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    POINT pts[2];

    GetCurrentPositionEx(dev->hdc, pts);
    pts[1].x = x;
    pts[1].y = y;

    LPtoDP(dev->hdc, pts, 2);

    reset_dash_origin(pdev);

    if(defer_pen(pdev) || !pdev->pen_line(pdev, pts, pts + 1))
        return next->funcs->pLineTo( next, x, y );

    return TRUE;
}

/***********************************************************************
 *           get_rop2_from_rop
 *
 * Returns the binary rop that is equivalent to the provided ternary rop
 * if the src bits are ignored.
 */
static inline INT get_rop2_from_rop(INT rop)
{
    return (((rop >> 18) & 0x0c) | ((rop >> 16) & 0x03)) + 1;
}

/***********************************************************************
 *           dibdrv_PatBlt
 */
BOOL CDECL dibdrv_PatBlt( PHYSDEV dev, INT x, INT y, INT width, INT height, DWORD rop )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pPatBlt );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    INT rop2 = get_rop2_from_rop(rop);
    RECT rect = get_device_rect( dev->hdc, x, y, x + width, y + height );
    BOOL done;

    TRACE("(%p, %d, %d, %d, %d, %06x)\n", dev, x, y, width, height, rop);

    if(defer_brush(pdev))
        return next->funcs->pPatBlt( next, x, y, width, height, rop );

    update_brush_rop( pdev, rop2 );

    done = pdev->brush_rects( pdev, 1, &rect );

    update_brush_rop( pdev, GetROP2(dev->hdc) );

    if(!done)
        return next->funcs->pPatBlt( next, x, y, width, height, rop );

    return TRUE;
}

/***********************************************************************
 *           dibdrv_Rectangle
 */
BOOL CDECL dibdrv_Rectangle( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pRectangle );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    RECT rect = get_device_rect( dev->hdc, left, top, right, bottom );
    POINT pts[4];

    TRACE("(%p, %d, %d, %d, %d)\n", dev, left, top, right, bottom);

    if(rect.left == rect.right || rect.top == rect.bottom) return TRUE;

    if(defer_pen(pdev) || defer_brush(pdev))
        return next->funcs->pRectangle( next, left, top, right, bottom );

    reset_dash_origin(pdev);

    /* 4 pts going anti-clockwise starting from top-right */
    pts[0].x = pts[3].x = rect.right - 1;
    pts[0].y = pts[1].y = rect.top;
    pts[1].x = pts[2].x = rect.left;
    pts[2].y = pts[3].y = rect.bottom - 1;

    pdev->pen_line(pdev, pts    , pts + 1);
    pdev->pen_line(pdev, pts + 1, pts + 2);
    pdev->pen_line(pdev, pts + 2, pts + 3);
    pdev->pen_line(pdev, pts + 3, pts    );

    /* FIXME: Will need updating when we support wide pens */

    rect.left   += 1;
    rect.top    += 1;
    rect.right  -= 1;
    rect.bottom -= 1;

    pdev->brush_rects(pdev, 1, &rect);

    return TRUE;
}

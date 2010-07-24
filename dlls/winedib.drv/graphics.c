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

#define LEFT_SIDE   1
#define TOP_SIDE    2
#define RIGHT_SIDE  4
#define BOTTOM_SIDE 8

/* clips a line segment by a rectangular window */
static inline BYTE outCodes(const POINT *p, const RECT *r)
{
    BYTE Code = 0; 

    if(p->y < r->top)
        Code |= TOP_SIDE;
    else if(p->y >= r->bottom)
        Code |= BOTTOM_SIDE;
    if(p->x >= r->right)
        Code |= RIGHT_SIDE;
    else if(p->x < r->left)
        Code |= LEFT_SIDE;
    return Code;
}

static BOOL ClipLine(const POINT *p1, const POINT *p2, const RECT *r, POINT *pc1, POINT *pc2)
{
    BYTE outCode1,outCode2;
    int tmp;
    BYTE tmpCode;

    pc1->x = p1->x; pc1->y = p1->y; 
    pc2->x = p2->x; pc2->y = p2->y; 
    while(TRUE)
    {
        outCode1 = outCodes(pc1, r);
        outCode2 = outCodes(pc2, r);
        if(outCode1 & outCode2)
            return FALSE;
        if(!outCode1 && !outCode2)
            return TRUE;
        if(!outCode1)
        {
            tmp = pc1->x; pc1->x = pc2->x; pc2->x = tmp;
            tmp = pc1->y; pc1->y = pc2->y; pc2->y = tmp;
            tmpCode = outCode1; outCode1 = outCode2; outCode2 = tmpCode;
        } 
        if(outCode1 & TOP_SIDE) 
        { 
            pc1->x += MulDiv(pc2->x - pc1->x, r->top - pc1->y, pc2->y - pc1->y);
            pc1->y = r->top;
        }
        else if(outCode1 & BOTTOM_SIDE) 
        { 
            pc1->x += MulDiv(pc2->x - pc1->x, r->bottom - 1 - pc1->y, pc2->y - pc1->y);
            pc1->y = r->bottom - 1;
        }
        else if(outCode1 & RIGHT_SIDE) 
        { 
            pc1->y += MulDiv(pc2->y - pc1->y, r->right - 1 - pc1->x, pc2->x - pc1->x);
            pc1->x = r->right - 1;
        }
        else if(outCode1 & LEFT_SIDE)
        { 
            pc1->y += MulDiv(pc2->y - pc1->y, r->left - pc1->x, pc2->x - pc1->x);
            pc1->x = r->left;
        }
    } 
}

/* Clips a polygon by an horizontal/vertical line
   which indicates the side :
*/
static inline BOOL PointInside(const POINT *p, const RECT *r, BYTE side)
{
	switch(side)
    {
		case 1: /* left */
			return p->x >= r->left;
		case 2: /* top */
			return p->y >= r->top;
		case 4: /* right */
			return p->x < r->right;
		case 8: /* bottom */
			return p->y < r->bottom;
        default:
            return FALSE;
	}
}

static inline void SideIntersect(const POINT *p1, const POINT *p2, const RECT *r, BYTE side, POINT *inters)
{
	switch( side )
    {
		case LEFT_SIDE: /* left */
            inters->x = r->left;
            inters->y = MulDiv(p2->y - p1->y, r->left - p1->x, p2->x - p1->x) + p1->y;
			break;
		case TOP_SIDE: /* top */
            inters->x = MulDiv(p2->x - p1->x, r->top - p1->y, p2->y - p1->y) + p1->x;
            inters->y = r->top;
			break;
		case RIGHT_SIDE: /* right */
            inters->x = r->right - 1;
            inters->y = MulDiv(p2->y - p1->y, r->right - 1 - p1->x, p2->x - p1->x) + p1->y;
			break;
		case BOTTOM_SIDE: /* bottom */
            inters->x = MulDiv(p2->x - p1->x, r->bottom - 1 - p1->y, p2->y - p1->y) + p1->x;
            inters->y = r->bottom - 1;
			break;
        default:
            break;
	}
}	


static BOOL ClipPolygonBySide(const POINT *pt, int count, const RECT *r, BYTE side, POINT **clipped, int *clippedCount)
{
	int iPoint;
	const POINT *p1, *p2;
    POINT *pOut;

    if(!(*clipped = HeapAlloc(GetProcessHeap(), 0, sizeof(POINT) * count * 2)))
        return FALSE;
    pOut = *clipped;
    *clippedCount = 0;

	p1 = pt + count - 1;
    p2 = pt;
	for(iPoint = 0 ; iPoint < count ; iPoint++)
    {
		if(PointInside(p2, r, side))
        {
			/* point p is "inside" */
			if(!PointInside(p1, r, side))
            {
				/* p is "inside" and s is "outside" */
				SideIntersect(p2, p1, r, side, pOut++);
				(*clippedCount)++;
			}
			pOut->x = p2->x;
            pOut->y = p2->y;
            pOut++;
            (*clippedCount)++;
		}
        else if(PointInside( p1, r, side ))
        {
			/* s is "inside" and p is "outside" */
			SideIntersect(p1, p2, r, side, pOut++);
			(*clippedCount)++;
		}
		p1 = p2++;
	}
    return *clippedCount;
}


/* Clips a polygon by a rectangular window - returns a new polygon */
static BOOL ClipPolygon(const POINT* pt, int count, const RECT *r, POINT **newPt, int *newCount)
{
    POINT *pc1, *pc2;
    int count1, count2;
    BOOL res;
    
    if(!ClipPolygonBySide(pt, count, r, LEFT_SIDE, &pc1, &count1))
        return FALSE;
    res = ClipPolygonBySide(pc1, count1, r, TOP_SIDE, &pc2, &count2);
    HeapFree(GetProcessHeap(), 0, pc1);
    if(!res)
        return FALSE;
    res = ClipPolygonBySide(pc2, count2, r, RIGHT_SIDE, &pc1, &count1);
    HeapFree(GetProcessHeap(), 0, pc2);
    if(!res)
        return FALSE;
    res = ClipPolygonBySide(pc1, count1, r, BOTTOM_SIDE, &pc2, &count2);
    HeapFree(GetProcessHeap(), 0, pc1);
    if(!res)
        return FALSE;

    *newPt = pc2;
    *newCount = count2;
    return TRUE;
}

/* Intersects a line given by 2 points with an horizontal scan line at height y */
static BOOL ScanLine(const POINT *p1, const POINT *p2, int ys, POINT *pRes)
{
    if(!pRes)
        return FALSE;
        
    /* if line lies completely over or under scan line, no intersection */
    if((p1->y < ys && p2->y < ys) || (p1->y >= ys && p2->y >= ys))
        return FALSE;
        
    /* if line is parallel to x axis, we consider it not intersecting */
    if(p1->y == p2->y)
        return FALSE;

    pRes->x = MulDiv(p2->x - p1->x, ys - p1->y, p2->y - p1->y) + p1->x;
    pRes->y = ys;
    return TRUE;
}

/* Gets an x-ordered list of intersection points of a scanline at position y
   with a polygon/polyline */
static BOOL ScanPolygon(const POINT *pt, int count, int ys, POINT **scans, int *scanCount)
{
    const POINT *p1, *p2;
    POINT *pDest;
    int iPoint;
    POINT *ps1, *ps2;
    int i, j, tmp;
    
    /* if not at least 2 points, nothing to return */
    if(count < 2)
        return FALSE;
    
    /* intersections count is AT MOST 'count'; we don't care to
       allocate exact memory needed */
    *scans = HeapAlloc(GetProcessHeap(), 0, sizeof(POINT)*count);
    if(!*scans)
        return FALSE;
        
    /* builds unordered intersections */
    pDest = *scans;
    *scanCount = 0;
    p2 = pt;
    for(iPoint = 0; iPoint < count-1; iPoint++)
    {
        p1 = p2;
        p2++;
        if(ScanLine(p1, p2, ys, pDest))
        {
            pDest++;
            (*scanCount)++;
        }
    }
    p1 = p2;
    p2 = pt;
    if(ScanLine(p1, p2, ys, pDest))
    {
        pDest++;
        (*scanCount)++;
    }
    
    /* now we sort the list -- duped point are left into
       as they're needed for the scanline fill algorithm */
    for(i = 0, ps1 = *scans; i < *scanCount -1; i++, ps1++)
        for(j = i+1, ps2 = ps1+1; j < *scanCount; j++, ps2++)
            if(ps2->x < ps1->x)
            {
                tmp = ps2->x;
                ps2->x = ps1->x;
                ps1->x = tmp;
                tmp = ps2->y;
                ps2->y = ps1->y;
                ps1->y = tmp;
            }
    
    return TRUE;
}

/* gets bounding box of a polygon */
static void PolygonBoundingBox(const POINT *pt, int count, RECT *bBox)
{
    const POINT *p;
    int iPoint;
    
    bBox->left = MAXLONG; bBox->right  = -MAXLONG;
    bBox->top  = MAXLONG; bBox->bottom = -MAXLONG;
    for(p = pt, iPoint = 0; iPoint < count; iPoint++, p++)
    {
        if(p->x < bBox->left  ) bBox->left   = p->x;
        if(p->x > bBox->right ) bBox->right  = p->x;
        if(p->y < bBox->top   ) bBox->top    = p->y;
        if(p->y > bBox->bottom) bBox->bottom = p->y;
    }
}

/* intersect 2 rectangles (just to not use USER32 one...)
   bottom and tight sides are considered OUTSIDE */
BOOL _DIBDRV_IntersectRect(RECT *d, const RECT *s1, const RECT *s2)
{
    if(s1->right <= s2->left ||
       s2->right <= s1->left ||
       s1->bottom <= s2->top ||
       s2->bottom <= s1->top
    )
        return FALSE;
    d->left = s1->left > s2->left ? s1->left : s2->left;
    d->top = s1->top > s2->top ? s1->top : s2->top;
    d->right = s1->right < s2->right ? s1->right : s2->right;
    d->bottom = s1->bottom < s2->bottom ? s1->bottom : s2->bottom;
    return TRUE;
}

/* converts a rectangle form Word space to Device space */
void _DIBDRV_Rect_ws2ds(DIBDRVPHYSDEV *physDev, const RECT *src, RECT *dst)
{
    POINT pts[2];
    pts[0].x = src->left;
    pts[0].y = src->top;
    pts[1].x = src->right;
    pts[1].y = src->bottom;
    LPtoDP(physDev->hdc, pts, 2);
    dst->left   = pts[0].x;
    dst->top    = pts[0].y;
    dst->right  = pts[1].x;
    dst->bottom = pts[1].y;
}

/* converts positions  from Word space to Device space */
void _DIBDRV_Position_ws2ds(DIBDRVPHYSDEV *physDev, int *x, int *y)
{
    POINT p;
    p.x = *x;
    p.y = *y;
    LPtoDP(physDev->hdc, &p, 1);
    *x = p.x;
    *y = p.y;
}

void _DIBDRV_Positions_ws2ds(DIBDRVPHYSDEV *physDev, int *x1, int *y1, int *x2, int *y2)
{
    POINT pts[2];
    pts[0].x = *x1;
    pts[0].y = *y1;
    pts[1].x = *x2;
    pts[1].y = *y2;
    LPtoDP(physDev->hdc, pts, 2);
    *x1 = pts[0].x;
    *y1 = pts[0].y;
    *x2 = pts[1].x;
    *y2 = pts[1].y;
}

/* converts sizes from Word space to Device space */
void _DIBDRV_Sizes_ws2ds(DIBDRVPHYSDEV *physDev, int *w, int *h)
{
    POINT pts[2];
    pts[0].x = 0;
    pts[0].y = 0;
    pts[1].x = *w;
    pts[1].y = *h;
    LPtoDP(physDev->hdc, pts, 2);
    *w = pts[1].x - pts[0].x;
    *h = pts[1].y - pts[0].y;
}

/* converts positions  from Device space to World space */
void _DIBDRV_Position_ds2ws(DIBDRVPHYSDEV *physDev, int *x, int *y)
{
    POINT p;
    p.x = *x;
    p.y = *y;
    DPtoLP(physDev->hdc, &p, 1);
    *x = p.x;
    *y = p.y;
}

void _DIBDRV_Positions_ds2ws(DIBDRVPHYSDEV *physDev, int *x1, int *y1, int *x2, int *y2)
{
    POINT pts[2];
    pts[0].x = *x1;
    pts[0].y = *y1;
    pts[1].x = *x2;
    pts[1].y = *y2;
    DPtoLP(physDev->hdc, pts, 2);
    *x1 = pts[0].x;
    *y1 = pts[0].y;
    *x2 = pts[1].x;
    *y2 = pts[1].y;
}

/* converts sizes from Device space to World space */
void _DIBDRV_Sizes_ds2ws(DIBDRVPHYSDEV *physDev, int *w, int *h)
{
    POINT pts[2];
    pts[0].x = 0;
    pts[0].y = 0;
    pts[1].x = *w;
    pts[1].y = *h;
    DPtoLP(physDev->hdc, pts, 2);
    *w = pts[1].x - pts[0].x;
    *h = pts[1].y - pts[0].y;
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
        _DIBDRV_Position_ws2ds(physDev, &x, &y);
        res = physDev->physBitmap->funcs->GetPixel(physDev->physBitmap, x, y);
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
    RECT *r;
    int iRec;
    POINT p1, p2, pc1, pc2; 
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d\n", physDev, x, y));

    if(physDev->hasDIB)
    {
        res = FALSE;
        GetCurrentPositionEx(physDev->hdc, &curPos);

        /* converts position to device space */
        p1.x = curPos.x; p1.y = curPos.y;
        p2.x = x; p2.y = y;
        LPtoDP(physDev->hdc, &p1, 1);
        LPtoDP(physDev->hdc, &p2, 1);

        /* cycle on all current clipping rectangles */
        r = physDev->regionRects;
        for(iRec = 0; iRec < physDev->regionRectCount; iRec++, r++)
        {
            _DIBDRV_ResetDashOrigin(physDev);
            
            /* clipe line on current region area */
            if(ClipLine(&p1, &p2, r, &pc1, &pc2))
            {
                if(pc1.y == pc2.y)
                    physDev->penHLine(physDev, pc1.x, pc2.x, pc1.y);
                else if(pc1.x == pc2.x)
                    physDev->penVLine(physDev, pc1.x, pc1.y, pc2.y);
                else
                    physDev->penLine(physDev, pc1.x, pc1.y, pc2.x, pc2.y);
                res = TRUE;
            }
        }
        
        /* moves current position to next point */
        MoveToEx(physDev->hdc, x, y, NULL);
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
BOOL DIBDRV_Polygon( DIBDRVPHYSDEV *physDev, const POINT* ptw, int count )
{
    BOOL res;
    POINT *pt;
    RECT *r;
    int iRec;
    POINT *clipped;
    int clippedCount;
    RECT bBox;
    int ys;
    POINT *scans;
    int scanCount, iScan;
    const POINT *p1, *p2;
    int iPoint;
    POINT pc1, pc2;
    
    MAYBE(TRACE("physDev:%p, pt:%p, count:%d\n", physDev, ptw, count));
 
    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        
        res = FALSE;
        
        /* first converts all points to device coords */
        if(!(pt = HeapAlloc(GetProcessHeap(), 0, sizeof(POINT) * count)))
            goto fin;
        memcpy(pt, ptw, sizeof(POINT) * count);
        LPtoDP(physDev->hdc, pt, count);
        
        /* cycle on all current clipping rectangles */
        r = physDev->regionRects;
        for(iRec = 0; iRec < physDev->regionRectCount; iRec++, r++)
        {
            /* filled area */
            if(ClipPolygon(pt, count, r, &clipped, &clippedCount))
            {
                /* gets polygon bounding box -- for ytop and ybottom */
                PolygonBoundingBox(clipped, clippedCount, &bBox);

                /* gets all ordered intersections of polygon with
                   current scanline */
                for(ys = bBox.top; ys < bBox.bottom; ys++)
                {
                    if(ScanPolygon(clipped, clippedCount, ys, &scans, &scanCount))
                    {
                        if(scanCount >= 2)
                        {
                            res = TRUE;
                            p1 = scans;
                            p2 = p1+1;
                            iScan = 0;
                            while(iScan < scanCount - 1)
                            {
                                physDev->brushHLine(physDev, p1->x, p2->x, ys);
                                p1 +=2;
                                p2 +=2;
                                iScan +=2;
                            }
                        }
                        HeapFree(GetProcessHeap(), 0, scans);
                    }
                }
                HeapFree(GetProcessHeap(), 0, clipped);
            }
            
            /* perimeter -- don't use PolyLine for speed */
            p2 = pt;
            for(iPoint = 0; iPoint < count -1; iPoint++)
            {
                p1 = p2++;
                if(ClipLine(p1, p2, r, &pc1, &pc2))
                {
                    res = TRUE;
                    physDev->penLine(physDev, pc1.x, pc1.y, pc2.x, pc2.y);
                }
            }
            p1 = p2;
            p2 = pt;
            if(ClipLine(p1, p2, r, &pc1, &pc2))
            {
                res = TRUE;
                physDev->penLine(physDev, pc1.x, pc1.y, pc2.x, pc2.y);
            }
        }
        
        HeapFree(GetProcessHeap(), 0, pt);
                        
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPolygon(physDev->X11PhysDev, ptw, count);
    }
fin:
    return res;
}

/**********************************************************************
 *          DIBDRV_Polyline
 */
BOOL DIBDRV_Polyline( DIBDRVPHYSDEV *physDev, const POINT* ptw, int count )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, pt:%p, count:%d\n", physDev, ptw, count));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        POINT *pt;
        RECT *r;
        POINT pc1, pc2;
        int iRec, iPoint;

        if(count < 2)
            return FALSE;
        res = FALSE;
            
        /* first converts all points to device coords */
        if(!(pt = HeapAlloc(GetProcessHeap(), 0, sizeof(POINT) * count)))
            return FALSE;
        memcpy(pt, ptw, sizeof(POINT) * count);
        LPtoDP(physDev->hdc, pt, count);
        
        r = physDev->regionRects;
        for(iRec = 0; iRec < physDev->regionRectCount; iRec++)
        {
            const POINT *p2 = pt, *p1;
            for(iPoint = 0; iPoint < count -1; iPoint++)
            {
                p1 = p2++;
                if(ClipLine(p1, p2, r, &pc1, &pc2))
                {
                    res = TRUE;
                    physDev->penLine(physDev, pc1.x, pc1.y, pc2.x, pc2.y);
                }
            }
            r++;
        }
        
        HeapFree(GetProcessHeap(), 0, pt);
        
        return res;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPolyline(physDev->X11PhysDev, ptw, count);
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
    BOOL res;
    int i;
    RECT rWorld, rDevice, rClipped;
    RECT *r;
    int iRec;
    
    MAYBE(TRACE("physDev:%p, left:%d, top:%d, right:%d, bottom:%d\n", physDev, x1, y1, x2, y2));

    if(physDev->hasDIB)
    {
        res = FALSE;
        
        OrderInt(&x1, &x2);
        OrderInt(&y1, &y2);

        /* converts to device space */
        rWorld.left = x1; rWorld.top = y1; rWorld.right = x2; rWorld.bottom = y2;
        _DIBDRV_Rect_ws2ds(physDev, &rWorld, &rDevice);

        /* loop on all clip region rectangles */
        r = physDev->regionRects;
        for(iRec = 0; iRec < physDev->regionRectCount; iRec++, r++)
        {
            /* clips rectangle to current region */
            if(_DIBDRV_IntersectRect(&rClipped, &rDevice, r))
            {
                x1 = rClipped.left; y1 = rClipped.top;
                x2 = rClipped.right; y2 = rClipped.bottom;
                
                _DIBDRV_ResetDashOrigin(physDev);
                
                /* fill the inside, if not null brush */
                if(physDev->brushStyle != BS_NULL)
                {
                    if(x2 > x1)
                        for (i = y1; i < y2; i++)
                            physDev->brushHLine(physDev, x1, x2, i);
                }

                /* draw perimeter, if not null pen */
                if(physDev->penStyle != PS_NULL)
                {
                
                    /* particular case where the rectangle
                       degenerates to a line or a point */
                    if(x1 >= x2 - 1)
                        physDev->penVLine(physDev, x1, y1, y2);
                    else if (y1 >= y2 -1)
                        physDev->penHLine(physDev, x1, x2, y1);
                    else
                    {
                        /* Draw the perimeter */
                        physDev->penHLine(physDev, x1    , x2    , y1   );
                        physDev->penHLine(physDev, x1    , x2    , y2 - 1);
                        physDev->penVLine(physDev, x1    , y1 + 1, y2 - 1);
                        physDev->penVLine(physDev, x2 - 1, y1 + 1, y2 - 1);
                    }
                }
                res = TRUE;
            }
        }
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
        /* get real colorref */
        color = _DIBDRV_MapColor(physDev, color);
        
        /* map to pixel color / palette index */
        color = physDev->physBitmap->funcs->ColorToPixel(physDev->physBitmap, color);
        
        _DIBDRV_Position_ws2ds(physDev, &x, &y);

        /* gets previous pixel */
        res = physDev->physBitmap->funcs->GetPixel(physDev->physBitmap, x, y);
     
        /* calculates AND and XOR from color */
        _DIBDRV_CalcAndXorMasks(GetROP2(physDev->hdc), color, &and, &xor);
        
        /* sets the pixel */
        physDev->physBitmap->funcs->SetPixel(physDev->physBitmap, x, y, and, xor);
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

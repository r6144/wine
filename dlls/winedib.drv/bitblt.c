/*
 * DIBDRV bit-blit operations
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

static inline void intSwap(int *a, int *b)
{
    int tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

static inline void setRect(RECT *r, int x1, int y1, int x2, int y2)
{
    r->left   = x1;
    r->top    = y1;
    r->right  = x2;
    r->bottom = y2;
}

static inline void setPoint(POINT *p, int x, int y)
{
    p->x = x;
    p->y = y;
}

static inline void setSize(SIZE *sz, int cx, int cy)
{
    sz->cx = cx;
    sz->cy = cy;
}

/* clips a source and destination areas to their respective clip rectangles
   returning both source and dest modified; result is TRUE if clipping
   leads to a non null rectangle, FALSE otherwise */
static BOOL BitBlt_ClipAreas(POINT *ps, POINT *pd, SIZE *sz, RECT*srcClip, RECT*dstClip)
{
    int xs1, ys1, xs2, ys2;
    int xsc1, ysc1, xsc2, ysc2;
    int xd1, yd1, xd2, yd2;
    int xdc1, ydc1, xdc2, ydc2;
    int w, h, dx, dy;
    
    /* extract sizes */
    w = sz->cx; h = sz->cy;
    
    /* if sizes null or negative, just return false */
    if(w <= 0 || h <= 0)
        return FALSE;
        
    /* extract dest area data */
    xd1 = pd->x;
    yd1 = pd->y;
    xd2 = xd1 + w;
    yd2 = yd1 + h;
    
    /* extract source data */
    xs1 = ps->x;
    ys1 = ps->y;
    xs2 = xs1 + w;
    ys2 = ys1 + h;

    /* if source clip area is not null, do first clipping on it */
    if(srcClip)
    {
        /* extract source clipping area */
        xsc1 = srcClip->left;
        ysc1 = srcClip->top;
        xsc2 = srcClip->right;
        ysc2 = srcClip->bottom;

        /* order clip area rectangle points */
        if(xsc1 > xsc2) intSwap(&xsc1, &xsc2);
        if(ysc1 > ysc2) intSwap(&ysc1, &ysc2);
        
        /* clip on source clipping start point */
        if(xs1 < xsc1) { dx = xsc1 - xs1; w -= dx; xd1 += dx; xs1 = xsc1; }
        if(ys1 < ysc1) { dy = ysc1 - ys1; h -= dy; yd1 += dy; ys1 = ysc1; }
        
        /* clip on source clipping end point */
        if(xs2 > xsc2) { dx = xs2 - xsc2; w -= dx; xd2 -= dx; xs2 = xsc2; }
        if(ys2 > ysc2) { dy = ys2 - ysc2; h -= dy; yd2 -= dy; ys2 = ysc2; }

        /* if already zero area, return false */
        if(w <= 0 || h <= 0)
            return FALSE;
    }
    /* now do clipping on destination area */
    if(dstClip)
    {    
        /* extract destination clipping area */
        xdc1 = dstClip->left;
        ydc1 = dstClip->top;
        xdc2 = dstClip->right;
        ydc2 = dstClip->bottom;

        /* order clip area rectangle points */
        if(xdc1 > xdc2) intSwap(&xdc1, &xdc2);
        if(ydc1 > ydc2) intSwap(&ydc1, &ydc2);
            
        /* clip on dest clipping start point */
        if(xd1 < xdc1) { dx = xdc1 - xd1; w -= dx; xs1 += dx; xd1 = xdc1; }
        if(yd1 < ydc1) { dy = ydc1 - yd1; h -= dy; ys1 += dy; yd1 = ydc1; }
        
        /* clip on dest clipping end point */
        if(xd2 > xdc2) { dx = xd2 - xdc2; w -= dx; xs2 -= dx; xd2 = xdc2; }
        if(yd2 > ydc2) { dy = yd2 - ydc2; h -= dy; ys2 -= dy; yd2 = ydc2; }
        
        /* if already zero area, return false */
        if(w <= 0 || h <= 0)
            return FALSE;
    }
        
    /* sets clipped/translated points and sizes and returns TRUE */
    ps->x = xs1; ps->y = ys1;
    pd->x = xd1; pd->y = yd1;
    sz->cx = w; sz->cy = h;
    
    return TRUE;
        
}


/* clips a source and destination areas to their respective clip rectangles
   returning both source and dest modified; result is TRUE if clipping
   leads to a non null rectangle, FALSE otherwise */
static BOOL StretchBlt_ClipAreas(POINT *ps, POINT *pd, SIZE *szSrc, SIZE *szDst, RECT*srcClip, RECT*dstClip)
{
    int xs1, ys1, xs2, ys2;
    int xsc1, ysc1, xsc2, ysc2;
    int xd1, yd1, xd2, yd2;
    int xdc1, ydc1, xdc2, ydc2;
    int ws, hs, wd, hd, dx, dy;
    int mulh, divh, mulv, divv;
    
    /* extract sizes */
    ws = szSrc->cx; hs = szSrc->cy;
    wd = szDst->cx; hd = szDst->cy;
    
    /* if sizes null or negative, just return false */
    /* FIXME : add support for mirror stretch */
    if(ws <= 0 || hs <= 0 || wd <= 0 || hd <= 0)
        return FALSE;
        
    /* stores scaling factors from source rect to dest one */
    mulh = wd; divh = ws;
    mulv = hd; divv = hs;
        
    /* extract dest area data */
    xd1 = pd->x;
    yd1 = pd->y;
    xd2 = xd1 + wd;
    yd2 = yd1 + hd;
    
    /* extract source data */
    xs1 = ps->x;
    ys1 = ps->y;
    xs2 = xs1 + ws;
    ys2 = ys1 + hs;
        
    /* if source clip area is not null, do first clipping on it */
    if(srcClip)
    {
        /* extract source clipping area */
        xsc1 = srcClip->left;
        ysc1 = srcClip->top;
        xsc2 = srcClip->right;
        ysc2 = srcClip->bottom;
        
        /* order clip area rectangle points */
        if(xsc1 > xsc2) intSwap(&xsc1, &xsc2);
        if(ysc1 > ysc2) intSwap(&ysc1, &ysc2);
        
        /* clip on source clipping start point */
        if(xs1 < xsc1) { dx = xsc1 - xs1; ws -= dx; xd1 += MulDiv(dx, mulh, divh); xs1 = xsc1; }
        if(ys1 < ysc1) { dy = ysc1 - ys1; hs -= dy; yd1 += MulDiv(dy, mulv, divv); ys1 = ysc1; }
        
        /* clip on source clipping end point */
        if(xs2 > xsc2) { dx = xs2 - xsc2; ws -= dx; xd2 -= MulDiv(dx, mulh, divh); xs2 = xsc2; }
        if(ys2 > ysc2) { dy = ys2 - ysc2; hs -= dy; yd2 -= MulDiv(dy, mulv, divv); ys2 = ysc2; }
        
        /* if already zero area, return false */
        if(ws <= 0 || hs <= 0)
            return FALSE;
        wd = xd2 - xd1;
        hd = yd2 - yd1;
    }
    /* now do clipping on destination area */

    if(dstClip)
    {    
        /* extract destination clipping area */
        xdc1 = dstClip->left;
        ydc1 = dstClip->top;
        xdc2 = dstClip->right;
        ydc2 = dstClip->bottom;
        
        /* order clip area rectangle points */
        if(xdc1 > xdc2) intSwap(&xdc1, &xdc2);
        if(ydc1 > ydc2) intSwap(&ydc1, &ydc2);
            
        /* clip on dest clipping start point */
        if(xd1 < xdc1) { dx = xdc1 - xd1; wd -= dx; xs1 += MulDiv(dx, divh, mulh); xd1 = xdc1; }
        if(yd1 < ydc1) { dy = ydc1 - yd1; hd -= dy; ys1 += MulDiv(dy, divv, mulv); yd1 = ydc1; }
        
        /* clip on dest clipping end point */
        if(xd2 > xdc2) { dx = xd2 - xdc2; wd -= dx; xs2 -= MulDiv(dx, divh, mulh); xd2 = xdc2; }
        if(yd2 > ydc2) { dy = yd2 - ydc2; hd -= dy; ys2 -= MulDiv(dy, divv, mulv); yd2 = ydc2; }
        
        /* if already zero area, return false */
        if(wd <= 0 || hd <= 0)
            return FALSE;
            
        ws = xs2 - xs1;
        hs = ys2 - ys1;
    }
        
    /* sets clipped/translated points and sizes and returns TRUE */
    ps->x = xs1; ps->y = ys1;
    pd->x = xd1; pd->y = yd1;
    szSrc->cx = ws; szSrc->cy = hs;
    szDst->cx = wd; szDst->cy = hd;
    
    return TRUE;

}

/***********************************************************************
 *           _DIBDRV_InternalAlphaBlend
 */
BOOL _DIBDRV_InternalAlphaBlend( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst, INT widthDst, INT heightDst,
                        DIBDRVPHYSDEV *physDevSrc, INT xSrc, INT ySrc, INT widthSrc, INT heightSrc,
                        BLENDFUNCTION blendfn)
{
    BOOL res;
    POINT pd, ps;
    SIZE szSrc, szDst;
    int iRec;
    RECT dstClip, srcClip;
    
    /* converts to device spaces */
    _DIBDRV_Position_ws2ds(physDevDst, &xDst, &yDst);
    _DIBDRV_Sizes_ws2ds(physDevDst, &widthDst, &heightDst);
    _DIBDRV_Position_ws2ds(physDevSrc, &xSrc, &ySrc);
    _DIBDRV_Sizes_ws2ds(physDevSrc, &widthSrc, &heightSrc);
    
    /* from tests, it seems that negative coords on phys space are not allowed */
    if(xDst < 0 || yDst < 0 || xSrc < 0 || ySrc < 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* first clip on physical DC sizes */
    setPoint(&pd, xDst, yDst);
    setPoint(&ps, xSrc, ySrc);
    setSize(&szDst, widthDst, heightDst);
    setSize(&szSrc, widthSrc, heightSrc);
    setRect(&dstClip, 0, 0, physDevDst->physBitmap.width, physDevDst->physBitmap.height);
    if(physDevSrc)
    {
        setRect(&srcClip, 0, 0, physDevSrc->physBitmap.width, physDevSrc->physBitmap.height);
        res = StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, &srcClip, &dstClip);
    }
    else
        res = StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, 0, &dstClip);
    if(!res)
        return TRUE;
    xDst = pd.x; yDst = pd.y;
    xSrc = ps.x; ySrc = ps.y;
    widthDst = szDst.cx; heightDst = szDst.cy;
    widthSrc = szSrc.cx; heightSrc = szSrc.cy;
    
    /* then, do blitting for each dest clip area (no clipping on source) */
    res = FALSE;
    for(iRec = 0; iRec < physDevDst->regionRectCount; iRec++)
    {
        RECT *r = physDevDst->regionRects + iRec;
        setRect(&dstClip, r->left, r->top, r->right, r->bottom);
        setPoint(&pd, xDst, yDst);
        setPoint(&ps, xSrc, ySrc);
        setSize(&szDst, widthDst, heightDst);
        setSize(&szSrc, widthSrc, heightSrc);
        if(!StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, 0, &dstClip))
            continue;
        if(physDevDst->physBitmap.funcs->AlphaBlend(physDevDst, pd.x, pd.y, szDst.cx, szDst.cy,
                                                    physDevSrc, ps.x, ps.y, szSrc.cx, szSrc.cy, blendfn))
            res = TRUE;
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_AlphaBlend
 */
BOOL DIBDRV_AlphaBlend( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst, INT widthDst, INT heightDst,
                        DIBDRVPHYSDEV *physDevSrc, INT xSrc, INT ySrc, INT widthSrc, INT heightSrc,
                        BLENDFUNCTION blendfn)
{
    BOOL res;
    
    POINT pd = {xDst, yDst};
    SIZE szDst = {widthDst, heightDst};

    MAYBE(TRACE("physDevDst:%p(%s%s), xDst:%d, yDst:%d, widthDst:%d, heightDst:%d, physDevSrc:%p(%s%s), xSrc:%d, ySrc:%d, widthSrc:%d, heightSrc:%d\n",
          physDevDst, physDevDst->hasDIB ? "DIB-" : "DDB", physDevDst->hasDIB ? _DIBDRVBITMAP_GetFormatName(&physDevDst->physBitmap) : "",
          xDst, yDst, widthDst, heightDst,
          physDevSrc, physDevSrc->hasDIB ? "DIB-" : "DDB", physDevSrc->hasDIB ? _DIBDRVBITMAP_GetFormatName(&physDevSrc->physBitmap) : "",
          xSrc, ySrc, widthSrc, heightSrc));

    /* if sizes are null or negative, or source positions are negatives, returns false */
    if(widthSrc <= 0 || heightSrc <= 0 ||
       widthDst <= 0 || heightDst <= 0)
    {
        res = FALSE;
        SetLastError(ERROR_INVALID_PARAMETER);
        goto fin;
    }
          
    /* source sould be a 32 bit DIB */
    if(!physDevSrc)
    {
        FIXME("Null source bitmap -- shouldn't happen\n");
        res = FALSE;
        goto fin;
    }
    else if(!physDevSrc->hasDIB)
    {
        FIXME("DDB source bitmap -- shouldn't happen\n");
        res = FALSE;
        goto fin;
    }
    
    if(physDevDst->hasDIB)
    {
        /* DIB section selected in dest DC, use DIB Engine */
        MAYBE(TRACE("Blending DIB->DIB\n"));
        res = _DIBDRV_InternalAlphaBlend(physDevDst, xDst, yDst, widthDst, heightDst,
                                         physDevSrc, xSrc, ySrc, widthSrc, heightSrc, blendfn);
    }
    else
    {
        /* DDB selected on dest DC -- must double-convert */
        HBITMAP tmpDIB, stock;
        HDC tmpDC;
        MAYBE(TRACE("Blending DIB->DDB\n"));
        
        /* we should anyways convert dest to physical coordinates here before processing
           in order to check its consistency -- source coords will be converted/clipped later
           As we do a conversion to a temporary DIB for destination, we don't care about it */
        _DIBDRV_Position_ws2ds(physDevDst, &pd.x, &pd.y);
        _DIBDRV_Sizes_ws2ds(physDevDst, &szDst.cx, &szDst.cy);
        
        /* test shows that negatives origins are not allowed */
        if(pd.x < 0 || pd.y < 0)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            res = FALSE;
            goto fin;
        }

        /* converts dest DDB onto a temporary DIB -- just the needed part */
        /* WARNING -- that one could fail if rectangle on dest id out of range */
        tmpDIB = _DIBDRV_ConvertDevDDBtoDIB(physDevDst->hdc, physDevSrc->hdc, pd.x, pd.y, szDst.cx, szDst.cy);
        if(!tmpDIB)
        {
            ERR("Couldn't convert dest DDB to DIB\n");
            res = FALSE;
            goto fin;
        }
        
        /* selects the temporary DIB into a temporary DC */
        tmpDC = CreateCompatibleDC(physDevDst->hdc);
        if(!tmpDC)
        {
            ERR("Couldn't create temporary DC\n");
            DeleteObject(tmpDIB);
            res = FALSE;
            goto fin;
        }
        stock = SelectObject(tmpDC, tmpDIB);
        if(!stock)
        {
            ERR("Couldn't select temporary DIB into temporary DC\n");
            DeleteDC(tmpDC);
            DeleteObject(tmpDIB);
            res = FALSE;
            goto fin;
        }
        
        /* blends source DIB onto temp DIB and re-blits onto dest DC */
        res = GdiAlphaBlend(tmpDC, 0, 0, szDst.cx, szDst.cy, physDevSrc->hdc, xSrc, ySrc, widthSrc, heightSrc, blendfn);
        if(!res)
            MAYBE(TRACE("AlphaBlend failed\n"));
        else
            res = BitBlt(physDevDst->hdc, xDst, yDst, widthDst, heightDst, tmpDC, 0, 0, SRCCOPY);
            
        /* frees resources */
        SelectObject(tmpDC, stock);
        DeleteDC(tmpDC);
        DeleteObject(tmpDIB);        
    }
fin:
    return res;
}

/***********************************************************************
 *           _DIBDRV_InternalBitBlt
 */
BOOL _DIBDRV_InternalBitBlt( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop )
{
    BOOL res;
    POINT pd, ps;
    SIZE sz;
    int iRec;
    RECT dstClip, srcClip;

    /* converts to device spaces */
    _DIBDRV_Position_ws2ds(physDevDst, &xDst, &yDst);
    _DIBDRV_Sizes_ws2ds(physDevDst, &width, &height);
    if(physDevSrc)
        _DIBDRV_Position_ws2ds(physDevSrc, &xSrc, &ySrc);

    /* first clip on physical DC sizes */
    setPoint(&pd, xDst, yDst);
    setPoint(&ps, xSrc, ySrc);
    setSize(&sz, width, height);
    setRect(&dstClip, 0, 0, physDevDst->physBitmap.width, physDevDst->physBitmap.height);

    if(physDevSrc)
    {
        setRect(&srcClip, 0, 0, physDevSrc->physBitmap.width, physDevSrc->physBitmap.height);
        res = BitBlt_ClipAreas(&ps, &pd, &sz, &srcClip, &dstClip);
    }
    else
        res = BitBlt_ClipAreas(&ps, &pd, &sz, 0, &dstClip);
    if(!res)
        return TRUE;
    xDst = pd.x; yDst = pd.y;
    xSrc = ps.x; ySrc = ps.y;
    width = sz.cx; height = sz.cy;
    
    /* then, do blitting for each dest clip area (no clipping on source) */
    res = TRUE;
    for(iRec = 0; iRec < physDevDst->regionRectCount; iRec++)
    {
        RECT *r = physDevDst->regionRects + iRec;
        setRect(&dstClip, r->left, r->top, r->right, r->bottom);
        setPoint(&pd, xDst, yDst);
        setPoint(&ps, xSrc, ySrc);
        setSize(&sz, width, height);
        if(!BitBlt_ClipAreas(&ps, &pd, &sz, 0, &dstClip))
            continue;
        if(!physDevDst->physBitmap.funcs->BitBlt(physDevDst, pd.x, pd.y, sz.cx, sz.cy, physDevSrc, ps.x, ps.y, rop))
            res = FALSE;
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_BitBlt                                             */
BOOL DIBDRV_BitBlt( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop )
{
    BOOL res;
    
    /* clip blit area */
    POINT pd = {xDst, yDst};
    POINT ps = {xSrc, ySrc};
    SIZE sz = {width, height};

    MAYBE(TRACE("physDevDst:%p(%s%s), xDst:%d, yDst:%d, width:%d, height:%d, physDevSrc:%p(%s%s), xSrc:%d, ySrc:%d, rop:%08x\n",
          physDevDst, physDevDst->hasDIB ? "DIB-" : "DDB", physDevDst->hasDIB ? _DIBDRVBITMAP_GetFormatName(&physDevDst->physBitmap) : "",
          xDst, yDst, width, height,
          physDevSrc, physDevSrc ? (physDevSrc->hasDIB ? "DIB-" : "DDB"): "---", physDevSrc && physDevSrc->hasDIB ? _DIBDRVBITMAP_GetFormatName(&physDevSrc->physBitmap) : "",
          xSrc, ySrc, rop));

    if(physDevDst->hasDIB)
    {
        /* DIB section selected in dest DC, use DIB Engine */
        /* clip blit area */
        RECT dstClip = {0, 0, physDevDst->physBitmap.width, physDevDst->physBitmap.height};
        
        if(!physDevSrc || physDevSrc->hasDIB)
        {
            /* source is null or has a DIB, no need to convert anyting */
            res = _DIBDRV_InternalBitBlt(physDevDst, xDst, yDst, width, height, physDevSrc, xSrc, ySrc, rop);
        }
        else
        {
            /* source is a DDB, must convert it to DIB */

            /* don't clip on source */            
            res = !BitBlt_ClipAreas(&ps, &pd, &sz, 0, &dstClip);
            if(res)
                goto noBlt2;
            xDst = pd.x; yDst = pd.y; width = sz.cx; height = sz.cy; xSrc = ps.x; ySrc = ps.y;

            /* we must differentiate from 2 cases :
               1) source DC is a memory DC
               2) source DC is a device DC */
            if(GetObjectType(physDevSrc->hdc) == OBJ_MEMDC)
            {
                /* memory DC */
                HBITMAP dib, ddb;

                ddb = SelectObject(physDevSrc->hdc, GetStockObject(DEFAULT_BITMAP));
                if(!ddb)
                {
                    ERR("Couldn't select out DDB from source HDC\n");
                    res = 0;
                    goto noBlt1;
                }
                dib = _DIBDRV_ConvertDDBtoDIB(physDevSrc->hdc, ddb, ySrc, height);
                if(!dib)
                {
                    ERR("Failed converting source DDB to DIB\n");
                    SelectObject(physDevSrc->hdc, ddb);
                    res = 0;
                    goto noBlt1;
                }
                SelectObject(physDevSrc->hdc, dib);
                res = _DIBDRV_InternalBitBlt(physDevDst, xDst, yDst, width, height, physDevSrc, xSrc, 0, rop);
                SelectObject(physDevSrc->hdc, ddb);
                DeleteObject(dib);
    noBlt1:
                ;
            }
            else
            {
                /* device DC */
                HBITMAP dib, stock;
                HDC memHdc;

                dib = _DIBDRV_ConvertDevDDBtoDIB(physDevSrc->hdc, physDevDst->hdc, xSrc, ySrc, width, height);
                if(!dib)
                {
                    ERR("Failed converting source DDB tp DIB for device DC\n");
                    res = 0;
                    goto noBlt2;
                }
                memHdc = CreateCompatibleDC(physDevDst->hdc);
                if(!memHdc)
                {
                    ERR("Failed creating temporary memory DC\n");
                    DeleteObject(dib);
                    res = 0;
                    goto noBlt2;
                }
                stock = SelectObject(memHdc, dib);
                if(!stock)
                {
                    ERR("Failed selecting converted DIB into temporary memory DC\n");
                    DeleteObject(dib);
                    DeleteDC(memHdc);
                    res = 0;
                    goto noBlt2;
                }
                res = BitBlt(physDevDst->hdc, xDst, yDst, width, height, memHdc, 0, 0, rop);

                SelectObject(memHdc, stock);
                DeleteObject(dib);
                DeleteDC(memHdc);
    noBlt2:
                ;
            }
        }
    }
    else /* dest is a DDB */
    {
        /* DDB selected on dest DC, use X11 Driver */
        if(!physDevSrc || !physDevSrc->hasDIB)
        {
            /* source is null or has also a DDB, no need to convert anything */
            res = _DIBDRV_GetDisplayDriver()->pBitBlt(physDevDst->X11PhysDev, xDst, yDst, width, height,
                                                      physDevSrc ? physDevSrc->X11PhysDev : 0, xSrc, ySrc, rop);
        }
        else
        {
            /* DIB on source, DDB on dest -- must convert source DIB to DDB and use X11 driver for blit */
            HBITMAP dib, ddb;

            /* clip blit area */
            if(physDevSrc)
            {
                RECT srcClip = {0, 0, physDevSrc->physBitmap.width, physDevSrc->physBitmap.height};
                res = !BitBlt_ClipAreas(&ps, &pd, &sz, &srcClip, 0);
            }
            else
                res = FALSE;
            if(res)
                goto noBlt3;
            xDst = pd.x; yDst = pd.y; width = sz.cx; height = sz.cy; xSrc = ps.x; ySrc = ps.y;

            dib = SelectObject(physDevSrc->hdc, GetStockObject(DEFAULT_BITMAP));
            if(!dib)
            {
                ERR("Couldn't select out DIB from source HDC\n");
                res = 0;
                goto noBlt3;
            }
            ddb = _DIBDRV_ConvertDIBtoDDB(physDevSrc->hdc, dib, ySrc, height);
            if(!ddb)
            {
                ERR("Failed converting source DIB to DDB\n");
                SelectObject(physDevSrc->hdc, dib);
                res = 0;
                goto noBlt3;
            }
            SelectObject(physDevSrc->hdc, ddb);
            res = _DIBDRV_GetDisplayDriver()->pBitBlt(physDevDst->X11PhysDev, xDst, yDst, width, height,
                                                      physDevSrc ? physDevSrc->X11PhysDev : 0, xSrc, 0, rop);
            SelectObject(physDevSrc->hdc, dib);
            DeleteObject(ddb);
noBlt3:
            ;
        }
    }
    return res;
}

/***********************************************************************
 *           _DIBDRV_InternalStretchBlt
 */
BOOL _DIBDRV_InternalStretchBlt( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT widthDst, INT heightDst, DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, int widthSrc, int heightSrc, DWORD rop )
{
    BOOL res;
    POINT pd, ps;
    SIZE szSrc, szDst;
    int iRec;
    RECT dstClip, srcClip;

    /* converts to device spaces */
    _DIBDRV_Position_ws2ds(physDevDst, &xDst, &yDst);
    _DIBDRV_Sizes_ws2ds(physDevDst, &widthDst, &heightDst);
    if(physDevSrc)
    {
        _DIBDRV_Position_ws2ds(physDevSrc, &xSrc, &ySrc);
        _DIBDRV_Sizes_ws2ds(physDevSrc, &widthSrc, &heightSrc);
    }

    /* first clip on physical DC sizes */
    setPoint(&pd, xDst, yDst);
    setPoint(&ps, xSrc, ySrc);
    setSize(&szDst, widthDst, heightDst);
    setSize(&szSrc, widthSrc, heightSrc);
    setRect(&dstClip, 0, 0, physDevDst->physBitmap.width, physDevDst->physBitmap.height);
    if(physDevSrc)
    {
        setRect(&srcClip, 0, 0, physDevSrc->physBitmap.width, physDevSrc->physBitmap.height);
        res = StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, &srcClip, &dstClip);
    }
    else
        res = StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, 0, &dstClip);
    if(!res)
        return FALSE;
    xDst = pd.x; yDst = pd.y;
    xSrc = ps.x; ySrc = ps.y;
    widthDst = szDst.cx; heightDst = szDst.cy;
    widthSrc = szSrc.cx; heightSrc = szSrc.cy;
    
    /* then, do blitting for each dest clip area (no clipping on source) */
    res = FALSE;
    for(iRec = 0; iRec < physDevDst->regionRectCount; iRec++)
    {
        RECT *r = physDevDst->regionRects + iRec;
        setRect(&dstClip, r->left, r->top, r->right, r->bottom);
        setPoint(&pd, xDst, yDst);
        setPoint(&ps, xSrc, ySrc);
        setSize(&szDst, widthDst, heightDst);
        setSize(&szSrc, widthSrc, heightSrc);
        if(!StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, 0, &dstClip))
            continue;
        if(physDevDst->physBitmap.funcs->StretchBlt(physDevDst, pd.x, pd.y, szDst.cx, szDst.cy,
                                                    physDevSrc, ps.x, ps.y, szSrc.cx, szSrc.cy, rop))
            res = TRUE;
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_StretchBlt
 */
BOOL DIBDRV_StretchBlt( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                        INT widthDst, INT heightDst,
                        DIBDRVPHYSDEV *physDevSrc, INT xSrc, INT ySrc,
                        INT widthSrc, INT heightSrc, DWORD rop )
{
    BOOL res;
    
    /* clip blit area */
    POINT pd = {xDst, yDst};
    POINT ps = {xSrc, ySrc};
    SIZE szDst = {widthDst, heightDst};
    SIZE szSrc = {widthSrc, heightSrc};

    /* if source and dest sizes match, just call BitBlt(), it's faster */
    if(!physDevSrc || (widthDst == widthSrc && heightDst == heightSrc))
        return DIBDRV_BitBlt(physDevDst, xDst, yDst, widthDst, heightDst, physDevSrc, xSrc, ySrc, rop);

    MAYBE(TRACE("physDevDst:%p(%s%s), xDst:%d, yDst:%d, widthDst:%d, heightDst:%d, physDevSrc:%p(%s%s), xSrc:%d, ySrc:%d, widthSrc:%d, heightSrc:%d, rop:%08x\n",
          physDevDst, physDevDst->hasDIB ? "DIB-" : "DDB", physDevDst->hasDIB ? _DIBDRVBITMAP_GetFormatName(&physDevDst->physBitmap) : "",
          xDst, yDst, widthDst, heightDst,
          physDevSrc, physDevSrc->hasDIB ? "DIB-" : "DDB", physDevSrc->hasDIB ? _DIBDRVBITMAP_GetFormatName(&physDevSrc->physBitmap) : "",
          xSrc, ySrc, widthSrc, heightSrc, rop));

    if(physDevDst->hasDIB)
    {
        /* DIB section selected in dest DC, use DIB Engine */

        if(!physDevSrc || physDevSrc->hasDIB)
        {
            /* source is null or has a DIB, no need to convert anyting */
            res = _DIBDRV_InternalStretchBlt(physDevDst, xDst, yDst, widthDst, heightDst, physDevSrc, xSrc, ySrc, widthSrc, heightSrc, rop);
        }
        else
        {
            /* source is a DDB, must convert it to DIB */

            /* we must differentiate from 2 cases :
               1) source DC is a memory DC
               2) source DC is a device DC */
            if(GetObjectType(physDevSrc->hdc) == OBJ_MEMDC)
            {
                /* memory DC */
                HBITMAP dib, ddb;

                ddb = SelectObject(physDevSrc->hdc, GetStockObject(DEFAULT_BITMAP));
                if(!ddb)
                {
                    ERR("Couldn't select out DDB from source HDC\n");
                    res = 0;
                    goto noBlt1;
                }
                dib = _DIBDRV_ConvertDDBtoDIB(physDevSrc->hdc, ddb, ySrc, heightSrc);
                if(!dib)
                {
                    ERR("Failed converting source DDB to DIB\n");
                    SelectObject(physDevSrc->hdc, ddb);
                    res = 0;
                    goto noBlt1;
                }
                SelectObject(physDevSrc->hdc, dib);
                res = _DIBDRV_InternalStretchBlt(physDevDst, xDst, yDst, widthDst, heightDst,
                                                 physDevSrc, xSrc, 0, widthSrc, heightSrc, rop);
                SelectObject(physDevSrc->hdc, ddb);
                DeleteObject(dib);
    noBlt1:
                ;
            }
            else
            {
                /* device DC */
                HBITMAP dib, stock;
                HDC memHdc;

                dib = _DIBDRV_ConvertDevDDBtoDIB(physDevSrc->hdc, physDevDst->hdc, xSrc, ySrc, widthSrc, heightSrc);
                if(!dib)
                {
                    ERR("Failed converting source DDB tp DIB for device DC\n");
                    res = 0;
                    goto noBlt2;
                }
                memHdc = CreateCompatibleDC(physDevDst->hdc);
                if(!memHdc)
                {
                    ERR("Failed creating temporary memory DC\n");
                    DeleteObject(dib);
                    res = 0;
                    goto noBlt2;
                }
                stock = SelectObject(memHdc, dib);
                if(!stock)
                {
                    ERR("Failed selecting converted DIB into temporary memory DC\n");
                    DeleteObject(dib);
                    DeleteDC(memHdc);
                    res = 0;
                    goto noBlt2;
                }
                res = StretchBlt(physDevDst->hdc, xDst, yDst, widthDst, heightDst, memHdc, 0, 0, widthSrc, widthDst, rop);

                SelectObject(memHdc, stock);
                DeleteObject(dib);
                DeleteDC(memHdc);
    noBlt2:
                ;
            }
        }
    }
    else /* dest is a DDB */
    {
        /* DDB selected on dest DC, use X11 Driver */
        if(!physDevSrc || !physDevSrc->hasDIB)
        {
            /* source is null or has also a DDB, no need to convert anything */
            res = _DIBDRV_GetDisplayDriver()->pStretchBlt(physDevDst->X11PhysDev, xDst, yDst, widthDst, heightDst,
                                                      physDevSrc ? physDevSrc->X11PhysDev : 0, xSrc, ySrc, widthSrc, heightSrc, rop);
        }
        else
        {
            /* DIB on source, DDB on dest -- must convert source DIB to DDB and use X11 driver for blit */
            HBITMAP dib, ddb;
            
            /* clip blit area */
            if(physDevSrc)
            {
                RECT srcClip = {0, 0, physDevSrc->physBitmap.width, physDevSrc->physBitmap.height};
                res = StretchBlt_ClipAreas(&ps, &pd, &szSrc, &szDst, &srcClip, 0);
            }
            else
                res = TRUE;
            if(!res)
                goto noBlt3;
            xDst = pd.x; yDst = pd.y; widthDst = szDst.cx; heightDst = szDst.cy; 
            xSrc = ps.x; ySrc = ps.y; widthSrc = szSrc.cx; heightSrc = szSrc.cy;

            dib = SelectObject(physDevSrc->hdc, GetStockObject(DEFAULT_BITMAP));
            if(!dib)
            {
                ERR("Couldn't select out DIB from source HDC\n");
                res = 0;
                goto noBlt3;
            }
            ddb = _DIBDRV_ConvertDIBtoDDB(physDevSrc->hdc, dib, ySrc, heightSrc);
            if(!ddb)
            {
                ERR("Failed converting source DIB to DDB\n");
                SelectObject(physDevSrc->hdc, dib);
                res = 0;
                goto noBlt3;
            }
            if(!SelectObject(physDevSrc->hdc, ddb))
            {
                ERR("Failed to select converted DDB into source HDC\n");
                SelectObject(physDevSrc->hdc, dib);
                DeleteObject(ddb);
                res = 0;
                goto noBlt3;
            }
            res = _DIBDRV_GetDisplayDriver()->pStretchBlt(physDevDst->X11PhysDev, xDst, yDst, widthDst, heightDst,
                                                      physDevSrc ? physDevSrc->X11PhysDev : 0, xSrc, 0, widthSrc, heightSrc, rop);
            SelectObject(physDevSrc->hdc, dib);
            DeleteObject(ddb);
noBlt3:
            ;
        }
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_PatBlt
 */
BOOL DIBDRV_PatBlt( DIBDRVPHYSDEV *physDev, INT left, INT top, INT width, INT height, DWORD rop )
{
    BOOL res;

    MAYBE(TRACE("physDev:%p, left:%d, top:%d, width:%d, height:%d, rop:%06x\n", physDev, left, top, width, height, rop));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - use BitBlt by now\n"));
        res = DIBDRV_BitBlt(physDev, left, top, width, height, NULL, 0, 0, rop);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pPatBlt(physDev->X11PhysDev, left, top, width, height, rop);
    }
    return res;
}

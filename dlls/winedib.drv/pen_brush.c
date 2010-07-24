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


static const DASHPATTERN dashPatterns[4] =
{
    {2, {18, 6}},
    {2, {3,  3}},
    {4, {9, 6, 3, 6}},
    {6, {9, 3, 3, 3, 3, 3}}
};

static inline void OrderEndPoints(int *s, int *e)
{
    if(*s > *e)
    {
        int tmp;
        tmp = *s + 1;
        *s = *e + 1;
        *e = tmp;
    }
}

static void SolidPenHLine(DIBDRVPHYSDEV *physDev, int x1, int x2, int y)
{
    OrderEndPoints(&x1, &x2);
    physDev->physBitmap->funcs->SolidHLine(physDev->physBitmap, x1, x2, y, physDev->penAnd, physDev->penXor);
}

static void SolidPenVLine(DIBDRVPHYSDEV *physDev, int x, int y1, int y2)
{
    OrderEndPoints(&y1, &y2);
    physDev->physBitmap->funcs->SolidVLine(physDev->physBitmap, x, y1, y2, physDev->penAnd, physDev->penXor);
}

static void WINAPI SolidPenLineCallback(int x, int y, LPARAM lparam)
{
    DIBDRVPHYSDEV *physDev = (DIBDRVPHYSDEV *)lparam;

    physDev->physBitmap->funcs->SetPixel(physDev->physBitmap, x, y, physDev->penAnd, physDev->penXor);
    return;
}

static void SolidPenLine(DIBDRVPHYSDEV *physDev, int x1, int y1, int x2, int y2)
{
    LineDDA(x1, y1, x2, y2, SolidPenLineCallback, (LPARAM)physDev);
}

static inline void GetDashColors(DIBDRVPHYSDEV *physDev, DWORD *and, DWORD *xor)
{
    if(physDev->markSpace == mark)
    {
        *and = physDev->penAnd;
        *xor = physDev->penXor;
    }
    else if(GetBkMode(physDev->hdc) == OPAQUE)
    {
        *and = physDev->backgroundAnd;
        *xor = physDev->backgroundXor;
    }
    else
    {
        *and = 0xffffffff;
        *xor = 0;
    }
}

static inline void NextDash(DIBDRVPHYSDEV *physDev)
{
    if(physDev->leftInDash != 0)
        return;

    physDev->curDash++;
    if(physDev->curDash == physDev->penPattern->count)
        physDev->curDash = 0;
    physDev->leftInDash = physDev->penPattern->dashes[physDev->curDash];
    if(physDev->markSpace == mark)
        physDev->markSpace = space;
    else
        physDev->markSpace = mark;
}

static void DashedPenHLine(DIBDRVPHYSDEV *physDev, int x1, int x2, int y)
{
    int x = x1;
    DWORD and, xor;
    DWORD dashLen;

    if(x1 <= x2)
    {
        while(x != x2)
        {
            GetDashColors(physDev, &and, &xor);

            dashLen = physDev->leftInDash;
            if(x + dashLen > x2)
                dashLen = x2 - x;

            physDev->physBitmap->funcs->SolidHLine(physDev->physBitmap, x, x + dashLen, y, and, xor);
            x += dashLen;

            physDev->leftInDash -= dashLen;
            NextDash(physDev);
        }
    }
    else
    {
        while(x != x2)
        {
            GetDashColors(physDev, &and, &xor);

            dashLen = physDev->leftInDash;
            if(x - (int)dashLen < x2)
                dashLen = x - x2;

            physDev->physBitmap->funcs->SolidHLine(physDev->physBitmap, x - dashLen + 1, x + 1, y, and, xor);
            x -= dashLen;

            physDev->leftInDash -= dashLen;
            NextDash(physDev);
        }
    }
}

static void DashedPenVLine(DIBDRVPHYSDEV *physDev, int x, int y1, int y2)
{
    int y = y1;
    DWORD and, xor;
    DWORD dashLen;

    if(y1 <= y2)
    {
        while(y != y2)
        {
            GetDashColors(physDev, &and, &xor);

            dashLen = physDev->leftInDash;
            if(y + dashLen > y2)
                dashLen = y2 - y;

            physDev->physBitmap->funcs->SolidVLine(physDev->physBitmap, x, y, y + dashLen, and, xor);
            y += dashLen;

            physDev->leftInDash -= dashLen;
            NextDash(physDev);
        }
    }
    else
    {
        while(y != y2)
        {
            GetDashColors(physDev, &and, &xor);

            dashLen = physDev->leftInDash;
            if(y - (int)dashLen < y2)
                dashLen = y - y2;

            physDev->physBitmap->funcs->SolidVLine(physDev->physBitmap, x, y - dashLen + 1, y + 1, and, xor);
            y -= dashLen;

            physDev->leftInDash -= dashLen;
            NextDash(physDev);
        }
    }
}

static void WINAPI DashedPenLineCallback(int x, int y, LPARAM lparam)
{
    DIBDRVPHYSDEV *physDev = (DIBDRVPHYSDEV *)lparam;
    DWORD and, xor;

    GetDashColors(physDev, &and, &xor);

    physDev->physBitmap->funcs->SetPixel(physDev->physBitmap, x, y, and, xor);

    physDev->leftInDash--;
    NextDash(physDev);

    return;
}

static void DashedPenLine(DIBDRVPHYSDEV *physDev, int x1, int y1, int x2, int y2)
{
    LineDDA(x1, y1, x2, y2, DashedPenLineCallback, (LPARAM)physDev);
}

void _DIBDRV_ResetDashOrigin(DIBDRVPHYSDEV *physDev)
{
    physDev->curDash = 0;
    if(physDev->penPattern)
        physDev->leftInDash = physDev->penPattern->dashes[0];
    physDev->markSpace = mark;
}

#if 0
/* For 1bpp bitmaps, unless the selected foreground color exactly
   matches foreground's colortable OR it's the WHITE color,
   the background color is used -- tested on WinXP */
static DWORD AdjustFgColor(DIBDRVPHYSDEV *physDev, COLORREF color)
{
    RGBQUAD *back = physDev->physBitmap->colorTable;
    RGBQUAD *fore = physDev->physBitmap->colorTable+1;
    
    if(
      fore->rgbRed   == GetRValue(color) &&
      fore->rgbGreen == GetGValue(color) &&
      fore->rgbBlue  == GetBValue(color))
        return 1;
    else if(
      back->rgbRed   == GetRValue(color) &&
      back->rgbGreen == GetGValue(color) &&
      back->rgbBlue  == GetBValue(color))
        return 0;
    else if((color & 0x00ffffff) == 0x00ffffff)
        return 1;
    else
        return 0;
}

static void FixupFgColors1(DIBDRVPHYSDEV *physDev)
{
    int rop = GetROP2(physDev->hdc);

    physDev->penColor   = AdjustFgColor(physDev, physDev->penColorref);
    physDev->brushColor = AdjustFgColor(physDev, physDev->brushColorref);

    _DIBDRV_CalcAndXorMasks(rop, physDev->penColor, &physDev->penAnd, &physDev->penXor);
    _DIBDRV_CalcAndXorMasks(rop, physDev->brushColor, &physDev->brushAnd, &physDev->brushXor);
    HeapFree(GetProcessHeap(), 0, physDev->brushAnds);
    HeapFree(GetProcessHeap(), 0, physDev->brushXors);
    physDev->brushAnds = NULL;
    physDev->brushXors = NULL;
}
#endif

#if 0
/* For 1bpp bitmaps, unless the selected foreground color exactly
   matches one of the colors in the colortable, then the color that
   isn't the bkgnd color is used. */
static DWORD AdjustFgColor(DIBDRVPHYSDEV *physDev, COLORREF color)
{
    RGBQUAD rgb;
    int i;

    rgb.rgbRed   = GetRValue(color);
    rgb.rgbGreen = GetGValue(color);
    rgb.rgbBlue  = GetBValue(color);

    for(i = 0; i < physDev->physBitmap->colorTableSize; i++)
    {
        RGBQUAD *cur = physDev->physBitmap->colorTable + i;
        if((rgb.rgbRed == cur->rgbRed) && (rgb.rgbGreen == cur->rgbGreen) && (rgb.rgbBlue == cur->rgbBlue))
            return i;
    }
    return ~physDev->backgroundColor & 1;
}

static void FixupFgColors1(DIBDRVPHYSDEV *physDev)
{
    int rop = GetROP2(physDev->hdc);

    physDev->penColor   = AdjustFgColor(physDev, physDev->penColorref);
    physDev->brushColor = AdjustFgColor(physDev, physDev->brushColorref);

    _DIBDRV_CalcAndXorMasks(rop, physDev->penColor, &physDev->penAnd, &physDev->penXor);
    _DIBDRV_CalcAndXorMasks(rop, physDev->brushColor, &physDev->brushAnd, &physDev->brushXor);
    HeapFree(GetProcessHeap(), 0, physDev->brushAnds);
    HeapFree(GetProcessHeap(), 0, physDev->brushXors);
    physDev->brushAnds = NULL;
    physDev->brushXors = NULL;
}
#endif

static void SolidBrushHLine(DIBDRVPHYSDEV *physDev, int x1, int x2, int y)
{
    OrderEndPoints(&x1, &x2);
    physDev->physBitmap->funcs->SolidHLine(physDev->physBitmap, x1, x2, y, physDev->brushAnd, physDev->brushXor);
}


static void GenerateMasks(DIBDRVPHYSDEV *physDev, DIBDRVBITMAP *bmp, DWORD **and, DWORD **xor)
{
    int rop = GetROP2(physDev->hdc);
    DWORD *color_ptr, *and_ptr, *xor_ptr;
    DWORD size = bmp->height * abs(bmp->stride);

    *and = HeapAlloc(GetProcessHeap(), 0, size);
    *xor = HeapAlloc(GetProcessHeap(), 0, size);

    color_ptr = bmp->bits;
    and_ptr = *and;
    xor_ptr = *xor;

    while(size)
    {
        _DIBDRV_CalcAndXorMasks(rop, *color_ptr++, and_ptr++, xor_ptr++);
        size -= 4;
    }
}

static void PatternBrushHLine(DIBDRVPHYSDEV *physDev, int x1, int x2, int y)
{
    DWORD *and, *xor, brushY = y % physDev->brushBitmap->height;

    if(!physDev->brushAnds)
        GenerateMasks(physDev, physDev->brushBitmap, &physDev->brushAnds, &physDev->brushXors);

    OrderEndPoints(&x1, &x2);
    and = (DWORD *)((char *)physDev->brushAnds + brushY * physDev->brushBitmap->stride);
    xor = (DWORD *)((char *)physDev->brushXors + brushY * physDev->brushBitmap->stride);

    physDev->physBitmap->funcs->PatternHLine(physDev->physBitmap, x1, x2, y, and, xor, physDev->brushBitmap->width, x1 % physDev->brushBitmap->width);
}

/* null function for PS_NULL and BS_NULL pen and brush styles */
void NullPenHLine(DIBDRVPHYSDEV *physDev, int x1, int x2, int y) {}
void NullPenVLine(DIBDRVPHYSDEV *physDev, int x, int y1, int y2) {}
void NullPenLine(DIBDRVPHYSDEV *physDev, int x1, int y1, int x2, int y2) {}
void NullBrushHLine(DIBDRVPHYSDEV *physDev, int x1, int x2, int y) {}

/***********************************************************************
 *           DIBDRV_SelectPen
 */
HPEN DIBDRV_SelectPen( DIBDRVPHYSDEV *physDev, HPEN hpen )
{
    HPEN res;
    LOGPEN logpen;
    
    MAYBE(TRACE("physDev:%p, hpen:%p\n", physDev, hpen));

    if(physDev->hasDIB)
    {
        GetObjectW(hpen, sizeof(logpen), &logpen);

        physDev->penColorref = logpen.lopnColor;
        physDev->penColor = physDev->physBitmap->funcs->ColorToPixel(
          physDev->physBitmap,
          _DIBDRV_MapColor(physDev, physDev->penColorref));

        _DIBDRV_CalcAndXorMasks(GetROP2(physDev->hdc), physDev->penColor, &physDev->penAnd, &physDev->penXor);

        physDev->penStyle = logpen.lopnStyle;
        switch(logpen.lopnStyle)
        {
            default:
                ONCE(FIXME("Unhandled pen style %d\n", logpen.lopnStyle));
                physDev->penStyle = PS_SOLID;
                /* fall through */
            case PS_SOLID:
                physDev->penHLine = SolidPenHLine;
                physDev->penVLine = SolidPenVLine;
                physDev->penLine  = SolidPenLine;
                physDev->penPattern = NULL;
                break;

            case PS_DASH:
            case PS_DOT:
            case PS_DASHDOT:
            case PS_DASHDOTDOT:
                physDev->penHLine = DashedPenHLine;
                physDev->penVLine = DashedPenVLine;
                physDev->penLine  = DashedPenLine;
                physDev->penPattern = &dashPatterns[logpen.lopnStyle - PS_DASH];
                _DIBDRV_ResetDashOrigin(physDev);
                break;
            case PS_NULL:
                physDev->penHLine = NullPenHLine;
                physDev->penVLine = NullPenVLine;
                physDev->penLine  = NullPenLine;
                physDev->penPattern = NULL;
                break;
        }
        res = hpen;
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
    
    MAYBE(TRACE("physDev:%p, crColor:%x\n", physDev, crColor));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = crColor;
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
    HBRUSH res = hbrush;
    LOGBRUSH logbrush;

    
    MAYBE(TRACE("physDev:%p, hbrush:%p\n", physDev, hbrush));

    if(physDev->hasDIB)
    {
        GetObjectW(hbrush, sizeof(logbrush), &logbrush);

        /* frees any currently selected DIB brush and cache */
        _DIBDRVBITMAP_Free(physDev->brushBitmap);
        physDev->brushBitmap = NULL;
        _DIBDRVBITMAP_Free(physDev->brushBmpCache);
        physDev->brushBmpCache = NULL;
        if(physDev->brushAnds)
        {
            HeapFree(GetProcessHeap(), 0, physDev->brushAnds);
            HeapFree(GetProcessHeap(), 0, physDev->brushXors);
        }
        physDev->brushAnds = NULL;
        physDev->brushXors = NULL;

        switch (logbrush.lbStyle)
        {
            default:
                FIXME("Unhandled brush style %d\n", logbrush.lbStyle);
                physDev->brushColorref = 0;
                goto solid;
                
            case BS_SOLID:
                physDev->brushColorref = logbrush.lbColor;
    solid:
                MAYBE(TRACE("SOLID Pattern -- color is %x\n", physDev->brushColorref));
                physDev->brushStyle = BS_SOLID;
                physDev->brushHLine = SolidBrushHLine;

                physDev->brushColor = physDev->physBitmap->funcs->ColorToPixel(
                  physDev->physBitmap,
                  _DIBDRV_MapColor(physDev, physDev->brushColorref));

                _DIBDRV_CalcAndXorMasks(physDev->rop2, physDev->brushColor,
                                           &physDev->brushAnd, &physDev->brushXor);

                /* set the physDev brush style */
                physDev->brushStyle = BS_SOLID;
                physDev->isBrushBitmap = FALSE;
                
                break;

            case BS_DIBPATTERN8X8:
            case BS_DIBPATTERN:
            {
                DIBDRVBITMAP src;
                BITMAPINFO *bmi;
                
                FIXME("DIB Pattern\n");
                
                /* if no DIB selected in, fallback to null brush */
                if(!physDev->physBitmap->bits)
                {
                    physDev->brushColorref = 0;
                    goto solid;
                }

                /* gets brush DIB's pointer */
                bmi = GlobalLock16(logbrush.lbHatch);
        
                /* initializes a temporary DIB with brush's one */
                if(!_DIBDRVBITMAP_InitFromBitmapinfo(&src, bmi, NULL))
                {
                    ERR("Failed to initialize brush DIB\n");
                    res = 0;
                    goto err;
                }
                
                /* converts brush bitmap to match currently selected one's format */
                if(!_DIBDRVBITMAP_Convert(physDev->brushBitmap, &src, physDev->physBitmap))
                {
                    ERR("Failed to convert brush DIB\n");
                    _DIBDRVBITMAP_Free(&src);
                    res = 0;
                    goto err;
                }

                /* frees temporary DIB's data */
                _DIBDRVBITMAP_Free(&src);
                
                /* use DIB pattern for brush lines */
                physDev->brushHLine = PatternBrushHLine;

    err:            
                /* frees brush's DIB pointer */
                GlobalUnlock16(logbrush.lbHatch);
                
                break;
            }
            case BS_DIBPATTERNPT:
                FIXME("BS_DIBPATTERNPT not supported\n");
                physDev->brushColorref = 0;
                goto solid;
                
            case BS_HATCHED:
                FIXME("BS_HATCHED not supported\n");
                physDev->brushColorref = 0;
                goto solid;
                
            case BS_NULL:
            {
                MAYBE(TRACE("NULL Pattern\n"));
                physDev->brushColorref = 0;
                physDev->brushColor = physDev->physBitmap->funcs->ColorToPixel(
                  physDev->physBitmap,
                  _DIBDRV_MapColor(physDev, 0));
                physDev->brushHLine = NullBrushHLine;
                break;
            }
                
            case BS_PATTERN:
            case BS_PATTERN8X8:
                FIXME("BS_PATTERN not supported\n");
                physDev->brushColorref = 0;
                goto solid;
        }

        MAYBE(TRACE("END\n"));
        return hbrush;
    }
    else
    {
        /* DDB selected in, use X11 driver */
 
        /* we must check if a DIB pattern is requested */       
        GetObjectW(hbrush, sizeof(logbrush), &logbrush);
        switch (logbrush.lbStyle)
        {
            case BS_DIBPATTERN8X8:
            case BS_DIBPATTERN:
            case BS_DIBPATTERNPT:
                FIXME("A DIB pattern was requested for a DDB bitmap\n");
                break;

            case BS_SOLID:
            case BS_HATCHED:
            case BS_NULL:
            case BS_PATTERN:
            case BS_PATTERN8X8:
            default:
                break;
        }
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
    
    MAYBE(TRACE("physDev:%p, crColor:%x\n", physDev, crColor));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = crColor;
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
int DIBDRV_SetROP2( DIBDRVPHYSDEV *physDev, int rop )
{
    int prevRop;
    
    MAYBE(TRACE("physDev:%p, rop:%x\n", physDev, rop));

    prevRop = physDev->rop2;
    physDev->rop2 = rop;

    if(prevRop != rop)
    {
        _DIBDRV_CalcAndXorMasks(rop, physDev->penColor,   &physDev->penAnd,   &physDev->penXor);
        _DIBDRV_CalcAndXorMasks(rop, physDev->brushColor, &physDev->brushAnd, &physDev->brushXor);
        _DIBDRV_CalcAndXorMasks(rop, physDev->backgroundColor, &physDev->backgroundAnd, &physDev->backgroundXor);
        if(physDev->brushAnds)
        {
            HeapFree(GetProcessHeap(), 0, physDev->brushAnds);
            HeapFree(GetProcessHeap(), 0, physDev->brushXors);
        }
        physDev->brushAnds = NULL;
        physDev->brushXors = NULL;
    }

    return prevRop;
    /* note : X11 Driver don't have SetROP2() function exported */
}

/***********************************************************************
 *           SetBkColor
 */
COLORREF DIBDRV_SetBkColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    COLORREF res;
    
    MAYBE(TRACE("physDev:%p, color:%x\n", physDev, color));

    if(physDev->hasDIB)
    {
        physDev->backgroundColor = _DIBDRV_MapColor(physDev, color);
        physDev->backgroundColor = physDev->physBitmap->funcs->ColorToPixel(physDev->physBitmap, physDev->backgroundColor);

        _DIBDRV_CalcAndXorMasks(physDev->rop2, physDev->backgroundColor, &physDev->backgroundAnd, &physDev->backgroundXor);
        
        res = TRUE;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetBkColor(physDev->X11PhysDev, color);
    }
    return res;
}

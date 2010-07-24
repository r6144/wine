/*
 * DIB Engine color Primitives
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

/* ------------------------------------------------------------*/
/*                     BITFIELD HELPERS                        */
static inline DWORD PutField32(DWORD field, int shift, int len)
{
    shift = shift - (8 - len);
    if (len <= 8)
        field &= (((1 << len) - 1) << (8 - len));
    if (shift < 0)
        field >>= -shift;
    else
        field <<= shift;
    return field;
}

static inline WORD PutField16(WORD field, int shift, int len)
{
    shift = shift - (8 - len);
    if (len <= 8)
        field &= (((1 << len) - 1) << (8 - len));
    if (shift < 0)
        field >>= -shift;
    else
        field <<= shift;
    return field;
}

/* ------------------------------------------------------------*/
/*                     COLOR FUNCTIONS                         */
DWORD _DIBDRV_ColorToPixel32_RGB(const DIBDRVBITMAP *dib, COLORREF color)
{
    return ( ((color >> 16) & 0xff) | (color & 0xff00) | ((color << 16) & 0xff0000) );
}

DWORD _DIBDRV_ColorToPixel32_BITFIELDS(const DIBDRVBITMAP *dib, COLORREF color)
{
    DWORD r,g,b;

    r = GetRValue(color);
    g = GetGValue(color);
    b = GetBValue(color);

    return PutField32(r, dib->redShift,   dib->redLen) |
           PutField32(g, dib->greenShift, dib->greenLen) |
           PutField32(b, dib->blueShift,  dib->blueLen);
}

DWORD _DIBDRV_ColorToPixel24(const DIBDRVBITMAP *dib, COLORREF color)
{
    return ( ((color >> 16) & 0xff) | (color & 0xff00) | ((color << 16) & 0xff0000) );
}

DWORD _DIBDRV_ColorToPixel16_RGB(const DIBDRVBITMAP *dib, COLORREF color)
{
    return ( ((color >> 19) & 0x001f) | ((color >> 6) & 0x03e0) | ((color << 7) & 0x7c00) );
}

DWORD _DIBDRV_ColorToPixel16_BITFIELDS(const DIBDRVBITMAP *dib, COLORREF color)
{
    DWORD r,g,b;

    r = GetRValue(color);
    g = GetGValue(color);
    b = GetBValue(color);

    return PutField16(r, dib->redShift,   dib->redLen) |
           PutField16(g, dib->greenShift, dib->greenLen) |
           PutField16(b, dib->blueShift,  dib->blueLen);
}

/* gets nearest color to DIB palette color */
DWORD _DIBDRV_GetNearestColor(const DIBDRVBITMAP *dib, COLORREF color)
{
    RGBQUAD *c;

    if(dib->bitCount > 8)
        return color;
        
    c = dib->colorTable + _DIBDRV_GetNearestColorIndex(dib, color);
    return RGB(c->rgbRed, c->rgbGreen, c->rgbBlue);

}

/* gets nearest color index in DIB palette of a given colorref */
DWORD _DIBDRV_GetNearestColorIndex(const DIBDRVBITMAP *dib, COLORREF color)
{
    DWORD r, g, b;
    int i, best_index = 0;
    DWORD diff, best_diff = 0xffffffff;
    
    r = GetRValue(color);
    g = GetGValue(color);
    b = GetBValue(color);
    
    for(i = 0; i < dib->colorTableSize; i++)
    {
        RGBQUAD *cur = dib->colorTable + i;
        diff = (r - cur->rgbRed)   * (r - cur->rgbRed)
            +  (g - cur->rgbGreen) * (g - cur->rgbGreen)
            +  (b - cur->rgbBlue)  * (b - cur->rgbBlue);

        if(diff == 0)
        {
            best_index = i;
            break;
        }

        if(diff < best_diff)
        {
            best_diff = diff;
            best_index = i;
        }
    }
    return best_index;
}

DWORD _DIBDRV_ColorToPixelColortable(const DIBDRVBITMAP *dib, COLORREF color)
{
    /* just in case it's being called without color table
       properly initialized */
    if(!dib->colorTableGrabbed)
        return 0;
        
    color &= 0xffffff;
        
    /* for monochrome bitmaps, color is :
           foreground if matching foreground ctable
           else background if matching background ctable
           else foreground if 0xffffff
           else background */
    if(dib->colorTableSize == 2)
    {
        RGBQUAD *back = dib->colorTable;
        RGBQUAD *fore = dib->colorTable + 1;
        COLORREF backColorref = RGB(back->rgbRed, back->rgbGreen, back->rgbBlue);
        COLORREF foreColorref = RGB(fore->rgbRed, fore->rgbGreen, fore->rgbBlue);
        if(color == foreColorref)
            return 1;
        else if(color == backColorref)
            return 0;
        else if(color == 0xffffff)
            return dib->lightColor;
        else
            return 0;
    }
    
    /* otherwise looks for nearest color in palette */
    return _DIBDRV_GetNearestColorIndex(dib, color);
}

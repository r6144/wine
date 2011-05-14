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

DWORD _DIBDRV_ColorToPixelColortable(const DIBDRVBITMAP *dib, COLORREF color)
{
    int i, best_index = 0;
    DWORD r, g, b;
    DWORD diff, best_diff = 0xffffffff;

    r = GetRValue(color);
    g = GetGValue(color);
    b = GetBValue(color);
    
    /* just in case it's being called without color table
       properly initialized */
    if(!dib->colorTableGrabbed)
        return 0;
        
    /* for monochrome bitmaps, color is background
       if not matching foreground */
    if(dib->colorTableSize == 2)
    {
        RGBQUAD *fore = dib->colorTable + 1;
        if(r == fore->rgbRed && g == fore->rgbGreen && b == fore->rgbBlue)
            return 1;
        return 0;
    }

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

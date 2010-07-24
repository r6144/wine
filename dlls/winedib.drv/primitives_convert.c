/*
 * DIB Engine conversions Primitives
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

static inline COLORREF SwapColors(DWORD c)
{
    return ((c & 0x0000ff) << 16) | (c & 0x00ff00) | ((c & 0xff0000) >> 16);
    
}

static inline DWORD PlaceField32(BYTE c, int shift, int len)
{
    DWORD res = c;
    if(len < 8)
        res >>= (8 - len);
    else
        res <<= (len - 8);
    return res << shift;
}

static inline WORD PlaceField16(BYTE c, int shift, int len)
{
    WORD res = c;
    if(len < 8)
        res >>= (8 - len);
    else
        res <<= (len - 8);
    return res << shift;
}

static inline BYTE GetField32(DWORD dwColor, int shift, int len)
{
    dwColor = dwColor & (((1 << (len)) - 1) << shift);
    dwColor = dwColor << (32 - (shift + len)) >> 24;
    return dwColor;
}

/* ----------------------------------------------------------------*/
/*                         CONVERT PRIMITIVES                      */
/* converts (part of) line of any DIB format from/to DIB32_RGB one */
BOOL _DIBDRV_GetLine32_RGB(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    DWORD *src;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        dwBuf -= startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = (DWORD *)((BYTE *)bmp->bits + line * bmp->stride + 4 * startx);
    for(; width; width--)
        *dwBuf++ = *src++;
    return TRUE;
}

BOOL _DIBDRV_GetLine32_BITFIELDS(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    DWORD *src;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        bBuf -= 4 * startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = (DWORD *)((BYTE *)bmp->bits + line * bmp->stride + 4 * startx);
    for(; width ; width--)
    {
        *dwBuf++ =
            GetField32(*src, bmp->redShift  , bmp->redLen  ) << 16 |
            GetField32(*src, bmp->greenShift, bmp->greenLen) <<  8 |
            GetField32(*src, bmp->blueShift , bmp->blueLen );
        src++;
    }
    return TRUE;
}

BOOL _DIBDRV_GetLine24(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    BYTE *bBuf = (BYTE *)buf;
    BYTE *src;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        bBuf -= 4 * startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = ((BYTE *)bmp->bits + line * bmp->stride + 3 * startx);
    for(; width ; width--)
    {
        *bBuf++ = *src++;
        *bBuf++ = *src++;
        *bBuf++ = *src++;
        *bBuf++ = 0x0;
    }
    return TRUE;
}

BOOL _DIBDRV_GetLine16_RGB(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    WORD *src;
    DWORD b;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        dwBuf -= startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = (WORD *)((BYTE *)bmp->bits + line * bmp->stride + 2 * startx);
    for(; width ; width--)
    {
        b = *src++;
        /* 0RRR|RRGG|GGGB|BBBB */
        *dwBuf++ = ((b & 0x1f) << 3) | ((b & 0x3e0) << 6) | ((b & 0x7c00) << 9);
    }
    return TRUE;
}

BOOL _DIBDRV_GetLine16_BITFIELDS(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    WORD *src;
    DWORD b;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        dwBuf -= startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = (WORD *)((BYTE *)bmp->bits + line * bmp->stride + 2 * startx);
    for(; width ; width--)
    {
        b = *src++;

        *dwBuf++ =((( b & bmp->blueMask) >> bmp->blueShift ) << ( 8 - bmp->blueLen )) |
                  (((b & bmp->greenMask) >> bmp->greenShift) << (16 - bmp->greenLen)) |
                  (((b & bmp->redMask  ) >> bmp->redShift  ) << (24 - bmp->redLen  ));
    }
    return TRUE;
}

BOOL _DIBDRV_GetLine8(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *src;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        dwBuf -= startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = ((BYTE *)bmp->bits + line * bmp->stride + startx);
    for(; width ; width--)
        *dwBuf++ = *((DWORD *)bmp->colorTable + *src++);
    return TRUE;
}

BOOL _DIBDRV_GetLine4(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *src;
    
#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        dwBuf -= startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
    if(!bmp->colorTable)
    {
        ERR("Called with uninitialized color table\n");
        return FALSE;
    }
#endif

    src = ((BYTE *)bmp->bits + line * bmp->stride + (startx >> 1));
    /* if startx is odd, get first nibble */
    if(startx & 0x01)
    {
        *dwBuf++ = *((DWORD *)bmp->colorTable + (*src++ & 0x0f));
        width--;
    }
    
    /* then gets all full image bytes */
    for( ; width > 1 ; width -= 2)
    {
        *dwBuf++ = *((DWORD *)bmp->colorTable + ((*src >> 4) & 0x0f));
        *dwBuf++ = *((DWORD *)bmp->colorTable + (*src++ & 0x0f));
    }
    
    /* last nibble, if any */
    if(width)
        *dwBuf++ = *((DWORD *)bmp->colorTable + ((*src >> 4) & 0x0f));
    return TRUE;
}

BOOL _DIBDRV_GetLine1(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *src;
    BYTE b;
    char i;
    DWORD pixOn  = *((DWORD *)bmp->colorTable + 1);
    DWORD pixOff = *(DWORD *)bmp->colorTable;

#ifdef DIBDRV_CHECK_RANGES
    /* range check */
    if(line < 0 || line >= bmp->height)
        return FALSE;
    if(startx < 0)
    {
        width += startx;
        dwBuf -= startx; 
        startx = 0;
    }
    if(startx + width > bmp->width)
        width = bmp->width - startx;
    if(width <= 0)
        return FALSE;
#endif

    src = ((BYTE *)bmp->bits + line * bmp->stride + (startx >> 3));
    /* get first partial byte, if any */
    startx = (8 - (startx & 0x07)) & 0x07;
    width -= startx;
    if(startx)
    {
        b = *src++ << (8 - startx);
        while(startx--)
        {
            if(b & 0x80)
                *dwBuf++ = pixOn;
            else
                *dwBuf++ = pixOff;
            b <<= 1;
        }
    }
    
    /* then gets full next bytes */
    for( ; width > 7 ; width -= 8)
    {
        b = *src++;
        for(i = 0 ; i < 8 ; i++)
        {
            if(b & 0x80)
                *dwBuf++ = pixOn;
            else
                *dwBuf++ = pixOff;
            b <<= 1;
        }
    }
    
    /* last partial byte, if any */
    if(width > 0)
    {
        b = *src;
        while(width--)
        {
            if(b & 0x80)
                *dwBuf++ = pixOn;
            else
                *dwBuf++ = pixOff;
            b <<= 1;
        }
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine32_RGB(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    DWORD *dst = (DWORD *)((BYTE *)bmp->bits + line * bmp->stride + 4 * startx);
    for(; width; width--)
        *dst++ = *dwBuf++;
    return TRUE;
}

BOOL _DIBDRV_PutLine32_BITFIELDS(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    DWORD *dst = (DWORD *)((BYTE *)bmp->bits + line * bmp->stride + 4 * startx);
    RGBQUAD *c;
    for(; width; width--)
    {
        c = (RGBQUAD *)dwBuf++;
        *dst++ =
            PlaceField32(c->rgbRed  , bmp->redShift  , bmp->redLen  ) |
            PlaceField32(c->rgbGreen, bmp->greenShift, bmp->greenLen) |
            PlaceField32(c->rgbBlue , bmp->blueShift , bmp->blueLen );
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine24(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *dst = ((BYTE *)bmp->bits + line * bmp->stride + 3 * startx);
    DWORD c;
    for(; width; width--)
    {
        c = *dwBuf++;
        *dst++ = c &  0x000000ff;
        *dst++ = (c & 0x0000ff00) >>  8;
        *dst++ = (c & 0x00ff0000) >> 16;
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine16_RGB(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    WORD *dst = (WORD *)((BYTE *)bmp->bits + line * bmp->stride + 2 * startx);
    DWORD c;
    for(; width; width--)
    {
        c = *dwBuf++;
        *dst++ =
        /* 0RRR|RRGG|GGGB|BBBB */
            ((c & 0x000000f8) >> 3) |
            ((c & 0x0000f800) >> 6) |
            ((c & 0x00f80000) >> 9);
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine16_BITFIELDS(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    WORD *dst = (WORD *)((BYTE *)bmp->bits + line * bmp->stride + 2 * startx);
    DWORD c;

    BYTE bShift =  8 - bmp->blueLen;
    BYTE gShift = 16 - bmp->greenLen;
    BYTE rShift = 24 - bmp->redLen;
    for(; width; width--)
    {
        c = *dwBuf++;
        *dst++ =
            ((((c & 0x000000ff) >> bShift) << bmp->blueShift)  & bmp->blueMask) |
            ((((c & 0x0000ff00) >> gShift) << bmp->greenShift) & bmp->greenMask) |
            ((((c & 0x00ff0000) >> rShift) << bmp->redShift)   & bmp->redMask);
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine8(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *dst = ((BYTE *)bmp->bits + line * bmp->stride + startx);
    DWORD c;
    DWORD last_color = 0xffffffff;
    int last_index = -1;

    for(; width; width--)
    {
        c = *dwBuf++;
        
        /* slight optimization, as images often have many
           consecutive pixels with same color */
        if(last_index == -1 || c != last_color)
        {
            last_index = bmp->funcs->ColorToPixel(bmp, SwapColors(c));
            last_color = c;
        }
        *dst++ = last_index;
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine4(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *dst = ((BYTE *)bmp->bits + line * bmp->stride + (startx >> 1));
    DWORD c;
    DWORD last_color = 0xffffffff;
    int last_index = -1;
    
    /* if startx is odd, put first nibble */
    if(startx & 0x01)
    {
        c = *dwBuf++;
        
        last_index = bmp->funcs->ColorToPixel(bmp, SwapColors(c));
        last_color = c;
        *dst = (*dst & 0xf0) | last_index;
        dst++;
        width--;
    }
    
    /* then gets all full image bytes */
    for( ; width > 1 ; width -= 2)
    {
        c = *dwBuf++;
        
        /* slight optimization, as images often have many
           consecutive pixels with same color */
        if(last_index == -1 || c != last_color)
        {
            last_index = bmp->funcs->ColorToPixel(bmp, SwapColors(c));
            last_color = c;
        }
        *dst = last_index << 4;
        
        c = *dwBuf++;
        
        /* slight optimization, as images often have many
           consecutive pixels with same color */
        if(last_index == -1 || c != last_color)
        {
            last_index = bmp->funcs->ColorToPixel(bmp, SwapColors(c));
            last_color = c;
        }
        *dst++ |= last_index;
    }
    
    /* last nibble, if any */
    if(width > 0)
    {
        c = *dwBuf;
        
        /* slight optimization, as images often have many
           consecutive pixels with same color */
        if(last_index == -1 || c != last_color)
            last_index = bmp->funcs->ColorToPixel(bmp, SwapColors(c));
        *dst = (*dst & 0x0f) | (last_index << 4);
    }
    return TRUE;
}

BOOL _DIBDRV_PutLine1(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf)
{
    DWORD *dwBuf = (DWORD *)buf;
    BYTE *dst = ((BYTE *)bmp->bits + line * bmp->stride + (startx >> 3));
    BYTE b, mask;
    char i;
    DWORD c;

    /* get foreground color */
    DWORD back = *(DWORD *)bmp->colorTable       & 0x00ffffff;
    DWORD fore = *((DWORD *)bmp->colorTable + 1) & 0x00ffffff;
    
    /* get 'light' color */
    int lightColor = bmp->lightColor;

    /* put first partial byte, if any */
    startx &= 0x07;
    mask = 0x80 >> startx;
    startx = (8 - startx) & 0x07;
    if(startx)
    {
        width -= startx;
        b = *dst;
        while(startx--)
        {
            c = *dwBuf++ & 0x00ffffff;
            if(c == fore)
                b |= mask;
            else if(c == back)
                b &= !mask;
            else if((c == 0x00ffffff && lightColor) || (c == 0 && !lightColor))
                b |= mask;
            else
                b &= !mask;
            mask >>= 1;
        }
        *dst++ = b; 
    }
    
    /* then puts full next bytes */
    for( ; width > 7 ; width -= 8)
    {
        b = 0;
        mask = 0x80;
        for(i = 0 ; i < 8 ; i++)
        {
            c = *dwBuf++ & 0x00ffffff;
            if(c == fore || (c == 0x00ffffff && c != back && lightColor) || (c == 0 && !lightColor))
                b |= mask;
            mask >>= 1;
        }
        *dst++ = b;
    }
    
    /* last partial byte, if any */
    if(width > 0)
    {
        b = *dst;
        mask = 0x80;
        while(width--)
        {
            c = *dwBuf++ & 0x00ffffff;
            if(c == fore)
                b |= mask;
            else if(c == back)
                b &= !mask;
            else if((c == 0x00ffffff && lightColor) || (c == 0 && !lightColor))
                b |= mask;
            else
                b &= !mask;
            mask >>= 1;
        }
        *dst = b;
    }
    return TRUE;
}

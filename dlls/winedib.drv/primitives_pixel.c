/*
 * DIB Engine pixel Primitives
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
static inline DWORD GetField32 (DWORD pixel, int shift, int len)
{
    pixel = pixel & (((1 << (len)) - 1) << shift);
    pixel = pixel << (32 - (shift + len)) >> 24;
    return pixel;
}

/* ------------------------------------------------------------*/
/*                     PIXEL POINTER READING                   */
void *_DIBDRV_GetPixelPointer32(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->bits;

    ptr += (y * dib->stride);

    ptr += x * 4;
    return ptr;
}

void *_DIBDRV_GetPixelPointer24(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->bits;

    ptr += (y * dib->stride);

    ptr += x * 3;
    return ptr;
}

void *_DIBDRV_GetPixelPointer16(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->bits;

    ptr += (y * dib->stride);

    ptr += x * 2;
    return ptr;
}

void *_DIBDRV_GetPixelPointer8(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->bits;

    ptr += (y * dib->stride);

    ptr += x;
    return ptr;
}

void *_DIBDRV_GetPixelPointer4(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->bits;

    ptr += (y * dib->stride);

    ptr += x / 2;
    return ptr;
}

void *_DIBDRV_GetPixelPointer1(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->bits;

    ptr += (y * dib->stride);

    ptr += x / 8;
    return ptr;
}

/* ------------------------------------------------------------*/
/*                     PIXEL WRITING                           */
void _DIBDRV_SetPixel32(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor)
{
    DWORD *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    _DIBDRV_rop32(ptr, and, xor);
}

void _DIBDRV_SetPixel24(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    _DIBDRV_rop8(ptr,      and        & 0xff,  xor        & 0xff);
    _DIBDRV_rop8(ptr + 1, (and >> 8)  & 0xff, (xor >> 8)  & 0xff);
    _DIBDRV_rop8(ptr + 2, (and >> 16) & 0xff, (xor >> 16) & 0xff);
}

void _DIBDRV_SetPixel16(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor)
{
    WORD *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    _DIBDRV_rop16(ptr, and, xor);
}

void _DIBDRV_SetPixel8(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    _DIBDRV_rop8(ptr, and, xor);
}

void _DIBDRV_SetPixel4(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    BYTE byte_and, byte_xor;

    if(x & 1) /* upper nibble untouched */
    {
        byte_and = (and & 0xf) | 0xf0;
        byte_xor = (xor & 0xf);
    }
    else
    {
        byte_and = ((and << 4) & 0xf0) | 0x0f;
        byte_xor = ((xor << 4) & 0xf0);
    }

    _DIBDRV_rop8(ptr, byte_and, byte_xor);
}

void _DIBDRV_SetPixel1(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor)
{
    BYTE *ptr;
    BYTE byte_and = 0, byte_xor = 0, mask;

    if(and & 1) byte_and = 0xff;
    if(xor & 1) byte_xor = 0xff;

    mask = 1 << (7 - (x & 7));

    byte_and |= ~mask;
    byte_xor &= mask;

    ptr = dib->funcs->GetPixelPointer(dib, x, y);

    _DIBDRV_rop8(ptr, byte_and, byte_xor);
}

/* ------------------------------------------------------------*/
/*                     PIXEL READING                           */
DWORD _DIBDRV_GetPixel32_RGB(const DIBDRVBITMAP *dib, int x, int y)
{
    DWORD c = *(DWORD *)dib->funcs->GetPixelPointer(dib, x, y);
    return
        ((c & 0x000000ff) << 16) |
        ((c & 0x0000ff00)      ) |
        ((c & 0x00ff0000) >> 16) |
        ( c & 0xff000000       );   /* last one for alpha channel */
}

DWORD _DIBDRV_GetPixel32_BITFIELDS(const DIBDRVBITMAP *dib, int x, int y)
{
    DWORD *ptr = dib->funcs->GetPixelPointer(dib, x, y);

    return GetField32(*ptr, dib->redShift,   dib->redLen)         |
           GetField32(*ptr, dib->greenShift, dib->greenLen) <<  8 |
           GetField32(*ptr, dib->blueShift,  dib->blueLen)  << 16;
}

DWORD _DIBDRV_GetPixel24(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    return ((WORD)ptr[0] << 16) | ((WORD)ptr[1] << 8) | (WORD)ptr[2];
}

DWORD _DIBDRV_GetPixel16_RGB(const DIBDRVBITMAP *dib, int x, int y)
{
    WORD c = *(WORD *)dib->funcs->GetPixelPointer(dib, x, y);
   /* 0RRR|RRGG|GGGB|BBBB */
   return ((c & 0x7c00) >> 7) | ((c & 0x03e0) << 6) | ((c & 0x001f) << 19);
}

DWORD _DIBDRV_GetPixel16_BITFIELDS(const DIBDRVBITMAP *dib, int x, int y)
{
    WORD c = *(WORD *)dib->funcs->GetPixelPointer(dib, x, y);

    return (((c & dib->blueMask ) >> dib->blueShift ) << (24 - dib->blueLen )) |
           (((c & dib->greenMask) >> dib->greenShift) << (16 - dib->greenLen)) |
           (((c & dib->redMask  ) >> dib->redShift  ) << ( 8 - dib->redLen  ));
}

DWORD _DIBDRV_GetPixel8(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y);
    RGBQUAD *color = dib->colorTable + *ptr;
    return (color->rgbRed) | (color->rgbGreen << 8) | (color->rgbBlue << 16);
}

DWORD _DIBDRV_GetPixel4(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y), pix;
    RGBQUAD *color;

    if(x & 1)
        pix = *ptr & 0x0f;
    else
        pix = *ptr >> 4;

    color = dib->colorTable + pix;
    return (color->rgbRed) | (color->rgbGreen << 8) | (color->rgbBlue << 16);
}

DWORD _DIBDRV_GetPixel1(const DIBDRVBITMAP *dib, int x, int y)
{
    BYTE *ptr = dib->funcs->GetPixelPointer(dib, x, y), pix;
    RGBQUAD *color;

    pix = *ptr;

    pix >>= (7 - (x & 7));
    pix &= 1;

    color = dib->colorTable + pix;
    return (color->rgbRed) | (color->rgbGreen << 8) | (color->rgbBlue << 16);
}

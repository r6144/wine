/*
 * DIB Engine line Primitives
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
/*                     HORIZONTAL LINES                        */
void _DIBDRV_SolidHLine32(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor)
{
    DWORD *ptr;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    for(i = start; i < end; i++)
        _DIBDRV_rop32(ptr++, and, xor);
}

void _DIBDRV_SolidHLine24(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;
    BYTE and_bytes[3], xor_bytes[3];

    and_bytes[0] =  and        & 0xff;
    and_bytes[1] = (and >> 8)  & 0xff;
    and_bytes[2] = (and >> 16) & 0xff;
    xor_bytes[0] =  xor        & 0xff;
    xor_bytes[1] = (xor >> 8)  & 0xff;
    xor_bytes[2] = (xor >> 16) & 0xff;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr++, and_bytes[0], xor_bytes[0]);
        _DIBDRV_rop8(ptr++, and_bytes[1], xor_bytes[1]);
        _DIBDRV_rop8(ptr++, and_bytes[2], xor_bytes[2]);
    }
}

void _DIBDRV_SolidHLine16(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor)
{
    WORD *ptr;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    for(i = start; i < end; i++)
        _DIBDRV_rop16(ptr++, and, xor);
}

void _DIBDRV_SolidHLine8(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    for(i = start; i < end; i++)
        _DIBDRV_rop8(ptr++, and, xor);
}

void _DIBDRV_SolidHLine4(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;
    BYTE byte_and, byte_xor;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);
    byte_and = (and & 0xf) | ((and << 4) & 0xf0);
    byte_xor = (xor & 0xf) | ((xor << 4) & 0xf0);

    if(start & 1) /* upper nibble untouched */
        _DIBDRV_rop8(ptr++, byte_and | 0xf0, byte_xor & 0x0f);

    for(i = (start + 1) / 2; i < end / 2; i++)
        _DIBDRV_rop8(ptr++, byte_and, byte_xor);

    if(end & 1) /* lower nibble untouched */
        _DIBDRV_rop8(ptr, byte_and | 0x0f, byte_xor & 0xf0);
}

void _DIBDRV_SolidHLine1(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;
    BYTE byte_and = 0, byte_xor = 0, mask;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    if(and & 1) byte_and = 0xff;
    if(xor & 1) byte_xor = 0xff;

    if((start & ~7) == (end & ~7)) /* special case run inside one byte */
    {
        mask = ((1L << ((end & 7) - (start & 7))) - 1) << (8 - (end & 7));
        _DIBDRV_rop8(ptr, byte_and | ~mask, byte_xor & mask);
        return;
    }

    if(start & 7)
    {
        mask = (1 << (8 - (start & 7))) - 1;
        _DIBDRV_rop8(ptr++, byte_and | ~mask, byte_xor & mask);
    }

    for(i = (start + 7) / 8; i < end / 8; i++)
        _DIBDRV_rop8(ptr++, byte_and, byte_xor);

    if(end & 7)
    {
        mask = ~((1 << (8 - (end & 7))) - 1);
        _DIBDRV_rop8(ptr++, byte_and | ~mask, byte_xor & mask);
    }
}

void _DIBDRV_PatternHLine32(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset)
{
    DWORD *ptr;
    const DWORD *and_ptr = and, *xor_ptr = xor;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    and_ptr += offset;
    xor_ptr += offset;
    for(i = start; i < end; i++)
    {
        _DIBDRV_rop32(ptr++, *and_ptr++, *xor_ptr++);
        if(++offset == count)
        {
            offset = 0;
            and_ptr = and;
            xor_ptr = xor;
        }
    }
}

void _DIBDRV_PatternHLine24(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset)
{
    BYTE *ptr;
    const BYTE *and_ptr = and, *xor_ptr = xor;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    and_ptr += offset * 3;
    xor_ptr += offset * 3;

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr++,  *and_ptr++, *xor_ptr++);
        _DIBDRV_rop8(ptr++,  *and_ptr++, *xor_ptr++);
        _DIBDRV_rop8(ptr++,  *and_ptr++, *xor_ptr++);
        if(++offset == count)
        {
            offset = 0;
            and_ptr = and;
            xor_ptr = xor;
        }
    }
}

void _DIBDRV_PatternHLine16(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset)
{
    WORD *ptr;
    const WORD *and_ptr = and, *xor_ptr = xor;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    and_ptr += offset;
    xor_ptr += offset;

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop16(ptr++, *and_ptr++, *xor_ptr++);
        if(++offset == count)
        {
            offset = 0;
            and_ptr = and;
            xor_ptr = xor;
        }
    }
}

void _DIBDRV_PatternHLine8(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset)
{
    BYTE *ptr;
    const BYTE *and_ptr = and, *xor_ptr = xor;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    and_ptr += offset;
    xor_ptr += offset;

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr++, *and_ptr++, *xor_ptr++);
        if(++offset == count)
        {
            offset = 0;
            and_ptr = and;
            xor_ptr = xor;
        }
    }
}

void _DIBDRV_PatternHLine4(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset)
{
    BYTE *ptr;
    const BYTE *and_ptr = and, *xor_ptr = xor;
    int i;
    BYTE byte_and, byte_xor;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    and_ptr += offset / 2;
    xor_ptr += offset / 2;

    for(i = start; i < end; i++)
    {
        if(offset & 1)
        {
            byte_and = *and_ptr++ & 0x0f;
            byte_xor = *xor_ptr++ & 0x0f;
        }
        else
        {
            byte_and = (*and_ptr & 0xf0) >> 4;
            byte_xor = (*xor_ptr & 0xf0) >> 4;
        }

        if(i & 1)
            byte_and |= 0xf0;
        else
        {
            byte_and = (byte_and << 4) | 0x0f;
            byte_xor <<= 4;
        }

        _DIBDRV_rop8(ptr, byte_and, byte_xor);

        if(i & 1) ptr++;

        if(++offset == count)
        {
            offset = 0;
            and_ptr = and;
            xor_ptr = xor;
        }
    }
}

void _DIBDRV_PatternHLine1(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset)
{
    BYTE *ptr;
    const BYTE *and_ptr = and, *xor_ptr = xor;
    int i;
    BYTE byte_and, byte_xor, dst_mask, brush_mask;

    ptr = dib->funcs->GetPixelPointer(dib, start, row);

    and_ptr += offset / 8;
    xor_ptr += offset / 8;

    for(i = start; i < end; i++)
    {
        dst_mask   = 1 << (7 - (i      & 7));
        brush_mask = 1 << (7 - (offset & 7));

        byte_and = (*and_ptr & brush_mask) ? 0xff : 0;
        byte_xor = (*xor_ptr & brush_mask) ? 0xff : 0;

        byte_and |= ~dst_mask;
        byte_xor &= dst_mask;

        _DIBDRV_rop8(ptr, byte_and, byte_xor);

        if((i & 7) == 7) ptr++;
        if(++offset == count)
        {
            offset = 0;
            and_ptr = and;
            xor_ptr = xor;
        }
        else if((offset & 7) == 7)
        {
            and_ptr++;
            xor_ptr++;
        }
    }
}

/* ------------------------------------------------------------*/
/*                      VERTICAL LINES                         */
void _DIBDRV_SolidVLine32(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, col, start);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop32((DWORD*)ptr, and, xor);
        ptr += dib->stride;
    }
}

void _DIBDRV_SolidVLine24(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;
    BYTE and_bytes[3], xor_bytes[3];

    and_bytes[0] =  and        & 0xff;
    and_bytes[1] = (and >> 8)  & 0xff;
    and_bytes[2] = (and >> 16) & 0xff;
    xor_bytes[0] =  xor        & 0xff;
    xor_bytes[1] = (xor >> 8)  & 0xff;
    xor_bytes[2] = (xor >> 16) & 0xff;

    ptr  = dib->funcs->GetPixelPointer(dib, col, start);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr, and_bytes[0], xor_bytes[0]);
        _DIBDRV_rop8(ptr + 1, and_bytes[1], xor_bytes[1]);
        _DIBDRV_rop8(ptr + 2, and_bytes[2], xor_bytes[2]);
        ptr += dib->stride;
    }
}

void _DIBDRV_SolidVLine16(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, col, start);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop16((WORD*)ptr, and, xor);
        ptr += dib->stride;
    }
}

void _DIBDRV_SolidVLine8(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;

    ptr = dib->funcs->GetPixelPointer(dib, col, start);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr, and, xor);
        ptr += dib->stride;
    }
}

void _DIBDRV_SolidVLine4(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;
    BYTE byte_and, byte_xor;

    if(col & 1) /* upper nibble untouched */
    {
        byte_and = (and & 0xf) | 0xf0;
        byte_xor = (xor & 0xf);
    }
    else
    {
        byte_and = ((and << 4) & 0xf0) | 0x0f;
        byte_xor = ((xor << 4) & 0xf0);
    }

    ptr = dib->funcs->GetPixelPointer(dib, col, start);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr, byte_and, byte_xor);
        ptr += dib->stride;
    }
}

void _DIBDRV_SolidVLine1(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor)
{
    BYTE *ptr;
    int i;
    BYTE byte_and = 0, byte_xor = 0, mask;

    if(and & 1) byte_and = 0xff;
    if(xor & 1) byte_xor = 0xff;

    mask = 1 << (7 - (col & 7));

    byte_and |= ~mask;
    byte_xor &= mask;

    ptr = dib->funcs->GetPixelPointer(dib, col, start);

    for(i = start; i < end; i++)
    {
        _DIBDRV_rop8(ptr, byte_and, byte_xor);
        ptr += dib->stride;
    }
}

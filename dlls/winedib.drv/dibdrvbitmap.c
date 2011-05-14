/*
 * DIB Engine DIBDRVBITMAP handling
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

/* gets human-readable dib format name */
const char *_DIBDRVBITMAP_GetFormatName(DIBDRVBITMAP const *bmp)
{
   if(!bmp)
   {
       ERR("Null bitmap\n");
       return "NULL BITMAP DETECTED";
   }
   switch(bmp->format)
   {
       case DIBFMT_DIB1:
           return "DIBFMT_DIB1";
       case DIBFMT_DIB4:
           return "DIBFMT_DIB4";
       case DIBFMT_DIB4_RLE:
           return "DIBFMT_DIB4_RLE";
       case DIBFMT_DIB8:
           return "DIBFMT_DIB8";
       case DIBFMT_DIB8_RLE:
           return "DIBFMT_DIB8_RLE";
       case DIBFMT_DIB16_RGB:
           return "DIBFMT_DIB_RGB";
       case DIBFMT_DIB16_BITFIELDS:
           return "DIBFMT_DIB16_BITFIELDS";
       case DIBFMT_DIB24:
           return "DIBFMT_DIB24";
       case DIBFMT_DIB32_RGB:
           return "DIBFMT_DIB32_RGB";
       case DIBFMT_DIB32_BITFIELDS:
           return "DIBFMT_DIB32_BITFIELDS";
       case DIBFMT_UNKNOWN:
       default:
           return "DIBFMT_UNKNOWN";
   }
}

/* calculates shift and length given a bit mask */
static void CalcShiftAndLen(DWORD mask, int *shift, int *len)
{
    int s, l;

    /* FIXME----*/
    if(mask == 0)
    {
        FIXME("color mask == 0 -- problem on init_dib\n");
        *shift = 0;
        *len = 0;
        return;
    }
    
    /* calculates bit shift
       (number of 0's on right of bit field */
    s = 0;
    while ((mask & 1) == 0)
    {
        mask >>= 1;
        s++;
    }

    /* calculates bitfield length
       (number of 1's in bit field */
    l = 0;
    while ((mask & 1) == 1)
    {
        mask >>= 1;
        l++;
    }
    *shift = s;
    *len = l;
}

/* initializes bit fields from bit masks */
static void InitBitFields(DIBDRVBITMAP *dib, const DWORD *bit_fields)
{
    dib->redMask    = bit_fields[0];
    dib->greenMask  = bit_fields[1];
    dib->blueMask   = bit_fields[2];
    CalcShiftAndLen(dib->redMask,   &dib->redShift,   &dib->redLen);
    CalcShiftAndLen(dib->greenMask, &dib->greenShift, &dib->greenLen);
    CalcShiftAndLen(dib->blueMask,  &dib->blueShift,  &dib->blueLen);
}

/* initializes dib from a bitmap : 
    dib           dib being initialized
    bi            source BITMAPINFOHEADER with required DIB format info
    bit_fields    color masks
    colorTable    color table, if any
    bits          pointer to image data array
    NOTE : DIBDRVBITMAP doesn't owns bits, but do own color table
*/
BOOL _DIBDRVBITMAP_InitFromBMIH(DIBDRVBITMAP *dib, const BITMAPINFOHEADER *bi, const DWORD *bit_fields,
                     const RGBQUAD *colorTable, void *bits)
{
    MAYBE(TRACE("dib=%p, bi=%p, bit_fields=%p, colorTable=%p, bits=%p\n", dib, bi, bit_fields, colorTable, bits));
    
    /* initializes DIB dimensions and color depth */
    dib->bitCount = bi->biBitCount;
    dib->width     = bi->biWidth;
    dib->height    = bi->biHeight;
    dib->stride    = ((dib->width * dib->bitCount + 31) >> 3) & ~3;

    /* initializes image data pointer */
    dib->bits      = bits;
    dib->ownsBits = FALSE;

    /* initializes color table */
    dib->colorTableSize = 0;
    dib->colorTable = NULL;
    dib->colorTableGrabbed = FALSE;

    /* checks whether dib is top-down or bottom-up one */
    if(dib->height < 0)
    {
        /* top-down dib */
        dib->height = -dib->height;
    }
    else
    {
        /* bottom-up dib */
        /* data->bits always points to the top-left corner and the stride is -ve */
        dib->bits    = (BYTE*)dib->bits + (dib->height - 1) * dib->stride;
        dib->stride  = -dib->stride;
    }

    /* gets and stores bitmap format */
    switch(dib->bitCount)
    {
        case 24:
            dib->format = DIBFMT_DIB24;
            dib->funcs = &DIBDRV_funcs_DIB24;
            break;
    
        case 32:
    
            if(bi->biCompression == BI_RGB)
            {
                dib->format = DIBFMT_DIB32_RGB;
                dib->funcs = &DIBDRV_funcs_DIB32_RGB;
            }
            else
            {
                InitBitFields(dib, bit_fields);
                dib->format = DIBFMT_DIB32_BITFIELDS;
                dib->funcs = &DIBDRV_funcs_DIB32_BITFIELDS;
            }
            break;
    
        case 16:
            if(bi->biCompression == BI_RGB)
            {
                dib->format = DIBFMT_DIB16_RGB;
                dib->funcs = &DIBDRV_funcs_DIB16_RGB;
            }
            else
            {
                InitBitFields(dib, bit_fields);
                dib->format = DIBFMT_DIB16_BITFIELDS;
                dib->funcs = &DIBDRV_funcs_DIB16_BITFIELDS;
            }
            break;
    
        case 8:
            dib->format = DIBFMT_DIB8;
            dib->funcs = &DIBDRV_funcs_DIB8;
            dib->colorTableSize = 256;
            if(bi->biClrUsed) dib->colorTableSize = bi->biClrUsed;
            break;
    
        case 4:
            dib->format = DIBFMT_DIB4;
            dib->funcs = &DIBDRV_funcs_DIB4;
            dib->colorTableSize = 16;
            if(bi->biClrUsed) dib->colorTableSize = bi->biClrUsed;
            break;
    
        case 1:
            dib->format = DIBFMT_DIB1;
            dib->funcs = &DIBDRV_funcs_DIB1;
            dib->colorTableSize = 2;
            if(bi->biClrUsed) dib->colorTableSize = bi->biClrUsed;
            break;
    
        default:
            dib->format = DIBFMT_UNKNOWN;
            dib->funcs = NULL;
            FIXME("bpp %d not supported\n", dib->bitCount);
            return FALSE;
    }
    MAYBE(TRACE("DIB FORMAT : %s\n", _DIBDRVBITMAP_GetFormatName(dib)));
    
    /* allocates color table and copy it from source, *if* source is
       not null */
    if(dib->colorTableSize && colorTable)
    {
        if(!(dib->colorTable = HeapAlloc(GetProcessHeap(), 0,
            dib->colorTableSize * sizeof(dib->colorTable[0]))
          ))
        {
            ERR("HeapAlloc failed\n");
            return FALSE;
        }
        memcpy(dib->colorTable, colorTable,
            dib->colorTableSize * sizeof(dib->colorTable[0]));
        dib->colorTableGrabbed = TRUE;
    }
    else if(!dib->colorTableSize)
        /* no color table on more than 8 bits/pixel */
        dib->colorTableGrabbed = TRUE;

    MAYBE(TRACE("END\n"));
    return TRUE;
}

BOOL _DIBDRVBITMAP_InitFromBitmapinfo(DIBDRVBITMAP *dib, BITMAPINFO *bmi)
{
    static const DWORD bit_fields_DIB32_RGB[3] = {0xff0000, 0x00ff00, 0x0000ff};
    static const DWORD bit_fields_DIB16_RGB[3] = {0x7c00, 0x03e0, 0x001f};
    BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *)bmi;
    const DWORD *masks = NULL;
    RGBQUAD *colorTable = NULL;
    BYTE *ptr = (BYTE*)bmi + bi->biSize;
    int num_colors = bi->biClrUsed;
    BOOL res;
    
    MAYBE(TRACE("dib=%p, bmi=%p\n", dib, bmi));

    if(bi->biCompression == BI_BITFIELDS)
    {
        masks = (DWORD *)ptr;
        ptr += 3 * sizeof(DWORD);
    }
    else if(bi->biBitCount == 32)
        masks = bit_fields_DIB32_RGB;
    else if(bi->biBitCount == 16)
        masks = bit_fields_DIB16_RGB;

    if(!num_colors && bi->biBitCount <= 8)
        num_colors = 1 << bi->biBitCount;
    if(num_colors)
        colorTable = (RGBQUAD*)ptr;
    ptr += num_colors * sizeof(*colorTable);

    res = _DIBDRVBITMAP_InitFromBMIH(dib, bi, masks, colorTable, ptr);
    MAYBE(TRACE("END\n"));
    return res;
}

/* initializes a DIBRDVBITMAP copying it from a source one
   Parameters :
      dib       destination DIBDRVBITMAP
      src       source DIBDRVBITMAP
      copy      TRUE->copy source pixel array FALSE->link to source pixel array 
*/
BOOL _DIBDRVBITMAP_InitFromDibdrvbitmap(DIBDRVBITMAP *dib, const DIBDRVBITMAP *src, BOOL copy)
{
    MAYBE(TRACE("dib=%p, src=%p, copy=%d\n", dib, src, copy));
    
    dib->format = src->format;
    dib->width = src->width;
    dib->height = src->height;
    dib->stride = src->stride;
    dib->bitCount = src->bitCount;

    dib->redMask = src->redMask;
    dib->greenMask = src->greenMask;
    dib->blueMask = src->blueMask;
    dib->redShift = src->redShift;
    dib->greenShift = src->greenShift;
    dib->blueShift = src->blueShift;
    dib->redLen = src->redLen;
    dib->greenLen = src->greenLen;
    dib->blueLen = src->blueLen;

    dib->funcs = src->funcs;
    
    if(copy)
    {
        int size = dib->height*abs(dib->stride);
        if(!(dib->bits = HeapAlloc(GetProcessHeap(), 0, size)))
        {
            ERR("Failed to allocate bits buffer\n");
            return FALSE;
        }
        dib->ownsBits = TRUE;

        /* check for bottom-up DIB */
        if(dib->stride < 0)
        {
            /* copy the bitmap array */
            memcpy(dib->bits, (BYTE *)src->bits + (src->height - 1) * src->stride, size);
        
            dib->bits = (BYTE *)dib->bits - (dib->height-1) * dib->stride;
        }
        else
        {
            /* copy the bitmap array */
            memcpy(dib->bits, src->bits, size);
        }
    }
    else
    {
        dib->bits = src->bits;
        dib->ownsBits = FALSE;
    }
    
    if(src->colorTable)
    {
        dib->colorTable = HeapAlloc(GetProcessHeap(), 0, src->colorTableSize * sizeof(src->colorTable[0]));
        memcpy(dib->colorTable, src->colorTable, src->colorTableSize * sizeof(src->colorTable[0]));
    }
    else
        dib->colorTable = NULL;
    dib->colorTableSize = src->colorTableSize;
    dib->colorTableGrabbed = TRUE;
    MAYBE(TRACE("END\n"));
    return TRUE;
}

/* creates a DIBRDVBITMAP copying format info from a source one
   Parameters :
      dib            destination DIBDRVBITMAP
      src            source DIBDRVBITMAP
      widht, height  sizes of newly created bitmap 
*/
BOOL _DIBDRVBITMAP_CreateFromDibdrvbitmap(DIBDRVBITMAP *dib, const DIBDRVBITMAP *src, int width, int height)
{
    MAYBE(TRACE("dib=%p, src=%p, width=%d, height=%d\n", dib, src, width, height));
    
    /* grab color and format info from source DIB */
    if(!_DIBDRVBITMAP_InitFromDibdrvbitmap(dib, src, FALSE))
    {
        ERR("Failed grabbing source dib format\n");
        return FALSE;
    }
    
    /* sets up new DIB dimensions */
    dib->width = width;
    dib->height = height;
    
    /* calculates new stride basing of new width */
    dib->stride = ((width * dib->bitCount +31) &~31) / 8;
    if(src->stride < 0)
        dib->stride = -dib->stride;
    
    /* allocates bits for newly created DIB */
    if(!(dib->bits = HeapAlloc(GetProcessHeap(), 0, height*abs(dib->stride))))
    {
        ERR("Failed to allocate bits buffer\n");
        return FALSE;
    }
    /* check for bottom-up DIB */
    if(dib->stride < 0)
        dib->bits = (BYTE *)dib->bits - (dib->height-1) * dib->stride;
    dib->ownsBits = TRUE;
    
    MAYBE(TRACE("END\n"));
    return TRUE;
 }

/* Clears a DIBDRVBITMAP structure data
   WARNING : doesn't free anything */
void _DIBDRVBITMAP_Clear(DIBDRVBITMAP *bmp)
{
    MAYBE(TRACE("bmp=%p\n", bmp));
    
    bmp->bits = NULL;
    bmp->ownsBits = FALSE;
    bmp->colorTable = NULL;
    bmp->colorTableSize = 0;
    bmp->colorTableGrabbed = FALSE;
    
    MAYBE(TRACE("END\n"));
}

/* Frees a DIBDRVBITMAP structure data */
void _DIBDRVBITMAP_Free(DIBDRVBITMAP *bmp)
{
    MAYBE(TRACE("bmp=%p\n", bmp));
    
    /* frees bits, if needed */
    if(bmp->bits && bmp->ownsBits)
    {
        /* on bottom-up dibs, bits doesn't point to starting
           of buffer.... bad design choice */
        if(bmp->stride < 0)
            bmp->bits = (BYTE *)bmp->bits + bmp->stride * (bmp->height -1);
        HeapFree(GetProcessHeap(), 0, bmp->bits);
    }
    bmp->ownsBits = FALSE;
    bmp->bits = NULL;
    
    /* frees color table */
    if(bmp->colorTable)
        HeapFree(GetProcessHeap(), 0, bmp->colorTable);
    bmp->colorTable = NULL;
    bmp->colorTableSize = 0;
    bmp->colorTableGrabbed = FALSE;
    
    MAYBE(TRACE("END\n"));
}


/* checks whether the format of 2 DIBs are identical
   it checks the pixel bit count and the color table size
   and content, if needed */
BOOL _DIBDRVBITMAP_FormatMatch(const DIBDRVBITMAP *d1, const DIBDRVBITMAP *d2)
{
    /* checks at first the format (bit count and color masks) */
    if(d1->format != d2->format)
        return FALSE;

    /* formats matches, now checks color tables if needed */
    switch(d1->format)
    {
        case DIBFMT_DIB32_RGB :
        case DIBFMT_DIB32_BITFIELDS :
        case DIBFMT_DIB24 :
        case DIBFMT_DIB16_RGB :
        case DIBFMT_DIB16_BITFIELDS :
            return TRUE;

        case DIBFMT_DIB1 :
        case DIBFMT_DIB4 :
        /*case DIBFMT_DIB4_RLE :*/
        case DIBFMT_DIB8 :
        /*case DIBFMT_DIB8_RLE :*/
            if(d1->colorTableSize != d2->colorTableSize)
                return FALSE;
        return !memcmp(d1->colorTable, d2->colorTable, d1->colorTableSize * sizeof(d1->colorTable[0]));

    default:
        ERR("Unexpected depth %d\n", d1->bitCount);
        return FALSE;
    }
}

/* convert a given dib into another format given by 'format' parameter */
BOOL _DIBDRVBITMAP_Convert(DIBDRVBITMAP *dst, const DIBDRVBITMAP *src, const DIBDRVBITMAP *format)
{
    int width, height;
    int iLine;
    void *buf;
    BOOL res;
    
    MAYBE(TRACE("dst=%p, src=%p, format=%p\n", dst, src, format));
    
    /* free, if needed, destination bitmap */
    _DIBDRVBITMAP_Free(dst);
    
    /* if format and source bitmaps format match,
       just copy source on destination */
    if(_DIBDRVBITMAP_FormatMatch(src, format))
    {
        res = _DIBDRVBITMAP_InitFromDibdrvbitmap(dst, src, TRUE);
        MAYBE(TRACE("END - Identical formats\n"));
        return res;
    }

    /* formats don't match, we create the dest bitmap with same format as format's one
       but with source's one dimensions */
    width = src->width;
    height = src->height;
    if(!_DIBDRVBITMAP_CreateFromDibdrvbitmap(dst, format, width, height))
    {
        ERR("Couldn't create destination bmp\n");
        return FALSE;
    }
    
    /* we now copy/convert from source to dest */
    if(!(buf = HeapAlloc(GetProcessHeap(), 0, width * 4)))
    {
        ERR("HeapAlloc failed\n");
        return FALSE;
    }

    for(iLine = 0; iLine < height; iLine++)
    {
        src->funcs->GetLine(src, iLine, 0, width, buf);
        dst->funcs->PutLine(dst, iLine, 0, width, buf);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    
    MAYBE(TRACE("END - different formats\n"));
    return TRUE;
}

/* creates a solid-filled DIB of given color and format
   DIB format is given by 'format' parameter */
BOOL _DIBDRVBITMAP_CreateSolid(DIBDRVBITMAP *bmp, DIBDRVBITMAP *format, int width, int height, DWORD Color)
{
    DWORD *buf, *bufPnt;
    int i;
    
    MAYBE(TRACE("bmp=%p, format=%p, width=%d, height=%d, Color=%08x\n", bmp, format, width, height, Color));
    
    /* swaps color bytes....*/
    Color = RGB((Color >> 8) & 0xff, (Color >> 16) &0xff, Color &0xff);
    
    /* creates the destination bitmap */
    if(!_DIBDRVBITMAP_CreateFromDibdrvbitmap(bmp, format, width, height))
    {
        ERR("Couldn't create destination bmp\n");
        return FALSE;
    }
       
    /* creates a temporary line filled with given color */
    if(!(buf = HeapAlloc(GetProcessHeap(), 0, width * 4)))
    {
        ERR("HeapAlloc failed\n");
        return FALSE;
    }
    
    for(i = 0, bufPnt = buf; i < width; i++)
        *bufPnt++ = Color;
    
    /* fills the bitmap */
    for(i = 0; i < height; i++)
        bmp->funcs->PutLine(bmp, i, 0, width, buf);
    
    /* frees temporaty line */
    HeapFree(GetProcessHeap(), 0, buf);
    
    MAYBE(TRACE("END\n"));
    return TRUE;
}

/* expands horizontally a bitmap to reach a minimum size,
   keeping its width as a multiple of a base width
   Used to widen brushes in order to optimize blitting */
BOOL _DIBDRVBITMAP_ExpandHoriz(DIBDRVBITMAP *dib, int baseWidth, int minWidth)
{
    BYTE *srcBuf, *dstBuf;
    int chunkSize;
    int iLine, iCol;
    DIBDRVBITMAP tmpDib;
    void *bits;
    BOOL ownsBits;
    
    MAYBE(TRACE("dib=%p, baseWidth=%d, minWidth=%d\n", dib, baseWidth, minWidth));

    /* if dst dib already wide enough, just do nothing */
    if(dib->width >= minWidth)
    {
        MAYBE(TRACE("END - No need to expand\n"));
        return TRUE;
    }

    /* source DIB can't be NULL */
    if(!dib->bits)
    {
        ERR("Empty source DIB detected\n");
        return FALSE;
    }
    
    /* round up minWidth to be a multiple of source width */
    minWidth += (baseWidth - (minWidth % baseWidth));
    
    /* creates a temporary destination bitmap with required sizes */
    _DIBDRVBITMAP_Clear(&tmpDib);
    if(!_DIBDRVBITMAP_CreateFromDibdrvbitmap(&tmpDib, dib, minWidth, dib->height))
    {
        ERR("Couldn't create the temporary DIB for brush cache\n");
        return FALSE;
    }

    /* if format uses almost 1 byte/pixel, fast copy path */
    if(dib->bitCount >= 8)
    {
        chunkSize = dib->width * dib->bitCount / 8;
        for(iLine = 0; iLine < dib->height; iLine++)
        {
            srcBuf = (BYTE *)dib->bits + iLine * dib->stride;
            dstBuf = (BYTE *)tmpDib.bits + iLine * tmpDib.stride;
            for(iCol = 0; iCol < tmpDib.width; iCol += dib->width)
            {
                memcpy(dstBuf, srcBuf, chunkSize);
                dstBuf += chunkSize;
            }
        }
    }
    /* otherwise slow path -- could be optimized */
    else
    {
        chunkSize = dib->width * 4;
        /* allocates a line buffer */
        if(!(srcBuf = HeapAlloc(GetProcessHeap(), 0, tmpDib.width * 4)))
        {
            ERR("HeapAlloc failed\n");
            return FALSE;
        }
        
        FIXME("dib:format=%s, funcs=%p, bits=%p, width=%d, height=%d, stride=%d\n",
            _DIBDRVBITMAP_GetFormatName(dib), dib->funcs, dib->bits, dib->width, dib->height, dib->stride);
        for(iLine = 0; iLine < dib->height; iLine++)
        {
            /* fills the line buffer repeating source's line data */
             dib->funcs->GetLine(dib, iLine, 0, dib->width, srcBuf);
             dstBuf = srcBuf + chunkSize;
            for(iCol = dib->width; iCol < tmpDib.width; iCol += dib->width)
            {
                memcpy(dstBuf, srcBuf, chunkSize);
                dstBuf += chunkSize;
            }
             /* stores the line on destination bmp */
            tmpDib.funcs->PutLine(&tmpDib, iLine, 0, tmpDib.width, srcBuf);
        }
        HeapFree(GetProcessHeap(), 0, srcBuf);
    }
    
    /* swaps temp DIB and source one */
    bits = dib->bits;
    ownsBits = dib->ownsBits;
    dib->bits = tmpDib.bits;
    dib->ownsBits = tmpDib.ownsBits;
    tmpDib.bits = bits;
    tmpDib.ownsBits = ownsBits;
    
    /* frees the temporary DIB */
    _DIBDRVBITMAP_Free(&tmpDib);
   
    MAYBE(TRACE("END\n"));
    return TRUE;
}

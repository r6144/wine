/*
 * DIB Engine Font Primitives
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
/*               FREETYPE FONT BITMAP BLITTING                 */
void _DIBDRV_freetype_blit_8888(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;
    DWORD *ptr;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        ptr = (DWORD *)((BYTE *)dib->bits + (dibY * dib->stride) + x * 4);
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                *ptr = c;
            }
            buf++;
            ptr++;
        }
    }
}

void _DIBDRV_freetype_blit_32_RGB(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_32_BITFIELDS(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_24(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_16_RGB(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_16_BITFIELDS(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_8(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_4(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_1(DIBDRVPHYSDEV *physDev, int x, int y, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = &physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int DIBX, DIBY;
    DWORD c;

    /* gets DIB limits */
    DIBX = dib->width;
    DIBY = dib->height;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < DIBX && dibY < DIBY && dibX > 0 && dibY > 0 && *buf)
            {
                c = physDev->textColorTable[*buf];
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

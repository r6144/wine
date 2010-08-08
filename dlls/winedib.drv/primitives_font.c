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
void _DIBDRV_freetype_blit_8888(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
    DWORD *ptr;
#ifdef DIBDRV_ANTIALIASED_FONTS
    const BYTE ng1 = bmp->num_grays - 1;
    DWORD c;
    BYTE r, g, b, negColor;
#else
    DWORD c = dib->funcs->ColorToPixel(dib, physDev->textColor);
#endif

    /* gets clip limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        ptr = (DWORD *)((BYTE *)dib->bits + (dibY * dib->stride) + x * 4);
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
            {
#ifdef DIBDRV_ANTIALIASED_FONTS        
                c = physDev->textColorTable[*buf];
                if(*buf < ng1)
                {
                    negColor = ng1 - *buf;
                    r = (*ptr >> 16) & 0xff;
                    g = (*ptr >>  8) & 0xff;
                    b = *ptr         & 0xff;
                    c += MulDiv(r, ng1 - *buf, ng1) << 16 |
                         MulDiv(g, ng1 - *buf, ng1) <<  8 |
                         MulDiv(b, ng1 - *buf, ng1);
                }
#endif
                *ptr = c;
            }
            buf++;
            ptr++;
        }
    }
}

void _DIBDRV_freetype_blit_32_RGB(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
    DWORD *ptr;
#ifdef DIBDRV_ANTIALIASED_FONTS        
    const BYTE ng1 = bmp->num_grays - 1;
    DWORD c;
    BYTE r, g, b, negColor;
#else
    DWORD c = dib->funcs->ColorToPixel(dib, physDev->textColor);
#endif

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        ptr = (DWORD *)((BYTE *)dib->bits + (dibY * dib->stride) + x * 4);
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
            {
#ifdef DIBDRV_ANTIALIASED_FONTS        
                c = physDev->textColorTable[*buf];
                if(*buf < ng1)
                {
                    negColor = ng1 - *buf;
                    r = (*ptr >> 16) & 0xff;
                    g = (*ptr >>  8) & 0xff;
                    b = *ptr         & 0xff;
                    c += MulDiv(r, ng1 - *buf, ng1) << 16 |
                         MulDiv(g, ng1 - *buf, ng1) <<  8 |
                         MulDiv(b, ng1 - *buf, ng1);
                }
#endif
                *ptr = c;
            }
            buf++;
            ptr++;
        }
    }
}

void _DIBDRV_freetype_blit_32_BITFIELDS(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
#ifdef DIBDRV_ANTIALIASED_FONTS        
    const BYTE ng1 = bmp->num_grays - 1;
    DWORD c;
    COLORREF pix;
    BYTE r, g, b, negColor;
#else
    DWORD c = dib->funcs->ColorToPixel(dib, physDev->textColor);
#endif

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
            {
#ifdef DIBDRV_ANTIALIASED_FONTS        
                c = physDev->textColorTable[*buf];
                if(*buf < ng1)
                {
                    negColor = ng1 - *buf;
                    pix = dib->funcs->GetPixel(dib, dibX, dibY);
                    r = pix         & 0xff;
                    g = (pix >>  8) & 0xff;
                    b = (pix >> 16) & 0xff;
                    c += MulDiv(r, ng1 - *buf, ng1) << 16 |
                         MulDiv(g, ng1 - *buf, ng1) <<  8 |
                         MulDiv(b, ng1 - *buf, ng1);
                }
#endif
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_24(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
#ifdef DIBDRV_ANTIALIASED_FONTS        
    const BYTE ng1 = bmp->num_grays - 1;
    DWORD c;
    COLORREF pix;
    BYTE r, g, b, negColor;
#else
    DWORD c = dib->funcs->ColorToPixel(dib, physDev->textColor);
#endif

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
            {
#ifdef DIBDRV_ANTIALIASED_FONTS        
                c = physDev->textColorTable[*buf];
                if(*buf < ng1)
                {
                    negColor = ng1 - *buf;
                    pix = dib->funcs->GetPixel(dib, dibX, dibY);
                    r = pix         & 0xff;
                    g = (pix >>  8) & 0xff;
                    b = (pix >> 16) & 0xff;
                    c += MulDiv(r, ng1 - *buf, ng1) << 16 |
                         MulDiv(g, ng1 - *buf, ng1) <<  8 |
                         MulDiv(b, ng1 - *buf, ng1);
                }
#endif
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_16_RGB(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
#ifdef DIBDRV_ANTIALIASED_FONTS        
    const BYTE ng1 = bmp->num_grays - 1;
    DWORD c;
    COLORREF pix;
    BYTE r, g, b, negColor;
#else
    DWORD c = dib->funcs->ColorToPixel(dib, physDev->textColor);
#endif

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
            {
#ifdef DIBDRV_ANTIALIASED_FONTS        
                c = physDev->textColorTable[*buf];
                if(*buf < ng1)
                {
                    negColor = ng1 - *buf;
                    pix = dib->funcs->GetPixel(dib, dibX, dibY);
                    r = pix         & 0xff;
                    g = (pix >>  8) & 0xff;
                    b = (pix >> 16) & 0xff;
                    c += MulDiv(r, ng1 - *buf, ng1) << 16 |
                         MulDiv(g, ng1 - *buf, ng1) <<  8 |
                         MulDiv(b, ng1 - *buf, ng1);
                }
#endif
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_16_BITFIELDS(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
#ifdef DIBDRV_ANTIALIASED_FONTS        
    const BYTE ng1 = bmp->num_grays - 1;
    DWORD c;
    COLORREF pix;
    BYTE r, g, b, negColor;
#else
    DWORD c = dib->funcs->ColorToPixel(dib, physDev->textColor);
#endif

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
            {
#ifdef DIBDRV_ANTIALIASED_FONTS        
                c = physDev->textColorTable[*buf];
                if(*buf < ng1)
                {
                    negColor = ng1 - *buf;
                    pix = dib->funcs->GetPixel(dib, dibX, dibY);
                    r = pix         & 0xff;
                    g = (pix >>  8) & 0xff;
                    b = (pix >> 16) & 0xff;
                    c += MulDiv(r, ng1 - *buf, ng1) << 16 |
                         MulDiv(g, ng1 - *buf, ng1) <<  8 |
                         MulDiv(b, ng1 - *buf, ng1);
                }
#endif
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            }
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_8(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
    DWORD c = physDev->textColor;

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_4(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
    DWORD c = physDev->textColor;

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            buf++;
        }
    }
}

void _DIBDRV_freetype_blit_1(DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp)
{
    /* FIXME : MUST BE OPTIMIZED !!! */
    
    DIBDRVBITMAP *dib = physDev->physBitmap;
    int bmpX, bmpY;
    BYTE *buf;
    int dibX, dibY;
    int xMin, xMax, yMin, yMax;
    DWORD c = physDev->textColor;

    /* gets DIB limits */
    xMin = clipRec->left;
    yMin = clipRec->top;
    xMax = clipRec->right;
    yMax = clipRec->bottom;

    /* loop for every pixel in bitmap */
    buf = bmp->buffer;
    for(bmpY = 0, dibY = y; bmpY < bmp->rows; bmpY++, dibY++)
    {
        for(bmpX = 0, dibX = x; bmpX < bmp->width; bmpX++, dibX++)
        {
            if(dibX < xMax && dibY < yMax && dibX >= xMin && dibY >= yMin && *buf)
                dib->funcs->SetPixel(dib, dibX, dibY, 0, c);
            buf++;
        }
    }
}

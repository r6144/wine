/*
 * DIB Engine Primitives function pointers
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
/*                     COLOR FUNCTIONS                         */
DWORD _DIBDRV_ColorToPixel32_RGB      (const DIBDRVBITMAP *dib, COLORREF color);
DWORD _DIBDRV_ColorToPixel32_BITFIELDS(const DIBDRVBITMAP *dib, COLORREF color);
DWORD _DIBDRV_ColorToPixel24          (const DIBDRVBITMAP *dib, COLORREF color);
DWORD _DIBDRV_ColorToPixel16_RGB      (const DIBDRVBITMAP *dib, COLORREF color);
DWORD _DIBDRV_ColorToPixel16_BITFIELDS(const DIBDRVBITMAP *dib, COLORREF color);
DWORD _DIBDRV_ColorToPixelColortable  (const DIBDRVBITMAP *dib, COLORREF color);

/* ------------------------------------------------------------*/
/*                     PIXEL POINTER READING                   */
void *_DIBDRV_GetPixelPointer32(const DIBDRVBITMAP *dib, int x, int y);
void *_DIBDRV_GetPixelPointer24(const DIBDRVBITMAP *dib, int x, int y);
void *_DIBDRV_GetPixelPointer16(const DIBDRVBITMAP *dib, int x, int y);
void *_DIBDRV_GetPixelPointer8 (const DIBDRVBITMAP *dib, int x, int y);
void *_DIBDRV_GetPixelPointer4 (const DIBDRVBITMAP *dib, int x, int y);
void *_DIBDRV_GetPixelPointer1 (const DIBDRVBITMAP *dib, int x, int y);

/* ------------------------------------------------------------*/
/*                     PIXEL WRITING                           */
void _DIBDRV_SetPixel32(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor);
void _DIBDRV_SetPixel24(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor);
void _DIBDRV_SetPixel16(DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor);
void _DIBDRV_SetPixel8 (DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor);
void _DIBDRV_SetPixel4 (DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor);
void _DIBDRV_SetPixel1 (DIBDRVBITMAP *dib, int x, int y, DWORD and, DWORD xor);

/* ------------------------------------------------------------*/
/*                     PIXEL READING                           */
DWORD _DIBDRV_GetPixel32_RGB      (const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel32_BITFIELDS(const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel24          (const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel16_RGB      (const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel16_BITFIELDS(const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel8           (const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel4           (const DIBDRVBITMAP *dib, int x, int y);
DWORD _DIBDRV_GetPixel1           (const DIBDRVBITMAP *dib, int x, int y);

/* ------------------------------------------------------------*/
/*                     HORIZONTAL SOLID LINES                  */
void _DIBDRV_SolidHLine32(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor);
void _DIBDRV_SolidHLine24(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor);
void _DIBDRV_SolidHLine16(DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor);
void _DIBDRV_SolidHLine8 (DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor);
void _DIBDRV_SolidHLine4 (DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor);
void _DIBDRV_SolidHLine1 (DIBDRVBITMAP *dib, int start, int end, int row, DWORD and, DWORD xor);

/* ------------------------------------------------------------*/
/*                     HORIZONTAL PATTERN LINES                */
void _DIBDRV_PatternHLine32(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset);
void _DIBDRV_PatternHLine24(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset);
void _DIBDRV_PatternHLine16(DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset);
void _DIBDRV_PatternHLine8 (DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset);
void _DIBDRV_PatternHLine4 (DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset);
void _DIBDRV_PatternHLine1 (DIBDRVBITMAP *dib, int start, int end, int row, const void *and, const void *xor, DWORD count, DWORD offset);

/* ------------------------------------------------------------*/
/*                      VERTICAL LINES                         */
void _DIBDRV_SolidVLine32(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor);
void _DIBDRV_SolidVLine24(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor);
void _DIBDRV_SolidVLine16(DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor);
void _DIBDRV_SolidVLine8 (DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor);
void _DIBDRV_SolidVLine4 (DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor);
void _DIBDRV_SolidVLine1 (DIBDRVBITMAP *dib, int col, int start, int end, DWORD and, DWORD xor);

/* ----------------------------------------------------------------*/
/*                         CONVERT PRIMITIVES                      */
/* converts (part of) line of any DIB format from/to DIB32_RGB one */
BOOL _DIBDRV_GetLine32_RGB      (const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine32_BITFIELDS(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine24          (const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine16_RGB      (const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine16_BITFIELDS(const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine8           (const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine4           (const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_GetLine1           (const DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);

BOOL _DIBDRV_PutLine32_RGB      (DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine32_BITFIELDS(DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine24          (DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine16_RGB      (DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine16_BITFIELDS(DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine8           (DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine4           (DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);
BOOL _DIBDRV_PutLine1           (DIBDRVBITMAP *bmp, INT line, INT startx, int width, void *buf);

/* ------------------------------------------------------------*/
/*                        BLITTING PRIMITIVES                  */
BOOL _DIBDRV_BitBlt_generic(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop);
BOOL _DIBDRV_BitBlt_32(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop);
BOOL _DIBDRV_BitBlt_24(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop);
BOOL _DIBDRV_BitBlt_16(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop);
BOOL _DIBDRV_BitBlt_8(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop);

BOOL _DIBDRV_StretchBlt_generic(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT widthDst, INT heightDst, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, int widthSrc, int heightSrc, DWORD rop);

/* ------------------------------------------------------------*/
/*               FREETYPE FONT BITMAP BLITTING                 */
void _DIBDRV_freetype_blit_8888        (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_32_RGB      (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_32_BITFIELDS(DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_24          (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_16_RGB      (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_16_BITFIELDS(DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_8           (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_4           (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);
void _DIBDRV_freetype_blit_1           (DIBDRVPHYSDEV *dib, int x, int y, FT_Bitmap *bmp);

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB32_RGB =
{
    _DIBDRV_ColorToPixel32_RGB,
    _DIBDRV_GetPixelPointer32,
    _DIBDRV_SetPixel32,
    _DIBDRV_GetPixel32_RGB,
    _DIBDRV_SolidHLine32,
    _DIBDRV_PatternHLine32,
    _DIBDRV_SolidVLine32,
    _DIBDRV_GetLine32_RGB,
    _DIBDRV_PutLine32_RGB,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_32_RGB
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB32_BITFIELDS =
{
    _DIBDRV_ColorToPixel32_BITFIELDS,
    _DIBDRV_GetPixelPointer32,
    _DIBDRV_SetPixel32,
    _DIBDRV_GetPixel32_BITFIELDS,
    _DIBDRV_SolidHLine32,
    _DIBDRV_PatternHLine32,
    _DIBDRV_SolidVLine32,
    _DIBDRV_GetLine32_BITFIELDS,
    _DIBDRV_PutLine32_BITFIELDS,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_32_BITFIELDS
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB24 =
{
    _DIBDRV_ColorToPixel24,
    _DIBDRV_GetPixelPointer24,
    _DIBDRV_SetPixel24,
    _DIBDRV_GetPixel24,
    _DIBDRV_SolidHLine24,
    _DIBDRV_PatternHLine24,
    _DIBDRV_SolidVLine24,
    _DIBDRV_GetLine24,
    _DIBDRV_PutLine24,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_24
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB16_RGB =
{
    _DIBDRV_ColorToPixel16_RGB,
    _DIBDRV_GetPixelPointer16,
    _DIBDRV_SetPixel16,
    _DIBDRV_GetPixel16_RGB,
    _DIBDRV_SolidHLine16,
    _DIBDRV_PatternHLine16,
    _DIBDRV_SolidVLine16,
    _DIBDRV_GetLine16_RGB,
    _DIBDRV_PutLine16_RGB,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_16_RGB
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB16_BITFIELDS =
{
    _DIBDRV_ColorToPixel16_BITFIELDS,
    _DIBDRV_GetPixelPointer16,
    _DIBDRV_SetPixel16,
    _DIBDRV_GetPixel16_BITFIELDS,
    _DIBDRV_SolidHLine16,
    _DIBDRV_PatternHLine16,
    _DIBDRV_SolidVLine16,
    _DIBDRV_GetLine16_BITFIELDS,
    _DIBDRV_PutLine16_BITFIELDS,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_16_BITFIELDS
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB8 =
{
    _DIBDRV_ColorToPixelColortable,
    _DIBDRV_GetPixelPointer8,
    _DIBDRV_SetPixel8,
    _DIBDRV_GetPixel8,
    _DIBDRV_SolidHLine8,
    _DIBDRV_PatternHLine8,
    _DIBDRV_SolidVLine8,
    _DIBDRV_GetLine8,
    _DIBDRV_PutLine8,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_8
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB4 =
{
    _DIBDRV_ColorToPixelColortable,
    _DIBDRV_GetPixelPointer4,
    _DIBDRV_SetPixel4,
    _DIBDRV_GetPixel4,
    _DIBDRV_SolidHLine4,
    _DIBDRV_PatternHLine4,
    _DIBDRV_SolidVLine4,
    _DIBDRV_GetLine4,
    _DIBDRV_PutLine4,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_4
};

DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB1 =
{
    _DIBDRV_ColorToPixelColortable,
    _DIBDRV_GetPixelPointer1,
    _DIBDRV_SetPixel1,
    _DIBDRV_GetPixel1,
    _DIBDRV_SolidHLine1,
    _DIBDRV_PatternHLine1,
    _DIBDRV_SolidVLine1,
    _DIBDRV_GetLine1,
    _DIBDRV_PutLine1,
    _DIBDRV_BitBlt_generic,
    _DIBDRV_StretchBlt_generic,
    _DIBDRV_freetype_blit_1
};

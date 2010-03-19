/*
 * DIBDRV text functions
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

/***********************************************************************
 *           DIBDRV_ExtTextOut
 */
BOOL DIBDRV_ExtTextOut( DIBDRVPHYSDEV *physDev, INT x, INT y, UINT flags,
                        const RECT *lprect, LPCWSTR wstr, UINT count,
                        const INT *lpDx )
{
    /* FIXME : TODO many, many stuffs... just trivial text support by now */

    BOOL res;
    FT_Face face;
    FT_UInt glyph_index;
    INT n;
    INT error;
    LOGFONTW lf;
    int w, h;
    LPCWSTR wstrPnt;
    
    FT_Glyph glyph;
    FT_BitmapGlyph bitmapGlyph;
    FT_Bitmap *bitmap, bitmap8;
    double cosEsc, sinEsc;
    FT_Matrix matrix;
    FT_Vector start; 
    int dx, dy;
    
    RECT *r;
    int iRec;
    RECT clipRec;
    DWORD backPixel; 
    
    MAYBE(TRACE("physDev:%p, x:%d, y:%d, flags:%x, lprect:%p, wstr:%s, count:%d, lpDx:%p\n",
          physDev, x, y, flags, lprect, debugstr_w(wstr), count, lpDx));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */

        face = physDev->face;
        if(!face)
        {
            ERR("FreeType face is null\n");
            res = FALSE;
            goto fin;
        }
        
        /* outputs the string in case it is given by glyph indexes
           make locating it in logs much easier */
        if(TRACE_ON(dibdrv) && flags & ETO_GLYPH_INDEX)
        {
            WCHAR a = 'A';
            WCHAR *str = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR)*(count+1));
            int delta = a - pFT_Get_Char_Index( face, a);
            int i;
            memcpy(str, wstr, sizeof(WCHAR)*count);
            for(i = 0; str[i] && i < count; i++)
                str[i] += delta;
            TRACE("String: '%s'\n", debugstr_w(str));
            HeapFree(GetProcessHeap(), 0, str);
        }

        /* gets background pixel value for opaque text */
        backPixel = physDev->backgroundColor;
    
        /* gets font data, etc */
        GetObjectW(GetCurrentObject(physDev->hdc, OBJ_FONT), sizeof(lf), &lf);
        
        /* convert sizes to device space */
        w = abs(lf.lfWidth); h = abs(lf.lfHeight);
        _DIBDRV_Sizes_ws2ds(physDev, &w, &h);
 
        /* loop on all clip region rectangles */
        r = physDev->regionRects;
        for(iRec = 0; iRec < physDev->regionRectCount; iRec++, r++)
        {
            /* if clipped on text rect, intersect with current region */
            if(flags & ETO_CLIPPED)
            {
                if(!_DIBDRV_IntersectRect(&clipRec, r, lprect))
                    continue;
            }
            else
                memcpy(&clipRec, r, sizeof(RECT));
                
            /* if opaque, paint the font background */
            if(flags & ETO_OPAQUE)
            {
                int iLine;
                RECT tr;
                
                /* clips text rectangle */
                if(_DIBDRV_IntersectRect(&tr, r, lprect))
                {
                    /* paints the backgound */
                    for(iLine = tr.top; iLine < tr.bottom; iLine++)
                        physDev->physBitmap->funcs->SolidHLine(physDev->physBitmap,
                            tr.left, tr.right-1, iLine, 0, backPixel);
                }
            }

            /* sets character pixel size */
            error = pFT_Set_Pixel_Sizes(face, w, h);
            if(error)
                ONCE(ERR("Couldn't set char size to (%d,%d), code %d\n", lf.lfWidth, lf.lfHeight, error));

            /* transformation matrix and vector */
            start.x = 0;
            start.y = 0;
            if(lf.lfEscapement != 0)
            {
                cosEsc = cos(lf.lfEscapement * M_PI / 1800);
                sinEsc = sin(lf.lfEscapement * M_PI / 1800);
            }
            else
            {
                cosEsc = 1;
                sinEsc = 0;
            }
            matrix.xx = (FT_Fixed)( cosEsc * 0x10000L );
            matrix.xy = (FT_Fixed)(-sinEsc * 0x10000L );
            matrix.yx = (FT_Fixed)( sinEsc * 0x10000L );
            matrix.yy = (FT_Fixed)( cosEsc * 0x10000L );
         
            /* outputs characters one by one */
            wstrPnt = wstr;
            for ( n = 0; n < count; n++ )
            {
                /* retrieve glyph index from character code */
                if(flags & ETO_GLYPH_INDEX)
                    glyph_index = *wstrPnt++;
                else
                    glyph_index = pFT_Get_Char_Index( face, *wstrPnt++);

                /* load glyph image into the slot (erase previous one) */
                error = pFT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
                if(error)
                {
                    ERR("Couldn't load glyph at index %d\n", glyph_index);
                    /* ignore errors */
                    continue;
                }
                error = pFT_Get_Glyph(face->glyph, &glyph);
                if ( error )
                {
                    FIXME("Couldn't get glyph\n");
                    continue; 
                }
         
                /* apply transformation to glyph */
                if ( glyph->format != FT_GLYPH_FORMAT_BITMAP )
                    pFT_Glyph_Transform(glyph, &matrix, &start ); 
                        
                /* gets advance BEFORE transforming... */
                dx = glyph->advance.x;
                dy = glyph->advance.y;
                 
                /* convert to an anti-aliased bitmap, if needed */
                if ( glyph->format != FT_GLYPH_FORMAT_BITMAP )
                {
                    error = pFT_Glyph_To_Bitmap(
                        &glyph,
    #ifdef DIBDRV_ANTIALIASED_FONTS        
                        physDev->physBitmap->bitCount > 8 ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO,
    #else
                        FT_RENDER_MODE_MONO,
    #endif
                        0, /* no additional translation */
                        1  /* destroy copy in "image" */
                    );  

                    /* ignore errors */
                    if ( error )
                    {
                        FIXME("Couldn't render glyph\n");
                        pFT_Done_Glyph(glyph);
                        continue;
                    }
                }
                bitmapGlyph = (FT_BitmapGlyph)glyph;
                
                /* convert the bitmap in an 8bpp 1 byte aligned one if needed */
                bitmap = &bitmapGlyph->bitmap;
                if(bitmap->pixel_mode != FT_PIXEL_MODE_GRAY)
                {
                    pFT_Bitmap_New(&bitmap8);
                    if(pFT_Bitmap_Convert(glyph->library, bitmap, &bitmap8, 1))
                    {
                        FIXME("Couldn't convert bitmap to 8 bit grayscale\n");
                        pFT_Done_Glyph(glyph);
                        continue;
                    }
                    bitmap = &bitmap8;
                }

                /* now, draw to our target surface */
                physDev->physBitmap->funcs->FreetypeBlit(physDev, x+bitmapGlyph->left, y-bitmapGlyph->top, &clipRec, bitmap);
                
                /* frees converted bitmap, if any */
                if(bitmap != &bitmapGlyph->bitmap)
                    pFT_Bitmap_Done(glyph->library, bitmap);

                /* increment pen position */
                x += dx>>16;
                y -= dy>>16;

                pFT_Done_Glyph(glyph);
                
                res = TRUE;
            }
        } /* end region rects loop */
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pExtTextOut(physDev->X11PhysDev, x, y, flags, lprect,
                                                   wstr, count, lpDx);
    }
fin:
    return res;
}

/***********************************************************************
 *           DIBDRV_GetTextExtentExPoint
 */
BOOL DIBDRV_GetTextExtentExPoint( DIBDRVPHYSDEV *physDev, LPCWSTR str, INT count,
                                  INT maxExt, LPINT lpnFit, LPINT alpDx, LPSIZE size )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, str:%s, count:%d, maxExt:%d, lpnFit:%p, alpDx:%p, size:%p\n",
        physDev, debugstr_w(str), count, maxExt, lpnFit, alpDx, size));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetTextExtentExPoint(physDev->X11PhysDev, str, count, maxExt,
                                                                lpnFit, alpDx, size);
    }
    return res;
}

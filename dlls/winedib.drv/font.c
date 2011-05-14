/*
 * DIBDRV font objects
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
 *
 */

#include "config.h"
#include "wine/port.h"

#include "dibdrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/**********************************************************************
 *          DIBDRV_SetTextColor
 */
COLORREF DIBDRV_SetTextColor( DIBDRVPHYSDEV *physDev, COLORREF color )
{
    COLORREF res;
    INT r, g, b;
    INT i;
    
    MAYBE(TRACE("physDev:%p, color:%08x\n", physDev, color));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */

        /* do nothing if color is the same as actual one */
        if(color == physDev->textColor)
            return color;

        /* stores old color */
        res = physDev->textColor;
        
        /* fills the text color table used on antialiased font display */
        if(physDev->physBitmap.funcs)
        {
            r = GetRValue(color);
            g = GetGValue(color);
            b = GetBValue(color);
            for(i = 0; i < 256; i++)
            {
                physDev->textColorTable[i] = physDev->physBitmap.funcs->ColorToPixel(&physDev->physBitmap, RGB(
                    MulDiv(r, i, 256),
                    MulDiv(g, i, 256),
                    MulDiv(b, i, 256)
                ));               
            }
        }
        
        /* returns previous text color */
        return res;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetTextColor(physDev->X11PhysDev, color);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_SelectFont
 */
HFONT DIBDRV_SelectFont( DIBDRVPHYSDEV *physDev, HFONT hfont, GdiFont *gdiFont )
{
    HFONT res;
    FT_Int      i;
    FT_Error    error;
    FT_CharMap  charmap = NULL;
    LOGFONTW lf;
    
    MAYBE(TRACE("physDev:%p, hfont:%p, gdiFont:%p\n", physDev, hfont, gdiFont));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */

        /* gets font information */
        GetObjectW(hfont, sizeof(lf), &lf);
        MAYBE(TRACE("Font is : '%s'\n", debugstr_w(lf.lfFaceName)));
        
        /* FIXME: just handles gdifont, don't know if it needs to handle hfont too
           BTW, still don't know how to get FT_Face from non-gdi font here
        */
        if(!gdiFont)
        {
            FIXME("No gdi font - unhandled by now.\n");
            return hfont;
        }

        physDev->face = gdiFont->ft_face;
        if(!physDev->face)
        {
            FIXME("Error, null Ft_Face\n");
            return hfont;
        }

        /* setup the correct charmap.... maybe */
        for (i = 0; i < physDev->face->num_charmaps; ++i)
        {
            if (physDev->face->charmaps[i]->platform_id != TT_PLATFORM_MICROSOFT)
            continue;

            if (physDev->face->charmaps[i]->encoding_id == TT_MS_ID_UNICODE_CS)
            {
                charmap = physDev->face->charmaps[i];
                break;
            }

            if (charmap == NULL)
            {
                WARN("Selected fallout charmap #%d\n", i);
                charmap = physDev->face->charmaps[i];
            }
        }
        if (charmap == NULL)
        {
            WARN("No Windows character map found\n");
            charmap = physDev->face->charmaps[0];
            return 0;
        }

        error = pFT_Set_Charmap(physDev->face, charmap);
        if (error != FT_Err_Ok)
        {
            ERR("%s returned %i\n", "FT_Set_Charmap", error);
            return 0;
        }

        /* we use GDI fonts, so just return false */
        return 0;

    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSelectFont(physDev->X11PhysDev, hfont, gdiFont);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_EnumDeviceFonts
 */
BOOL DIBDRV_EnumDeviceFonts( DIBDRVPHYSDEV *physDev, LPLOGFONTW plf,
                             FONTENUMPROCW proc, LPARAM lp )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, plf:%p, proc:%p, lp:%lx\n", physDev, plf, proc, lp));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = 0;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pEnumDeviceFonts(physDev->X11PhysDev, plf, proc, lp);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetTextMetrics
 */
BOOL DIBDRV_GetTextMetrics( DIBDRVPHYSDEV *physDev, TEXTMETRICW *metrics )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, metrics:%p\n", physDev, metrics));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = 0;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetTextMetrics(physDev->X11PhysDev, metrics);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetCharWidth
 */
BOOL DIBDRV_GetCharWidth( DIBDRVPHYSDEV *physDev, UINT firstChar, UINT lastChar,
                          LPINT buffer )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, firstChar:%d, lastChar:%d, buffer:%pn", physDev, firstChar, lastChar, buffer));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = 0;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetCharWidth(physDev->X11PhysDev, firstChar, lastChar, buffer);
    }
    return res;
}

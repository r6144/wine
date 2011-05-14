/*
 * Truetype font functions
 *
 * Copyright 2008 Massimo Del Fedele
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

#define MAKE_FUNCPTR(f) typeof(f) * p##f = NULL;
MAKE_FUNCPTR(FT_Done_Face)
MAKE_FUNCPTR(FT_Done_FreeType)
MAKE_FUNCPTR(FT_Get_Char_Index)
MAKE_FUNCPTR(FT_Get_Glyph_Name)
MAKE_FUNCPTR(FT_Get_Sfnt_Name)
MAKE_FUNCPTR(FT_Get_Sfnt_Name_Count)
MAKE_FUNCPTR(FT_Get_Sfnt_Table)
MAKE_FUNCPTR(FT_Init_FreeType)
MAKE_FUNCPTR(FT_Load_Glyph)
MAKE_FUNCPTR(FT_Load_Char)
MAKE_FUNCPTR(FT_Get_Glyph)
MAKE_FUNCPTR(FT_Glyph_Copy)
MAKE_FUNCPTR(FT_Glyph_To_Bitmap)
MAKE_FUNCPTR(FT_Done_Glyph)
MAKE_FUNCPTR(FT_New_Face)
MAKE_FUNCPTR(FT_Set_Charmap)
MAKE_FUNCPTR(FT_Set_Char_Size)
MAKE_FUNCPTR(FT_Set_Pixel_Sizes)
MAKE_FUNCPTR(FT_Get_First_Char)
MAKE_FUNCPTR(FT_Render_Glyph)
MAKE_FUNCPTR(FT_Glyph_Transform)
MAKE_FUNCPTR(FT_Bitmap_New)
MAKE_FUNCPTR(FT_Bitmap_Done)
MAKE_FUNCPTR(FT_Bitmap_Convert)
#undef MAKE_FUNCPTR

/* freetype initialization flag */
static BOOL FreeType_Initialized = FALSE;

/* freetype dll handle */
static void *ft_handle = NULL;

/* freetype library handle */
FT_Library DIBDRV_ftLibrary = NULL;

/* initialize freetype library */
BOOL _DIBDRV_FreeType_Init(void)
{
    FT_Int error;
    
    ft_handle = wine_dlopen(SONAME_LIBFREETYPE, RTLD_NOW, NULL, 0);
    if(!ft_handle)
    {
        WINE_MESSAGE(
            "Wine cannot find the FreeType font library.  To enable Wine to\n"
            "use TrueType fonts please install a version of FreeType greater than\n"
            "or equal to 2.0.5.\n"
            "http://www.freetype.org\n");
        return FALSE;
    }

#define LOAD_FUNCPTR(f) if((p##f = wine_dlsym(ft_handle, #f, NULL, 0)) == NULL) goto sym_not_found;
    LOAD_FUNCPTR(FT_Done_Face)
    LOAD_FUNCPTR(FT_Done_FreeType)
    LOAD_FUNCPTR(FT_Get_Char_Index)
    LOAD_FUNCPTR(FT_Get_Glyph_Name)
    LOAD_FUNCPTR(FT_Get_Sfnt_Name)
    LOAD_FUNCPTR(FT_Get_Sfnt_Name_Count)
    LOAD_FUNCPTR(FT_Get_Sfnt_Table)
    LOAD_FUNCPTR(FT_Init_FreeType)
    LOAD_FUNCPTR(FT_Load_Glyph)
    LOAD_FUNCPTR(FT_Load_Char)
    LOAD_FUNCPTR(FT_Get_Glyph)
    LOAD_FUNCPTR(FT_Glyph_Copy)
    LOAD_FUNCPTR(FT_Glyph_To_Bitmap)
    LOAD_FUNCPTR(FT_Done_Glyph)
    LOAD_FUNCPTR(FT_New_Face)
    LOAD_FUNCPTR(FT_Set_Charmap)
    LOAD_FUNCPTR(FT_Set_Char_Size)
    LOAD_FUNCPTR(FT_Set_Pixel_Sizes)
    LOAD_FUNCPTR(FT_Get_First_Char)
    LOAD_FUNCPTR(FT_Render_Glyph)
    LOAD_FUNCPTR(FT_Glyph_Transform)
    LOAD_FUNCPTR(FT_Bitmap_New)
    LOAD_FUNCPTR(FT_Bitmap_Done)
    LOAD_FUNCPTR(FT_Bitmap_Convert)
#undef LOAD_FUNCPTR

    error = pFT_Init_FreeType(&DIBDRV_ftLibrary);
    if (error != FT_Err_Ok)
    {
        ERR("%s returned %i\n", "FT_Init_FreeType", error);
        wine_dlclose(ft_handle, NULL, 0);
        return FALSE;
    }

    /* marks library as initialized */
    FreeType_Initialized = TRUE;

    return TRUE;
    
sym_not_found:
    WINE_MESSAGE(
      "Wine cannot find certain functions that it needs inside the FreeType\n"
      "font library.  To enable Wine to use TrueType fonts please upgrade\n"
      "FreeType to at least version 2.0.5.\n"
      "http://www.freetype.org\n");
    wine_dlclose(ft_handle, NULL, 0);
    ft_handle = NULL;
    return FALSE;
}

/* terminates freetype library */
void _DIBDRV_FreeType_Terminate(void)
{
    if(!FreeType_Initialized)
        return;
    
    /* terminates and unload freetype library */
    pFT_Done_FreeType(DIBDRV_ftLibrary);
    wine_dlclose(ft_handle, NULL, 0);
    ft_handle = NULL;
    FreeType_Initialized = FALSE;
    
}

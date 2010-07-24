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
#ifndef __WINE_DIBDRV_FREETYPE_H
#define __WINE_DIBDRV__FREETYPE_H

/* freetype library for font support */
#ifdef HAVE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

/* freetype library handle */
extern FT_Library DIBDRV_ftLibrary;

/******************************************************************************************/
/*                                     FREETYPE STUFFS                                    */
/* grabbed from winex11.drv/freetype.c                                                    */
/******************************************************************************************/

/* This is basically a copy of FT_Bitmap_Size with an extra element added */
typedef struct {
    FT_Short height;
    FT_Short width;
    FT_Pos size;
    FT_Pos x_ppem;
    FT_Pos y_ppem;
    FT_Short internal_leading;
} Bitmap_Size;

/* FT_Bitmap_Size gained 3 new elements between FreeType 2.1.4 and 2.1.5
   So to let this compile on older versions of FreeType we'll define the
   new structure here. */
typedef struct {
    FT_Short height, width;
    FT_Pos size, x_ppem, y_ppem;
} My_FT_Bitmap_Size;

struct enum_data
{
    ENUMLOGFONTEXW elf;
    NEWTEXTMETRICEXW ntm;
    DWORD type;
};

typedef struct tagFace {
    struct list entry;
    WCHAR *StyleName;
    char *file;
    void *font_data_ptr;
    DWORD font_data_size;
    FT_Long face_index;
    FONTSIGNATURE fs;
    FONTSIGNATURE fs_links;
    DWORD ntmFlags;
    FT_Fixed font_version;
    BOOL scalable;
    Bitmap_Size size;     /* set if face is a bitmap */
    BOOL external; /* TRUE if we should manually add this font to the registry */
    struct tagFamily *family;
    /* Cached data for Enum */
    struct enum_data *cached_enum_data;
} Face;

typedef struct tagFamily {
    struct list entry;
    const WCHAR *FamilyName;
    struct list faces;
} Family;

typedef struct {
    GLYPHMETRICS gm;
    INT adv; /* These three hold to widths of the unrotated chars */
    INT lsb;
    INT bbx;
    BOOL init;
} GM;

typedef struct {
    FLOAT eM11, eM12;
    FLOAT eM21, eM22;
} FMAT2;

typedef struct {
    DWORD hash;
    LOGFONTW lf;
    FMAT2 matrix;
    BOOL can_use_bitmap;
} FONT_DESC;

typedef struct tagHFONTLIST {
    struct list entry;
    HFONT hfont;
} HFONTLIST;

typedef struct {
    struct list entry;
    Face *face;
    struct tagGdiFont *font;
} CHILD_FONT;

typedef struct tagGdiFont {
    struct list entry;
    GM **gm;
    DWORD gmsize;
    struct list hfontlist;
    OUTLINETEXTMETRICW *potm;
    DWORD total_kern_pairs;
    KERNINGPAIR *kern_pairs;
    struct list child_fonts;

    /* the following members can be accessed without locking, they are never modified after creation */
    FT_Face ft_face;
    struct font_mapping *mapping;
    LPWSTR name;
    int charset;
    int codepage;
    BOOL fake_italic;
    BOOL fake_bold;
    BYTE underline;
    BYTE strikeout;
    INT orientation;
    FONT_DESC font_desc;
    LONG aveWidth, ppem;
    double scale_y;
    SHORT yMax;
    SHORT yMin;
    DWORD ntmFlags;
    FONTSIGNATURE fs;
    struct tagGdiFont *base_font;
    VOID *GSUB_Table;
    DWORD cache_num;
} GdiFont;

/* initialize freetype library */
BOOL _DIBDRV_FreeType_Init(void);

/* terminates freetype library */
void _DIBDRV_FreeType_Terminate(void);

#define MAKE_FUNCPTR(f) extern typeof(f) * p##f;
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
#undef MAKE_FUNCPTR

#endif /* HAVE_FREETYPE */

#endif

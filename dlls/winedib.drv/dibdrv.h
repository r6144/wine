/*
 * DIB driver private definitions
 *
 * Copyright 2009 Massimo Del Fedele
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

#ifndef __WINE_DIBDRV_H
#define __WINE_DIBDRV_H

#include <stdarg.h>
#include <stdlib.h>
#include <X11/Xlib.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wingdi.h"
#include "wine/list.h"
#include "wine/library.h"
#include "wine/debug.h"
#include "wingdi.h"
#include "winreg.h"
#include "wine/winbase16.h" /* GlobalLock16 */

#include "freetype.h"

/* data structures needed to access opaque pointers
 * defined in gdi32.h */
#include "dibdrv_gdi32.h"

/* enable this if you want debugging (i.e. TRACEs) output */
#define DIBDRV_ENABLE_MAYBE

/* enable this if you want antialiased fonts */
#define DIBDRV_ANTIALIASED_FONTS

/* provide a way to make debugging output appear
   only once. Usage example:
   ONCE(FIXME("Some message\n")); */
#define ONCE(x) \
{ \
    static BOOL done = FALSE; \
    if(!done) \
    { \
        done = TRUE; \
        x; \
    } \
}

/* provide a way to make debugging output appear
   only if enabled here. Can speed up stuffs
   avoiding long traces.Usage example:
   MAYBE(TRACE("Some message\n")); */
#ifdef DIBDRV_ENABLE_MAYBE
#define MAYBE(x) x
#else
#define MAYBE(x)
#endif


/* extra stock object: default 1x1 bitmap for memory DCs
   grabbed from gdi_private.h */
#define DEFAULT_BITMAP (STOCK_LAST+1)

struct _DIBDRVBITMAP;
struct _DIBDRVPHYSDEV;
typedef struct _DIBDRV_PRIMITIVE_FUNCS
{
    /* color to pixel data conversion */
    DWORD (* ColorToPixel)     (const struct _DIBDRVBITMAP *bmp, COLORREF color);
    
    /* pixel primitives */
    void* (* GetPixelPointer)  (const struct _DIBDRVBITMAP *bmp, int x, int y);
    void  (* SetPixel)         (      struct _DIBDRVBITMAP *bmp, int x, int y, DWORD and, DWORD xor);
    DWORD (* GetPixel)         (const struct _DIBDRVBITMAP *bmp, int x, int y);
    
    /* line drawing primitives */
    void  (* SolidHLine)       (      struct _DIBDRVBITMAP *bmp, int x1, int x2, int y, DWORD and, DWORD xor);
    void  (* PatternHLine)     (      struct _DIBDRVBITMAP *bmp, int x1, int x2, int y, const void *and, const void *xor, DWORD offset, DWORD count);
    void  (* SolidVLine)       (      struct _DIBDRVBITMAP *bmp, int x, int y1, int y2, DWORD and, DWORD xor);
    
    /* bitmap conversion helpers */
    BOOL  (* GetLine)          (const struct _DIBDRVBITMAP *bmp, int line, int startx, int width, void *buf);
    BOOL  (* PutLine)          (      struct _DIBDRVBITMAP *bmp, int line, int startx, int width, void *buf);
    
    /* BitBlt primitives */
    BOOL  (* AlphaBlend)       (      struct _DIBDRVPHYSDEV *physDevDst, int xDst, int yDst, int widthDst, int heightDst,
                                const struct _DIBDRVPHYSDEV *physDevSrc, int xSrc, int ySrc, int widthSrc, int heightSrc, BLENDFUNCTION blendFn );
    BOOL  (* BitBlt)           (      struct _DIBDRVPHYSDEV *physDevDst, int xDst, int yDst, int width, int height,
                                const struct _DIBDRVPHYSDEV *physDevSrc, int xSrc, int ySrc, DWORD rop );
    BOOL  (* StretchBlt)       (      struct _DIBDRVPHYSDEV *physDevDst, int xDst, int yDst, int widthDst, int heightDst,
                                const struct _DIBDRVPHYSDEV *physDevSrc, int xSrc, int ySrc, int widthSrc, int heightSrc, DWORD rop );
                                
    /* font drawing helper */
    void  (* FreetypeBlit)     (      struct _DIBDRVPHYSDEV *physDev, int x, int y, RECT *clipRec, FT_Bitmap *bmp);

} DIBDRV_PRIMITIVE_FUNCS;

extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB32_RGB;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB32_BITFIELDS;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB24;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB16_RGB;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB16_BITFIELDS;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB8;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB4;
extern DIBDRV_PRIMITIVE_FUNCS DIBDRV_funcs_DIB1;

/* DIB bitmaps formats */
typedef enum _DIBFORMAT
{
    DIBFMT_UNKNOWN          =   0,
    DIBFMT_DIB1             =   1,
    DIBFMT_DIB4             =   2,
    DIBFMT_DIB4_RLE         =   3,
    DIBFMT_DIB8             =   4,
    DIBFMT_DIB8_RLE         =   5,
    DIBFMT_DIB16_RGB        =   6,
    DIBFMT_DIB16_BITFIELDS  =   7,
    DIBFMT_DIB24            =   8,
    DIBFMT_DIB32_RGB        =   9,
    DIBFMT_DIB32_BITFIELDS  =  10
} DIBFORMAT;

/* DIB driver's generic bitmap structure */
typedef struct _DIBDRVBITMAP
{
    /* bitmap format of dib */
    DIBFORMAT format;

    /* pointer to top left corner of bitmap */
    void *bits;

    /* flags indicating if bits array is owned
       by the bitmap */
    BOOL ownsBits;

    /* bitmap dimensions */
    int width;
    int height;

    /* bitmap stride (== width in bytes) of a bitmap line */
    /* negative for a bottom-up bitmap */
    int stride;

    /* number of bits/pixel in bitmap */
    int bitCount;

    /* calculated numbers for bitfields */
    
    /* bitfields masks */
    DWORD redMask, greenMask, blueMask;
    /* shifting required within a COLORREF's BYTE */
    int redShift, greenShift, blueShift;
    /* size of the fields */
    int redLen, greenLen, blueLen;

    /* color table and its size */
    RGBQUAD *colorTable;
    DWORD colorTableSize;
    
    /* lightest color index, for monochrome bitmaps */
    int lightColor;
    
    /* flag indicating that color table has been grabbed */
    BOOL colorTableGrabbed;

    /* primitive function pointers */
    DIBDRV_PRIMITIVE_FUNCS *funcs;

} DIBDRVBITMAP;

/* dash patterns */
typedef struct _DASHPATTERN
{
    DWORD count;
    DWORD dashes[6];

} DASHPATTERN;

/* DIB driver physical device */
typedef struct _DIBDRVPHYSDEV
{
    /* X11 driver physical device */
    PHYSDEV X11PhysDev;
    
    /* HDC associated with physDev */
    HDC hdc;
    
    /* is a DIB selected into DC ? */
    BOOL hasDIB;
    
    /* currently selected HBITMAP */
    HBITMAP hbitmap;
    
    /* physical bitmap */
    DIBDRVBITMAP *physBitmap;

    /* active ROP2 */
    INT rop2;
    
    /* clipping region and its rectangles */
    HRGN region;
    RGNDATA *regionData;
    RECT *regionRects;
    int regionRectCount;

    /* background color and active ROP2 precalculated
       AND and XOR values for it */
    COLORREF backgroundColor;
    DWORD backgroundAnd, backgroundXor;
    
    /* pen color and active ROP2 precalculated
       AND and XOR values for it */
    COLORREF penColorref;
    DWORD penColor;
    DWORD penAnd, penXor;
    const DASHPATTERN *penPattern;
    DWORD curDash, leftInDash;
    enum MARKSPACE { mark, space } markSpace;
    
    /* pen style */
    UINT penStyle;
    
    /* pen drawing functions */
    void (* penHLine)  (struct _DIBDRVPHYSDEV *physDev, int x1, int x2, int y);
    void (* penVLine)  (struct _DIBDRVPHYSDEV *physDev, int x, int y1, int y2);
    void (* penLine)   (struct _DIBDRVPHYSDEV *physDev, int x1, int y1, int x2, int y2);
    void (* brushHLine)(struct _DIBDRVPHYSDEV *physDev, int x1, int x2, int y);

    /* brush color and active ROP2 precalculated
       AND and XOR values for it */
    COLORREF brushColorref;
    DWORD brushColor;
    DWORD brushAnd, brushXor;
    DWORD *brushAnds, *brushXors;
    
    /* brush style */
    UINT brushStyle;
    
    /* brush bitmap, if needed, and its converted/resized cache copy */
    BOOL isBrushBitmap;
    DIBDRVBITMAP *brushBitmap;
    DIBDRVBITMAP *brushBmpCache;
    
    /* text color */
    COLORREF textColor;
    COLORREF textBackground;

#ifdef DIBDRV_ANTIALIASED_FONTS
    /* text color table for antialiased fonts */
    COLORREF textColorTable[256];
#endif

    /* freetype face associated to current DC HFONT */
    FT_Face face;

} DIBDRVPHYSDEV;


/* *********************************************************************
 * DISPLAY DRIVER ACCESS FUNCTIONS
 * ********************************************************************/

/* LoadDisplayDriver
 * Loads display driver - partially grabbed from gdi32 */
DC_FUNCTIONS *_DIBDRV_LoadDisplayDriver(void);

/* FreeDisplayDriver
   Frees resources allocated by Display driver */
void _DIBDRV_FreeDisplayDriver(void);

/* GetDisplayDriver
   Gets a pointer to display drives'function table */
inline DC_FUNCTIONS *_DIBDRV_GetDisplayDriver(void);

/* *********************************************************************
 * ROP2 AND OTHER DRAWING RELATED FUNCTIONS
 * ********************************************************************/

void _DIBDRV_CalcAndXorMasks(INT rop, DWORD color, DWORD *and, DWORD *xor);

inline void _DIBDRV_rop32(DWORD *ptr, DWORD and, DWORD xor);
inline void _DIBDRV_rop16(WORD *ptr, WORD and, WORD xor);
inline void _DIBDRV_rop8(BYTE *ptr, BYTE and, BYTE xor);

void _DIBDRV_ResetDashOrigin(DIBDRVPHYSDEV *physDev);

/* *********************************************************************
 * ROP2 FUNCTIONS
 * ********************************************************************/

/* the ROP3 operations
   this is a BIG case block; beware that some
   commons ROP3 operations will be optimized
   from inside blt routines */
DWORD _DIBDRV_ROP3(DWORD p, DWORD s, DWORD d, BYTE rop);

/* *********************************************************************
 * PHYSICAL BITMAP  FUNCTIONS
 * ********************************************************************/

/* gets human-readable dib format name */
const char *_DIBDRVBITMAP_GetFormatName(DIBDRVBITMAP const *bmp);

/* sets/gets bits of a DIBDRVBITMAP taking in account if it's a top down
   or a bottom-up DIB */
void _DIBDRVBITMAP_Set_Bits(DIBDRVBITMAP *dib, void *bits, BOOL owns);
void *_DIBDRVBITMAP_Get_Bits(DIBDRVBITMAP *dib);

/* calculates and sets the lightest color for monochrome bitmaps */
int _DIBDRVBITMAP_GetLightestColorIndex(DIBDRVBITMAP *dib);

/* initialize or create dib from a bitmap : 
    dib           dib being initialized
    bi            source BITMAPINFOHEADER with required DIB format info
    bit_fields    color masks
    color_table   color table, if any
    bits          pointer to image data array
    NOTE : DIBDRVBITMAP doesn't owns bits, but do own color table */
BOOL _DIBDRVBITMAP_InitFromBMIH(DIBDRVBITMAP *dib, const BITMAPINFOHEADER *bi, const DWORD *bit_fields,
                                const RGBQUAD *color_table, void *bits);
DIBDRVBITMAP *_DIBDRVBITMAP_CreateFromBMIH(const BITMAPINFOHEADER *bi, const DWORD *bit_fields,
                     const RGBQUAD *colorTable, void *bits);

BOOL _DIBDRVBITMAP_InitFromBitmapinfo(DIBDRVBITMAP *dib, const BITMAPINFO *bmi, void *bits);
DIBDRVBITMAP *_DIBDRVBITMAP_CreateFromBitmapinfo(const BITMAPINFO *bmi, void *bits);

/* initializes a DIBRDVBITMAP copying it from a source one
   Parameters :
      dib       destination DIBDRVBITMAP
      src       source DIBDRVBITMAP
      copy      TRUE->copy source pixel array FALSE->link to source pixel array */
BOOL _DIBDRVBITMAP_InitFromDibdrvbitmap(DIBDRVBITMAP *dib, const DIBDRVBITMAP *src, BOOL copy);

/* creates a DIBRDVBITMAP copying format info from a source one
   Parameters :
      dib            destination DIBDRVBITMAP
      src            source DIBDRVBITMAP
      widht, height  sizes of newly created bitmap  */
BOOL _DIBDRVBITMAP_CreateFromDibdrvbitmap(DIBDRVBITMAP *dib, const DIBDRVBITMAP *src, int width, int height);

/* allocates a new DIBDTVBITMAP */
DIBDRVBITMAP *_DIBDRVBITMAP_New(void);

/* Frees and de-allocates a DIBDRVBITMAP structure data */
void _DIBDRVBITMAP_Free(DIBDRVBITMAP *bmp);

/* Clears a DIBDRVBITMAP structure data
   WARNING : doesn't free anything */
void _DIBDRVBITMAP_Clear(DIBDRVBITMAP *bmp);

/* checks whether the format of 2 DIBs are identical
   it checks the pixel bit count and the color table size
   and content, if needed */
BOOL _DIBDRVBITMAP_FormatMatch(const DIBDRVBITMAP *d1, const DIBDRVBITMAP *d2);

/* convert a given dib into another format given by 'format' parameter */
BOOL _DIBDRVBITMAP_Convert(DIBDRVBITMAP *dst, const DIBDRVBITMAP *src, const DIBDRVBITMAP *format);

/* creates a solid-filled DIB of given color and format
   DIB format is given by 'format' parameter */
BOOL _DIBDRVBITMAP_CreateSolid(DIBDRVBITMAP *bmp, DIBDRVBITMAP *format, int width, int height, DWORD Color);

/* expands horizontally a bitmap to reach a minimum size,
   keeping its width as a multiple of a base width
   Used to widen brushes in order to optimize blitting */
BOOL _DIBDRVBITMAP_ExpandHoriz(DIBDRVBITMAP *dib, int baseWidth, int minWidth);

/* *********************************************************************
 * BITMAP LIST MANAGEMENT FUNCTIONS
 * ********************************************************************/

/* initializes bitmap list -- to be called at process attach */
void _BITMAPLIST_Init(void);

/* terminates bitmap list -- to be called at process detach */
void _BITMAPLIST_Terminate(void);

/* adds a DIB to the list - it adds it on top, as
   usually most recently created DIBs are used first */
BOOL _BITMAPLIST_Add(HBITMAP hbmp, DIBDRVBITMAP *bmp);

/* removes a DIB from the list */
DIBDRVBITMAP *_BITMAPLIST_Remove(HBITMAP hbmp);

/* scans list for a DIB */
DIBDRVBITMAP *_BITMAPLIST_Get(HBITMAP hbmp);

/* *********************************************************************
 * DIB <--> DDB CONVERSION ROUTINES
 * ********************************************************************/

/***********************************************************************
 * Creates DDB that is compatible with source hdc.
 * hdc is the HDC on where the DIB MUST be selected in
 * srcBmp is the source DIB
 * startScan and scanLines specify the portion of DIB to convert
 * in order to avoid unneeded conversion of large DIBs on blitting
 * 
 * NOTE : the srcBmp DIB MUST NOT be selected in any DC */
HBITMAP _DIBDRV_ConvertDIBtoDDB( HDC hdc, HBITMAP srcBmp, int startScan, int scanLines );

/***********************************************************************
 * Creates DIB that is compatible with the target hdc.
 * startScan and scanLines specify the portion of DDB to convert
 * in order to avoid unneeded conversion of large DDBs on blitting
 * 
 * NOTE : the srcBmp DDB MUST NOT be selected in any DC */
HBITMAP _DIBDRV_ConvertDDBtoDIB( HDC hdc, HBITMAP srcBmp, int startScan, int scanLines );

/***********************************************************************
 * Creates DIB that is compatible with the target hdc for a device (non memory) source DC */
HBITMAP _DIBDRV_ConvertDevDDBtoDIB( HDC hdcSrc, HDC hdcDst, int xSrc, int ySrc, int width, int height );

/* *********************************************************************
 * QUERY FUNCTIONS
 * ********************************************************************/

/***********************************************************************
 *           DIBDRV_GetDeviceCaps */
INT DIBDRV_GetDeviceCaps( DIBDRVPHYSDEV *physDev, INT cap );

/* *********************************************************************
 * GEOMETRIC UTILITIES
 * ********************************************************************/

/* intersect 2 rectangles (just to not use USER32 one...) */
BOOL _DIBDRV_IntersectRect(RECT *d, const RECT *s1, const RECT *s2);

/* converts positions  from Word space to Device space */
void _DIBDRV_Position_ws2ds(DIBDRVPHYSDEV *physDev, int *x, int *y);
void _DIBDRV_Positions_ws2ds(DIBDRVPHYSDEV *physDev, int *x1, int *y1, int *x2, int *y2);

/* converts sizes from Word space to Device space */
void _DIBDRV_Sizes_ws2ds(DIBDRVPHYSDEV *physDev, int *w, int *h);

/* converts a rectangle form Word space to Device space */
void _DIBDRV_Rect_ws2ds(DIBDRVPHYSDEV *physDev, const RECT *src, RECT *dst);

/* converts positions  from Device space to World space */
void _DIBDRV_Position_ds2ws(DIBDRVPHYSDEV *physDev, int *x, int *y);
void _DIBDRV_Positions_ds2ws(DIBDRVPHYSDEV *physDev, int *x1, int *y1, int *x2, int *y2);

/* converts sizes from Device space to World space */
void _DIBDRV_Sizes_ds2ws(DIBDRVPHYSDEV *physDev, int *w, int *h);

/* *********************************************************************
 * COLOR UTILITIES
 * ********************************************************************/

/* maps a colorref to actual color */
COLORREF _DIBDRV_MapColor(DIBDRVPHYSDEV *physDev, COLORREF color);

/* gets nearest color index in DIB palette of a given colorref */
DWORD _DIBDRV_GetNearestColorIndex(const DIBDRVBITMAP *dib, COLORREF color);

/* gets nearest color to DIB palette color */
DWORD _DIBDRV_GetNearestColor(const DIBDRVBITMAP *dib, COLORREF color);

#endif

/*
 * DIB driver initialization functions
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


/* some screen caps */
static unsigned int screen_width;
static unsigned int screen_height;
static unsigned int screen_bpp;
static unsigned int screen_depth;
static RECT virtual_screen_rect;

/* a few dynamic device caps */
static int log_pixels_x;  /* pixels per logical inch in x direction */
static int log_pixels_y;  /* pixels per logical inch in y direction */
static int horz_size;     /* horz. size of screen in millimeters */
static int vert_size;     /* vert. size of screen in millimeters */
static int palette_size;
static int device_init_done;

/* NOTE :
    Removing TC_RA_ABLE avoids bitmapped fonts, so FT_Face is always non-NULL
    Adding TC_VA_ABLE forces to use gdi fonts always, so we can get an FT_Face
*/
unsigned int text_caps = (TC_OP_CHARACTER | TC_OP_STROKE | TC_CP_STROKE |
                          TC_CR_ANY | TC_SA_DOUBLE | TC_SA_INTEGER |
                          TC_SA_CONTIN | TC_UA_ABLE | TC_SO_ABLE /* | TC_RA_ABLE */ | TC_VA_ABLE);
                          /* X11R6 adds TC_SF_X_YINDEP, Xrender adds TC_VA_ABLE */


static const WCHAR dpi_key_name[] = {'S','o','f','t','w','a','r','e','\\','F','o','n','t','s','\0'};
static const WCHAR dpi_value_name[] = {'L','o','g','P','i','x','e','l','s','\0'};

/******************************************************************************
 *      get_dpi
 *
 * get the dpi from the registry
 */
static DWORD get_dpi( void )
{
    DWORD dpi = 96;
    HKEY hkey;

    if (RegOpenKeyW(HKEY_CURRENT_CONFIG, dpi_key_name, &hkey) == ERROR_SUCCESS)
    {
        DWORD type, size, new_dpi;

        size = sizeof(new_dpi);
        if(RegQueryValueExW(hkey, dpi_value_name, NULL, &type, (void *)&new_dpi, &size) == ERROR_SUCCESS)
        {
            if(type == REG_DWORD && new_dpi != 0)
                dpi = new_dpi;
        }
        RegCloseKey(hkey);
    }
    return dpi;
}

/**********************************************************************
 *       device_init
 *
 * Perform initializations needed upon creation of the first device.
 */
static void device_init(void)
{
    Display *display;
    Screen *screen;

    /* opens default X11 Display */
    if( (display = XOpenDisplay(NULL)) == NULL)
        return;

    /* gets default screen */
    screen = XDefaultScreenOfDisplay(display);

    /* gets screen sizes */
    screen_width = XWidthOfScreen(screen);
    screen_height = XHeightOfScreen(screen);

    /* not sure about these ones... */
    screen_bpp = XDefaultDepthOfScreen(screen);
    screen_depth = XPlanesOfScreen(screen);
    virtual_screen_rect.left = 0;
    virtual_screen_rect.top = 0;
    virtual_screen_rect.right = screen_width;
    virtual_screen_rect.bottom = screen_height;

    /* dummy ? */
    palette_size = 0;

    /* Initialize device caps */
    log_pixels_x = log_pixels_y = get_dpi();
    horz_size = MulDiv( screen_width, 254, log_pixels_x * 10 );
    vert_size = MulDiv( screen_height, 254, log_pixels_y * 10 );

    device_init_done = TRUE;
}

/**********************************************************************
 *           DIBDRV_CreateDC
 */
BOOL DIBDRV_CreateDC( HDC hdc, DIBDRVPHYSDEV **pdev, LPCWSTR driver, LPCWSTR device,
                      LPCWSTR output, const DEVMODEW* initData )
{
    DIBDRVPHYSDEV *physDev;
    PHYSDEV X11PhysDev;
    
    MAYBE(TRACE("hdc:%p, pdev:%p, driver:%s, device:%s, output:%s, initData:%p\n",
          hdc, pdev, debugstr_w(driver), debugstr_w(device), debugstr_w(output), initData));

    /* allocates physical device */
    physDev = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DIBDRVPHYSDEV) );
    if (!physDev)
        return FALSE;
        
    /* creates X11 physical device */
    if(!_DIBDRV_GetDisplayDriver()->pCreateDC(hdc, &X11PhysDev, driver, device, output, initData))
    {
        HeapFree(GetProcessHeap(), 0, physDev);
        return FALSE;
    }
    
    /* sets X11 Device pointer in DIB Engine device */
    physDev->X11PhysDev = X11PhysDev;
    
    /* stores the HDC */
    physDev->hdc = hdc;
    
    /* initializes device data (for GetDeviceCaps() )
       on first DC creation */
    if (!device_init_done)
       device_init();
    
    /* stock bitmap selected on DC creation */
    physDev->hbitmap = GetStockObject(DEFAULT_BITMAP);
    
    /* no DIB selected into DC on creation */
    physDev->hasDIB = FALSE;
    
    /* clear physical bitmap */
    _DIBDRVBITMAP_Clear(&physDev->physBitmap);
    
    /* clears pen and brush */
    physDev->rop2 = R2_COPYPEN;

    physDev->backgroundColor = 0;
    _DIBDRV_CalcAndXorMasks(physDev->rop2, 0, &physDev->backgroundAnd, &physDev->backgroundXor);
    
    physDev->penColor = 0;
    _DIBDRV_CalcAndXorMasks(physDev->rop2, 0, &physDev->penAnd, &physDev->penXor);
    
    physDev->brushColor = 0;
    _DIBDRV_CalcAndXorMasks(physDev->rop2, 0, &physDev->brushAnd, &physDev->brushXor);
    physDev->brushAnds = NULL;
    physDev->brushXors = NULL;
    
    physDev->brushStyle = BS_NULL;
    
    physDev->isBrushBitmap = FALSE;
    _DIBDRVBITMAP_Clear(&physDev->brushBitmap);
    _DIBDRVBITMAP_Clear(&physDev->brushBmpCache);
    
    /* text color */
    physDev->textColor = 0;
    physDev->textBackground = 0;

    /* text color table for antialiased fonts */
    memset(physDev->textColorTable, 0, 256);

    /* freetype face associated to current DC HFONT */
    physDev->face = NULL;

    /* sets the result value and returns */
    *pdev = physDev;

    return TRUE;
}

/**********************************************************************
 *           DIBDRV_DeleteDC
 */
BOOL DIBDRV_DeleteDC( DIBDRVPHYSDEV *physDev )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p\n", physDev));

    /* frees X11 device */
    res = _DIBDRV_GetDisplayDriver()->pDeleteDC(physDev->X11PhysDev);
    physDev->X11PhysDev = NULL;
    
    /* frees physical bitmap */
    _DIBDRVBITMAP_Free(&physDev->physBitmap);
    
    /* frees brush bitmap */
    _DIBDRVBITMAP_Free(&physDev->brushBitmap);
    _DIBDRVBITMAP_Free(&physDev->brushBmpCache);
    
    /* free brush ands and xors */
    if(physDev->brushAnds)
    {
        HeapFree(GetProcessHeap(), 0, physDev->brushAnds);
        HeapFree(GetProcessHeap(), 0, physDev->brushXors);
    }
    physDev->brushAnds = NULL;
    physDev->brushXors = NULL;
    
    /* frees DIB Engine device */
    HeapFree(GetProcessHeap(), 0, physDev);
    
    return res;
}

/**********************************************************************
 *           DIBDRV_ExtEscape
 */
INT DIBDRV_ExtEscape( DIBDRVPHYSDEV *physDev, INT escape, INT in_count, LPCVOID in_data,
                      INT out_count, LPVOID out_data )
{
    INT res;
    
    MAYBE(TRACE("physDev:%p, escape:%d, in_count:%d, in_data:%p, out_count:%d, out_data:%p\n",
          physDev, escape, in_count, in_data, out_count, out_data));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pExtEscape(physDev->X11PhysDev, escape, in_count, in_data, out_count, out_data);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pExtEscape(physDev->X11PhysDev, escape, in_count, in_data, out_count, out_data);
    }
    return res;
}

/***********************************************************************
 *           DIBDRV_GetDeviceCaps
 */
INT DIBDRV_GetDeviceCaps( DIBDRVPHYSDEV *physDev, INT cap )
{
    INT res;
    
    MAYBE(TRACE("physDev:%p, cap:%d\n", physDev, cap)); 

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        switch(cap)
        {
        case DRIVERVERSION:
            res = 0x300;
            break;
        case TECHNOLOGY:
            res = DT_RASDISPLAY;
            break;
        case HORZSIZE:
            res = horz_size;
            break;
        case VERTSIZE:
            res = vert_size;
            break;
        case HORZRES:
            res = screen_width;
            break;
        case VERTRES:
            res = screen_height;
            break;
        case DESKTOPHORZRES:
            res = virtual_screen_rect.right - virtual_screen_rect.left;
            break;
        case DESKTOPVERTRES:
            res = virtual_screen_rect.bottom - virtual_screen_rect.top;
            break;
        case BITSPIXEL:
            res = screen_bpp;
            break;
        case PLANES:
            res = 1;
            break;
        case NUMBRUSHES:
            res = -1;
            break;
        case NUMPENS:
            res = -1;
            break;
        case NUMMARKERS:
            res = 0;
            break;
        case NUMFONTS:
            res = 0;
            break;
        case NUMCOLORS:
            /* MSDN: Number of entries in the device's color table, if the device has
             * a color depth of no more than 8 bits per pixel.For devices with greater
             * color depths, -1 is returned. */
            res = (screen_depth > 8) ? -1 : (1 << screen_depth);
            break;
        case CURVECAPS:
            res = (CC_CIRCLES | CC_PIE | CC_CHORD | CC_ELLIPSES | CC_WIDE |
                    CC_STYLED | CC_WIDESTYLED | CC_INTERIORS | CC_ROUNDRECT);
            break;
        case LINECAPS:
            res = (LC_POLYLINE | LC_MARKER | LC_POLYMARKER | LC_WIDE |
                    LC_STYLED | LC_WIDESTYLED | LC_INTERIORS);
            break;
        case POLYGONALCAPS:
            res = (PC_POLYGON | PC_RECTANGLE | PC_WINDPOLYGON | PC_SCANLINE |
                    PC_WIDE | PC_STYLED | PC_WIDESTYLED | PC_INTERIORS);
            break;
        case TEXTCAPS:
            res = text_caps;
            break;
        case CLIPCAPS:
            res = CP_REGION;
            break;
        case RASTERCAPS:
            res = (RC_BITBLT | RC_BANDING | RC_SCALING | RC_BITMAP64 | RC_DI_BITMAP |
                    RC_DIBTODEV | RC_BIGFONT | RC_STRETCHBLT | RC_STRETCHDIB | RC_DEVBITS |
                    (palette_size ? RC_PALETTE : 0));
            break;
        case SHADEBLENDCAPS:
            res = (SB_GRAD_RECT | SB_GRAD_TRI | SB_CONST_ALPHA | SB_PIXEL_ALPHA);
        case ASPECTX:
        case ASPECTY:
            res = 36;
            break;
        case ASPECTXY:
            res = 51;
            break;
        case LOGPIXELSX:
            res = log_pixels_x;
            break;
        case LOGPIXELSY:
            res = log_pixels_y;
            break;
        case CAPS1:
            FIXME("(%p): CAPS1 is unimplemented, will return 0\n", physDev->hdc );
            /* please see wingdi.h for the possible bit-flag values that need
               to be returned. */
            res = 0;
            break;
        case SIZEPALETTE:
            res = palette_size;
            break;
        case NUMRESERVED:
        case COLORRES:
        case PHYSICALWIDTH:
        case PHYSICALHEIGHT:
        case PHYSICALOFFSETX:
        case PHYSICALOFFSETY:
        case SCALINGFACTORX:
        case SCALINGFACTORY:
        case VREFRESH:
        case BLTALIGNMENT:
            res = 0;
            break;
        default:
            FIXME("(%p): unsupported capability %d, will return 0\n", physDev->hdc, cap );
            res = 0;
            break;
        }
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetDeviceCaps(physDev->X11PhysDev, cap);
    }
    return res;
}

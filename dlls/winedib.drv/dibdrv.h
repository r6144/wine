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

/* data structures needed to access opaque pointers
 * defined in gdi32.h */
#include "dibdrv_gdi32.h"

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

/* extra stock object: default 1x1 bitmap for memory DCs
   grabbed from gdi_private.h */
#define DEFAULT_BITMAP (STOCK_LAST+1)

/* DIB driver physical device */
typedef struct _DIBDRVPHYSDEV
{
    /* X11 driver physical device */
    PHYSDEV X11PhysDev;
    
    /* is a DIB selected into DC ? */
    BOOL hasDIB;
    
    /* currently selected HBITMAP */
    HBITMAP hbitmap;

    /* active ROP2 */
    INT rop2;

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

#endif /* __WINE_DIBDRV_H */

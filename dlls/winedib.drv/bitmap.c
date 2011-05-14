/*
 * DIB driver bitmap objects
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


/****************************************************************************
 *       SelectBitmap   (WINEDIB.DRV.@)
 */
HBITMAP DIBDRV_SelectBitmap( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap )
{
    TRACE("physDev:%p, hbitmap:%p\n", physDev, hbitmap);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSelectBitmap(physDev->X11PhysDev, hbitmap);
}

/****************************************************************************
 *        DIBDRV_CreateBitmap
 */
BOOL DIBDRV_CreateBitmap( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, LPVOID bmBits )
{
    TRACE("physDev:%p, hbitmap:%p, bmBits:%p\n", physDev, hbitmap, bmBits);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pCreateBitmap(physDev->X11PhysDev, hbitmap, bmBits);
}

/***********************************************************************
 *           DIBDRV_DeleteBitmap
 */
BOOL DIBDRV_DeleteBitmap( HBITMAP hbitmap )
{
    TRACE("hbitmap:%p\n", hbitmap);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pDeleteBitmap(hbitmap);
}

/***********************************************************************
 *           DIBDRV_GetBitmapBits
 */
LONG DIBDRV_GetBitmapBits( HBITMAP hbitmap, void *buffer, LONG count )
{
    TRACE("hbitmap:%p, buffer:%p, count:%d\n", hbitmap, buffer, count);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetBitmapBits(hbitmap, buffer, count);
}

/******************************************************************************
 *             DIBDRV_SetBitmapBits
 */
LONG DIBDRV_SetBitmapBits( HBITMAP hbitmap, const void *bits, LONG count )
{
    TRACE("hbitmap:%p, bits:%p, count:%d\n", hbitmap, bits, count);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pSetBitmapBits(hbitmap, bits, count);
}

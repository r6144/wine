/*
 * DIBDRV clipping functions
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
 *           DIBDRV_SetDeviceClipping
 */
void DIBDRV_SetDeviceClipping( DIBDRVPHYSDEV *physDev, HRGN vis_rgn, HRGN clip_rgn )
{
    TRACE("physDev:%p, vis_rgn:%p, clip_rgn:%p\n", physDev, vis_rgn, clip_rgn);

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        _DIBDRV_GetDisplayDriver()->pSetDeviceClipping(physDev->X11PhysDev, vis_rgn, clip_rgn);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        _DIBDRV_GetDisplayDriver()->pSetDeviceClipping(physDev->X11PhysDev, vis_rgn, clip_rgn);
    }
}

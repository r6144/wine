/*
 * DIBDRV video functions
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
 *              DIBDRV_GetDeviceGammaRamp
 */
BOOL DIBDRV_GetDeviceGammaRamp( DIBDRVPHYSDEV *physDev, LPVOID ramp )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, ramp:%p\n", physDev, ramp));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("TEMPORARY - fallback to X11 driver\n"));
        res = _DIBDRV_GetDisplayDriver()->pGetDeviceGammaRamp(physDev->X11PhysDev, ramp);
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetDeviceGammaRamp(physDev->X11PhysDev, ramp);
    }
    return res;
}

/***********************************************************************
 *              DIBDRV_SetDeviceGammaRamp
 */
BOOL DIBDRV_SetDeviceGammaRamp( DIBDRVPHYSDEV *physDev, LPVOID ramp )
{
    BOOL res;
    
    MAYBE(TRACE("physDev:%p, ramp:%p\n", physDev, ramp));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        ONCE(FIXME("STUB\n"));
        res = TRUE;
    }
    else
    {
        /* DDB selected in, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pSetDeviceGammaRamp(physDev->X11PhysDev, ramp);
    }
    return res;
}

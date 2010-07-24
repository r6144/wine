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

/**********************************************************************
 *           DIBDRV_CreateDC
 */
BOOL DIBDRV_CreateDC( HDC hdc, DIBDRVPHYSDEV **pdev, LPCWSTR driver, LPCWSTR device,
                      LPCWSTR output, const DEVMODEW* initData )
{
    DIBDRVPHYSDEV *physDev;
    PHYSDEV X11PhysDev;
    
    TRACE("hdc:%p, pdev:%p, driver:%s, device:%s, output:%s, initData:%p\n",
          hdc, pdev, debugstr_w(driver), debugstr_w(device), debugstr_w(output), initData);

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
    
    /* sets the result value and returns */
    *pdev = physDev;

    ONCE(FIXME("stub\n"));
    return TRUE;
}

/**********************************************************************
 *           DIBDRV_DeleteDC
 */
BOOL DIBDRV_DeleteDC( DIBDRVPHYSDEV *physDev )
{
    BOOL res;
    
    TRACE("physDev:%p\n", physDev);

    /* frees X11 device */
    res = _DIBDRV_GetDisplayDriver()->pDeleteDC(physDev->X11PhysDev);
    physDev->X11PhysDev = NULL;
    
    /* frees DIB Engine device */
    HeapFree(GetProcessHeap(), 0, physDev);
    
    ONCE(FIXME("stub\n"));
    return res;
}

/**********************************************************************
 *           DIBDRV_ExtEscape
 */
INT DIBDRV_ExtEscape( DIBDRVPHYSDEV *physDev, INT escape, INT in_count, LPCVOID in_data,
                      INT out_count, LPVOID out_data )
{
    TRACE("physDev:%p, escape:%d, in_count:%d, in_data:%p, out_count:%d, out_data:%p\n",
          physDev, escape, in_count, in_data, out_count, out_data);

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pExtEscape(physDev->X11PhysDev, escape, in_count, in_data, out_count, out_data);
}

/***********************************************************************
 *           DIBDRV_GetDeviceCaps
 */
INT DIBDRV_GetDeviceCaps( DIBDRVPHYSDEV *physDev, INT cap )
{
    TRACE("physDev:%p, cap:%d\n", physDev, cap); 

    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pGetDeviceCaps(physDev->X11PhysDev, cap);
}

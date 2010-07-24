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
    RGNDATA *data;
    DWORD size;
    int iRect;

    MAYBE(TRACE("physDev:%p, vis_rgn:%p, clip_rgn:%p\n", physDev, vis_rgn, clip_rgn));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */

        CombineRgn( physDev->region, vis_rgn, clip_rgn, clip_rgn ? RGN_AND : RGN_COPY );

        /* get region rectangles */
        if(!(size = GetRegionData(physDev->region, 0, NULL)))
            return;
        data = HeapAlloc(GetProcessHeap(), 0, size);
        if (!GetRegionData(physDev->region, size, data))
        {
            HeapFree( GetProcessHeap(), 0, data );
            return;
        }
        
        /* frees any previous regions rectangles in DC */
        if(physDev->regionData)
            HeapFree(GetProcessHeap(), 0, physDev->regionData);
            
        /* sets the rectangles on physDev */
        physDev->regionData = data;
        physDev->regionRects = (RECT *)data->Buffer;
        physDev->regionRectCount = data->rdh.nCount;
        
        if(TRACE_ON(dibdrv))
        {
            TRACE("Region dump : %d rectangles\n", physDev->regionRectCount);
            for(iRect = 0; iRect < physDev->regionRectCount; iRect++)
            {
                RECT *r = physDev->regionRects + iRect;
                TRACE("Rect #%03d, x1:%4d, y1:%4d, x2:%4d, y2:%4d\n", iRect, r->left, r->top, r->right, r->bottom);
            }
        }
    }
    else
    {
        /* DDB selected in, use X11 driver */
        _DIBDRV_GetDisplayDriver()->pSetDeviceClipping(physDev->X11PhysDev, vis_rgn, clip_rgn);
    }
}

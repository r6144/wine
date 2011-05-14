/*
 * DIBDRV bit-blit operations
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
 *           DIBDRV_AlphaBlend
 */
BOOL DIBDRV_AlphaBlend( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst, INT widthDst, INT heightDst,
                        DIBDRVPHYSDEV *physDevSrc, INT xSrc, INT ySrc, INT widthSrc, INT heightSrc,
                        BLENDFUNCTION blendfn)
{
    TRACE("physDevDst:%p, xDst:%d, yDst:%d, widthDst:%d, heightDst:%d, physDevSrc:%p, xSrc:%d, ySrc:%d, widthSrc:%d, heightSrc:%d\n",
          physDevDst, xDst, yDst, widthDst, heightDst, physDevSrc, xSrc, ySrc, widthSrc, heightSrc);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pAlphaBlend(physDevDst->X11PhysDev, xDst, yDst, widthDst, heightDst,
                                                   physDevSrc->X11PhysDev, xSrc, ySrc, widthSrc, heightSrc,
                                                   blendfn);
}

/***********************************************************************
 *           DIBDRV_BitBlt
 */
BOOL DIBDRV_BitBlt( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop )
{
    TRACE("physDevDst:%p, xDst:%d, yDst:%d, width:%d, height:%d, physDevSrc:%p, xSrc:%d, ySrc:%d, rop:%08x\n",
          physDevDst, xDst, yDst, width, height, physDevSrc, xSrc, ySrc, rop);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pBitBlt(physDevDst->X11PhysDev, xDst, yDst, width, height,
                                                      physDevSrc->X11PhysDev, xSrc, ySrc, rop);
}

/***********************************************************************
 *           DIBDRV_StretchBlt
 */
BOOL DIBDRV_StretchBlt( DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                        INT widthDst, INT heightDst,
                        DIBDRVPHYSDEV *physDevSrc, INT xSrc, INT ySrc,
                        INT widthSrc, INT heightSrc, DWORD rop )
{
    TRACE("physDevDst:%p, xDst:%d, yDst:%d, widthDst:%d, heightDst:%d, physDevSrc:%p, xSrc:%d, ySrc:%d, widthSrc:%d, heightSrc:%d, rop:%8x\n",
          physDevDst, xDst, yDst, widthDst, heightDst, physDevSrc, xSrc, ySrc, widthSrc, heightSrc, rop);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pStretchBlt(physDevDst->X11PhysDev, xDst, yDst, widthSrc, heightSrc,
                                                          physDevSrc->X11PhysDev, xSrc, ySrc, widthDst, heightDst, rop);
}

/***********************************************************************
 *           DIBDRV_PatBlt
 */
BOOL DIBDRV_PatBlt( DIBDRVPHYSDEV *physDev, INT left, INT top, INT width, INT height, DWORD rop )
{
    TRACE("physDev:%p, left:%d, top:%d, width:%d, height:%d, rop:%06x\n", physDev, left, top, width, height, rop);
    ONCE(FIXME("stub\n"));
    return _DIBDRV_GetDisplayDriver()->pPatBlt(physDev->X11PhysDev, left, top, width, height, rop);
}

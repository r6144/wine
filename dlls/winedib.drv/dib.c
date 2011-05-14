/*
 * DIBDRV device-independent bitmaps
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

/* Default 1 BPP palette */
static DWORD pal1[] =
{
    0x000000,
    0xffffff
};

/* Default 4 BPP palette */
static DWORD pal4[] =
{
    0x000000,0x800000,0x008000,0x808000,
    0x000080,0x800080,0x008080,0x808080,
    0xc0c0c0,0xff0000,0x00ff00,0xffff00,
    0x0000ff,0xff00ff,0x00ffff,0xffffff
};

/* Default 8 BPP palette */
static DWORD pal8[] =
{
    0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xc0c0c0,0xc0dcc0,0xa6caf0,0x000000,0x000033,0x000066,0x000099,0x0000cc,0x0000ff,
    0x003300,0x003333,0x003366,0x003399,0x0033cc,0x0033ff,0x006600,0x006633,0x006666,0x006699,0x0066cc,0x0066ff,0x009900,0x009933,0x009966,0x009999,
    0x0099cc,0x0099ff,0x00cc00,0x00cc33,0x00cc66,0x00cc99,0x00cccc,0x00ccff,0x00ff00,0x00ff33,0x00ff66,0x00ff99,0x00ffcc,0x00ffff,0x330000,0x330033,
    0x330066,0x330099,0x3300cc,0x3300ff,0x333300,0x333333,0x333366,0x333399,0x3333cc,0x3333ff,0x336600,0x336633,0x336666,0x336699,0x3366cc,0x3366ff,
    0x339900,0x339933,0x339966,0x339999,0x3399cc,0x3399ff,0x33cc00,0x33cc33,0x33cc66,0x33cc99,0x33cccc,0x33ccff,0x33ff00,0x33ff33,0x33ff66,0x33ff99,
    0x33ffcc,0x33ffff,0x660000,0x660033,0x660066,0x660099,0x6600cc,0x6600ff,0x663300,0x663333,0x663366,0x663399,0x6633cc,0x6633ff,0x666600,0x666633,
    0x666666,0x666699,0x6666cc,0x6666ff,0x669900,0x669933,0x669966,0x669999,0x6699cc,0x6699ff,0x66cc00,0x66cc33,0x66cc66,0x66cc99,0x66cccc,0x66ccff,
    0x66ff00,0x66ff33,0x66ff66,0x66ff99,0x66ffcc,0x66ffff,0x990000,0x990033,0x990066,0x990099,0x9900cc,0x9900ff,0x993300,0x993333,0x993366,0x993399,
    0x9933cc,0x9933ff,0x996600,0x996633,0x996666,0x996699,0x9966cc,0x9966ff,0x999900,0x999933,0x999966,0x999999,0x9999cc,0x9999ff,0x99cc00,0x99cc33,
    0x99cc66,0x99cc99,0x99cccc,0x99ccff,0x99ff00,0x99ff33,0x99ff66,0x99ff99,0x99ffcc,0x99ffff,0xcc0000,0xcc0033,0xcc0066,0xcc0099,0xcc00cc,0xcc00ff,
    0xcc3300,0xcc3333,0xcc3366,0xcc3399,0xcc33cc,0xcc33ff,0xcc6600,0xcc6633,0xcc6666,0xcc6699,0xcc66cc,0xcc66ff,0xcc9900,0xcc9933,0xcc9966,0xcc9999,
    0xcc99cc,0xcc99ff,0xcccc00,0xcccc33,0xcccc66,0xcccc99,0xcccccc,0xccccff,0xccff00,0xccff33,0xccff66,0xccff99,0xccffcc,0xccffff,0xff0000,0xff0033,
    0xff0066,0xff0099,0xff00cc,0xff00ff,0xff3300,0xff3333,0xff3366,0xff3399,0xff33cc,0xff33ff,0xff6600,0xff6633,0xff6666,0xff6699,0xff66cc,0xff66ff,
    0xff9900,0xff9933,0xff9966,0xff9999,0xff99cc,0xff99ff,0xffcc00,0xffcc33,0xffcc66,0xffcc99,0xffcccc,0xffccff,0xffff00,0xffff33,0xffff66,0xffff99,
    0xffffcc,0xffffff,0x050500,0x050501,0x050502,0x050503,0x050504,0x050505,0xe8e8e8,0xe9e9e9,0xeaeaea,0xebebeb,0xececec,0xededed,0xeeeeee,0xefefef,
    0xf0f0f0,0xf1f1f1,0xf2f2f2,0xf3f3f3,0xf4f4f4,0xf5f5f5,0xfffbf0,0xa0a0a4,0x808080,0xf00000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff
};

/***********************************************************************
 *           DIBDRV_CreateDIBSection
 */
HBITMAP DIBDRV_CreateDIBSection( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap,
                                 const BITMAPINFO *bmi, UINT usage )
{
    HBITMAP res;
    
    MAYBE(TRACE("physDev:%p, hbitmap:%p, bmi:%p, usage:%d\n", physDev, hbitmap, bmi, usage));

    /* createDIBSection is only DIB-related, so we just use the engine */
    ONCE(FIXME("STUB\n"));
    res = hbitmap;

    return res;
}

/***********************************************************************
 *           DIBDRV_GetDIBits
*/
INT DIBDRV_GetDIBits( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, UINT startscan,
                      UINT lines, LPVOID bits, BITMAPINFO *info, UINT coloruse )
{
    INT res;
    DIBSECTION ds;
    DIBDRVBITMAP sBmp, dBmp;
    DWORD *buf;
    int iLine;
    int size;
    BOOL justInfo, justInfoHeader;
    int requestedBpp;
    RGBQUAD *colorTable;
    int colorTableSize;
    BITMAPINFO *sourceInfo;

    MAYBE(TRACE("physDev:%p, hbitmap:%p, startscan:%d, lines:%d, bits:%p, info:%p, coloruse:%d\n",
        physDev, hbitmap, startscan, lines, bits, info, coloruse));
    if(GetObjectW(hbitmap, sizeof(DIBSECTION), &ds) == sizeof(DIBSECTION))
    {
        /* GetDIBits reads bits from a DIB, so we should use the engine driver */
        
        /* for the moment, we don't support startscan != 0 */
        if(startscan != 0)
        {
            FIXME("startscan != 0 still not supported\n");
            return 0;
        }
        
        /* sanity check */
        size = info->bmiHeader.biSize;
        if(size != sizeof(BITMAPINFOHEADER) && size != sizeof(BITMAPCOREHEADER))
        {
            ERR("Unknown header size %d\n", size);
            return 0;
        }
        
        /* get requested BPP */
        requestedBpp = info->bmiHeader.biBitCount;
        
        /* check wetrher we wants just the BITMAPINFO data */
        justInfo = (bits == NULL);
        
        /* check wether we wants just to get BITMAPINFOHEADER data */
        justInfoHeader = (requestedBpp == 0);
        
        if(!justInfo && justInfoHeader)
        {
            ERR("Bad requested BPP\n");
            return 0;
        }

        /* copy / set missing DIB info */
        if(justInfo)
        {
            info->bmiHeader.biWidth = ds.dsBmih.biWidth;
            info->bmiHeader.biHeight = ds.dsBmih.biHeight;
        }
        info->bmiHeader.biPlanes = 1;
        if(justInfoHeader)
            info->bmiHeader.biBitCount = ds.dsBmih.biBitCount;
        if(size == sizeof(BITMAPINFOHEADER))
        {
            if(justInfoHeader)
            {
                info->bmiHeader.biCompression = ds.dsBmih.biCompression;
                info->bmiHeader.biXPelsPerMeter = ds.dsBmih.biXPelsPerMeter;
                info->bmiHeader.biYPelsPerMeter = ds.dsBmih.biYPelsPerMeter;
                info->bmiHeader.biClrUsed = ds.dsBmih.biClrUsed;
                info->bmiHeader.biClrImportant = ds.dsBmih.biClrImportant;
            }
            info->bmiHeader.biSizeImage = ds.dsBmih.biSizeImage;
        }

        /* width and height *must* match source's ones */
        if(info->bmiHeader.biWidth != ds.dsBmih.biWidth ||
           abs(info->bmiHeader.biHeight) != abs(ds.dsBmih.biHeight))
        {
            ERR("Size of requested bitmap data don't match source's ones\n");
            return 0;
        }

        /* if we just wants the BITMAPINFOHEADER data, we're done */
        if(justInfoHeader || (justInfo && ds.dsBmih.biBitCount > 8))
            return abs(info->bmiHeader.biHeight);
            
        /* we now have to get source data -- we need it for palette, for example */
        colorTableSize = 0;
        if(ds.dsBmih.biBitCount <= 8)
            colorTableSize = (1 << ds.dsBmih.biBitCount);
        else if(ds.dsBmih.biCompression == BI_BITFIELDS)
            colorTableSize = 3;
        sourceInfo = HeapAlloc(GetProcessHeap(), 0, sizeof(BITMAPINFOHEADER) + colorTableSize * sizeof(RGBQUAD));
        if(!sourceInfo)
        {
            ERR("HeapAlloc failed\n");
            return 0;
        }
        memcpy(sourceInfo, &ds.dsBmih, sizeof(BITMAPINFOHEADER));
        if(ds.dsBmih.biBitCount <= 8)
        {
            /* grab palette - well, we should. No way to do it by now....
               we should add a list HBITMAP <--> DIBDRVBITMAP and fiddle to many
               , many parts of the engine. Not worth the effort by now.
               So, we just synthesize the table */
            switch(requestedBpp)
            {
                case 1:
                    memcpy((BYTE *)sourceInfo + sizeof(BITMAPINFOHEADER), pal1, 2*sizeof(DWORD));
                    break;
                    
                case 4:
                    memcpy((BYTE *)sourceInfo + sizeof(BITMAPINFOHEADER), pal4, 16*sizeof(DWORD));
                    break;
                
                case 8:
                    memcpy((BYTE *)sourceInfo + sizeof(BITMAPINFOHEADER), pal8, 256*sizeof(DWORD));
                    break;
                    
                default:
                    ERR("Unknown requested bith depth %d\n", requestedBpp);
                    _DIBDRVBITMAP_Free(&sBmp);
                    return 0;
            }
        }
        else if(ds.dsBmih.biCompression == BI_BITFIELDS)
            memcpy((BYTE *)sourceInfo + sizeof(BITMAPINFOHEADER), ds.dsBitfields, 3 * sizeof(RGBQUAD));
        _DIBDRVBITMAP_Clear(&sBmp);
        if(!_DIBDRVBITMAP_InitFromBitmapinfo(&sBmp, sourceInfo))
        {
            ERR("_DIBDRVBITMAP_InitFromBitmapinfo failed\n");
            HeapFree(GetProcessHeap(), 0, sourceInfo);
            return 0;
        }
        _DIBDRVBITMAP_Set_Bits(&sBmp, ds.dsBm.bmBits, FALSE);
        HeapFree(GetProcessHeap(), 0, sourceInfo);
                
        /* now grab / synthesize the color table if needed */
        if(requestedBpp <= 8)
        {
            colorTable = (RGBQUAD *)((BYTE *)info + size);
            if(requestedBpp == ds.dsBmih.biBitCount)
            {
                /* same source and dest format - copy color tables */
                memcpy(colorTable, sBmp.colorTable, colorTableSize);
            }
            else
            {
                /* different formats -- synthesize color table */
                switch(requestedBpp)
                {
                    case 1:
                        memcpy(colorTable, pal1, 2*sizeof(DWORD));
                        break;
                        
                    case 4:
                        memcpy(colorTable, pal4, 16*sizeof(DWORD));
                        break;
                    
                    case 8:
                        memcpy(colorTable, pal8, 256*sizeof(DWORD));
                        break;
                        
                    default:
                        ERR("Unknown requested bith depth %d\n", requestedBpp);
                        _DIBDRVBITMAP_Free(&sBmp);
                        return 0;
                }
            }
        }
        
        /* if we just wanted DIB info, job is done */
        if(justInfo)
        {
            _DIBDRVBITMAP_Free(&sBmp);
            return abs(info->bmiHeader.biHeight);
        }
        
        /* Creates a DIBDRVBITMAP from dest dib */
        _DIBDRVBITMAP_Clear(&dBmp);
        if(!_DIBDRVBITMAP_InitFromBitmapinfo(&dBmp, info))
        {
            ERR("_DIBDRVBITMAP_InitFromBitmapinfo failed\n");
            _DIBDRVBITMAP_Free(&sBmp);
            return 0;
        }
        _DIBDRVBITMAP_Set_Bits(&dBmp, bits, FALSE);
        
        /* now we can do the bit conversion */
        buf = HeapAlloc(GetProcessHeap(), 0, ds.dsBmih.biWidth * sizeof(RGBQUAD));
        for(iLine = 0; iLine < lines; iLine++)
        {
            sBmp.funcs->GetLine(&sBmp, iLine, 0, ds.dsBmih.biWidth, buf);
            dBmp.funcs->PutLine(&dBmp, iLine, 0, ds.dsBmih.biWidth, buf);
        }
        _DIBDRVBITMAP_Free(&sBmp);
        _DIBDRVBITMAP_Free(&dBmp);
        return lines;
    }
    else
        /* GetDIBits reads bits from a DDB, use X11 driver */
        res = _DIBDRV_GetDisplayDriver()->pGetDIBits(physDev->X11PhysDev, hbitmap, startscan, lines, bits, info, coloruse);
    
    return res;
}

/***********************************************************************
 *           DIBDRV_SetDIBColorTable
 */
UINT DIBDRV_SetDIBColorTable( DIBDRVPHYSDEV *physDev, UINT start, UINT count,
                              const RGBQUAD *colors )
{
    DIBDRVBITMAP *dib = &physDev->physBitmap;
#if 0
    HBITMAP thisDIB;
#endif

    MAYBE(TRACE("physDev:%p, start:%d, count:%d, colors:%p\n", physDev, start, count, colors));

    /* SetDIBColorTable operates on a DIB, so we use the engine */
    
    /* if bpp > 8, some error occurred... */
    if(dib->bitCount > 8)
    {
        ERR("Called for BPP > 8\n");
        return 0;
    }
    
    /* if dib hasn't a color table, or has a small one, we must before
       create/extend it */
    if(!dib->colorTable)
    {
        dib->colorTableSize = (1 << dib->bitCount);
        dib->colorTable = HeapAlloc(GetProcessHeap(), 0, sizeof(RGBQUAD) * dib->colorTableSize);
    }
    else if(dib->colorTableSize < (1 << dib->bitCount))
    {
        int newSize = (1 << dib->bitCount);
        RGBQUAD *newTable = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RGBQUAD) * newSize);
        memcpy(newTable, dib->colorTable, sizeof(RGBQUAD) * dib->colorTableSize);
        HeapFree(GetProcessHeap(), 0, dib->colorTable);
        dib->colorTable = newTable;
        dib->colorTableSize = newSize;
    }
    
    /* sanity check */
    if(start + count > dib->colorTableSize)
    {
        ERR("Out of range setting color table, size is %d, requested is %d\n", dib->colorTableSize, start+count);
        return 0;
    }
    memcpy(dib->colorTable + start, colors, sizeof(RGBQUAD) * count);
    dib->colorTableGrabbed = TRUE;

    /* hack to make GDI32 sense the DIB color table change
       (fixes a couple of todos in bitmap test suite */
#if 0
    thisDIB = SelectObject(physDev->hdc, GetStockObject(OBJ_BITMAP));
    SelectObject(physDev->hdc, thisDIB);
#endif

    return TRUE;
}

/***********************************************************************
 *           DIBDRV_SetDIBits
 */
INT DIBDRV_SetDIBits( DIBDRVPHYSDEV *physDev, HBITMAP hbitmap, UINT startscan,
                      UINT lines, LPCVOID bits, const BITMAPINFO *info, UINT coloruse )
{
    INT res;
    DIBSECTION ds;
    DIBDRVBITMAP sBmp, dBmp;
    DWORD *buf;
    int iLine;
    
    MAYBE(TRACE("physDev:%p, hbitmap:%p, startscan:%d, lines:%d, bits:%p, bmi:%p, coloruse:%d\n",
        physDev, hbitmap, startscan, lines, bits, info, coloruse));


    if(GetObjectW(hbitmap, sizeof(DIBSECTION), &ds) == sizeof(DIBSECTION))
    {
        /* SetDIBits writes bits to a DIB, so we should use the engine driver */

        /* for the moment, we don't support startscan != 0 */
        if(startscan != 0)
        {
            FIXME("startscan != 0 still not supported\n");
            return 0;
        }
        
        /* Creates a DIBDRVBITMAP from source dib */
        _DIBDRVBITMAP_Clear(&sBmp);
        if(!_DIBDRVBITMAP_InitFromBitmapinfo(&sBmp, info))
        {
            ERR("_DIBDRVBITMAP_InitFromBitmapinfo failed\n");
            return 0;
        }
        _DIBDRVBITMAP_Set_Bits(&sBmp, (LPVOID)bits, FALSE);
        
        /* same for destination dib */
        if(!_DIBDRVBITMAP_InitFromHBITMAP(&dBmp, hbitmap, FALSE))
        {
            ERR("_DIBDRVBITMAP_InitFromHBITMAP failed\n");
            _DIBDRVBITMAP_Free(&sBmp);
            return 0;
        }
        
        /* now we can do the bit conversion */
        buf = HeapAlloc(GetProcessHeap(), 0, ds.dsBmih.biWidth * sizeof(RGBQUAD));
        for(iLine = 0; iLine < lines; iLine++)
        {
            sBmp.funcs->GetLine(&sBmp, iLine, 0, ds.dsBmih.biWidth, buf);
            dBmp.funcs->PutLine(&dBmp, iLine, 0, ds.dsBmih.biWidth, buf);
        }
        _DIBDRVBITMAP_Free(&sBmp);
        _DIBDRVBITMAP_Free(&dBmp);
        return lines;
    }
    else
    {
        /* SetDIBits writes bits to a DDB, so we should use the X11 driver */
        res =  _DIBDRV_GetDisplayDriver()->pSetDIBits(physDev->X11PhysDev, hbitmap, startscan, lines, bits, info, coloruse);
    }
    return res;
}

/*************************************************************************
 *              DIBDRV_SetDIBitsToDevice
 */
INT DIBDRV_SetDIBitsToDevice( DIBDRVPHYSDEV *physDev, INT xDest, INT yDest, DWORD cx,
                              DWORD cy, INT xSrc, INT ySrc,
                              UINT startscan, UINT lines, LPCVOID bits,
                              const BITMAPINFO *info, UINT coloruse )
{
    BITMAPINFO *bitmapInfo;
    int bmInfoSize;
    int dibHeight, dibWidth;
    DIBDRVBITMAP sBmp;
    int sLine, dLine, iLine;
    void *buf;
    
    MAYBE(TRACE("physDev:%p, xDest:%d, yDest:%d, cx:%x, cy:%x, xSrc:%d, ySrc:%d, startscan:%d, lines:%d, bits:%p, info:%p, coloruse:%d\n",
        physDev, xDest, yDest, cx, cy, xSrc, ySrc, startscan, lines, bits, info, coloruse));

    if(physDev->hasDIB)
    {
        /* DIB section selected in, use DIB Engine */
        
        /* inverts y on source -- FIXME: check if right with some tests, it seems so */
        ySrc = abs(info->bmiHeader.biHeight) - ySrc - cy;
        
        dibHeight = info->bmiHeader.biHeight;
        dibWidth = info->bmiHeader.biWidth;

        /* sanity check and source clipping on physical sizes */
        if(startscan >= abs(dibHeight))
            return 0;
        if(startscan + lines > abs(dibHeight))
            lines = abs(dibHeight) - startscan;

        /* adjust height because of startscan */
        dibHeight += (dibHeight > 0 ? -startscan : startscan);

        if(xSrc >= dibWidth)
            return 0;
        if(xSrc + cx > dibWidth)
            cx = dibWidth - xSrc;
        if(ySrc > abs(dibHeight))
            return 0;
        if(ySrc + cy > abs(dibHeight))
            cy = abs(dibHeight) - ySrc;
            
        ySrc -= startscan;
        cy -= startscan;
        if(cy <= 0)
            return 0;
        if(ySrc < 0)
        {
            yDest += ySrc;
            cy += ySrc;
            ySrc = 0;
        }
        if(cy <= 0)
            return 0;

        /* grab a copy of BITMAPINFO */
        bmInfoSize = sizeof(BITMAPINFOHEADER);
        if(info->bmiHeader.biCompression == BI_BITFIELDS)
            bmInfoSize += 3 * sizeof(RGBQUAD);
        else if (info->bmiHeader.biBitCount <= 8)
        {
            if(info->bmiHeader.biClrUsed)
                bmInfoSize += info->bmiHeader.biClrUsed * sizeof(RGBQUAD);
            else
                bmInfoSize += (1 << info->bmiHeader.biBitCount) * sizeof(RGBQUAD);
        }
        if(!(bitmapInfo = HeapAlloc(GetProcessHeap(), 0, bmInfoSize)))
        {
            ERR("HeapAlloc failed\n");
            return 0;
        }
        memcpy(bitmapInfo, info, bmInfoSize);
        bitmapInfo->bmiHeader.biHeight = dibHeight;

        /* create a DIBDRVBITMAP from BITMAPINFO data */
        _DIBDRVBITMAP_Clear(&sBmp);
        if(!_DIBDRVBITMAP_InitFromBitmapinfo(&sBmp, bitmapInfo))
        {
            ERR("_DIBDRVBITMAP_InitFromBitmapinfo failed\n");
            HeapFree(GetProcessHeap, 0, bitmapInfo);
            return 0;
        }
        HeapFree(GetProcessHeap(), 0, bitmapInfo);
        _DIBDRVBITMAP_Set_Bits(&sBmp, (LPVOID)bits, FALSE);
        
        /* transfer lines to dest bitmap */
        if(!(buf = HeapAlloc(GetProcessHeap(), 0, cx * sizeof(RGBQUAD))))
        {
            ERR("HeapAlloc failed\n");
            return 0;
        }
        for(sLine = ySrc, dLine = yDest, iLine = 0; iLine < cy; sLine++, dLine++, iLine++)
        {
            sBmp.funcs->GetLine(&sBmp, sLine, xSrc, cx, buf); 
            physDev->physBitmap.funcs->PutLine(&physDev->physBitmap, dLine, xDest, cx, buf);
        }
        HeapFree(GetProcessHeap(), 0, buf);

        return cy;
    }
    else
    {
        return _DIBDRV_GetDisplayDriver()->pSetDIBitsToDevice(physDev->X11PhysDev, xDest, yDest, cx, cy, xSrc, ySrc,
                                                            startscan, lines, bits, info, coloruse);
    }
}

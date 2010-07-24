/*
 * DIB Engine conversion routines
 * Converts DDB <--> DIB
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

#include "winuser.h"

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/***********************************************************************
 * Creates DDB that is compatible with source hdc.
 * hdc is the HDC on where the DIB MUST be selected in
 * srcBmp is the source DIB
 * startScan and scanLines specify the portion of DIB to convert
 * in order to avoid unneeded conversion of large DIBs on blitting
 * 
 * NOTE : the srcBmp DIB MUST NOT be selected in any DC */
HBITMAP _DIBDRV_ConvertDIBtoDDB( HDC hdc, HBITMAP srcBmp, int startScan, int scanLines )
{
    DIBSECTION ds;
    BITMAPINFO *bmi;
    HBITMAP hBmp = NULL;
    
    int dibWidth, dibHeight;
    BOOL topDown;
    void *bits;
    int stride;
    UINT colorUsed;
    int bitFields;
    HDC tmpHdc;
    HBITMAP tmpBmp;
    int res;

    /* gets DIBSECTION data from source DIB */
    if(GetObjectW(srcBmp, sizeof(DIBSECTION), &ds) != sizeof(DIBSECTION))
    {
        ERR("Couldn't get DIBSECTION data\n");
        return 0;
    }

    /* gets DIB info */
    dibWidth  = ds.dsBmih.biWidth;
    dibHeight = ds.dsBmih.biHeight;
    bits = ds.dsBm.bmBits;
    stride = ((dibWidth * ds.dsBmih.biBitCount +31) &~31) / 8;
    
    /* adjust bits to point at needed starting stripe */
    if(dibHeight < 0)
    {
        /* top-down DIB */
        topDown = TRUE;
        dibHeight = -dibHeight;
        bits = (BYTE *)bits + startScan * stride;
    }
    else
    {
        topDown = FALSE;
        bits = (BYTE *)bits + (dibHeight - startScan - scanLines) * stride;
    }
    
    /* if requested part is out of source bitmap, returns 0 */
    if(startScan >= dibHeight)
        return 0;
    if(startScan + scanLines >= dibHeight)
        scanLines = dibHeight - startScan;
        
    /* gets the size of DIB palette and bitfields, if any */
    bitFields = 0;
    colorUsed = 0;
    if(ds.dsBmih.biCompression == BI_BITFIELDS)
            bitFields = 3;
    else if(ds.dsBmih.biBitCount <= 8)
    {
        colorUsed = ds.dsBmih.biClrUsed;
        if(!colorUsed)
            colorUsed = 1 << ds.dsBmih.biBitCount;
    }

    /* builds the needed BITMAPINFOHEADER */
    bmi = HeapAlloc( GetProcessHeap(), 0, sizeof(BITMAPINFOHEADER)+
                     sizeof(RGBQUAD)*colorUsed + sizeof(DWORD) * bitFields );
    if (!bmi)
    {
        ERR("HeapAlloc failed\n");
        return 0;
    }
        
    /* copy the header part */
    memcpy( &bmi->bmiHeader, &ds.dsBmih, sizeof(BITMAPINFOHEADER) );
    
    /* gets the color table part, if any */
    if(colorUsed)
    {
        /* create a temporary DC, GetDIBColorTable() requests that
           the DIB is selected in a DC.... */
        if(!(tmpHdc = CreateCompatibleDC(hdc)))
        {
            ERR("Couldn't create the temporary HDC\n");
            HeapFree(GetProcessHeap(), 0, bmi);
            return 0;
        }
        /* selects the DIB into the temporary DC */
        if( !(tmpBmp = SelectObject(tmpHdc, srcBmp)))
        {
            ERR("Couldn't select source DIB into temporary DC\n");
            DeleteDC(tmpHdc);
            HeapFree(GetProcessHeap(), 0, bmi);
            return 0;
        }
        if(!GetDIBColorTable(tmpHdc, 0, colorUsed, bmi->bmiColors))
            ERR("GetDIBColorTable failed\n");
        SelectObject(tmpHdc, tmpBmp);
        DeleteDC(tmpHdc);
        
    }
    
    /* fill the bitfields part, if any */
    if(bitFields)
        memcpy(bmi->bmiColors, ds.dsBitfields, 3 * sizeof(DWORD));

    /* adjust dib size for SetDIBits, as it needs it to reverse top-down dibs
       it must be set to the number of scanLines to transfer */
    bmi->bmiHeader.biHeight = topDown ? -scanLines : scanLines;
    
    /* creates destination compatible bitmap */
    tmpHdc = GetDC(NULL);
    hBmp = CreateCompatibleBitmap(tmpHdc, dibWidth, scanLines);
    ReleaseDC(NULL, tmpHdc);
    if(!hBmp)
    {
        ERR("CreateCompatibleBitmap failed\n");
        HeapFree( GetProcessHeap(), 0, bmi );
        return 0;
    }
    
    /* copies the requested scan lines from DIB to DDB */
    /* FIXME : still no support for RLE packed DIBs */
    res = SetDIBits( hdc, hBmp, 0, scanLines, bits, bmi, DIB_RGB_COLORS );
    HeapFree( GetProcessHeap(), 0, bmi );
    if(!res)
    {
        ERR("SetDIBits failed\n");
        DeleteObject( hBmp );
        return 0;
    }
    
    return hBmp;
}

/***********************************************************************
 * Creates DIB that is compatible with the target hdc.
 * startScan and scanLines specify the portion of DDB to convert
 * in order to avoid unneeded conversion of large DDBs on blitting
 * 
 * NOTE : the srcBmp DDB MUST NOT be selected in any DC */
HBITMAP _DIBDRV_ConvertDDBtoDIB( HDC hdc, HBITMAP srcBmp, int startScan, int scanLines )
{
    HBITMAP hBmp = NULL;
    BITMAP bitmap;
    BITMAPINFO *bi;
    void *bits = NULL;

    if (!GetObjectW( srcBmp, sizeof(bitmap), &bitmap ))
    {
        ERR("Couldn't retrieve source bitmap\n");
        return 0;
    }

    if(startScan + scanLines >= bitmap.bmHeight)
        scanLines = bitmap.bmHeight - startScan;

    bi = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BITMAPINFOHEADER) /* + 256 * sizeof(RGBQUAD) */);
    if (!bi)
    {
        ERR("HeapAlloc failed\n");
        return 0;
    }
    
    bi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth       = bitmap.bmWidth;
    bi->bmiHeader.biHeight      = scanLines;
    bi->bmiHeader.biPlanes      = bitmap.bmPlanes;
    bi->bmiHeader.biBitCount    = 32;
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biSizeImage   = 0;
    
    /* No need to get the color table or the color masks 
       as we're requesting a 32 bit rgba DIB */

    /* Create bitmap and fill in bits */
    hBmp = CreateDIBSection(hdc, bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if(!hBmp)
    {
        ERR("Failed to create DIB section\n");
        HeapFree( GetProcessHeap(), 0, bi );
        return 0;
    }
    if (bits)
    {
        if(!GetDIBits(hdc, srcBmp, startScan, scanLines, bits, bi, DIB_RGB_COLORS))
        {
            ERR("GetDIBits failed\n");
            DeleteObject( hBmp );
            HeapFree( GetProcessHeap(), 0, bi );
            return 0;
        }
    }
    else
    {
        ERR("CreateDibSection couldn't allocate DIB bits\n");
        DeleteObject( hBmp );
        HeapFree( GetProcessHeap(), 0, bi );
        return 0;
    }
    HeapFree( GetProcessHeap(), 0, bi );
    return hBmp;
}

/***********************************************************************
 *           BITBLT_ConvertDevDDBtoDIB
 *
 * Creates DIB that is compatible with the target hdc for a device (non memory) source DC */
HBITMAP _DIBDRV_ConvertDevDDBtoDIB( HDC hdcSrc, HDC hdcDst, int xSrc, int ySrc, int width, int height )
{
    HBITMAP bmp, dib;
    HDC memHDC = NULL;
    
    /* at first, we create a compatible DC and a bitmap with needed sizes */
    memHDC = CreateCompatibleDC(hdcSrc);
    if(!memHDC)
    {
        ERR("CreateCompatibleDC failed\n");
        return 0;
    }
    bmp = CreateCompatibleBitmap(hdcSrc, width, height);
    if(!bmp)
    {
        ERR("CreateCompatibleBitmap failed\n");
        DeleteDC(memHDC);
        return 0;
    }
    
    /* select the newly created DDB into the temporary DC */
    bmp = SelectObject(memHDC, bmp);
    if(!bmp)
    {
        ERR("Failed selecting DDB into temporary DC\n");
        DeleteObject(bmp);
        DeleteDC(memHDC);
        return 0;
    }
    
    /* next, we blit pixels from device to the compatible bitmap */
    if(!BitBlt(memHDC, 0, 0, width, height, hdcSrc, xSrc, ySrc, SRCCOPY))
    {
        ERR("BitBlt failed\n");
        DeleteObject(bmp);
        DeleteDC(memHDC);
        return 0;
    }
    
    /* select out the DDB from the temporary DC */
    bmp = SelectObject(memHDC, bmp);
    DeleteDC(memHDC);
    if(!bmp)
    {
        ERR("Failed selecting DDB out temporary DC\n");
        DeleteObject(bmp);
        return 0;
    }

    /*now we can convert the bitmap to a DIB */
    dib = _DIBDRV_ConvertDDBtoDIB( hdcDst, bmp, 0, height );
    if(!dib)
        FIXME("ConvertDDBtoDIB failed\n");
    
    /* free resources and return the created dib */
    DeleteObject(bmp);
    return dib;
}

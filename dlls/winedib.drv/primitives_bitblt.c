/*
 * DIB Engine BitBlt Primitives
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

/* shrinks a line -- srcWidth >= dstWidth */
static void ShrinkLine(DWORD *dst, int dstWidth, DWORD *src, int srcWidth)
{
    int srcPos, dstPos;
    int delta;
    
    srcPos = 0;
    dstPos = 0;
    delta = 0;
    while(dstPos < dstWidth)
    {
        *dst++ = *src;
        while(delta < srcWidth)
        {
            srcPos++;
            src++;
            delta += dstWidth;
        }
        delta -= srcWidth;
        dstPos++;
    }
}

/* expands a line -- srcWidth <= dstWidth */
static void ExpandLine(DWORD *dst, int dstWidth, DWORD *src, int srcWidth)
{
    int srcPos;
    int delta;

    srcPos = 0;
    delta = 0;
    while(srcPos < srcWidth)
    {
        while(delta < dstWidth)
        {
            *dst++ = *src;
            delta += srcWidth;
        }
        delta -= dstWidth;
        src++;
        srcPos++;
    }
}

/* stretch a line */
static void StretchLine(DWORD *dst, int dstWidth, DWORD *src, int srcWidth)
{
    if(srcWidth > dstWidth)
        ShrinkLine(dst, dstWidth, src, srcWidth);
    else if(srcWidth < dstWidth)
        ExpandLine(dst, dstWidth, src, srcWidth);
    else
        memcpy(dst, src, 4 * srcWidth);
}

/* premultiply alpha channel on a line by a constant alpha
   note : it seems that pixels are already premultiplied
   by alpha channel content */
static void PemultiplyLine(DWORD *dst, int width, BYTE constAlpha)
{
    int i = width;
    BYTE *alphaPnt = (BYTE *)dst + 3;
    
    /* small optimization for 0 and 255 values of constAlpha */

    /* fully transparent -- just sets all pix to 0 */
    if(constAlpha == 0)
    {
        while(i--)
            *dst++ = 0;
        return;
    }
    
    /* fully opaque, just do nothing */
    if(constAlpha == 255)
        return;

    /* intermediate -- premultiply alpha values */
    while(i--)
    {
        *alphaPnt = MulDiv(*alphaPnt, constAlpha, 255);
        alphaPnt += 4;
    }
    return;
        
}

/* alpha blends a source line onto a destination line
   preconditions :
   1) source and dest widths must be the same
   2) source line should be already premultiplied by constant alpha */
static void BlendLine(DWORD *dst, DWORD *src, int width)
{
    int i = width;
    BYTE *blueDst  = (BYTE *)dst;
    BYTE *greenDst = blueDst  + 1;
    BYTE *redDst   = greenDst + 1;
    BYTE *blueSrc  = (BYTE *)src;
    BYTE *greenSrc = blueSrc  + 1;
    BYTE *redSrc   = greenSrc + 1;
    BYTE *alphaSrc = redSrc   + 1;
    BYTE alpha;
    
    /* still don't know if it must take in account an eventual dest
       alpha channel..... */
    while(i--)
    {
        alpha = 255 - *alphaSrc;
        
        *blueDst  = *blueSrc  + MulDiv(*blueDst,  alpha, 255);
        *greenDst = *greenSrc + MulDiv(*greenDst, alpha, 255);
        *redDst   = *redSrc   + MulDiv(*redDst,   alpha, 255);

        blueSrc  += 4;
        greenSrc += 4;
        redSrc   += 4;
        alphaSrc += 4;
        blueDst  += 4;
        greenDst += 4;
        redDst   += 4;
    }

}

/* ------------------------------------------------------------*/
/*                        ALPHABLEND PRIMITIVES                */
BOOL _DIBDRV_AlphaBlend_generic(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT widthDst, INT heightDst, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, int widthSrc, int heightSrc, BLENDFUNCTION blendFn)
{
    /* flags indicating wether source should be stretched */
    BOOL horStretch = (widthSrc != widthDst);
    BOOL verStretch = (heightSrc != heightDst);
    
    /* constant alpha value */
    BYTE constAlpha = blendFn.SourceConstantAlpha;
    
    /* source and dest bitmaps */
    const DIBDRVBITMAP *srcBmp = &physDevSrc->physBitmap;
    DIBDRVBITMAP *dstBmp = &physDevDst->physBitmap;
    
    /* source and destination line buffers */
    DWORD *sBuf = HeapAlloc(GetProcessHeap(), 0, abs(srcBmp->stride));
    DWORD *dBuf = HeapAlloc(GetProcessHeap(), 0, abs(dstBmp->stride));
    
    int ys = ySrc;
    int yd = yDst;
    int iLine;
    int delta;

    /* in order to optimize a bit, we divide the routine in 4 parts,
       depending on stretching modes */
    if(!horStretch && !verStretch)
    {
        /* simplest case, no stretching needed */
        MAYBE(TRACE("No stretching\n"));
        for(iLine = 0; iLine < heightSrc; iLine++, ys++, yd++)
        {
            /* load source and dest lines */
            srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBuf);
            dstBmp->funcs->GetLine(dstBmp, yd, xDst, widthDst, dBuf);
            
            /* premultiply source by constant and pixel alpha */
            PemultiplyLine(sBuf, widthSrc, constAlpha);
            
            /* blends source on dest */
            BlendLine(dBuf, sBuf, widthSrc);
            
            /* puts dest line back */
            dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
        }
    }
    else if (horStretch && !verStretch)
    {
        /* just horizontal stretching needed */
        DWORD *strBuf = HeapAlloc(GetProcessHeap(), 0, abs(dstBmp->stride));
        MAYBE(TRACE("Horizontal stretching\n"));

        for(iLine = 0; iLine < heightSrc; iLine++, ys++, yd++)
        {
            /* load source and dest lines */
            srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBuf);
            dstBmp->funcs->GetLine(dstBmp, yd, xDst, widthDst, dBuf);
            
            /* stretch source line to match dest one */
            StretchLine(strBuf, widthDst, sBuf, widthSrc);
        
            /* premultiply source by constant and pixel alpha */
            PemultiplyLine(strBuf, widthDst, constAlpha);
            
            /* blends source on dest */
            BlendLine(dBuf, sBuf, widthDst);
            
            /* puts dest line back */
            dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
        }
        HeapFree(GetProcessHeap(), 0, strBuf);
    }
    else if (!horStretch && verStretch)
    {
        /* just vertical stretching needed */
        MAYBE(TRACE("Vertical stretching\n"));

        if(heightSrc > heightDst)
        {
            iLine = 0;
            delta = 0;
            while(iLine < heightDst)
            {
                /* load source and dest lines */
                srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBuf);
                dstBmp->funcs->GetLine(dstBmp, yd, xDst, widthDst, dBuf);

                /* premultiply source by constant and pixel alpha */
                PemultiplyLine(sBuf, widthSrc, constAlpha);
                
                /* blends source on dest */
                BlendLine(dBuf, sBuf, widthDst);
                
                /* puts dest line back */
                dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);

                while(delta < heightSrc)
                {
                    ys++;
                    delta += heightDst;
                }
                delta -= heightSrc;
                yd++;
                iLine++;
            }
        }
        else if(heightSrc < heightDst)
        {
            iLine = 0;
            delta = 0;
            while(iLine < heightSrc)
            {
                /* load source line */
                srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBuf);
                
                /* premultiply source by constant and pixel alpha */
                PemultiplyLine(sBuf, widthSrc, constAlpha);
                
                while(delta < heightDst)
                {
                    /* load dest line */
                    dstBmp->funcs->GetLine(dstBmp, yd, xDst, widthDst, dBuf);
                    
                    /* blends source on dest */
                    BlendLine(dBuf, sBuf, widthDst);
                    
                    /* puts dest line back */
                    dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    yd++;
                    delta += heightSrc;
                }
                delta -= heightDst;
                ys++;
                iLine++;
            }
        }
    }
    else
    {
        DWORD *strBuf = HeapAlloc(GetProcessHeap(), 0, abs(dstBmp->stride));
        /* both stretching needed -- generic case */
        MAYBE(TRACE("Horizontal and vertical stretching\n"));

        if(heightSrc > heightDst)
        {
            iLine = 0;
            delta = 0;
            while(iLine < heightDst)
            {
                /* load source and dest lines */
                srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBuf);
                dstBmp->funcs->GetLine(dstBmp, yd, xDst, widthDst, dBuf);

                /* stretch source line to match dest one */
                StretchLine(strBuf, widthDst, sBuf, widthSrc);

                /* premultiply source by constant and pixel alpha */
                PemultiplyLine(strBuf, widthDst, constAlpha);
                
                /* blends source on dest */
                BlendLine(dBuf, strBuf, widthDst);
                
                /* puts dest line back */
                dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);

                while(delta < heightSrc)
                {
                    ys++;
                    delta += heightDst;
                }
                delta -= heightSrc;
                yd++;
                iLine++;
            }
        }
        else if(heightSrc < heightDst)
        {
            iLine = 0;
            delta = 0;
            while(iLine < heightSrc)
            {
                /* load source line */
                srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBuf);
                
                /* stretch source line to match dest one */
                StretchLine(strBuf, widthDst, sBuf, widthSrc);

                /* premultiply source by constant and pixel alpha */
                PemultiplyLine(strBuf, widthDst, constAlpha);
                
                while(delta < heightDst)
                {
                    /* load dest line */
                    dstBmp->funcs->GetLine(dstBmp, yd, xDst, widthDst, dBuf);
                    
                    /* blends source on dest */
                    BlendLine(dBuf, strBuf, widthDst);
                    
                    /* puts dest line back */
                    dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    yd++;
                    delta += heightSrc;
                }
                delta -= heightDst;
                ys++;
                iLine++;
            }
        }
        HeapFree(GetProcessHeap(), 0, strBuf);
    }
    
    HeapFree(GetProcessHeap(), 0, sBuf);
    HeapFree(GetProcessHeap(), 0, dBuf);
    return TRUE;
}

/* ------------------------------------------------------------*/
/*                        BLITTING PRIMITIVES                  */
BOOL _DIBDRV_BitBlt_generic(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT width, INT height, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, DWORD rop)
{
    int ys, yd;
    int i;
    DWORD *dwBuf;
    DIBDRVBITMAP *dstBmp, *patBmp;
    const DIBDRVBITMAP *srcBmp;
    DWORD *wDstPnt, *wSrcPnt, *wPatPnt;
    BOOL usePat, useSrc, useDst;
    DWORD patColor;
    BOOL res = FALSE;

    /* 32 bit RGB source and destination buffer, if needed */
    DWORD *sBuf = 0, *dBuf = 0, *pBuf = 0;

    /* get elements usage */
    usePat = (((rop >> 4) & 0x0f0000) != (rop & 0x0f0000));
    useSrc = (((rop >> 2) & 0x330000) != (rop & 0x330000));
    useDst = (((rop >> 1) & 0x550000) != (rop & 0x550000));
    
    /* gets source, dest and pattern bitmaps, if available */
    if(usePat && physDevDst->isBrushBitmap)
        patBmp = &physDevDst->brushBmpCache;
    else
        patBmp = NULL;

    if(useSrc)
        srcBmp = &physDevSrc->physBitmap;
    else
        srcBmp = NULL;
    dstBmp = &physDevDst->physBitmap;
    
    /* gets pattern color, in case it's needed */
    if(usePat)
        patColor = physDevDst->brushColor;
    else
        patColor = 0;

    /* allocate 32 bit RGB destination buffer */
    if(!(dBuf = (DWORD *)HeapAlloc( GetProcessHeap(), 0, width * 4)))
        goto error;
    
    MAYBE(TRACE("dstBmp:%p(%s), xDst:%d, yDst:%d, width:%d, height:%d, srcBmp:%p(%s), xSrc:%d, ySrc:%d, rop:%8x\n",
          dstBmp, _DIBDRVBITMAP_GetFormatName(dstBmp), xDst, yDst, width, height, 
          srcBmp, _DIBDRVBITMAP_GetFormatName(srcBmp), xSrc, ySrc, rop));
              
    /* some simple ROPs optimizations */
    switch(rop)
    {
          case BLACKNESS:
              MAYBE(TRACE("BLACKNESS\n"));
              memset(dBuf, 0x00, width * 4);
              for(yd = yDst; yd < yDst+height; yd++)
                  dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
              break;
          
          case WHITENESS:
              MAYBE(TRACE("WHITENESS\n"));
              for(dwBuf = dBuf, i = width; i; i--)
                  *dwBuf++ = 0x00ffffff;
              for(yd = yDst; yd < yDst+height; yd++)
                  dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
              break;

          case SRCCOPY:
              MAYBE(TRACE("SRCCOPY\n"));
              for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
              {
                  srcBmp->funcs->GetLine(srcBmp, ys, xSrc, width, dBuf);
                  dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
              }
              break;

          /* fallback for generic ROP operation */
          default:
              rop >>= 16;
              if(useSrc && useDst && usePat && physDevDst->isBrushBitmap)
              {
                  MAYBE(TRACE("BitBlt use: src+dst+pat - pattern brush\n"));
                  if(!(sBuf = HeapAlloc( GetProcessHeap(), 0, width * 4)))
                      goto error;
                  if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, width * 4)))
                      goto error;
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      srcBmp->funcs->GetLine(srcBmp, ys, xSrc, width, sBuf);
                      dstBmp->funcs->GetLine(dstBmp, ys, xDst, width, dBuf);
                      patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, width, pBuf);
                      wDstPnt = dBuf;
                      wSrcPnt = sBuf;
                      wPatPnt = pBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, *wDstPnt, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(useSrc && useDst)
              {
                  if(usePat)
                      MAYBE(TRACE("BitBlt use: src+dst+pat - solid brush\n"));
                  else
                      MAYBE(TRACE("BitBlt use: src+dst\n"));
                  if(!(sBuf = HeapAlloc( GetProcessHeap(), 0, width * 4)))
                      goto error;
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      srcBmp->funcs->GetLine(srcBmp, ys, xSrc, width, sBuf);
                      dstBmp->funcs->GetLine(dstBmp, yd, xDst, width, dBuf);
                      wDstPnt = dBuf;
                      wSrcPnt = sBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, *wDstPnt, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(useSrc && usePat && physDevDst->isBrushBitmap)
              {
                  MAYBE(TRACE("BitBlt use: src+pat -- pattern brush\n"));
                  if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, width * 4)))
                      goto error;
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      srcBmp->funcs->GetLine(srcBmp, ys, xSrc, width, dBuf);
                      patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, width, pBuf);
                      wDstPnt = sBuf;
                      wSrcPnt = sBuf;
                      wPatPnt = pBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, 0, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(useSrc)
              {
                  if(usePat)
                      MAYBE(TRACE("BitBlt use: src+pat - solid brush\n"));
                  else
                      MAYBE(TRACE("BitBlt use: src\n"));
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      srcBmp->funcs->GetLine(srcBmp, ys, xSrc, width, dBuf);
                      wDstPnt = sBuf;
                      wSrcPnt = sBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, 0, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(useDst && usePat && physDevDst->isBrushBitmap)
              {
                  MAYBE(TRACE("BitBlt use: dst+pat -- pattern brush\n"));
                  if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, width * 4)))
                      goto error;
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      dstBmp->funcs->GetLine(srcBmp, ys, xDst, width, dBuf);
                      patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, width, pBuf);
                      wDstPnt = sBuf;
                      wPatPnt = pBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, *wDstPnt, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(useDst)
              {
                  if(usePat)
                      MAYBE(TRACE("BitBlt use: dst+pat - solid brush\n"));
                  else
                      MAYBE(TRACE("BitBlt use: dst\n"));
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      dstBmp->funcs->GetLine(srcBmp, ys, xDst, width, dBuf);
                      wDstPnt = sBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(patColor, 0, *wDstPnt, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(usePat && physDevDst->isBrushBitmap)
              {
                  MAYBE(TRACE("BitBlt use: pat -- pattern brush\n"));
                  if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, width * 4)))
                      goto error;
                  for(ys = ySrc, yd = yDst; ys < ySrc+height; ys++, yd++)
                  {
                      patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, width, pBuf);
                      wDstPnt = dBuf;
                      wPatPnt = pBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, 0, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else if(usePat)
              {
                  MAYBE(TRACE("BitBlt use: pat -- solid brush -- rop is %02x, color is %08x\n", rop, patColor));
                  MAYBE(TRACE("Dest BMP is a '%s'\n", _DIBDRVBITMAP_GetFormatName(dstBmp)));
                  MAYBE(TRACE("xDst = %d, yDst = %d, width = %d, height = %d\n", xDst, yDst, width, height));
                  for(yd = yDst; yd < yDst+height; yd++)
                  {
                      wDstPnt = dBuf;
                      for(i = width; i > 0 ; i--)
                      {
                          *wDstPnt = _DIBDRV_ROP3(patColor, 0, 0, rop);
                          wDstPnt++;
                      }
                      dstBmp->funcs->PutLine(dstBmp, yd, xDst, width, dBuf);
                  }
              }
              else
                  ERR("What happened ?????? \n");
              break;
    } /* switch */
    res = TRUE;
error:
    if(sBuf) HeapFree( GetProcessHeap(), 0, sBuf );
    if(dBuf) HeapFree( GetProcessHeap(), 0, dBuf );
    if(pBuf) HeapFree( GetProcessHeap(), 0, pBuf );
    return res;
}

/* ------------------------------------------------------------*/
/*                        STRETCHING PRIMITIVES                */
BOOL _DIBDRV_StretchBlt_generic(DIBDRVPHYSDEV *physDevDst, INT xDst, INT yDst,
                    INT widthDst, INT heightDst, const DIBDRVPHYSDEV *physDevSrc,
                    INT xSrc, INT ySrc, int widthSrc, int heightSrc, DWORD rop)
{
    int ys, yd;
    int i, delta;
    DWORD *dwBuf;
    DIBDRVBITMAP *dstBmp, *patBmp;
    const DIBDRVBITMAP *srcBmp;
    DWORD *wDstPnt, *wSrcPnt, *wPatPnt;
    BOOL usePat, useSrc, useDst;
    DWORD patColor;
    BOOL res = FALSE;

    /* 32 bit RGB source and destination buffer, if needed */
    DWORD *sBufOrig = 0, *sBufStr = 0, *dBuf = 0, *pBuf = 0;

    /* get elements usage */
    usePat = (((rop >> 4) & 0x0f0000) != (rop & 0x0f0000));
    useSrc = (((rop >> 2) & 0x330000) != (rop & 0x330000));
    useDst = (((rop >> 1) & 0x550000) != (rop & 0x550000));
    
    /* gets source, dest and pattern bitmaps, if available */
    if(usePat && physDevDst->isBrushBitmap)
        patBmp = &physDevDst->brushBmpCache;
    else
        patBmp = NULL;

    if(useSrc)
        srcBmp = &physDevSrc->physBitmap;
    else
        srcBmp = NULL;
    dstBmp = &physDevDst->physBitmap;
    
    /* gets pattern color, in case it's needed */
    if(usePat)
        patColor = physDevDst->brushColor;
    else
        patColor = 0;

    /* allocate 32 bit RGB destination buffer */
    if(!(dBuf = (DWORD *)HeapAlloc( GetProcessHeap(), 0, widthDst * 4)))
        goto error;
    
    MAYBE(TRACE("dstBmp:%p(%s), xDst:%d, yDst:%d, widthDst:%d, heightDst:%d, srcBmp:%p(%s), xSrc:%d, ySrc:%d, , widthSrc:%d, heightSrc:%drop:%8x\n",
          dstBmp, _DIBDRVBITMAP_GetFormatName(dstBmp), xDst, yDst, widthDst, heightDst, 
          srcBmp, _DIBDRVBITMAP_GetFormatName(srcBmp), xSrc, ySrc, widthSrc, heightSrc, rop));
              
    /* some simple ROPs optimizations */
    switch(rop)
    {
        case BLACKNESS:
            MAYBE(TRACE("BLACKNESS\n"));
            memset(dBuf, 0x00, widthDst * 4);
            for(yd = yDst; yd < yDst+heightDst; yd++)
                dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
            break;
        
        case WHITENESS:
            MAYBE(TRACE("WHITENESS\n"));
            for(dwBuf = dBuf, i = widthDst; i; i--)
                *dwBuf++ = 0x00ffffff;
            for(yd = yDst; yd < yDst+heightDst; yd++)
                dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
            break;

        case SRCCOPY:
            MAYBE(TRACE("SRCCOPY\n"));
            sBufOrig = HeapAlloc(GetProcessHeap(), 0, widthSrc * 4);
            if(heightSrc > heightDst)
            {
                ys = 0;
                yd = 0;
                delta = 0;
                while(yd < heightDst)
                {
                    srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                    StretchLine(dBuf, widthDst, sBufOrig, widthSrc);
                    dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                    while(delta < heightSrc)
                    {
                        ys++;
                        delta += heightDst;
                    }
                    delta -= heightSrc;
                    yd++;
                }
            }
            else if(heightSrc < heightDst)
            {
                ys = 0;
                yd = 0;
                delta = 0;
                while(ys < heightSrc)
                {
                    srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                    StretchLine(dBuf, widthDst, sBufOrig, widthSrc);
                    while(delta < heightDst)
                    {
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        yd++;
                        delta += heightSrc;
                    }
                    delta -= heightDst;
                    ys++;
                }
            }
            else
            {
                for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                {
                    srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBufOrig);
                    StretchLine(dBuf, widthDst, sBufOrig, widthSrc);
                    dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                }
            }
            break;

        /* fallback for generic ROP operation */
        default:
            rop >>= 16;
            if(useSrc && useDst && usePat && physDevDst->isBrushBitmap)
            {
                MAYBE(TRACE("StretchBlt use: src+dst+pat - pattern brush\n"));
                if(!(sBufOrig = HeapAlloc(GetProcessHeap(), 0, widthSrc * 4)))
                    goto error;
                if(!(sBufStr = HeapAlloc(GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            wSrcPnt = sBufStr;
                            wPatPnt = pBuf;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, *wDstPnt, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        dstBmp->funcs->GetLine(dstBmp, ys, xDst, widthDst, dBuf);
                        patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(useSrc && useDst)
            {
                if(usePat)
                    MAYBE(TRACE("StretchBlt use: src+dst+pat - solid brush\n"));
                else
                    MAYBE(TRACE("StretchBlt use: src+dst\n"));
                if(!(sBufOrig = HeapAlloc(GetProcessHeap(), 0, widthSrc * 4)))
                    goto error;
                if(!(sBufStr = HeapAlloc(GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            wSrcPnt = sBufStr;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, *wDstPnt, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        dstBmp->funcs->GetLine(dstBmp, ys, xDst, widthDst, dBuf);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(useSrc && usePat && physDevDst->isBrushBitmap)
            {
                MAYBE(TRACE("StretchBlt use: src+pat -- pattern brush\n"));
                if(!(sBufOrig = HeapAlloc(GetProcessHeap(), 0, widthSrc * 4)))
                    goto error;
                if(!(sBufStr = HeapAlloc(GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            wSrcPnt = sBufStr;
                            wPatPnt = pBuf;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, 0, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, *wSrcPnt++, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(useSrc)
            {
                if(usePat)
                    MAYBE(TRACE("StretchBlt use: src+pat - solid brush\n"));
                else
                    MAYBE(TRACE("StretchBlt use: src\n"));
                if(!(sBufOrig = HeapAlloc(GetProcessHeap(), 0, widthSrc * 4)))
                    goto error;
                if(!(sBufStr = HeapAlloc(GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys + ySrc, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            wSrcPnt = sBufStr;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, 0, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        srcBmp->funcs->GetLine(srcBmp, ys, xSrc, widthSrc, sBufOrig);
                        StretchLine(sBufStr, widthDst, sBufOrig, widthSrc);
                        wDstPnt = dBuf;
                        wSrcPnt = sBufStr;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, *wSrcPnt++, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(useDst && usePat && physDevDst->isBrushBitmap)
            {
                MAYBE(TRACE("StretchBlt use: dst+pat -- pattern brush\n"));
                if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            wPatPnt = pBuf;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, *wDstPnt, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        dstBmp->funcs->GetLine(dstBmp, ys, xDst, widthDst, dBuf);
                        patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(useDst)
            {
                if(usePat)
                    MAYBE(TRACE("StretchBlt use: dst+pat - solid brush\n"));
                else
                    MAYBE(TRACE("StretchBlt use: dst\n"));
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        wDstPnt = dBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, 0, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        dstBmp->funcs->GetLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(patColor, 0, *wDstPnt, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        dstBmp->funcs->GetLine(dstBmp, ys, xDst, widthDst, dBuf);
                        wDstPnt = dBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, 0, *wDstPnt, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(usePat && physDevDst->isBrushBitmap)
            {
                MAYBE(TRACE("StretchBlt use: pat -- pattern brush\n"));
                if(!(pBuf = HeapAlloc( GetProcessHeap(), 0, widthDst * 4)))
                    goto error;
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        patBmp->funcs->GetLine(patBmp, (yd + yDst)%patBmp->height, 0, widthDst, pBuf);
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            wPatPnt = pBuf;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, 0, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        patBmp->funcs->GetLine(patBmp, ys%patBmp->height, 0, widthDst, pBuf);
                        wDstPnt = dBuf;
                        wPatPnt = pBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(*wPatPnt++, 0, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else if(usePat)
            {
                MAYBE(TRACE("StretchBlt use: pat -- solid brush -- rop is %02x, color is %08x\n", rop, patColor));
                MAYBE(TRACE("Dest BMP is a '%s'\n", _DIBDRVBITMAP_GetFormatName(dstBmp)));
                MAYBE(TRACE("xDst = %d, yDst = %d, widthDst = %d, heightDst = %d\n", xDst, yDst, widthDst, heightDst));
                if(heightSrc > heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(yd < heightDst)
                    {
                        wDstPnt = dBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, 0, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                        while(delta < heightSrc)
                        {
                            ys++;
                            delta += heightDst;
                        }
                        delta -= heightSrc;
                        yd++;
                    }
                }
                else if(heightSrc < heightDst)
                {
                    ys = 0;
                    yd = 0;
                    delta = 0;
                    while(ys < heightSrc)
                    {
                        while(delta < heightDst)
                        {
                            wDstPnt = dBuf;
                            for(i = widthDst; i > 0 ; i--)
                            {
                                *wDstPnt = _DIBDRV_ROP3(patColor, 0, 0, rop);
                                wDstPnt++;
                            }
                            dstBmp->funcs->PutLine(dstBmp, yd + yDst, xDst, widthDst, dBuf);
                            yd++;
                            delta += heightSrc;
                        }
                        delta -= heightDst;
                        ys++;
                    }
                }
                else
                {
                    for(ys = ySrc, yd = yDst; ys < ySrc+heightSrc; ys++, yd++)
                    {
                        wDstPnt = dBuf;
                        for(i = widthDst; i > 0 ; i--)
                        {
                            *wDstPnt = _DIBDRV_ROP3(patColor, 0, 0, rop);
                            wDstPnt++;
                        }
                        dstBmp->funcs->PutLine(dstBmp, yd, xDst, widthDst, dBuf);
                    }
                }
            }
            else
                ERR("What happened ?????? \n");
            break;
    } /* switch */
    res = TRUE;
error:
    if(sBufOrig) HeapFree( GetProcessHeap(), 0, sBufOrig );
    if(sBufStr)  HeapFree( GetProcessHeap(), 0, sBufStr );
    if(dBuf)     HeapFree( GetProcessHeap(), 0, dBuf );
    if(pBuf)     HeapFree( GetProcessHeap(), 0, pBuf );
    return res;
}

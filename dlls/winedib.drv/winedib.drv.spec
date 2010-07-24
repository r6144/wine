@ cdecl AlphaBlend(ptr long long long long ptr long long long long long) DIBDRV_AlphaBlend
@ cdecl Arc(ptr long long long long long long long long) DIBDRV_Arc
@ cdecl BitBlt(ptr long long long long ptr long long long) DIBDRV_BitBlt
@ cdecl ChoosePixelFormat(ptr ptr) DIBDRV_ChoosePixelFormat
@ cdecl Chord(ptr long long long long long long long long) DIBDRV_Chord
@ cdecl CreateBitmap(ptr long ptr) DIBDRV_CreateBitmap
@ cdecl CreateDC(long ptr wstr wstr wstr ptr) DIBDRV_CreateDC
@ cdecl CreateDIBSection(ptr long ptr long) DIBDRV_CreateDIBSection
@ cdecl DeleteBitmap(long) DIBDRV_DeleteBitmap
@ cdecl DeleteDC(ptr) DIBDRV_DeleteDC
@ cdecl DescribePixelFormat(ptr long long ptr) DIBDRV_DescribePixelFormat
@ cdecl Ellipse(ptr long long long long) DIBDRV_Ellipse
@ cdecl EnumDeviceFonts(ptr ptr ptr long) DIBDRV_EnumDeviceFonts
@ cdecl ExtEscape(ptr long long ptr long ptr) DIBDRV_ExtEscape
@ cdecl ExtFloodFill(ptr long long long long) DIBDRV_ExtFloodFill
@ cdecl ExtTextOut(ptr long long long ptr ptr long ptr) DIBDRV_ExtTextOut
@ cdecl GetBitmapBits(long ptr long) DIBDRV_GetBitmapBits
@ cdecl GetCharWidth(ptr long long ptr) DIBDRV_GetCharWidth
@ cdecl GetDCOrgEx(ptr ptr) DIBDRV_GetDCOrgEx
@ cdecl GetDIBits(ptr long long long ptr ptr long) DIBDRV_GetDIBits
@ cdecl GetDeviceCaps(ptr long) DIBDRV_GetDeviceCaps
@ cdecl GetDeviceGammaRamp(ptr ptr) DIBDRV_GetDeviceGammaRamp
@ cdecl GetICMProfile(ptr ptr ptr) DIBDRV_GetICMProfile
@ cdecl GetNearestColor(ptr long) DIBDRV_GetNearestColor
@ cdecl GetPixel(ptr long long) DIBDRV_GetPixel
@ cdecl GetPixelFormat(ptr) DIBDRV_GetPixelFormat
@ cdecl GetSystemPaletteEntries(ptr long long ptr) DIBDRV_GetSystemPaletteEntries
@ cdecl GetTextExtentExPoint(ptr ptr long long ptr ptr ptr) DIBDRV_GetTextExtentExPoint
@ cdecl GetTextMetrics(ptr ptr) DIBDRV_GetTextMetrics
@ cdecl LineTo(ptr long long) DIBDRV_LineTo
@ cdecl PaintRgn(ptr long) DIBDRV_PaintRgn
@ cdecl PatBlt(ptr long long long long long) DIBDRV_PatBlt
@ cdecl Pie(ptr long long long long long long long long) DIBDRV_Pie
@ cdecl PolyPolygon(ptr ptr ptr long) DIBDRV_PolyPolygon
@ cdecl PolyPolyline(ptr ptr ptr long) DIBDRV_PolyPolyline
@ cdecl Polygon(ptr ptr long) DIBDRV_Polygon
@ cdecl Polyline(ptr ptr long) DIBDRV_Polyline
@ cdecl RealizeDefaultPalette(ptr) DIBDRV_RealizeDefaultPalette
@ cdecl RealizePalette(ptr long long) DIBDRV_RealizePalette
@ cdecl Rectangle(ptr long long long long) DIBDRV_Rectangle
@ cdecl RoundRect(ptr long long long long long long) DIBDRV_RoundRect
@ cdecl SelectBitmap(ptr long) DIBDRV_SelectBitmap
@ cdecl SelectBrush(ptr long) DIBDRV_SelectBrush
@ cdecl SelectFont(ptr long long) DIBDRV_SelectFont
@ cdecl SelectPen(ptr long) DIBDRV_SelectPen
@ cdecl SetBitmapBits(long ptr long) DIBDRV_SetBitmapBits
@ cdecl SetBkColor(ptr long) DIBDRV_SetBkColor
@ cdecl SetDCBrushColor(ptr long) DIBDRV_SetDCBrushColor
@ cdecl SetDCOrg(ptr long long) DIBDRV_SetDCOrg
@ cdecl SetDCPenColor(ptr long) DIBDRV_SetDCPenColor
@ cdecl SetDIBColorTable(ptr long long ptr) DIBDRV_SetDIBColorTable
@ cdecl SetDIBits(ptr long long long ptr ptr long) DIBDRV_SetDIBits
@ cdecl SetDIBitsToDevice(ptr long long long long long long long long ptr ptr long) DIBDRV_SetDIBitsToDevice
@ cdecl SetDeviceClipping(ptr long long) DIBDRV_SetDeviceClipping
@ cdecl SetDeviceGammaRamp(ptr ptr) DIBDRV_SetDeviceGammaRamp
@ cdecl SetPixel(ptr long long long) DIBDRV_SetPixel
@ cdecl SetPixelFormat(ptr long ptr) DIBDRV_SetPixelFormat
@ cdecl SetTextColor(ptr long) DIBDRV_SetTextColor
@ cdecl StretchBlt(ptr long long long long ptr long long long long long) DIBDRV_StretchBlt
@ cdecl SwapBuffers(ptr) DIBDRV_SwapBuffers
@ cdecl UnrealizePalette(long) DIBDRV_UnrealizePalette
@ cdecl SetROP2(ptr long) DIBDRV_SetROP2
# OpenGL
@ cdecl wglCopyContext(long long long) DIBDRV_wglCopyContext
@ cdecl wglCreateContext(ptr) DIBDRV_wglCreateContext
@ cdecl wglDeleteContext(long) DIBDRV_wglDeleteContext
@ cdecl wglGetProcAddress(str) DIBDRV_wglGetProcAddress
@ cdecl wglGetPbufferDCARB(ptr ptr) DIBDRV_wglGetPbufferDCARB
@ cdecl wglMakeContextCurrentARB(ptr ptr long) DIBDRV_wglMakeContextCurrentARB
@ cdecl wglMakeCurrent(ptr long) DIBDRV_wglMakeCurrent
@ cdecl wglSetPixelFormatWINE(ptr long ptr) DIBDRV_wglSetPixelFormatWINE
@ cdecl wglShareLists(long long) DIBDRV_wglShareLists
@ cdecl wglUseFontBitmapsA(ptr long long long) DIBDRV_wglUseFontBitmapsA
@ cdecl wglUseFontBitmapsW(ptr long long long) DIBDRV_wglUseFontBitmapsW

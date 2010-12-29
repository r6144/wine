/*
 * Copyright (C) 2006 Vitaliy Margolen
 * Copyright (C) 2006 Chris Robinson
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

#define COBJMACROS
#include <initguid.h>
#include <d3d8.h>
#include "wine/test.h"

static IDirect3D8 *(WINAPI *pDirect3DCreate8)(UINT);

static BOOL (WINAPI *pGetCursorInfo)(PCURSORINFO);

static const DWORD simple_vs[] = {0xFFFE0101,       /* vs_1_1               */
    0x00000009, 0xC0010000, 0x90E40000, 0xA0E40000, /* dp4 oPos.x, v0, c0   */
    0x00000009, 0xC0020000, 0x90E40000, 0xA0E40001, /* dp4 oPos.y, v0, c1   */
    0x00000009, 0xC0040000, 0x90E40000, 0xA0E40002, /* dp4 oPos.z, v0, c2   */
    0x00000009, 0xC0080000, 0x90E40000, 0xA0E40003, /* dp4 oPos.w, v0, c3   */
    0x0000FFFF};                                    /* END                  */
static const DWORD simple_ps[] = {0xFFFF0101,                               /* ps_1_1                       */
    0x00000051, 0xA00F0001, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, /* def c1 = 1.0, 0.0, 0.0, 0.0  */
    0x00000042, 0xB00F0000,                                                 /* tex t0                       */
    0x00000008, 0x800F0000, 0xA0E40001, 0xA0E40000,                         /* dp3 r0, c1, c0               */
    0x00000005, 0x800F0000, 0x90E40000, 0x80E40000,                         /* mul r0, v0, r0               */
    0x00000005, 0x800F0000, 0xB0E40000, 0x80E40000,                         /* mul r0, t0, r0               */
    0x0000FFFF};                                                            /* END                          */

static int get_refcount(IUnknown *object)
{
    IUnknown_AddRef( object );
    return IUnknown_Release( object );
}

/* try to make sure pending X events have been processed before continuing */
static void flush_events(void)
{
    MSG msg;
    int diff = 200;
    int min_timeout = 100;
    DWORD time = GetTickCount() + diff;

    while (diff > 0)
    {
        if (MsgWaitForMultipleObjects( 0, NULL, FALSE, min_timeout, QS_ALLINPUT ) == WAIT_TIMEOUT) break;
        while (PeekMessage( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessage( &msg );
        diff = time - GetTickCount();
    }
}

static IDirect3DDevice8 *create_device(IDirect3D8 *d3d8, HWND device_window, HWND focus_window, BOOL windowed)
{
    D3DPRESENT_PARAMETERS present_parameters = {0};
    IDirect3DDevice8 *device;

    present_parameters.Windowed = windowed;
    present_parameters.hDeviceWindow = device_window;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferWidth = 640;
    present_parameters.BackBufferHeight = 480;
    present_parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    if (SUCCEEDED(IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, focus_window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device))) return device;

    present_parameters.AutoDepthStencilFormat = D3DFMT_D16;
    if (SUCCEEDED(IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, focus_window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device))) return device;

    if (SUCCEEDED(IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, focus_window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device))) return device;

    return NULL;
}

static HRESULT reset_device(IDirect3DDevice8 *device, HWND device_window, BOOL windowed)
{
    D3DPRESENT_PARAMETERS present_parameters = {0};

    present_parameters.Windowed = windowed;
    present_parameters.hDeviceWindow = device_window;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferWidth = 640;
    present_parameters.BackBufferHeight = 480;
    present_parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    return IDirect3DDevice8_Reset(device, &present_parameters);
}

#define CHECK_CALL(r,c,d,rc) \
    if (SUCCEEDED(r)) {\
        int tmp1 = get_refcount( (IUnknown *)d ); \
        int rc_new = rc; \
        ok(tmp1 == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, tmp1); \
    } else {\
        trace("%s failed: %#08x\n", c, r); \
    }

#define CHECK_RELEASE(obj,d,rc) \
    if (obj) { \
        int tmp1, rc_new = rc; \
        IUnknown_Release( obj ); \
        tmp1 = get_refcount( (IUnknown *)d ); \
        ok(tmp1 == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, tmp1); \
    }

#define CHECK_REFCOUNT(obj,rc) \
    { \
        int rc_new = rc; \
        int count = get_refcount( (IUnknown *)obj ); \
        ok(count == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, count); \
    }

#define CHECK_RELEASE_REFCOUNT(obj,rc) \
    { \
        int rc_new = rc; \
        int count = IUnknown_Release( (IUnknown *)obj ); \
        ok(count == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, count); \
    }

#define CHECK_ADDREF_REFCOUNT(obj,rc) \
    { \
        int rc_new = rc; \
        int count = IUnknown_AddRef( (IUnknown *)obj ); \
        ok(count == rc_new, "Invalid refcount. Expected %d got %d\n", rc_new, count); \
    }

#define CHECK_SURFACE_CONTAINER(obj,iid,expected) \
    { \
        void *container_ptr = (void *)0x1337c0d3; \
        hr = IDirect3DSurface8_GetContainer(obj, &iid, &container_ptr); \
        ok(SUCCEEDED(hr) && container_ptr == expected, "GetContainer returned: hr %#08x, container_ptr %p. " \
            "Expected hr %#08x, container_ptr %p\n", hr, container_ptr, S_OK, expected); \
        if (container_ptr && container_ptr != (void *)0x1337c0d3) IUnknown_Release((IUnknown *)container_ptr); \
    }

static void check_mipmap_levels(IDirect3DDevice8 *device, UINT width, UINT height, UINT count)
{
    IDirect3DBaseTexture8* texture = NULL;
    HRESULT hr = IDirect3DDevice8_CreateTexture( device, width, height, 0, 0,
            D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, (IDirect3DTexture8**) &texture );

    if (SUCCEEDED(hr)) {
        DWORD levels = IDirect3DBaseTexture8_GetLevelCount(texture);
        ok(levels == count, "Invalid level count. Expected %d got %u\n", count, levels);
    } else
        trace("CreateTexture failed: %#08x\n", hr);

    if (texture) IUnknown_Release( texture );
}

static void test_mipmap_levels(void)
{

    HRESULT               hr;
    HWND                  hwnd = NULL;

    IDirect3D8            *pD3d = NULL;
    IDirect3DDevice8      *pDevice = NULL;
    D3DPRESENT_PARAMETERS d3dpp;
    D3DDISPLAYMODE        d3ddm;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }

    check_mipmap_levels(pDevice, 32, 32, 6);
    check_mipmap_levels(pDevice, 256, 1, 9);
    check_mipmap_levels(pDevice, 1, 256, 9);
    check_mipmap_levels(pDevice, 1, 1, 1);

cleanup:
    if (pDevice)
    {
        UINT refcount = IUnknown_Release( pDevice );
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IUnknown_Release( pD3d );
    DestroyWindow( hwnd );
}

static void test_swapchain(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    IDirect3DSwapChain8         *swapchain1         = NULL;
    IDirect3DSwapChain8         *swapchain2         = NULL;
    IDirect3DSwapChain8         *swapchain3         = NULL;
    IDirect3DSurface8           *backbuffer         = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferCount  = 0;

    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }

    /* Check if the back buffer count was modified */
    ok(d3dpp.BackBufferCount == 1, "The back buffer count in the presentparams struct is %d\n", d3dpp.BackBufferCount);

    /* Create a bunch of swapchains */
    d3dpp.BackBufferCount = 0;
    hr = IDirect3DDevice8_CreateAdditionalSwapChain(pDevice, &d3dpp, &swapchain1);
    ok(SUCCEEDED(hr), "Failed to create a swapchain (%#08x)\n", hr);
    ok(d3dpp.BackBufferCount == 1, "The back buffer count in the presentparams struct is %d\n", d3dpp.BackBufferCount);

    d3dpp.BackBufferCount  = 1;
    hr = IDirect3DDevice8_CreateAdditionalSwapChain(pDevice, &d3dpp, &swapchain2);
    ok(SUCCEEDED(hr), "Failed to create a swapchain (%#08x)\n", hr);

    d3dpp.BackBufferCount  = 2;
    hr = IDirect3DDevice8_CreateAdditionalSwapChain(pDevice, &d3dpp, &swapchain3);
    ok(SUCCEEDED(hr), "Failed to create a swapchain (%#08x)\n", hr);
    if(SUCCEEDED(hr)) {
        /* Swapchain 3, created with backbuffercount 2 */
        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain8_GetBackBuffer(swapchain3, 0, 0, &backbuffer);
        ok(SUCCEEDED(hr), "Failed to get the 1st back buffer (%#08x)\n", hr);
        ok(backbuffer != NULL && backbuffer != (void *) 0xdeadbeef, "The back buffer is %p\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain8_GetBackBuffer(swapchain3, 1, 0, &backbuffer);
        ok(SUCCEEDED(hr), "Failed to get the 2nd back buffer (%#08x)\n", hr);
        ok(backbuffer != NULL && backbuffer != (void *) 0xdeadbeef, "The back buffer is %p\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain8_GetBackBuffer(swapchain3, 2, 0, &backbuffer);
        ok(hr == D3DERR_INVALIDCALL, "GetBackBuffer returned %#08x\n", hr);
        ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

        backbuffer = (void *) 0xdeadbeef;
        hr = IDirect3DSwapChain8_GetBackBuffer(swapchain3, 3, 0, &backbuffer);
        ok(FAILED(hr), "Failed to get the back buffer (%#08x)\n", hr);
        ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
        if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);
    }

    /* Check the back buffers of the swapchains */
    /* Swapchain 1, created with backbuffercount 0 */
    hr = IDirect3DSwapChain8_GetBackBuffer(swapchain1, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(SUCCEEDED(hr), "Failed to get the back buffer (%#08x)\n", hr);
    ok(backbuffer != NULL, "The back buffer is NULL (%#08x)\n", hr);
    if(backbuffer) IDirect3DSurface8_Release(backbuffer);

    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain8_GetBackBuffer(swapchain1, 1, 0, &backbuffer);
    ok(FAILED(hr), "Failed to get the back buffer (%#08x)\n", hr);
    ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

    /* Swapchain 2 - created with backbuffercount 1 */
    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain8_GetBackBuffer(swapchain2, 0, 0, &backbuffer);
    ok(SUCCEEDED(hr), "Failed to get the back buffer (%#08x)\n", hr);
    ok(backbuffer != NULL && backbuffer != (void *) 0xdeadbeef, "The back buffer is %p\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain8_GetBackBuffer(swapchain2, 1, 0, &backbuffer);
    ok(hr == D3DERR_INVALIDCALL, "GetBackBuffer returned %#08x\n", hr);
    ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

    backbuffer = (void *) 0xdeadbeef;
    hr = IDirect3DSwapChain8_GetBackBuffer(swapchain2, 2, 0, &backbuffer);
    ok(FAILED(hr), "Failed to get the back buffer (%#08x)\n", hr);
    ok(backbuffer == (void *) 0xdeadbeef, "The back buffer pointer was modified (%p)\n", backbuffer);
    if(backbuffer && backbuffer != (void *) 0xdeadbeef) IDirect3DSurface8_Release(backbuffer);

cleanup:
    if(swapchain1) IDirect3DSwapChain8_Release(swapchain1);
    if(swapchain2) IDirect3DSwapChain8_Release(swapchain2);
    if(swapchain3) IDirect3DSwapChain8_Release(swapchain3);
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice8_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IDirect3D8_Release(pD3d);
    DestroyWindow( hwnd );
}

static void test_refcount(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    IDirect3DVertexBuffer8      *pVertexBuffer      = NULL;
    IDirect3DIndexBuffer8       *pIndexBuffer       = NULL;
    DWORD                       dVertexShader       = -1;
    DWORD                       dPixelShader        = -1;
    IDirect3DCubeTexture8       *pCubeTexture       = NULL;
    IDirect3DTexture8           *pTexture           = NULL;
    IDirect3DVolumeTexture8     *pVolumeTexture     = NULL;
    IDirect3DVolume8            *pVolumeLevel       = NULL;
    IDirect3DSurface8           *pStencilSurface    = NULL;
    IDirect3DSurface8           *pImageSurface      = NULL;
    IDirect3DSurface8           *pRenderTarget      = NULL;
    IDirect3DSurface8           *pRenderTarget2     = NULL;
    IDirect3DSurface8           *pRenderTarget3     = NULL;
    IDirect3DSurface8           *pTextureLevel      = NULL;
    IDirect3DSurface8           *pBackBuffer        = NULL;
    DWORD                       dStateBlock         = -1;
    IDirect3DSwapChain8         *pSwapChain         = NULL;
    D3DCAPS8                    caps;

    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    int                          refcount = 0, tmp;

    DWORD decl[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT3),  /* D3DVSDE_POSITION, Register v0 */
        D3DVSD_REG(D3DVSDE_DIFFUSE, D3DVSDT_D3DCOLOR), /* D3DVSDE_DIFFUSE, Register v5 */
        D3DVSD_END()
    };

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }
    IDirect3DDevice8_GetDeviceCaps(pDevice, &caps);

    refcount = get_refcount( (IUnknown *)pDevice );
    ok(refcount == 1, "Invalid device RefCount %d\n", refcount);

    /**
     * Check refcount of implicit surfaces. Findings:
     *   - the container is the device
     *   - they hold a reference to the device
     *   - they are created with a refcount of 0 (Get/Release returns original refcount)
     *   - they are not freed if refcount reaches 0.
     *   - the refcount is not forwarded to the container.
     */
    hr = IDirect3DDevice8_GetRenderTarget(pDevice, &pRenderTarget);
    CHECK_CALL( hr, "GetRenderTarget", pDevice, ++refcount);
    if(pRenderTarget)
    {
        CHECK_SURFACE_CONTAINER( pRenderTarget, IID_IDirect3DDevice8, pDevice);
        CHECK_REFCOUNT( pRenderTarget, 1);

        CHECK_ADDREF_REFCOUNT(pRenderTarget, 2);
        CHECK_REFCOUNT(pDevice, refcount);
        CHECK_RELEASE_REFCOUNT(pRenderTarget, 1);
        CHECK_REFCOUNT(pDevice, refcount);

        hr = IDirect3DDevice8_GetRenderTarget(pDevice, &pRenderTarget);
        CHECK_CALL( hr, "GetRenderTarget", pDevice, refcount);
        CHECK_REFCOUNT( pRenderTarget, 2);
        CHECK_RELEASE_REFCOUNT( pRenderTarget, 1);
        CHECK_RELEASE_REFCOUNT( pRenderTarget, 0);
        CHECK_REFCOUNT( pDevice, --refcount);

        /* The render target is released with the device, so AddRef with refcount=0 is fine here. */
        CHECK_ADDREF_REFCOUNT(pRenderTarget, 1);
        CHECK_REFCOUNT(pDevice, ++refcount);
        CHECK_RELEASE_REFCOUNT(pRenderTarget, 0);
        CHECK_REFCOUNT(pDevice, --refcount);
    }

    /* Render target and back buffer are identical. */
    hr = IDirect3DDevice8_GetBackBuffer(pDevice, 0, 0, &pBackBuffer);
    CHECK_CALL( hr, "GetBackBuffer", pDevice, ++refcount);
    if(pBackBuffer)
    {
        CHECK_RELEASE_REFCOUNT(pBackBuffer, 0);
        ok(pRenderTarget == pBackBuffer, "RenderTarget=%p and BackBuffer=%p should be the same.\n",
           pRenderTarget, pBackBuffer);
        pBackBuffer = NULL;
    }
    CHECK_REFCOUNT( pDevice, --refcount);

    hr = IDirect3DDevice8_GetDepthStencilSurface(pDevice, &pStencilSurface);
    CHECK_CALL( hr, "GetDepthStencilSurface", pDevice, ++refcount);
    if(pStencilSurface)
    {
        CHECK_SURFACE_CONTAINER( pStencilSurface, IID_IDirect3DDevice8, pDevice);
        CHECK_REFCOUNT( pStencilSurface, 1);

        CHECK_ADDREF_REFCOUNT(pStencilSurface, 2);
        CHECK_REFCOUNT(pDevice, refcount);
        CHECK_RELEASE_REFCOUNT(pStencilSurface, 1);
        CHECK_REFCOUNT(pDevice, refcount);

        CHECK_RELEASE_REFCOUNT( pStencilSurface, 0);
        CHECK_REFCOUNT( pDevice, --refcount);

        /* The stencil surface is released with the device, so AddRef with refcount=0 is fine here. */
        CHECK_ADDREF_REFCOUNT(pStencilSurface, 1);
        CHECK_REFCOUNT(pDevice, ++refcount);
        CHECK_RELEASE_REFCOUNT(pStencilSurface, 0);
        CHECK_REFCOUNT(pDevice, --refcount);
        pStencilSurface = NULL;
    }

    /* Buffers */
    hr = IDirect3DDevice8_CreateIndexBuffer( pDevice, 16, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &pIndexBuffer );
    CHECK_CALL( hr, "CreateIndexBuffer", pDevice, ++refcount );
    if(pIndexBuffer)
    {
        tmp = get_refcount( (IUnknown *)pIndexBuffer );

        hr = IDirect3DDevice8_SetIndices(pDevice, pIndexBuffer, 0);
        CHECK_CALL( hr, "SetIndices", pIndexBuffer, tmp);
        hr = IDirect3DDevice8_SetIndices(pDevice, NULL, 0);
        CHECK_CALL( hr, "SetIndices", pIndexBuffer, tmp);
    }

    hr = IDirect3DDevice8_CreateVertexBuffer( pDevice, 16, 0, D3DFVF_XYZ, D3DPOOL_DEFAULT, &pVertexBuffer );
    CHECK_CALL( hr, "CreateVertexBuffer", pDevice, ++refcount );
    if(pVertexBuffer)
    {
        IDirect3DVertexBuffer8 *pVBuf = (void*)~0;
        UINT stride = ~0;

        tmp = get_refcount( (IUnknown *)pVertexBuffer );

        hr = IDirect3DDevice8_SetStreamSource(pDevice, 0, pVertexBuffer, 3 * sizeof(float));
        CHECK_CALL( hr, "SetStreamSource", pVertexBuffer, tmp);
        hr = IDirect3DDevice8_SetStreamSource(pDevice, 0, NULL, 0);
        CHECK_CALL( hr, "SetStreamSource", pVertexBuffer, tmp);

        hr = IDirect3DDevice8_GetStreamSource(pDevice, 0, &pVBuf, &stride);
        ok(SUCCEEDED(hr), "GetStreamSource did not succeed with NULL stream!\n");
        ok(pVBuf==NULL, "pVBuf not NULL (%p)!\n", pVBuf);
        ok(stride==3*sizeof(float), "stride not 3 floats (got %u)!\n", stride);
    }

    /* Shaders */
    hr = IDirect3DDevice8_CreateVertexShader( pDevice, decl, simple_vs, &dVertexShader, 0 );
    CHECK_CALL( hr, "CreateVertexShader", pDevice, refcount );
    if (caps.PixelShaderVersion >= D3DPS_VERSION(1, 0))
    {
        hr = IDirect3DDevice8_CreatePixelShader( pDevice, simple_ps, &dPixelShader );
        CHECK_CALL( hr, "CreatePixelShader", pDevice, refcount );
    }
    /* Textures */
    hr = IDirect3DDevice8_CreateTexture( pDevice, 32, 32, 3, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pTexture );
    CHECK_CALL( hr, "CreateTexture", pDevice, ++refcount );
    if (pTexture)
    {
        tmp = get_refcount( (IUnknown *)pTexture );

        /* SetTexture should not increase refcounts */
        hr = IDirect3DDevice8_SetTexture(pDevice, 0, (IDirect3DBaseTexture8 *) pTexture);
        CHECK_CALL( hr, "SetTexture", pTexture, tmp);
        hr = IDirect3DDevice8_SetTexture(pDevice, 0, NULL);
        CHECK_CALL( hr, "SetTexture", pTexture, tmp);

        /* This should not increment device refcount */
        hr = IDirect3DTexture8_GetSurfaceLevel( pTexture, 1, &pTextureLevel );
        CHECK_CALL( hr, "GetSurfaceLevel", pDevice, refcount );
        /* But should increment texture's refcount */
        CHECK_REFCOUNT( pTexture, tmp+1 );
        /* Because the texture and surface refcount are identical */
        if (pTextureLevel)
        {
            CHECK_REFCOUNT        ( pTextureLevel, tmp+1 );
            CHECK_ADDREF_REFCOUNT ( pTextureLevel, tmp+2 );
            CHECK_REFCOUNT        ( pTexture     , tmp+2 );
            CHECK_RELEASE_REFCOUNT( pTextureLevel, tmp+1 );
            CHECK_REFCOUNT        ( pTexture     , tmp+1 );
            CHECK_RELEASE_REFCOUNT( pTexture     , tmp   );
            CHECK_REFCOUNT        ( pTextureLevel, tmp   );
        }
    }
    if(caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP)
    {
        hr = IDirect3DDevice8_CreateCubeTexture( pDevice, 32, 0, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pCubeTexture );
        CHECK_CALL( hr, "CreateCubeTexture", pDevice, ++refcount );
    }
    else
    {
        skip("Cube textures not supported\n");
    }
    if(caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP)
    {
        hr = IDirect3DDevice8_CreateVolumeTexture( pDevice, 32, 32, 2, 0, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pVolumeTexture );
        CHECK_CALL( hr, "CreateVolumeTexture", pDevice, ++refcount );
    }
    else
    {
        skip("Volume textures not supported\n");
    }

    if (pVolumeTexture)
    {
        tmp = get_refcount( (IUnknown *)pVolumeTexture );

        /* This should not increment device refcount */
        hr = IDirect3DVolumeTexture8_GetVolumeLevel(pVolumeTexture, 0, &pVolumeLevel);
        CHECK_CALL( hr, "GetVolumeLevel", pDevice, refcount );
        /* But should increment volume texture's refcount */
        CHECK_REFCOUNT( pVolumeTexture, tmp+1 );
        /* Because the volume texture and volume refcount are identical */
        if (pVolumeLevel)
        {
            CHECK_REFCOUNT        ( pVolumeLevel  , tmp+1 );
            CHECK_ADDREF_REFCOUNT ( pVolumeLevel  , tmp+2 );
            CHECK_REFCOUNT        ( pVolumeTexture, tmp+2 );
            CHECK_RELEASE_REFCOUNT( pVolumeLevel  , tmp+1 );
            CHECK_REFCOUNT        ( pVolumeTexture, tmp+1 );
            CHECK_RELEASE_REFCOUNT( pVolumeTexture, tmp   );
            CHECK_REFCOUNT        ( pVolumeLevel  , tmp   );
        }
    }
    /* Surfaces */
    hr = IDirect3DDevice8_CreateDepthStencilSurface( pDevice, 32, 32, D3DFMT_D16, D3DMULTISAMPLE_NONE, &pStencilSurface );
    CHECK_CALL( hr, "CreateDepthStencilSurface", pDevice, ++refcount );
    CHECK_REFCOUNT( pStencilSurface, 1);
    hr = IDirect3DDevice8_CreateImageSurface( pDevice, 32, 32, D3DFMT_X8R8G8B8, &pImageSurface );
    CHECK_CALL( hr, "CreateImageSurface", pDevice, ++refcount );
    CHECK_REFCOUNT( pImageSurface, 1);
    hr = IDirect3DDevice8_CreateRenderTarget( pDevice, 32, 32, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, TRUE, &pRenderTarget3 );
    CHECK_CALL( hr, "CreateRenderTarget", pDevice, ++refcount );
    CHECK_REFCOUNT( pRenderTarget3, 1);
    /* Misc */
    hr = IDirect3DDevice8_CreateStateBlock( pDevice, D3DSBT_ALL, &dStateBlock );
    CHECK_CALL( hr, "CreateStateBlock", pDevice, refcount );
    hr = IDirect3DDevice8_CreateAdditionalSwapChain( pDevice, &d3dpp, &pSwapChain );
    CHECK_CALL( hr, "CreateAdditionalSwapChain", pDevice, ++refcount );
    if(pSwapChain)
    {
        /* check implicit back buffer */
        hr = IDirect3DSwapChain8_GetBackBuffer(pSwapChain, 0, 0, &pBackBuffer);
        CHECK_CALL( hr, "GetBackBuffer", pDevice, ++refcount);
        CHECK_REFCOUNT( pSwapChain, 1);
        if(pBackBuffer)
        {
            CHECK_SURFACE_CONTAINER( pBackBuffer, IID_IDirect3DDevice8, pDevice);
            CHECK_REFCOUNT( pBackBuffer, 1);
            CHECK_RELEASE_REFCOUNT( pBackBuffer, 0);
            CHECK_REFCOUNT( pDevice, --refcount);

            /* The back buffer is released with the swapchain, so AddRef with refcount=0 is fine here. */
            CHECK_ADDREF_REFCOUNT(pBackBuffer, 1);
            CHECK_REFCOUNT(pDevice, ++refcount);
            CHECK_RELEASE_REFCOUNT(pBackBuffer, 0);
            CHECK_REFCOUNT(pDevice, --refcount);
            pBackBuffer = NULL;
        }
        CHECK_REFCOUNT( pSwapChain, 1);
    }

    if(pVertexBuffer)
    {
        BYTE *data;
        /* Vertex buffers can be locked multiple times */
        hr = IDirect3DVertexBuffer8_Lock(pVertexBuffer, 0, 0, &data, 0);
        ok(hr == D3D_OK, "IDirect3DVertexBuffer8::Lock failed with %#08x\n", hr);
        hr = IDirect3DVertexBuffer8_Lock(pVertexBuffer, 0, 0, &data, 0);
        ok(hr == D3D_OK, "IDirect3DVertexBuffer8::Lock failed with %#08x\n", hr);
        hr = IDirect3DVertexBuffer8_Unlock(pVertexBuffer);
        ok(hr == D3D_OK, "IDirect3DVertexBuffer8::Unlock failed with %#08x\n", hr);
        hr = IDirect3DVertexBuffer8_Unlock(pVertexBuffer);
        ok(hr == D3D_OK, "IDirect3DVertexBuffer8::Unlock failed with %#08x\n", hr);
    }

    /* The implicit render target is not freed if refcount reaches 0.
     * Otherwise GetRenderTarget would re-allocate it and the pointer would change.*/
    hr = IDirect3DDevice8_GetRenderTarget(pDevice, &pRenderTarget2);
    CHECK_CALL( hr, "GetRenderTarget", pDevice, ++refcount);
    if(pRenderTarget2)
    {
        CHECK_RELEASE_REFCOUNT(pRenderTarget2, 0);
        ok(pRenderTarget == pRenderTarget2, "RenderTarget=%p and RenderTarget2=%p should be the same.\n",
           pRenderTarget, pRenderTarget2);
        CHECK_REFCOUNT( pDevice, --refcount);
        pRenderTarget2 = NULL;
    }
    pRenderTarget = NULL;

cleanup:
    CHECK_RELEASE(pDevice,              pDevice, --refcount);

    /* Buffers */
    CHECK_RELEASE(pVertexBuffer,        pDevice, --refcount);
    CHECK_RELEASE(pIndexBuffer,         pDevice, --refcount);
    /* Shaders */
    if (dVertexShader != ~0U) IDirect3DDevice8_DeleteVertexShader( pDevice, dVertexShader );
    if (dPixelShader != ~0U) IDirect3DDevice8_DeletePixelShader( pDevice, dPixelShader );
    /* Textures */
    CHECK_RELEASE(pTexture,             pDevice, --refcount);
    CHECK_RELEASE(pCubeTexture,         pDevice, --refcount);
    CHECK_RELEASE(pVolumeTexture,       pDevice, --refcount);
    /* Surfaces */
    CHECK_RELEASE(pStencilSurface,      pDevice, --refcount);
    CHECK_RELEASE(pImageSurface,        pDevice, --refcount);
    CHECK_RELEASE(pRenderTarget3,       pDevice, --refcount);
    /* Misc */
    if (dStateBlock != ~0U) IDirect3DDevice8_DeleteStateBlock( pDevice, dStateBlock );
    /* This will destroy device - cannot check the refcount here */
    if (pSwapChain)           CHECK_RELEASE_REFCOUNT( pSwapChain, 0);

    if (pD3d)                 CHECK_RELEASE_REFCOUNT( pD3d, 0);

    DestroyWindow( hwnd );
}

static void test_cursor(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    CURSORINFO                   info;
    IDirect3DSurface8 *cursor = NULL;
    HCURSOR cur;
    HMODULE user32_handle = GetModuleHandleA("user32.dll");

    pGetCursorInfo = (void *)GetProcAddress(user32_handle, "GetCursorInfo");
    if (!pGetCursorInfo)
    {
        win_skip("GetCursorInfo is not available\n");
        return;
    }

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    ok(pGetCursorInfo(&info), "GetCursorInfo failed\n");
    cur = info.hCursor;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }

    IDirect3DDevice8_CreateImageSurface(pDevice, 32, 32, D3DFMT_A8R8G8B8, &cursor);
    ok(cursor != NULL, "IDirect3DDevice8_CreateOffscreenPlainSurface failed with %#08x\n", hr);

    /* Initially hidden */
    hr = IDirect3DDevice8_ShowCursor(pDevice, TRUE);
    ok(hr == FALSE, "IDirect3DDevice8_ShowCursor returned %#08x\n", hr);

    /* Not enabled without a surface*/
    hr = IDirect3DDevice8_ShowCursor(pDevice, TRUE);
    ok(hr == FALSE, "IDirect3DDevice8_ShowCursor returned %#08x\n", hr);

    /* Fails */
    hr = IDirect3DDevice8_SetCursorProperties(pDevice, 0, 0, NULL);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_SetCursorProperties returned %#08x\n", hr);

    hr = IDirect3DDevice8_SetCursorProperties(pDevice, 0, 0, cursor);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetCursorProperties returned %#08x\n", hr);

    IDirect3DSurface8_Release(cursor);

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    ok(pGetCursorInfo(&info), "GetCursorInfo failed\n");
    ok(info.flags & CURSOR_SHOWING, "The gdi cursor is hidden (%08x)\n", info.flags);
    ok(info.hCursor == cur, "The cursor handle is %p\n", info.hCursor); /* unchanged */

    /* Still hidden */
    hr = IDirect3DDevice8_ShowCursor(pDevice, TRUE);
    ok(hr == FALSE, "IDirect3DDevice8_ShowCursor returned %#08x\n", hr);

    /* Enabled now*/
    hr = IDirect3DDevice8_ShowCursor(pDevice, TRUE);
    ok(hr == TRUE, "IDirect3DDevice8_ShowCursor returned %#08x\n", hr);

    /* GDI cursor unchanged */
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    ok(pGetCursorInfo(&info), "GetCursorInfo failed\n");
    ok(info.flags & CURSOR_SHOWING, "The gdi cursor is hidden (%08x)\n", info.flags);
    ok(info.hCursor == cur, "The cursor handle is %p\n", info.hCursor); /* unchanged */

cleanup:
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice8_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IDirect3D8_Release(pD3d);
}

static void test_states(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = d3ddm.Format;

    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }

    hr = IDirect3DDevice8_SetRenderState(pDevice, D3DRS_ZVISIBLE, TRUE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState(D3DRS_ZVISIBLE, TRUE) returned %#08x\n", hr);
    hr = IDirect3DDevice8_SetRenderState(pDevice, D3DRS_ZVISIBLE, FALSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetRenderState(D3DRS_ZVISIBLE, FALSE) returned %#08x\n", hr);

cleanup:
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice8_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IDirect3D8_Release(pD3d);
}

static void test_shader_versions(void)
{
    HRESULT                      hr;
    IDirect3D8                  *pD3d               = NULL;
    D3DCAPS8                     d3dcaps;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    if (pD3d != NULL) {
        hr = IDirect3D8_GetDeviceCaps(pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dcaps);
        ok(SUCCEEDED(hr) || hr == D3DERR_NOTAVAILABLE, "Failed to get D3D8 caps (%#08x)\n", hr);
        if (SUCCEEDED(hr)) {
            ok(d3dcaps.VertexShaderVersion <= D3DVS_VERSION(1,1), "Unexpected VertexShaderVersion (%#x > %#x)\n", d3dcaps.VertexShaderVersion, D3DVS_VERSION(1,1));
            ok(d3dcaps.PixelShaderVersion <= D3DPS_VERSION(1,4), "Unexpected PixelShaderVersion (%#x > %#x)\n", d3dcaps.PixelShaderVersion, D3DPS_VERSION(1,4));
        } else {
            skip("No Direct3D support\n");
        }
        IDirect3D8_Release(pD3d);
    }
}


/* Test adapter display modes */
static void test_display_modes(void)
{
    UINT max_modes, i;
    D3DDISPLAYMODE dmode;
    HRESULT res;
    IDirect3D8 *pD3d;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    if(!pD3d) return;

    max_modes = IDirect3D8_GetAdapterModeCount(pD3d, D3DADAPTER_DEFAULT);
    ok(max_modes > 0 ||
       broken(max_modes == 0), /* VMware */
       "GetAdapterModeCount(D3DADAPTER_DEFAULT) returned 0!\n");

    for(i=0; i<max_modes;i++) {
        res = IDirect3D8_EnumAdapterModes(pD3d, D3DADAPTER_DEFAULT, i, &dmode);
        ok(res==D3D_OK, "EnumAdapterModes returned %#08x for mode %u!\n", res, i);
        if(res != D3D_OK)
            continue;

        ok(dmode.Format==D3DFMT_X8R8G8B8 || dmode.Format==D3DFMT_R5G6B5,
           "Unexpected display mode returned for mode %u: %#x\n", i , dmode.Format);
    }

    IDirect3D8_Release(pD3d);
}

static void test_scene(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;


    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL || broken(hr == D3DERR_NOTAVAILABLE), "IDirect3D8_CreateDevice failed with %#08x\n", hr);
    if(!pDevice)
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }

    /* Test an EndScene without beginscene. Should return an error */
    hr = IDirect3DDevice8_EndScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_EndScene returned %#08x\n", hr);

    /* Test a normal BeginScene / EndScene pair, this should work */
    hr = IDirect3DDevice8_BeginScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed with %#08x\n", hr);
    if(SUCCEEDED(hr))
    {
        hr = IDirect3DDevice8_EndScene(pDevice);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed with %#08x\n", hr);
    }

    /* Test another EndScene without having begun a new scene. Should return an error */
    hr = IDirect3DDevice8_EndScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_EndScene returned %#08x\n", hr);

    /* Two nested BeginScene and EndScene calls */
    hr = IDirect3DDevice8_BeginScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed with %#08x\n", hr);
    hr = IDirect3DDevice8_BeginScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_BeginScene returned %#08x\n", hr);
    hr = IDirect3DDevice8_EndScene(pDevice);
    ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed with %#08x\n", hr);
    hr = IDirect3DDevice8_EndScene(pDevice);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_EndScene returned %#08x\n", hr);

    /* StretchRect does not exit in Direct3D8, so no equivalent to the d3d9 stretchrect tests */

cleanup:
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice8_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IDirect3D8_Release(pD3d);
    if(hwnd) DestroyWindow(hwnd);
}

static void test_shader(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    DWORD                        hPixelShader = 0, hVertexShader = 0;
    DWORD                        hPixelShader2 = 0, hVertexShader2 = 0;
    DWORD                        hTempHandle;
    D3DCAPS8                     caps;
    DWORD fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    DWORD data_size;
    void *data;

    static DWORD dwVertexDecl[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(D3DVSDE_POSITION,  D3DVSDT_FLOAT3),
        D3DVSD_END()
    };
    DWORD decl_normal_float2[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT3),  /* D3DVSDE_POSITION, Register v0 */
        D3DVSD_REG(D3DVSDE_NORMAL,   D3DVSDT_FLOAT2),  /* D3DVSDE_NORMAL,   Register v1 */
        D3DVSD_END()
    };
    DWORD decl_normal_float4[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT3),  /* D3DVSDE_POSITION, Register v0 */
        D3DVSD_REG(D3DVSDE_NORMAL,   D3DVSDT_FLOAT4),  /* D3DVSDE_NORMAL,   Register v1 */
        D3DVSD_END()
    };
    DWORD decl_normal_d3dcolor[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(D3DVSDE_POSITION, D3DVSDT_FLOAT3),  /* D3DVSDE_POSITION, Register v0 */
        D3DVSD_REG(D3DVSDE_NORMAL,   D3DVSDT_D3DCOLOR),/* D3DVSDE_NORMAL,   Register v1 */
        D3DVSD_END()
    };
    const DWORD vertex_decl_size = sizeof(dwVertexDecl);
    const DWORD simple_vs_size = sizeof(simple_vs);
    const DWORD simple_ps_size = sizeof(simple_ps);

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;


    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL || broken(hr == D3DERR_NOTAVAILABLE), "IDirect3D8_CreateDevice failed with %#08x\n", hr);
    if(!pDevice)
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }
    IDirect3DDevice8_GetDeviceCaps(pDevice, &caps);

    /* Test setting and retrieving a FVF */
    hr = IDirect3DDevice8_SetVertexShader(pDevice, fvf);
    ok(SUCCEEDED(hr), "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_GetVertexShader(pDevice, &hTempHandle);
    ok(SUCCEEDED(hr), "IDirect3DDevice8_GetVertexShader returned %#08x\n", hr);
    ok(hTempHandle == fvf, "Vertex shader %#08x is set, expected %#08x\n", hTempHandle, fvf);

    /* First create a vertex shader */
    hr = IDirect3DDevice8_SetVertexShader(pDevice, 0);
    ok(SUCCEEDED(hr), "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_CreateVertexShader(pDevice, dwVertexDecl, simple_vs, &hVertexShader, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);
    /* Msdn says that the new vertex shader is set immediately. This is wrong, apparently */
    hr = IDirect3DDevice8_GetVertexShader(pDevice, &hTempHandle);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShader returned %#08x\n", hr);
    ok(hTempHandle == 0, "Vertex Shader %d is set, expected shader %d\n", hTempHandle, 0);
    /* Assign the shader, then verify that GetVertexShader works */
    hr = IDirect3DDevice8_SetVertexShader(pDevice, hVertexShader);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_GetVertexShader(pDevice, &hTempHandle);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShader returned %#08x\n", hr);
    ok(hTempHandle == hVertexShader, "Vertex Shader %d is set, expected shader %d\n", hTempHandle, hVertexShader);
    /* Verify that we can retrieve the declaration */
    hr = IDirect3DDevice8_GetVertexShaderDeclaration(pDevice, hVertexShader, NULL, &data_size);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShaderDeclaration returned %#08x\n", hr);
    ok(data_size == vertex_decl_size, "Got data_size %u, expected %u\n", data_size, vertex_decl_size);
    data = HeapAlloc(GetProcessHeap(), 0, vertex_decl_size);
    data_size = 1;
    hr = IDirect3DDevice8_GetVertexShaderDeclaration(pDevice, hVertexShader, data, &data_size);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_GetVertexShaderDeclaration returned (%#08x), "
            "expected D3DERR_INVALIDCALL\n", hr);
    ok(data_size == 1, "Got data_size %u, expected 1\n", data_size);
    data_size = vertex_decl_size;
    hr = IDirect3DDevice8_GetVertexShaderDeclaration(pDevice, hVertexShader, data, &data_size);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShaderDeclaration returned %#08x\n", hr);
    ok(data_size == vertex_decl_size, "Got data_size %u, expected %u\n", data_size, vertex_decl_size);
    ok(!memcmp(data, dwVertexDecl, vertex_decl_size), "data not equal to shader declaration\n");
    HeapFree(GetProcessHeap(), 0, data);
    /* Verify that we can retrieve the shader function */
    hr = IDirect3DDevice8_GetVertexShaderFunction(pDevice, hVertexShader, NULL, &data_size);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShaderFunction returned %#08x\n", hr);
    ok(data_size == simple_vs_size, "Got data_size %u, expected %u\n", data_size, simple_vs_size);
    data = HeapAlloc(GetProcessHeap(), 0, simple_vs_size);
    data_size = 1;
    hr = IDirect3DDevice8_GetVertexShaderFunction(pDevice, hVertexShader, data, &data_size);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_GetVertexShaderFunction returned (%#08x), "
            "expected D3DERR_INVALIDCALL\n", hr);
    ok(data_size == 1, "Got data_size %u, expected 1\n", data_size);
    data_size = simple_vs_size;
    hr = IDirect3DDevice8_GetVertexShaderFunction(pDevice, hVertexShader, data, &data_size);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShaderFunction returned %#08x\n", hr);
    ok(data_size == simple_vs_size, "Got data_size %u, expected %u\n", data_size, simple_vs_size);
    ok(!memcmp(data, simple_vs, simple_vs_size), "data not equal to shader function\n");
    HeapFree(GetProcessHeap(), 0, data);
    /* Delete the assigned shader. This is supposed to work */
    hr = IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);
    ok(hr == D3D_OK, "IDirect3DDevice8_DeleteVertexShader returned %#08x\n", hr);
    /* The shader should be unset now */
    hr = IDirect3DDevice8_GetVertexShader(pDevice, &hTempHandle);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShader returned %#08x\n", hr);
    ok(hTempHandle == 0, "Vertex Shader %d is set, expected shader %d\n", hTempHandle, 0);

    /* Test a broken declaration. 3DMark2001 tries to use normals with 2 components
     * First try the fixed function shader function, then a custom one
     */
    hr = IDirect3DDevice8_CreateVertexShader(pDevice, decl_normal_float2, 0, &hVertexShader, 0);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);
    if(SUCCEEDED(hr)) IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);
    hr = IDirect3DDevice8_CreateVertexShader(pDevice, decl_normal_float4, 0, &hVertexShader, 0);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);
    if(SUCCEEDED(hr)) IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);
    hr = IDirect3DDevice8_CreateVertexShader(pDevice, decl_normal_d3dcolor, 0, &hVertexShader, 0);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);
    if(SUCCEEDED(hr)) IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);

    hr = IDirect3DDevice8_CreateVertexShader(pDevice, decl_normal_float2, simple_vs, &hVertexShader, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);
    if(SUCCEEDED(hr)) IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);

    if (caps.PixelShaderVersion >= D3DPS_VERSION(1, 0))
    {
        /* The same with a pixel shader */
        hr = IDirect3DDevice8_CreatePixelShader(pDevice, simple_ps, &hPixelShader);
        ok(hr == D3D_OK, "IDirect3DDevice8_CreatePixelShader returned %#08x\n", hr);
        /* Msdn says that the new pixel shader is set immediately. This is wrong, apparently */
        hr = IDirect3DDevice8_GetPixelShader(pDevice, &hTempHandle);
        ok(hr == D3D_OK, "IDirect3DDevice8_GetPixelShader returned %#08x\n", hr);
        ok(hTempHandle == 0, "Pixel Shader %d is set, expected shader %d\n", hTempHandle, 0);
        /* Assign the shader, then verify that GetPixelShader works */
        hr = IDirect3DDevice8_SetPixelShader(pDevice, hPixelShader);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetPixelShader returned %#08x\n", hr);
        hr = IDirect3DDevice8_GetPixelShader(pDevice, &hTempHandle);
        ok(hr == D3D_OK, "IDirect3DDevice8_GetPixelShader returned %#08x\n", hr);
        ok(hTempHandle == hPixelShader, "Pixel Shader %d is set, expected shader %d\n", hTempHandle, hPixelShader);
        /* Verify that we can retrieve the shader function */
        hr = IDirect3DDevice8_GetPixelShaderFunction(pDevice, hPixelShader, NULL, &data_size);
        ok(hr == D3D_OK, "IDirect3DDevice8_GetPixelShaderFunction returned %#08x\n", hr);
        ok(data_size == simple_ps_size, "Got data_size %u, expected %u\n", data_size, simple_ps_size);
        data = HeapAlloc(GetProcessHeap(), 0, simple_ps_size);
        data_size = 1;
        hr = IDirect3DDevice8_GetPixelShaderFunction(pDevice, hPixelShader, data, &data_size);
        ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_GetPixelShaderFunction returned (%#08x), "
                "expected D3DERR_INVALIDCALL\n", hr);
        ok(data_size == 1, "Got data_size %u, expected 1\n", data_size);
        data_size = simple_ps_size;
        hr = IDirect3DDevice8_GetPixelShaderFunction(pDevice, hPixelShader, data, &data_size);
        ok(hr == D3D_OK, "IDirect3DDevice8_GetPixelShaderFunction returned %#08x\n", hr);
        ok(data_size == simple_ps_size, "Got data_size %u, expected %u\n", data_size, simple_ps_size);
        ok(!memcmp(data, simple_ps, simple_ps_size), "data not equal to shader function\n");
        HeapFree(GetProcessHeap(), 0, data);
        /* Delete the assigned shader. This is supposed to work */
        hr = IDirect3DDevice8_DeletePixelShader(pDevice, hPixelShader);
        ok(hr == D3D_OK, "IDirect3DDevice8_DeletePixelShader returned %#08x\n", hr);
        /* The shader should be unset now */
        hr = IDirect3DDevice8_GetPixelShader(pDevice, &hTempHandle);
        ok(hr == D3D_OK, "IDirect3DDevice8_GetPixelShader returned %#08x\n", hr);
        ok(hTempHandle == 0, "Pixel Shader %d is set, expected shader %d\n", hTempHandle, 0);

        /* What happens if a non-bound shader is deleted? */
        hr = IDirect3DDevice8_CreatePixelShader(pDevice, simple_ps, &hPixelShader);
        ok(hr == D3D_OK, "IDirect3DDevice8_CreatePixelShader returned %#08x\n", hr);
        hr = IDirect3DDevice8_CreatePixelShader(pDevice, simple_ps, &hPixelShader2);
        ok(hr == D3D_OK, "IDirect3DDevice8_CreatePixelShader returned %#08x\n", hr);

        hr = IDirect3DDevice8_SetPixelShader(pDevice, hPixelShader);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetPixelShader returned %#08x\n", hr);
        hr = IDirect3DDevice8_DeletePixelShader(pDevice, hPixelShader2);
        ok(hr == D3D_OK, "IDirect3DDevice8_DeletePixelShader returned %#08x\n", hr);
        hr = IDirect3DDevice8_GetPixelShader(pDevice, &hTempHandle);
        ok(hr == D3D_OK, "IDirect3DDevice8_GetPixelShader returned %#08x\n", hr);
        ok(hTempHandle == hPixelShader, "Pixel Shader %d is set, expected shader %d\n", hTempHandle, hPixelShader);
        hr = IDirect3DDevice8_DeletePixelShader(pDevice, hPixelShader);
        ok(hr == D3D_OK, "IDirect3DDevice8_DeletePixelShader returned %#08x\n", hr);

        /* Check for double delete. */
        hr = IDirect3DDevice8_DeletePixelShader(pDevice, hPixelShader2);
        ok(hr == D3DERR_INVALIDCALL || broken(hr == D3D_OK), "IDirect3DDevice8_DeletePixelShader returned %#08x\n", hr);
        hr = IDirect3DDevice8_DeletePixelShader(pDevice, hPixelShader);
        ok(hr == D3DERR_INVALIDCALL || broken(hr == D3D_OK), "IDirect3DDevice8_DeletePixelShader returned %#08x\n", hr);
    }
    else
    {
        skip("Pixel shaders not supported\n");
    }

    /* What happens if a non-bound shader is deleted? */
    hr = IDirect3DDevice8_CreateVertexShader(pDevice, dwVertexDecl, NULL, &hVertexShader, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_CreateVertexShader(pDevice, dwVertexDecl, NULL, &hVertexShader2, 0);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateVertexShader returned %#08x\n", hr);

    hr = IDirect3DDevice8_SetVertexShader(pDevice, hVertexShader);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader2);
    ok(hr == D3D_OK, "IDirect3DDevice8_DeleteVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_GetVertexShader(pDevice, &hTempHandle);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetVertexShader returned %#08x\n", hr);
    ok(hTempHandle == hVertexShader, "Vertex Shader %d is set, expected shader %d\n", hTempHandle, hVertexShader);
    hr = IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);
    ok(hr == D3D_OK, "IDirect3DDevice8_DeleteVertexShader returned %#08x\n", hr);

    /* Check for double delete. */
    hr = IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader2);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_DeleteVertexShader returned %#08x\n", hr);
    hr = IDirect3DDevice8_DeleteVertexShader(pDevice, hVertexShader);
    ok(hr == D3DERR_INVALIDCALL, "IDirect3DDevice8_DeleteVertexShader returned %#08x\n", hr);

cleanup:
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice8_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IDirect3D8_Release(pD3d);
    if(hwnd) DestroyWindow(hwnd);
}

static void test_limits(void)
{
    HRESULT                      hr;
    HWND                         hwnd               = NULL;
    IDirect3D8                  *pD3d               = NULL;
    IDirect3DDevice8            *pDevice            = NULL;
    D3DPRESENT_PARAMETERS        d3dpp;
    D3DDISPLAYMODE               d3ddm;
    IDirect3DTexture8           *pTexture           = NULL;
    int i;

    pD3d = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(pD3d != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!pD3d || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( pD3d, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D8_CreateDevice( pD3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice );
    ok(hr == D3D_OK || hr == D3DERR_INVALIDCALL || broken(hr == D3DERR_NOTAVAILABLE), "IDirect3D8_CreateDevice failed with %#08x\n", hr);
    if(!pDevice)
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#08x\n", hr);
        goto cleanup;
    }

    hr = IDirect3DDevice8_CreateTexture(pDevice, 16, 16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture);
    ok(hr == D3D_OK, "IDirect3DDevice8_CreateTexture failed with %#08x\n", hr);
    if(!pTexture) goto cleanup;

    /* There are 8 texture stages. We should be able to access all of them */
    for(i = 0; i < 8; i++) {
        hr = IDirect3DDevice8_SetTexture(pDevice, i, (IDirect3DBaseTexture8 *) pTexture);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture for sampler %d failed with %#08x\n", i, hr);
        hr = IDirect3DDevice8_SetTexture(pDevice, i, NULL);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTexture for sampler %d failed with %#08x\n", i, hr);
        hr = IDirect3DDevice8_SetTextureStageState(pDevice, i, D3DTSS_COLOROP, D3DTOP_ADD);
        ok(hr == D3D_OK, "IDirect3DDevice8_SetTextureStageState for texture %d failed with %#08x\n", i, hr);
    }

    /* Investigations show that accessing higher textures stage states does not return an error either. Writing
     * to too high texture stages(approximately texture 40) causes memory corruption in windows, so there is no
     * bounds checking but how do I test that?
     */

cleanup:
    if(pTexture) IDirect3DTexture8_Release(pTexture);
    if (pDevice)
    {
        UINT refcount = IDirect3DDevice8_Release(pDevice);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (pD3d) IDirect3D8_Release(pD3d);
    if(hwnd) DestroyWindow(hwnd);
}

static void test_lights(void)
{
    D3DPRESENT_PARAMETERS d3dpp;
    IDirect3DDevice8 *device = NULL;
    IDirect3D8 *d3d8;
    HWND hwnd;
    HRESULT hr;
    unsigned int i;
    BOOL enabled;
    D3DCAPS8 caps;
    D3DDISPLAYMODE               d3ddm;

    d3d8 = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(d3d8 != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d8 || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( d3d8, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D8_CreateDevice( d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &d3dpp, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE || hr == D3DERR_INVALIDCALL,
       "IDirect3D8_CreateDevice failed with %08x\n", hr);
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    memset(&caps, 0, sizeof(caps));
    hr = IDirect3DDevice8_GetDeviceCaps(device, &caps);
    ok(hr == D3D_OK, "IDirect3DDevice8_GetDeviceCaps failed with %08x\n", hr);

    for(i = 1; i <= caps.MaxActiveLights; i++) {
        hr = IDirect3DDevice8_LightEnable(device, i, TRUE);
        ok(hr == D3D_OK, "Enabling light %u failed with %08x\n", i, hr);
        hr = IDirect3DDevice8_GetLightEnable(device, i, &enabled);
        ok(hr == D3D_OK || broken(hr == D3DERR_INVALIDCALL),
            "GetLightEnable on light %u failed with %08x\n", i, hr);
        ok(enabled, "Light %d is %s\n", i, enabled ? "enabled" : "disabled");
    }

    /* TODO: Test the rendering results in this situation */
    hr = IDirect3DDevice8_LightEnable(device, i + 1, TRUE);
    ok(hr == D3D_OK ||
       broken(hr == D3DERR_INVALIDCALL), /* Some Win9x and WinME */
       "Enabling one light more than supported returned %08x\n", hr);
    hr = IDirect3DDevice8_GetLightEnable(device, i + 1, &enabled);
    ok(hr == D3D_OK ||
       broken(hr == D3DERR_INVALIDCALL), /* Some Win9x and WinME */
       "GetLightEnable on light %u failed with %08x\n", i + 1, hr);
    ok(enabled, "Light %d is %s\n", i + 1, enabled ? "enabled" : "disabled");
    hr = IDirect3DDevice8_LightEnable(device, i + 1, FALSE);
    ok(hr == D3D_OK, "Disabling the additional returned %08x\n", hr);

    for(i = 1; i <= caps.MaxActiveLights; i++) {
        hr = IDirect3DDevice8_LightEnable(device, i, FALSE);
        ok(hr == D3D_OK, "Disabling light %u failed with %08x\n", i, hr);
    }

cleanup:
    if (device)
    {
        UINT refcount = IDirect3DDevice8_Release(device);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (d3d8) IDirect3D8_Release(d3d8);
}

static void test_render_zero_triangles(void)
{
    D3DPRESENT_PARAMETERS d3dpp;
    IDirect3DDevice8 *device = NULL;
    IDirect3D8 *d3d8;
    HWND hwnd;
    HRESULT hr;
    D3DDISPLAYMODE               d3ddm;

    struct nvertex
    {
        float x, y, z;
        float nx, ny, nz;
        DWORD diffuse;
    }  quad[] =
    {
        { 0.0f, -1.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
        { 0.0f,  0.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
        { 1.0f,  0.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
        { 1.0f, -1.0f,   0.1f,  1.0f,   1.0f,   1.0f,   0xff0000ff},
    };

    d3d8 = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(d3d8 != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d8 || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( d3d8, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D8_CreateDevice( d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL /* no NULLREF here */, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &d3dpp, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE || hr == D3DERR_INVALIDCALL,
       "IDirect3D8_CreateDevice failed with %08x\n", hr);
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    hr = IDirect3DDevice8_SetVertexShader(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    ok(hr == D3D_OK, "IDirect3DDevice8_SetVertexShader returned %#08x\n", hr);

    hr = IDirect3DDevice8_BeginScene(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_BeginScene failed with %#08x\n", hr);
    if(hr == D3D_OK)
    {
        hr = IDirect3DDevice8_DrawIndexedPrimitiveUP(device, D3DPT_TRIANGLELIST, 0 /* MinIndex */, 0 /* NumVerts */,
                                                    0 /*PrimCount */, NULL, D3DFMT_INDEX16, quad, sizeof(quad[0]));
        ok(hr == D3D_OK, "IDirect3DDevice8_DrawIndexedPrimitiveUP failed with %#08x\n", hr);

        IDirect3DDevice8_EndScene(device);
        ok(hr == D3D_OK, "IDirect3DDevice8_EndScene failed with %#08x\n", hr);
    }

    IDirect3DDevice8_Present(device, NULL, NULL, NULL, NULL);

cleanup:
    if (device)
    {
        UINT refcount = IDirect3DDevice8_Release(device);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (d3d8) IDirect3D8_Release(d3d8);
}

static void test_depth_stencil_reset(void)
{
    D3DPRESENT_PARAMETERS present_parameters;
    D3DDISPLAYMODE display_mode;
    IDirect3DSurface8 *surface;
    IDirect3DDevice8 *device = NULL;
    IDirect3D8 *d3d8;
    UINT refcount;
    HRESULT hr;
    HWND hwnd;

    d3d8 = pDirect3DCreate8(D3D_SDK_VERSION);
    ok(d3d8 != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow("static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d8 || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode(d3d8, D3DADAPTER_DEFAULT, &display_mode);
    memset(&present_parameters, 0, sizeof(present_parameters));
    present_parameters.Windowed               = TRUE;
    present_parameters.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferFormat       = display_mode.Format;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    hr = IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device);
    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#x\n", hr);
        goto cleanup;
    }

    hr = IDirect3DDevice8_TestCooperativeLevel(device);
    ok(SUCCEEDED(hr), "TestCooperativeLevel failed with %#x\n", hr);

    hr = IDirect3DDevice8_SetRenderTarget(device, NULL, NULL);
    ok(hr == D3D_OK, "SetRenderTarget failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_GetRenderTarget(device, &surface);
    ok(hr == D3D_OK, "GetRenderTarget failed with 0x%08x\n", hr);
    ok(surface != NULL, "Render target should not be NULL\n");
    if (surface) IDirect3DSurface8_Release(surface);

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &surface);
    ok(hr == D3DERR_NOTFOUND, "GetDepthStencilSurface returned 0x%08x, expected D3DERR_NOTFOUND\n", hr);
    ok(surface == NULL, "Depth stencil should be NULL\n");

    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;
    hr = IDirect3DDevice8_Reset(device, &present_parameters);
    ok(hr == D3D_OK, "Reset failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &surface);
    ok(hr == D3D_OK, "GetDepthStencilSurface failed with 0x%08x\n", hr);
    ok(surface != NULL, "Depth stencil should not be NULL\n");
    if (surface) IDirect3DSurface8_Release(surface);

    present_parameters.EnableAutoDepthStencil = FALSE;
    hr = IDirect3DDevice8_Reset(device, &present_parameters);
    ok(hr == D3D_OK, "Reset failed with 0x%08x\n", hr);

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &surface);
    ok(hr == D3DERR_NOTFOUND, "GetDepthStencilSurface returned 0x%08x, expected D3DERR_NOTFOUND\n", hr);
    ok(surface == NULL, "Depth stencil should be NULL\n");

    refcount = IDirect3DDevice8_Release(device);
    ok(!refcount, "Device has %u references left.\n", refcount);
    device = NULL;

    IDirect3D8_GetAdapterDisplayMode( d3d8, D3DADAPTER_DEFAULT, &display_mode );

    ZeroMemory( &present_parameters, sizeof(present_parameters) );
    present_parameters.Windowed         = TRUE;
    present_parameters.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferFormat = display_mode.Format;
    present_parameters.EnableAutoDepthStencil = FALSE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    hr = IDirect3D8_CreateDevice( d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device );

    if(FAILED(hr))
    {
        skip("could not create device, IDirect3D8_CreateDevice returned %#x\n", hr);
        goto cleanup;
    }

    hr = IDirect3DDevice8_TestCooperativeLevel(device);
    ok(hr == D3D_OK, "IDirect3DDevice8_TestCooperativeLevel after creation returned %#x\n", hr);

    present_parameters.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    present_parameters.Windowed         = TRUE;
    present_parameters.BackBufferWidth  = 400;
    present_parameters.BackBufferHeight = 300;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    hr = IDirect3DDevice8_Reset(device, &present_parameters);
    ok(hr == D3D_OK, "IDirect3DDevice8_Reset failed with 0x%08x\n", hr);

    if (FAILED(hr)) goto cleanup;

    hr = IDirect3DDevice8_GetDepthStencilSurface(device, &surface);
    ok(hr == D3D_OK, "GetDepthStencilSurface failed with 0x%08x\n", hr);
    ok(surface != NULL, "Depth stencil should not be NULL\n");
    if (surface) IDirect3DSurface8_Release(surface);

cleanup:
    if (device)
    {
        refcount = IDirect3DDevice8_Release(device);
        ok(!refcount, "Device has %u references left.\n", refcount);
    }
    if (d3d8) IDirect3D8_Release(d3d8);
}

static HWND filter_messages;

enum message_window
{
    DEVICE_WINDOW,
    FOCUS_WINDOW,
};

struct message
{
    UINT message;
    enum message_window window;
};

static const struct message *expect_messages;
static HWND device_window, focus_window;

struct wndproc_thread_param
{
    HWND dummy_window;
    HANDLE window_created;
    HANDLE test_finished;
};

static LRESULT CALLBACK test_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (filter_messages && filter_messages == hwnd)
    {
        if (message != WM_DISPLAYCHANGE && message != WM_IME_NOTIFY)
            todo_wine ok(0, "Received unexpected message %#x for window %p.\n", message, hwnd);
    }

    if (expect_messages)
    {
        HWND w;

        switch (expect_messages->window)
        {
            case DEVICE_WINDOW:
                w = device_window;
                break;

            case FOCUS_WINDOW:
                w = focus_window;
                break;

            default:
                w = NULL;
                break;
        };

        if (hwnd == w && expect_messages->message == message) ++expect_messages;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static DWORD WINAPI wndproc_thread(void *param)
{
    struct wndproc_thread_param *p = param;
    DWORD res;
    BOOL ret;

    p->dummy_window = CreateWindowA("d3d8_test_wndproc_wc", "d3d8_test",
            WS_MAXIMIZE | WS_VISIBLE | WS_CAPTION, 0, 0, 640, 480, 0, 0, 0, 0);

    ret = SetEvent(p->window_created);
    ok(ret, "SetEvent failed, last error %#x.\n", GetLastError());

    for (;;)
    {
        MSG msg;

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
        res = WaitForSingleObject(p->test_finished, 100);
        if (res == WAIT_OBJECT_0) break;
        if (res != WAIT_TIMEOUT)
        {
            ok(0, "Wait failed (%#x), last error %#x.\n", res, GetLastError());
            break;
        }
    }

    DestroyWindow(p->dummy_window);

    return 0;
}

static void test_wndproc(void)
{
    struct wndproc_thread_param thread_params;
    IDirect3DDevice8 *device;
    WNDCLASSA wc = {0};
    IDirect3D8 *d3d8;
    HANDLE thread;
    LONG_PTR proc;
    ULONG ref;
    DWORD res, tid;
    HWND tmp;

    static const struct message messages[] =
    {
        {WM_WINDOWPOSCHANGING,  FOCUS_WINDOW},
        {WM_ACTIVATE,           FOCUS_WINDOW},
        {WM_SETFOCUS,           FOCUS_WINDOW},
        {WM_WINDOWPOSCHANGING,  DEVICE_WINDOW},
        {WM_MOVE,               DEVICE_WINDOW},
        {WM_SIZE,               DEVICE_WINDOW},
        {0,                     0},
    };

    if (!(d3d8 = pDirect3DCreate8(D3D_SDK_VERSION)))
    {
        skip("Failed to create IDirect3D8 object, skipping tests.\n");
        return;
    }

    wc.lpfnWndProc = test_proc;
    wc.lpszClassName = "d3d8_test_wndproc_wc";
    ok(RegisterClassA(&wc), "Failed to register window class.\n");

    thread_params.window_created = CreateEvent(NULL, FALSE, FALSE, NULL);
    ok(!!thread_params.window_created, "CreateEvent failed, last error %#x.\n", GetLastError());
    thread_params.test_finished = CreateEvent(NULL, FALSE, FALSE, NULL);
    ok(!!thread_params.test_finished, "CreateEvent failed, last error %#x.\n", GetLastError());

    focus_window = CreateWindowA("d3d8_test_wndproc_wc", "d3d8_test",
            WS_MAXIMIZE | WS_VISIBLE | WS_CAPTION , 0, 0, 640, 480, 0, 0, 0, 0);
    device_window = CreateWindowA("d3d8_test_wndproc_wc", "d3d8_test",
            WS_MAXIMIZE | WS_VISIBLE | WS_CAPTION , 0, 0, 640, 480, 0, 0, 0, 0);
    thread = CreateThread(NULL, 0, wndproc_thread, &thread_params, 0, &tid);
    ok(!!thread, "Failed to create thread, last error %#x.\n", GetLastError());

    res = WaitForSingleObject(thread_params.window_created, INFINITE);
    ok(res == WAIT_OBJECT_0, "Wait failed (%#x), last error %#x.\n", res, GetLastError());

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);
    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    trace("device_window %p, focus_window %p, dummy_window %p.\n",
            device_window, focus_window, thread_params.dummy_window);

    tmp = GetFocus();
    ok(tmp == device_window, "Expected focus %p, got %p.\n", device_window, tmp);
    tmp = GetForegroundWindow();
    ok(tmp == thread_params.dummy_window, "Expected foreground window %p, got %p.\n",
            thread_params.dummy_window, tmp);

    flush_events();

    expect_messages = messages;

    device = create_device(d3d8, device_window, focus_window, FALSE);
    if (!device)
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    ok(!expect_messages->message, "Expected message %#x for window %#x, but didn't receive it.\n",
            expect_messages->message, expect_messages->window);
    expect_messages = NULL;

    if (0) /* Disabled until we can make this work in a reliable way on Wine. */
    {
        tmp = GetFocus();
        ok(tmp == focus_window, "Expected focus %p, got %p.\n", focus_window, tmp);
        tmp = GetForegroundWindow();
        ok(tmp == focus_window, "Expected foreground window %p, got %p.\n", focus_window, tmp);
    }
    SetForegroundWindow(focus_window);
    flush_events();

    filter_messages = focus_window;

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc != (LONG_PTR)test_proc, "Expected wndproc != %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    ref = IDirect3DDevice8_Release(device);
    ok(ref == 0, "The device was not properly freed: refcount %u.\n", ref);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    device = create_device(d3d8, focus_window, focus_window, FALSE);
    if (!device)
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    ref = IDirect3DDevice8_Release(device);
    ok(ref == 0, "The device was not properly freed: refcount %u.\n", ref);

    device = create_device(d3d8, device_window, focus_window, FALSE);
    if (!device)
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    proc = SetWindowLongPtrA(focus_window, GWLP_WNDPROC, (LONG_PTR)DefWindowProcA);
    ok(proc != (LONG_PTR)test_proc, "Expected wndproc != %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    ref = IDirect3DDevice8_Release(device);
    ok(ref == 0, "The device was not properly freed: refcount %u.\n", ref);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)DefWindowProcA, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)DefWindowProcA, proc);

done:
    filter_messages = NULL;
    IDirect3D8_Release(d3d8);

    SetEvent(thread_params.test_finished);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread_params.test_finished);
    CloseHandle(thread_params.window_created);
    CloseHandle(thread);

    DestroyWindow(device_window);
    DestroyWindow(focus_window);
    UnregisterClassA("d3d8_test_wndproc_wc", GetModuleHandleA(NULL));
}

static void test_wndproc_windowed(void)
{
    struct wndproc_thread_param thread_params;
    IDirect3DDevice8 *device;
    WNDCLASSA wc = {0};
    IDirect3D8 *d3d8;
    HANDLE thread;
    LONG_PTR proc;
    HRESULT hr;
    ULONG ref;
    DWORD res, tid;
    HWND tmp;

    if (!(d3d8 = pDirect3DCreate8(D3D_SDK_VERSION)))
    {
        skip("Failed to create IDirect3D8 object, skipping tests.\n");
        return;
    }

    wc.lpfnWndProc = test_proc;
    wc.lpszClassName = "d3d8_test_wndproc_wc";
    ok(RegisterClassA(&wc), "Failed to register window class.\n");

    thread_params.window_created = CreateEvent(NULL, FALSE, FALSE, NULL);
    ok(!!thread_params.window_created, "CreateEvent failed, last error %#x.\n", GetLastError());
    thread_params.test_finished = CreateEvent(NULL, FALSE, FALSE, NULL);
    ok(!!thread_params.test_finished, "CreateEvent failed, last error %#x.\n", GetLastError());

    focus_window = CreateWindowA("d3d8_test_wndproc_wc", "d3d8_test",
            WS_MAXIMIZE | WS_VISIBLE | WS_CAPTION, 0, 0, 640, 480, 0, 0, 0, 0);
    device_window = CreateWindowA("d3d8_test_wndproc_wc", "d3d8_test",
            WS_MAXIMIZE | WS_VISIBLE | WS_CAPTION, 0, 0, 640, 480, 0, 0, 0, 0);
    thread = CreateThread(NULL, 0, wndproc_thread, &thread_params, 0, &tid);
    ok(!!thread, "Failed to create thread, last error %#x.\n", GetLastError());

    res = WaitForSingleObject(thread_params.window_created, INFINITE);
    ok(res == WAIT_OBJECT_0, "Wait failed (%#x), last error %#x.\n", res, GetLastError());

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);
    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    trace("device_window %p, focus_window %p, dummy_window %p.\n",
            device_window, focus_window, thread_params.dummy_window);

    tmp = GetFocus();
    ok(tmp == device_window, "Expected focus %p, got %p.\n", device_window, tmp);
    tmp = GetForegroundWindow();
    ok(tmp == thread_params.dummy_window, "Expected foreground window %p, got %p.\n",
            thread_params.dummy_window, tmp);

    filter_messages = focus_window;

    device = create_device(d3d8, device_window, focus_window, TRUE);
    if (!device)
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    tmp = GetFocus();
    ok(tmp == device_window, "Expected focus %p, got %p.\n", device_window, tmp);
    tmp = GetForegroundWindow();
    ok(tmp == thread_params.dummy_window, "Expected foreground window %p, got %p.\n",
            thread_params.dummy_window, tmp);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    filter_messages = NULL;

    hr = reset_device(device, device_window, FALSE);
    ok(SUCCEEDED(hr), "Failed to reset device, hr %#x.\n", hr);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc != (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    hr = reset_device(device, device_window, TRUE);
    ok(SUCCEEDED(hr), "Failed to reset device, hr %#x.\n", hr);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    filter_messages = focus_window;

    ref = IDirect3DDevice8_Release(device);
    ok(ref == 0, "The device was not properly freed: refcount %u.\n", ref);

    filter_messages = device_window;

    device = create_device(d3d8, focus_window, focus_window, TRUE);
    if (!device)
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    filter_messages = NULL;

    hr = reset_device(device, focus_window, FALSE);
    ok(SUCCEEDED(hr), "Failed to reset device, hr %#x.\n", hr);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc != (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    hr = reset_device(device, focus_window, TRUE);
    ok(SUCCEEDED(hr), "Failed to reset device, hr %#x.\n", hr);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    filter_messages = device_window;

    ref = IDirect3DDevice8_Release(device);
    ok(ref == 0, "The device was not properly freed: refcount %u.\n", ref);

    device = create_device(d3d8, device_window, focus_window, TRUE);
    if (!device)
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    filter_messages = NULL;

    hr = reset_device(device, device_window, FALSE);
    ok(SUCCEEDED(hr), "Failed to reset device, hr %#x.\n", hr);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc != (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    hr = reset_device(device, device_window, TRUE);
    ok(SUCCEEDED(hr), "Failed to reset device, hr %#x.\n", hr);

    proc = GetWindowLongPtrA(device_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    proc = GetWindowLongPtrA(focus_window, GWLP_WNDPROC);
    ok(proc == (LONG_PTR)test_proc, "Expected wndproc %#lx, got %#lx.\n",
            (LONG_PTR)test_proc, proc);

    filter_messages = device_window;

    ref = IDirect3DDevice8_Release(device);
    ok(ref == 0, "The device was not properly freed: refcount %u.\n", ref);

done:
    filter_messages = NULL;
    IDirect3D8_Release(d3d8);

    SetEvent(thread_params.test_finished);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread_params.test_finished);
    CloseHandle(thread_params.window_created);
    CloseHandle(thread);

    DestroyWindow(device_window);
    DestroyWindow(focus_window);
    UnregisterClassA("d3d8_test_wndproc_wc", GetModuleHandleA(NULL));
}

static inline void set_fpu_cw(WORD cw)
{
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile ("fnclex");
    __asm__ volatile ("fldcw %0" : : "m" (cw));
#endif
}

static inline WORD get_fpu_cw(void)
{
    WORD cw = 0;
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
#endif
    return cw;
}

static void test_fpu_setup(void)
{
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    D3DPRESENT_PARAMETERS present_parameters;
    IDirect3DDevice8 *device;
    D3DDISPLAYMODE d3ddm;
    HWND window = NULL;
    IDirect3D8 *d3d8;
    HRESULT hr;
    WORD cw;

    d3d8 = pDirect3DCreate8(D3D_SDK_VERSION);
    ok(!!d3d8, "Failed to create a d3d8 object.\n");
    if (!d3d8) return;

    window = CreateWindowA("static", "d3d8_test", WS_CAPTION, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!window, "Failed to create a window.\n");
    if (!window) goto done;

    hr = IDirect3D8_GetAdapterDisplayMode(d3d8, D3DADAPTER_DEFAULT, &d3ddm);
    ok(SUCCEEDED(hr), "GetAdapterDisplayMode failed, hr %#x.\n", hr);

    memset(&present_parameters, 0, sizeof(present_parameters));
    present_parameters.Windowed = TRUE;
    present_parameters.hDeviceWindow = window;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.BackBufferFormat = d3ddm.Format;

    set_fpu_cw(0xf60);
    cw = get_fpu_cw();
    ok(cw == 0xf60, "cw is %#x, expected 0xf60.\n", cw);

    hr = IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device);
    if (FAILED(hr))
    {
        skip("Failed to create a device, hr %#x.\n", hr);
        set_fpu_cw(0x37f);
        goto done;
    }

    cw = get_fpu_cw();
    ok(cw == 0x7f, "cw is %#x, expected 0x7f.\n", cw);

    IDirect3DDevice8_Release(device);

    cw = get_fpu_cw();
    ok(cw == 0x7f, "cw is %#x, expected 0x7f.\n", cw);
    set_fpu_cw(0xf60);
    cw = get_fpu_cw();
    ok(cw == 0xf60, "cw is %#x, expected 0xf60.\n", cw);

    hr = IDirect3D8_CreateDevice(d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &present_parameters, &device);
    ok(SUCCEEDED(hr), "CreateDevice failed, hr %#x.\n", hr);

    cw = get_fpu_cw();
    ok(cw == 0xf60, "cw is %#x, expected 0xf60.\n", cw);
    set_fpu_cw(0x37f);

    IDirect3DDevice8_Release(device);

done:
    if (window) DestroyWindow(window);
    if (d3d8) IDirect3D8_Release(d3d8);
#endif
}

static void test_ApplyStateBlock(void)
{
    D3DPRESENT_PARAMETERS d3dpp;
    IDirect3DDevice8 *device = NULL;
    IDirect3D8 *d3d8;
    HWND hwnd;
    HRESULT hr;
    D3DDISPLAYMODE d3ddm;
    DWORD received, token;

    d3d8 = pDirect3DCreate8( D3D_SDK_VERSION );
    ok(d3d8 != NULL, "Failed to create IDirect3D8 object\n");
    hwnd = CreateWindow( "static", "d3d8_test", WS_OVERLAPPEDWINDOW, 100, 100, 160, 160, NULL, NULL, NULL, NULL );
    ok(hwnd != NULL, "Failed to create window\n");
    if (!d3d8 || !hwnd) goto cleanup;

    IDirect3D8_GetAdapterDisplayMode( d3d8, D3DADAPTER_DEFAULT, &d3ddm );
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed         = TRUE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth  = 800;
    d3dpp.BackBufferHeight  = 600;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    hr = IDirect3D8_CreateDevice( d3d8, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, &d3dpp, &device );
    ok(hr == D3D_OK || hr == D3DERR_NOTAVAILABLE || hr == D3DERR_INVALIDCALL,
       "IDirect3D8_CreateDevice failed with %#x\n", hr);
    if(!device)
    {
        skip("Failed to create a d3d device\n");
        goto cleanup;
    }

    IDirect3DDevice8_CreateStateBlock(device, D3DSBT_ALL, &token);
    ok(token !=0, "received null token\n");

    IDirect3DDevice8_BeginStateBlock(device);
    IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, TRUE);
    IDirect3DDevice8_EndStateBlock(device, &token);
    IDirect3DDevice8_SetRenderState(device, D3DRS_ZENABLE, FALSE);

    hr = IDirect3DDevice8_GetRenderState(device, D3DRS_ZENABLE, &received);
    ok(hr==D3D_OK, "Expected= D3D_OK, Got= %#x\n", hr);
    ok(received==FALSE, "Expected = TRUE, received FALSE\n");

    IDirect3DDevice8_ApplyStateBlock(device, 0);
    ok(hr == D3D_OK, "Expected= D3D_OK, Got= %#x\n", hr);
    hr = IDirect3DDevice8_GetRenderState(device, D3DRS_ZENABLE, &received);
    ok(hr==D3D_OK, "Expected= D3D_OK, Got= %#x\n", hr);
    ok(received==FALSE, "Expected = TRUE, received FALSE\n");

    IDirect3DDevice8_ApplyStateBlock(device, token);
    ok(hr == D3D_OK, "Expected= D3D_OK, Got= %#x\n", hr);
    hr = IDirect3DDevice8_GetRenderState(device, D3DRS_ZENABLE, &received);
    ok(hr==D3D_OK, "Expected= D3D_OK, Got= %#x\n", hr);
    ok(received==TRUE, "Expected = TRUE, received FALSE\n");

    cleanup:
    if(device) IDirect3DDevice8_Release(device);
    if(d3d8) IDirect3D8_Release(d3d8);
}

START_TEST(device)
{
    HMODULE d3d8_handle = LoadLibraryA( "d3d8.dll" );
    if (!d3d8_handle)
    {
        skip("Could not load d3d8.dll\n");
        return;
    }

    pDirect3DCreate8 = (void *)GetProcAddress( d3d8_handle, "Direct3DCreate8" );
    ok(pDirect3DCreate8 != NULL, "Failed to get address of Direct3DCreate8\n");
    if (pDirect3DCreate8)
    {
        IDirect3D8 *d3d8;
        d3d8 = pDirect3DCreate8( D3D_SDK_VERSION );
        if(!d3d8)
        {
            skip("could not create D3D8\n");
            return;
        }
        IDirect3D8_Release(d3d8);

        test_fpu_setup();
        test_display_modes();
        test_shader_versions();
        test_swapchain();
        test_refcount();
        test_mipmap_levels();
        test_cursor();
        test_states();
        test_scene();
        test_shader();
        test_limits();
        test_lights();
        test_ApplyStateBlock();
        test_render_zero_triangles();
        test_depth_stencil_reset();
        test_wndproc();
        test_wndproc_windowed();
    }
}

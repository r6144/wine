/*
 * Copyright 2010 Maarten Lankhorst for Codeweavers
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

/* This test is for audio capture specific mechanisms
 * Tests:
 * - IAudioClient with eCapture and IAudioCaptureClient
 */

#include "wine/test.h"

#define CINTERFACE
#define COBJMACROS

#ifdef STANDALONE
#include "initguid.h"
#endif

#include "unknwn.h"
#include "uuids.h"
#include "mmdeviceapi.h"
#include "audioclient.h"

static void test_uninitialized(IAudioClient *ac)
{
    HRESULT hr;
    UINT32 num;
    REFERENCE_TIME t1;

    HANDLE handle = CreateEventW(NULL, FALSE, FALSE, NULL);
    IUnknown *unk;

    hr = IAudioClient_GetBufferSize(ac, &num);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized GetBufferSize call returns %08x\n", hr);

    hr = IAudioClient_GetStreamLatency(ac, &t1);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized GetStreamLatency call returns %08x\n", hr);

    hr = IAudioClient_GetCurrentPadding(ac, &num);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized GetCurrentPadding call returns %08x\n", hr);

    hr = IAudioClient_Start(ac);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized Start call returns %08x\n", hr);

    hr = IAudioClient_Stop(ac);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized Stop call returns %08x\n", hr);

    hr = IAudioClient_Reset(ac);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized Reset call returns %08x\n", hr);

    hr = IAudioClient_SetEventHandle(ac, handle);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized SetEventHandle call returns %08x\n", hr);

    hr = IAudioClient_GetService(ac, &IID_IAudioStreamVolume, (void**)&unk);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Uninitialized GetService call returns %08x\n", hr);

    CloseHandle(handle);
}

static void test_capture(IAudioClient *ac, HANDLE handle, WAVEFORMATEX *wfx)
{
    IAudioCaptureClient *acc;
    HRESULT hr;
    UINT32 frames = 0;
    BYTE *data = NULL;
    DWORD flags;
    UINT64 devpos, qpcpos;

    hr = IAudioClient_GetService(ac, &IID_IAudioCaptureClient, (void**)&acc);
    ok(hr == S_OK, "IAudioClient_GetService(IID_IAudioCaptureClient) returns %08x\n", hr);
    if (hr != S_OK)
        return;

    hr = IAudioCaptureClient_GetNextPacketSize(acc, NULL);
    ok(hr == E_POINTER, "IAudioCaptureClient_GetNextPacketSize(NULL) returns %08x\n", hr);

    ok(WaitForSingleObject(handle, 1000) == WAIT_OBJECT_0, "Waiting on event handle failed!\n");

    /* frames can be 0 if no data is available yet.. */
    hr = IAudioCaptureClient_GetNextPacketSize(acc, &frames);
    ok(hr == S_OK, "IAudioCaptureClient_GetNextPacketSize returns %08x\n", hr);

    data = (BYTE*)(DWORD_PTR)0xdeadbeef;
    hr = IAudioCaptureClient_GetBuffer(acc, &data, NULL, NULL, NULL, NULL);
    ok(hr == E_POINTER, "IAudioCaptureClient_GetBuffer(data, NULL, NULL) returns %08x\n", hr);
    ok((DWORD_PTR)data == 0xdeadbeef, "data is reset to %p\n", data);

    frames = 0xdeadbeef;
    hr = IAudioCaptureClient_GetBuffer(acc, NULL, &frames, NULL, NULL, NULL);
    ok(hr == E_POINTER, "IAudioCaptureClient_GetBuffer(NULL, &frames, NULL) returns %08x\n", hr);
    ok(frames == 0xdeadbeef, "frames is reset to %08x\n", frames);

    flags = 0xdeadbeef;
    hr = IAudioCaptureClient_GetBuffer(acc, NULL, NULL, &flags, NULL, NULL);
    ok(hr == E_POINTER, "IAudioCaptureClient_GetBuffer(NULL, NULL, &flags) returns %08x\n", hr);
    ok(flags == 0xdeadbeef, "flags is reset to %08x\n", flags);

    hr = IAudioCaptureClient_GetBuffer(acc, &data, &frames, NULL, NULL, NULL);
    ok(hr == E_POINTER, "IAudioCaptureClient_GetBuffer(&ata, &frames, NULL) returns %08x\n", hr);

    hr = IAudioCaptureClient_GetBuffer(acc, &data, &frames, &flags, &devpos, &qpcpos);
    ok(hr == S_OK || hr == AUDCLNT_S_BUFFER_EMPTY, "Valid IAudioCaptureClient_GetBuffer returns %08x\n", hr);
    if (hr == S_OK)
        ok(frames, "Amount of frames locked is 0!\n");
    else if (hr == AUDCLNT_S_BUFFER_EMPTY)
        ok(!frames, "Amount of frames locked with empty buffer is %u!\n", frames);
    trace("Device position is at %u, amount of frames locked: %u\n", (DWORD)devpos, frames);

    if (frames) {
        hr = IAudioCaptureClient_GetBuffer(acc, &data, &frames, &flags, &devpos, &qpcpos);
        ok(hr == AUDCLNT_E_OUT_OF_ORDER, "Out of order IAudioCaptureClient_GetBuffer returns %08x\n", hr);
    }

    hr = IAudioCaptureClient_ReleaseBuffer(acc, frames);
    ok(hr == S_OK, "Releasing buffer returns %08x\n", hr);

    if (frames) {
        hr = IAudioCaptureClient_ReleaseBuffer(acc, frames);
        ok(hr == AUDCLNT_E_OUT_OF_ORDER, "Releasing buffer twice returns %08x\n", hr);
    }

    IUnknown_Release(acc);
}

static void test_audioclient(IAudioClient *ac)
{
    IUnknown *unk;
    HRESULT hr;
    ULONG ref;
    WAVEFORMATEX *pwfx, *pwfx2;
    REFERENCE_TIME t1, t2;

    HANDLE handle = CreateEventW(NULL, FALSE, FALSE, NULL);

    hr = IAudioClient_QueryInterface(ac, &IID_IUnknown, NULL);
    ok(hr == E_POINTER, "QueryInterface(NULL) returned %08x\n", hr);

    unk = (void*)(LONG_PTR)0x12345678;
    hr = IAudioClient_QueryInterface(ac, &IID_NULL, (void**)&unk);
    ok(hr == E_NOINTERFACE, "QueryInterface(IID_NULL) returned %08x\n", hr);
    ok(!unk, "QueryInterface(IID_NULL) returned non-null pointer %p\n", unk);

    hr = IAudioClient_QueryInterface(ac, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface(IID_IUnknown) returned %08x\n", hr);
    if (unk)
    {
        ref = IUnknown_Release(unk);
        ok(ref == 1, "Released count is %u\n", ref);
    }

    hr = IAudioClient_QueryInterface(ac, &IID_IAudioClient, (void**)&unk);
    ok(hr == S_OK, "QueryInterface(IID_IAudioClient) returned %08x\n", hr);
    if (unk)
    {
        ref = IUnknown_Release(unk);
        ok(ref == 1, "Released count is %u\n", ref);
    }

    hr = IAudioClient_GetDevicePeriod(ac, NULL, NULL);
    ok(hr == E_POINTER, "Invalid GetDevicePeriod call returns %08x\n", hr);

    hr = IAudioClient_GetDevicePeriod(ac, &t1, NULL);
    ok(hr == S_OK, "Valid GetDevicePeriod call returns %08x\n", hr);

    hr = IAudioClient_GetDevicePeriod(ac, NULL, &t2);
    ok(hr == S_OK, "Valid GetDevicePeriod call returns %08x\n", hr);

    hr = IAudioClient_GetDevicePeriod(ac, &t1, &t2);
    ok(hr == S_OK, "Valid GetDevicePeriod call returns %08x\n", hr);
    trace("Returned periods: %u.%05u ms %u.%05u ms\n",
          (UINT)(t1/10000), (UINT)(t1 % 10000),
          (UINT)(t2/10000), (UINT)(t2 % 10000));

    hr = IAudioClient_GetMixFormat(ac, NULL);
    ok(hr == E_POINTER, "GetMixFormat returns %08x\n", hr);

    hr = IAudioClient_GetMixFormat(ac, &pwfx);
    ok(hr == S_OK, "Valid GetMixFormat returns %08x\n", hr);

    trace("Tag: %04x\n", pwfx->wFormatTag);
    trace("bits: %u\n", pwfx->wBitsPerSample);
    trace("chan: %u\n", pwfx->nChannels);
    trace("rate: %u\n", pwfx->nSamplesPerSec);
    trace("align: %u\n", pwfx->nBlockAlign);
    trace("extra: %u\n", pwfx->cbSize);
    ok(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE, "wFormatTag is %x\n", pwfx->wFormatTag);
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        WAVEFORMATEXTENSIBLE *pwfxe = (void*)pwfx;
        trace("Res: %u\n", pwfxe->Samples.wReserved);
        trace("Mask: %x\n", pwfxe->dwChannelMask);
        trace("Alg: %s\n",
              IsEqualGUID(&pwfxe->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)?"PCM":
              (IsEqualGUID(&pwfxe->SubFormat,
                           &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)?"FLOAT":"Other"));
    }

    if (hr == S_OK)
    {
        hr = IAudioClient_IsFormatSupported(ac, AUDCLNT_SHAREMODE_SHARED, pwfx, &pwfx2);
        ok(hr == S_OK, "Valid IsFormatSupported(Shared) call returns %08x\n", hr);
        ok(pwfx2 == NULL, "pwfx2 is non-null\n");
        CoTaskMemFree(pwfx2);

        hr = IAudioClient_IsFormatSupported(ac, AUDCLNT_SHAREMODE_SHARED, NULL, NULL);
        ok(hr == E_POINTER, "IsFormatSupported(NULL) call returns %08x\n", hr);

        hr = IAudioClient_IsFormatSupported(ac, AUDCLNT_SHAREMODE_SHARED, pwfx, NULL);
        ok(hr == E_POINTER, "IsFormatSupported(Shared,NULL) call returns %08x\n", hr);

        hr = IAudioClient_IsFormatSupported(ac, AUDCLNT_SHAREMODE_EXCLUSIVE, pwfx, NULL);
        ok(hr == S_OK || hr == AUDCLNT_E_UNSUPPORTED_FORMAT, "IsFormatSupported(Exclusive) call returns %08x\n", hr);

        hr = IAudioClient_IsFormatSupported(ac, AUDCLNT_SHAREMODE_EXCLUSIVE, pwfx, &pwfx2);
        ok(hr == S_OK || hr == AUDCLNT_E_UNSUPPORTED_FORMAT, "IsFormatSupported(Exclusive) call returns %08x\n", hr);
        ok(pwfx2 == NULL, "pwfx2 non-null on exclusive IsFormatSupported\n");

        hr = IAudioClient_IsFormatSupported(ac, 0xffffffff, pwfx, NULL);
        ok(hr == E_INVALIDARG, "IsFormatSupported(0xffffffff) call returns %08x\n", hr);
    }

    test_uninitialized(ac);

    hr = IAudioClient_Initialize(ac, 3, 0, 5000000, 0, pwfx, NULL);
    ok(hr == AUDCLNT_E_NOT_INITIALIZED, "Initialize with invalid sharemode returns %08x\n", hr);

    hr = IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, 0xffffffff, 5000000, 0, pwfx, NULL);
    ok(hr == E_INVALIDARG, "Initialize with invalid flags returns %08x\n", hr);

    /* It seems that if length > 2s or periodicity != 0 the length is ignored and call succeeds
     * Since we can only initialize succesfully once skip those tests
     */
    hr = IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, 0, 5000000, 0, NULL, NULL);
    ok(hr == E_POINTER, "Initialize with null format returns %08x\n", hr);

    hr = IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 5000000, 0, pwfx, NULL);
    ok(hr == S_OK, "Valid Initialize returns %08x\n", hr);

    if (hr != S_OK)
    {
        skip("Cannot initialize %08x, remainder of tests is useless\n", hr);
        CoTaskMemFree(pwfx);
        return;
    }

    hr = IAudioClient_GetStreamLatency(ac, NULL);
    ok(hr == E_POINTER, "GetStreamLatency(NULL) call returns %08x\n", hr);

    hr = IAudioClient_GetStreamLatency(ac, &t1);
    ok(hr == S_OK, "Valid GetStreamLatency call returns %08x\n", hr);
    trace("Returned latency: %u.%05u ms\n",
          (UINT)(t1/10000), (UINT)(t1 % 10000));

    hr = IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, 0, 5000000, 0, pwfx, NULL);
    ok(hr == AUDCLNT_E_ALREADY_INITIALIZED, "Calling Initialize twice returns %08x\n", hr);

    hr = IAudioClient_SetEventHandle(ac, NULL);
    ok(hr == E_INVALIDARG, "SetEventHandle(NULL) returns %08x\n", hr);

    hr = IAudioClient_Start(ac);
    ok(hr == AUDCLNT_E_EVENTHANDLE_NOT_SET, "Start before SetEventHandle returns %08x\n", hr);

    hr = IAudioClient_SetEventHandle(ac, handle);
    ok(hr == S_OK, "SetEventHandle returns %08x\n", hr);

    hr = IAudioClient_Reset(ac);
    ok(hr == S_OK, "Reset on a resetted stream returns %08x\n", hr);

    hr = IAudioClient_Stop(ac);
    ok(hr == S_FALSE, "Stop on a stopped stream returns %08x\n", hr);

    hr = IAudioClient_Start(ac);
    ok(hr == S_OK, "Start on a stopped stream returns %08x\n", hr);

    test_capture(ac, handle, pwfx);

    CloseHandle(handle);
    CoTaskMemFree(pwfx);
}

START_TEST(capture)
{
    HRESULT hr;
    IMMDeviceEnumerator *mme = NULL;
    IMMDevice *dev = NULL;
    IAudioClient *ac = NULL;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void**)&mme);
    if (FAILED(hr))
    {
        skip("mmdevapi not available: 0x%08x\n", hr);
        goto cleanup;
    }

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(mme, eCapture, eMultimedia, &dev);
    ok(hr == S_OK || hr == E_NOTFOUND, "GetDefaultAudioEndpoint failed: 0x%08x\n", hr);
    if (hr != S_OK || !dev)
    {
        if (hr == E_NOTFOUND)
            skip("No sound card available\n");
        else
            skip("GetDefaultAudioEndpoint returns 0x%08x\n", hr);
        goto cleanup;
    }

    hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void**)&ac);
    ok(hr == S_OK, "Activation failed with %08x\n", hr);
    if (ac)
    {
        test_audioclient(ac);
        IAudioClient_Release(ac);
    }
    IMMDevice_Release(dev);

cleanup:
    if (mme)
        IMMDeviceEnumerator_Release(mme);
    CoUninitialize();
}

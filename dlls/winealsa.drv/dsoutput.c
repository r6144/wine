/*
 * Sample Wine Driver for Advanced Linux Sound System (ALSA)
 *      Based on version <final> of the ALSA API
 *
 * Copyright    2002 Eric Pouech
 *              2002 Marco Pietrobono
 *              2003 Christian Costa : WaveIn support
 *              2006-2007 Maarten Lankhorst
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

/*======================================================================*
 *              Low level dsound output implementation			*
 *======================================================================*/

#include "config.h"
#include "wine/port.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winerror.h"
#include "winuser.h"
#include "mmddk.h"

#include "alsa.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#ifdef HAVE_ALSA

WINE_DEFAULT_DEBUG_CHANNEL(dsalsa);

typedef struct IDsDriverImpl IDsDriverImpl;
typedef struct IDsDriverBufferImpl IDsDriverBufferImpl;

struct IDsDriverImpl
{
    /* IUnknown fields */
    const IDsDriverVtbl *lpVtbl;
    LONG ref;

    /* IDsDriverImpl fields */
    IDsDriverBufferImpl* primary;
    UINT wDevID;
};

struct IDsDriverBufferImpl
{
    const IDsDriverBufferVtbl *lpVtbl;
    LONG ref;
    IDsDriverImpl* drv;

    CRITICAL_SECTION pcm_crst;
    BYTE *mmap_buffer;
    DWORD mmap_buflen_bytes;
    BOOL mmap;

    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_uframes_t mmap_buflen_frames, mmap_pos, mmap_commitahead;
};

/** Fill buffers, for starting and stopping
 * Alsa won't start playing until everything is filled up
 * This also updates mmap_pos
 *
 * Returns: Amount of periods in use so snd_pcm_avail_update
 * doesn't have to be called up to 4x in GetPosition()
 */
static snd_pcm_uframes_t CommitAll(IDsDriverBufferImpl *This)
{
    const snd_pcm_channel_area_t *areas;
    snd_pcm_sframes_t used;
    const snd_pcm_uframes_t commitahead = This->mmap_commitahead;

    used = This->mmap_buflen_frames - snd_pcm_avail_update(This->pcm);
    if (used < 0) used = 0;
    TRACE("%p needs to commit to %lu, used: %ld\n", This, commitahead, used);
    if (used < commitahead)
    {
        snd_pcm_sframes_t done;
        snd_pcm_uframes_t putin = commitahead - used;
        if (This->mmap)
        {
            snd_pcm_mmap_begin(This->pcm, &areas, &This->mmap_pos, &putin);
            done = snd_pcm_mmap_commit(This->pcm, This->mmap_pos, putin);
        }
        else
        {
            if (putin + This->mmap_pos > This->mmap_buflen_frames)
                putin = This->mmap_buflen_frames - This->mmap_pos;
            done = snd_pcm_writei(This->pcm, This->mmap_buffer + snd_pcm_frames_to_bytes(This->pcm, This->mmap_pos), putin);
            if (done < putin) WARN("Short write %ld/%ld\n", putin, done);
        }
        if (done < 0) done = 0;
        This->mmap_pos += done;
        used += done;
        putin = commitahead - used;

        if (This->mmap_pos == This->mmap_buflen_frames && (snd_pcm_sframes_t)putin > 0)
        {
            if (This->mmap)
            {
                snd_pcm_mmap_begin(This->pcm, &areas, &This->mmap_pos, &putin);
                done = snd_pcm_mmap_commit(This->pcm, This->mmap_pos, putin);
                This->mmap_pos += done;
            }
            else
            {
                done = snd_pcm_writei(This->pcm, This->mmap_buffer, putin);
                if (done < putin) WARN("Short write %ld/%ld\n", putin, done);
                if (done < 0) done = 0;
                This->mmap_pos = done;
            }
            used += done;
        }
    }

    if (This->mmap_pos == This->mmap_buflen_frames)
        This->mmap_pos = 0;

    return used;
}

static void CheckXRUN(IDsDriverBufferImpl* This)
{
    snd_pcm_state_t state = snd_pcm_state(This->pcm);
    snd_pcm_sframes_t delay;
    int err;

    snd_pcm_hwsync(This->pcm);
    snd_pcm_delay(This->pcm, &delay);
    if ( state == SND_PCM_STATE_XRUN )
    {
        err = snd_pcm_prepare(This->pcm);
        CommitAll(This);
        snd_pcm_start(This->pcm);
        WARN("xrun occurred\n");
        if ( err < 0 )
            ERR("recovery from xrun failed, prepare failed: %s\n", snd_strerror(err));
    }
    else if ( state == SND_PCM_STATE_SUSPENDED )
    {
        int err = snd_pcm_resume(This->pcm);
        TRACE("recovery from suspension occurred\n");
        if (err < 0 && err != -EAGAIN){
            err = snd_pcm_prepare(This->pcm);
            if (err < 0)
                ERR("recovery from suspend failed, prepare failed: %s\n", snd_strerror(err));
        }
    } else if ( state != SND_PCM_STATE_RUNNING ) {
        FIXME("Unhandled state: %d\n", state);
    }
}

/**
 * Allocate the memory-mapped buffer for direct sound, and set up the
 * callback.
 */
static int DSDB_CreateMMAP(IDsDriverBufferImpl* pdbi)
{
    snd_pcm_t *pcm = pdbi->pcm;
    snd_pcm_format_t format;
    snd_pcm_uframes_t frames, ofs, avail, psize, boundary;
    unsigned int channels, bits_per_sample, bits_per_frame;
    int err, mmap_mode;
    const snd_pcm_channel_area_t *areas;
    snd_pcm_hw_params_t *hw_params = pdbi->hw_params;
    snd_pcm_sw_params_t *sw_params = pdbi->sw_params;
    void *buf;

    mmap_mode = snd_pcm_type(pcm);

    if (mmap_mode == SND_PCM_TYPE_HW)
        TRACE("mmap'd buffer is a direct hardware buffer.\n");
    else if (mmap_mode == SND_PCM_TYPE_DMIX)
        TRACE("mmap'd buffer is an ALSA dmix buffer\n");
    else
        TRACE("mmap'd buffer is an ALSA type %d buffer\n", mmap_mode);

    err = snd_pcm_hw_params_get_period_size(hw_params, &psize, NULL);

    err = snd_pcm_hw_params_get_format(hw_params, &format);
    err = snd_pcm_hw_params_get_buffer_size(hw_params, &frames);
    err = snd_pcm_hw_params_get_channels(hw_params, &channels);
    bits_per_sample = snd_pcm_format_physical_width(format);
    bits_per_frame = bits_per_sample * channels;

    if (TRACE_ON(dsalsa))
        ALSA_TraceParameters(hw_params, NULL, FALSE);

    TRACE("format=%s  frames=%ld  channels=%d  bits_per_sample=%d  bits_per_frame=%d\n",
          snd_pcm_format_name(format), frames, channels, bits_per_sample, bits_per_frame);

    pdbi->mmap_buflen_frames = frames;
    pdbi->mmap_buflen_bytes = snd_pcm_frames_to_bytes( pcm, frames );

    snd_pcm_sw_params_current(pcm, sw_params);
    snd_pcm_sw_params_set_start_threshold(pcm, sw_params, 0);
    snd_pcm_sw_params_get_boundary(sw_params, &boundary);
    snd_pcm_sw_params_set_stop_threshold(pcm, sw_params, boundary);
    snd_pcm_sw_params_set_silence_threshold(pcm, sw_params, boundary);
    snd_pcm_sw_params_set_silence_size(pcm, sw_params, 0);
    snd_pcm_sw_params_set_avail_min(pcm, sw_params, 0);
    err = snd_pcm_sw_params(pcm, sw_params);

    avail = snd_pcm_avail_update(pcm);
    if ((snd_pcm_sframes_t)avail < 0)
    {
        ERR("No buffer is available: %s.\n", snd_strerror(avail));
        return DSERR_GENERIC;
    }

    if (!pdbi->mmap)
    {
        buf = pdbi->mmap_buffer = HeapAlloc(GetProcessHeap(), 0, pdbi->mmap_buflen_bytes);
        if (!buf)
            return DSERR_OUTOFMEMORY;

        snd_pcm_format_set_silence(format, buf, pdbi->mmap_buflen_frames);
        pdbi->mmap_pos = 0;
    }
    else
    {
        err = snd_pcm_mmap_begin(pcm, &areas, &ofs, &avail);
        if ( err < 0 )
        {
            ERR("Can't map sound device for direct access: %s/%d\n", snd_strerror(err), err);
            return DSERR_GENERIC;
        }
        snd_pcm_format_set_silence(format, areas->addr, pdbi->mmap_buflen_frames);
        pdbi->mmap_pos = ofs + snd_pcm_mmap_commit(pcm, ofs, 0);
        pdbi->mmap_buffer = areas->addr;
    }

    TRACE("created mmap buffer of %ld frames (%d bytes) at %p\n",
        frames, pdbi->mmap_buflen_bytes, pdbi->mmap_buffer);

    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_QueryInterface(PIDSDRIVERBUFFER iface, REFIID riid, LPVOID *ppobj)
{
    /* IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface; */
    FIXME("(): stub!\n");
    return DSERR_UNSUPPORTED;
}

static ULONG WINAPI IDsDriverBufferImpl_AddRef(PIDSDRIVERBUFFER iface)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(ref before=%u)\n",This, refCount - 1);

    return refCount;
}

static ULONG WINAPI IDsDriverBufferImpl_Release(PIDSDRIVERBUFFER iface)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(ref before=%u)\n",This, refCount + 1);

    if (refCount)
        return refCount;

    TRACE("mmap buffer %p destroyed\n", This->mmap_buffer);

    if (This == This->drv->primary)
        This->drv->primary = NULL;

    This->pcm_crst.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection(&This->pcm_crst);

    snd_pcm_drop(This->pcm);
    snd_pcm_close(This->pcm);
    This->pcm = NULL;
    HeapFree(GetProcessHeap(), 0, This->sw_params);
    HeapFree(GetProcessHeap(), 0, This->hw_params);
    if (!This->mmap)
        HeapFree(GetProcessHeap(), 0, This->mmap_buffer);
    HeapFree(GetProcessHeap(), 0, This);
    return 0;
}

static HRESULT WINAPI IDsDriverBufferImpl_Lock(PIDSDRIVERBUFFER iface,
					       LPVOID*ppvAudio1,LPDWORD pdwLen1,
					       LPVOID*ppvAudio2,LPDWORD pdwLen2,
					       DWORD dwWritePosition,DWORD dwWriteLen,
					       DWORD dwFlags)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    snd_pcm_uframes_t writepos;

    TRACE("%d bytes from %d\n", dwWriteLen, dwWritePosition);

    /* **** */
    EnterCriticalSection(&This->pcm_crst);

    if (dwFlags & DSBLOCK_ENTIREBUFFER)
        dwWriteLen = This->mmap_buflen_bytes;

    if (dwWriteLen > This->mmap_buflen_bytes || dwWritePosition >= This->mmap_buflen_bytes)
    {
        /* **** */
        LeaveCriticalSection(&This->pcm_crst);
        return DSERR_INVALIDPARAM;
    }

    if (ppvAudio2) *ppvAudio2 = NULL;
    if (pdwLen2) *pdwLen2 = 0;

    *ppvAudio1 = This->mmap_buffer + dwWritePosition;
    *pdwLen1 = dwWriteLen;

    if (dwWritePosition+dwWriteLen > This->mmap_buflen_bytes)
    {
        DWORD remainder = This->mmap_buflen_bytes - dwWritePosition;
        *pdwLen1 = remainder;

        if (ppvAudio2 && pdwLen2)
        {
            *ppvAudio2 = This->mmap_buffer;
            *pdwLen2 = dwWriteLen - remainder;
        }
        else dwWriteLen = remainder;
    }

    writepos = snd_pcm_bytes_to_frames(This->pcm, dwWritePosition);
    if (writepos == This->mmap_pos)
    {
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t writelen = snd_pcm_bytes_to_frames(This->pcm, dwWriteLen), putin = writelen;
        TRACE("Hit mmap_pos, locking data!\n");
        if (This->mmap)
            snd_pcm_mmap_begin(This->pcm, &areas, &This->mmap_pos, &putin);
    }
    else
        WARN("mmap_pos (%lu) != writepos (%lu) not locking data!\n", This->mmap_pos, writepos);

    LeaveCriticalSection(&This->pcm_crst);
    /* **** */
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Unlock(PIDSDRIVERBUFFER iface,
						 LPVOID pvAudio1,DWORD dwLen1,
						 LPVOID pvAudio2,DWORD dwLen2)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    snd_pcm_uframes_t writepos;

    if (!dwLen1)
        return DS_OK;

    /* **** */
    EnterCriticalSection(&This->pcm_crst);

    writepos = snd_pcm_bytes_to_frames(This->pcm, (DWORD_PTR)pvAudio1 - (DWORD_PTR)This->mmap_buffer);
    if (writepos == This->mmap_pos)
    {
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t writelen = snd_pcm_bytes_to_frames(This->pcm, dwLen1);
        TRACE("Committing data\n");
        if (This->mmap)
            This->mmap_pos += snd_pcm_mmap_commit(This->pcm, This->mmap_pos, writelen);
        else
        {
            int ret;
            ret = snd_pcm_writei(This->pcm, pvAudio1, writelen);
            if (ret == -EPIPE)
            {
                WARN("Underrun occurred\n");
                wine_snd_pcm_recover(This->pcm, -EPIPE, 1);
                ret = snd_pcm_writei(This->pcm, pvAudio1, writelen);

                /* Advance mmap pointer a little to make dsound notice the underrun and respond to it */
                if (ret < writelen) WARN("Short write %ld/%d\n", writelen, ret);
                This->mmap_pos += This->mmap_commitahead + ret;
                This->mmap_pos %= This->mmap_buflen_frames;
            }
            else if (ret > 0)
                This->mmap_pos += ret;
            if (ret < 0)
                WARN("Committing data: %d / %s (%p %ld)\n", ret, snd_strerror(ret), pvAudio1, writelen);
        }

        if (This->mmap_pos == This->mmap_buflen_frames)
            This->mmap_pos = 0;
        if (dwLen2)
        {
            writelen = snd_pcm_bytes_to_frames(This->pcm, dwLen2);
            if (This->mmap)
            {
                snd_pcm_mmap_begin(This->pcm, &areas, &This->mmap_pos, &writelen);
                This->mmap_pos += snd_pcm_mmap_commit(This->pcm, This->mmap_pos, writelen);
            }
            else
            {
                int ret;
                ret = snd_pcm_writei(This->pcm, pvAudio2, writelen);
                if (ret < writelen) WARN("Short write %ld/%d\n", writelen, ret);
                This->mmap_pos = ret > 0 ? ret : 0;
            }
            assert(This->mmap_pos < This->mmap_buflen_frames);
        }
    }
    LeaveCriticalSection(&This->pcm_crst);
    /* **** */

    return DS_OK;
}

static HRESULT SetFormat(IDsDriverBufferImpl *This, LPWAVEFORMATEX pwfx)
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *hw_params = This->hw_params;
    unsigned int buffer_time = 500000;
    snd_pcm_format_t format = -1;
    snd_pcm_uframes_t psize;
    DWORD rate = pwfx->nSamplesPerSec;
    int err=0;

    switch (pwfx->wBitsPerSample)
    {
        case  8: format = SND_PCM_FORMAT_U8; break;
        case 16: format = SND_PCM_FORMAT_S16_LE; break;
        case 24: format = SND_PCM_FORMAT_S24_3LE; break;
        case 32: format = SND_PCM_FORMAT_S32_LE; break;
        default: FIXME("Unsupported bpp: %d\n", pwfx->wBitsPerSample); return DSERR_GENERIC;
    }

    err = snd_pcm_open(&pcm, WOutDev[This->drv->wDevID].pcmname, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0)
    {
        if (errno != EBUSY || !This->pcm)
        {
            WARN("Cannot open sound device: %s\n", snd_strerror(err));
            return DSERR_GENERIC;
        }
        snd_pcm_drop(This->pcm);
        snd_pcm_close(This->pcm);
        This->pcm = NULL;
        err = snd_pcm_open(&pcm, WOutDev[This->drv->wDevID].pcmname, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (err < 0)
        {
            WARN("Cannot open sound device: %s\n", snd_strerror(err));
            return DSERR_BUFFERLOST;
        }
    }

    /* Set some defaults */
    snd_pcm_hw_params_any(pcm, hw_params);
    err = snd_pcm_hw_params_set_channels(pcm, hw_params, pwfx->nChannels);
    if (err < 0) { WARN("Could not set channels to %d\n", pwfx->nChannels); goto err; }

    err = snd_pcm_hw_params_set_format(pcm, hw_params, format);
    if (err < 0) { WARN("Could not set format to %d bpp\n", pwfx->wBitsPerSample); goto err; }

    /* Alsa's rate resampling is only used if the application specifically requests
     * a buffer at a certain frequency, else it is better to disable it due to unwanted
     * side effects, which may include: Less granular pointer, changing buffer sizes, etc
     */
#if SND_LIB_VERSION >= 0x010009
    snd_pcm_hw_params_set_rate_resample(pcm, hw_params, 0);
#endif

    err = snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, NULL);
    if (err < 0) { rate = pwfx->nSamplesPerSec; WARN("Could not set rate\n"); goto err; }

    if (!ALSA_NearMatch(rate, pwfx->nSamplesPerSec))
    {
        WARN("Could not set sound rate to %d, but instead to %d\n", pwfx->nSamplesPerSec, rate);
        pwfx->nSamplesPerSec = rate;
        pwfx->nAvgBytesPerSec = rate * pwfx->nBlockAlign;
        /* Let DirectSound detect this */
    }

    snd_pcm_hw_params_set_periods_integer(pcm, hw_params);
    snd_pcm_hw_params_set_buffer_time_near(pcm, hw_params, &buffer_time, NULL);
    buffer_time = 10000;
    snd_pcm_hw_params_set_period_time_near(pcm, hw_params, &buffer_time, NULL);

    err = snd_pcm_hw_params_get_period_size(hw_params, &psize, NULL);
    buffer_time = 16;
    snd_pcm_hw_params_set_periods_near(pcm, hw_params, &buffer_time, NULL);

    if (!This->mmap)
    {
        HeapFree(GetProcessHeap(), 0, This->mmap_buffer);
        This->mmap_buffer = NULL;
    }

    err = snd_pcm_hw_params_set_access (pcm, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (err >= 0)
        This->mmap = 1;
    else
    {
        This->mmap = 0;
        err = snd_pcm_hw_params_set_access (pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    }

    err = snd_pcm_hw_params(pcm, hw_params);

    /* ALSA needs at least 3 buffers to work successfully */
    This->mmap_commitahead = 3 * psize;
    while (This->mmap_commitahead <= 512)
        This->mmap_commitahead += psize;

    if (This->pcm)
    {
        snd_pcm_drop(This->pcm);
        snd_pcm_close(This->pcm);
    }
    This->pcm = pcm;
    snd_pcm_prepare(This->pcm);
    DSDB_CreateMMAP(This);
    return S_OK;

    err:
    if (err < 0)
        WARN("Failed to apply changes: %s\n", snd_strerror(err));

    if (!This->pcm)
        This->pcm = pcm;
    else
        snd_pcm_close(pcm);

    if (This->pcm)
        snd_pcm_hw_params_current(This->pcm, This->hw_params);

    return DSERR_BADFORMAT;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetFormat(PIDSDRIVERBUFFER iface, LPWAVEFORMATEX pwfx)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    HRESULT hr = S_OK;

    TRACE("(%p, %p)\n", iface, pwfx);

    /* **** */
    EnterCriticalSection(&This->pcm_crst);
    hr = SetFormat(This, pwfx);
    /* **** */
    LeaveCriticalSection(&This->pcm_crst);

    if (hr == DS_OK)
        return S_FALSE;
    return hr;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetFrequency(PIDSDRIVERBUFFER iface, DWORD dwFreq)
{
    /* IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface; */
    FIXME("(%p,%d): stub\n",iface,dwFreq);
    return S_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetVolumePan(PIDSDRIVERBUFFER iface, PDSVOLUMEPAN pVolPan)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    FIXME("(%p,%p): stub\n",This,pVolPan);
    /* TODO: Bring volume control back */
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_SetPosition(PIDSDRIVERBUFFER iface, DWORD dwNewPos)
{
    /* IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface; */
    /* I don't even think alsa allows this */
    FIXME("(%p,%d): stub\n",iface,dwNewPos);
    return DSERR_UNSUPPORTED;
}

static HRESULT WINAPI IDsDriverBufferImpl_GetPosition(PIDSDRIVERBUFFER iface,
						      LPDWORD lpdwPlay, LPDWORD lpdwWrite)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    snd_pcm_uframes_t hw_pptr, hw_wptr;
    snd_pcm_state_t state;

    /* **** */
    EnterCriticalSection(&This->pcm_crst);

    if (!This->pcm)
    {
        FIXME("Bad pointer for pcm: %p\n", This->pcm);
        LeaveCriticalSection(&This->pcm_crst);
        return DSERR_GENERIC;
    }

    if (!lpdwPlay && !lpdwWrite)
        CommitAll(This);

    state = snd_pcm_state(This->pcm);

    if (state != SND_PCM_STATE_PREPARED && state != SND_PCM_STATE_RUNNING)
    {
        CheckXRUN(This);
        state = snd_pcm_state(This->pcm);
    }
    if (state == SND_PCM_STATE_RUNNING)
    {
        snd_pcm_sframes_t used = This->mmap_buflen_frames - snd_pcm_avail_update(This->pcm);

        if (used < 0)
        {
            WARN("Underrun: %ld / %ld\n", used, snd_pcm_avail_update(This->pcm));
            if (This->mmap)
            {
                snd_pcm_forward(This->pcm, -used);
                This->mmap_pos += -used;
                This->mmap_pos %= This->mmap_buflen_frames;
            }
            used = 0;
        }

        if (This->mmap_pos > used)
            hw_pptr = This->mmap_pos - used;
        else
            hw_pptr = This->mmap_buflen_frames + This->mmap_pos - used;
        hw_pptr %= This->mmap_buflen_frames;

        TRACE("At position: %ld (%ld) - Used %ld\n", hw_pptr, This->mmap_pos, used);
    }
    else hw_pptr = This->mmap_pos;
    hw_wptr = This->mmap_pos;

    LeaveCriticalSection(&This->pcm_crst);
    /* **** */

    if (lpdwPlay)
        *lpdwPlay = snd_pcm_frames_to_bytes(This->pcm, hw_pptr);
    if (lpdwWrite)
        *lpdwWrite = snd_pcm_frames_to_bytes(This->pcm, hw_wptr);

    TRACE("hw_pptr=0x%08x, hw_wptr=0x%08x playpos=%d, writepos=%d\n", (unsigned int)hw_pptr, (unsigned int)hw_wptr, lpdwPlay?*lpdwPlay:-1, lpdwWrite?*lpdwWrite:-1);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Play(PIDSDRIVERBUFFER iface, DWORD dwRes1, DWORD dwRes2, DWORD dwFlags)
{
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    TRACE("(%p,%x,%x,%x)\n",iface,dwRes1,dwRes2,dwFlags);

    /* **** */
    EnterCriticalSection(&This->pcm_crst);
    snd_pcm_start(This->pcm);
    /* **** */
    LeaveCriticalSection(&This->pcm_crst);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverBufferImpl_Stop(PIDSDRIVERBUFFER iface)
{
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t avail;
    snd_pcm_format_t format;
    IDsDriverBufferImpl *This = (IDsDriverBufferImpl *)iface;
    TRACE("(%p)\n",iface);

    /* **** */
    EnterCriticalSection(&This->pcm_crst);
    avail = This->mmap_buflen_frames;
    snd_pcm_drop(This->pcm);
    snd_pcm_prepare(This->pcm);
    avail = snd_pcm_avail_update(This->pcm);
    snd_pcm_hw_params_get_format(This->hw_params, &format);
    if (This->mmap)
    {
        snd_pcm_mmap_begin(This->pcm, &areas, &This->mmap_pos, &avail);
        snd_pcm_format_set_silence(format, areas->addr, This->mmap_buflen_frames);
        snd_pcm_mmap_commit(This->pcm, This->mmap_pos, 0);
    }
    else
    {
        snd_pcm_format_set_silence(format, This->mmap_buffer, This->mmap_buflen_frames);
        snd_pcm_writei(This->pcm, This->mmap_buffer, This->mmap_buflen_frames);
        This->mmap_pos = 0;
    }

    /* **** */
    LeaveCriticalSection(&This->pcm_crst);
    return DS_OK;
}

static const IDsDriverBufferVtbl dsdbvt =
{
    IDsDriverBufferImpl_QueryInterface,
    IDsDriverBufferImpl_AddRef,
    IDsDriverBufferImpl_Release,
    IDsDriverBufferImpl_Lock,
    IDsDriverBufferImpl_Unlock,
    IDsDriverBufferImpl_SetFormat,
    IDsDriverBufferImpl_SetFrequency,
    IDsDriverBufferImpl_SetVolumePan,
    IDsDriverBufferImpl_SetPosition,
    IDsDriverBufferImpl_GetPosition,
    IDsDriverBufferImpl_Play,
    IDsDriverBufferImpl_Stop
};

static HRESULT WINAPI IDsDriverImpl_QueryInterface(PIDSDRIVER iface, REFIID riid, LPVOID *ppobj)
{
    /* IDsDriverImpl *This = (IDsDriverImpl *)iface; */
    FIXME("(%p): stub!\n",iface);
    return DSERR_UNSUPPORTED;
}

static ULONG WINAPI IDsDriverImpl_AddRef(PIDSDRIVER iface)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(ref before=%u)\n",This, refCount - 1);

    return refCount;
}

static ULONG WINAPI IDsDriverImpl_Release(PIDSDRIVER iface)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(ref before=%u)\n",This, refCount + 1);

    if (refCount)
        return refCount;

    HeapFree(GetProcessHeap(), 0, This);
    return 0;
}

static HRESULT WINAPI IDsDriverImpl_GetDriverDesc(PIDSDRIVER iface, PDSDRIVERDESC pDesc)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    TRACE("(%p,%p)\n",iface,pDesc);
    *pDesc			= WOutDev[This->wDevID].ds_desc;
    pDesc->dwFlags		= DSDDESC_DONTNEEDSECONDARYLOCK | DSDDESC_DONTNEEDWRITELEAD;
    pDesc->dnDevNode		= WOutDev[This->wDevID].waveDesc.dnDevNode;
    pDesc->wVxdId		= 0;
    pDesc->wReserved		= 0;
    pDesc->ulDeviceNum		= This->wDevID;
    pDesc->dwHeapType		= DSDHEAP_NOHEAP;
    pDesc->pvDirectDrawHeap	= NULL;
    pDesc->dwMemStartAddress	= 0xDEAD0000;
    pDesc->dwMemEndAddress	= 0xDEAF0000;
    pDesc->dwMemAllocExtra	= 0;
    pDesc->pvReserved1		= NULL;
    pDesc->pvReserved2		= NULL;
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_Open(PIDSDRIVER iface)
{
    HRESULT hr = S_OK;
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    int err=0;
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *hw_params;

    /* While this is not really needed, it is a good idea to do this,
     * to see if sound can be initialized */

    hw_params = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, snd_pcm_hw_params_sizeof());

    if (!hw_params)
    {
        hr = DSERR_OUTOFMEMORY;
        goto unalloc;
    }

    err = snd_pcm_open(&pcm, WOutDev[This->wDevID].pcmname, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0) goto err;
    err = snd_pcm_hw_params_any(pcm, hw_params);
    if (err < 0) goto err;
    err = snd_pcm_hw_params_set_access (pcm, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (err < 0)
        err = snd_pcm_hw_params_set_access (pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) goto err;

    TRACE("Success\n");
    snd_pcm_close(pcm);
    goto unalloc;

    err:
    hr = DSERR_GENERIC;
    FIXME("Failed to open device: %s\n", snd_strerror(err));
    if (pcm)
        snd_pcm_close(pcm);
    unalloc:
    HeapFree(GetProcessHeap(), 0, hw_params);
    if (hr != S_OK)
        WARN("--> %08x\n", hr);
    return hr;
}

static HRESULT WINAPI IDsDriverImpl_Close(PIDSDRIVER iface)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    TRACE("(%p) stub, harmless\n",This);
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_GetCaps(PIDSDRIVER iface, PDSDRIVERCAPS pCaps)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    TRACE("(%p,%p)\n",iface,pCaps);
    *pCaps = WOutDev[This->wDevID].ds_caps;
    return DS_OK;
}

static HRESULT WINAPI IDsDriverImpl_CreateSoundBuffer(PIDSDRIVER iface,
						      LPWAVEFORMATEX pwfx,
						      DWORD dwFlags, DWORD dwCardAddress,
						      LPDWORD pdwcbBufferSize,
						      LPBYTE *ppbBuffer,
						      LPVOID *ppvObj)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    IDsDriverBufferImpl** ippdsdb = (IDsDriverBufferImpl**)ppvObj;
    HRESULT err;

    TRACE("(%p,%p,%x,%x)\n",iface,pwfx,dwFlags,dwCardAddress);
    /* we only support primary buffers... for now */
    if (!(dwFlags & DSBCAPS_PRIMARYBUFFER))
        return DSERR_UNSUPPORTED;
    if (This->primary)
        return DSERR_ALLOCATED;

    *ippdsdb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDsDriverBufferImpl));
    if (*ippdsdb == NULL)
        return DSERR_OUTOFMEMORY;

    (*ippdsdb)->hw_params = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, snd_pcm_hw_params_sizeof());
    (*ippdsdb)->sw_params = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, snd_pcm_sw_params_sizeof());
    if (!(*ippdsdb)->hw_params || !(*ippdsdb)->sw_params)
    {
        HeapFree(GetProcessHeap(), 0, (*ippdsdb)->sw_params);
        HeapFree(GetProcessHeap(), 0, (*ippdsdb)->hw_params);
        return DSERR_OUTOFMEMORY;
    }
    (*ippdsdb)->lpVtbl  = &dsdbvt;
    (*ippdsdb)->ref	= 1;
    (*ippdsdb)->drv	= This;
    InitializeCriticalSection(&(*ippdsdb)->pcm_crst);
    (*ippdsdb)->pcm_crst.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": ALSA_DSOUTPUT.pcm_crst");

    /* SetFormat has to re-initialize pcm here anyway */
    err = SetFormat(*ippdsdb, pwfx);
    if (FAILED(err))
    {
        WARN("Error occurred: %08x\n", err);
        goto err;
    }

    if (dwFlags & DSBCAPS_PRIMARYBUFFER)
        This->primary = *ippdsdb;

    *pdwcbBufferSize = (*ippdsdb)->mmap_buflen_bytes;
    *ppbBuffer = (*ippdsdb)->mmap_buffer;

    /* buffer is ready to go */
    TRACE("buffer created at %p\n", *ippdsdb);
    return err;

    err:
    HeapFree(GetProcessHeap(), 0, (*ippdsdb)->sw_params);
    HeapFree(GetProcessHeap(), 0, (*ippdsdb)->hw_params);
    HeapFree(GetProcessHeap(), 0, *ippdsdb);
    *ippdsdb = NULL;
    return err;
}

static HRESULT WINAPI IDsDriverImpl_DuplicateSoundBuffer(PIDSDRIVER iface,
							 PIDSDRIVERBUFFER pBuffer,
							 LPVOID *ppvObj)
{
    IDsDriverImpl *This = (IDsDriverImpl *)iface;
    FIXME("(%p,%p): stub\n",This,pBuffer);
    return DSERR_INVALIDCALL;
}

static const IDsDriverVtbl dsdvt =
{
    IDsDriverImpl_QueryInterface,
    IDsDriverImpl_AddRef,
    IDsDriverImpl_Release,
    IDsDriverImpl_GetDriverDesc,
    IDsDriverImpl_Open,
    IDsDriverImpl_Close,
    IDsDriverImpl_GetCaps,
    IDsDriverImpl_CreateSoundBuffer,
    IDsDriverImpl_DuplicateSoundBuffer
};

DWORD wodDsCreate(UINT wDevID, PIDSDRIVER* drv)
{
    IDsDriverImpl** idrv = (IDsDriverImpl**)drv;

    TRACE("driver created\n");

    *idrv = HeapAlloc(GetProcessHeap(),0,sizeof(IDsDriverImpl));
    if (!*idrv)
        return MMSYSERR_NOMEM;
    (*idrv)->lpVtbl	= &dsdvt;
    (*idrv)->ref	= 1;

    (*idrv)->wDevID	= wDevID;
    (*idrv)->primary	= NULL;
    return MMSYSERR_NOERROR;
}

DWORD wodDsDesc(UINT wDevID, PDSDRIVERDESC desc)
{
    *desc = WOutDev[wDevID].ds_desc;
    return MMSYSERR_NOERROR;
}

#endif /* HAVE_ALSA */

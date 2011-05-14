/*
 * Wine Driver for ALSA
 *
 * Copyright	2002 Eric Pouech
 * Copyright	2006 Jaroslav Kysela
 * Copyright	2007 Maarten Lankhorst
 *
 * This file has a few shared generic subroutines shared among the alsa
 * implementation.
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winnls.h"
#include "winerror.h"
#include "mmddk.h"
#include "mmreg.h"
#include "dsound.h"
#include "dsdriver.h"
#include "ks.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#include "alsa.h"

#include "initguid.h"
#include "ksmedia.h"

WINE_DEFAULT_DEBUG_CHANNEL(alsa);
/* unless someone makes a wineserver kernel module, Unix pipes are faster than win32 events */
#define USE_PIPE_SYNC

#ifdef USE_PIPE_SYNC
#define INIT_OMR(omr) do { if (pipe(omr->msg_pipe) < 0) { omr->msg_pipe[0] = omr->msg_pipe[1] = -1; } } while (0)
#define CLOSE_OMR(omr) do { close(omr->msg_pipe[0]); close(omr->msg_pipe[1]); } while (0)
#define SIGNAL_OMR(omr) do { int x = 0; write((omr)->msg_pipe[1], &x, sizeof(x)); } while (0)
#define CLEAR_OMR(omr) do { int x = 0; read((omr)->msg_pipe[0], &x, sizeof(x)); } while (0)
#define RESET_OMR(omr) do { } while (0)
#define WAIT_OMR(omr, sleep) \
  do { struct pollfd pfd; pfd.fd = (omr)->msg_pipe[0]; \
       pfd.events = POLLIN; poll(&pfd, 1, sleep); } while (0)
#else
#define INIT_OMR(omr) do { omr->msg_event = CreateEventW(NULL, FALSE, FALSE, NULL); } while (0)
#define CLOSE_OMR(omr) do { CloseHandle(omr->msg_event); } while (0)
#define SIGNAL_OMR(omr) do { SetEvent((omr)->msg_event); } while (0)
#define CLEAR_OMR(omr) do { } while (0)
#define RESET_OMR(omr) do { ResetEvent((omr)->msg_event); } while (0)
#define WAIT_OMR(omr, sleep) \
  do { WaitForSingleObject((omr)->msg_event, sleep); } while (0)
#endif

#define ALSA_RING_BUFFER_INCREMENT      64

/******************************************************************
 *		ALSA_InitRingMessage
 *
 * Initialize the ring of messages for passing between driver's caller and playback/record
 * thread
 */
int ALSA_InitRingMessage(ALSA_MSG_RING* omr)
{
    omr->msg_toget = 0;
    omr->msg_tosave = 0;
    INIT_OMR(omr);
    omr->ring_buffer_size = ALSA_RING_BUFFER_INCREMENT;
    omr->messages = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,omr->ring_buffer_size * sizeof(ALSA_MSG));

    InitializeCriticalSection(&omr->msg_crst);
    omr->msg_crst.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": ALSA_MSG_RING.msg_crst");
    return 0;
}

/******************************************************************
 *		ALSA_DestroyRingMessage
 *
 */
int ALSA_DestroyRingMessage(ALSA_MSG_RING* omr)
{
    CLOSE_OMR(omr);
    HeapFree(GetProcessHeap(),0,omr->messages);
    omr->ring_buffer_size = 0;
    omr->msg_crst.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection(&omr->msg_crst);
    return 0;
}
/******************************************************************
 *		ALSA_ResetRingMessage
 *
 */
void ALSA_ResetRingMessage(ALSA_MSG_RING* omr)
{
    RESET_OMR(omr);
}

/******************************************************************
 *		ALSA_WaitRingMessage
 *
 */
void ALSA_WaitRingMessage(ALSA_MSG_RING* omr, DWORD sleep)
{
    WAIT_OMR(omr, sleep);
}

/******************************************************************
 *		ALSA_AddRingMessage
 *
 * Inserts a new message into the ring (should be called from DriverProc derived routines)
 */
int ALSA_AddRingMessage(ALSA_MSG_RING* omr, enum win_wm_message msg, DWORD_PTR param, BOOL wait)
{
    HANDLE	hEvent = NULL;

    EnterCriticalSection(&omr->msg_crst);
    if (omr->msg_toget == ((omr->msg_tosave + 1) % omr->ring_buffer_size))
    {
	int old_ring_buffer_size = omr->ring_buffer_size;
	omr->ring_buffer_size += ALSA_RING_BUFFER_INCREMENT;
	omr->messages = HeapReAlloc(GetProcessHeap(),0,omr->messages, omr->ring_buffer_size * sizeof(ALSA_MSG));
	/* Now we need to rearrange the ring buffer so that the new
	   buffers just allocated are in between omr->msg_tosave and
	   omr->msg_toget.
	*/
	if (omr->msg_tosave < omr->msg_toget)
	{
	    memmove(&(omr->messages[omr->msg_toget + ALSA_RING_BUFFER_INCREMENT]),
		    &(omr->messages[omr->msg_toget]),
		    sizeof(ALSA_MSG)*(old_ring_buffer_size - omr->msg_toget)
		    );
	    omr->msg_toget += ALSA_RING_BUFFER_INCREMENT;
	}
    }
    if (wait)
    {
        hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!hEvent)
        {
            ERR("can't create event !?\n");
            LeaveCriticalSection(&omr->msg_crst);
            return 0;
        }
        if (omr->msg_toget != omr->msg_tosave && omr->messages[omr->msg_toget].msg != WINE_WM_HEADER) {
            FIXME("two fast messages in the queue!!!! toget = %d(%s), tosave=%d(%s)\n",
                  omr->msg_toget,ALSA_getCmdString(omr->messages[omr->msg_toget].msg),
                  omr->msg_tosave,ALSA_getCmdString(omr->messages[omr->msg_tosave].msg));
	    wait = FALSE;
	    CloseHandle(hEvent);
	    hEvent = INVALID_HANDLE_VALUE;
	}

        /* fast messages have to be added at the start of the queue */
        omr->msg_toget = (omr->msg_toget + omr->ring_buffer_size - 1) % omr->ring_buffer_size;

        omr->messages[omr->msg_toget].msg = msg;
        omr->messages[omr->msg_toget].param = param;
        omr->messages[omr->msg_toget].hEvent = hEvent;
    }
    else
    {
        omr->messages[omr->msg_tosave].msg = msg;
        omr->messages[omr->msg_tosave].param = param;
        omr->messages[omr->msg_tosave].hEvent = NULL;
        omr->msg_tosave = (omr->msg_tosave + 1) % omr->ring_buffer_size;
    }
    LeaveCriticalSection(&omr->msg_crst);
    /* signal a new message */
    SIGNAL_OMR(omr);
    if (wait)
    {
        /* wait for playback/record thread to have processed the message */
        WaitForSingleObject(hEvent, INFINITE);
        CloseHandle(hEvent);
    }
    return 1;
}

/******************************************************************
 *		ALSA_RetrieveRingMessage
 *
 * Get a message from the ring. Should be called by the playback/record thread.
 */
int ALSA_RetrieveRingMessage(ALSA_MSG_RING* omr, enum win_wm_message *msg,
                             DWORD_PTR *param, HANDLE *hEvent)
{
    EnterCriticalSection(&omr->msg_crst);

    if (omr->msg_toget == omr->msg_tosave) /* buffer empty ? */
    {
        LeaveCriticalSection(&omr->msg_crst);
	return 0;
    }

    *msg = omr->messages[omr->msg_toget].msg;
    omr->messages[omr->msg_toget].msg = 0;
    *param = omr->messages[omr->msg_toget].param;
    *hEvent = omr->messages[omr->msg_toget].hEvent;
    omr->msg_toget = (omr->msg_toget + 1) % omr->ring_buffer_size;
    CLEAR_OMR(omr);
    LeaveCriticalSection(&omr->msg_crst);
    return 1;
}

/*======================================================================*
 *                  Utility functions                                   *
 *======================================================================*/

/* These strings used only for tracing */
const char * ALSA_getCmdString(enum win_wm_message msg)
{
#define MSG_TO_STR(x) case x: return #x
    switch(msg) {
    MSG_TO_STR(WINE_WM_PAUSING);
    MSG_TO_STR(WINE_WM_RESTARTING);
    MSG_TO_STR(WINE_WM_RESETTING);
    MSG_TO_STR(WINE_WM_HEADER);
    MSG_TO_STR(WINE_WM_UPDATE);
    MSG_TO_STR(WINE_WM_BREAKLOOP);
    MSG_TO_STR(WINE_WM_CLOSING);
    MSG_TO_STR(WINE_WM_STARTING);
    MSG_TO_STR(WINE_WM_STOPPING);
    }
#undef MSG_TO_STR
    return wine_dbg_sprintf("UNKNOWN(0x%08x)", msg);
}

const char * ALSA_getMessage(UINT msg)
{
#define MSG_TO_STR(x) case x: return #x
    switch(msg) {
    MSG_TO_STR(DRVM_INIT);
    MSG_TO_STR(DRVM_EXIT);
    MSG_TO_STR(DRVM_ENABLE);
    MSG_TO_STR(DRVM_DISABLE);
    MSG_TO_STR(WIDM_OPEN);
    MSG_TO_STR(WIDM_CLOSE);
    MSG_TO_STR(WIDM_ADDBUFFER);
    MSG_TO_STR(WIDM_PREPARE);
    MSG_TO_STR(WIDM_UNPREPARE);
    MSG_TO_STR(WIDM_GETDEVCAPS);
    MSG_TO_STR(WIDM_GETNUMDEVS);
    MSG_TO_STR(WIDM_GETPOS);
    MSG_TO_STR(WIDM_RESET);
    MSG_TO_STR(WIDM_START);
    MSG_TO_STR(WIDM_STOP);
    MSG_TO_STR(WODM_OPEN);
    MSG_TO_STR(WODM_CLOSE);
    MSG_TO_STR(WODM_WRITE);
    MSG_TO_STR(WODM_PAUSE);
    MSG_TO_STR(WODM_GETPOS);
    MSG_TO_STR(WODM_BREAKLOOP);
    MSG_TO_STR(WODM_PREPARE);
    MSG_TO_STR(WODM_UNPREPARE);
    MSG_TO_STR(WODM_GETDEVCAPS);
    MSG_TO_STR(WODM_GETNUMDEVS);
    MSG_TO_STR(WODM_GETPITCH);
    MSG_TO_STR(WODM_SETPITCH);
    MSG_TO_STR(WODM_GETPLAYBACKRATE);
    MSG_TO_STR(WODM_SETPLAYBACKRATE);
    MSG_TO_STR(WODM_GETVOLUME);
    MSG_TO_STR(WODM_SETVOLUME);
    MSG_TO_STR(WODM_RESTART);
    MSG_TO_STR(WODM_RESET);
    MSG_TO_STR(DRV_QUERYDEVICEINTERFACESIZE);
    MSG_TO_STR(DRV_QUERYDEVICEINTERFACE);
    MSG_TO_STR(DRV_QUERYDSOUNDIFACE);
    MSG_TO_STR(DRV_QUERYDSOUNDDESC);
    }
#undef MSG_TO_STR
    return wine_dbg_sprintf("UNKNOWN(0x%04x)", msg);
}

const char * ALSA_getFormat(WORD wFormatTag)
{
#define FMT_TO_STR(x) case x: return #x
    switch(wFormatTag) {
    FMT_TO_STR(WAVE_FORMAT_PCM);
    FMT_TO_STR(WAVE_FORMAT_EXTENSIBLE);
    FMT_TO_STR(WAVE_FORMAT_MULAW);
    FMT_TO_STR(WAVE_FORMAT_ALAW);
    FMT_TO_STR(WAVE_FORMAT_ADPCM);
    }
#undef FMT_TO_STR
    return wine_dbg_sprintf("UNKNOWN(0x%04x)", wFormatTag);
}

/* Allow 1% deviation for sample rates (some ES137x cards) */
BOOL ALSA_NearMatch(int rate1, int rate2)
{
    return (((100 * (rate1 - rate2)) / rate1) == 0);
}

DWORD ALSA_bytes_to_mmtime(LPMMTIME lpTime, DWORD position, WAVEFORMATPCMEX* format)
{
    TRACE("wType=%04X wBitsPerSample=%u nSamplesPerSec=%u nChannels=%u nAvgBytesPerSec=%u\n",
          lpTime->wType, format->Format.wBitsPerSample, format->Format.nSamplesPerSec,
          format->Format.nChannels, format->Format.nAvgBytesPerSec);
    TRACE("Position in bytes=%u\n", position);

    switch (lpTime->wType) {
    case TIME_SAMPLES:
        lpTime->u.sample = position / (format->Format.wBitsPerSample / 8 * format->Format.nChannels);
        TRACE("TIME_SAMPLES=%u\n", lpTime->u.sample);
        break;
    case TIME_MS:
        lpTime->u.ms = 1000.0 * position / (format->Format.wBitsPerSample / 8 * format->Format.nChannels * format->Format.nSamplesPerSec);
        TRACE("TIME_MS=%u\n", lpTime->u.ms);
        break;
    case TIME_SMPTE:
        lpTime->u.smpte.fps = 30;
        position = position / (format->Format.wBitsPerSample / 8 * format->Format.nChannels);
        position += (format->Format.nSamplesPerSec / lpTime->u.smpte.fps) - 1; /* round up */
        lpTime->u.smpte.sec = position / format->Format.nSamplesPerSec;
        position -= lpTime->u.smpte.sec * format->Format.nSamplesPerSec;
        lpTime->u.smpte.min = lpTime->u.smpte.sec / 60;
        lpTime->u.smpte.sec -= 60 * lpTime->u.smpte.min;
        lpTime->u.smpte.hour = lpTime->u.smpte.min / 60;
        lpTime->u.smpte.min -= 60 * lpTime->u.smpte.hour;
        lpTime->u.smpte.fps = 30;
        lpTime->u.smpte.frame = position * lpTime->u.smpte.fps / format->Format.nSamplesPerSec;
        TRACE("TIME_SMPTE=%02u:%02u:%02u:%02u\n",
              lpTime->u.smpte.hour, lpTime->u.smpte.min,
              lpTime->u.smpte.sec, lpTime->u.smpte.frame);
        break;
    default:
        WARN("Format %d not supported, using TIME_BYTES !\n", lpTime->wType);
        lpTime->wType = TIME_BYTES;
        /* fall through */
    case TIME_BYTES:
        lpTime->u.cb = position;
        TRACE("TIME_BYTES=%u\n", lpTime->u.cb);
        break;
    }
    return MMSYSERR_NOERROR;
}

void ALSA_copyFormat(LPWAVEFORMATEX wf1, LPWAVEFORMATPCMEX wf2)
{
    unsigned int iLength;

    ZeroMemory(wf2, sizeof(*wf2));
    if (wf1->wFormatTag == WAVE_FORMAT_PCM)
        iLength = sizeof(PCMWAVEFORMAT);
    else if (wf1->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        iLength = sizeof(WAVEFORMATPCMEX);
    else
        iLength = sizeof(WAVEFORMATEX) + wf1->cbSize;
    memcpy(wf2, wf1, iLength);
}

BOOL ALSA_supportedFormat(LPWAVEFORMATEX wf)
{
    TRACE("(%p)\n",wf);

    if (wf->nSamplesPerSec<DSBFREQUENCY_MIN||wf->nSamplesPerSec>DSBFREQUENCY_MAX)
        return FALSE;

    if (wf->wFormatTag == WAVE_FORMAT_PCM) {
        if (wf->nChannels==1||wf->nChannels==2) {
            if (wf->wBitsPerSample==8||wf->wBitsPerSample==16)
                return TRUE;
        }
    } else if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE 	* wfex = (WAVEFORMATEXTENSIBLE *)wf;

        if (wf->cbSize == 22 &&
            (IsEqualGUID(&wfex->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM) ||
             IsEqualGUID(&wfex->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))) {
            if (wf->nChannels>=1 && wf->nChannels<=6) {
                if (wf->wBitsPerSample==wfex->Samples.wValidBitsPerSample) {
                    if (wf->wBitsPerSample==8||wf->wBitsPerSample==16||
                        wf->wBitsPerSample==24||wf->wBitsPerSample==32) {
                        return TRUE;
                    }
                } else
                    WARN("wBitsPerSample != wValidBitsPerSample not supported yet\n");
            }
        } else
            WARN("only KSDATAFORMAT_SUBTYPE_PCM and KSDATAFORMAT_SUBTYPE_IEEE_FLOAT "
                 "supported\n");
    } else
        WARN("only WAVE_FORMAT_PCM and WAVE_FORMAT_EXTENSIBLE supported\n");

    return FALSE;
}

/*======================================================================*
 *                  Low level WAVE implementation			*
 *======================================================================*/

/**************************************************************************
 * 			ALSA_CheckSetVolume		[internal]
 *
 *  Helper function for Alsa volume queries.  This tries to simplify
 * the process of managing the volume.  All parameters are optional
 * (pass NULL to ignore or not use).
 *  Return values are MMSYSERR_NOERROR on success, or !0 on failure;
 * error codes are normalized into the possible documented return
 * values from waveOutGetVolume.
 */
int ALSA_CheckSetVolume(snd_hctl_t *hctl, int *out_left, int *out_right,
            int *out_min, int *out_max, int *out_step,
            int *new_left, int *new_right)
{
    int rc = MMSYSERR_NOERROR;
    int value_count = 0;
    snd_hctl_elem_t *           elem = NULL;
    snd_ctl_elem_info_t *       eleminfop = NULL;
    snd_ctl_elem_value_t *      elemvaluep = NULL;
    snd_ctl_elem_id_t *         elemidp = NULL;
    const char *names[] = {"PCM Playback Volume", "Line Playback Volume", NULL};
    const char **name;


#define EXIT_ON_ERROR(f,txt,exitcode) do \
{ \
    int err; \
    if ( (err = (f) ) < 0) \
    { \
	ERR(txt " failed: %s\n", snd_strerror(err)); \
        rc = exitcode; \
        goto out; \
    } \
} while(0)

    if (! hctl)
        return MMSYSERR_NOTSUPPORTED;

    /* Allocate areas to return information about the volume */
    EXIT_ON_ERROR(snd_ctl_elem_id_malloc(&elemidp), "snd_ctl_elem_id_malloc", MMSYSERR_NOMEM);
    EXIT_ON_ERROR(snd_ctl_elem_value_malloc (&elemvaluep), "snd_ctl_elem_value_malloc", MMSYSERR_NOMEM);
    EXIT_ON_ERROR(snd_ctl_elem_info_malloc (&eleminfop), "snd_ctl_elem_info_malloc", MMSYSERR_NOMEM);
    snd_ctl_elem_id_clear(elemidp);
    snd_ctl_elem_value_clear(elemvaluep);
    snd_ctl_elem_info_clear(eleminfop);

    /* Setup and find an element id that exactly matches the characteristic we want
    ** FIXME:  It is probably short sighted to hard code and fixate on PCM Playback Volume */

    for( name = names; *name; name++ )
    {
	snd_ctl_elem_id_set_name(elemidp, *name);
	snd_ctl_elem_id_set_interface(elemidp, SND_CTL_ELEM_IFACE_MIXER);
	elem = snd_hctl_find_elem(hctl, elemidp);
	if (elem)
	{
	    /* Read and return volume information */
	    EXIT_ON_ERROR(snd_hctl_elem_info(elem, eleminfop), "snd_hctl_elem_info", MMSYSERR_NOTSUPPORTED);
	    value_count = snd_ctl_elem_info_get_count(eleminfop);
	    if (out_min || out_max || out_step)
	    {
		if (!snd_ctl_elem_info_is_readable(eleminfop))
		{
		    ERR("snd_ctl_elem_info_is_readable returned false; cannot return info\n");
		    rc = MMSYSERR_NOTSUPPORTED;
		    goto out;
		}

		if (out_min)
		    *out_min = snd_ctl_elem_info_get_min(eleminfop);

		if (out_max)
		    *out_max = snd_ctl_elem_info_get_max(eleminfop);

		if (out_step)
		    *out_step = snd_ctl_elem_info_get_step(eleminfop);
	    }

	    if (out_left || out_right)
	    {
		EXIT_ON_ERROR(snd_hctl_elem_read(elem, elemvaluep), "snd_hctl_elem_read", MMSYSERR_NOTSUPPORTED);

		if (out_left)
		    *out_left = snd_ctl_elem_value_get_integer(elemvaluep, 0);

		if (out_right)
		{
		    if (value_count == 1)
			*out_right = snd_ctl_elem_value_get_integer(elemvaluep, 0);
		    else if (value_count == 2)
			*out_right = snd_ctl_elem_value_get_integer(elemvaluep, 1);
		    else
		    {
			ERR("Unexpected value count %d from snd_ctl_elem_info_get_count while getting volume info\n", value_count);
			rc = -1;
			goto out;
		    }
		}
	    }

	    /* Set the volume */
	    if (new_left || new_right)
	    {
		EXIT_ON_ERROR(snd_hctl_elem_read(elem, elemvaluep), "snd_hctl_elem_read", MMSYSERR_NOTSUPPORTED);
		if (new_left)
		    snd_ctl_elem_value_set_integer(elemvaluep, 0, *new_left);
		if (new_right)
		{
		    if (value_count == 1)
			snd_ctl_elem_value_set_integer(elemvaluep, 0, *new_right);
		    else if (value_count == 2)
			snd_ctl_elem_value_set_integer(elemvaluep, 1, *new_right);
		    else
		    {
			ERR("Unexpected value count %d from snd_ctl_elem_info_get_count while setting volume info\n", value_count);
			rc = -1;
			goto out;
		    }
		}

		EXIT_ON_ERROR(snd_hctl_elem_write(elem, elemvaluep), "snd_hctl_elem_write", MMSYSERR_NOTSUPPORTED);
	    }

	    break;
	}
    }

    if( !*name )
    {
        ERR("Could not find '{PCM,Line} Playback Volume' element\n");
        rc = MMSYSERR_NOTSUPPORTED;
    }


#undef EXIT_ON_ERROR

out:

    if (elemvaluep)
        snd_ctl_elem_value_free(elemvaluep);
    if (eleminfop)
        snd_ctl_elem_info_free(eleminfop);
    if (elemidp)
        snd_ctl_elem_id_free(elemidp);

    return rc;
}


/**************************************************************************
 * 			wine_snd_pcm_recover		[internal]
 *
 * Code slightly modified from alsa-lib v1.0.23 snd_pcm_recover implementation.
 * used to recover from XRUN errors (buffer underflow/overflow)
 */
int wine_snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{
    if (err > 0)
        err = -err;
    if (err == -EINTR)	/* nothing to do, continue */
        return 0;
    if (err == -EPIPE) {
        const char *s;
        if (snd_pcm_stream(pcm) == SND_PCM_STREAM_PLAYBACK)
            s = "underrun";
        else
            s = "overrun";
        if (!silent)
            ERR("%s occurred\n", s);
        err = snd_pcm_prepare(pcm);
        if (err < 0) {
            ERR("cannot recover from %s, prepare failed: %s\n", s, snd_strerror(err));
            return err;
        }
        return 0;
    }
    if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
            /* wait until suspend flag is released */
            poll(NULL, 0, 1000);
        if (err < 0) {
            err = snd_pcm_prepare(pcm);
            if (err < 0) {
                ERR("cannot recover from suspend, prepare failed: %s\n", snd_strerror(err));
                return err;
            }
        }
        return 0;
    }
    return err;
}

/**************************************************************************
 * 			ALSA_TraceParameters		[internal]
 *
 * used to trace format changes, hw and sw parameters
 */
void ALSA_TraceParameters(snd_pcm_hw_params_t * hw_params, snd_pcm_sw_params_t * sw, int full)
{
    int err;
    snd_pcm_format_t   format;
    snd_pcm_access_t   access;

#define X(x) ((x)? "true" : "false")
    if (full)
	TRACE("FLAGS: sampleres=%s overrng=%s pause=%s resume=%s syncstart=%s batch=%s block=%s double=%s "
	      "halfd=%s joint=%s\n",
	      X(snd_pcm_hw_params_can_mmap_sample_resolution(hw_params)),
	      X(snd_pcm_hw_params_can_overrange(hw_params)),
	      X(snd_pcm_hw_params_can_pause(hw_params)),
	      X(snd_pcm_hw_params_can_resume(hw_params)),
	      X(snd_pcm_hw_params_can_sync_start(hw_params)),
	      X(snd_pcm_hw_params_is_batch(hw_params)),
	      X(snd_pcm_hw_params_is_block_transfer(hw_params)),
	      X(snd_pcm_hw_params_is_double(hw_params)),
	      X(snd_pcm_hw_params_is_half_duplex(hw_params)),
	      X(snd_pcm_hw_params_is_joint_duplex(hw_params)));
#undef X

    err = snd_pcm_hw_params_get_access(hw_params, &access);
    if (err >= 0)
    {
	TRACE("access=%s\n", snd_pcm_access_name(access));
    }
    else
    {
	snd_pcm_access_mask_t * acmask;

        acmask = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, snd_pcm_access_mask_sizeof());
	snd_pcm_hw_params_get_access_mask(hw_params, acmask);
	for ( access = SND_PCM_ACCESS_MMAP_INTERLEAVED; access <= SND_PCM_ACCESS_LAST; access++)
	    if (snd_pcm_access_mask_test(acmask, access))
		TRACE("access=%s\n", snd_pcm_access_name(access));
        HeapFree( GetProcessHeap(), 0, acmask );
    }

    err = snd_pcm_hw_params_get_format(hw_params, &format);
    if (err >= 0)
    {
	TRACE("format=%s\n", snd_pcm_format_name(format));

    }
    else
    {
	snd_pcm_format_mask_t *     fmask;

        fmask = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, snd_pcm_format_mask_sizeof());
	snd_pcm_hw_params_get_format_mask(hw_params, fmask);
	for ( format = SND_PCM_FORMAT_S8; format <= SND_PCM_FORMAT_LAST ; format++)
	    if ( snd_pcm_format_mask_test(fmask, format) )
		TRACE("format=%s\n", snd_pcm_format_name(format));
        HeapFree( GetProcessHeap(), 0, fmask );
    }

    do {
      int err=0;
      unsigned int val=0;
      err = snd_pcm_hw_params_get_channels(hw_params, &val);
      if (err<0) {
        unsigned int min = 0;
        unsigned int max = 0;
        err = snd_pcm_hw_params_get_channels_min(hw_params, &min),
	err = snd_pcm_hw_params_get_channels_max(hw_params, &max);
        TRACE("channels_min=%u, channels_min_max=%u\n", min, max);
      } else {
        TRACE("channels=%d\n", val);
      }
    } while(0);
    do {
      int err=0;
      snd_pcm_uframes_t val=0;
      err = snd_pcm_hw_params_get_buffer_size(hw_params, &val);
      if (err<0) {
        snd_pcm_uframes_t min = 0;
        snd_pcm_uframes_t max = 0;
        err = snd_pcm_hw_params_get_buffer_size_min(hw_params, &min),
	err = snd_pcm_hw_params_get_buffer_size_max(hw_params, &max);
        TRACE("buffer_size_min=%lu, buffer_size_min_max=%lu\n", min, max);
      } else {
        TRACE("buffer_size=%lu\n", val);
      }
    } while(0);

#define X(x) do { \
int err=0; \
int dir=0; \
unsigned int val=0; \
err = snd_pcm_hw_params_get_##x(hw_params,&val, &dir); \
if (err<0) { \
  unsigned int min = 0; \
  unsigned int max = 0; \
  err = snd_pcm_hw_params_get_##x##_min(hw_params, &min, &dir); \
  err = snd_pcm_hw_params_get_##x##_max(hw_params, &max, &dir); \
  TRACE(#x "_min=%u " #x "_max=%u\n", min, max); \
} else \
    TRACE(#x "=%d\n", val); \
} while(0)

    X(rate);
    X(buffer_time);
    X(periods);
    do {
      int err=0;
      int dir=0;
      snd_pcm_uframes_t val=0;
      err = snd_pcm_hw_params_get_period_size(hw_params, &val, &dir);
      if (err<0) {
        snd_pcm_uframes_t min = 0;
        snd_pcm_uframes_t max = 0;
        err = snd_pcm_hw_params_get_period_size_min(hw_params, &min, &dir),
	err = snd_pcm_hw_params_get_period_size_max(hw_params, &max, &dir);
        TRACE("period_size_min=%lu, period_size_min_max=%lu\n", min, max);
      } else {
        TRACE("period_size=%lu\n", val);
      }
    } while(0);

    X(period_time);
#undef X

    if (!sw)
	return;
}

/**************************************************************************
 * 				DriverProc (WINEALSA.@)
 */
LRESULT CALLBACK ALSA_DriverProc(DWORD_PTR dwDevID, HDRVR hDriv, UINT wMsg,
                                 LPARAM dwParam1, LPARAM dwParam2)
{
/* EPP     TRACE("(%08lX, %04X, %08lX, %08lX, %08lX)\n",  */
/* EPP 	  dwDevID, hDriv, wMsg, dwParam1, dwParam2); */

    switch(wMsg) {
    case DRV_LOAD:
    case DRV_FREE:
    case DRV_OPEN:
    case DRV_CLOSE:
    case DRV_ENABLE:
    case DRV_DISABLE:
    case DRV_QUERYCONFIGURE:
        return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "ALSA MultiMedia Driver !", "ALSA Driver", MB_OK);	return 1;
    case DRV_INSTALL:
    case DRV_REMOVE:
        return DRV_SUCCESS;
    default:
	return 0;
    }
}

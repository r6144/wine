/*
 * Sample MIXER Wine Driver for Mac OS X (based on OSS mixer)
 *
 * Copyright 	1997 Marcus Meissner
 * 		1999,2001 Eric Pouech
 *              2006,2007 Emmanuel Maillard
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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "mmddk.h"
#include "coreaudio.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mixer);

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#define	WINE_MIXER_NAME "CoreAudio Mixer"

#define InputDevice (1 << 0)
#define OutputDevice (1 << 1)

#define IsInput(dir) ((dir) & InputDevice)
#define IsOutput(dir) ((dir) & OutputDevice)

#define ControlsPerLine 2 /* number of control per line : volume & (mute | onoff) */

#define IDControlVolume 0
#define IDControlMute 1

typedef struct tagMixerLine
{
    char *name;
    int direction;
    int numChannels;
    int componentType;
    AudioDeviceID deviceID;
} MixerLine;

typedef struct tagMixerCtrl
{
    DWORD dwLineID;
    MIXERCONTROLW ctrl;
} MixerCtrl;

typedef struct tagCoreAudio_Mixer
{
    MIXERCAPSW caps;

    MixerCtrl *mixerCtrls;
    MixerLine *lines;
    DWORD numCtrl;
} CoreAudio_Mixer;

static CoreAudio_Mixer mixer;
static int numMixers = 1;

/**************************************************************************
*/

static const char * getMessage(UINT uMsg)
{
#define MSG_TO_STR(x) case x: return #x;
    switch (uMsg) {
        MSG_TO_STR(DRVM_INIT);
        MSG_TO_STR(DRVM_EXIT);
        MSG_TO_STR(DRVM_ENABLE);
        MSG_TO_STR(DRVM_DISABLE);
        MSG_TO_STR(MXDM_GETDEVCAPS);
        MSG_TO_STR(MXDM_GETLINEINFO);
        MSG_TO_STR(MXDM_GETNUMDEVS);
        MSG_TO_STR(MXDM_OPEN);
        MSG_TO_STR(MXDM_CLOSE);
        MSG_TO_STR(MXDM_GETLINECONTROLS);
        MSG_TO_STR(MXDM_GETCONTROLDETAILS);
        MSG_TO_STR(MXDM_SETCONTROLDETAILS);
    }
#undef MSG_TO_STR
    return wine_dbg_sprintf("UNKNOWN(%08x)", uMsg);
}

static const char * getControlType(DWORD dwControlType)
{
#define TYPE_TO_STR(x) case x: return #x;
    switch (dwControlType) {
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_CUSTOM);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_BOOLEANMETER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_SIGNEDMETER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_PEAKMETER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_UNSIGNEDMETER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_BOOLEAN);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_ONOFF);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MUTE);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MONO);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_LOUDNESS);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_STEREOENH);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_BASS_BOOST);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_BUTTON);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_DECIBELS);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_SIGNED);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_UNSIGNED);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_PERCENT);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_SLIDER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_PAN);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_QSOUNDPAN);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_FADER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_VOLUME);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_BASS);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_TREBLE);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_EQUALIZER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_SINGLESELECT);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MUX);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MULTIPLESELECT);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MIXER);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MICROTIME);
        TYPE_TO_STR(MIXERCONTROL_CONTROLTYPE_MILLITIME);
    }
#undef TYPE_TO_STR
    return wine_dbg_sprintf("UNKNOWN(%08x)", dwControlType);
}

static const char * getComponentType(DWORD dwComponentType)
{
#define TYPE_TO_STR(x) case x: return #x;
    switch (dwComponentType) {
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_UNDEFINED);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_DIGITAL);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_LINE);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_MONITOR);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_SPEAKERS);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_HEADPHONES);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_TELEPHONE);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_WAVEIN);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_DST_VOICEIN);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_DIGITAL);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_LINE);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_TELEPHONE);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY);
        TYPE_TO_STR(MIXERLINE_COMPONENTTYPE_SRC_ANALOG);
    }
#undef TYPE_TO_STR
    return wine_dbg_sprintf("UNKNOWN(%08x)", dwComponentType);
}

static const char * getTargetType(DWORD dwType)
{
#define TYPE_TO_STR(x) case x: return #x;
    switch (dwType) {
        TYPE_TO_STR(MIXERLINE_TARGETTYPE_UNDEFINED);
        TYPE_TO_STR(MIXERLINE_TARGETTYPE_WAVEOUT);
        TYPE_TO_STR(MIXERLINE_TARGETTYPE_WAVEIN);
        TYPE_TO_STR(MIXERLINE_TARGETTYPE_MIDIOUT);
        TYPE_TO_STR(MIXERLINE_TARGETTYPE_MIDIIN);
        TYPE_TO_STR(MIXERLINE_TARGETTYPE_AUX);
    }
#undef TYPE_TO_STR
    return wine_dbg_sprintf("UNKNOWN(%08x)", dwType);
}

/* FIXME is there a better way ? */
static DWORD DeviceComponentType(char *name)
{
    if (strcmp(name, "Built-in Microphone") == 0)
        return MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;

    if (strcmp(name, "Built-in Line Input") == 0)
        return MIXERLINE_COMPONENTTYPE_SRC_LINE;

    if (strcmp(name, "Built-in Output") == 0)
        return MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;

    return MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED;
}

static BOOL DeviceHasMute(AudioDeviceID deviceID, Boolean isInput)
{
    Boolean writable = false;
    OSStatus err = noErr;
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyMute;
    propertyAddress.mScope = isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    propertyAddress.mElement = 0;
    if (AudioObjectHasProperty(deviceID, &propertyAddress))
    {
        /* check if we can set it */
        err = AudioObjectIsPropertySettable(deviceID, &propertyAddress, &writable);
        if (err == noErr)
            return writable;
    }
    return FALSE;
}

/*
 * Getters
 */
static BOOL MIX_LineGetVolume(DWORD lineID, DWORD channels, Float32 *left, Float32 *right)
{
    MixerLine *line = &mixer.lines[lineID];
    UInt32 size = sizeof(Float32);
    OSStatus err = noErr;
    AudioObjectPropertyAddress address;
    *left = *right = 0.0;

    address.mSelector = kAudioDevicePropertyVolumeScalar;
    address.mScope = IsInput(line->direction) ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    address.mElement = 1;
    err = AudioObjectGetPropertyData(line->deviceID, &address, 0, NULL, &size, left);
    if (err != noErr)
        return FALSE;

    if (channels == 2)
    {
        size = sizeof(Float32);
        address.mElement = 2;
        err = AudioObjectGetPropertyData(line->deviceID, &address, 0, NULL, &size, right);
        if (err != noErr)
            return FALSE;
    }

    TRACE("lineID %d channels %d return left %f right %f\n", lineID, channels, *left, *right);
    return (err == noErr);
}

static BOOL MIX_LineGetMute(DWORD lineID, BOOL *muted)
{
    MixerLine *line = &mixer.lines[lineID];
    UInt32 size = sizeof(UInt32);
    UInt32 val = 0;
    OSStatus err = noErr;
    AudioObjectPropertyAddress address;
    address.mSelector = kAudioDevicePropertyMute;
    address.mScope = IsInput(line->direction) ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    address.mElement = 0;
    err = AudioObjectGetPropertyData(line->deviceID, &address, 0, NULL, &size, &val);
    *muted = val;

    return (err == noErr);
}

/*
 * Setters
 */
static BOOL MIX_LineSetVolume(DWORD lineID, DWORD channels, Float32 left, Float32 right)
{
    MixerLine *line = &mixer.lines[lineID];
    UInt32 size = sizeof(Float32);
    AudioObjectPropertyAddress address;
    OSStatus err = noErr;
    TRACE("lineID %d channels %d left %f right %f\n", lineID, channels, left, right);

    address.mSelector = kAudioDevicePropertyVolumeScalar;
    address.mScope = IsInput(line->direction) ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    if (channels == 2)
    {
        address.mElement = 1;
        err = AudioObjectSetPropertyData(line->deviceID, &address, 0, NULL, size, &left);
        if (err != noErr)
            return FALSE;

        address.mElement = 2;
        err = AudioObjectSetPropertyData(line->deviceID, &address, 0, NULL, size, &right);
    }
    else
    {
        /*
            FIXME Using master channel failed ?? return kAudioHardwareUnknownPropertyError
            address.mElement = 0;
            err = AudioObjectSetPropertyData(line->deviceID, &address, 0, NULL, size, &left);
        */
        right = left;
        address.mElement = 1;
        err = AudioObjectSetPropertyData(line->deviceID, &address, 0, NULL, size, &left);
        if (err != noErr)
            return FALSE;
        address.mElement = 2;
        err = AudioObjectSetPropertyData(line->deviceID, &address, 0, NULL, size, &right);
    }
    return (err == noErr);
}

static BOOL MIX_LineSetMute(DWORD lineID, BOOL mute)
{
    MixerLine *line = &mixer.lines[lineID];
    UInt32 val = mute;
    UInt32 size = sizeof(UInt32);
    AudioObjectPropertyAddress address;
    OSStatus err = noErr;

    address.mSelector = kAudioDevicePropertyMute;
    address.mScope = IsInput(line->direction) ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    address.mElement = 0;
    err = AudioObjectSetPropertyData(line->deviceID, &address, 0, 0, size, &val);
    return (err == noErr);
}

static void MIX_FillControls(void)
{
    int i;
    int ctrl = 0;
    MixerLine *line;
    for (i = 0; i < mixer.caps.cDestinations; i++)
    {
        line = &mixer.lines[i];
        mixer.mixerCtrls[ctrl].dwLineID = i;
        mixer.mixerCtrls[ctrl].ctrl.cbStruct = sizeof(MIXERCONTROLW);
        mixer.mixerCtrls[ctrl].ctrl.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
        mixer.mixerCtrls[ctrl].ctrl.dwControlID = ctrl;
        mixer.mixerCtrls[ctrl].ctrl.Bounds.s1.dwMinimum = 0;
        mixer.mixerCtrls[ctrl].ctrl.Bounds.s1.dwMaximum = 65535;
        mixer.mixerCtrls[ctrl].ctrl.Metrics.cSteps = 656;
        ctrl++;

        mixer.mixerCtrls[ctrl].dwLineID = i;
        if ( !DeviceHasMute(line->deviceID, IsInput(line->direction)) )
            mixer.mixerCtrls[ctrl].ctrl.fdwControl |= MIXERCONTROL_CONTROLF_DISABLED;

        mixer.mixerCtrls[ctrl].ctrl.cbStruct = sizeof(MIXERCONTROLW);
        mixer.mixerCtrls[ctrl].ctrl.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
        mixer.mixerCtrls[ctrl].ctrl.dwControlID = ctrl;
        mixer.mixerCtrls[ctrl].ctrl.Bounds.s1.dwMinimum = 0;
        mixer.mixerCtrls[ctrl].ctrl.Bounds.s1.dwMaximum = 1;
        ctrl++;
    }
    assert(ctrl == mixer.numCtrl);
}

/**************************************************************************
* 				CoreAudio_MixerInit
*/
LONG CoreAudio_MixerInit(void)
{
    OSStatus status;
    UInt32 propertySize;
    AudioObjectPropertyAddress propertyAddress;
    AudioDeviceID *deviceArray = NULL;
    CFStringRef name;
    int i;
    int numLines;

    AudioStreamBasicDescription streamDescription;

    /* Find number of lines */
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;
    status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize);
    if (status)
    {
        ERR("AudioObjectGetPropertyDataSize for kAudioHardwarePropertyDevices return %s\n", wine_dbgstr_fourcc(status));
        return DRV_FAILURE;
    }

    numLines = propertySize / sizeof(AudioDeviceID);

    mixer.mixerCtrls = NULL;
    mixer.lines = NULL;
    mixer.numCtrl = 0;

    mixer.caps.cDestinations = numLines;
    mixer.caps.wMid = 0xAA;
    mixer.caps.wPid = 0x55;
    mixer.caps.vDriverVersion = 0x0100;

    MultiByteToWideChar(CP_ACP, 0, WINE_MIXER_NAME, -1, mixer.caps.szPname, sizeof(mixer.caps.szPname) / sizeof(WCHAR));

    mixer.caps.fdwSupport = 0; /* No bits defined yet */

    mixer.lines = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MixerLine) * numLines);
    if (!mixer.lines)
        goto error;

    deviceArray = HeapAlloc(GetProcessHeap(), 0, sizeof(AudioDeviceID) * numLines);

    propertySize = sizeof(AudioDeviceID) * numLines;
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, deviceArray);
    if (status)
    {
        ERR("AudioObjectGetPropertyData for kAudioHardwarePropertyDevices return %s\n", wine_dbgstr_fourcc(status));
        goto error;
    }

    for (i = 0; i < numLines; i++)
    {
        MixerLine *line = &mixer.lines[i];

        line->deviceID = deviceArray[i];

        propertySize = sizeof(CFStringRef);
        propertyAddress.mSelector = kAudioObjectPropertyName;
        propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement = kAudioObjectPropertyElementMaster;
        status = AudioObjectGetPropertyData(line->deviceID, &propertyAddress, 0, NULL, &propertySize, &name);
        if (status) {
            ERR("AudioObjectGetPropertyData for kAudioObjectPropertyName return %s\n", wine_dbgstr_fourcc(status));
            goto error;
        }

        line->name = HeapAlloc(GetProcessHeap(), 0, CFStringGetLength(name) + 1);
        if (!line->name)
            goto error;

        CFStringGetCString(name, line->name, CFStringGetLength(name) + 1, kCFStringEncodingUTF8);

        line->componentType = DeviceComponentType(line->name);

        /* check for directions */
        /* Output ? */
        propertySize = sizeof(UInt32);
        propertyAddress.mSelector = kAudioDevicePropertyStreams;
        propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
	status = AudioObjectGetPropertyDataSize(line->deviceID, &propertyAddress, 0, NULL, &propertySize);
        if (status) {
            ERR("AudioObjectGetPropertyDataSize for kAudioDevicePropertyStreams return %s\n", wine_dbgstr_fourcc(status));
            goto error;
        }

        if ( (propertySize / sizeof(AudioStreamID)) != 0)
        {
            line->direction |= OutputDevice;

            /* Check the number of channel for the stream */
            propertySize = sizeof(streamDescription);
            propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
            status = AudioObjectGetPropertyData(line->deviceID, &propertyAddress, 0, NULL, &propertySize, &streamDescription);
            if (status != noErr) {
                ERR("AudioObjectGetPropertyData for kAudioDevicePropertyStreamFormat return %s\n", wine_dbgstr_fourcc(status));
                goto error;
            }
            line->numChannels = streamDescription.mChannelsPerFrame;
        }
        else
        {
            /* Input ? */
            propertySize = sizeof(UInt32);
            propertyAddress.mScope = kAudioDevicePropertyScopeInput;
            status = AudioObjectGetPropertyDataSize(line->deviceID, &propertyAddress, 0, NULL, &propertySize);
            if (status) {
                ERR("AudioObjectGetPropertyDataSize for kAudioDevicePropertyStreams return %s\n", wine_dbgstr_fourcc(status));
                goto error;
            }
            if ( (propertySize / sizeof(AudioStreamID)) != 0)
            {
                line->direction |= InputDevice;

                /* Check the number of channel for the stream */
                propertySize = sizeof(streamDescription);
                propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
                status = AudioObjectGetPropertyData(line->deviceID, &propertyAddress, 0, NULL, &propertySize, &streamDescription);
                if (status != noErr) {
                    ERR("AudioObjectGetPropertyData for kAudioDevicePropertyStreamFormat return %s\n", wine_dbgstr_fourcc(status));
                    goto error;
                }
                line->numChannels = streamDescription.mChannelsPerFrame;
            }
        }

        mixer.numCtrl += ControlsPerLine; /* volume & (mute | onoff) */
    }
    mixer.mixerCtrls = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MixerCtrl) * mixer.numCtrl);
    if (!mixer.mixerCtrls)
        goto error;

    MIX_FillControls();

    HeapFree(GetProcessHeap(), 0, deviceArray);
    return DRV_SUCCESS;

error:
    if (mixer.lines)
    {
        int i;
        for (i = 0; i < mixer.caps.cDestinations; i++)
        {
            HeapFree(GetProcessHeap(), 0, mixer.lines[i].name);
        }
        HeapFree(GetProcessHeap(), 0, mixer.lines);
    }
    HeapFree(GetProcessHeap(), 0, deviceArray);
    if (mixer.mixerCtrls)
        HeapFree(GetProcessHeap(), 0, mixer.mixerCtrls);
    return DRV_FAILURE;
}

/**************************************************************************
* 				CoreAudio_MixerRelease
*/
void CoreAudio_MixerRelease(void)
{
    TRACE("()\n");

    if (mixer.lines)
    {
        int i;
        for (i = 0; i < mixer.caps.cDestinations; i++)
        {
            HeapFree(GetProcessHeap(), 0, mixer.lines[i].name);
        }
        HeapFree(GetProcessHeap(), 0, mixer.lines);
    }
    if (mixer.mixerCtrls)
        HeapFree(GetProcessHeap(), 0, mixer.mixerCtrls);
}

/**************************************************************************
* 				MIX_Open			[internal]
*/
static DWORD MIX_Open(WORD wDevID, LPMIXEROPENDESC lpMod, DWORD_PTR flags)
{
    TRACE("wDevID=%d lpMod=%p dwSize=%08lx\n", wDevID, lpMod, flags);
    if (lpMod == NULL) {
        WARN("invalid parameter: lpMod == NULL\n");
        return MMSYSERR_INVALPARAM;
    }

    if (wDevID >= numMixers) {
        WARN("bad device ID: %04X\n", wDevID);
        return MMSYSERR_BADDEVICEID;
    }
    return MMSYSERR_NOERROR;
}

/**************************************************************************
* 				MIX_GetNumDevs			[internal]
*/
static DWORD MIX_GetNumDevs(void)
{
    TRACE("()\n");
    return numMixers;
}

static DWORD MIX_GetDevCaps(WORD wDevID, LPMIXERCAPSW lpCaps, DWORD_PTR dwSize)
{
    TRACE("wDevID=%d lpCaps=%p\n", wDevID, lpCaps);

    if (lpCaps == NULL) {
	WARN("Invalid Parameter\n");
	return MMSYSERR_INVALPARAM;
    }

    if (wDevID >= numMixers) {
        WARN("bad device ID : %d\n", wDevID);
	return MMSYSERR_BADDEVICEID;
    }
    memcpy(lpCaps, &mixer.caps, min(dwSize, sizeof(*lpCaps)));
    return MMSYSERR_NOERROR;
}

/**************************************************************************
* 				MIX_GetLineInfo			[internal]
*/
static DWORD MIX_GetLineInfo(WORD wDevID, LPMIXERLINEW lpMl, DWORD_PTR fdwInfo)
{
    int i;
    DWORD ret = MMSYSERR_ERROR;
    MixerLine *line = NULL;

    TRACE("%04X, %p, %08lx\n", wDevID, lpMl, fdwInfo);

    if (lpMl == NULL) {
        WARN("invalid parameter: lpMl = NULL\n");
	return MMSYSERR_INVALPARAM;
    }

    if (lpMl->cbStruct != sizeof(*lpMl)) {
        WARN("invalid parameter: lpMl->cbStruct\n");
	return MMSYSERR_INVALPARAM;
    }

    if (wDevID >= numMixers) {
        WARN("bad device ID: %04X\n", wDevID);
        return MMSYSERR_BADDEVICEID;
    }

    /* FIXME: set all the variables correctly... the lines below
        * are very wrong...
        */
    lpMl->dwUser = 0;

    switch (fdwInfo & MIXER_GETLINEINFOF_QUERYMASK)
    {
        case MIXER_GETLINEINFOF_DESTINATION:
            TRACE("MIXER_GETLINEINFOF_DESTINATION %d\n", lpMl->dwDestination);
            if ( (lpMl->dwDestination >= 0) && (lpMl->dwDestination < mixer.caps.cDestinations) )
            {
                lpMl->dwLineID = lpMl->dwDestination;
                line = &mixer.lines[lpMl->dwDestination];
            }
            else ret = MIXERR_INVALLINE;
            break;
        case MIXER_GETLINEINFOF_COMPONENTTYPE:
            TRACE("MIXER_GETLINEINFOF_COMPONENTTYPE %s\n", getComponentType(lpMl->dwComponentType));
            for (i = 0; i < mixer.caps.cDestinations; i++)
            {
                if (mixer.lines[i].componentType == lpMl->dwComponentType)
                {
                    lpMl->dwDestination = lpMl->dwLineID = i;
                    line = &mixer.lines[i];
                    break;
                }
            }
            if (line == NULL)
            {
                WARN("can't find component type %s\n", getComponentType(lpMl->dwComponentType));
                ret = MIXERR_INVALVALUE;
            }
            break;
        case MIXER_GETLINEINFOF_SOURCE:
            FIXME("MIXER_GETLINEINFOF_SOURCE %d dst=%d\n", lpMl->dwSource, lpMl->dwDestination);
            break;
        case MIXER_GETLINEINFOF_LINEID:
            TRACE("MIXER_GETLINEINFOF_LINEID %d\n", lpMl->dwLineID);
            if ( (lpMl->dwLineID >= 0) && (lpMl->dwLineID < mixer.caps.cDestinations) )
            {
                lpMl->dwDestination = lpMl->dwLineID;
                line = &mixer.lines[lpMl->dwLineID];
            }
            else ret = MIXERR_INVALLINE;
            break;
        case MIXER_GETLINEINFOF_TARGETTYPE:
            FIXME("MIXER_GETLINEINFOF_TARGETTYPE (%s)\n", getTargetType(lpMl->Target.dwType));
            switch (lpMl->Target.dwType) {
                case MIXERLINE_TARGETTYPE_UNDEFINED:
                case MIXERLINE_TARGETTYPE_WAVEOUT:
                case MIXERLINE_TARGETTYPE_WAVEIN:
                case MIXERLINE_TARGETTYPE_MIDIOUT:
                case MIXERLINE_TARGETTYPE_MIDIIN:
                case MIXERLINE_TARGETTYPE_AUX:
                default:
                    FIXME("Unhandled target type (%s)\n",
                          getTargetType(lpMl->Target.dwType));
                    return MMSYSERR_INVALPARAM;
            }
                break;
        default:
            WARN("Unknown flag (%08lx)\n", fdwInfo & MIXER_GETLINEINFOF_QUERYMASK);
            break;
    }

    if (line)
    {
        lpMl->dwComponentType = line->componentType;
        lpMl->cChannels = line->numChannels;
        lpMl->cControls = ControlsPerLine;

        /* FIXME check there with CoreAudio */
        lpMl->cConnections = 1;
        lpMl->fdwLine = MIXERLINE_LINEF_ACTIVE;

        MultiByteToWideChar(CP_ACP, 0, line->name, -1, lpMl->szShortName, sizeof(lpMl->szShortName) / sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, line->name, -1, lpMl->szName, sizeof(lpMl->szName) / sizeof(WCHAR));

        if ( IsInput(line->direction) )
            lpMl->Target.dwType = MIXERLINE_TARGETTYPE_WAVEIN;
        else
            lpMl->Target.dwType = MIXERLINE_TARGETTYPE_WAVEOUT;

        lpMl->Target.dwDeviceID = line->deviceID;
        lpMl->Target.wMid = mixer.caps.wMid;
        lpMl->Target.wPid = mixer.caps.wPid;
        lpMl->Target.vDriverVersion = mixer.caps.vDriverVersion;

        MultiByteToWideChar(CP_ACP, 0, WINE_MIXER_NAME, -1, lpMl->Target.szPname, sizeof(lpMl->Target.szPname) / sizeof(WCHAR));
        ret = MMSYSERR_NOERROR;
    }
    return ret;
}

/**************************************************************************
* 				MIX_GetLineControls		[internal]
*/
static DWORD MIX_GetLineControls(WORD wDevID, LPMIXERLINECONTROLSW lpMlc, DWORD_PTR flags)
{
    DWORD ret = MMSYSERR_NOTENABLED;
    int ctrl = 0;
    TRACE("%04X, %p, %08lX\n", wDevID, lpMlc, flags);

    if (lpMlc == NULL) {
        WARN("invalid parameter: lpMlc == NULL\n");
        return MMSYSERR_INVALPARAM;
    }

    if (lpMlc->cbStruct < sizeof(*lpMlc)) {
        WARN("invalid parameter: lpMlc->cbStruct = %d\n", lpMlc->cbStruct);
	return MMSYSERR_INVALPARAM;
    }

    if (lpMlc->cbmxctrl < sizeof(MIXERCONTROLW)) {
        WARN("invalid parameter: lpMlc->cbmxctrl = %d\n", lpMlc->cbmxctrl);
	return MMSYSERR_INVALPARAM;
    }

    if (wDevID >= numMixers) {
        WARN("bad device ID: %04X\n", wDevID);
        return MMSYSERR_BADDEVICEID;
    }

    switch (flags & MIXER_GETLINECONTROLSF_QUERYMASK)
    {
        case MIXER_GETLINECONTROLSF_ALL:
            FIXME("dwLineID=%d MIXER_GETLINECONTROLSF_ALL (%d)\n", lpMlc->dwLineID, lpMlc->cControls);
            if (lpMlc->cControls != ControlsPerLine)
            {
                WARN("invalid parameter lpMlc->cControls %d\n", lpMlc->cControls);
                ret = MMSYSERR_INVALPARAM;
	    }
            else
            {
                if ( (lpMlc->dwLineID >= 0) && (lpMlc->dwLineID < mixer.caps.cDestinations) )
                {
                    int i;
                    for (i = 0; i < lpMlc->cControls; i++)
                    {
                        lpMlc->pamxctrl[i] = mixer.mixerCtrls[lpMlc->dwLineID * i].ctrl;
                    }
                    ret = MMSYSERR_NOERROR;
                }
                else ret = MIXERR_INVALLINE;
            }
            break;
        case MIXER_GETLINECONTROLSF_ONEBYID:
            TRACE("dwLineID=%d MIXER_GETLINECONTROLSF_ONEBYID (%d)\n", lpMlc->dwLineID, lpMlc->u.dwControlID);
            if ( lpMlc->u.dwControlID >= 0 && lpMlc->u.dwControlID < mixer.numCtrl )
            {
                lpMlc->pamxctrl[0] = mixer.mixerCtrls[lpMlc->u.dwControlID].ctrl;
                ret = MMSYSERR_NOERROR;
            }
            else ret = MIXERR_INVALVALUE;
            break;
        case MIXER_GETLINECONTROLSF_ONEBYTYPE:
            TRACE("dwLineID=%d MIXER_GETLINECONTROLSF_ONEBYTYPE (%s)\n", lpMlc->dwLineID, getControlType(lpMlc->u.dwControlType));
            if ( (lpMlc->dwLineID < 0) || (lpMlc->dwLineID >= mixer.caps.cDestinations) )
            {
                ret = MIXERR_INVALLINE;
                break;
            }
            if (lpMlc->u.dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME)
            {
                ctrl = (lpMlc->dwLineID * ControlsPerLine) + IDControlVolume;
                lpMlc->pamxctrl[0] = mixer.mixerCtrls[ctrl].ctrl;
                ret = MMSYSERR_NOERROR;
            }
            else
                if (lpMlc->u.dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE)
                {
                    ctrl = (lpMlc->dwLineID * ControlsPerLine) + IDControlMute;
                    lpMlc->pamxctrl[0] = mixer.mixerCtrls[ctrl].ctrl;
                    ret = MMSYSERR_NOERROR;
                }
                break;
        default:
            ERR("Unknown flag %08lx\n", flags & MIXER_GETLINECONTROLSF_QUERYMASK);
            ret = MMSYSERR_INVALPARAM;
    }

    return ret;
}

/**************************************************************************
 * 				MIX_GetControlDetails		[internal]
 */
static DWORD MIX_GetControlDetails(WORD wDevID, LPMIXERCONTROLDETAILS lpmcd, DWORD_PTR fdwDetails)
{
    DWORD ret = MMSYSERR_NOTSUPPORTED;
    DWORD dwControlType;

    TRACE("%04X, %p, %08lx\n", wDevID, lpmcd, fdwDetails);

    if (lpmcd == NULL) {
        TRACE("invalid parameter: lpmcd == NULL\n");
        return MMSYSERR_INVALPARAM;
    }

    if (wDevID >= numMixers) {
        WARN("bad device ID: %04X\n", wDevID);
        return MMSYSERR_BADDEVICEID;
    }

    if ( (fdwDetails & MIXER_GETCONTROLDETAILSF_QUERYMASK) != MIXER_GETCONTROLDETAILSF_VALUE )
    {
        WARN("Unknown/unimplement GetControlDetails flag (%08lx)\n", fdwDetails & MIXER_GETCONTROLDETAILSF_QUERYMASK);
        return MMSYSERR_NOTSUPPORTED;
    }

    if ( lpmcd->dwControlID < 0 || lpmcd->dwControlID >= mixer.numCtrl )
    {
        WARN("bad control ID: %d\n", lpmcd->dwControlID);
        return MIXERR_INVALVALUE;
    }

    TRACE("MIXER_GETCONTROLDETAILSF_VALUE %d\n", lpmcd->dwControlID);

    dwControlType = mixer.mixerCtrls[lpmcd->dwControlID].ctrl.dwControlType;
    switch (dwControlType)
    {
        case MIXERCONTROL_CONTROLTYPE_VOLUME:
            FIXME("controlType : %s channels %d\n", getControlType(dwControlType), lpmcd->cChannels);
            {
                LPMIXERCONTROLDETAILS_UNSIGNED mcdu;
                Float32 left, right;

                if (lpmcd->cbDetails != sizeof(MIXERCONTROLDETAILS_UNSIGNED)) {
                    WARN("invalid parameter: lpmcd->cbDetails == %d\n", lpmcd->cbDetails);
                    return MMSYSERR_INVALPARAM;
                }

                if ( MIX_LineGetVolume(mixer.mixerCtrls[lpmcd->dwControlID].dwLineID, lpmcd->cChannels, &left, &right) )
                {
                    mcdu = (LPMIXERCONTROLDETAILS_UNSIGNED)lpmcd->paDetails;

		    switch (lpmcd->cChannels)
		    {
                        case 1:
                            /* mono... so R = L */
                            mcdu->dwValue = left * 65535;
                            TRACE("Reading RL = %d\n", mcdu->dwValue);
                            break;
                        case 2:
                            /* stereo, left is paDetails[0] */
                            mcdu->dwValue = left * 65535;
                            TRACE("Reading L = %d\n", mcdu->dwValue);
                            mcdu++;
                            mcdu->dwValue = right * 65535;
                            TRACE("Reading R = %d\n", mcdu->dwValue);
                            break;
                        default:
                            WARN("Unsupported cChannels (%d)\n", lpmcd->cChannels);
                            return MMSYSERR_INVALPARAM;
		    }
                    TRACE("=> %08x\n", mcdu->dwValue);
                    ret = MMSYSERR_NOERROR;
                }
            }
            break;
        case MIXERCONTROL_CONTROLTYPE_MUTE:
        case MIXERCONTROL_CONTROLTYPE_ONOFF:
            FIXME("%s MIXERCONTROLDETAILS_BOOLEAN[%u]\n", getControlType(dwControlType), lpmcd->cChannels);
            {
                LPMIXERCONTROLDETAILS_BOOLEAN mcdb;
                BOOL muted;
                if (lpmcd->cbDetails != sizeof(MIXERCONTROLDETAILS_BOOLEAN)) {
                    WARN("invalid parameter: lpmcd->cbDetails = %d\n", lpmcd->cbDetails);
                    return MMSYSERR_INVALPARAM;
                }
                mcdb = (LPMIXERCONTROLDETAILS_BOOLEAN)lpmcd->paDetails;

                if ( MIX_LineGetMute(mixer.mixerCtrls[lpmcd->dwControlID].dwLineID, &muted) )
                {
                    mcdb->fValue = muted;
		    TRACE("=> %s\n", mcdb->fValue ? "on" : "off");
                    ret = MMSYSERR_NOERROR;
                }
            }
            break;
        case MIXERCONTROL_CONTROLTYPE_MIXER:
        case MIXERCONTROL_CONTROLTYPE_MUX:
        default:
            FIXME("controlType : %s\n", getControlType(dwControlType));
            break;
    }
    return ret;
}

/**************************************************************************
* 				MIX_SetControlDetails		[internal]
*/
static DWORD MIX_SetControlDetails(WORD wDevID, LPMIXERCONTROLDETAILS lpmcd, DWORD_PTR fdwDetails)
{
    DWORD ret = MMSYSERR_NOTSUPPORTED;
    DWORD dwControlType;

    TRACE("%04X, %p, %08lx\n", wDevID, lpmcd, fdwDetails);

    if (lpmcd == NULL) {
        TRACE("invalid parameter: lpmcd == NULL\n");
        return MMSYSERR_INVALPARAM;
    }

    if (wDevID >= numMixers) {
        WARN("bad device ID: %04X\n", wDevID);
        return MMSYSERR_BADDEVICEID;
    }

    if ( (fdwDetails & MIXER_SETCONTROLDETAILSF_QUERYMASK) != MIXER_GETCONTROLDETAILSF_VALUE )
    {
        WARN("Unknown SetControlDetails flag (%08lx)\n", fdwDetails & MIXER_SETCONTROLDETAILSF_QUERYMASK);
        return MMSYSERR_NOTSUPPORTED;
    }

    TRACE("MIXER_SETCONTROLDETAILSF_VALUE dwControlID=%d\n", lpmcd->dwControlID);
    dwControlType = mixer.mixerCtrls[lpmcd->dwControlID].ctrl.dwControlType;
    switch (dwControlType)
    {
        case MIXERCONTROL_CONTROLTYPE_VOLUME:
            FIXME("controlType : %s\n", getControlType(dwControlType));
            {
                LPMIXERCONTROLDETAILS_UNSIGNED mcdu;
                Float32 left, right = 0;

                if (lpmcd->cbDetails != sizeof(MIXERCONTROLDETAILS_UNSIGNED)) {
                    WARN("invalid parameter: lpmcd->cbDetails == %d\n", lpmcd->cbDetails);
                    return MMSYSERR_INVALPARAM;
                }

                mcdu = (LPMIXERCONTROLDETAILS_UNSIGNED)lpmcd->paDetails;

                switch (lpmcd->cChannels)
                {
                    case 1:
                        /* mono... so R = L */
                        TRACE("Setting RL to %d\n", mcdu->dwValue);
                        left = (Float32) mcdu->dwValue / 65535.0;
                        break;
                    case 2:
                        /* stereo, left is paDetails[0] */
			TRACE("Setting L to %d\n", mcdu->dwValue);
			left = (Float32) mcdu->dwValue / 65535.0;
                        mcdu++;
			TRACE("Setting R to %d\n", mcdu->dwValue);
			right = (Float32) mcdu->dwValue / 65535.0;
			break;
                    default:
                        WARN("Unsupported cChannels (%d)\n", lpmcd->cChannels);
                        return MMSYSERR_INVALPARAM;
                }
                if ( MIX_LineSetVolume(mixer.mixerCtrls[lpmcd->dwControlID].dwLineID, lpmcd->cChannels, left, right) )
                    ret = MMSYSERR_NOERROR;
            }
            break;
        case MIXERCONTROL_CONTROLTYPE_MUTE:
        case MIXERCONTROL_CONTROLTYPE_ONOFF:
            TRACE("%s MIXERCONTROLDETAILS_BOOLEAN[%u]\n", getControlType(dwControlType), lpmcd->cChannels);
            {
                LPMIXERCONTROLDETAILS_BOOLEAN	mcdb;

                if (lpmcd->cbDetails != sizeof(MIXERCONTROLDETAILS_BOOLEAN)) {
                    WARN("invalid parameter: cbDetails\n");
                    return MMSYSERR_INVALPARAM;
                }
                mcdb = (LPMIXERCONTROLDETAILS_BOOLEAN)lpmcd->paDetails;
                if ( MIX_LineSetMute(mixer.mixerCtrls[lpmcd->dwControlID].dwLineID, mcdb->fValue) )
                    ret = MMSYSERR_NOERROR;
            }
            break;
        case MIXERCONTROL_CONTROLTYPE_MIXER:
        case MIXERCONTROL_CONTROLTYPE_MUX:
        default:
            FIXME("controlType : %s\n", getControlType(dwControlType));
            ret = MMSYSERR_NOTSUPPORTED;
            break;
    }
    return ret;
}

/**************************************************************************
* 				mxdMessage
*/
DWORD WINAPI CoreAudio_mxdMessage(UINT wDevID, UINT wMsg, DWORD_PTR dwUser,
                                  DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    TRACE("(%04X, %s, %08lX, %08lX, %08lX);\n", wDevID, getMessage(wMsg),
          dwUser, dwParam1, dwParam2);

    switch (wMsg)
    {
        case DRVM_INIT:
        case DRVM_EXIT:
        case DRVM_ENABLE:
        case DRVM_DISABLE:
            /* FIXME: Pretend this is supported */
            return 0;
        case MXDM_OPEN:
            return MIX_Open(wDevID, (LPMIXEROPENDESC)dwParam1, dwParam2);
        case MXDM_CLOSE:
            return MMSYSERR_NOERROR;
        case MXDM_GETNUMDEVS:
            return MIX_GetNumDevs();
        case MXDM_GETDEVCAPS:
            return MIX_GetDevCaps(wDevID, (LPMIXERCAPSW)dwParam1, dwParam2);
        case MXDM_GETLINEINFO:
            return MIX_GetLineInfo(wDevID, (LPMIXERLINEW)dwParam1, dwParam2);
        case MXDM_GETLINECONTROLS:
            return MIX_GetLineControls(wDevID, (LPMIXERLINECONTROLSW)dwParam1, dwParam2);
        case MXDM_GETCONTROLDETAILS:
            return MIX_GetControlDetails(wDevID, (LPMIXERCONTROLDETAILS)dwParam1, dwParam2);
        case MXDM_SETCONTROLDETAILS:
            return MIX_SetControlDetails(wDevID, (LPMIXERCONTROLDETAILS)dwParam1, dwParam2);
        default:
            WARN("unknown message %d!\n", wMsg);
            return MMSYSERR_NOTSUPPORTED;
    }
}

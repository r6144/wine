/*
 * Copyright 2009 Maarten Lankhorst
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

extern HRESULT MMDevEnum_Create(REFIID riid, void **ppv);
extern void MMDevEnum_Free(void);

extern HRESULT MMDevice_GetPropValue(const GUID *devguid, DWORD flow, REFPROPERTYKEY key, PROPVARIANT *pv);
extern HRESULT MMDevice_SetPropValue(const GUID *devguid, DWORD flow, REFPROPERTYKEY key, REFPROPVARIANT pv);

typedef struct MMDevice {
    const IMMDeviceVtbl *lpVtbl;
    const IMMEndpointVtbl *lpEndpointVtbl;
    LONG ref;

    CRITICAL_SECTION crst;

    EDataFlow flow;
    DWORD state;
    GUID devguid;
    WCHAR *alname;
} MMDevice;

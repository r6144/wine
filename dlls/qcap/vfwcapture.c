/* Video For Windows Steering structure
 *
 * Copyright 2005 Maarten Lankhorst
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
 *
 */

#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#define COBJMACROS

#include "config.h"
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wtypes.h"
#include "wingdi.h"
#include "winuser.h"
#include "dshow.h"

#include "qcap_main.h"
#include "wine/debug.h"

#include "capture.h"
#include "uuids.h"
#include "vfwmsgs.h"
#include "amvideo.h"
#include "strmif.h"
#include "ddraw.h"
#include "ocidl.h"
#include "oleauto.h"

WINE_DEFAULT_DEBUG_CHANNEL(qcap);

#define ICOM_THIS_MULTI(impl,field,iface) \
    impl* const This=(impl*)((char*)(iface) - offsetof(impl,field))

static const IBaseFilterVtbl VfwCapture_Vtbl;
static const IAMStreamConfigVtbl IAMStreamConfig_VTable;
static const IAMVideoProcAmpVtbl IAMVideoProcAmp_VTable;
static const IPersistPropertyBagVtbl IPersistPropertyBag_VTable;
static const IPinVtbl VfwPin_Vtbl;

static HRESULT VfwPin_Construct( IBaseFilter *, LPCRITICAL_SECTION, IPin ** );

typedef struct VfwCapture
{
    BaseFilter filter;
    IAMStreamConfig IAMStreamConfig_iface;
    IAMVideoProcAmp IAMVideoProcAmp_iface;
    IPersistPropertyBag IPersistPropertyBag_iface;

    BOOL init;
    Capture *driver_info;

    IPin * pOutputPin;
} VfwCapture;

static inline VfwCapture *impl_from_IAMStreamConfig(IAMStreamConfig *iface)
{
    return CONTAINING_RECORD(iface, VfwCapture, IAMStreamConfig_iface);
}

static inline VfwCapture *impl_from_IAMVideoProcAmp(IAMVideoProcAmp *iface)
{
    return CONTAINING_RECORD(iface, VfwCapture, IAMVideoProcAmp_iface);
}

static inline VfwCapture *impl_from_IPersistPropertyBag(IPersistPropertyBag *iface)
{
    return CONTAINING_RECORD(iface, VfwCapture, IPersistPropertyBag_iface);
}

/* VfwPin implementation */
typedef struct VfwPinImpl
{
    BaseOutputPin pin;
    Capture *driver_info;
    VfwCapture *parent;
    const IKsPropertySetVtbl * KSP_VT;
} VfwPinImpl;

static IPin* WINAPI VfwCapture_GetPin(BaseFilter *iface, int pos)
{
    VfwCapture *This = (VfwCapture *)iface;

    if (pos >= 1 || pos < 0)
        return NULL;

    IPin_AddRef(This->pOutputPin);
    return This->pOutputPin;
}

static LONG WINAPI VfwCapture_GetPinCount(BaseFilter *iface)
{
    return 1;
}

static const BaseFilterFuncTable BaseFuncTable = {
    VfwCapture_GetPin,
    VfwCapture_GetPinCount
};

IUnknown * WINAPI QCAP_createVFWCaptureFilter(IUnknown *pUnkOuter, HRESULT *phr)
{
    VfwCapture *pVfwCapture;
    HRESULT hr;

    TRACE("%p - %p\n", pUnkOuter, phr);

    *phr = CLASS_E_NOAGGREGATION;
    if (pUnkOuter)
        return NULL;
    *phr = E_OUTOFMEMORY;

    pVfwCapture = CoTaskMemAlloc( sizeof(VfwCapture) );

    if (!pVfwCapture)
        return NULL;

    BaseFilter_Init(&pVfwCapture->filter, &VfwCapture_Vtbl, &CLSID_VfwCapture, (DWORD_PTR)(__FILE__ ": VfwCapture.csFilter"), &BaseFuncTable);

    pVfwCapture->IAMStreamConfig_iface.lpVtbl = &IAMStreamConfig_VTable;
    pVfwCapture->IAMVideoProcAmp_iface.lpVtbl = &IAMVideoProcAmp_VTable;
    pVfwCapture->IPersistPropertyBag_iface.lpVtbl = &IPersistPropertyBag_VTable;
    pVfwCapture->init = FALSE;

    hr = VfwPin_Construct((IBaseFilter *)&pVfwCapture->filter.lpVtbl,
                   &pVfwCapture->filter.csFilter, &pVfwCapture->pOutputPin);
    if (FAILED(hr))
    {
        CoTaskMemFree(pVfwCapture);
        return NULL;
    }
    TRACE("-- created at %p\n", pVfwCapture);

    ObjectRefCount(TRUE);
    *phr = S_OK;
    return (IUnknown *)pVfwCapture;
}

static HRESULT WINAPI VfwCapture_QueryInterface(IBaseFilter * iface, REFIID riid, LPVOID * ppv)
{
    VfwCapture *This = (VfwCapture *)iface;
    TRACE("(%s, %p)\n", debugstr_guid(riid), ppv);
    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IPersist) ||
        IsEqualIID(riid, &IID_IMediaFilter) ||
        IsEqualIID(riid, &IID_IBaseFilter))
    {
        *ppv = This;
    }
    else if (IsEqualIID(riid, &IID_IAMStreamConfig))
        *ppv = &This->IAMStreamConfig_iface;
    else if (IsEqualIID(riid, &IID_IAMVideoProcAmp))
        *ppv = &This->IAMVideoProcAmp_iface;
    else if (IsEqualIID(riid, &IID_IPersistPropertyBag))
        *ppv = &This->IPersistPropertyBag_iface;

    if (!IsEqualIID(riid, &IID_IUnknown) &&
        !IsEqualIID(riid, &IID_IPersist) &&
        !IsEqualIID(riid, &IID_IPersistPropertyBag) &&
        !This->init)
    {
        FIXME("Capture system not initialised when looking for %s, "
              "trying it on primary device now\n", debugstr_guid(riid));
        This->driver_info = qcap_driver_init( This->pOutputPin, 0 );
        if (!This->driver_info)
        {
            ERR("VfwCapture initialisation failed\n");
            return E_UNEXPECTED;
        }
        This->init = TRUE;
    }

    if (*ppv)
    {
        TRACE("Returning %s interface\n", debugstr_guid(riid));
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s!\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI VfwCapture_Release(IBaseFilter * iface)
{
    VfwCapture *This = (VfwCapture *)iface;
    ULONG refCount = BaseFilterImpl_Release(iface);

    TRACE("%p->() New refcount: %d\n", This, refCount);

    if (!refCount)
    {
        BasePin *pin;

        TRACE("destroying everything\n");
        if (This->init)
        {
            if (This->filter.state != State_Stopped)
                qcap_driver_stop(This->driver_info, &This->filter.state);
            qcap_driver_destroy(This->driver_info);
        }
        pin = (BasePin*) This->pOutputPin;
        if (pin->pConnectedTo != NULL)
        {
            IPin_Disconnect(pin->pConnectedTo);
            IPin_Disconnect(This->pOutputPin);
        }
        IPin_Release(This->pOutputPin);
        CoTaskMemFree(This);
        ObjectRefCount(FALSE);
    }
    return refCount;
}

/** IMediaFilter methods **/

static HRESULT WINAPI VfwCapture_Stop(IBaseFilter * iface)
{
    VfwCapture *This = (VfwCapture *)iface;

    TRACE("()\n");
    return qcap_driver_stop(This->driver_info, &This->filter.state);
}

static HRESULT WINAPI VfwCapture_Pause(IBaseFilter * iface)
{
    VfwCapture *This = (VfwCapture *)iface;

    TRACE("()\n");
    return qcap_driver_pause(This->driver_info, &This->filter.state);
}

static HRESULT WINAPI VfwCapture_Run(IBaseFilter * iface, REFERENCE_TIME tStart)
{
    VfwCapture *This = (VfwCapture *)iface;
    TRACE("(%x%08x)\n", (ULONG)(tStart >> 32), (ULONG)tStart);
    return qcap_driver_run(This->driver_info, &This->filter.state);
}

/** IBaseFilter methods **/
static HRESULT WINAPI VfwCapture_FindPin(IBaseFilter * iface, LPCWSTR Id, IPin **ppPin)
{
    FIXME("(%s, %p) - stub\n", debugstr_w(Id), ppPin);
    return E_NOTIMPL;
}

static const IBaseFilterVtbl VfwCapture_Vtbl =
{
    VfwCapture_QueryInterface,
    BaseFilterImpl_AddRef,
    VfwCapture_Release,
    BaseFilterImpl_GetClassID,
    VfwCapture_Stop,
    VfwCapture_Pause,
    VfwCapture_Run,
    BaseFilterImpl_GetState,
    BaseFilterImpl_SetSyncSource,
    BaseFilterImpl_GetSyncSource,
    BaseFilterImpl_EnumPins,
    VfwCapture_FindPin,
    BaseFilterImpl_QueryFilterInfo,
    BaseFilterImpl_JoinFilterGraph,
    BaseFilterImpl_QueryVendorInfo
};

/* AMStreamConfig interface, we only need to implement {G,S}etFormat */
static HRESULT WINAPI
AMStreamConfig_QueryInterface( IAMStreamConfig * iface, REFIID riid, LPVOID * ppv )
{
    VfwCapture *This = impl_from_IAMStreamConfig(iface);

    TRACE("%p --> %s\n", This, debugstr_guid(riid));

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IAMStreamConfig))
    {
        IAMStreamConfig_AddRef(iface);
        *ppv = iface;
        return S_OK;
    }

    FIXME("No interface for iid %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI AMStreamConfig_AddRef( IAMStreamConfig * iface )
{
    VfwCapture *This = impl_from_IAMStreamConfig(iface);

    TRACE("%p --> Forwarding to VfwCapture (%p)\n", iface, This);
    return IUnknown_AddRef((IUnknown *)This);
}

static ULONG WINAPI AMStreamConfig_Release( IAMStreamConfig * iface )
{
    VfwCapture *This = impl_from_IAMStreamConfig(iface);

    TRACE("%p --> Forwarding to VfwCapture (%p)\n", iface, This);
    return IUnknown_Release((IUnknown *)This);
}

static HRESULT WINAPI
AMStreamConfig_SetFormat(IAMStreamConfig *iface, AM_MEDIA_TYPE *pmt)
{
    HRESULT hr;
    VfwCapture *This = impl_from_IAMStreamConfig(iface);
    BasePin *pin;

    TRACE("(%p): %p->%p\n", iface, pmt, pmt ? pmt->pbFormat : NULL);

    if (This->filter.state != State_Stopped)
    {
        TRACE("Returning not stopped error\n");
        return VFW_E_NOT_STOPPED;
    }

    if (!pmt)
    {
        TRACE("pmt is NULL\n");
        return E_POINTER;
    }

    dump_AM_MEDIA_TYPE(pmt);

    pin = (BasePin *)This->pOutputPin;
    if (pin->pConnectedTo != NULL)
    {
        hr = IPin_QueryAccept(pin->pConnectedTo, pmt);
        TRACE("Would accept: %d\n", hr);
        if (hr == S_FALSE)
            return VFW_E_INVALIDMEDIATYPE;
    }

    hr = qcap_driver_set_format(This->driver_info, pmt);
    if (SUCCEEDED(hr) && This->filter.filterInfo.pGraph && pin->pConnectedTo )
    {
        hr = IFilterGraph_Reconnect(This->filter.filterInfo.pGraph, This->pOutputPin);
        if (SUCCEEDED(hr))
            TRACE("Reconnection completed, with new media format..\n");
    }
    TRACE("Returning: %d\n", hr);
    return hr;
}

static HRESULT WINAPI
AMStreamConfig_GetFormat( IAMStreamConfig *iface, AM_MEDIA_TYPE **pmt )
{
    VfwCapture *This = impl_from_IAMStreamConfig(iface);

    TRACE("%p -> (%p)\n", iface, pmt);
    return qcap_driver_get_format(This->driver_info, pmt);
}

static HRESULT WINAPI
AMStreamConfig_GetNumberOfCapabilities( IAMStreamConfig *iface, int *piCount,
                                        int *piSize )
{
    FIXME("%p: %p %p - stub, intentional\n", iface, piCount, piSize);
    return E_NOTIMPL; /* Not implemented for this interface */
}

static HRESULT WINAPI
AMStreamConfig_GetStreamCaps( IAMStreamConfig *iface, int iIndex,
                              AM_MEDIA_TYPE **pmt, BYTE *pSCC )
{
    FIXME("%p: %d %p %p - stub, intentional\n", iface, iIndex, pmt, pSCC);
    return E_NOTIMPL; /* Not implemented for this interface */
}

static const IAMStreamConfigVtbl IAMStreamConfig_VTable =
{
    AMStreamConfig_QueryInterface,
    AMStreamConfig_AddRef,
    AMStreamConfig_Release,
    AMStreamConfig_SetFormat,
    AMStreamConfig_GetFormat,
    AMStreamConfig_GetNumberOfCapabilities,
    AMStreamConfig_GetStreamCaps
};

static HRESULT WINAPI
AMVideoProcAmp_QueryInterface( IAMVideoProcAmp * iface, REFIID riid,
                               LPVOID * ppv )
{
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IAMVideoProcAmp))
    {
        *ppv = iface;
        IAMVideoProcAmp_AddRef( iface );
        return S_OK;
    }

    FIXME("No interface for iid %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI AMVideoProcAmp_AddRef(IAMVideoProcAmp * iface)
{
    VfwCapture *This = impl_from_IAMVideoProcAmp(iface);

    return IUnknown_AddRef((IUnknown *)This);
}

static ULONG WINAPI AMVideoProcAmp_Release(IAMVideoProcAmp * iface)
{
    VfwCapture *This = impl_from_IAMVideoProcAmp(iface);

    return IUnknown_Release((IUnknown *)This);
}

static HRESULT WINAPI
AMVideoProcAmp_GetRange( IAMVideoProcAmp * iface, LONG Property, LONG *pMin,
        LONG *pMax, LONG *pSteppingDelta, LONG *pDefault, LONG *pCapsFlags )
{
    VfwCapture *This = impl_from_IAMVideoProcAmp(iface);

    return qcap_driver_get_prop_range( This->driver_info, Property, pMin, pMax,
                   pSteppingDelta, pDefault, pCapsFlags );
}

static HRESULT WINAPI
AMVideoProcAmp_Set( IAMVideoProcAmp * iface, LONG Property, LONG lValue,
                    LONG Flags )
{
    VfwCapture *This = impl_from_IAMVideoProcAmp(iface);

    return qcap_driver_set_prop(This->driver_info, Property, lValue, Flags);
}

static HRESULT WINAPI
AMVideoProcAmp_Get( IAMVideoProcAmp * iface, LONG Property, LONG *lValue,
                    LONG *Flags )
{
    VfwCapture *This = impl_from_IAMVideoProcAmp(iface);

    return qcap_driver_get_prop(This->driver_info, Property, lValue, Flags);
}

static const IAMVideoProcAmpVtbl IAMVideoProcAmp_VTable =
{
    AMVideoProcAmp_QueryInterface,
    AMVideoProcAmp_AddRef,
    AMVideoProcAmp_Release,
    AMVideoProcAmp_GetRange,
    AMVideoProcAmp_Set,
    AMVideoProcAmp_Get,
};

static HRESULT WINAPI
PPB_QueryInterface( IPersistPropertyBag * iface, REFIID riid, LPVOID * ppv )
{
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IPersist) ||
        IsEqualIID(riid, &IID_IPersistPropertyBag))
    {
        IPersistPropertyBag_AddRef(iface);
        *ppv = iface;
        return S_OK;
    }
    if (IsEqualIID(riid, &IID_IBaseFilter))
    {
        /* FIXME: native devenum asks for IBaseFilter, should we return it? */
        IPersistPropertyBag_AddRef(iface);
        *ppv = iface;
        return S_OK;
    }

    FIXME("No interface for iid %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI PPB_AddRef(IPersistPropertyBag * iface)
{
    VfwCapture *This = impl_from_IPersistPropertyBag(iface);

    TRACE("%p --> Forwarding to VfwCapture (%p)\n", iface, This);

    return IUnknown_AddRef((IUnknown *)This);
}

static ULONG WINAPI PPB_Release(IPersistPropertyBag * iface)
{
    VfwCapture *This = impl_from_IPersistPropertyBag(iface);

    TRACE("%p --> Forwarding to VfwCapture (%p)\n", iface, This);

    return IUnknown_Release((IUnknown *)This);
}

static HRESULT WINAPI
PPB_GetClassID( IPersistPropertyBag * iface, CLSID * pClassID )
{
    VfwCapture *This = impl_from_IPersistPropertyBag(iface);

    FIXME("%p - stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI PPB_InitNew(IPersistPropertyBag * iface)
{
    VfwCapture *This = impl_from_IPersistPropertyBag(iface);

    FIXME("%p - stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI
PPB_Load( IPersistPropertyBag * iface, IPropertyBag *pPropBag,
          IErrorLog *pErrorLog )
{
    VfwCapture *This = impl_from_IPersistPropertyBag(iface);
    HRESULT hr;
    VARIANT var;
    const OLECHAR VFWIndex[] = {'V','F','W','I','n','d','e','x',0};

    TRACE("%p/%p-> (%p, %p)\n", iface, This, pPropBag, pErrorLog);

    V_VT(&var) = VT_I4;
    hr = IPropertyBag_Read(pPropBag, VFWIndex, &var, pErrorLog);

    if (SUCCEEDED(hr))
    {
        VfwPinImpl *pin;

        This->driver_info = qcap_driver_init( This->pOutputPin,
               var.__VARIANT_NAME_1.__VARIANT_NAME_2.__VARIANT_NAME_3.ulVal );
        if (This->driver_info)
        {
            pin = (VfwPinImpl *)This->pOutputPin;
            pin->driver_info = This->driver_info;
            pin->parent = This;
            This->init = TRUE;
            hr = S_OK;
        }
        else
            hr = E_FAIL;
    }

    return hr;
}

static HRESULT WINAPI
PPB_Save( IPersistPropertyBag * iface, IPropertyBag *pPropBag,
          BOOL fClearDirty, BOOL fSaveAllProperties )
{
    VfwCapture *This = impl_from_IPersistPropertyBag(iface);
    FIXME("%p - stub\n", This);
    return E_NOTIMPL;
}

static const IPersistPropertyBagVtbl IPersistPropertyBag_VTable =
{
    PPB_QueryInterface,
    PPB_AddRef,
    PPB_Release,
    PPB_GetClassID,
    PPB_InitNew,
    PPB_Load,
    PPB_Save
};

/* IKsPropertySet interface */
static HRESULT WINAPI
KSP_QueryInterface( IKsPropertySet * iface, REFIID riid, LPVOID * ppv )
{
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IKsPropertySet))
    {
        *ppv = iface;
        IKsPropertySet_AddRef( iface );
        return S_OK;
    }

    FIXME("No interface for iid %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI KSP_AddRef(IKsPropertySet * iface)
{
    ICOM_THIS_MULTI(VfwPinImpl, KSP_VT, iface);

    TRACE("%p --> Forwarding to VfwPin (%p)\n", iface, This);

    return IUnknown_AddRef((IUnknown *)This);
}

static ULONG WINAPI KSP_Release(IKsPropertySet * iface)
{
    ICOM_THIS_MULTI(VfwPinImpl, KSP_VT, iface);

    TRACE("%p --> Forwarding to VfwPin (%p)\n", iface, This);

    return IUnknown_Release((IUnknown *)This);
}

static HRESULT WINAPI
KSP_Set( IKsPropertySet * iface, REFGUID guidPropSet, DWORD dwPropID,
         LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData,
         DWORD cbPropData )
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI
KSP_Get( IKsPropertySet * iface, REFGUID guidPropSet, DWORD dwPropID,
         LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData,
         DWORD cbPropData, DWORD *pcbReturned )
{
    LPGUID pGuid;

    TRACE("()\n");

    if (!IsEqualIID(guidPropSet, &AMPROPSETID_Pin))
        return E_PROP_SET_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)
        return E_POINTER;
    if (pcbReturned)
        *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)
        return S_OK;
    if (cbPropData < sizeof(GUID))
        return E_UNEXPECTED;
    pGuid = pPropData;
    *pGuid = PIN_CATEGORY_PREVIEW;
    FIXME("() Not adding a pin with PIN_CATEGORY_CAPTURE\n");
    return S_OK;
}

static HRESULT WINAPI
KSP_QuerySupported( IKsPropertySet * iface, REFGUID guidPropSet,
                    DWORD dwPropID, DWORD *pTypeSupport )
{
   FIXME("%p: stub\n", iface);
   return E_NOTIMPL;
}

static const IKsPropertySetVtbl KSP_VTable =
{
   KSP_QueryInterface,
   KSP_AddRef,
   KSP_Release,
   KSP_Set,
   KSP_Get,
   KSP_QuerySupported
};

static HRESULT WINAPI VfwPin_GetMediaType(BasePin *iface, int iPosition, AM_MEDIA_TYPE *pmt)
{
    VfwPinImpl *This = (VfwPinImpl *)iface;
    AM_MEDIA_TYPE *vfw_pmt;
    HRESULT hr;

    if (iPosition < 0)
        return E_INVALIDARG;
    if (iPosition > 0)
        return VFW_S_NO_MORE_ITEMS;

    hr = qcap_driver_get_format(This->driver_info, &vfw_pmt);
    CopyMediaType(pmt, vfw_pmt);
    DeleteMediaType(vfw_pmt);

    return hr;
}

LONG WINAPI VfwPin_GetMediaTypeVersion(BasePin *iface)
{
    return 1;
}

static HRESULT WINAPI VfwPin_DecideBufferSize(BaseOutputPin *iface, IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest)
{
    ALLOCATOR_PROPERTIES actual;

    /* What we put here doesn't matter, the
       driver function should override it then commit */
    if (!ppropInputRequest->cBuffers)
        ppropInputRequest->cBuffers = 3;
    if (!ppropInputRequest->cbBuffer)
        ppropInputRequest->cbBuffer = 230400;
    if (!ppropInputRequest->cbAlign)
        ppropInputRequest->cbAlign = 1;

    return IMemAllocator_SetProperties(pAlloc, ppropInputRequest, &actual);
}

static const  BasePinFuncTable output_BaseFuncTable = {
    NULL,
    BaseOutputPinImpl_AttemptConnection,
    VfwPin_GetMediaTypeVersion,
    VfwPin_GetMediaType
};

static const BaseOutputPinFuncTable output_BaseOutputFuncTable = {
    VfwPin_DecideBufferSize,
    BaseOutputPinImpl_DecideAllocator,
    BaseOutputPinImpl_BreakConnect
};

static HRESULT
VfwPin_Construct( IBaseFilter * pBaseFilter, LPCRITICAL_SECTION pCritSec,
                  IPin ** ppPin )
{
    static const WCHAR wszOutputPinName[] = { 'O','u','t','p','u','t',0 };
    PIN_INFO piOutput;
    HRESULT hr;

    ppPin = NULL;

    piOutput.dir = PINDIR_OUTPUT;
    piOutput.pFilter = pBaseFilter;
    lstrcpyW(piOutput.achName, wszOutputPinName);
    ObjectRefCount(TRUE);

    hr = BaseOutputPin_Construct(&VfwPin_Vtbl, sizeof(VfwPinImpl), &piOutput, &output_BaseFuncTable, &output_BaseOutputFuncTable, pCritSec, ppPin);

    if (SUCCEEDED(hr))
    {
        VfwPinImpl *pPinImpl = (VfwPinImpl*)*ppPin;
        pPinImpl->KSP_VT = &KSP_VTable;
    }

    return hr;
}

static HRESULT WINAPI VfwPin_QueryInterface(IPin * iface, REFIID riid, LPVOID * ppv)
{
    VfwPinImpl *This = (VfwPinImpl *)iface;

    TRACE("%s %p\n", debugstr_guid(riid), ppv);

    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPin))
        *ppv = This;
    else if (IsEqualIID(riid, &IID_IKsPropertySet))
        *ppv = &(This->KSP_VT);
    else if (IsEqualIID(riid, &IID_IAMStreamConfig))
        return IUnknown_QueryInterface((IUnknown *)This->parent, riid, ppv);

    if (*ppv)
    {
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s!\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI
VfwPin_Release(IPin * iface)
{
   VfwPinImpl *This = (VfwPinImpl *)iface;
   ULONG refCount = InterlockedDecrement(&This->pin.pin.refCount);

   TRACE("() -> new refcount: %u\n", refCount);

   if (!refCount)
   {
      CoTaskMemFree(This);
      ObjectRefCount(FALSE);
   }
   return refCount;
}

static HRESULT WINAPI
VfwPin_EnumMediaTypes(IPin * iface, IEnumMediaTypes ** ppEnum)
{
    AM_MEDIA_TYPE *pmt;
    HRESULT hr;

    VfwPinImpl *This = (VfwPinImpl *)iface;
    hr = qcap_driver_get_format(This->driver_info, &pmt);
    if (SUCCEEDED(hr))
        hr = BasePinImpl_EnumMediaTypes(iface, ppEnum);
    TRACE("%p -- %x\n", This, hr);
    DeleteMediaType(pmt);

    return hr;
}

static HRESULT WINAPI
VfwPin_QueryInternalConnections(IPin * iface, IPin ** apPin, ULONG * cPin)
{
    TRACE("(%p)->(%p, %p)\n", iface, apPin, cPin);
    return E_NOTIMPL;
}

static const IPinVtbl VfwPin_Vtbl =
{
    VfwPin_QueryInterface,
    BasePinImpl_AddRef,
    VfwPin_Release,
    BaseOutputPinImpl_Connect,
    BaseOutputPinImpl_ReceiveConnection,
    BaseOutputPinImpl_Disconnect,
    BasePinImpl_ConnectedTo,
    BasePinImpl_ConnectionMediaType,
    BasePinImpl_QueryPinInfo,
    BasePinImpl_QueryDirection,
    BasePinImpl_QueryId,
    BasePinImpl_QueryAccept,
    VfwPin_EnumMediaTypes,
    VfwPin_QueryInternalConnections,
    BaseOutputPinImpl_EndOfStream,
    BaseOutputPinImpl_BeginFlush,
    BaseOutputPinImpl_EndFlush,
    BasePinImpl_NewSegment
};

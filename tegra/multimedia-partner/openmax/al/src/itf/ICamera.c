/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Camera implementation */

#include "linux_cameradevice.h"

//***************************************************************************
// ICamera_RegisterCallback
//***************************************************************************
static XAresult
ICamera_RegisterCallback(
    XACameraItf self,
    xaCameraCallback callback,
    void *pContext)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceRegisterCallback(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        callback, pContext);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetFlashMode
//***************************************************************************
static XAresult
ICamera_SetFlashMode(
    XACameraItf self,
    XAuint32 flashMode)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetFlashMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), flashMode);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetFlashMode
//***************************************************************************
static XAresult
ICamera_GetFlashMode(
    XACameraItf self,
    XAuint32 *pFlashMode)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pFlashMode);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetFlashMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), pFlashMode);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_IsFlashReady
//***************************************************************************
static XAresult
ICamera_IsFlashReady(
    XACameraItf self,
    XAboolean *pbReady)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pbReady);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceIsFlashReady(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), pbReady);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetFocusMode
//***************************************************************************
static XAresult
ICamera_SetFocusMode(
    XACameraItf self,
    XAuint32 focusMode,
    XAmillimeter manualSetting,
    XAboolean bMacroEnabled)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetFocusMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        focusMode, manualSetting, bMacroEnabled);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetFocusMode
//***************************************************************************
static XAresult
ICamera_GetFocusMode(
    XACameraItf self,
    XAuint32 *pFocusMode,
    XAmillimeter *pManualSetting,
    XAboolean *pbMacroEnabled)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pFocusMode &&
        pManualSetting && pbMacroEnabled);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetFocusMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        pFocusMode, pManualSetting, pbMacroEnabled);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetFocusRegionPattern
//***************************************************************************
static XAresult
ICamera_SetFocusRegionPattern(
    XACameraItf self,
    XAuint32 focusPattern,
    XAuint32 activePoints1,
    XAuint32 activePoints2)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetFocusRegionPattern
//***************************************************************************
static XAresult
ICamera_GetFocusRegionPattern(
    XACameraItf self,
    XAuint32 *pFocusPattern,
    XAuint32 *pActivePoints1,
    XAuint32 *pActivePoints2)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetFocusRegionPositions
//***************************************************************************
static XAresult
ICamera_GetFocusRegionPositions(
    XACameraItf self,
    XAuint32 *pNumPositionEntries,
    XAFocusPointPosition *pFocusPosition)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetFocusModeStatus
//***************************************************************************
static XAresult
ICamera_GetFocusModeStatus(
    XACameraItf self,
    XAuint32 *pFocusStatus,
    XAuint32 *pRegionStatus1,
    XAuint32 *pRegionStatus2)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetMeteringMode
//***************************************************************************
static XAresult
ICamera_SetMeteringMode(
    XACameraItf self,
    XAuint32 meteringMode)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetMeteringMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), meteringMode);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetMeteringMode
//***************************************************************************
static XAresult
ICamera_GetMeteringMode(
    XACameraItf self,
    XAuint32 *pMeteringMode)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pMeteringMode);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetMeteringMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), pMeteringMode);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetExposureMode
//***************************************************************************
static XAresult
ICamera_SetExposureMode(
    XACameraItf self,
    XAuint32 exposureMode,
    XAuint32 compensation)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetExposureMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        exposureMode, compensation);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetExposureMode
//***************************************************************************
static XAresult
ICamera_GetExposureMode(
    XACameraItf self,
    XAuint32 *pExposureMode,
    XAuint32 *pCompensation)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pExposureMode &&
        pCompensation);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetExposureMode(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        pExposureMode, pCompensation);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetISOSensitivity
//***************************************************************************
static XAresult
ICamera_SetISOSensitivity(
    XACameraItf self,
    XAuint32 isoSensitivity,
    XAuint32 manualSetting)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetISOSensitivity(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        isoSensitivity, manualSetting);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetISOSensitivity
//***************************************************************************
static XAresult
ICamera_GetISOSensitivity(
    XACameraItf self,
    XAuint32 *pIsoSensitivity,
    XAuint32 *pManualSetting)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pIsoSensitivity && pManualSetting);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetISOSensitivity(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        pIsoSensitivity, pManualSetting);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetAperture
//***************************************************************************
static XAresult
ICamera_SetAperture(
    XACameraItf self,
    XAuint32 aperture,
    XAuint32 manualSetting)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetAperture
//***************************************************************************
static XAresult
ICamera_GetAperture(
    XACameraItf self,
    XAuint32 *pAperture,
    XAuint32 *pManualSetting)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetShutterSpeed
//***************************************************************************
static XAresult
ICamera_SetShutterSpeed(
    XACameraItf self,
    XAuint32 shutterSpeed,
    XAmicrosecond manualSetting)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetShutterSpeed(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        shutterSpeed, manualSetting);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetShutterSpeed
//***************************************************************************
static XAresult
ICamera_GetShutterSpeed(
    XACameraItf self,
    XAuint32 *pShutterSpeed,
    XAmicrosecond *pManualSetting)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pShutterSpeed && pManualSetting);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetShutterSpeed(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        pShutterSpeed, pManualSetting);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetWhiteBalance
//***************************************************************************
static XAresult
ICamera_SetWhiteBalance(
    XACameraItf self,
    XAuint32 whiteBalance,
    XAuint32 manualSetting)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetWhiteBalance(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        whiteBalance, manualSetting);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetWhiteBalance
//***************************************************************************
static XAresult
ICamera_GetWhiteBalance(
    XACameraItf self,
    XAuint32 *pWhiteBalance,
    XAuint32 *pManualSetting)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pWhiteBalance && pManualSetting);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetWhiteBalance(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        pWhiteBalance, pManualSetting);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetAutoLocks
//***************************************************************************
static XAresult
ICamera_SetAutoLocks(
    XACameraItf self,
    XAuint32 locks)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetAutoLocks(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), locks);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetAutoLocks
//***************************************************************************
static XAresult
ICamera_GetAutoLocks(
    XACameraItf self,
    XAuint32 *pLocks)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pLocks);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetAutoLocks(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)), pLocks);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_SetZoom
//***************************************************************************
static XAresult
ICamera_SetZoom(
    XACameraItf self,
    XApermille zoom,
    XAboolean bDigitalEnabled,
    XAuint32 speed,
    XAboolean bAsync)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceSetZoom(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        zoom, bDigitalEnabled, speed, bAsync);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_GetZoom
//***************************************************************************
static XAresult
ICamera_GetZoom(
    XACameraItf self,
    XApermille *pZoom,
    XAboolean *pbDigital)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCamDevice && pZoom && pbDigital);

    interface_lock_poke(pCamDevice);
    result = ALBackendCameraDeviceGetZoom(
        ((CCameraDevice *)InterfaceToIObject(pCamDevice)),
        pZoom, pbDigital);
    interface_unlock_poke(pCamDevice);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICamera_Itf
//***************************************************************************
static const struct XACameraItf_ ICamera_Itf =
{
    ICamera_RegisterCallback,
    ICamera_SetFlashMode,
    ICamera_GetFlashMode,
    ICamera_IsFlashReady,
    ICamera_SetFocusMode,
    ICamera_GetFocusMode,
    ICamera_SetFocusRegionPattern,
    ICamera_GetFocusRegionPattern,
    ICamera_GetFocusRegionPositions,
    ICamera_GetFocusModeStatus,
    ICamera_SetMeteringMode,
    ICamera_GetMeteringMode,
    ICamera_SetExposureMode,
    ICamera_GetExposureMode,
    ICamera_SetISOSensitivity,
    ICamera_GetISOSensitivity,
    ICamera_SetAperture,
    ICamera_GetAperture,
    ICamera_SetShutterSpeed,
    ICamera_GetShutterSpeed,
    ICamera_SetWhiteBalance,
    ICamera_GetWhiteBalance,
    ICamera_SetAutoLocks,
    ICamera_GetAutoLocks,
    ICamera_SetZoom,
    ICamera_GetZoom
};

//***************************************************************************
// ICamera_init
//***************************************************************************
void ICameraDevice_init(void *self)
{
    ICameraDevice *pCamDevice = (ICameraDevice *)self;

    XA_ENTER_INTERFACE_VOID;

    if (pCamDevice)
    {
        pCamDevice->mItf = &ICamera_Itf;
    }

    XA_LEAVE_INTERFACE_VOID;
}

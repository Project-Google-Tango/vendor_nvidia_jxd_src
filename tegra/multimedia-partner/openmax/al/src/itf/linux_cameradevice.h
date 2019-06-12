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

#ifndef OMXAL_LINUX_CAMERADEVICE_H
#define OMXAL_LINUX_CAMERADEVICE_H

#include "linux_common.h"
#include "NVOMXAL_CameraExtensions.h"
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/descriptions.h>

typedef struct
{
    GstElement *pCamerabin;
    GstElement *pCameraSrc;
    GstElement *pFakeSink;
    xaCameraCallback mCallback;
    void *pContext;
    XAuint32 mLocks;
} CameraDeviceData;

typedef struct
{
    // Unique ID of the camera
    XAuint32 id;
    // Camera Descriptor
    XACameraDescriptor mDescriptor;
    // Number of Supported AutoLocks Settings
    XAuint32 mNumAutoLocksSettings;
    // Represents Array of Supported AutoLocks Settings
    XAuint32 *pAutoLocksSettings;
    // Number of Zoom Settings
    XAuint32 mNumZoomSettings;
    // Represents Array of Zoom settings
    XApermille *pZoomSettings;
    // Number of ISO Manual Settings
    XAuint32 mNumISOManualSettings;
    // Represents Array of ISO Manual Settings
    XAuint32 *pISOManualSettings;
} CameraDescriptor;

typedef struct
{
    // Number of Camera Devices Present
    XAuint32 mNumCameraDevices;
    // Array of CameraDescriptor representing each camera properties
    CameraDescriptor *pDescriptorList;
    // Signifies Default Camera Device ID
    XAuint32 mDefaultCameraDeviceID;
    // Index of the default Camera Device ID
    XAuint32 mDefaultCameraDeviceIDIndex;
} CameraCapabilitiesData;

typedef struct CamResRec
{
    NVXACameraResolutionDescriptor *pResDescs;
    XAuint32 NumRes;
} CamRes;

/***************************************************************************
 * CCameraDevice
 ****************************/
extern XAresult
ALBackendCameraDeviceCreate(
    CCameraDevice *pCameraDevice);

extern XAresult
ALBackendCameraDeviceRealize(
    CCameraDevice *pCameraDevice,
    XAboolean async);

extern XAresult
ALBackendCameraDeviceResume(
    CCameraDevice *pCameraDevice,
    XAboolean async);

extern void
ALBackendCameraDeviceDestroy(
    CCameraDevice *pCameraDevice);

/*****************************
 * ICamera
 ****************************/
extern XAresult
ALBackendCameraDeviceRegisterCallback(
    CCameraDevice *pCameraDevice,
    xaCameraCallback callback,
    void *pContext);

extern XAresult
ALBackendCameraDeviceSetFlashMode(
    CCameraDevice *pCameraDevice,
    XAuint32 flashMode);

extern  XAresult
ALBackendCameraDeviceGetFlashMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pFlashMode);

extern XAresult
ALBackendCameraDeviceIsFlashReady(
    CCameraDevice *pCameraDevice,
    XAboolean *pReady);

extern XAresult
ALBackendCameraDeviceSetFocusMode(
    CCameraDevice *pCameraDevice,
    XAuint32  focusMode,
    XAmillimeter  manualSetting,
    XAboolean macroEnabled);

extern XAresult
ALBackendCameraDeviceGetFocusMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pFocusMode,
    XAmillimeter *pManualSetting,
    XAboolean *pMacroEnabled);

extern XAresult
ALBackendCameraDeviceSetFocusRegionPattern(
    CCameraDevice *pCameraDevice,
    XAuint32 focusPattern,
    XAuint32 activePoints1,
    XAuint32 activePoints2);

extern XAresult
ALBackendCameraDeviceGetFocusRegionPattern(
    CCameraDevice *pCameraDevice,
    XAuint32 *pFocusPattern,
    XAuint32 *pActivePoints1,
    XAuint32 *pActivePoints2);

extern XAresult
ALBackendCameraDeviceSetMeteringMode(
    CCameraDevice *pCameraDevice,
    XAuint32 meteringMode);

extern XAresult
ALBackendCameraDeviceGetMeteringMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pMeteringMode);

extern XAresult
ALBackendCameraDeviceSetExposureMode(
    CCameraDevice *pCameraDevice,
    XAuint32 exposureMode,
    XAuint32 compensation);

extern XAresult
ALBackendCameraDeviceGetExposureMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pExposureMode,
    XAuint32 *pCompensation);

extern XAresult
ALBackendCameraDeviceSetISOSensitivity(
    CCameraDevice *pCameraDevice,
    XAuint32 isoSensitivity,
    XAuint32 manualSetting);

extern XAresult
ALBackendCameraDeviceGetISOSensitivity(
    CCameraDevice *pCameraDevice,
    XAuint32 *pIsoSensitivity,
    XAuint32 *pManualSetting);

extern XAresult
ALBackendCameraDeviceSetShutterSpeed(
    CCameraDevice *pCameraDevice,
    XAuint32 shutterSpeed,
    XAmicrosecond manualSetting);

extern XAresult
ALBackendCameraDeviceGetShutterSpeed(
    CCameraDevice *pCameraDevice,
    XAuint32 *pShutterSpeed,
    XAmicrosecond *pManualSetting);

extern XAresult
ALBackendCameraDeviceGetWhiteBalance(
    CCameraDevice *pCameraDevice,
    XAuint32 *pWhiteBalance,
    XAuint32 *pManualSetting);

extern XAresult
ALBackendCameraDeviceSetWhiteBalance(
    CCameraDevice *pCameraDevice,
    XAuint32 whiteBalance,
    XAuint32 manualSetting);

extern XAresult
ALBackendCameraDeviceGetAutoLocks(
    CCameraDevice *pCameraDevice,
    XAuint32 *pLocks);

extern XAresult
ALBackendCameraDeviceSetAutoLocks(
    CCameraDevice *pCameraDevice,
    XAuint32  locks);

extern XAresult
ALBackendCameraDeviceSetZoom(
    CCameraDevice *pCameraDevice,
    XApermille zoom,
    XAboolean digitalEnabled,
    XAuint32 speed,
    XAboolean async);

extern XAresult
ALBackendCameraDeviceGetZoom(
    CCameraDevice *pCameraDevice,
    XApermille *pZoom,
    XAboolean *pDigital);

extern XAresult
ALBackendCameraDeviceSetBrightness(
    CCameraDevice *pCameraDevice,
    XAuint32 brightness);

extern XAresult
ALBackendCameraDeviceGetBrightness(
    CCameraDevice *pCameraDevice,
    XAuint32 *pBrightness);

extern XAresult
ALBackendCameraDeviceSetContrast(
    CCameraDevice *pCameraDevice,
    XAint32 contrast);

extern XAresult
ALBackendCameraDeviceGetContrast(
    CCameraDevice *pCameraDevice,
    XAint32 *pContrast);

extern XAresult
ALBackendCameraDeviceSetGamma(
    CCameraDevice *pCameraDevice,
    XApermille gamma);

extern XAresult
ALBackendCameraDeviceGetGamma(
    CCameraDevice *pCameraDevice,
    XApermille *pGamma);

extern XAresult
ALBackendCameraDeviceGetSupportedGammaSettings(
    CCameraDevice *pCameraDevice,
    XApermille *pMinValue,
    XApermille *pMaxValue);

extern gboolean
CameraDeviceAutoLocksCallback(
    CCameraDevice *pCameraDevice);

#endif

/** @} */
/* File EOF */


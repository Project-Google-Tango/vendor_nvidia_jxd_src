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

/* ImageControls implementation */

#include "linux_cameradevice.h"
#include "linux_mediaplayer.h"

static XAresult
IImageControls_SetBrightness(
    XAImageControlsItf self,
    XAuint32 brightness)
{
    XAuint32 objectID = 0;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls)
    //brightness should be in range from 0 to 100
    AL_CHK_ARG((brightness >= 0) && (brightness <= 100))

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerSetBrightness(
                pCMediaPlayer,
                brightness);
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceSetBrightness(
                pCameraDevice,
                brightness);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageControls_GetBrightness(
    XAImageControlsItf self,
    XAuint32 *pBrightness)
{
    XAuint32 objectID = 0;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls && pBrightness)

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerGetBrightness(
                pCMediaPlayer,
                pBrightness);
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceGetBrightness(
                pCameraDevice,
                pBrightness);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageControls_SetContrast(
    XAImageControlsItf self,
    XAint32 contrast)
{
    XAuint32 objectID = 0;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls)
    //contrast should be in range from -100 to 100
    AL_CHK_ARG((contrast >= -100) && (contrast <= 100))

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerSetContrast(
                pCMediaPlayer,
                contrast);
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceSetContrast(
                pCameraDevice,
                contrast);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageControls_GetContrast(
    XAImageControlsItf self,
    XAint32 *pContrast)
{
    XAuint32 objectID = 0;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls && pContrast)

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerGetContrast(
                pCMediaPlayer,
                pContrast);
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceGetContrast(
                pCameraDevice,
                pContrast);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageControls_SetGamma(
    XAImageControlsItf self,
    XApermille gamma)
{
    XAuint32 objectID = 0;
    XApermille minValue, maxValue;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls)

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerGetSupportedGammaSettings(
                pCMediaPlayer, &minValue, &maxValue);
            if (result == XA_RESULT_SUCCESS)
            {
                if ((gamma >= minValue) && (gamma <= maxValue))
                {
                    result = ALBackendMediaPlayerSetGamma(
                        pCMediaPlayer,
                        gamma);
                }
                else
                {
                    result = XA_RESULT_PARAMETER_INVALID;
                }
            }
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceGetSupportedGammaSettings(
                pCameraDevice, &minValue, &maxValue);
            if (result == XA_RESULT_SUCCESS)
            {
                if ((gamma >= minValue) && (gamma <= maxValue))
                {
                    result = ALBackendCameraDeviceSetGamma(
                        pCameraDevice,
                        gamma);
                }
                else
                {
                    result = XA_RESULT_PARAMETER_INVALID;
                }
            }
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageControls_GetGamma(
    XAImageControlsItf self,
    XApermille *pGamma)
{
    XAuint32 objectID = 0;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls && pGamma)

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerGetGamma(
                pCMediaPlayer,
                pGamma);
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceGetGamma(
                pCameraDevice,
                pGamma);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageControls_GetSupportedGammaSettings(
    XAImageControlsItf self,
    XApermille *pMinValue,
    XApermille *pMaxValue,
    XAuint32 *pNumSettings,
    XApermille **ppSettings)
{
    XAuint32 objectID = 0;
    CMediaPlayer *pCMediaPlayer = NULL;
    CCameraDevice *pCameraDevice = NULL;
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageControls && pMinValue && pMaxValue && pNumSettings)

    //supporting continous range, ppSettings can be ignored
    *pNumSettings = 0;

    objectID = InterfaceToObjectID(pImageControls);
    interface_lock_poke(pImageControls);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pImageControls);
            result = ALBackendMediaPlayerGetSupportedGammaSettings(
                pCMediaPlayer, pMinValue, pMaxValue);
            break;
        }
        case XA_OBJECTID_CAMERADEVICE:
        {
            pCameraDevice = (CCameraDevice *)InterfaceToIObject(pImageControls);
            result = ALBackendCameraDeviceGetSupportedGammaSettings(
                pCameraDevice, pMinValue, pMaxValue);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pImageControls);

    XA_LEAVE_INTERFACE
}

static const struct XAImageControlsItf_ IImageControls_Itf =
{
    IImageControls_SetBrightness,
    IImageControls_GetBrightness,
    IImageControls_SetContrast,
    IImageControls_GetContrast,
    IImageControls_SetGamma,
    IImageControls_GetGamma,
    IImageControls_GetSupportedGammaSettings
};

void IImageControls_init(void *self)
{
    IImageControls *pImageControls = (IImageControls *)self;

    XA_ENTER_INTERFACE_VOID
    if (pImageControls)
    {
        pImageControls->mItf = &IImageControls_Itf;
        pImageControls->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}

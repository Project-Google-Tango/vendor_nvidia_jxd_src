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

// Linux implementation of OpenMAX AL interfaces
#include "linux_cameradevice.h"
#include <gst/interfaces/photography.h>

gboolean
CameraDeviceAutoLocksCallback(
    CCameraDevice *pCameraDevice)
{
    CameraDeviceData *pCamData = NULL;
    XACameraItf ICameraItf;

    XA_ENTER_INTERFACE_VOID;
    if (!(pCameraDevice && pCameraDevice->pData))
        return XA_BOOLEAN_FALSE;

    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    ICameraItf = (XACameraItf)pCameraDevice->mCamera.mItf;
    if (!pCamData)
        return XA_BOOLEAN_FALSE;

    if (pCamData->mCallback)
    {
        XAuint32 eventId = 0;
        eventId = (XA_CAMERACBEVENT_FOCUSSTATUS |
            XA_CAMERACBEVENT_EXPOSURESTATUS |
            XA_CAMERACBEVENT_WHITEBALANCELOCKED);
        pCamData->mCallback(
            ICameraItf,
            pCamData->pContext,
            eventId,
            XA_CAMERA_FOCUSMODESTATUS_REACHED);
    }

    return XA_BOOLEAN_TRUE;
}

XAresult
ALBackendCameraDeviceCreate(
    CCameraDevice *pCameraDevice)
{
    CameraDeviceData *pCamData = NULL;
    GstCaps *pCameraCaps = NULL;
    GstEncodingContainerProfile *pCprof = NULL;
    GstEncodingVideoProfile *pVprof = NULL;
    GstEncodingAudioProfile *pAprof = NULL;
    gint flags = 0;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCameraDevice);

    InitializeFramework();

    pCamData = (CameraDeviceData *)malloc(sizeof(CameraDeviceData));
    // Check for memory failure
    if (!pCamData)
    {
        result = XA_RESULT_MEMORY_FAILURE;
        XA_LEAVE_INTERFACE;
    }
    memset(pCamData, 0, sizeof(CameraDeviceData));

    // Create Camerabin2 element
    pCamData->pCamerabin = gst_element_factory_make(
        "camerabin2", "OMXALCamerabin2");
    if (!pCamData->pCamerabin)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create Camara element for Camerabin2
    pCamData->pCameraSrc = gst_element_factory_make(
        "nv_omx_camera2", "OMXALCameraSource");
    if (!pCamData->pCameraSrc)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    pCamData->pFakeSink = gst_element_factory_make(
        "fakesink", "OMXALFakesink");
    if (!pCamData->pFakeSink)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //set omx camera component as source for video
    g_object_set(
        G_OBJECT(pCamData->pCamerabin),
        "camera-source",
        pCamData->pCameraSrc,
        NULL);

    //set fakesink as viewfinder sink element. App will configure CMediaPlayer
    //to set viewfinder-sink element in case needed.
    g_object_set(
        G_OBJECT(pCamData->pCamerabin),
        "viewfinder-sink",
        pCamData->pFakeSink,
        NULL);

    // Store the data back to the Camera Device
    pCameraDevice->pData = pCamData;

    flags = (0x00000001 | 0x00000002 | 0x00000004 | 0x00000008);
    g_object_set(G_OBJECT(pCamData->pCamerabin), "flags", flags, NULL);

    pCameraCaps = gst_caps_new_simple("video/quicktime",
        "variant", G_TYPE_STRING, "iso", NULL);
    pCprof = gst_encoding_container_profile_new("mp4",
        NULL, pCameraCaps, NULL);
    gst_caps_unref(pCameraCaps);

    pCameraCaps = gst_caps_new_simple("video/mpeg",
        "mpegversion", G_TYPE_INT, 4, NULL);
    pVprof = gst_encoding_video_profile_new(pCameraCaps,
        NULL, NULL, 0);
    gst_caps_unref(pCameraCaps);
    gst_encoding_video_profile_set_variableframerate(pVprof, TRUE);
    if (!gst_encoding_container_profile_add_profile(pCprof,
        (GstEncodingProfile *)pVprof))
    {
        XA_LOGE("Failed to create encoding profiles");
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    pCameraCaps = gst_caps_new_simple("audio/mpeg",
        "mpegversion", G_TYPE_INT, 4, NULL);
    pAprof = gst_encoding_audio_profile_new(pCameraCaps,
        NULL, NULL, 1);
    gst_caps_unref(pCameraCaps);
    if (!gst_encoding_container_profile_add_profile(pCprof,
        (GstEncodingProfile *)pAprof))
    {
        XA_LOGE("Failed to create encoding profiles");
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }
    g_object_set(G_OBJECT(pCamData->pCamerabin),
        "video-profile", pCprof, NULL);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceRealize(
    CCameraDevice *pCameraDevice,
    XAboolean async)
{
    XA_ENTER_INTERFACE;
    XA_LEAVE_INTERFACE;
}

void
ALBackendCameraDeviceDestroy(
    CCameraDevice *pCameraDevice)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE_VOID;
    if (pCameraDevice && pCameraDevice->pData)
    {
        pCamData = (CameraDeviceData *)pCameraDevice->pData;

        if (pCamData->pCameraSrc)
            g_object_unref(pCamData->pCameraSrc);
        if (pCamData->pFakeSink)
            g_object_unref(pCamData->pFakeSink);
        if (pCamData->pCamerabin)
            g_object_unref(pCamData->pCamerabin);

        free(pCamData);
        pCameraDevice->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID;
}

XAresult
ALBackendCameraDeviceResume(
    CCameraDevice *pCameraDevice,
    XAboolean async)
{
    XA_ENTER_INTERFACE;
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceRegisterCallback(
    CCameraDevice *pCameraDevice,
    xaCameraCallback callback,
    void *pContext)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);

    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    pCamData->mCallback = callback;
    pCamData->pContext = pContext;

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetFlashMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pFlashMode)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData && pFlashMode);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstFlashMode gstFlashMode;
        if (!gst_photography_get_flash_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &gstFlashMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        switch (gstFlashMode)
        {
            case GST_PHOTOGRAPHY_FLASH_MODE_OFF:
                *pFlashMode = XA_CAMERA_FLASHMODE_OFF;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_ON:
                *pFlashMode = XA_CAMERA_FLASHMODE_ON;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_AUTO:
                *pFlashMode = XA_CAMERA_FLASHMODE_AUTO;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_RED_EYE:
                *pFlashMode = XA_CAMERA_FLASHMODE_REDEYEREDUCTION;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_FILL_IN:
                *pFlashMode = XA_CAMERA_FLASHMODE_FILLIN;
                break;
            default:
                *pFlashMode = 0;
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint flashMode = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "flash-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "flash-mode", &flashMode, NULL);

        switch (flashMode)
        {
            case GST_PHOTOGRAPHY_FLASH_MODE_AUTO:
                *pFlashMode = XA_CAMERA_FLASHMODE_AUTO;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_OFF:
                *pFlashMode = XA_CAMERA_FLASHMODE_OFF;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_ON:
                *pFlashMode = XA_CAMERA_FLASHMODE_ON;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_FILL_IN:
                *pFlashMode = XA_CAMERA_FLASHMODE_FILLIN;
                break;
            case GST_PHOTOGRAPHY_FLASH_MODE_RED_EYE:
                *pFlashMode = XA_CAMERA_FLASHMODE_REDEYEREDUCTION;
                break;
            default:
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceIsFlashReady(
    CCameraDevice *pCameraDevice,
    XAboolean *pReady)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetFlashMode(
    CCameraDevice *pCameraDevice,
    XAuint32 flashMode)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstFlashMode gstFlashMode = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
        switch (flashMode)
        {
            case XA_CAMERA_FLASHMODE_OFF:
                gstFlashMode = GST_PHOTOGRAPHY_FLASH_MODE_OFF;
                break;
            case XA_CAMERA_FLASHMODE_ON:
                gstFlashMode = GST_PHOTOGRAPHY_FLASH_MODE_ON;
                break;
            case XA_CAMERA_FLASHMODE_AUTO:
                gstFlashMode = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
                break;
            case XA_CAMERA_FLASHMODE_FILLIN:
                gstFlashMode = GST_PHOTOGRAPHY_FLASH_MODE_FILL_IN;
                break;
            case XA_CAMERA_FLASHMODE_REDEYEREDUCTION:
            case XA_CAMERA_FLASHMODE_REDEYEREDUCTION_AUTO:
                gstFlashMode = GST_PHOTOGRAPHY_FLASH_MODE_RED_EYE;
                break;
            default:
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }

        if (!gst_photography_set_flash_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), gstFlashMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint flashModeValue = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "flash-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        switch (flashMode)
        {
            case XA_CAMERA_FLASHMODE_OFF:
                flashModeValue = GST_PHOTOGRAPHY_FLASH_MODE_OFF;
                break;
            case XA_CAMERA_FLASHMODE_ON:
                flashModeValue = GST_PHOTOGRAPHY_FLASH_MODE_ON;
                break;
            case XA_CAMERA_FLASHMODE_AUTO:
                flashModeValue = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
                break;
            case XA_CAMERA_FLASHMODE_FILLIN:
                flashModeValue = GST_PHOTOGRAPHY_FLASH_MODE_FILL_IN;
                break;
            case XA_CAMERA_FLASHMODE_REDEYEREDUCTION:
            case XA_CAMERA_FLASHMODE_REDEYEREDUCTION_AUTO:
                flashModeValue = GST_PHOTOGRAPHY_FLASH_MODE_RED_EYE;
                break;
            default:
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "flash-mode", flashModeValue, NULL);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetFocusMode(
    CCameraDevice *pCameraDevice,
    XAuint32  focusMode,
    XAmillimeter manualSetting,
    XAboolean macroEnabled)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstFocusMode gstFocusMode = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
        switch (focusMode)
        {
            case XA_CAMERA_FOCUSMODE_AUTO:
                gstFocusMode = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
                break;
            default:
                XA_LOGE("Unsupported focus mode\n");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }
        if (!gst_photography_set_focus_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), gstFocusMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint focusModeValue = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "focus-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        switch (focusMode)
        {
            case XA_CAMERA_FOCUSMODE_AUTO:
                focusModeValue = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
                break;
            default:
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "focus-mode", focusModeValue, NULL);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetFocusMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pFocusMode,
    XAmillimeter *pManualSetting,
    XAboolean *pMacroEnabled)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    AL_CHK_ARG(pFocusMode && pManualSetting && pMacroEnabled)
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    // Macro mode is not supported
    *pMacroEnabled = XA_BOOLEAN_FALSE;
    // Manual settings are not supported
    *pManualSetting = 0;

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstFocusMode gstFocusMode = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
        if (!gst_photography_get_focus_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &gstFocusMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        switch (gstFocusMode)
        {
            case GST_PHOTOGRAPHY_FOCUS_MODE_AUTO:
                *pFocusMode = XA_CAMERA_FOCUSMODE_AUTO;
                break;
            default:
                XA_LOGE("Unsupported focus mode\n");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint focusModeValue = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "focus-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "focus-mode", &focusModeValue, NULL);
        switch (focusModeValue)
        {
            case GST_PHOTOGRAPHY_FOCUS_MODE_AUTO:
                *pFocusMode = XA_CAMERA_FLASHMODE_AUTO;
                break;
            default:
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }

    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetFocusRegionPattern(
    CCameraDevice *pCameraDevice,
    XAuint32 focusPattern,
    XAuint32 activePoints1,
    XAuint32 activePoints2)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetFocusRegionPattern(
    CCameraDevice *pCameraDevice,
    XAuint32 * pFocusPattern,
    XAuint32 * pActivePoints1,
    XAuint32 * pActivePoints2)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetMeteringMode(
    CCameraDevice *pCameraDevice,
    XAuint32 meteringMode)
{
    CameraDeviceData *pCamData = NULL;
    guint meteringValue = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (!g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "metering-mode"))
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    switch (meteringMode)
    {
        case XA_CAMERA_METERINGMODE_AVERAGE:
            meteringValue = 0;
            break;
        case XA_CAMERA_METERINGMODE_SPOT:
            meteringValue = 1;
            break;
        case XA_CAMERA_METERINGMODE_MATRIX:
            meteringValue = 2;
            break;
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
    }
    g_object_set(
        G_OBJECT(pCamData->pCameraSrc), "metering-mode", meteringValue, NULL);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetMeteringMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pMeteringMode)
{
    CameraDeviceData *pCamData = NULL;
    guint meteringValue = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData && pMeteringMode);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (!g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "metering-mode"))
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }
    g_object_get(
        G_OBJECT(pCamData->pCameraSrc), "metering-mode", &meteringValue, NULL);
    switch (meteringValue)
    {
        case 0:
            *pMeteringMode = XA_CAMERA_METERINGMODE_AVERAGE;
            break;
        case 1:
            *pMeteringMode = XA_CAMERA_METERINGMODE_SPOT;
            break;
        case 2:
            *pMeteringMode = XA_CAMERA_METERINGMODE_MATRIX;
            break;
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetExposureMode(
    CCameraDevice *pCameraDevice,
    XAuint32 exposureMode,
    XAuint32 compensation)
{
    CameraDeviceData *pCamData = NULL;
    GstSceneMode gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        switch (exposureMode)
        {
            case XA_CAMERA_EXPOSUREMODE_AUTO:
                gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_AUTO;
                break;
            case XA_CAMERA_EXPOSUREMODE_MANUAL:
                gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
                break;
            case XA_CAMERA_EXPOSUREMODE_PORTRAIT:
                gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT;
                break;
            case XA_CAMERA_EXPOSUREMODE_NIGHT:
                gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT;
                break;
            case XA_CAMERA_EXPOSUREMODE_SPORTS:
                gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_SPORT;
                break;
            default:
                XA_LOGE("Unmapped exposure mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }

        if (!gst_photography_set_scene_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), gstSceneMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint sceneModeValue = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "scene-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        switch (exposureMode)
        {
            case XA_CAMERA_EXPOSUREMODE_AUTO:
                sceneModeValue = GST_PHOTOGRAPHY_SCENE_MODE_AUTO;
                break;
            case XA_CAMERA_EXPOSUREMODE_MANUAL:
                sceneModeValue = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
                break;
            case XA_CAMERA_EXPOSUREMODE_PORTRAIT:
                sceneModeValue = GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT;
                break;
            case XA_CAMERA_EXPOSUREMODE_NIGHT:
                sceneModeValue = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT;
                break;
            case XA_CAMERA_EXPOSUREMODE_SPORTS:
                sceneModeValue = GST_PHOTOGRAPHY_SCENE_MODE_SPORT;
                break;
            default:
                XA_LOGE("Unmapped exposure mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "scene-mode", sceneModeValue, NULL);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetExposureMode(
    CCameraDevice *pCameraDevice,
    XAuint32 *pExposureMode,
    XAuint32 *pCompensation)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    AL_CHK_ARG(pExposureMode && pCompensation);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstSceneMode gstSceneMode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
        if (!gst_photography_get_scene_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &gstSceneMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        *pCompensation = 0;
        switch (gstSceneMode)
        {
            case GST_PHOTOGRAPHY_SCENE_MODE_AUTO:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_AUTO;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_MANUAL:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_MANUAL;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_PORTRAIT;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_NIGHT;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_SPORT:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_SPORTS;
                break;
            default:
                XA_LOGE("Unmapped exposure mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint sceneModeValue = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "scene-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "scene-mode", &sceneModeValue, NULL);
        switch (sceneModeValue)
        {
            case GST_PHOTOGRAPHY_SCENE_MODE_MANUAL:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_MANUAL;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_PORTRAIT;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_SPORT:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_SPORTS;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_NIGHT;
                break;
            case GST_PHOTOGRAPHY_SCENE_MODE_AUTO:
                *pExposureMode = XA_CAMERA_EXPOSUREMODE_AUTO;
                break;
            default:
                XA_LOGE("Unmapped exposure mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetISOSensitivity(
    CCameraDevice *pCameraDevice,
    XAuint32 isoSensitivity,
    XAuint32 manualSetting)
{
    CameraDeviceData *pCamData = NULL;
    guint isoValue = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (isoSensitivity == XA_CAMERA_ISOSENSITIVITYMODE_AUTO)
    {
        isoValue = 0; // Auto
    }
    else if (isoSensitivity == XA_CAMERA_ISOSENSITIVITYMODE_MANUAL)
    {
        if (manualSetting > 800)
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        isoValue = manualSetting; // Manual Settings
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        if (!gst_photography_set_iso_speed(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), isoValue))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "iso-speed"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "iso-speed", isoValue, NULL);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetISOSensitivity(
    CCameraDevice *pCameraDevice,
    XAuint32 *pIsoSensitivity,
    XAuint32 *pManualSetting)
{
    CameraDeviceData *pCamData = NULL;
    guint isoValue = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    AL_CHK_ARG(pIsoSensitivity && pManualSetting);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        if (!gst_photography_get_iso_speed(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &isoValue))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
    }
    else
    {
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "iso-speed"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "iso-speed", &isoValue, NULL);
    }

    if (isoValue == 0)
    {
        *pIsoSensitivity = XA_CAMERA_ISOSENSITIVITYMODE_AUTO;
        *pManualSetting = 0;
    }
    else
    {
        *pIsoSensitivity = XA_CAMERA_ISOSENSITIVITYMODE_MANUAL;
        *pManualSetting = isoValue;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetShutterSpeed(
    CCameraDevice *pCameraDevice,
    XAuint32 shutterSpeed,
    XAmicrosecond manualSetting)
{
    CameraDeviceData *pCamData = NULL;
    guint exposureValue = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (shutterSpeed == XA_CAMERA_SHUTTERSPEEDMODE_AUTO)
    {
        exposureValue = 0;
    }
    else if(shutterSpeed == XA_CAMERA_SHUTTERSPEEDMODE_MANUAL)
    {
        exposureValue = manualSetting;
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }
    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        if (!gst_photography_set_exposure(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), exposureValue))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "exposure"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "exposure", exposureValue, NULL);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetShutterSpeed(
    CCameraDevice *pCameraDevice,
    XAuint32 *pShutterSpeed,
    XAmicrosecond *pManualSetting)
{
    CameraDeviceData *pCamData = NULL;
    guint exposureValue = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    AL_CHK_ARG(pShutterSpeed && pManualSetting);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        if (!gst_photography_get_exposure(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &exposureValue))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
    }
    else
    {
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "exposure"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "exposure", &exposureValue, NULL);
    }

    if (exposureValue == 0)
    {
        *pShutterSpeed = XA_CAMERA_SHUTTERSPEEDMODE_AUTO;
        *pManualSetting = 0;
    }
    else
    {
        *pShutterSpeed = XA_CAMERA_SHUTTERSPEEDMODE_MANUAL;
        *pManualSetting = exposureValue;
    }
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetWhiteBalance(
    CCameraDevice *pCameraDevice,
    XAuint32 whiteBalance,
    XAuint32 manualSetting)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstWhiteBalanceMode gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_AUTO;
        switch(whiteBalance)
        {
            case XA_CAMERA_WHITEBALANCEMODE_AUTO:
                gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_AUTO;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_SUNLIGHT:
                gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_CLOUDY:
                gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_CLOUDY;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_TUNGSTEN:
                gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_FLUORESCENT:
                gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_SUNSET:
                gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_SUNSET;
                break;
            default:
                XA_LOGE("Unmapped white balance mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }

        if (!gst_photography_set_white_balance_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), gstWhiteBalMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint whiteBalenceValue = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "white-balance-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }

        switch (whiteBalance)
        {
            case XA_CAMERA_WHITEBALANCEMODE_AUTO:
                whiteBalenceValue = GST_PHOTOGRAPHY_WB_MODE_AUTO;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_SUNLIGHT:
                whiteBalenceValue = GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_CLOUDY:
                whiteBalenceValue = GST_PHOTOGRAPHY_WB_MODE_CLOUDY;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_TUNGSTEN:
                whiteBalenceValue = GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_SUNSET:
                whiteBalenceValue = GST_PHOTOGRAPHY_WB_MODE_SUNSET;
                break;
            case XA_CAMERA_WHITEBALANCEMODE_FLUORESCENT:
                whiteBalenceValue = GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT;
                break;
            default:
                XA_LOGE("Unmapped white balance mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "white-balance-mode", whiteBalenceValue, NULL);
    }
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetWhiteBalance(
    CCameraDevice *pCameraDevice,
    XAuint32 *pWhiteBalance,
    XAuint32 *pManualSetting)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    AL_CHK_ARG(pWhiteBalance && pManualSetting);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    *pManualSetting = 0;
    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        GstWhiteBalanceMode gstWhiteBalMode = GST_PHOTOGRAPHY_WB_MODE_AUTO;
        if (!gst_photography_get_white_balance_mode(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &gstWhiteBalMode))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        switch(gstWhiteBalMode)
        {
            case GST_PHOTOGRAPHY_WB_MODE_AUTO:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_AUTO;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_SUNLIGHT;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_CLOUDY:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_CLOUDY;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_TUNGSTEN;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_FLUORESCENT;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_SUNSET:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_SUNSET;
                break;
            default:
                XA_LOGV("Unmapped white balance mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        guint whiteBalanceValue = 0;
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "white-balance-mode"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "white-balance-mode", &whiteBalanceValue, NULL);
        switch (whiteBalanceValue)
        {
            case GST_PHOTOGRAPHY_WB_MODE_AUTO:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_AUTO;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_SUNLIGHT;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_CLOUDY:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_CLOUDY;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_TUNGSTEN;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_SUNSET:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_SUNSET;
                break;
            case GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT:
                *pWhiteBalance = XA_CAMERA_WHITEBALANCEMODE_FLUORESCENT;
                break;
            default:
                XA_LOGV("Unmapped white balance mode");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetAutoLocks(
    CCameraDevice *pCameraDevice,
    XAuint32 locks)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (locks)
        g_signal_emit_by_name(
            G_OBJECT(pCamData->pCameraSrc), "start-auto-focus", 0);
    else
        g_signal_emit_by_name(
            G_OBJECT(pCamData->pCameraSrc), "stop-auto-focus", 0);
    pCamData->mLocks = locks;

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetAutoLocks(
    CCameraDevice *pCameraDevice,
    XAuint32 *pLocks)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData && pLocks);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);
    *pLocks = pCamData->mLocks;

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetZoom(
    CCameraDevice *pCameraDevice,
    XApermille zoom,
    XAboolean digitalEnabled,
    XAuint32 speed,
    XAboolean async)
{
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    if (zoom < 1 || zoom > 8)
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        if (!gst_photography_set_zoom(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), zoom))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    else
    {
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "max-zoom"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "max-zoom", zoom, NULL);
    }
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetZoom(
    CCameraDevice *pCameraDevice,
    XApermille *pZoom,
    XAboolean *pDigital)
{
    CameraDeviceData *pCamData = NULL;
    gfloat zoomFactor = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    AL_CHK_ARG(pZoom && pDigital);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;

    if (GST_IS_PHOTOGRAPHY(pCamData->pCameraSrc))
    {
        if (!gst_photography_get_zoom(
            GST_PHOTOGRAPHY(pCamData->pCameraSrc), &zoomFactor))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
    }
    else
    {
        if (!g_object_class_find_property(
            G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "max-zoom"))
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "max-zoom", &zoomFactor, NULL);
    }
    *pZoom = zoomFactor;
    *pDigital = XA_BOOLEAN_TRUE;

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetBrightness(
    CCameraDevice *pCameraDevice,
    XAuint32 brightness)
{
    XAint32 brightnessValue = 0;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "brightness");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        // Translate to scale of 0 to 100
        brightnessValue = pIntegerParamSpec->minimum + ((brightness *
            (pIntegerParamSpec->maximum - pIntegerParamSpec->minimum)) / 100);
        g_object_set(G_OBJECT(pCamData->pCameraSrc),
            "brightness", brightnessValue, NULL);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetBrightness(
    CCameraDevice *pCameraDevice,
    XAuint32 *pBrightness)
{
    XAint32 brightness = 0;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData && pBrightness);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "brightness");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "brightness", &brightness, NULL);
        brightness = abs(brightness - pIntegerParamSpec->minimum) * 100;
        *pBrightness = (XAuint32)(brightness /
            abs(pIntegerParamSpec->maximum - pIntegerParamSpec->minimum));
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetContrast(
    CCameraDevice *pCameraDevice,
    XAint32 contrast)
{
    XAint32 contrastValue = 0;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "contrast");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        // Translate to scale of -100 to 100. Add 100 to normalize scale to 0
        contrastValue = pIntegerParamSpec->minimum + (((contrast + 100) *
            (pIntegerParamSpec->maximum - pIntegerParamSpec->minimum)) / 200);
        g_object_set(
            G_OBJECT(pCamData->pCameraSrc), "contrast", contrastValue, NULL);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetContrast(
    CCameraDevice *pCameraDevice,
    XAint32 *pContrast)
{
    XAint32 contrast = 0;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    CameraDeviceData *pCamData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraDevice && pCameraDevice->pData && pContrast);
    pCamData = (CameraDeviceData *)pCameraDevice->pData;
    AL_CHK_ARG(pCamData->pCameraSrc);

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCamData->pCameraSrc), "contrast");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        g_object_get(
            G_OBJECT(pCamData->pCameraSrc), "contrast", &contrast, NULL);
        contrast = abs(contrast - pIntegerParamSpec->minimum) * 200;
        *pContrast = (contrast /
            abs(pIntegerParamSpec->maximum - pIntegerParamSpec->minimum));
        *pContrast -= 100; //shift scale from -100 to 100
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceSetGamma(
    CCameraDevice *pCameraDevice,
    XApermille gamma)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetGamma(
    CCameraDevice *pCameraDevice,
    XApermille *pGamma)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendCameraDeviceGetSupportedGammaSettings(
    CCameraDevice *pCameraDevice,
    XApermille *pMinValue,
    XApermille *pMaxValue)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}


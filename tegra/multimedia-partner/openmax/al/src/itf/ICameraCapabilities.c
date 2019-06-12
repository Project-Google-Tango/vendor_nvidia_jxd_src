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

/* CameraCapabilities implementation */

#include "linux_cameradevice.h"

#define NUM_ISO_MANUAL_SETTINGS 4
#define NUM_ZOOM_SETTINGS 8
#define NUM_AUTOLOCKS_SETTINGS 2

static const XACameraDescriptor CameraDescriptorArray[] =
{
    //Camera 0 //Back facing camera
    {
        (XAchar*)"Back Facing Camera", //Name
        3264, //maxWidth
        2448, //maxHeight
        XA_ORIENTATION_OUTWARDS, //orientation
        (XA_CAMERACAP_AUTOEXPOSURE | XA_CAMERACAP_FLASH |
        XA_CAMERACAP_AUTOFOCUS | XA_CAMERACAP_METERING |
        XA_CAMERACAP_AUTOWHITEBALANCE | XA_CAMERACAP_AUTOISOSENSITIVITY |
        XA_CAMERACAP_DIGITALZOOM | XA_CAMERACAP_MANUALISOSENSITIVITY |
        XA_CAMERACAP_BRIGHTNESS | XA_CAMERACAP_CONTRAST), //featuresSupported
        (XA_CAMERA_EXPOSUREMODE_AUTO |XA_CAMERA_EXPOSUREMODE_MANUAL |//exposureModesSupported
        XA_CAMERA_EXPOSUREMODE_PORTRAIT | XA_CAMERA_EXPOSUREMODE_NIGHT |
        XA_CAMERA_EXPOSUREMODE_SPORTS),
        (XA_CAMERA_FLASHMODE_OFF | XA_CAMERA_FLASHMODE_ON |
        XA_CAMERA_FLASHMODE_AUTO | XA_CAMERA_FLASHMODE_REDEYEREDUCTION |
        XA_CAMERA_FLASHMODE_REDEYEREDUCTION_AUTO |
        XA_CAMERA_FLASHMODE_FILLIN), //flashModesSupported
        XA_CAMERA_FOCUSMODE_AUTO, //focusModesSupported
        (XA_CAMERA_METERINGMODE_AVERAGE | XA_CAMERA_METERINGMODE_SPOT |
        XA_CAMERA_METERINGMODE_MATRIX), //meteringModesSupported
        (XA_CAMERA_WHITEBALANCEMODE_AUTO |
        XA_CAMERA_WHITEBALANCEMODE_FLUORESCENT |
        XA_CAMERA_WHITEBALANCEMODE_SUNLIGHT |
        XA_CAMERA_WHITEBALANCEMODE_CLOUDY |
        XA_CAMERA_WHITEBALANCEMODE_TUNGSTEN |
        XA_CAMERA_WHITEBALANCEMODE_FLUORESCENT |
        XA_CAMERA_WHITEBALANCEMODE_SUNSET), //whiteBalanceModesSupported
    }
};

static XAresult
populateCameraCapabilities(CameraCapabilitiesData *pCameraCaps)
{
    CameraDescriptor *pDescriptor = NULL;
    XAuint32 i = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraCaps);

    //Set orientation and name
    for (i = 0; i < pCameraCaps->mNumCameraDevices; i++)
    {
        pDescriptor = &pCameraCaps->pDescriptorList[i];
        pDescriptor->id = i;
        memcpy((void *)&pDescriptor->mDescriptor,
            (const void *)&CameraDescriptorArray[i],
            sizeof(XACameraDescriptor));

        //AutoLocks Settings
        pDescriptor->mNumAutoLocksSettings = NUM_AUTOLOCKS_SETTINGS;
        pDescriptor->pAutoLocksSettings =
            (XAuint32 *)malloc(
                (sizeof(XAuint32) * pDescriptor->mNumAutoLocksSettings));
        if (pDescriptor->pAutoLocksSettings)
        {
            memset(pDescriptor->pAutoLocksSettings, 0,
                (sizeof(XAuint32)) * pDescriptor->mNumAutoLocksSettings);
            pDescriptor->pAutoLocksSettings[0] = 0;
            pDescriptor->pAutoLocksSettings[1] = XA_CAMERA_LOCK_AUTOFOCUS |
                XA_CAMERA_LOCK_AUTOEXPOSURE | XA_CAMERA_LOCK_AUTOWHITEBALANCE;
        }
        else
        {
            result = XA_RESULT_MEMORY_FAILURE;
            XA_LEAVE_INTERFACE;
        }
        //Zoom Settings
        pDescriptor->mNumZoomSettings = NUM_ZOOM_SETTINGS;
        pDescriptor->pZoomSettings =
            (XApermille *)malloc(
                (sizeof(XApermille)) * pDescriptor->mNumZoomSettings);
        if (pDescriptor->pZoomSettings)
        {
            memset(pDescriptor->pZoomSettings, 0,
                (sizeof(XApermille)) * pDescriptor->mNumZoomSettings);
            pDescriptor->pZoomSettings[0] = 1000;
            pDescriptor->pZoomSettings[1] = 2000;
            pDescriptor->pZoomSettings[2] = 3000;
            pDescriptor->pZoomSettings[3] = 4000;
            pDescriptor->pZoomSettings[4] = 5000;
            pDescriptor->pZoomSettings[5] = 6000;
            pDescriptor->pZoomSettings[6] = 7000;
            pDescriptor->pZoomSettings[7] = 8000;
        }
        else
        {
            result = XA_RESULT_MEMORY_FAILURE;
            XA_LEAVE_INTERFACE;
        }
        //ISO Manual settings
        pDescriptor->mNumISOManualSettings = NUM_ISO_MANUAL_SETTINGS;
        pDescriptor->pISOManualSettings =
            (XAuint32*)malloc(
                (sizeof(XAuint32)) * pDescriptor->mNumISOManualSettings);
        if (pDescriptor->pISOManualSettings)
        {
            memset(pDescriptor->pISOManualSettings, 0,
                (sizeof(XAuint32)) * pDescriptor->mNumISOManualSettings);
            pDescriptor->pISOManualSettings[0] = 100;
            pDescriptor->pISOManualSettings[1] = 200;
            pDescriptor->pISOManualSettings[2] = 400;
            pDescriptor->pISOManualSettings[3] = 800;
        }
        else
        {
            result = XA_RESULT_MEMORY_FAILURE;
        }
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_GetCameraCapabilities
//***************************************************************************
static XAresult
ICameraCapabilities_GetCameraCapabilities(
    XACameraCapabilitiesItf self,
    XAuint32 *pIndex,
    XAuint32 *pCameraDeviceID,
    XACameraDescriptor *pDescriptor)
{
    XAuint32 index = 0;
    XAuint32 i = 0;
    CameraDescriptor *pCamDesc = NULL;
    CameraCapabilitiesData *pCameraCaps = NULL;
    ICameraCapabilities *pCaps = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCaps && pCaps->pData && pIndex);
    pCameraCaps = (CameraCapabilitiesData *)pCaps->pData;

    // Return number of Camera Devices if pDescriptor is NULL
    if (NULL == pDescriptor)
    {
        *pIndex = pCameraCaps->mNumCameraDevices;
        XA_LEAVE_INTERFACE;
    }
    // Index of Camera device is already given. Use it directly
    if (pIndex)
    {
        index = *pIndex;
    }
    // Device ID is given. Map the camera Device index corresponding to the device ID
    else if (pCameraDeviceID)
    {
        if (*pCameraDeviceID == XA_DEFAULTDEVICEID_CAMERA)
            index = pCameraCaps->mDefaultCameraDeviceIDIndex;
        else
            index = *pCameraDeviceID;
    }

    //index should be in range of 0 to n-1
    AL_CHK_ARG((index >= 0) && (index < pCameraCaps->mNumCameraDevices));
    // Get the Camera descriptor
    for (i = 0; i < pCameraCaps->mNumCameraDevices; i++)
    {
        pCamDesc = &pCameraCaps->pDescriptorList[i];
        if ((pCamDesc) && (pCamDesc->id == index))
        {
            memcpy(pDescriptor, &pCamDesc->mDescriptor,
                sizeof(XACameraDescriptor));
            if (pIndex)
                *pCameraDeviceID = pCamDesc->id;
            break;
        }
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_QueryFocusRegionPatterns
//***************************************************************************
static XAresult
ICameraCapabilities_QueryFocusRegionPatterns(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAuint32 *pPatternID,
    XAuint32 *pFocusPattern,
    XAuint32 *pCustomPoints1,
    XAuint32 *pCustomPoints2)
{
    XAuint32 index = 0;
    CameraCapabilitiesData *pCameraCaps = NULL;
    CameraDescriptor *pDescriptor = NULL;
    ICameraCapabilities *pCaps = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCaps && pCaps->pData && pPatternID);
    pCameraCaps = (CameraCapabilitiesData *)pCaps->pData;

    //Check the Camera device ID
    if (cameraDeviceID == XA_DEFAULTDEVICEID_CAMERA)
        index = pCameraCaps->mDefaultCameraDeviceIDIndex;
    else
        index = cameraDeviceID;

    //index should be in range of 0 to n-1
    AL_CHK_ARG((index >= 0) && (index < pCameraCaps->mNumCameraDevices));

    // Check if AUTO focus feature is supported
    pDescriptor = &pCameraCaps->pDescriptorList[index];
    AL_CHK_ARG(pDescriptor);

    if (!(pDescriptor->mDescriptor.featuresSupported & XA_CAMERACAP_AUTOFOCUS))
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    // We support only one Focus Pattern
    // Return number of Focus Patterns supported when pFocusPattern is NULL
    if (pFocusPattern == NULL)
        *pPatternID = 1;
    // Since we support only one Pattern ID, only pFocusPattern[0] is valid
    else if((*pPatternID) == 0)
        pFocusPattern[0] = XA_FOCUSPOINTS_ONE;
    // Return Parameter INVALID
    else
        result = XA_RESULT_PARAMETER_INVALID;

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_GetSupportedAutoLocks
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedAutoLocks(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAuint32 *pNumCombinations,
    XAuint32 **ppLocks)
{
    XAuint32 index = 0;
    CameraCapabilitiesData *pCameraCaps = NULL;
    CameraDescriptor *pDescriptor = NULL;

    ICameraCapabilities *pCameraCapsItf = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraCapsItf && pCameraCapsItf->pData && pNumCombinations);
    pCameraCaps = (CameraCapabilitiesData *)pCameraCapsItf->pData;

    //Check the id
    if (cameraDeviceID == XA_DEFAULTDEVICEID_CAMERA)
        index = pCameraCaps->mDefaultCameraDeviceIDIndex;
    else
        index = cameraDeviceID;

    // Check if MANUAL ISO setting feature is supported
    pDescriptor = &pCameraCaps->pDescriptorList[index];
    AL_CHK_ARG(pDescriptor);

    if (!(pDescriptor->mDescriptor.featuresSupported &
        XA_CAMERACAP_MANUALISOSENSITIVITY) ||
        (pDescriptor->mNumISOManualSettings == 0))
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    // Set the number for combinations supported
    *pNumCombinations = pDescriptor->mNumAutoLocksSettings;
    if ((ppLocks != NULL) && (*ppLocks != NULL))
    {
        memcpy((void *)*ppLocks, pDescriptor->pAutoLocksSettings,
            (sizeof(XAuint32) * pDescriptor->mNumAutoLocksSettings));
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_GetSupportedFocusManualSettings
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedFocusManualSettings(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAboolean macroEnabled,
    XAmillimeter *pMinValue,
    XAmillimeter *pMaxValue,
    XAuint32 *pNumSettings,
    XAmillimeter **ppSettings)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_GetSupportedISOSensitivitySettings
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedISOSensitivitySettings(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAuint32 *pMinValue,
    XAuint32 *pMaxValue,
    XAuint32 *pNumSettings,
    XAuint32 **ppSettings)
{
    XAuint32 index = 0;
    CameraCapabilitiesData *pCameraCaps = NULL;
    CameraDescriptor *pDescriptor = NULL;
    ICameraCapabilities *pCameraCapsItf = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCameraCapsItf && pCameraCapsItf->pData);
    AL_CHK_ARG(pNumSettings && pMinValue && pMaxValue);
    pCameraCaps = (CameraCapabilitiesData *)pCameraCapsItf->pData;

    //Check the id
    if (cameraDeviceID == XA_DEFAULTDEVICEID_CAMERA)
        index = pCameraCaps->mDefaultCameraDeviceIDIndex;
    else
        index = cameraDeviceID;

    // Check if MANUAL ISO setting feature is supported
    pDescriptor = &pCameraCaps->pDescriptorList[index];
    AL_CHK_ARG(pDescriptor);

    if (!(pDescriptor->mDescriptor.featuresSupported &
        XA_CAMERACAP_MANUALISOSENSITIVITY) ||
        (pDescriptor->mNumISOManualSettings == 0))
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    // Length of Array is number of settings available
    *pNumSettings = pDescriptor->mNumISOManualSettings;
    if ((ppSettings != NULL) && (*ppSettings != NULL))
    {
        *pMinValue = pDescriptor->pISOManualSettings[0];
        *pMaxValue = pDescriptor->pISOManualSettings[(*pNumSettings) - 1];
        // Assign the array of settings
        memcpy((void *)*ppSettings,
            (const void *)pDescriptor->pISOManualSettings,
            (sizeof(XAuint32) * (*pNumSettings)));
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_GetSupportedApertureManualSettings
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedApertureManualSettings(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAuint32 *pMinValue,
    XAuint32 *pMaxValue,
    XAuint32 *pNumSettings,
    XAuint32 **ppSettings)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE
}

//***************************************************************************
// ICameraCapabilities_GetSupportedShutterSpeedManualSettings
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedShutterSpeedManualSettings(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAmicrosecond *pMinValue,
    XAmicrosecond *pMaxValue,
    XAuint32 *pNumSettings,
    XAmicrosecond **ppSettings)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE
}

//***************************************************************************
// ICameraCapabilities_GetSupportedWhiteBalanceManualSettings
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedWhiteBalanceManualSettings(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAuint32 *pMinValue,
    XAuint32 *pMaxValue,
    XAuint32 *pNumSettings,
    XAuint32 **ppSettings)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE
}

//***************************************************************************
// ICameraCapabilities_GetSupportedZoomSettings
//***************************************************************************
static XAresult
ICameraCapabilities_GetSupportedZoomSettings(
    XACameraCapabilitiesItf self,
    XAuint32 cameraDeviceID,
    XAboolean digitalEnabled,
    XAboolean macroEnabled,
    XApermille *pMaxValue,
    XAuint32 *pNumSettings,
    XApermille **ppSettings,
    XAboolean *pSpeedSupported)
{
    XAuint32 index = 0;
    CameraCapabilitiesData *pCameraCaps = NULL;
    CameraDescriptor *pDescriptor = NULL;
    ICameraCapabilities *pCaps = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCaps && pCaps->pData);
    AL_CHK_ARG(pMaxValue && pNumSettings && pSpeedSupported);
    pCameraCaps = (CameraCapabilitiesData *)pCaps->pData;

    // Optical zoom not supported
    // Macro mode not supported
    if ((digitalEnabled == XA_BOOLEAN_FALSE) ||
        (macroEnabled == XA_BOOLEAN_TRUE))
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    //Speed control not supported
    *pSpeedSupported = XA_BOOLEAN_FALSE;

    //Check the id
    if (cameraDeviceID == XA_DEFAULTDEVICEID_CAMERA)
        index = pCameraCaps->mDefaultCameraDeviceIDIndex;
    else
        index = cameraDeviceID;

    // Check if feature is supported
    pDescriptor = &pCameraCaps->pDescriptorList[index];
    AL_CHK_ARG(pDescriptor);

    if (!(pDescriptor->mDescriptor.featuresSupported &
        XA_CAMERACAP_DIGITALZOOM) ||
        (pDescriptor->mNumZoomSettings == 0))
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LEAVE_INTERFACE;
    }

    // Length of Array is number of settings available
    *pNumSettings = pDescriptor->mNumZoomSettings;
    if ((ppSettings != NULL) && (*ppSettings != NULL))
    {
        // Assign the array of zoom settings
        memcpy((void *)*ppSettings,
            (const void *)pDescriptor->pZoomSettings,
            (sizeof(XApermille) * pDescriptor->mNumZoomSettings));
        *pMaxValue =
            pDescriptor->pZoomSettings[pDescriptor->mNumZoomSettings - 1];
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// ICameraCapabilities_Itf
//***************************************************************************
static const struct XACameraCapabilitiesItf_ ICameraCapabilities_Itf =
{
    ICameraCapabilities_GetCameraCapabilities,
    ICameraCapabilities_QueryFocusRegionPatterns,
    ICameraCapabilities_GetSupportedAutoLocks,
    ICameraCapabilities_GetSupportedFocusManualSettings,
    ICameraCapabilities_GetSupportedISOSensitivitySettings,
    ICameraCapabilities_GetSupportedApertureManualSettings,
    ICameraCapabilities_GetSupportedShutterSpeedManualSettings,
    ICameraCapabilities_GetSupportedWhiteBalanceManualSettings,
    ICameraCapabilities_GetSupportedZoomSettings
};

//***************************************************************************
// ICameraCapabilities_init
//***************************************************************************
void ICameraCapabilities_init(void *self)
{
    CameraCapabilitiesData *pCameraCapsData = NULL;
    ICameraCapabilities *pCameraCaps = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE_VOID;
    if (pCameraCaps)
    {
        pCameraCaps->mItf = &ICameraCapabilities_Itf;
        pCameraCapsData = (CameraCapabilitiesData *)malloc(
            sizeof(CameraCapabilitiesData));
        if(pCameraCapsData)
        {
            memset(pCameraCapsData, 0, sizeof(CameraCapabilitiesData));
            // Set number of Camera devices as 1 as we support one camera now
            pCameraCapsData->mNumCameraDevices = 1;
            // Say the default device ID is 1
            pCameraCapsData->mDefaultCameraDeviceID = 1;
            // We have only one camera device, so that would be first element
            pCameraCapsData->mDefaultCameraDeviceIDIndex = 0;
            pCameraCapsData->pDescriptorList = (CameraDescriptor *)malloc(
                (sizeof(CameraDescriptor)) * (pCameraCapsData->mNumCameraDevices));
            if (pCameraCapsData->pDescriptorList)
            {
                memset(pCameraCapsData->pDescriptorList, 0,
                    ((sizeof(CameraDescriptor)) *
                    (pCameraCapsData->mNumCameraDevices)));
                populateCameraCapabilities(pCameraCapsData);
            }
        }
        pCameraCaps->pData = pCameraCapsData;
    }

    XA_LEAVE_INTERFACE_VOID;
}

//***************************************************************************
// ICameraCapabilities_deinit
//***************************************************************************
void ICameraCapabilities_deinit(void *self)
{
    XAuint32 i = 0;
    CameraDescriptor *pDescriptor = NULL;
    CameraCapabilitiesData *pCameraCapsData = NULL;
    ICameraCapabilities *pCameraCaps = (ICameraCapabilities *)self;

    XA_ENTER_INTERFACE_VOID;
    if (pCameraCaps && pCameraCaps->pData)
    {
        pCameraCapsData = (CameraCapabilitiesData *)pCameraCaps->pData;
        if (pCameraCapsData->pDescriptorList)
        {
            for (i = 0; i < pCameraCapsData->mNumCameraDevices; i++)
            {
                pDescriptor = &pCameraCapsData->pDescriptorList[i];
                if (pDescriptor->pAutoLocksSettings)
                    free(pDescriptor->pAutoLocksSettings);
                if (pDescriptor->pISOManualSettings)
                    free(pDescriptor->pISOManualSettings);
                if (pDescriptor->pZoomSettings)
                    free(pDescriptor->pZoomSettings);
            }
            free(pCameraCapsData->pDescriptorList);
        }
        free(pCameraCapsData);
        pCameraCaps->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID;
}

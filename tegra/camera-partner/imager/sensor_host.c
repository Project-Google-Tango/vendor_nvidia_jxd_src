/*
 * Copyright (c) 2010-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "sensor_host.h"
#include "imager_util.h"
#include "sensor_virtual_calibration.h"


/** Implementation NvOdmSensor driver for HOST
 * Host Mode is when the user chooses to inject the
 * camera driver with buffers.  This is most useful
 * when sending Bayer data through ISP processing.
 *
 * the limits should be set much wider than we would
 * ever encounter in real life so that simulations can
 * set manual exposure values for images without interference
 */

#define EXPOSURE_MIN    0.0f
#define EXPOSURE_MAX    10000.f
#define GAIN_MIN        0.0f
#define GAIN_MAX        10000.f
#define FRAMERATE_MIN   0.f
#define FRAMERATE_MAX   30.f
#define FOCUS_MIN       0
#define FOCUS_MAX       8

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides.isp",
};

NvOdmImagerCapabilities SensorHostCaps =
{
    "Host",
    NvOdmImagerSensorInterface_Host,
    {
        NvOdmImagerPixelType_UYVY,
        NvOdmImagerPixelType_BayerRGGB,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0},{0,0}}, // clocks are both 120MHz
    {0,0,NV_FALSE}, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {48, 16}, // padding for blank
    0, // preferred mode 0
    0,
    0,
    NVODM_IMAGER_CAPABILITIES_END
};

static SensorSetModeSequence g_SensorHostSetModeSequenceList[] =
{
     // WxH        x, y    fps ratio  set mode sequence
    {{{320, 240}, {0, 0},  30, 0}, NULL, NULL},
};

typedef struct SensorHostContextRec
{
    NvU32 NumModes;

    // A real sensor would read/write these values to the actual
    // sensor hardware, but for the fake sensors, simply store
    // it in a private structure.
    NvF32 Exposure;
    NvF32 Gain;
    NvU32 FocusPosition;
    NvF32 FrameRate;
    NvOdmImagerPowerLevel PowerLevel;

} SensorHostContext;

static NvBool SensorHost_Open(NvOdmImagerHandle hImager)
{
    SensorHostContext *pSensorHostContext = NULL;

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorHostContext =
        (SensorHostContext*)NvOdmOsAlloc(sizeof(SensorHostContext));
    if (!pSensorHostContext)
        return NV_FALSE;

    NvOdmOsMemset(pSensorHostContext, 0, sizeof(SensorHostContext));
    pSensorHostContext->NumModes =
        NV_ARRAY_SIZE(g_SensorHostSetModeSequenceList);

    pSensorHostContext->PowerLevel = NvOdmImagerPowerLevel_Off;

    hImager->pSensor->pPrivateContext = pSensorHostContext;


    return NV_TRUE;
}

static void SensorHost_Close(NvOdmImagerHandle hImager)
{
    SensorHostContext *pContext = NULL;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
        return;

    pContext = (SensorHostContext*)hImager->pSensor->pPrivateContext;

    // cleanup
    NvOdmOsFree(pContext);
    hImager->pSensor->pPrivateContext = NULL;
}

static void
SensorHost_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    NvOdmOsMemcpy(pCapabilities, &SensorHostCaps, sizeof(NvOdmImagerCapabilities));
}

static void
SensorHost_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pMode,
    NvS32 *pNumberOfModes)
{
   SensorHostContext *pContext =
        (SensorHostContext*)hImager->pSensor->pPrivateContext;
    NvS32 i;

    if (pNumberOfModes)
    {
        *pNumberOfModes = (NvS32)pContext->NumModes;
        if (pMode)
        {
            // Copy the modes from g_SensorYuvSetModeSequenceList
            for (i = 0; i < *pNumberOfModes; i++)
                pMode[i] = g_SensorHostSetModeSequenceList[i].Mode;
        }
    }
}

static NvBool
SensorHost_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult)
{
    SensorHostContext *pContext =
        (SensorHostContext*)hImager->pSensor->pPrivateContext;

    if (pParameters)
    {
        pContext->Exposure = pParameters->Exposure;
        pContext->Gain = pParameters->Gains[0];
        // In host mode, we allow changing the resolution to anything
        g_SensorHostSetModeSequenceList[0].Mode.ActiveDimensions = pParameters->Resolution;
    }

    if (pSelectedMode)
    {
        *pSelectedMode = g_SensorHostSetModeSequenceList[0].Mode;
    }

    if (pResult)
    {
        NvU32 i = 0;

        pResult->Resolution = g_SensorHostSetModeSequenceList[0].Mode.ActiveDimensions;
        pResult->Exposure = pContext->Exposure;

        for (i = 0; i < 4; i++)
            pResult->Gains[i] = pContext->Gain;
    }

    return NV_TRUE;
}

static NvBool
SensorHost_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
    SensorHostContext *pContext =
        (SensorHostContext*)hImager->pSensor->pPrivateContext;

    pContext->PowerLevel = PowerLevel;

    return NV_TRUE;
}

static NvBool
SensorHost_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    SensorHostContext *pContext =
        (SensorHostContext*)hImager->pSensor->pPrivateContext;
    NvF32 *pFloatValue = (NvF32*)pValue;
    NvU32 *pIntValue = (NvU32*)pValue;

    switch(Param)
    {
        case NvOdmImagerParameter_SensorExposure:
            if (*pFloatValue > EXPOSURE_MAX ||
                *pFloatValue < EXPOSURE_MIN)
            {
                return NV_FALSE;
            }

            pContext->Exposure = *pFloatValue;
            break;

        case NvOdmImagerParameter_SensorGain:
            if (*pFloatValue > GAIN_MAX ||
                *pFloatValue < GAIN_MIN)
            {
                return NV_FALSE;
            }

            pContext->Gain = *pFloatValue;
            break;

        case NvOdmImagerParameter_SensorFrameRate:
            if (*pFloatValue > FRAMERATE_MAX ||
                *pFloatValue < FRAMERATE_MIN)
            {
                return NV_FALSE;
            }
            pContext->FrameRate = *pFloatValue;
            break;

        case NvOdmImagerParameter_FocuserLocus:

            if ((NvS32)*pIntValue > (NvS32)FOCUS_MAX ||
                (NvS32)*pIntValue < (NvS32)FOCUS_MIN)
                return NV_FALSE;

            pContext->FocusPosition = *pIntValue;
            break;

        default:
            return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
SensorHost_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    SensorHostContext *pContext =
        (SensorHostContext*)hImager->pSensor->pPrivateContext;

    switch(Param)
    {
        // Read/write params
        case NvOdmImagerParameter_SensorExposure:
            *(NvF32*)pValue = pContext->Exposure;
            break;

        case NvOdmImagerParameter_SensorGain:
            {
                NvU32 i = 0;
                for (i = 0; i < 4; i++)
                {
                    ((NvF32*)pValue)[i] = pContext->Gain;
                }
            }
            break;

        case NvOdmImagerParameter_SensorFrameRate:
            *(NvF32*)pValue = pContext->FrameRate;
            break;

        case NvOdmImagerParameter_FocuserLocus:
            *(NvU32*)pValue = pContext->FocusPosition;
            break;

        // Read-only params
        case NvOdmImagerParameter_SensorExposureLimits:
            ((NvF32*)pValue)[0] = EXPOSURE_MIN;
            ((NvF32*)pValue)[1] = EXPOSURE_MAX;
            break;

        case NvOdmImagerParameter_SensorGainLimits:
            ((NvF32*)pValue)[0] = GAIN_MIN;
            ((NvF32*)pValue)[1] = GAIN_MAX;
            break;

        case NvOdmImagerParameter_SensorFrameRateLimits:
            ((NvF32*)pValue)[0] = FRAMERATE_MIN;
            ((NvF32*)pValue)[1] = FRAMERATE_MAX;
            break;

        case NvOdmImagerParameter_SensorExposureLatchTime:
            *(NvU32*)pValue = 1;
            break;

        case NvOdmImagerParameter_CommonCalibrationData:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerCalibrationData));
            {
                NvOdmImagerCalibrationData *pCalibration =
                        (NvOdmImagerCalibrationData*)pValue;
                pCalibration->CalibrationData = pCommonSensorCalibrationData;
                pCalibration->NeedsFreeing = NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_CalibrationOverrides:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerCalibrationData));
            {
                NvOdmImagerCalibrationData *pCalibration =
                    (NvOdmImagerCalibrationData*)pValue;

                pCalibration->CalibrationData =
                    LoadOverridesFile(pOverrideFiles,
                        (sizeof(pOverrideFiles)/sizeof(pOverrideFiles[0])));
                pCalibration->NeedsFreeing = (pCalibration->CalibrationData != NULL);
            }
            break;

        default:
            return NV_FALSE;
    }
    return NV_TRUE;
}

static void
SensorHost_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel)
{
    SensorHostContext *pContext =
        (SensorHostContext*)hImager->pSensor->pPrivateContext;

    *pPowerLevel = pContext->PowerLevel;
}

NvBool SensorHost_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorHost_Open;
    hImager->pSensor->pfnClose = SensorHost_Close;
    hImager->pSensor->pfnGetCapabilities = SensorHost_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorHost_ListModes;
    hImager->pSensor->pfnSetMode = SensorHost_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorHost_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorHost_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorHost_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorHost_GetParameter;

    return NV_TRUE;
}

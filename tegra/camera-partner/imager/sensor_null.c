/*
 * Copyright (c) 2010-2014, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "sensor_null.h"
#include "imager_util.h"
#include "sensor_virtual_calibration.h"


#define EXPOSURE_MIN    1.0f
#define EXPOSURE_MAX    1.0f
#define GAIN_MIN        1.0f
#define GAIN_MAX        16.0f
#define FRAMERATE_MIN   1.0f
#define FRAMERATE_MAX   1.0f
#define FOCUS_MIN       0
#define FOCUS_MAX       8


static NvOdmImagerCapabilities SensorNullBayerCaps =
{
    "Null Bayer Sensor",
    NvOdmImagerSensorInterface_Parallel10,
    {
        NvOdmImagerPixelType_BayerRGGB,
        NvOdmImagerPixelType_BayerBGGR,
        NvOdmImagerPixelType_BayerGBRG,
        NvOdmImagerPixelType_BayerGRBG,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {32, 24}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullYuvCaps =
{
    "Null YUV Sensor",
    NvOdmImagerSensorInterface_Parallel8,
    {
        NvOdmImagerPixelType_UYVY,
        NvOdmImagerPixelType_YVYU,
        NvOdmImagerPixelType_VYUY,
        NvOdmImagerPixelType_YUYV,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0},{0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {0, 0}, // blank time
    0, // preferred mode
    0ULL, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullCSIACaps =
{
    "Null CSIA Sensor",
    NvOdmImagerSensorInterface_SerialA,
    {
        NvOdmImagerPixelType_BayerRGGB8,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_A,
        NVODMSENSORMIPITIMINGS_DATALANES_ONE,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        1, // USE !CONTINUOUS_CLOCK,
        0 // t-hs-settle-time not relevant for null sensor
    }, // serial settings (CSI)
    {0, 0}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullCSIBCaps =
{
    "Null CSIB Sensor",
    NvOdmImagerSensorInterface_SerialB,
    {
        NvOdmImagerPixelType_BayerRGGB8,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_B,
        NVODMSENSORMIPITIMINGS_DATALANES_ONE,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        1, // USE !CONTINUOUS_CLOCK,
        0 // t-hs-settle-time not relevant for null sensor
    }, // serial settings (CSI)
    {0, 0}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullTPGABayerCaps =
{
    "Null TPGA Bayer Sensor",
    NvOdmImagerSensorInterface_SerialA,
    {
        NvOdmImagerPixelType_BayerRGGB,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {32, 24}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullTPGARgbCaps =
{
    "Null TPGA Rgb Sensor",
    NvOdmImagerSensorInterface_SerialA,
    {
        NvOdmImagerPixelType_RGB888,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {32, 24}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullTPGBBayerCaps =
{
    "Null TPGB Bayer Sensor",
    NvOdmImagerSensorInterface_SerialB,
    {
        NvOdmImagerPixelType_BayerRGGB,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {32, 24}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static NvOdmImagerCapabilities SensorNullTPGBRgbCaps =
{
    "Null TPGB Rgb Sensor",
    NvOdmImagerSensorInterface_SerialB,
    {
        NvOdmImagerPixelType_RGB888,
    },
    NvOdmImagerOrientation_0_Degrees,
    NvOdmImagerDirection_Away,
    120000, // minimum input clock in khz
    {{120000, 1.0}, {0,0}}, // uses 120MHz always
    {
        NvOdmImagerSyncEdge_Falling,
        NvOdmImagerSyncEdge_Rising,
        NV_FALSE,
    }, // parallel
    {0,0,0,NV_FALSE,0}, // serial
    {32, 24}, // blank time
    0, // preferred mode
    0, // no focuser
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static SensorSetModeSequence g_SensorNullCamSetModeSequenceList[] =
{
    {
        // NvOdmImagerSensorMode
        {
            {4000, 3000},                       // ActiveDimensions
            {0, 0},                             // ActiveStart
            30,                                 // PeakFrameRate
            1.0,                                // PixelAspectRatio
            20.0,                               // PLL_Multiplier
            NvOdmImagerNoCropMode,              // CropMode
            {
                // NvOdmImagerSensorModeInfo
                {0, 0, 0, 0},                   // scan area rect
                {1.f, 1.f}                     // x/y scales for binning/scaling
            },
            NvOdmImagerModeType_Normal,         // NvOdmImagerModeType
            4000                                // Line Length
        },
        NULL,                                   // pSequence
        NULL                                    // pModeDependentSettings
    },
};

typedef struct SensorNullContextRec
{
    NvU32 SensorPixelType;
    NvU32 NumModes;

    // A real sensor would read/write these values to the actual
    // sensor hardware, but for the fake sensors, simply store
    // it in a private structure.
    NvF32 Exposure;
    NvF32 Gain;
    NvU32 FocusPosition;
    NvF32 FrameRate;
    NvOdmImagerPowerLevel PowerLevel;
    NvOdmImagerStereoCameraMode StereoCameraMode;

} SensorNullContext;

static NvBool
SensorNull_CommonOpen(
    NvOdmImagerHandle hImager,
    NvU32 SensorPixelType)
{
    SensorNullContext *pSensorNullContext = NULL;

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorNullContext =
        (SensorNullContext*)NvOdmOsAlloc(sizeof(SensorNullContext));
    if (!pSensorNullContext)
        goto fail;

    NvOdmOsMemset(pSensorNullContext, 0, sizeof(SensorNullContext));

    pSensorNullContext->SensorPixelType = SensorPixelType;

    pSensorNullContext->NumModes =
            NV_ARRAY_SIZE(g_SensorNullCamSetModeSequenceList);

    pSensorNullContext->PowerLevel = NvOdmImagerPowerLevel_Off;

    hImager->pSensor->pPrivateContext = pSensorNullContext;


    return NV_TRUE;
fail:
    NvOdmOsFree(pSensorNullContext);
    return NV_FALSE;
}

static NvBool SensorNullYuv_Open(NvOdmImagerHandle hImager)
{
    return SensorNull_CommonOpen(hImager, NVODMIMAGERPIXELTYPE_YUV);
}

static NvBool SensorNullBayer_Open(NvOdmImagerHandle hImager)
{
    return SensorNull_CommonOpen(hImager, NVODMIMAGERPIXELTYPE_BAYER);
}

static NvBool SensorNullRgb_Open(NvOdmImagerHandle hImager)
{
    return SensorNull_CommonOpen(hImager, NVODMIMAGERPIXELTYPE_RGB);
}

static void SensorNull_Close(NvOdmImagerHandle hImager)
{
    SensorNullContext *pContext = NULL;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
        return;

    pContext = (SensorNullContext*)hImager->pSensor->pPrivateContext;

    // cleanup
    NvOdmOsFree(pContext);
    hImager->pSensor->pPrivateContext = NULL;
}

static void
SensorNull_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

    switch (pContext->SensorPixelType)
    {
        case NVODMIMAGERPIXELTYPE_BAYER:
            NvOdmOsMemcpy(pCapabilities, &SensorNullBayerCaps,
                sizeof(NvOdmImagerCapabilities));
            break;

        case NVODMIMAGERPIXELTYPE_YUV:
            NvOdmOsMemcpy(pCapabilities, &SensorNullYuvCaps,
                sizeof(NvOdmImagerCapabilities));
            break;

        default:
            NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmImagerCapabilities));
            break;
    }
}

static void
SensorNullTPGA_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

    switch (pContext->SensorPixelType)
    {
        case NVODMIMAGERPIXELTYPE_BAYER:
            NvOdmOsMemcpy(pCapabilities, &SensorNullTPGABayerCaps,
                sizeof(NvOdmImagerCapabilities));
            break;

        case NVODMIMAGERPIXELTYPE_RGB:
            NvOdmOsMemcpy(pCapabilities, &SensorNullTPGARgbCaps,
                sizeof(NvOdmImagerCapabilities));
            break;

        default:
            NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmImagerCapabilities));
            break;
    }
}

static void
SensorNullTPGB_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

    switch (pContext->SensorPixelType)
    {
        case NVODMIMAGERPIXELTYPE_BAYER:
            NvOdmOsMemcpy(pCapabilities, &SensorNullTPGBBayerCaps,
                sizeof(NvOdmImagerCapabilities));
            break;

        case NVODMIMAGERPIXELTYPE_RGB:
            NvOdmOsMemcpy(pCapabilities, &SensorNullTPGBRgbCaps,
                sizeof(NvOdmImagerCapabilities));
            break;

        default:
            NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmImagerCapabilities));
            break;
    }
}

static void
SensorNullCSIA_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    NvOdmOsMemcpy(pCapabilities, &SensorNullCSIACaps,
        sizeof(NvOdmImagerCapabilities));
}

static void
SensorNullCSIB_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    NvOdmOsMemcpy(pCapabilities, &SensorNullCSIBCaps,
        sizeof(NvOdmImagerCapabilities));
}

static void
SensorNull_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pMode,
    NvS32 *pNumberOfModes)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;
    NvS32 i;

    if (pNumberOfModes)
    {
        *pNumberOfModes = (NvS32)pContext->NumModes;
        if (pMode)
        {
            // Copy the modes from g_SensorNullCamSetModeSequenceList
            for (i = 0; i < *pNumberOfModes; i++)
            {
                pMode[i] = g_SensorNullCamSetModeSequenceList[i].Mode;

                if (pContext->StereoCameraMode == StereoCameraMode_Stereo)
                {
                    pMode[i].PixelAspectRatio = 0.5;
                }
            }
        }
    }
}

static NvBool SensorNull_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;
    SensorSetModeSequence *pModeList = NULL;

    pModeList = g_SensorNullCamSetModeSequenceList;

    // In Null mode, we allow changing the resolution to anything
    if (pParameters)
    {
        pModeList[0].Mode.ActiveDimensions = pParameters->Resolution;
    }

    // This is a minimum requirement by the ISP's STALL_CONTROL logic
    if (pContext->StereoCameraMode == StereoCameraMode_Stereo)
    {
        pModeList[0].Mode.ActiveDimensions.width += 20;
        pModeList[0].Mode.ActiveDimensions.height += 20;
    }

    if (pSelectedMode)
    {
        *pSelectedMode = pModeList[0].Mode;
    }

    if (pParameters)
    {
        pContext->Exposure = pParameters->Exposure;
        pContext->Gain = pParameters->Gains[0];
    }

    if (pResult)
    {
        NvU32 i = 0;

        pResult->Resolution = pModeList[0].Mode.ActiveDimensions;
        pResult->Exposure = pContext->Exposure;

        for (i = 0; i < 4; i++)
            pResult->Gains[i] = pContext->Gain;
    }

    return NV_TRUE;
}

static NvBool
SensorNull_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

    pContext->PowerLevel = PowerLevel;

    return NV_TRUE;
}

static void
SensorNull_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

    *pPowerLevel = pContext->PowerLevel;
}

static NvBool
SensorNull_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;
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

        // ignore these settings
        case NvOdmImagerParameter_AWBLock:
        case NvOdmImagerParameter_AELock:
        case NvOdmImagerParameter_ISPSetting:
        case NvOdmImagerParameter_OptimizeResolutionChange:
            return NV_TRUE;

        default:
            return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
SensorNull_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

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
            {
                NvOdmImagerCalibrationData *pCalibration =
                    (NvOdmImagerCalibrationData*)pValue;

                pCalibration->CalibrationData = NULL;
                pCalibration->NeedsFreeing = NV_FALSE;
            }

        break;

        case NvOdmImagerParameter_FocalLength:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32*)pValue = 1.0f;
            break;

        case NvOdmImagerParameter_HorizontalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32*)pValue = 1.0f;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32*)pValue = 1.0f;
            break;

        case NvOdmImagerParameter_SensorIspSupport:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));
            *(NvBool*)pValue = pContext->SensorPixelType == NVODMIMAGERPIXELTYPE_BAYER;
            break;

        case NvOdmImagerParameter_ISPSetting:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerISPSetting));

            if (pValue)
            {
                switch(((NvOdmImagerISPSetting *)pValue)->attribute)
                {
                    case NvCameraIspAttribute_FocusPositionRange:
                        {
                            NvCameraIspFocusingParameters *pParams =
                                (NvCameraIspFocusingParameters *)(((NvOdmImagerISPSetting *)pValue)->pData);
                            pParams->sensorispAFsupport = NV_FALSE;
                        }
                        break;

                    default:
                        return NV_FALSE;
                 }
            }
            break;

        default:
            return NV_FALSE;
    }
    return NV_TRUE;
}


NvBool SensorNullBayer_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullBayer_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNull_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNull_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNull_GetParameter;

    return NV_TRUE;
}

static NvBool
SensorNullCSI_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;
    NvF32 *pFloatValue = (NvF32*)pValue;
    NvU32 *pIntValue = (NvU32*)pValue;

    switch(Param)
    {
        case NvOdmImagerParameter_StereoCameraMode:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerStereoCameraMode));


            NvOdmImagerStereoCameraMode CameraMode = *(NvOdmImagerStereoCameraMode *)pValue;
            pContext->StereoCameraMode = CameraMode;

            pContext->NumModes =
                NV_ARRAY_SIZE(g_SensorNullCamSetModeSequenceList);

            break;

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

        case NvOdmImagerParameter_OptimizeResolutionChange:
            return NV_TRUE;

        default:
            return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
SensorNullCSI_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    SensorNullContext *pContext =
        (SensorNullContext*)hImager->pSensor->pPrivateContext;

    switch(Param)
    {
        case NvOdmImagerParameter_StereoCapable:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));
            {
                NvBool *pBL = (NvBool*)pValue;
                *pBL = NV_TRUE;
            }
            break;

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

        case NvOdmImagerParameter_CalibrationOverrides:
            {
                NvOdmImagerCalibrationData *pCalibration =
                    (NvOdmImagerCalibrationData*)pValue;

                pCalibration->CalibrationData = NULL;
                pCalibration->NeedsFreeing = NV_FALSE;
            }

        break;

        default:
            return NV_FALSE;
    }
    return NV_TRUE;
}

NvBool SensorNullCSIA_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullBayer_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNullCSIA_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNullCSI_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNullCSI_GetParameter;

    return NV_TRUE;
}

NvBool SensorNullCSIB_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullBayer_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNullCSIB_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNullCSI_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNullCSI_GetParameter;

    return NV_TRUE;
}


NvBool SensorNullYuv_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullYuv_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNull_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNull_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNull_GetParameter;

    return NV_TRUE;
}

NvBool SensorNullTPGABayer_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullBayer_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNullTPGA_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNull_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNull_GetParameter;

    return NV_TRUE;
}

NvBool SensorNullTPGARgb_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullRgb_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNullTPGA_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNull_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNull_GetParameter;

    return NV_TRUE;
}

NvBool SensorNullTPGBBayer_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullBayer_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNullTPGB_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNull_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNull_GetParameter;

    return NV_TRUE;
}

NvBool SensorNullTPGBRgb_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorNullRgb_Open;
    hImager->pSensor->pfnClose = SensorNull_Close;
    hImager->pSensor->pfnGetCapabilities = SensorNullTPGB_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorNull_ListModes;
    hImager->pSensor->pfnSetMode = SensorNull_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorNull_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorNull_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorNull_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorNull_GetParameter;

    return NV_TRUE;
}

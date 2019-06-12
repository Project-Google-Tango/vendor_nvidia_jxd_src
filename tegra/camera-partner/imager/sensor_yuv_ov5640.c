/*
 * Copyright (c) 2012-2014  NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#define NV_ENABLE_DEBUG_PRINTS 0

#if (BUILD_FOR_AOS == 0)
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <stdint.h>
#include <ov5640.h>
#endif

#include "nvassert.h"
#include <nvcamera_isp.h>
#include "imager_util.h"
#include "sensor_yuv_ov5640.h"

/****************************************************************************
 * Implementing for a New Sensor:
 *
 * When implementing for a new sensor from this template, there are four
 * phases:
 * Phase 0: Gather sensor information. In phase 0, we should gather the sensor
 *          information for bringup such as sensor capabilities, set mode i2c
 *          write sequence, NvOdmIoAddress in nvodm_query_discovery_imager.h,
 *          and so on.
 * Phase 1: Bring up a sensor. After phase 1, we should be able to do still
 *          captures. Since this is a yuv sensor, there won't be many
 *          interactions between camera and sensor.
 * Phase 2: Fully functioning sensor. After phase 2, the sensor should be fully
 *          functioning as described in nvodm_imager.h
 *
 * To help implementing for a new sensor, the template code will be marked as
 * Phase 0, 1, 2
 ****************************************************************************/



/**
 * ----------------------------------------------------------
 *  Start of Phase 0 code
 * ----------------------------------------------------------
 */

/**
 * Sensor Specific Variables/Defines. Phase 0.
 *
 * If a new sensor code is created from this template, the following
 * variable/defines should be created for the new sensor specifically.
 * 1. A sensor capabilities (NvOdmImagerCapabilities) for this specific sensor
 * 2. A sensor set mode sequence list  (an array of SensorSetModeSequence)
 *    consisting of pairs of a sensor mode and corresponding set mode sequence
 * 3. A set mode sequence for each mode defined in 2.
 */

/**
 * Sensor yuv's capabilities. Phase 0. Sensor Dependent.
 * This is the first thing the camera driver requests. The information here
 * is used to setup the proper interface code (CSI, VIP, pixel type), start
 * up the clocks at the proper speeds, and so forth.
 * For Phase 0, one could start with these below as an example for a yuv
 * sensor and just update clocks from what is given in the reference documents
 * for that device. The FAE may have specified the clocks when giving the
 * reccommended i2c settings.
 */

#define CCT_INCANDESCENT (2950)
#define CCT_DAYLIGHT (6100)
#define CCT_FLUORESCENT (4300)
#define CCT_CLOUDY (7000)

#define LENS_HORIZONTAL_VIEW_ANGLE (60.0f)  //These parameters are set to some value in order to pass CTS
#define LENS_VERTICAL_VIEW_ANGLE   (60.0f)  //We'll have to replace them with appropriate values if we get these sensor specs from some source.
#define LENS_FOCAL_LENGTH          (10.0f)
#define FOCUSER_POSITIONS(ctx)     ((ctx)->Config.pos_high - (ctx)->Config.pos_low)
#define TIME_DIFF(_from, _to)                 \
    (((_from) <= (_to)) ? ((_to) - (_from)) : \
     (~0UL - ((_from) - (_to)) + 1))

static NvOdmImagerCapabilities g_SensorYuvCaps =
{
    "ov5640",  // string identifier, used for debugging
    NvOdmImagerSensorInterface_SerialB, // interconnect type
    {
        NvOdmImagerPixelType_UYVY,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
    6000, // initial sensor clock (in khz)
    { {24000, 88.0f/24.0f}, {0,0} }, // preferred clock profile
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_B,
        NVODMSENSORMIPITIMINGS_DATALANES_TWO,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        1, // USE !CONTINUOUS_CLOCK,
        0x7 // THS_SETTLE is a receiver timing parameter and can vary with CSI clk.
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    0, // FOCUSER_GUID, only set if focuser code available
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

/**
 * A list of sensor mode and its SetModeSequence. Phase 0. Sensor Dependent.
 * When SetMode is called, it searches the list for desired resolution and
 * frame rate and write the SetModeSequence through i2c.
 */
static SensorSetModeSequence g_SensorYuvSetModeSequenceList[] =
{
     // WxH        x, y    fps   set mode sequence
    {{{2592, 1944}, {0, 0},  15, 0}, NULL, NULL},
    {{{1296, 964}, {0, 0},  30, 1.0, 0, NvOdmImagerNoCropMode}, NULL, NULL},
    {{{1920, 1080}, {0, 0},  30, 1.0, 0, NvOdmImagerPartialCropMode}, NULL, NULL},
};

/**
 * ----------------------------------------------------------
 *  Start of Phase 1 code
 * ----------------------------------------------------------
 */

/**
 * Sensor yuv's private context. Phase 1.
 */
typedef struct SensorYuvContextRec
{
    // Phase 1 variables.
    int   camera_fd;
    NvU32 ModeIndex;
    NvU32 NumModes;
    NvOdmImagerPowerLevel PowerLevel;
    NvBool IspSupport;
} SensorYuvContext;

/**
 * Static functions
 * Some compilers require these to be declared upfront.
 */
static NvBool SensorYuv_Open(NvOdmImagerHandle hImager);
static void SensorYuv_Close(NvOdmImagerHandle hImager);

static void
SensorYuv_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities);

static void
SensorYuv_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumberOfModes);

static NvBool
SensorYuv_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult);

static NvBool
SensorYuv_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel);

static NvBool
SensorYuv_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue);

static NvBool
SensorYuv_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue);

static void
SensorYuv_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel);


/**
 * SensorYuv_Open. Phase 1.
 * Initialize sensor yuv and its private context
 */
static NvBool SensorYuv_Open(NvOdmImagerHandle hImager)
{
    SensorYuvContext *pSensorYuvContext = NULL;

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorYuvContext =
        (SensorYuvContext*)NvOdmOsAlloc(sizeof(SensorYuvContext));
    if (!pSensorYuvContext)
        goto fail;

    NvOdmOsMemset(pSensorYuvContext, 0, sizeof(SensorYuvContext));

    pSensorYuvContext->NumModes =
        NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList);
    pSensorYuvContext->ModeIndex =
        pSensorYuvContext->NumModes + 1; // invalid mode
    pSensorYuvContext->IspSupport = NV_FALSE;

#if (BUILD_FOR_AOS == 0)

#ifdef O_CLOEXEC
    pSensorYuvContext->camera_fd = open("/dev/ov5640", O_RDWR|O_CLOEXEC);
#else
    pSensorYuvContext->camera_fd = open("/dev/ov5640", O_RDWR);
#endif // O_CLOEXEC
    if (pSensorYuvContext->camera_fd < 0)
    {
        NvOsDebugPrintf("Can not open camera device: %s\n",
                        strerror(errno));
        goto fail;
    }
    else
    {
        NvOsDebugPrintf("OV5640 Camera fd open as: %d\n", pSensorYuvContext->camera_fd);
        #if 0
        if (ioctl(pSensorYuvContext->camera_fd, OV5640_IOCTL_GET_CONFIG,
                  &pSensorYuvContext->Config) < 0)
        {
            NvOsDebugPrintf("Can not get focuser config: %s\n", strerror(errno));
            close(pSensorYuvContext->camera_fd);
            goto fail;
        }
        #endif
    }

#endif // BUILD_FOR_AOS
    hImager->pSensor->pPrivateContext = pSensorYuvContext;

    return NV_TRUE;

fail:
    NvOdmOsFree(pSensorYuvContext);
    return NV_FALSE;
}

/**
 * SensorYuv_Close. Phase 1.
 * Free the sensor's private context
 */
static void SensorYuv_Close(NvOdmImagerHandle hImager)
{
    SensorYuvContext *pContext = NULL;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
        return;

    pContext = (SensorYuvContext*)hImager->pSensor->pPrivateContext;

#if (BUILD_FOR_AOS == 0)
    close(pContext->camera_fd);
#endif
    // cleanup
    NvOdmOsFree(pContext);
    hImager->pSensor->pPrivateContext = NULL;
}

/**
 * SensorYuv_GetCapabilities. Phase 1.
 * Returnt sensor Yuv's capabilities
 */
static void
SensorYuv_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    // Copy the sensor caps from g_SensorYuvCaps
    NvOdmOsMemcpy(pCapabilities, &g_SensorYuvCaps,
        sizeof(NvOdmImagerCapabilities));
}

/**
 * SensorYuv_ListModes. Phase 1.
 * Return a list of modes that sensor yuv supports.
 * If called with a NULL ptr to pModes, then it just returns the count
 */
static void
SensorYuv_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumberOfModes)
{
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;
    NvS32 i;

    if (pNumberOfModes)
    {
        *pNumberOfModes = (NvS32)pContext->NumModes;
        if (pModes)
        {
            // Copy the modes from g_SensorYuvSetModeSequenceList
            for (i = 0; i < *pNumberOfModes; i++)
                pModes[i] = g_SensorYuvSetModeSequenceList[i].Mode;
        }
    }
}

/**
 * SensorYuv_SetMode. Phase 1.
 * Set sensor yuv to the mode of the desired resolution and frame rate.
 */
static NvBool
SensorYuv_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult)
{
    NvBool Status = NV_FALSE;

#if (BUILD_FOR_AOS == 0)
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;
    NvU32 Index;

    NV_ASSERT(pParameters);

    NV_DEBUG_PRINTF(("Setting resolution to %dx%d\n",
        pParameters->Resolution.width, pParameters->Resolution.height));

    // Find the right entrys in g_SensorYuvSetModeSequenceList that matches
    // the desired resolution and framerate
    for (Index = 0; Index < pContext->NumModes; Index++)
    {
        if ((pParameters->Resolution.width == g_SensorYuvSetModeSequenceList[Index].
             Mode.ActiveDimensions.width) &&
            (pParameters->Resolution.height == g_SensorYuvSetModeSequenceList[Index].
             Mode.ActiveDimensions.height))
            break;
    }

    // No match found
    if (Index == pContext->NumModes)
        return NV_FALSE;

    if (pSelectedMode)
    {
        *pSelectedMode = g_SensorYuvSetModeSequenceList[Index].Mode;
    }

    if (pContext->ModeIndex == Index)
    {
        Status = NV_TRUE;
        NvOsDebugPrintf("Mode unchanged. skip mode set.\n");
        goto setmode_done;
    }

    int ret;
    struct ov5640_mode mode = {
        g_SensorYuvSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_SensorYuvSetModeSequenceList[Index].Mode.
            ActiveDimensions.height
    };
    ret = ioctl(pContext->camera_fd, OV5640_IOCTL_SET_SENSOR_MODE, &mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__,
            strerror(errno));
        return NV_FALSE;
    } else {
        Status = NV_TRUE;

        // Skip some frames to avoid garbage output from sensor. Sensor specific
        // Also use peakFrameRate to simplify the formula
        NvOsSleepMS((NvU32)(6000.0 / g_SensorYuvSetModeSequenceList[Index].Mode.PeakFrameRate));
    }

setmode_done:
    pContext->ModeIndex = Index;

    if (pResult)
    {
        pResult->Resolution = g_SensorYuvSetModeSequenceList[Index].
                              Mode.ActiveDimensions;
        pResult->Exposure = 0;
        NvOdmOsMemset(pResult->Gains, 0, sizeof(NvF32) * 4);
    }
#endif

    return Status;
}

/**
 * SensorYuv_SetPowerLevel. Phase 1
 * Set the sensor's power level
 */
static NvBool
SensorYuv_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
    NvBool Status = NV_TRUE;
#if (BUILD_FOR_AOS == 0)
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("SensorYuv_SetPowerLevel %d\n", PowerLevel));
    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
        #if (BUILD_FOR_AOS == 0)
            if (ioctl(pContext->camera_fd, OV5640_IOCTL_POWER_LEVEL,
                      OV5640_POWER_LEVEL_ON) < 0)
            {
                NvOsDebugPrintf("Can not turn on power: %s\n", strerror(errno));
                return NV_FALSE;
            }
            return NV_TRUE;
        #endif
            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
        #if (BUILD_FOR_AOS == 0)
            if (ioctl(pContext->camera_fd, OV5640_IOCTL_POWER_LEVEL,
                      OV5640_POWER_LEVEL_OFF) < 0)
            {
                NvOsDebugPrintf("Can not turn off power: %s\n", strerror(errno));
                return NV_FALSE;
            }
        #endif
            return NV_TRUE;

        default:
            NV_ASSERT("!Unknown power level\n");
            Status = NV_FALSE;
            break;
    }

    if (Status)
        pContext->PowerLevel = PowerLevel;
#endif
    return Status;
}

static NvBool UnusedISPParameters(NvOdmImagerISPSetting *pSetting)
{
#if NV_ENABLE_DEBUG_PRINTS
    switch(pSetting->attribute)
    {
        case NvCameraIspAttribute_AntiFlicker:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AntiFlicker");
            break;
        case NvCameraIspAttribute_AutoFocusStatus:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFocusStatus");
            break;
        case NvCameraIspAttribute_AutoFrameRate:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFrameRate");
            break;
        case NvCameraIspAttribute_EdgeEnhancement:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_EdgeEnhancement");
            break;
        case NvCameraIspAttribute_Stylize:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_Stylize");
            break;
        case NvCameraIspAttribute_ContinuousAutoFocus:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ContinuousAutoFocus");
            break;
        case NvCameraIspAttribute_ContinuousAutoExposure:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ContinuousAutoExposure");
            break;
        case NvCameraIspAttribute_ContinuousAutoWhiteBalance:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ContinuousAutoWhiteBalance");
            break;
        case NvCameraIspAttribute_AutoNoiseReduction:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoNoiseReduction");
            break;
        case NvCameraIspAttribute_EffectiveISO:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_EffectiveISO");
            break;
        case NvCameraIspAttribute_ExposureTime:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ExposureTime");
            break;
        case NvCameraIspAttribute_FlickerFrequency:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_FlickerFrequency");
            break;
        case NvCameraIspAttribute_WhiteBalanceCompensation:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_WhiteBalanceCompensation");
            break;
        case NvCameraIspAttribute_ExposureCompensation:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ExposureCompensation");
            break;
        case NvCameraIspAttribute_MeteringMode:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_MeteringMode");
            break;
        case NvCameraIspAttribute_StylizeMode:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_StylizeMode");
            break;
        case NvCameraIspAttribute_LumaBias:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_LumaBias");
            break;
        case NvCameraIspAttribute_EmbossStrength:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_EmbossStrength");
            break;
        case NvCameraIspAttribute_ImageGamma:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ImageGamma");
            break;
        case NvCameraIspAttribute_ContrastEnhancement:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ContrastEnhancement");
            break;
        case NvCameraIspAttribute_FocusPosition:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_FocusPosition");
            break;
        case NvCameraIspAttribute_FocusPositionRange:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_FocusPositionRange");
            break;
        case NvCameraIspAttribute_AutoFrameRateRange:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFrameRateRange");
            break;
        case NvCameraIspAttribute_StylizeColorMatrix:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_StylizeColorMatrix");
            break;
        case NvCameraIspAttribute_DefaultYUVConversionMatrix:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_DefaultYUVConversionMatrix");
            break;
        case NvCameraIspAttribute_AutoFocusRegions:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFocusRegions");
            break;
        case NvCameraIspAttribute_AutoFocusRegionMask:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFocusRegionMask");
            break;
        case NvCameraIspAttribute_AutoFocusRegionStatus:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFocusRegionStatus");
            break;
        case NvCameraIspAttribute_Hue:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_Hue");
            break;
        case NvCameraIspAttribute_Saturation:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_Saturation");
            break;
        case NvCameraIspAttribute_Brightness:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_Brightness");
            break;
        case NvCameraIspAttribute_Warmth:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_Warmth");
            break;
        case NvCameraIspAttribute_Stabilization:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_Stabilization");
            break;
        case NvCameraIspAttribute_ExposureTimeRange:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ExposureTimeRange");
            break;
        case NvCameraIspAttribute_ISORange:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ISORange");
            break;
        case NvCameraIspAttribute_SceneBrightness:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_SceneBrightness");
            break;
        case NvCameraIspAttribute_AutoFocusControl:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFocusControl");
            break;
        case NvCameraIspAttribute_WhiteBalanceCCTRange:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_WhiteBalanceCCTRange");
            break;
        case NvCameraIspAttribute_AutoFocusTrigger:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_AutoFocusTrigger");
            break;
        case NvCameraIspAttribute_ExposureProgram:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ExposureProgram");
            break;
        case NvCameraIspAttribute_ExposureMode:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_ExposureMode");
            break;
        case NvCameraIspAttribute_SceneCaptureType:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_SceneCaptureType");
            break;
        case NvCameraIspAttribute_FlashStatus:
            NvOdmOsDebugPrintf("NvCameraIspAttribute_FlashStatus");
            break;
        default:
            NvOdmOsDebugPrintf("ISPSetting: undefined attribute 0x%X = %d.",pSetting->attribute,pSetting->attribute);
            break;
    }
#endif
    return NV_FALSE;
}

/**
 * SensorYuv_SetParameter. Phase 2.
 * Set sensor specific parameters.
 * For Phase 1. This can return NV_TRUE.
 */
static NvBool
SensorYuv_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    NvBool Status = NV_TRUE;
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("YUV_SetParameter(): %x\n", Param));
#if (BUILD_FOR_AOS == 0)
    switch(Param)
    {
        case NvOdmImagerParameter_OptimizeResolutionChange:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));

            // Sensor yuv doesn't support optimized resolution change.
            if (*((NvBool*)pValue) == NV_TRUE)
            {
                Status = NV_FALSE;
            }
            break;

       case NvOdmImagerParameter_ISPSetting:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerISPSetting));
            NvOdmImagerISPSetting *pSetting = (NvOdmImagerISPSetting *)pValue;
            if (pSetting)
            {
                switch(pSetting->attribute)
                {
                  case NvCameraIspAttribute_AutoFocusTrigger:
                  {
                      NvOdmImagerISPSetting *pSetting = (NvOdmImagerISPSetting *)pValue;
                      NvOdmImagerOv5640AfType AFType;
                      int ret = 0;

                      if (*((NvOdmImagerISPSettingDataPointer *)pSetting->pData))
                          AFType= NvOdmImagerOv5640AfType_Trigger;
                      else
                          AFType= NvOdmImagerOv5640AfType_Infinity;
                      // Send the I2C focus command to the Sensor
                      ret = ioctl(pContext->camera_fd, OV5640_IOCTL_SET_AF_MODE, (NvU8)AFType);

                      if (ret < 0) {
                          NvOsDebugPrintf("%s: ioctl to set AF failed %s\n",
                                  __func__, strerror(errno));
                          break;
                      }
                      Status = NV_TRUE;
                      break;
                  }

                  case NvCameraIspAttribute_WhiteBalanceMode:
                  {
                      NvU8 wb_val = OV5640_WB_AUTO;
                      NvOdmImagerISPSetting *pSetting = (NvOdmImagerISPSetting *)pValue;

                      switch (*(NvU32 *)(pSetting->pData)) {
                      case NvCameraIspWhiteBalanceMode_Sunlight:
                          wb_val = OV5640_WB_DAYLIGHT;
                          break;
                      case NvCameraIspWhiteBalanceMode_Cloudy:
                          wb_val = OV5640_WB_CLOUDY;
                          break;
                      case NvCameraIspWhiteBalanceMode_Incandescent:
                          wb_val = OV5640_WB_INCANDESCENT;
                          break;
                      case NvCameraIspWhiteBalanceMode_Fluorescent:
                          wb_val = OV5640_WB_FLUORESCENT;
                          break;
                      case NvCameraIspWhiteBalanceMode_Auto:
                          wb_val = OV5640_WB_AUTO;
                          break;
                      default:
                          NvOsDebugPrintf("%s: Invalid wb mode, use auto instead\n", __func__);
                          wb_val = OV5640_WB_AUTO;
                          break;
                      }

                      int ret = 0;
                      ret = ioctl(pContext->camera_fd, OV5640_IOCTL_SET_WB, wb_val);
                      if (ret < 0) {
                          NvOsDebugPrintf("%s: ioctl to set wb failed %s\n",
                                  __func__, strerror(errno));
                          Status = NV_FALSE;
                          break;
                      }
                      Status = NV_TRUE;
                      break;
                  }
                  default:
                      Status = UnusedISPParameters(pSetting);
                      break;
                  }
            }
            break;

        default:
            NV_DEBUG_PRINTF(("YUV_SetParameter(): %d not supported\n", Param));
            break;
    }
#endif
    return Status;
}

/**
 * SensorYuv_GetParameter. Phase 1.
 * Get sensor specific parameters
 */
static NvBool
SensorYuv_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    NvBool Status = NV_TRUE;
#if (BUILD_FOR_AOS == 0)
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("YUV_GetParameter(): %x\n", Param));

    switch(Param)
    {
        case NvOdmImagerParameter_DeviceStatus:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerDeviceStatus));
            {
                NvOdmImagerDeviceStatus *pStatus;
                pStatus = (NvOdmImagerDeviceStatus *)pValue;
                {
                    uint8_t status;
                    int ret;

                    ret = ioctl(pContext->camera_fd, OV5640_IOCTL_GET_SENSOR_STATUS, &status);
                    if (ret < 0)
                    {
                        NvOsDebugPrintf("ioctl to gets status failed %s\n", strerror(errno));
                        Status = NV_FALSE;
                    }
                    pStatus->Count = 1;
                }
            }
            break;

        case NvOdmImagerParameter_SensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvF32));

            // TODO: Get the real frame rate.
            *((NvF32*)pValue) = 30.0f;
            break;

        case NvOdmImagerParameter_FlashCapabilities:
            Status = NV_FALSE;
            break;

        case NvOdmImagerParameter_SensorFrameRateLimitsAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerFrameRateLimitAtResolution));
            {
                NvOdmImagerFrameRateLimitAtResolution *pData =
                    (NvOdmImagerFrameRateLimitAtResolution *)pValue;

                pData->MinFrameRate = 30.0f;
                pData->MaxFrameRate = 30.0f;
            }
            break;

        case NvOdmImagerParameter_SensorExposureLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvF32) * 2);

            ((NvF32*)pValue)[0] = 1.0f;
            ((NvF32*)pValue)[1] = 1.0f;
            break;

        case NvOdmImagerParameter_SensorGainLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvF32) * 2);

            ((NvF32*)pValue)[0] = 1.0f;
            ((NvF32*)pValue)[1] = 1.0f;
            break;

        case NvOdmImagerParameter_SensorIspSupport:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));

            if (pValue)
            {
                *(NvBool*)pValue = pContext->IspSupport;
            }
            else
            {
                NvOsDebugPrintf("cannot store value in NULL pointer for"\
                    "NvOdmImagerParameter_SensorIspSupport in %s:%d\n",__FILE__,__LINE__);
                Status = NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_FocalLength:
            //In case, focuser is not present the query is redirected here.
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,sizeof(NvF32));
            *((NvF32*)pValue) = LENS_FOCAL_LENGTH;
            break;

        case NvOdmImagerParameter_HorizontalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *((NvF32*)pValue) = LENS_HORIZONTAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *((NvF32*)pValue) = LENS_VERTICAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_ISPSetting:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerISPSetting));
            NvOdmImagerISPSetting *pSetting = (NvOdmImagerISPSetting *)pValue;
            Status = NV_FALSE;
            if (pSetting)
            {
                switch(pSetting->attribute)
                {
                    case NvCameraIspAttribute_FocusPositionRange:
                        {
                            NvCameraIspFocusingParameters *pParams = (NvCameraIspFocusingParameters *)pSetting->pData;
                            NV_DEBUG_PRINTF(("NvCameraIspAttribute_FocusPositionRange"));
                            pParams->sensorispAFsupport = NV_TRUE;
                        }
                        Status = NV_TRUE;
                        break;

                    case NvCameraIspAttribute_AutoFocusStatus:
                        {
                            NvU32 *pU32 = (NvU32 *)pSetting->pData;
                            NvU8 reg = 0;
                            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusStatus\n"));
                            int err = ioctl(pContext->camera_fd, OV5640_IOCTL_GET_AF_STATUS, &reg);
                            if (err < 0)
                            {
                                NvOsDebugPrintf("%s: ioctl get AF status failed! %x\n", __func__, err);
                                Status = NV_FALSE;
                            }
                            else
                            {
                                //Check AF is ready or not
                                if(!reg)
                                    *pU32 = NvCameraIspFocusStatus_Locked;
                                else
                                    *pU32 = NvCameraIspFocusStatus_Busy;
                                Status = NV_TRUE;
                            }
                        }
                        break;

                    default:
                        Status = UnusedISPParameters(pSetting);
                        break;
                 }
            }
            break;

        default:
            NV_DEBUG_PRINTF(("%d not supported\n", Param));
            Status = NV_FALSE;
            break;
    }
#endif
    return Status;
}

/**
 * SensorYuv_GetPowerLevel. Phase 2.
 * Get the sensor's current power level
 */
static void
SensorYuv_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel)
{
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    *pPowerLevel = pContext->PowerLevel;
}

/**
 * SensorYuv_GetHal. Phase 1.
 * return the hal functions associated with sensor yuv
 */
NvBool SensorYuvOV5640_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    hImager->pSensor->pfnOpen = SensorYuv_Open;
    hImager->pSensor->pfnClose = SensorYuv_Close;
    hImager->pSensor->pfnGetCapabilities = SensorYuv_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorYuv_ListModes;
    hImager->pSensor->pfnSetMode = SensorYuv_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorYuv_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorYuv_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorYuv_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorYuv_GetParameter;

    return NV_TRUE;
}

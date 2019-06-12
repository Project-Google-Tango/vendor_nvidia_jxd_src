/*
 * Copyright (c) 2012-2014  NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#define NV_ENABLE_DEBUG_PRINTS 1

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

#endif

#include "nvassert.h"
#include <nvcamera_isp.h>
#include "imager_util.h"
#include "sensor_yuv_sp2529.h"
#include "nvcam_properties_public.h"
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

#define NVCAMERAISP_WHITEBALANCE_MODE_AUTO      (0x0)
#define NVCAMERAISP_WHITEBALANCE_MODE_MANUAL    (0x1)
#define NVCAMERAISP_WHITEBALANCE_MODE_NONE      (0x2)

#define LENS_HORIZONTAL_VIEW_ANGLE (60.0f)  //These parameters are set to some value in order to pass CTS
#define LENS_VERTICAL_VIEW_ANGLE   (60.0f)  //We'll have to replace them with appropriate values if we get these sensor specs from some source.
#define LENS_FOCAL_LENGTH (4.76f)

#define SENSOR_BAYER_EFFECTIVE_PIXEL_LENGTH     1296 // 1616 //
#define SENSOR_BAYER_EFFECTIVE_LINE_HEIGHT      736 //1216// 
#define SENSOR_BAYER_ACTIVE_X0                   8
#define SENSOR_BAYER_ACTIVE_Y0                   8
#define SENSOR_BAYER_ACTIVE_X1                 1287 // 1607// 
#define SENSOR_BAYER_ACTIVE_Y1                 727  //1207 //
#define SENSOR_BAYER_PHYSICAL_X                  1.148f //mm
#define SENSOR_BAYER_PHYSICAL_Y                  0.868f //mm

#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF        (5)
#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH       (0xfffc)
#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE    (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE    (0x002)
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL       (1600)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL      (1200)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL   (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL-\
                                                     SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL)

#define DIFF_INTEGRATION_TIME_OF_ADDITION_MODE      (0.11f)
#define DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE   (0.23f)
#define DIFF_INTEGRATION_TIME_OF_MODE               DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE

#define SENSOR_MIPI_LANE_NUMBER                     (1)
#define SENSOR_DEFAULT_ISO                          100
#define SENSOR_DEFAULT_EXPOSURE                     (0.066f)     // 1/15 sec

static NvOdmImagerCapabilities g_SensorYuvCaps =
{
    "sp2529",  // string identifier, used for debugging
    NvOdmImagerSensorInterface_SerialC, // interconnect type
    {
        NvOdmImagerPixelType_UYVY,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Toward,          // toward or away the main display
    6000,
    { {24000, 24.0f/24.0f}}, // preferred clock profile
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_B,
        NVODMSENSORMIPITIMINGS_DATALANES_TWO,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        0,
        0x4,
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    0, // FOCUSER_GUID, only set if focuser code available
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END,
    0x00, // FlashControlEnabled
    0x00, // AdjustableFlashTiming
    0x00  // isHDR
};

typedef struct SensorYUVSP2529ModeDependentSettings
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 MinFrameLength;
    NvF32 InherentGain;
    NvU32 PllMult;
    NvU32 PllPreDiv;
    NvU32 PllPosDiv;
} SensorYUVSP2529ModeDependentSettings;
/**
 * A list of sensor mode and its SetModeSequence. Phase 0. Sensor Dependent.
 * When SetMode is called, it searches the list for desired resolution and
 * frame rate and write the SetModeSequence through i2c.
 */
static SensorYUVSP2529ModeDependentSettings ModeDependentSettings_1600X1200 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL,
    1.0f,
    0x20,
    2,
    8,
};
static SensorSetModeSequence g_SensorYuvSetModeSequenceList[] =
{
     // WxH        x, y    fps   set mode sequence

{{{1280, 720}, {0, 0},  30, 1.0, 16, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL}, NULL, &ModeDependentSettings_1600X1200},

};
static NvCamAwbMode SP2529_SupportedAwbModes[] =
{
    NvCamAwbMode_Auto,
    NvCamAwbMode_Incandescent,
    NvCamAwbMode_Fluorescent,
    NvCamAwbMode_Daylight,
    NvCamAwbMode_CloudyDaylight
};

static NvCamAwbMode SP2529_SupportedAeModes[] =
{
    NvCamAeMode_Off,
    NvCamAeMode_On
};

static NvCamAwbMode SP2529_SupportedAfModes[] =
{
    NvCamAfMode_Off
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
    int WhiteBalanceMode;
    NvCamProperty_Public_Controls LastPublicControls;
    NvBool SensorInitialized;
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
    if (!hImager || !hImager->pSensor){
        return NV_FALSE;}

    pSensorYuvContext =
        (SensorYuvContext*)NvOdmOsAlloc(sizeof(SensorYuvContext));
    if (!pSensorYuvContext)
        goto fail;

    NvOdmOsMemset(pSensorYuvContext, 0, sizeof(SensorYuvContext));

    pSensorYuvContext->NumModes =
        NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList);
    pSensorYuvContext->ModeIndex =
        pSensorYuvContext->NumModes + 1; // invalid mode

    pSensorYuvContext->WhiteBalanceMode = YUV_Whitebalance_Auto;

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

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext){
        return;}

    pContext = (SensorYuvContext*)hImager->pSensor->pPrivateContext;

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
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;
    NvU32 Index;
    struct SP2529_mode mode;
    int ret;
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
    if (Index == pContext->NumModes){
        return NV_FALSE;}

    if (pSelectedMode)
    {
        *pSelectedMode = g_SensorYuvSetModeSequenceList[Index].Mode;
    }

    if (pContext->ModeIndex == Index){
       return NV_TRUE;}

    mode.xres = g_SensorYuvSetModeSequenceList[Index].Mode.ActiveDimensions.width;
    mode.yres = g_SensorYuvSetModeSequenceList[Index].Mode.ActiveDimensions.height;

    ret = ioctl(pContext->camera_fd, SP2529_IOCTL_SET_SENSOR_MODE, &mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed\n", __func__);
        return NV_FALSE;
    }

    pContext->ModeIndex = Index;

    if (pResult)
    {
        pResult->Resolution = g_SensorYuvSetModeSequenceList[Index].
                              Mode.ActiveDimensions;
        pResult->Exposure = 0;
        NvOdmOsMemset(pResult->Gains, 0, sizeof(NvF32) * 4);
    }

    pContext->SensorInitialized = NV_TRUE;

    return NV_TRUE;
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

    if (pContext->PowerLevel == PowerLevel){
        return NV_TRUE;}

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
        #if (BUILD_FOR_AOS == 0)
			 pContext->camera_fd = open("/dev/sp2529", O_RDWR);
            if (pContext->camera_fd < 0)
            {
                NvOsDebugPrintf("Can not open camera device\n");
                Status = NV_FALSE;
            }
            else
            {
                NV_DEBUG_PRINTF(("Camera fd open as: %d\n", pContext->camera_fd));
                Status = NV_TRUE;
            }
            if (ioctl(pContext->camera_fd, SP2529_IOCTL_POWER_LEVEL,
                      SP2529_POWER_LEVEL_ON) < 0)
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
            if (ioctl(pContext->camera_fd, SP2529_IOCTL_POWER_LEVEL,
                      SP2529_POWER_LEVEL_OFF) < 0)
            {
                NvOsDebugPrintf("Can not turn off power: %s\n", strerror(errno));
                return NV_FALSE;
            }
        #endif
		    if(pContext->camera_fd >= 0){
                close(pContext->camera_fd);
                pContext->camera_fd = -1;
            }
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
  //  SensorYuvContext *pContext =
   //     (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("YUV_SetParameter(): %d\n", Param));
#if (BUILD_FOR_AOS == 0)
    switch(Param)
    {
        case NvOdmImagerParameter_OptimizeResolutionChange:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));

            //Support preview in low resolution, high frame rate
            return NV_TRUE;
            break;

        case NvOdmImagerParameter_CustomizedBlockInfo:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerCustomInfo));

            if (pValue)
            {
                NvOdmImagerCustomInfo *pCustom =
                    (NvOdmImagerCustomInfo *)pValue;
                if (pCustom->pCustomData && pCustom->SizeOfData)
                {
                    NV_DEBUG_PRINTF(("nvodm_imager: (%d) %s\n",
                          pCustom->SizeOfData,
                          pCustom->pCustomData));
                    // one could parse and interpret the data packet here
                }
                else
                {
                    NV_DEBUG_PRINTF(("nvodm_imager: null packet\n"));
                }
            }
            else
            {
                NV_DEBUG_PRINTF(("nvodm_imager: null packet\n"));
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

                      Status = NV_TRUE;
                      break;
                  }

                  case NvCameraIspAttribute_WhiteBalanceMode:
                  {

                      Status = NV_TRUE;
                      break;
                  }
                  default:
                      Status = UnusedISPParameters(pSetting);
                      break;
                  }
            }
            break;

        case NvOdmImagerParameter_IspPublicControls:

            return NV_FALSE;
        case NvOdmImagerParameter_SelfTest:
            // not implemented.
            return NV_FALSE; //default return FAIL, for most part is unsupported.
        case NvOdmImagerParameter_AWBLock:
            return NV_TRUE; /* no support yet */
        case NvOdmImagerParameter_AELock:
            return NV_TRUE; /* no support yet */
        default:
            NV_DEBUG_PRINTF(("YUV_SetParameter(): %d not supported\n", Param));
            return NV_FALSE;
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
  //  SensorYuvContext *pContext =
   //     (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("YUV_GetParameter(): %d\n", Param));

    switch(Param)
    {
        case NvOdmImagerParameter_MaxAperture:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            (*(NvF32 *)pValue) = 2.5261f;
            return NV_TRUE;

        case NvOdmImagerParameter_FNumber:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            (*(NvF32 *)pValue) = 2.4f;
            return NV_TRUE;

        case NvOdmImagerParameter_FocalLength:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvF32));
            {
                NvF32 *pFocalLength;
                pFocalLength = (NvF32 *)pValue;
                *pFocalLength = 2.08;
            }
            return NV_TRUE;
        case NvOdmImagerParameter_DeviceStatus:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerDeviceStatus));
            {
                NvOdmImagerDeviceStatus *pStatus;
                pStatus = (NvOdmImagerDeviceStatus *)pValue;
                {
                    // Pick your own useful registers to use for debug.
                    // If the camera hangs, these register values are printed
                    // Sensor Dependent.
                    pStatus->Count = 0;
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
            return NV_FALSE;

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

        case NvOdmImagerParameter_AWBLock:
            return NV_TRUE;
        case NvOdmImagerParameter_AELock:
            return NV_TRUE;
        case NvOdmImagerParameter_SensorIspSupport:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));

            if (pValue)
            {
                *(NvBool*)pValue = NV_FALSE;
                return NV_TRUE;
            }
            else
            {
                NvOsDebugPrintf("cannot store value in NULL pointer for"\
                    "NvOdmImagerParameter_SensorIspSupport in %s:%d\n",__FILE__,__LINE__);
                return NV_FALSE;
            }
            break;
        case NvOdmImagerParameter_StereoCapable:
            NV_DEBUG_PRINTF(("StereoCapable is Not Support\n"));
            return NV_TRUE;

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

                            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusStatus\n"));
      
                            Status = NV_TRUE;
                            
                        }
                        break;

                    default:
                        Status = UnusedISPParameters(pSetting);
                        break;
                 }
            }
            break;

        default:
            NV_DEBUG_PRINTF(("%s: Not Implemented %d\n", __func__, (int)Param));
            return NV_FALSE;
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

static NvBool SensorYuv_GetStaticProperties(
    NvOdmImagerHandle hImager,
    SensorStaticProperties *pProperties)
{
    SensorYUVSP2529ModeDependentSettings *pModeSettings;
    ModeProperties *pModeProperty;
    NvBool Status = NV_TRUE;
    NvBool bNewInst = NV_FALSE;
    NvU32 idx, ModeNum;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (!hImager || !hImager->pSensor)
    {
        return NV_FALSE;
    }

    // skip in the case camera-core already opened the sensor
    if (!hImager->pSensor->pPrivateContext)
    {
        Status = SensorYuv_Open(hImager);
        if (Status == NV_FALSE)
        {
            return NV_FALSE;
        }
        bNewInst = NV_TRUE;
    }

    pProperties->pCap = &g_SensorYuvCaps;

    // get num of modes supported
    SensorYuv_ListModes(hImager, NULL, (NvS32 *)&pProperties->NumOfModes);
    NV_DEBUG_PRINTF(("%s %d\n", __func__, pProperties->NumOfModes));

    if (pProperties->NumOfModes > NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList))
    {
        ModeNum = NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList);
    }
    else
    {
        ModeNum = pProperties->NumOfModes;
    }
    // get mode settings
    for (idx = 0; idx < ModeNum; idx++) {
        pModeSettings = (SensorYUVSP2529ModeDependentSettings*)
            g_SensorYuvSetModeSequenceList[idx].pModeDependentSettings;
        pModeProperty = &pProperties->ModeList[idx];
        NvOdmOsMemcpy(&pModeProperty->ActiveSize,
            &g_SensorYuvSetModeSequenceList[idx].Mode.ActiveDimensions,
            sizeof(pModeProperty->ActiveSize));
        pModeProperty->PeakFrameRate =
            g_SensorYuvSetModeSequenceList[idx].Mode.PeakFrameRate;
        pModeProperty->LineLength = pModeSettings->LineLength;
        pModeProperty->FrameLength = pModeSettings->FrameLength;
        pModeProperty->CoarseTime = pModeSettings->CoarseTime;
        pModeProperty->MinFrameLength = pModeSettings->MinFrameLength;
        pModeProperty->InherentGain = pModeSettings->InherentGain;
        pModeProperty->PllMult = pModeSettings->PllMult;
        pModeProperty->PllPreDiv = pModeSettings->PllPreDiv;
        pModeProperty->PllPosDiv = pModeSettings->PllPosDiv;
    }

    // get sensor settings
    pProperties->ActiveArraySize.left = SENSOR_BAYER_ACTIVE_X0;
    pProperties->ActiveArraySize.right = SENSOR_BAYER_ACTIVE_X1;
    pProperties->ActiveArraySize.top = SENSOR_BAYER_ACTIVE_Y0;
    pProperties->ActiveArraySize.bottom = SENSOR_BAYER_ACTIVE_Y1;
    pProperties->PixelArraySize.width = SENSOR_BAYER_EFFECTIVE_PIXEL_LENGTH;
    pProperties->PixelArraySize.height = SENSOR_BAYER_EFFECTIVE_LINE_HEIGHT;
    pProperties->PhysicalSize.x = SENSOR_BAYER_PHYSICAL_X;
    pProperties->PhysicalSize.y =  SENSOR_BAYER_PHYSICAL_Y;
    pProperties->MaxFrameLength = SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH;
    pProperties->MaxCoarseTime = SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE;
    pProperties->MinCoarseTime = SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE;
    pProperties->IntegrationTime = DIFF_INTEGRATION_TIME_OF_MODE * 2;

    // get gain settings
    pProperties->MinGain = 1.0f;
    pProperties->MaxGain = 16.0f;

    // get lens settings
    pProperties->FocalLength = LENS_FOCAL_LENGTH;

    if (bNewInst)
    {
        SensorYuv_Close(hImager);
    }
    return Status;
}
static NvBool SensorYuv_GetISPStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerISPStaticProperty *pProperties)
{
    NvBool Status = NV_TRUE;
    NvBool bNewInst = NV_FALSE;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (!hImager || !hImager->pSensor)
    {
        return NV_FALSE;
    }

    // skip in the case camera-core already opened the sensor
    if (!hImager->pSensor->pPrivateContext)
    {
        Status = SensorYuv_Open(hImager);
        if (Status == NV_FALSE)
        {
            return NV_FALSE;
        }
        bNewInst = NV_TRUE;
    }

     // Load AE static properties.
    NvOdmOsMemcpy(pProperties->SupportedAeModes.Property,
                  SP2529_SupportedAeModes,
                  sizeof(SP2529_SupportedAeModes));
    pProperties->SupportedAeModes.Size =
        NV_ARRAY_SIZE(SP2529_SupportedAeModes);

    // Load supported framerates.
    {
        int count = 0;
        while (count < NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList))
        {
            pProperties->SupportedFpsRanges.Points[count].x = 15.0f; // Min
            pProperties->SupportedFpsRanges.Points[count].y = 30.0f; // Max
            count++;
        }
        pProperties->SupportedFpsRanges.Size = count;
    }

    // Load AWB static properties.
    NvOdmOsMemcpy(pProperties->SupportedAwbModes.Property,
                  SP2529_SupportedAwbModes,
                  sizeof(SP2529_SupportedAwbModes));
    pProperties->SupportedAwbModes.Size =
        NV_ARRAY_SIZE(SP2529_SupportedAwbModes);

    // Load AF static properties.
    NvOdmOsMemcpy(pProperties->SupportedAfModes.Property,
                  SP2529_SupportedAfModes,
                  sizeof(SP2529_SupportedAfModes));
    pProperties->SupportedAfModes.Size =
        NV_ARRAY_SIZE(SP2529_SupportedAfModes);


    if (bNewInst)
    {
        SensorYuv_Close(hImager);
    }
    return Status;
}

static NvBool SensorYuv_GetISPControlProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerYUVControlProperty *pProperties)
{
    NvBool Status = NV_TRUE;
    SensorYuvContext *pContext;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        return NV_FALSE;
    }
    pContext = (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    // Load AE defaults.
    pProperties->AeLock = NV_FALSE;
    pProperties->AeMode = NvCamAeMode_OnAutoFlash;
    pProperties->SensorSensitivity = SENSOR_DEFAULT_ISO;
    pProperties->SensorExposureTime = SENSOR_DEFAULT_EXPOSURE;
    pProperties->AeAntibandingMode = NvCamAeAntibandingMode_AUTO;
    pProperties->AeExposureCompensation = 0;
    pProperties->AeRegions.numOfRegions=0;
    pProperties->AePrecaptureTrigger = NvCamAePrecaptureTrigger_Idle;

    // Load AF Defaults.
    pProperties->AfMode = NvCamAfMode_Off;
    pProperties->AfTrigger = NvCamAfTrigger_Idle;
    pProperties->AfRegions.numOfRegions = 0;

    // Load AWB defaults.
    pProperties->AwbMode = NvCamAwbMode_Auto;
    pProperties->AwbLock = NV_FALSE;
    pProperties->AwbRegions.numOfRegions = 0;

    //Load Effect Mode default control properties
    pProperties->ColorCorrectionMode = NvOdmImagerColorCorrectionMode_Matrix;
    pProperties->ColorEffectsMode = NvOdmImagerColorEffectsMode_Off;
    pProperties->ControlMode = NvOdmImagerControlMode_Auto;
    pProperties->SceneMode = NvCamSceneMode_Unsupported;
    pProperties->VideoStabalizationMode = NV_FALSE;
    pProperties->DemosaicMode = NvOdmImagerDemosaicMode_Fast;
    pProperties->EdgeEnhanceMode = NvOdmImagerEdgeEnhanceMode_Off;
    //pProperties->EdgeEnhanceStrength;
    pProperties->FlashMode = NvOdmImagerFlashMode_Off;
    //pProperties->FlashFiringPower;
    //pProperties->FlashFiringTime;
    pProperties->GeometricMode = NvOdmImagerGeometricMode_Off;
    //pProperties->GeometricStrength;
    pProperties->HotPixelMode = NvOdmImagerHotPixelMode_Off;
    pProperties->NoiseReductionMode = NvOdmImagerNoiseReductionMode_HighQuality;
    pProperties->NoiseReductionStrength = 1.0f;
    pProperties->ScalerCropRegion.left = SENSOR_BAYER_ACTIVE_X0;
    pProperties->ScalerCropRegion.top = SENSOR_BAYER_ACTIVE_Y0;
    pProperties->ScalerCropRegion.right =
        SENSOR_BAYER_ACTIVE_X1 - SENSOR_BAYER_ACTIVE_X0 + 1;
    pProperties->ScalerCropRegion.bottom =
        SENSOR_BAYER_ACTIVE_Y1 - SENSOR_BAYER_ACTIVE_Y0 + 1;
    pProperties->ShadingMode = NV_FALSE;
    //pProperties->ShadingStrength;
    pProperties->StatsFaceDetectMode = NvOdmImagerFaceDetectMode_Off;
    pProperties->StatsHistogramMode = NV_FALSE;
    pProperties->StatsSharpnessMode = NV_FALSE;
    if ((pContext->camera_fd >=0) && (pContext->ModeIndex < pContext->NumModes))
    {
        pProperties->LedTransmit = NV_TRUE;
    }
    else
    {
        pProperties->LedTransmit = NV_FALSE;
    }

    return Status;
}

static NvBool SensorYuv_GetISPDynamicProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerYUVDynamicProperty *pProperties)
{
    NvBool Status = NV_TRUE;
    SensorYuvContext *pContext;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        return NV_FALSE;
    }
    pContext = (SensorYuvContext*)hImager->pSensor->pPrivateContext;

    pProperties->AwbMode = pContext->LastPublicControls.AwbMode;
    pProperties->SensorExposureTime = SENSOR_DEFAULT_EXPOSURE;
    pProperties->SensorSensitivity = SENSOR_DEFAULT_ISO;
    pProperties->AeState = NvOdmImagerAeState_Converged;
	
//	pProperties->FrameDuration = SENSOR_DEFAULT_FRAME_DURATION;

    return Status;
}
/**
 * SensorYuv_GetHal. Phase 1.
 * return the hal functions associated with sensor yuv
 */
NvBool SensorYuvSP2529_GetHal(NvOdmImagerHandle hImager)
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
    hImager->pSensor->pfnStaticQuery = SensorYuv_GetStaticProperties;
   	hImager->pSensor->pfnISPStaticQuery = SensorYuv_GetISPStaticProperties;
   	hImager->pSensor->pfnISPControlQuery = SensorYuv_GetISPControlProperties;
   	hImager->pSensor->pfnISPDynamicQuery = SensorYuv_GetISPDynamicProperties;

    return NV_TRUE;
}

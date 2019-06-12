/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#if (BUILD_FOR_AOS == 0)

#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "mt9m114.h"

#include "imager_util.h"
#include "nvodm_services.h"
#include "imager_nvc.h"
#include "sensor_yuv_mt9m114.h"
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

#define LENS_HORIZONTAL_VIEW_ANGLE (60.0f)  //These parameters are set to some value in order to pass CTS
#define LENS_VERTICAL_VIEW_ANGLE   (60.0f)  //We'll have to replace them with appropriate values if we get these sensor specs from some source.
#define LENS_FOCAL_LENGTH (4.76f)

#define SENSOR_BAYER_EFFECTIVE_PIXEL_LENGTH      1296
#define SENSOR_BAYER_EFFECTIVE_LINE_HEIGHT       976
#define SENSOR_BAYER_ACTIVE_X0                   8
#define SENSOR_BAYER_ACTIVE_Y0                   8
#define SENSOR_BAYER_ACTIVE_X1                   1287
#define SENSOR_BAYER_ACTIVE_Y1                   967
#define SENSOR_BAYER_PHYSICAL_X                  4.09f //mm
#define SENSOR_BAYER_PHYSICAL_Y                  3.89f //mm

#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF        (5)
#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH       (0xfffc)
#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE    (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE    (0x002)
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL       (0x0680)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL      (0x0348)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL   (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL-\
                                                     SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL)

#define DIFF_INTEGRATION_TIME_OF_ADDITION_MODE      (0.11f)
#define DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE   (0.23f)
#define DIFF_INTEGRATION_TIME_OF_MODE               DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE

#define SENSOR_MIPI_LANE_NUMBER                     (1)
#define SENSOR_DEFAULT_ISO                          100
#define SENSOR_DEFAULT_EXPOSURE                     (0.066f)     // 1/15 sec

static NvBool IspSupport = NV_FALSE;

static NvOdmImagerCapabilities g_SensorYuvCaps =
{
    "MT9M114",  // string identifier, used for debugging
#if (BUILD_FOR_ARDBEG == 1) // this is only needed for Ardbeg
    NvOdmImagerSensorInterface_SerialC, // interconnect type
#else
    NvOdmImagerSensorInterface_SerialA, // interconnect type
#endif
    {
        NvOdmImagerPixelType_UYVY,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Toward,          // toward or away the main display
    24000,
    { {24400, 125.0f/24.0f}}, // preferred clock profile
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_B,
        NVODMSENSORMIPITIMINGS_DATALANES_ONE,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        0,
        0xa,
    }, // serial settings (CSI)
    { 0, 0 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    0, // FOCUSER_GUID, only set if focuser code available
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END,
    0x00, // FlashControlEnabled
    0x00, // AdjustableFlashTiming
    0x00 // isHDR
};

typedef struct SensorYUVMT9M114ModeDependentSettingsRec
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 MinFrameLength;
    NvF32 InherentGain;
    NvU32 PllMult;
    NvU32 PllPreDiv;
    NvU32 PllPosDiv;
} SensorYUVMT9M114ModeDependentSettings;

static DevCtrlReg16 SetModeSequence_1280x960[] =
{ // Not used, don't care.
   // {SEQUENCE_WAIT_MS, 100},
  //  {SEQUENCE_END, 0x0000}
};

static SensorYUVMT9M114ModeDependentSettings ModeDependentSettings_1280X960 =
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
    {{{1280, 960}, {0, 0},  30, 1.0, 4, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL}, SetModeSequence_1280x960, &ModeDependentSettings_1280X960},
};

static NvCamAwbMode MT9M114_SupportedAwbModes[] =
{
    NvCamAwbMode_Auto,
    NvCamAwbMode_Incandescent,
    NvCamAwbMode_Fluorescent,
    NvCamAwbMode_Daylight,
    NvCamAwbMode_CloudyDaylight
};

static NvCamAwbMode MT9M114_SupportedAeModes[] =
{
    NvCamAeMode_Off,
    NvCamAeMode_On
};

static NvCamAwbMode MT9M114_SupportedAfModes[] =
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
    NvU16 MinFrameRate
}SensorYuvContext;

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
    struct mt9m114_mode mode;
    int ret;
    NV_ASSERT(pParameters);

    NvOdmOsDebugPrintf("Setting resolution to %dx%d\n",
        pParameters->Resolution.width, pParameters->Resolution.height);

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

    ret = ioctl(pContext->camera_fd, MT9M114_SENSOR_IOCTL_SET_MODE, &mode);

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
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;
    if (pContext->PowerLevel == PowerLevel){
        return NV_TRUE;}

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
#ifdef O_CLOEXEC
            pContext->camera_fd = open("/dev/mt9m114", O_RDWR|O_CLOEXEC);
#else
            pContext->camera_fd = open("/dev/mt9m114", O_RDWR);
#endif
            if (pContext->camera_fd < 0)
            {
                NvOsDebugPrintf("Can not open camera device\n");
                Status = NV_FALSE;
            }
            else
            {
                NvOsDebugPrintf("Camera fd open as: %d\n", pContext->camera_fd);
                Status = NV_TRUE;
            }
            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
            NV_DEBUG_PRINTF(("Camera fd close (MT9M114)\n"));
            if(pContext->camera_fd >= 0){
                close(pContext->camera_fd);
                pContext->camera_fd = -1;
            }
            break;

        default:
            NV_ASSERT("!Unknown power level\n");
            Status = NV_FALSE;
            break;
    }

    if (Status)
        pContext->PowerLevel = PowerLevel;

    return Status;
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
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;
    switch(Param)
    {
        case NvOdmImagerParameter_SensorInputClock:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                                             sizeof(NvOdmImagerClockProfile));

            {
                // Verify the input clock is valid.
                NvU32 InClock =
                    ((NvOdmImagerClockProfile*)pValue)->ExternalClockKHz;

                if (InClock < g_SensorYuvCaps.InitialSensorClockRateKHz){
                    return NV_FALSE;}
            }
            break;
        case NvOdmImagerParameter_OptimizeResolutionChange:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));

            //Support preview in low resolution, high frame rate
            return NV_TRUE;
            break;
       case NvOdmImagerParameter_CustomizedBlockInfo:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerCustomInfo));
             NvOdmImagerCustomInfo *pCustom = (NvOdmImagerCustomInfo *)pValue;

            if (pCustom)
            {
              if (pCustom->pCustomData && (pCustom->SizeOfData >=8))
              {
                  int cmd,value;
                  int *pInt;
                  // unsigned char c;

                  pInt=(int *)(pCustom->pCustomData);
                  cmd = pInt[0];
                  value = pInt[1];
                  switch(cmd)
                  {
                    case 0xa0:  //color effect
                    {
                        NvU16 coloreffect = (NvU16)value;
                        ioctl(pContext->camera_fd, MT9M114_SENSOR_IOCTL_SET_COLOR_EFFECT, &coloreffect);
                        break;
                    }
                    default:
                      break;
                  }

              }
            }
            return NV_TRUE;
            break;
        case NvOdmImagerParameter_ISPSetting:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerISPSetting));
            NvOdmImagerISPSetting *p = (NvOdmImagerISPSetting *)pValue;
            if (p)
            {
              switch(p->attribute)
              {
                case NvCameraIspAttribute_AutoExposureRegions:
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusTrigger:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusTrigger"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusStatus:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusStatus"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AntiFlicker:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AntiFlicker"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFrameRate:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFrameRate"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_EdgeEnhancement:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_EdgeEnhancement"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Stylize:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_Stylize"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContinuousAutoFocus:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContinuousAutoFocus"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContinuousAutoExposure:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContinuousAutoExposure"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContinuousAutoWhiteBalance:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContinuousAutoWhiteBalance"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoNoiseReduction:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoNoiseReduction"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_EffectiveISO:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_EffectiveISO"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ExposureTime:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ExposureTime"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_FlickerFrequency:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_FlickerFrequency"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_WhiteBalanceCompensation:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_WhiteBalanceCompensation"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ExposureCompensation:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ExposureCompensation"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_MeteringMode:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_MeteringMode"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_StylizeMode:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_StylizeMode"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_LumaBias:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_LumaBias"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_EmbossStrength:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_EmbossStrength"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ImageGamma:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ImageGamma"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContrastEnhancement:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContrastEnhancement"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_FocusPosition:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_FocusPosition"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_FocusPositionRange:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_FocusPositionRange"));
                    return NV_TRUE; /* no support yet */

                case NvCameraIspAttribute_WhiteBalanceMode:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_WhiteBalanceMode"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_WhiteBalanceCCTRange:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_WhiteBalanceMode"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_AutoFrameRateRange:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFrameRateRange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_StylizeColorMatrix:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_StylizeColorMatrix"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_DefaultYUVConversionMatrix:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_DefaultYUVConversionMatrix"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusRegions:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusRegions"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusRegionMask:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusRegionMask"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusRegionStatus:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusRegionStatus"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Hue:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_Hue"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Saturation:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_Saturation"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Brightness:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_Brightness"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Warmth:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_Warmth"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Stabilization:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_Stabilization"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ExposureTimeRange:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ExposureTimeRange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ISORange:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_ISORange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_SceneBrightness:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_SceneBrightness"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusControl:
                    NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusControl"));
                    return NV_FALSE; /* no support yet */

                default:
                  break;
                }
            }
            return NV_FALSE; //default return FAIL, for most part is unsupported.
            break;

        case NvOdmImagerParameter_IspPublicControls:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvCamProperty_Public_Controls));
            {
                NvCamProperty_Public_Controls *pPublicControls =
                    (NvCamProperty_Public_Controls *) pValue;
                int ret;

                // sensor is not intialized, ignore all controls
                if (!pContext->SensorInitialized)
                    return NV_TRUE;

                // EV compensation
                if (pContext->LastPublicControls.AeExposureCompensation !=
                    pPublicControls->AeExposureCompensation)
                {
                    NvS16 ev = (NvS16) pPublicControls->AeExposureCompensation;
                    NV_DEBUG_PRINTF(("Set EV compensation to %d\n", ev));
                    ret = ioctl(pContext->camera_fd,
                                MT9M114_SENSOR_IOCTL_SET_EV,
                                &ev);
                    if (ret)
                    {
                        NvOsDebugPrintf("Failed to set EV\n");
                        return NV_FALSE;
                    }
                }

                // manual white balance
                if (pContext->LastPublicControls.AwbMode !=
                    pPublicControls->AwbMode)
                {
                    NV_DEBUG_PRINTF(("Set WB to %d\n",
                            pPublicControls->AwbMode));
                    ret = ioctl(pContext->camera_fd,
                                MT9M114_SENSOR_IOCTL_SET_WHITE_BALANCE,
                                &pPublicControls->AwbMode);
                    if (ret)
                    {
                        NvOsDebugPrintf("Failed to set WB\n");
                        return NV_FALSE;
                    }
                }

                if (pContext->LastPublicControls.CaptureIntent !=
                    pPublicControls->CaptureIntent)
                {
                    NvU16 MinFrameRate = 0;
                    NvCameraCoreUseCase UseCase = pPublicControls->CaptureIntent;
                    switch(UseCase)
                        {

                        case NvCameraCoreUseCase_Preview:
                            NV_DEBUG_PRINTF(("Setting to var frame rate\n"));
                            MinFrameRate = 0x0780;
                            break;

                        case NvCameraCoreUseCase_Still:
                            NV_DEBUG_PRINTF(("No change in frame rate for still capture\n"));
                            break;

                        case NvCameraCoreUseCase_Video:
                            NV_DEBUG_PRINTF(("Setting to fix frame rate for video\n"));
                            MinFrameRate = 0x1E02;
                            break;

                        case NvCameraCoreUseCase_VideoSnapshot:
                            NV_DEBUG_PRINTF(("No change in frame rate for snapshot\n"));
                            break;

                        case NvCameraCoreUseCase_ZSL:
                            NV_DEBUG_PRINTF(("No change in frame rate for ZSL\n"));
                            break;

                        default:
                            NV_DEBUG_PRINTF(("Incorrect Use Case\n"));
                            break;
                       }

                    if (MinFrameRate != 0 && MinFrameRate != pContext->MinFrameRate)
                    {
                        ret = ioctl(pContext->camera_fd,
                                    MT9M114_SENSOR_IOCTL_SET_MIN_FPS,
                                    &MinFrameRate);
                        if (ret)
                        {
                            NvOsDebugPrintf("Failed to set min frame rate\n");
                        }
                        pContext->MinFrameRate = MinFrameRate;
                    }
                }

                // save current public controls
                memcpy(&pContext->LastPublicControls,
                       pPublicControls,
                       sizeof(NvCamProperty_Public_Controls));
            }
            return NV_TRUE;


        case NvOdmImagerParameter_SelfTest:
            // not implemented.
            return NV_FALSE; //default return FAIL, for most part is unsupported.
        case NvOdmImagerParameter_AWBLock:
            NV_DEBUG_PRINTF(("CAM ISP AWB lock"));
            return NV_TRUE; /* no support yet */
        case NvOdmImagerParameter_AELock:
            NV_DEBUG_PRINTF(("CAM ISP AE lock"));
            return NV_TRUE; /* no support yet */
        default:
            //NV_ASSERT(!"Not Implemented");
            NV_FALSE;
            break;
    }

    return NV_TRUE;
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
        case NvOdmImagerParameter_ISPSetting:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                    sizeof(NvOdmImagerISPSetting));
            NvOdmImagerISPSetting *p = (NvOdmImagerISPSetting *)pValue;
            if (p)
            {
              switch(p->attribute)
              {
                case NvCameraIspAttribute_AntiFlicker:
                    NV_DEBUG_PRINTF(("ISP AntiFlicker"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFrameRate:
                    NV_DEBUG_PRINTF(("ISP AutoFrameRate"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_EdgeEnhancement:
                    NV_DEBUG_PRINTF(("ISP EdgeEnhancement"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Stylize:
                    NV_DEBUG_PRINTF(("ISP Stylize"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContinuousAutoFocus:
                    NV_DEBUG_PRINTF(("ISP ContinuousAutoFocus"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContinuousAutoExposure:
                    NV_DEBUG_PRINTF(("ISP ContinuousAutoExposure"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContinuousAutoWhiteBalance:
                    NV_DEBUG_PRINTF(("ISP ContinuousAutoWhiteBalance"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoNoiseReduction:
                    NV_DEBUG_PRINTF(("ISP AutoNoiseReduction"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_EffectiveISO:
                    NV_DEBUG_PRINTF(("ISP EffectiveISO"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_ExposureTime:
                    NV_DEBUG_PRINTF(("ISP ExposureTime"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_FlickerFrequency:
                    NV_DEBUG_PRINTF(("ISP FlickerFrequency"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_WhiteBalanceCompensation:
                    NV_DEBUG_PRINTF(("ISP WhiteBalanceCompensation"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ExposureCompensation:
                    NV_DEBUG_PRINTF(("ISP ExposureCompensation"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_MeteringMode:
                    NV_DEBUG_PRINTF(("ISP MeteringMode"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_StylizeMode:
                    NV_DEBUG_PRINTF(("ISP StylizeMode"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_LumaBias:
                    NV_DEBUG_PRINTF(("ISP LumaBias"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_EmbossStrength:
                    NV_DEBUG_PRINTF(("ISP EmbossStrength"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ImageGamma:
                    NV_DEBUG_PRINTF(("ISP ImageGamma"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ContrastEnhancement:
                    NV_DEBUG_PRINTF(("ISP ContrastEnhancement"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_FocusPosition:
                    NV_DEBUG_PRINTF(("ISP FocusPosition"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_FocusPositionRange:
                    {
                        NvCameraIspFocusingParameters *pParams = (NvCameraIspFocusingParameters *)p->pData;
                        NV_DEBUG_PRINTF(("NvCameraIspAttribute_FocusPositionRange"));
                        pParams->sensorispAFsupport = NV_FALSE;
                    }
                    return NV_TRUE; /* no support yet */

                case NvCameraIspAttribute_WhiteBalanceCCTRange:
                    NV_DEBUG_PRINTF(("ISP WhiteBalanceCCTRange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFrameRateRange:
                    NV_DEBUG_PRINTF(("ISP AutoFrameRateRange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_StylizeColorMatrix:
                    NV_DEBUG_PRINTF(("ISP StylizeColorMatrix"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_DefaultYUVConversionMatrix:
                    NV_DEBUG_PRINTF(("ISP DefaultYUVConversionMatrix"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusRegions:
                    NV_DEBUG_PRINTF(("ISP AutoFocusRegions"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusRegionMask:
                    NV_DEBUG_PRINTF(("ISP AutoFocusRegionMask"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusRegionStatus:
                    NV_DEBUG_PRINTF(("ISP AutoFocusRegionStatus"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Hue:
                    NV_DEBUG_PRINTF(("ISP Hue"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Saturation:
                    NV_DEBUG_PRINTF(("ISP Saturation"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Brightness:
                    NV_DEBUG_PRINTF(("ISP Brightness"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Warmth:
                    NV_DEBUG_PRINTF(("ISP Warmth"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_Stabilization:
                    NV_DEBUG_PRINTF(("ISP Stabilization"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ExposureTimeRange:
                    NV_DEBUG_PRINTF(("ISP ExposureTimeRange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_ISORange:
                    NV_DEBUG_PRINTF(("ISP ISORange"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_SceneBrightness:
                    NV_DEBUG_PRINTF(("ISP SceneBrightness"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusControl:
                    NV_DEBUG_PRINTF(("ISP AutoFocusControl"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_AutoFocusTrigger:
                    NV_DEBUG_PRINTF(("ISP AutoFocusTrigger"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_AutoFocusStatus:
                    NV_DEBUG_PRINTF(("ISP AutoFocusStatus"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_ExposureProgram:
                    NV_DEBUG_PRINTF(("ISP ExposureProgram"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_ExposureMode:
                    NV_DEBUG_PRINTF(("ISP ExposureMode"));
                    return NV_FALSE; /* no support yet */

                case NvCameraIspAttribute_SceneCaptureType:
                    NV_DEBUG_PRINTF(("ISP SceneCaptureType"));
                    return NV_FALSE; /* no support yet */
                case NvCameraIspAttribute_FlashStatus:
                   NV_DEBUG_PRINTF(("ISP FlashStatus"));
                   return NV_FALSE; /* no support yet */
                 case NvCameraIspAttribute_WhiteBalanceMode:
                   NV_DEBUG_PRINTF(("ISP WhiteBalanceMode"));
                   return NV_FALSE; /* no support yet */
                 case NvCameraIspAttribute_EdgeEnhanceStrengthBias:
                     NV_DEBUG_PRINTF(("ISP EdgeEnhanceStrengthBias"));
                   return NV_FALSE; /* no support yet */
                 default:
                   NV_DEBUG_PRINTF(("Undefined camera ISP setting"));
                }
            }
            return NV_FALSE;

            break;

        case NvOdmImagerParameter_AWBLock:
            NV_DEBUG_PRINTF(("CAM ISP AWB lock"));
            return NV_TRUE; /* no support yet */
        case NvOdmImagerParameter_AELock:
            NV_DEBUG_PRINTF(("CAM ISP AE lock"));
            return NV_TRUE; /* no support yet */
        case NvOdmImagerParameter_SensorIspSupport:
        {
            NvOsDebugPrintf("NvOdmImagerParameter_SensorIspSupport %d\n", IspSupport);
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));

            if (pValue)
            {
                *(NvBool*)pValue = IspSupport;
                return NV_TRUE;
            }
            else
            {
                NvOsDebugPrintf("cannot store value in NULL pointer for"\
                    "NvOdmImagerParameter_SensorIspSupport in %s:%d\n",__FILE__,__LINE__);
                return NV_FALSE;
            }
        }
        case NvOdmImagerParameter_HorizontalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *((NvF32*)pValue) = LENS_HORIZONTAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *((NvF32*)pValue) = LENS_VERTICAL_VIEW_ANGLE;
            break;
        default:
           return NV_FALSE;
    }

    return NV_TRUE;
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
    SensorYUVMT9M114ModeDependentSettings *pModeSettings;
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
        pModeSettings = (SensorYUVMT9M114ModeDependentSettings*)
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
                  MT9M114_SupportedAeModes,
                  sizeof(MT9M114_SupportedAeModes));
    pProperties->SupportedAeModes.Size =
        NV_ARRAY_SIZE(MT9M114_SupportedAeModes);

    // Load supported framerates.
    {
        NvU32 count = 0;
        while (count < NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList))
        {
            pProperties->SupportedFpsRanges.Points[count].x = 7.5f; // Min
            pProperties->SupportedFpsRanges.Points[count].y = 30.0f; // Max
            count++;
        }
        pProperties->SupportedFpsRanges.Size = count;
    }

    // Load AWB static properties.
    NvOdmOsMemcpy(pProperties->SupportedAwbModes.Property,
                  MT9M114_SupportedAwbModes,
                  sizeof(MT9M114_SupportedAwbModes));
    pProperties->SupportedAwbModes.Size =
        NV_ARRAY_SIZE(MT9M114_SupportedAwbModes);

    // Load AF static properties.
    NvOdmOsMemcpy(pProperties->SupportedAfModes.Property,
                  MT9M114_SupportedAfModes,
                  sizeof(MT9M114_SupportedAfModes));
    pProperties->SupportedAfModes.Size =
        NV_ARRAY_SIZE(MT9M114_SupportedAfModes);

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
    pProperties->AeMode = NvOdmImagerAeMode_On;
    pProperties->SensorSensitivity = SENSOR_DEFAULT_ISO;
    pProperties->SensorExposureTime = SENSOR_DEFAULT_EXPOSURE;
    pProperties->AeAntibandingMode = NvCamAeAntibandingMode_AUTO;
    pProperties->AeExposureCompensation = 0;
    //pProperties->AeRegions;
    pProperties->AePrecaptureTrigger = NvCamAePrecaptureTrigger_Idle;

    // Load AF Defaults.
    pProperties->AfMode = NvCamAfMode_Off;
    pProperties->AfTrigger = NvCamAfTrigger_Idle;
    //pProperties->AfRegions.numOfRegions = 0;

    // Load AWB defaults.
    pProperties->AwbMode = NvCamAwbMode_Auto;
    pProperties->AwbLock = NV_FALSE;
    //pProperties->AwbRegions;

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

    return Status;
}

/**
 * SensorYuv_GetHal. Phase 1.
 * return the hal functions associated with sensor yuv
 */
NvBool SensorYuvMT9M114_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor){
        return NV_FALSE;}

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

#endif // BUILD_FOR_AOS

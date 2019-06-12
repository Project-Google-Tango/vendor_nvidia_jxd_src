/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/*
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0
#define LOG_TAG "ImagerODM-OV2710"
#include <cutils/log.h>
*/
#if (BUILD_FOR_AOS == 0)
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <asm/types.h>
#include <ov2710.h>
#endif

#include "sensor_bayer_ov2710.h"
#include "nvc.h"
#include "imager_util.h"
#include "nvodm_query_gpio.h"

#ifndef BUILD_FOR_VENTANA
#define BUILD_FOR_VENTANA (0)
#endif

#if (BUILD_FOR_VENTANA == 1)
#include "sensor_bayer_ov2710_config_ventana.h"
#else
#include "sensor_bayer_ov2710_config_cardhu.h"
#endif

#define LENS_FOCAL_LENGTH (2.7f)
#define LENS_HORIZONTAL_VIEW_ANGLE (56.8f)
#define LENS_VERTICAL_VIEW_ANGLE (56.8f)

// the standard assert doesn't help us
#undef NV_ASSERT
#define NV_ASSERT(x) \
    do{if(!(x)){NvOsDebugPrintf("ASSERT at %s:%d\n", __FILE__,__LINE__);}}while(0)

/****************************************************************************
 * Implementing for a New Sensor:
 *
 * When implementing for a new sensor from this template, there are four
 * phases:
 * Phase 0: Gather sensor information. In phase 0, we should gather the sensor
 *          information for bringup such as sensor capabilities, set mode i2c
 *          write sequence, NvOdmIoAddress in nvodm_query_discovery_imager.h,
 *          and so on.
 * Phase 1: Bring up a sensor. After phase 1, we should be able to do a basic
 *          still capture with minimal interactions between camera driver
 *          and the sensor
 * Phase 2: Calibrate a sensor. After phase 2, we should be able to calibrate
 *          a sensor. Calibration will need some interactions between camera
 *          driver and the sensor such as setting exposure, gains, and so on.
 * Phase 3: Fully functioning sensor. After phase 3, the sensor should be fully
 *          functioning as described in nvodm_imager.h
 *
 * To help implementing for a new sensor, the template code will be marked as
 * Phase 0, 1, 2, or 3.
 ****************************************************************************/

/**
 * Sensor default settings. Phase 2. Sensor Dependent.
 * #defines make the code more readable and helps maintainability
 * The template tries to keep the list reduced to just things that
 * are used more than once.  Ideally, all numbers in the code would use
 * a #define.
 */
// manual exposure control
//
#define SENSOR_BAYER_DEFAULT_PLL_MULT            (0x0046) // refer to Div_cnt6b and Div45
#define SENSOR_BAYER_DEFAULT_PLL_PRE_DIV         (0x0002) // refer to R_PREDIV.
#define SENSOR_BAYER_DEFAULT_PLL_POS_DIV         (0x0001) // Keep as 1.
#define SENSOR_BAYER_DEFAULT_PLL_VT_PIX_DIV      (0x000a) // refer to R_DIVS, R_DIVL, R_SELD2P5, R_PREDIV and out_block_div
#define SENSOR_BAYER_DEFAULT_PLL_VT_SYS_DIV      (0x0001) // Keep as 1.

#define SENSOR_BAYER_DEFAULT_MAX_GAIN            (15.5) // reg 0x350a-0x350b, but seems only 0x350b is in use? Equation below. Min is 1x1x1x1, max is 2x2x2x1.9375=15.5
                                                        // ??? reg 0x3508-9 long gain
                                                        //
#define SENSOR_BAYER_DEFAULT_MIN_GAIN             (1.0) // reg 0x350b with all bits to be zero. Gain = (0x350b[6]+1) x (0x350b[5]+1) x (0x350b[4]+1) x (0x350b[3:0]/16+1)
                                                        // ??? reg 0x3a18-9 about AEC gain ceiling, what are they?
                                                        //

#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF     (6)     // Minmum margin between frame length and coarse integration time.

#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH  (0x10002) // register 0x380e-f is 13 bits, but consider coarse integration time is of 12 bits with max 0xffff, set this to 0x10002
                                                         // use reg 0x3500 for LONG EXPO 1/3
                                                         // use reg 0x3501 for LONG EXPO 2/3
                                                         // use reg 0x3502 for LONG EXPO 3/3
                                                         // The max "coarse integration time" allowed in these registers are 0xffff
                                                         //
#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
                                                         // use reg 0x3500 for LONG EXPO 1/3
                                                         // use reg 0x3501 for LONG EXPO 2/3
                                                         // use reg 0x3502 for LONG EXPO 3/3
                                                         //
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE (0x002) //
                                                         // use reg 0x3500 for LONG EXPO 1/3
                                                         // use reg 0x3501 for LONG EXPO 2/3
                                                         // use reg 0x3502 for LONG EXPO 3/3, use 0x2 for 2 lines of integration time (register 0x3502  to be programmed as 0x2 << 4).

#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P       (0x0700)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P      (0x02f0)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P   (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P-\
                                                      SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)

#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_1080P       (0x0974)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1080P      (0x0450)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1080P   (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1080P-\
                                                      SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)


#define DIFF_INTEGRATION_TIME_OF_ADDITION_MODE    (0.0f)
#define DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE (0.0f)
#define DIFF_INTEGRATION_TIME_OF_MODE            DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE


#define DEBUG_PRINTS    (0)

#define HACT_START_FULL (0)
#define VACT_START_FULL (2)
#define HACT_START_QUART (0)
#define VACT_START_QUART (2)

#define KERNEL_DEVICE_NODE  "/dev/ov2710"

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides_front.isp",
};

static const char *pFactoryBlobFiles[] =
{
    "/data/factory_front.bin",
    "/data/calibration_front.bin",
};

static NV_INLINE NvF32
SENSOR_GAIN_TO_F32(NvU16 x, NvF32 MaxGain, NvF32 MinGain)
{
    NvU16 reg0x350bBit6;
    NvU16 reg0x350bBit5;
    NvU16 reg0x350bBit0To3;

    reg0x350bBit6 = (x>>6) & 0x1;
    reg0x350bBit5 = (x>>5) & 0x1;
    reg0x350bBit0To3 = x & 0xff;
    return ((NvF32)reg0x350bBit6+1.f) * ((NvF32)reg0x350bBit5+1.f) * ((NvF32)reg0x350bBit5+1.f) * ((NvF32)reg0x350bBit0To3/16.f+1.f);
}

static NV_INLINE NvU16
SENSOR_F32_TO_GAIN(NvF32 x, NvF32 MaxGain, NvF32 MinGain)
{
    NvU16 reg0x350bBit6=0x0;
    NvU16 reg0x350bBit5=0x0;
    NvU16 reg0x350bBit4=0x0;
    NvU16 reg0x350bBit0To3=0x0;

    NvF32 gainForBit0To3;
    NvF32 gainReminder;

    // for OV2710, gain is up to x15.5
    // 15.5 (max), 7.75, 3.875, 1.9375, 1.0 (min)
    gainReminder = x;
    if(x > MaxGain/2.0)
    {
        reg0x350bBit6 = 0x1;
        reg0x350bBit5 = 0x1;
        reg0x350bBit4 = 0x1;
        gainReminder = (NvF32)(x / 8.0);
    }
    else if(x > MaxGain/4.0)
    {
        reg0x350bBit6 = 0x0;
        reg0x350bBit5 = 0x1;
        reg0x350bBit4 = 0x1;
        gainReminder = (NvF32)(x / 4.0);
    }
    else if(x > MaxGain/8.0)
    {
        reg0x350bBit6 = 0x0;
        reg0x350bBit5 = 0x0;
        reg0x350bBit4 = 0x1;
        gainReminder = (NvF32)(x / 2.0);
    }

    if(gainReminder > MaxGain/8.0)
    {
        gainReminder = (NvF32)(MaxGain/8.0);
    }
    if(gainReminder < MinGain)
    {
        gainReminder = MinGain;
    }

    gainForBit0To3 = (NvF32)((gainReminder-MinGain)*16.0);
    reg0x350bBit0To3 = (NvU16)(gainForBit0To3);
    if((gainForBit0To3 - (NvF32)reg0x350bBit0To3 > 0.5) && (reg0x350bBit0To3!=0xf))
    {
        reg0x350bBit0To3 += 1;
    }
    reg0x350bBit0To3 &= 0xf; // might not need this?

#if DEBUG_PRINTS
    NvOsDebugPrintf("x=%f, MaxGain=%f, reg0x350bBit6=%d, reg0x350bBit5=%d, reg0x350bBit4=%d, reg0x350bBit0To3=%d\n",
                     x, MaxGain, reg0x350bBit6, reg0x350bBit5, reg0x350bBit4, reg0x350bBit0To3);
#endif

    return (reg0x350bBit6<<6) | (reg0x350bBit5<<5) | (reg0x350bBit4<<4) | (reg0x350bBit0To3);
}

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
 * Sensor bayer's capabilities. Phase 0. Sensor Dependent.
 * This is the first thing the camera driver requests. The information here
 * is used to setup the proper interface code (CSI, VIP, pixel type), start
 * up the clocks at the proper speeds, and so forth.
 * For Phase 0, one could start with these below as an example for a bayer
 * sensor and just update clocks from what is given in the reference documents
 * for that device. The FAE may have specified the clocks when giving the
 * reccommended i2c settings.
 */
static NvOdmImagerCapabilities g_SensorBayerCaps =
{
    "OV2710",  // string identifier, used for debugging
#if ((BUILD_FOR_CARDHU == 1) || (BUILD_FOR_KAI == 1))
    NvOdmImagerSensorInterface_SerialB,
#else
    NvOdmImagerSensorInterface_SerialA,
#endif
    {
        NvOdmImagerPixelType_BayerBGGR,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
    6000, // initial sensor clock (in khz)
    // preferred clock profile: {mclk, ratio = max_output_pixelclock/mclk}
    // max_output_pixelclock should be >= max pixel clock in the mode table
    // for this sensor, it is mode 1920x1080_30fps: 1920 x 1080 x 30fps = 62.21MHz
#if ((BUILD_FOR_CARDHU == 1) || (BUILD_FOR_KAI == 1))
    { {24000, 62.21f/24.f}},
#else
    { {24000, 130.f/24.f}},
#endif
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_A,
        NVODMSENSORMIPITIMINGS_DATALANES_ONE,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        1, // USE !CONTINUOUS_CLOCK,
#if ((BUILD_FOR_CARDHU == 1) || (BUILD_FOR_KAI == 1))
        0x9 // THS_SETTLE is a receiver timing parameter and can vary with CSI clk.
#else
        0x7 // THS_SETTLE is a receiver timing parameter and can vary with CSI clk.
#endif
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    0, // only set if focuser device is available
    0, // only set if flash device is available
    NVODM_IMAGER_CAPABILITIES_END
};

typedef struct SensorBayerOV2710ModeDependentSettingsRec
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 MinFrameLength;
    NvF32 InherentGain;
} SensorBayerOV2710ModeDependentSettings;

SensorBayerOV2710ModeDependentSettings ModeDependentSettings_OV2710_1280X720 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P,
    2.0f,
};

SensorBayerOV2710ModeDependentSettings ModeDependentSettings_OV2710_1920X1080 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_1080P,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1080P,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1080P,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1080P,
    1.0f,
};

/**
 * A list of sensor mode and its SetModeSequence. Phase 0. Sensor Dependent.
 * When SetMode is called, it searches the list for desired resolution and
 * frame rate and write the SetModeSequence through i2c.
 * For Phase 3, more resolutions will be added to this table, allowing
 * for peak performance during Preview, D1 and 720P encode.
 */
static SensorSetModeSequence *g_pSensorBayerSetModeSequenceList;

/*
   AW: Note that there is no set sequence data, as the actual mode set is done
   in the ov2710 kernel driver.
*/

static SensorSetModeSequence g_SensorBayerSetModeSequenceListCSISingle[] =
{

    {{{1920, 1080}, {0, 2}, 30, 1.0, 18, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_1080P}, NULL, &ModeDependentSettings_OV2710_1920X1080},

    // temporary disable 1280x720 mode until we resolved the field of view and lens shading issues.
    //{{{1280, 720}, {0, 2}, 30, 1.0, 20, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P}, NULL, &ModeDependentSettings_OV2710_1280X720},
};

/**
 * ----------------------------------------------------------
 *  Start of Phase 1, Phase 2, and Phase 3 code
 * ----------------------------------------------------------
 */

/**
 * Sensor bayer's private context. Phase 1.
 */
typedef struct SensorBayerContextRec
{
    // Phase 1 variables.
    int   camera_fd;
    NvU32 ModeIndex;
    NvU32 NumModes;
    NvOdmImagerPowerLevel PowerLevel;

    // Phase 2 variables.
    NvBool SensorInitialized;

    NvU32 SensorInputClockkHz; // mclk (extclk)

    NvF32 Exposure;
    NvF32 MaxExposure;
    NvF32 MinExposure;

    NvF32 Gains[4];
    NvF32 MaxGain;
    NvF32 MinGain;
    NvF32 InherentGain;

    NvF32 FrameRate;
    NvF32 MaxFrameRate;
    NvF32 MinFrameRate;
    NvF32 RequestedMaxFrameRate;

    // Phase 2 variables. Sensor Dependent.
    // These are used to set or get exposure, frame rate, and so on.
    NvU32 PllMult;
    NvF32 PllPreDiv;
    NvU32 PllPosDiv;
    NvU32 PllVtPixDiv;
    NvU32 PllVtSysDiv;

    NvU32 CoarseTime;
    NvU32 VtPixClkFreqHz;

    NvU32 LineLength;
    NvU32 FrameLength;

    NvU32 MaxFrameLength;
    NvU32 MinFrameLength;
    NvBool TestPatternMode;

    NvBool showSkipWriteGains;
    NvBool showSkipWriteExposure;

    NvOdmImagerStereoCameraMode StereoCameraMode;
}SensorBayerContext;

/**
 * Static functions
 * Some compilers require these to be declared upfront.
 */
static NvBool SensorBayer_Open(NvOdmImagerHandle hImager);

static void SensorBayer_Close(NvOdmImagerHandle hImager);

static void
SensorBayer_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities);

static void
SensorBayer_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumberOfModes);

static NvBool
SensorBayer_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult);

static NvBool
SensorBayer_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel);

static NvBool
SensorBayer_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue);

static NvBool
SensorBayer_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue);

static void
SensorBayer_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel);

static NV_INLINE NvBool
SensorOV2710_ChooseModeIndex(
    SensorBayerContext *pContext,
    NvSize Resolution,
    NvU32 *pIndex)
{
    NvU32 Index;

    for (Index = 0; Index < pContext->NumModes; Index++)
    {
        if ((Resolution.width ==
             g_pSensorBayerSetModeSequenceList[Index].Mode.
             ActiveDimensions.width) &&
            (Resolution.height ==
             g_pSensorBayerSetModeSequenceList[Index].Mode.
             ActiveDimensions.height))
        {
            *pIndex = Index;
            return NV_TRUE;
        }
    }

    return NV_FALSE;
}

/**
 * SensorBayer_WriteExposure. Phase 2. Sensor Dependent.
 * Calculate and write the values of the sensor registers for the new
 * exposure time.
 * Without this, calibration will not be able to capture images
 * at various brightness levels, and the final product won't be able
 * to compensate for bright or dark scenes.
 *
 * if pFrameLength or pCoarseTime is not NULL, return the resulting
 * frame length or corase integration time instead of writing the
 * register.
 */
static NvBool
SensorBayer_WriteExposure(
    SensorBayerContext *pContext,
    const NvF32 *pNewExposureTime,
    NvU32 *pFrameLength,
    NvU32 *pCoarseTime)
{
#if (BUILD_FOR_AOS == 0)
    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 NewExpTime = *pNewExposureTime;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;
    NvF32 NewFrameRate = 0.f;

    if (pContext->TestPatternMode)
        return NV_FALSE;

    if (NewExpTime > pContext->MaxExposure) {
        NV_DEBUG_PRINTF(("New exptime over max limit: %f out of (%f, %f)\n",
              NewExpTime, pContext->MinExposure, pContext->MaxExposure));
        NewExpTime = pContext->MaxExposure;
    }
    else if (NewExpTime < pContext->MinExposure) {
        NV_DEBUG_PRINTF(("New exptime over min limit: %f out of (%f, %f)\n",
              NewExpTime, pContext->MinExposure, pContext->MaxExposure));
        NewExpTime = pContext->MinExposure;
    }

    // Here, we have to decide if the new exposure time is valid
    // based on the sensor and current sensor settings.
    // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
    NewCoarseTime = (NvU32)((Freq * NewExpTime) / LineLength + DIFF_INTEGRATION_TIME_OF_MODE);
    if ((int)NewCoarseTime <= 0) NewCoarseTime = 1;

    NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
    if (NewFrameLength > pContext->MaxFrameLength)
        NewFrameLength = pContext->MaxFrameLength;
    else if (NewFrameLength < pContext->MinFrameLength)
        NewFrameLength = pContext->MinFrameLength;

    NewFrameRate = Freq / (NvF32)(NewFrameLength * LineLength);
    if ((NewFrameRate > pContext->RequestedMaxFrameRate)
        && (pContext->RequestedMaxFrameRate > 0.0f))
        NewFrameRate = pContext->RequestedMaxFrameRate;

#if DEBUG_PRINTS
    NvOsDebugPrintf("1st calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime);
#endif

    // Calc back
    NewFrameLength = (NvU32)(Freq / (NewFrameRate * LineLength));
    if (NewFrameLength > pContext->MaxFrameLength)
        NewFrameLength = pContext->MaxFrameLength;
    else if (NewFrameLength < pContext->MinFrameLength)
        NewFrameLength = pContext->MinFrameLength;

    if (NewCoarseTime > NewFrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
        NewCoarseTime = NewFrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
    if ((int)NewCoarseTime <= 0) NewCoarseTime = 1;

    NewExpTime = (NvF32)((NewCoarseTime - DIFF_INTEGRATION_TIME_OF_MODE) * LineLength) / Freq;
    if (NewExpTime < pContext->MinExposure)
        NewExpTime  = pContext->MinExposure;
    else if (NewExpTime > pContext->MaxExposure)
        NewExpTime  = pContext->MaxExposure;

    // Pass back,
    if (pFrameLength)
        *pFrameLength = NewFrameLength;
    if (pCoarseTime)
        *pCoarseTime = NewCoarseTime;

    // ioctl
    if (NewFrameLength != pContext->FrameLength)
    {
        // write new value only when pFrameLength is NULL
        if (!pFrameLength)
        {
            int ret;
            ret = ioctl(pContext->camera_fd, OV2710_IOCTL_SET_FRAME_LENGTH,
                        NewFrameLength);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set frame length failed %s\n", strerror(errno));
        }
    }
    else
    {
#if DEBUG_PRINTS
        NvOsDebugPrintf("Frame length keep as before : %d \n", NewFrameLength);
#endif
    }

    if (pContext->CoarseTime != NewCoarseTime)
    {
        // write new coarse time only when pCoarseTime is NULL
        if (!pCoarseTime)
        {
            int ret;
            ret = ioctl(pContext->camera_fd, OV2710_IOCTL_SET_COARSE_TIME,
                        NewCoarseTime);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set coarse time failed %s\n", strerror(errno));
        }
    }

    // Update Context
    pContext->Exposure = NewExpTime;
    pContext->FrameRate = NewFrameRate;
    pContext->FrameLength = NewFrameLength;
    pContext->CoarseTime = NewCoarseTime;

#if DEBUG_PRINTS
    NvOsDebugPrintf("2nd calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime);
#endif

#endif
    return NV_TRUE;
}

/**
 * SensorBayer_WriteGains. Phase 2. Sensor Dependent.
 * Writing the sensor registers for the new gains.
 * Just like exposure, this allows the sensor to brighten an image. If the
 * exposure time is insufficient to make the picture viewable, gains are
 * applied.  During calibration, the gains will be measured for maximum
 * effectiveness before the noise level becomes too high.
 * If per-channel gains are available, they are used to normalize the color.
 * Most sensors output overly greenish images, so the red and blue channels
 * are increased.
 *
 * if pGain is not NULL, return the resulting gain instead of writing the
 * register.
 */
static NvBool
SensorBayer_WriteGains(
    SensorBayerContext *pContext,
    const NvF32 *pGains,
    NvU16 *pGain)
{
// OV2710's
// manual gain control, reg 0x350b, gain formula is
//
//   Gain = (0x350b[6]+1) x (0x350b[5]+1) x (0x350b[4]+1) x (0x350b[3:0]/16+1)
//
#if (BUILD_FOR_AOS == 0)
    NvU32 i = 1;
    NvU16 NewGains = 0;

    if (pGains[i] > pContext->MaxGain)
        return NV_FALSE;
    if (pGains[i] < pContext->MinGain)
        return NV_FALSE;

    // prepare and write register 0x350b
    NewGains = SENSOR_F32_TO_GAIN(pGains[i], pContext->MaxGain, pContext->MinGain);

    if (pGain)
    {
        *pGain = NewGains;
    }
    else
    {
        int ret;
        ret = ioctl(pContext->camera_fd, OV2710_IOCTL_SET_GAIN, NewGains);
        if (ret < 0)
            NvOsDebugPrintf("ioctl to set gain failed %s\n", strerror(errno));
    }

    NvOdmOsMemcpy(pContext->Gains, pGains, sizeof(NvF32)*4);

#if DEBUG_PRINTS
    NvOsDebugPrintf("new gains = %f, %f, %f, %f\n\n", pGains[0],
        pGains[1], pGains[2], pGains[3]);
#endif
#endif
    return NV_TRUE;
}

#if (BUILD_FOR_AOS == 0)
static NvBool
SensorBayer_GroupHold(
    SensorBayerContext *pContext,
    const NvOdmImagerSensorAE *sensorAE)
{
    int ret;
    struct ov2710_ae ae;
    NvU32 i = 1;

    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 NewExpTime = sensorAE->ET;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;
    NvF32 NewFrameRate = 0.f;

    NvOdmOsMemset(&ae, 0, sizeof(struct ov2710_ae));

    if (sensorAE->gains_enable==NV_TRUE) {
        if (sensorAE->gains[i] > pContext->MaxGain)
            return NV_FALSE;
        if (sensorAE->gains[i] < pContext->MinGain)
            return NV_FALSE;

        // prepare and write register 0x350b
        ae.gain = SENSOR_F32_TO_GAIN(sensorAE->gains[i], pContext->MaxGain, pContext->MinGain);
        ae.gain_enable = NV_TRUE;
        NvOdmOsMemcpy(pContext->Gains, sensorAE->gains, sizeof(NvF32)*4);
    }

    if (sensorAE->ET_enable==NV_TRUE) {

        if (NewExpTime > pContext->MaxExposure) {
            NV_DEBUG_PRINTF(("New exptime over max limit: %f out of (%f, %f)\n",
                  NewExpTime, pContext->MinExposure, pContext->MaxExposure));
            NewExpTime = pContext->MaxExposure;
        }
        else if (NewExpTime < pContext->MinExposure) {
            NV_DEBUG_PRINTF(("New exptime over min limit: %f out of (%f, %f)\n",
                  NewExpTime, pContext->MinExposure, pContext->MaxExposure));
            NewExpTime = pContext->MinExposure;
        }

        // Here, we have to decide if the new exposure time is valid
        // based on the sensor and current sensor settings.
        // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
        NewCoarseTime = (NvU32)((Freq * NewExpTime) / LineLength + DIFF_INTEGRATION_TIME_OF_MODE);
        if ((int)NewCoarseTime <= 0)
            NewCoarseTime = 1;

        NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
        if (NewFrameLength > pContext->MaxFrameLength)
            NewFrameLength = pContext->MaxFrameLength;
        else if (NewFrameLength < pContext->MinFrameLength)
            NewFrameLength = pContext->MinFrameLength;

        NewFrameRate = Freq / (NvF32)(NewFrameLength * LineLength);
        if ((NewFrameRate > pContext->RequestedMaxFrameRate)
            && (pContext->RequestedMaxFrameRate > 0.0f))
            NewFrameRate = pContext->RequestedMaxFrameRate;

#if DEBUG_PRINTS
        NvOsDebugPrintf("1st calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                         NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime);
#endif

        // Calc back
        if (NewCoarseTime > NewFrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
            NewCoarseTime = NewFrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
        if ((int)NewCoarseTime <= 0) NewCoarseTime = 1;

        NewExpTime = (NvF32)((NewCoarseTime - DIFF_INTEGRATION_TIME_OF_MODE) * LineLength) / Freq;
        if (NewExpTime < pContext->MinExposure)
            NewExpTime  = pContext->MinExposure;
        else if (NewExpTime > pContext->MaxExposure)
            NewExpTime  = pContext->MaxExposure;

        if (NewFrameLength != pContext->FrameLength)
        {
            ae.frame_length = NewFrameLength;
            ae.frame_length_enable = NV_TRUE;

            // Update variables depending on FrameLength.
            pContext->FrameLength = NewFrameLength;
            pContext->FrameRate = NewFrameRate;
        }
        else
        {
            NV_DEBUG_PRINTF(("Frame length keep as before : %d \n", NewFrameLength));
        }

        if (pContext->CoarseTime != NewCoarseTime)
        {
            ae.coarse_time = NewCoarseTime;
            ae.coarse_time_enable = NV_TRUE;

            // Calculate the new exposure based on the sensor and sensor settings.
            pContext->Exposure = NewExpTime;
            pContext->CoarseTime = NewCoarseTime;
        }
    }

#if DEBUG_PRINTS
    NvOsDebugPrintf("2nd calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime);
#endif

    if (ae.gain_enable==NV_TRUE || ae.coarse_time_enable==NV_TRUE ||
            ae.frame_length_enable==NV_TRUE) {
        ret = ioctl(pContext->camera_fd, OV2710_IOCTL_SET_GROUP_HOLD, &ae);
        if (ret < 0)
        {
            NvOsDebugPrintf("ioctl to set group hold failed %s\n", strerror(errno));
            return NV_FALSE;
        }
    }
    return NV_TRUE;
}
#endif

/**
 * SensorBayer_Open. Phase 1.
 * Initialize sensor bayer and its private context
 */
static NvBool SensorBayer_Open(NvOdmImagerHandle hImager)
{
    SensorBayerContext *pSensorBayerContext = NULL;

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorBayerContext =
        (SensorBayerContext*)NvOdmOsAlloc(sizeof(SensorBayerContext));
    if (!pSensorBayerContext)
        goto fail;

    pSensorBayerContext->TestPatternMode = NV_FALSE;

    NvOdmOsMemset(pSensorBayerContext, 0, sizeof(SensorBayerContext));

    g_pSensorBayerSetModeSequenceList = g_SensorBayerSetModeSequenceListCSISingle;
    pSensorBayerContext->NumModes =
        NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceListCSISingle);

    pSensorBayerContext->ModeIndex =
        pSensorBayerContext->NumModes; // invalid mode

    pSensorBayerContext->showSkipWriteGains = NV_TRUE;
    pSensorBayerContext->showSkipWriteExposure = NV_TRUE;

    /**
     * Phase 2. Initialize exposure and gains.
     */
    pSensorBayerContext->Exposure = -1.0; // invalid exposure

     // no need to convert gain's min/max to float for OV2710, because they are defined as float already.
    pSensorBayerContext->MaxGain = SENSOR_BAYER_DEFAULT_MAX_GAIN; // SENSOR_GAIN_TO_F32(SENSOR_BAYER_DEFAULT_MAX_GAIN);
    pSensorBayerContext->MinGain = SENSOR_BAYER_DEFAULT_MIN_GAIN; // SENSOR_GAIN_TO_F32(SENSOR_BAYER_DEFAULT_MIN_GAIN);
    /**
     * Phase 2 ends here.
     */

    pSensorBayerContext->PowerLevel = NvOdmImagerPowerLevel_Off;

    pSensorBayerContext->SensorInputClockkHz = g_SensorBayerCaps.ClockProfiles[0].ExternalClockKHz;

    hImager->pSensor->pPrivateContext = pSensorBayerContext;
    return NV_TRUE;

fail:
    NvOdmOsFree(pSensorBayerContext);
    return NV_FALSE;
}


/**
 * SensorBayer_Close. Phase 1.
 * Free the sensor's private context
 */
static void SensorBayer_Close(NvOdmImagerHandle hImager)
{
    SensorBayerContext *pContext = NULL;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
        return;

    pContext = (SensorBayerContext*)hImager->pSensor->pPrivateContext;

    // cleanup
    NvOdmOsFree(pContext);
    hImager->pSensor->pPrivateContext = NULL;
}

/**
 * SensorBayer_GetCapabilities. Phase 1.
 * Returnt sensor bayer's capabilities
 */
static void
SensorBayer_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    // Copy the sensor caps from g_SensorBayerCaps
    NvOdmOsMemcpy(pCapabilities, &g_SensorBayerCaps,
        sizeof(NvOdmImagerCapabilities));

    // TBD: Confirm whether this is required
//    pCapabilities->MipiTiming.CILThresholdSettle = 0x2;
}

/**
 * SensorBayer_ListModes. Phase 1.
 * Return a list of modes that sensor bayer supports.
 * If called with a NULL ptr to pModes, then it just returns the count
 */
static void
SensorBayer_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumberOfModes)
{
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
    NvS32 i;

    if (pNumberOfModes)
    {
        *pNumberOfModes = (NvS32)pContext->NumModes;
        if (pModes)
        {
            // Copy the modes from g_pSensorBayerSetModeSequenceList
            for (i = 0; i < *pNumberOfModes; i++)
            {
                pModes[i] = g_pSensorBayerSetModeSequenceList[i].Mode;
            }
        }
    }
}

/**
 * SensorBayer_SetMode. Phase 1.
 * Set sensor bayer to the mode of the desired resolution and frame rate.
 */
static NvBool
SensorBayer_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult)
{
#if (BUILD_FOR_AOS == 0)
    NvBool Status = NV_FALSE;
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
    NvU32 Index;
    SensorBayerOV2710ModeDependentSettings *pModeSettings;
    NvU32 CoarseTime = 0;
    NvU32 FrameLength = 0;
    NvU16 Gain = 0;

#if DEBUG_PRINTS
    NvOsDebugPrintf("Setting resolution to %dx%d, exposure %f, gains %f\n",
        pParameters->Resolution.width, pParameters->Resolution.height,
        pParameters->Exposure, pParameters->Gains[0]);
#endif

    // Find the right entrys in g_pSensorBayerSetModeSequenceList that matches
    // the desired resolution and framerate
    for (Index = 0; Index < pContext->NumModes; Index++)
    {
        if ((pParameters->Resolution.width ==
             g_pSensorBayerSetModeSequenceList[Index].Mode.
             ActiveDimensions.width) &&
            (pParameters->Resolution.height ==
             g_pSensorBayerSetModeSequenceList[Index].Mode.
             ActiveDimensions.height))
             break;
    }
    // No match found
    if (Index == pContext->NumModes)
        return NV_FALSE;

    if (pSelectedMode)
    {
        *pSelectedMode = g_pSensorBayerSetModeSequenceList[Index].Mode;
    }

    // These hardcoded numbers are from the set mode sequence and this formula
    // is based on this sensor. Sensor Dependent.
    //
    pContext->PllMult = SENSOR_BAYER_DEFAULT_PLL_MULT;
    pContext->PllPreDiv = SENSOR_BAYER_DEFAULT_PLL_PRE_DIV;
    pContext->PllPosDiv = SENSOR_BAYER_DEFAULT_PLL_POS_DIV;
    pContext->PllVtPixDiv = SENSOR_BAYER_DEFAULT_PLL_VT_PIX_DIV;
    pContext->PllVtSysDiv = SENSOR_BAYER_DEFAULT_PLL_VT_SYS_DIV;

    pContext->VtPixClkFreqHz =
        (NvU32)((pContext->SensorInputClockkHz * 1000 * pContext->PllMult) /
                (pContext->PllPreDiv * pContext->PllVtPixDiv *
                 pContext->PllVtSysDiv));

    pModeSettings =
        (SensorBayerOV2710ModeDependentSettings*)
        g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

    // set initial line lenth, frame length, coarse time, and max/min of frame length
    pContext->LineLength = pModeSettings->LineLength;
    pContext->FrameLength = pModeSettings->FrameLength;
    pContext->CoarseTime = pModeSettings->CoarseTime;
    pContext->MaxFrameLength = SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH;
    pContext->MinFrameLength = pModeSettings->MinFrameLength;
    pContext->InherentGain = pModeSettings->InherentGain;

    pContext->Exposure    =
              (((NvF32)pContext->CoarseTime - DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength) /(NvF32)pContext->VtPixClkFreqHz;
    pContext->MaxExposure =
              (((NvF32)SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE - DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength ) / (NvF32)pContext->VtPixClkFreqHz;
    pContext->MinExposure =
              (((NvF32)SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE - DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength ) / (NvF32)pContext->VtPixClkFreqHz;

    pContext->FrameRate =
              (NvF32)pContext->VtPixClkFreqHz /
              (NvF32)(pContext->FrameLength * pContext->LineLength);
    pContext->MaxFrameRate =
              (NvF32)pContext->VtPixClkFreqHz /
              (NvF32)(pContext->MinFrameLength * pContext->LineLength);
    pContext->MinFrameRate =
              (NvF32)pContext->VtPixClkFreqHz /
              (NvF32)(SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH * pContext->LineLength);

    // Clamp maxframerate
    if (g_pSensorBayerSetModeSequenceList[Index].Mode.PeakFrameRate < pContext->MaxFrameRate)
        pContext->MaxFrameRate = g_pSensorBayerSetModeSequenceList[Index].Mode.PeakFrameRate;

    pContext->Gains[0] = 1.0;
    pContext->Gains[1] = 1.0;
    pContext->Gains[2] = 1.0;
    pContext->Gains[3] = 1.0;

    if ((pParameters->Exposure != 0.0) && (pContext->TestPatternMode != NV_TRUE))
    {
        Status = SensorBayer_WriteExposure(pContext, &pParameters->Exposure,
                    &FrameLength, &CoarseTime);
        if (!Status)
        {
            NvOsDebugPrintf("SensorOV2710_WriteExposure failed\n");
        }
    }
    else
    {
        FrameLength = pModeSettings->FrameLength;
        CoarseTime = pModeSettings->CoarseTime;
    }

    if (pParameters->Gains[0] != 0.0 && pParameters->Gains[1] != 0.0 &&
        pParameters->Gains[2] != 0.0 && pParameters->Gains[3] != 0.0)
    {
        Status = SensorBayer_WriteGains(pContext, pParameters->Gains, &Gain);
        if (!Status)
        {
            NvOsDebugPrintf("SensorOV2710_WriteGains failed\n");
        }
    }

    int ret;
    struct ov2710_mode mode = {
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.height,
            FrameLength, CoarseTime, Gain
    };
    ret = ioctl(pContext->camera_fd, OV2710_IOCTL_SET_MODE, &mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__,
            strerror(errno));
        return NV_FALSE;
    } else {
        Status = NV_TRUE;
    }

    /**
     * the following is Phase 2.
     */
    pContext->SensorInitialized = NV_TRUE;

    // Update sensor context after set mode
    NV_ASSERT(pContext->SensorInputClockkHz > 0); // UN-commented


#if DEBUG_PRINTS
    NvOsDebugPrintf("-------------SetMode---------------\n");
    NvOsDebugPrintf("Exposure : %f (%f, %f)\n",
             pContext->Exposure,
             pContext->MinExposure,
             pContext->MaxExposure);
    NvOsDebugPrintf("Gain : %f (%f, %f)\n",
             pContext->Gains[1],
             pContext->MinGain,
             pContext->MaxGain);
    NvOsDebugPrintf("FrameRate : %f (%f, %f)\n",
             pContext->FrameRate,
             pContext->MinFrameRate,
             pContext->MaxFrameRate);
#endif

    if (pResult)
    {
        pResult->Resolution = g_pSensorBayerSetModeSequenceList[Index].
                              Mode.ActiveDimensions;
        pResult->Exposure = pContext->Exposure;
        NvOdmOsMemcpy(pResult->Gains, &pContext->Gains, sizeof(NvF32) * 4);
    }

    /**
     * Phase 2 ends here.
     */
    pContext->ModeIndex = Index;

    // Wait 2 frames for gains/exposure to take effect.
    NvOsSleepMS((NvU32)(2000.0 / (NvF32)(pContext->FrameRate)));

    if (pContext->TestPatternMode)
    {
        NvF32 Gains[4];
        NvU32 i;

        // reset gains
        for (i = 0; i < 4; i++)
            Gains[i] = pContext->MinGain;

        Status = SensorBayer_WriteGains(pContext, Gains, NULL);
        if (!Status)
            return NV_FALSE;

        // TODO: Make this more predictable (ideally, move into driver).
        NvOdmOsWaitUS(350 * 1000);
    }
    return Status;
#else
    NvOsDebugPrintf("[ov2701] Stub for AOS\n");
#endif
}


/**
 * SensorBayer_SetPowerLevel. Phase 1
 * Set the sensor's power level
 */
static NvBool
SensorBayer_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
    NvBool Status = NV_TRUE;

#if (BUILD_FOR_AOS == 0)
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;

    NvOsDebugPrintf("SensorBayer_SetPowerLevel %d\n", PowerLevel);

    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
#ifdef O_CLOEXEC
            pContext->camera_fd = open(KERNEL_DEVICE_NODE, O_RDWR|O_CLOEXEC);
#else
            pContext->camera_fd = open(KERNEL_DEVICE_NODE, O_RDWR);
#endif
            if (pContext->camera_fd < 0) {
                NvOsDebugPrintf("Can not open camera device: %s\n",
                strerror(errno));
                Status = NV_FALSE;
            } else {
                NvOsDebugPrintf("OV2710 Camera fd open as: %d\n", pContext->camera_fd);
                Status = NV_TRUE;
            }
            if (!Status)
                return NV_FALSE;
            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
            close(pContext->camera_fd);
            pContext->camera_fd = -1;
            break;

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

/**
 * SensorBayer_SetParameter. Phase 2.
 * Set sensor specific parameters.
 * For Phase 1. This can return NV_TRUE.
 */
static NvBool
SensorBayer_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    NvBool Status = NV_TRUE;

    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;

#if DEBUG_PRINTS
    NvOsDebugPrintf("SetParameter(): %x\n", Param);
#endif

    switch(Param)
    {
        // Phase 2.
        case NvOdmImagerParameter_SensorExposure:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            Status = SensorBayer_WriteExposure(pContext, (NvF32*)pValue, NULL, NULL);
        }
        break;

        // Phase 2.
        case NvOdmImagerParameter_SensorGain:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));
            Status = SensorBayer_WriteGains(pContext, pValue, NULL);
        }
        break;

        case NvOdmImagerParameter_SensorGroupHold:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorAE));
#if (BUILD_FOR_AOS == 0)
            Status = SensorBayer_GroupHold(pContext, (NvOdmImagerSensorAE *)pValue);
#endif
        }
        break;

        // Phase 3.
        case NvOdmImagerParameter_OptimizeResolutionChange:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            Status = NV_TRUE;
        }
        break;

        // This is optional but nice to have.
        case NvOdmImagerParameter_SelfTest:
        {
            // Not Implemented.
        }
        break;

        // Phase 3.
        case NvOdmImagerParameter_MaxSensorFrameRate:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pContext->RequestedMaxFrameRate = *((NvF32*)pValue);
        }
        break;
        case NvOdmImagerParameter_TestMode:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            pContext->TestPatternMode = *((NvBool *)pValue);

            // If Sensor is initialized then only program the test mode registers
            // else just save the test mode in pContext->TestPatternMode and once
            // the sensor gets initialized in SensorOV2710_SetMode() there itself
            // program the test mode registers.
            if(pContext->SensorInitialized)
            {
                if (pContext->TestPatternMode)
                {
                    NvF32 Gains[4];
                    NvU32 i;

                    // reset gains
                    for (i = 0; i < 4; i++)
                    Gains[i] = pContext->MinGain;

                    Status = SensorBayer_WriteGains(pContext, Gains, NULL);
                    if (!Status)
                        return NV_FALSE;

                }

                // TODO: Make this more predictable (ideally, move into driver).
                NvOdmOsWaitUS(350 * 1000);
            }
        }
        break;

#if 0
        case NvOdmImagerParameter_Reset:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerReset));
            switch(*(NvOdmImagerReset*)pValue)
            {
                case NvOdmImagerReset_Hard:
                    Status = SensorBayer_HardReset(hImager);
            break;

                case NvOdmImagerReset_Soft:
                default:
                    NV_ASSERT(!"Not Implemented!");
                    Status = NV_FALSE;
            }
            break;
#endif

        default:
            NvOsDebugPrintf("SetParameter(): %d not supported\n", Param);
            break;
        }
        return Status;
}

/**
 * SensorBayer_GetParameter. Phase 1.
 * Get sensor specific parameters
 */
static NvBool
SensorBayer_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    NvBool Status = NV_TRUE;
#if (BUILD_FOR_AOS == 0)
    NvF32 *pFloatValue = pValue;
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
#if DEBUG_PRINTS
    NvOsDebugPrintf("GetParameter(): %d\n", Param);
#endif

    switch(Param)
    {
        case NvOdmImagerParameter_SensorType:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorType));
            *((NvOdmImagerSensorType*)pValue) = NvOdmImager_SensorType_Raw;
            break;

        // Phase 1.
        case NvOdmImagerParameter_CalibrationData:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerCalibrationData));
            {
                NvOdmImagerCalibrationData *pCalibration =
                        (NvOdmImagerCalibrationData*)pValue;
                pCalibration->CalibrationData = pSensorCalibrationData;
                pCalibration->NeedsFreeing = NV_FALSE;
            }
            break;

        // Phase 1, it can return min = max = 1.0f
        // Phase 2, return the real numbers.
        case NvOdmImagerParameter_SensorExposureLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 2 * sizeof(NvF32));

            pFloatValue[0] = pContext->MinExposure;
            pFloatValue[1] = pContext->MaxExposure;
            break;

        // Phase 1, it can return min = max = 1.0f
        // Phase 2, return the real numbers.
        case NvOdmImagerParameter_SensorGainLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 2 * sizeof(NvF32));
            pFloatValue[0] = pContext->MinGain;
            pFloatValue[1] = pContext->MaxGain;
            break;

        case NvOdmImagerParameter_FocalLength:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            ((NvF32*)pValue)[0] = LENS_FOCAL_LENGTH;
            break;

        // Get the sensor status. This is optional but nice to have.
        case NvOdmImagerParameter_DeviceStatus:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerDeviceStatus));
            {
                NvOdmImagerDeviceStatus *pStatus;
                pStatus = (NvOdmImagerDeviceStatus *)pValue;
                {
                    uint16_t status;
                    int ret;

                    ret = ioctl(pContext->camera_fd, OV2710_IOCTL_GET_STATUS, &status);
                    if (ret < 0)
                        NvOsDebugPrintf("ioctl to gets status failed "
                            "%s\n", strerror(errno));
                    pStatus->Count = 1;
                }
            }
            break;

        // Phase 1, it can return min = max = 10.0f
        //          (the fps in g_pSensorBayerSetModeSequenceList)
        // Phase 2, return the real numbers
        case NvOdmImagerParameter_SensorFrameRateLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                2 * sizeof(NvF32));

            pFloatValue[0] = pContext->MinFrameRate;
            pFloatValue[1] = pContext->MaxFrameRate;

            break;

        // Phase 1, it can return 1.0f
        // Phase 2, return the real numbers.
        case NvOdmImagerParameter_SensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));

            pFloatValue[0] = pContext->FrameRate;
            break;

        // Get the override config data.
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
                Status = pCalibration->NeedsFreeing;
            }
            break;

        // Get the factory calibration data.
        case NvOdmImagerParameter_FactoryCalibrationData:
            {
                Status = LoadBlobFile(pFactoryBlobFiles,
                                    sizeof(pFactoryBlobFiles)/sizeof(pFactoryBlobFiles[0]),
                                    pValue, SizeOfValue);
            }
            break;

        case NvOdmImagerParameter_FuseID:
#if (BUILD_FOR_AOS == 0)
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(struct nvc_fuseid));
            {
                struct nvc_fuseid fuse_id;
                int ret;
                NvOdmImagerPowerLevel PreviousPowerLevel;
                SensorBayer_GetPowerLevel(hImager, &PreviousPowerLevel);
                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = SensorBayer_SetPowerLevel(hImager, NvOdmImagerPowerLevel_On);
                    if (!ret)
                    {
                        NvOsDebugPrintf("failed to set power to ON. Aborting fuse ID read.\n");
                        Status = NV_FALSE;
                        break;
                    }
                }
                ret = ioctl(pContext->camera_fd, OV2710_IOCTL_GET_FUSEID, &fuse_id);
                if (ret < 0)
                {
                    NvOsDebugPrintf("ioctl to get sensor data failed %s\n", strerror(errno));
                    Status = NV_FALSE;
                }
                else
                {
                    NvOdmOsMemset(pValue, 0, SizeOfValue);
                    NvOdmOsMemcpy(pValue, &fuse_id, sizeof(struct nvc_fuseid));
                }
                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = SensorBayer_SetPowerLevel(hImager, PreviousPowerLevel);
                    if (!ret)
                        NvOsDebugPrintf("failed to set power to previous state after fuse id read.\n");
                }
            }
#endif
            break;

        case NvOdmImagerParameter_RegionUsedByCurrentResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerRegion));


            if (pContext->ModeIndex >= pContext->NumModes)
            {
                // No info available until we've actually set a mode.
                return NV_FALSE;
            }
            else
            {
                NvOdmImagerRegion *pRegion = (NvOdmImagerRegion*)pValue;

                pRegion->RegionStart.x = 0;
                pRegion->RegionStart.y = 0;

                if (pContext->ModeIndex == 1)
                {
                    pRegion->xScale = 2;
                    pRegion->yScale = 2;
                }
                else
                {
                    pRegion->xScale = 1;
                    pRegion->yScale = 1;
                }
            }
            break;
        case NvOdmImagerParameter_LinesPerSecond:
            {
                NvF32 *pFloat = (NvF32 *)pValue;
                NvF32 fps; // frames per second
                NvF32 lines; // lines per frame
                fps = (NvF32)pContext->VtPixClkFreqHz /
                    (NvF32)(pContext->FrameLength * pContext->LineLength);
                lines = (NvF32)g_pSensorBayerSetModeSequenceList
                    [pContext->ModeIndex].Mode.ActiveDimensions.height;
                *pFloat = fps * lines;
            }
            break;

        case NvOdmImagerParameter_SensorActiveRegionReadOutTime:
            {
                NvF32 regionReadOutTime;
                NvF32 *pFloat = (NvF32 *)pValue;
                SensorBayerOV2710ModeDependentSettings *pModeSettings =
                (SensorBayerOV2710ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[pContext->ModeIndex].pModeDependentSettings;
                NvF32 height = (NvF32)(g_pSensorBayerSetModeSequenceList
                    [pContext->ModeIndex].Mode.ActiveDimensions.height);
                regionReadOutTime = (height * pModeSettings->LineLength) / (NvF32)pContext->VtPixClkFreqHz;
                *pFloat = regionReadOutTime;

            }
            break;

        case NvOdmImagerParameter_MaxSensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = pContext->RequestedMaxFrameRate;
            break;

        case NvOdmImagerParameter_ExpectedValues:
            break;

        case NvOdmImagerParameter_SensorGain:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));

            NvOdmOsMemcpy(pValue, pContext->Gains, sizeof(NvF32) * 4);
            break;

        case NvOdmImagerParameter_SensorExposure:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));

            NvOdmOsMemcpy(pValue, &pContext->Exposure, sizeof(NvF32));
            break;

        case NvOdmImagerParameter_SensorGroupHold:
            {
                NvBool *grouphold = (NvBool *) pValue;
                CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
                *grouphold = NV_TRUE;
                break;
            }

        case NvOdmImagerParameter_SensorFrameRateLimitsAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerFrameRateLimitAtResolution));

            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);

            {
                NvOdmImagerFrameRateLimitAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                SensorBayerOV2710ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerFrameRateLimitAtResolution *)pValue;
                pData->MinFrameRate = 0.0f;
                pData->MaxFrameRate = 0.0f;

                MatchFound =
                    SensorOV2710_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerOV2710ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;


                pData->MaxFrameRate = (NvF32)pContext->VtPixClkFreqHz /
                                      (NvF32)(pModeSettings->FrameLength *
                                              pModeSettings->LineLength);
                pData->MinFrameRate =
                    (NvF32)pContext->VtPixClkFreqHz /
                    (NvF32)(SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH *
                            pModeSettings->LineLength);
            }
            break;

        case NvOdmImagerParameter_SensorInherentGainAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerInherentGainAtResolution));

            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);

            {
                NvOdmImagerInherentGainAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                SensorBayerOV2710ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerInherentGainAtResolution *)pValue;
                pData->InherentGain = pContext->InherentGain;

                if (pData->Resolution.width == 0 && pData->Resolution.height == 0)
                    break; // They just wanted the current value

                MatchFound =
                    SensorOV2710_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerOV2710ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

                pData->InherentGain = pModeSettings->InherentGain;
            }
            break;

        case NvOdmImagerParameter_SensorExposureLatchTime:
            // FIXME: implement this
            break;

        case NvOdmImagerParameter_HorizontalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = LENS_HORIZONTAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = LENS_VERTICAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_BracketCaps:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerBracketConfiguration));
            {
                NvOdmImagerBracketConfiguration *pData;
                pData = (NvOdmImagerBracketConfiguration *)pValue;
                pData->FlushCount = 1;
                pData->InitialIntraFrameSkip = 0;
                pData->SteadyStateIntraFrameSkip = 1;
                pData->SteadyStateFrameNumer = 1;
            }
            break;

        // Phase 3.
        default:
            NvOsDebugPrintf("GetParameter(): %d not supported\n", Param);
            Status = NV_FALSE;
    }
#endif
    return Status;
}

/**
 * SensorBayer_GetPowerLevel. Phase 3.
 * Get the sensor's current power level
 */
static void
SensorBayer_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel)
{
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;

    *pPowerLevel = pContext->PowerLevel;
}

#if (BUILD_FOR_CARDHU == 1)
static NvBool IsOV2710_Installed(void)
{
#if (BUILD_FOR_AOS == 0)
    int fd = open(KERNEL_DEVICE_NODE, O_RDWR);

    if (fd >= 0)
    {
        close(fd);
        return NV_TRUE;
    }

#endif
    return NV_FALSE;
}
#endif

/**
 * SensorBayer_GetHal. Phase 1.
 * return the hal functions associated with sensor bayer
 */
NvBool SensorBayerOV2710_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

#if (BUILD_FOR_CARDHU == 1)
    if (!IsOV2710_Installed())
    {
        NvOsDebugPrintf("OV2710 not installed, shift to OV5640\n");
        return SensorYuvOV5640_GetHal(hImager);
    }
#endif
    hImager->pSensor->pfnOpen = SensorBayer_Open;
    hImager->pSensor->pfnClose = SensorBayer_Close;
    hImager->pSensor->pfnGetCapabilities = SensorBayer_GetCapabilities;
    hImager->pSensor->pfnListModes = SensorBayer_ListModes;
    hImager->pSensor->pfnSetMode = SensorBayer_SetMode;
    hImager->pSensor->pfnSetPowerLevel = SensorBayer_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = SensorBayer_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = SensorBayer_SetParameter;
    hImager->pSensor->pfnGetParameter = SensorBayer_GetParameter;

    return NV_TRUE;
}

/**
 * Copyright (c) 2011-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "ImagerODM-OV9726"

#if (BUILD_FOR_AOS == 0)
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <stdint.h>
#include <ov9726.h>
#endif

#include "sensor_bayer_ov9726.h"
#include "imager_util.h"
#include "nvodm_query_gpio.h"

#define DEBUG_PRINTS    (0)

#if DEBUG_PRINTS
#define NVDEBUG_LOG(...)    NvOsDebugPrintf(__VA_ARGS__)
#else
#define NVDEBUG_LOG(...)
#endif

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

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides_front.isp",
};

static const char *pFactoryBlobFiles[] =
{
    "/data/factory_front.bin",
    "/data/calibration_front.bin",
};

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
#include "sensor_bayer_ov9726_camera_config.h"

// the standard assert doesn't help us
#undef NV_ASSERT
#define NV_ASSERT(x) \
    do{if(!(x)){NvOsDebugPrintf("ASSERT at %s:%d\n", __FILE__,__LINE__);}}while(0)

#define LENS_FOCAL_LENGTH (6.12f)
#define LENS_HORIZONTAL_VIEW_ANGLE (56.8f)
#define LENS_VERTICAL_VIEW_ANGLE (56.8f)

/**
 * Sensor default settings. Phase 2. Sensor Dependent.
 * #defines make the code more readable and helps maintainability
 * The template tries to keep the list reduced to just things that
 * are used more than once.  Ideally, all numbers in the code would use
 * a #define.
 */
// manual exposure control
//
#define SENSOR_BAYER_DEFAULT_PLL_MULT               (0x0046)
#define SENSOR_BAYER_DEFAULT_PLL_PRE_DIV            (0x0004)
#define SENSOR_BAYER_DEFAULT_PLL_POS_DIV            (0x0001)
#define SENSOR_BAYER_DEFAULT_PLL_VT_PIX_DIV         (0x000A)
#define SENSOR_BAYER_DEFAULT_PLL_VT_SYS_DIV         (0x0001)

#define SENSOR_BAYER_DEFAULT_MAX_GAIN               (15.5)
#define SENSOR_BAYER_DEFAULT_MIN_GAIN               (1)
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

enum {
    OV9726_SENSOR_MODE_1280X720,
    OV9726_SENSOR_MODE_1280X800,
};

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
    "ov9726",  // string identifier, used for debugging
    NvOdmImagerSensorInterface_SerialB,
    {
        NvOdmImagerPixelType_BayerGBRG,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    // NvOdmImagerOrientation_90_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
    6000, // initial sensor clock (in khz)
    // preferred clock profile, the maximum pixel clk ran on sensor is 42MHz
    {{24000, 42.f/24.f}},
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_B,
        NVODMSENSORMIPITIMINGS_DATALANES_ONE,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        // 0: USE CONTINUOUS_CLOCK, 1: USE !CONTINUOUS_CLOCK
        0,
        0x7, // 0x2B
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    0, // only set if focuser device is available
    0, // only set if flash device is available
    NVODM_IMAGER_CAPABILITIES_END
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

static NvU16
SENSOR_F32_TO_GAIN(NvF32 x, NvF32 MaxGain, NvF32 MinGain)
{
    /* According to Omnivision, the gain range should be between 1.0 ~ 15.5.
       Bit7 is the digital gain and shoudn't be used. */
    NvU16 reg0x0205Bit6to4;
    NvU16 reg0x0205Bit0To3;

    NvF32 gainForBit0To3;
    NvF32 gainReminder;

    // for OV9726, gain is up to x15.5
    // 15.5, 7.75, 3.875, 1.9375, 1.0 (min)
    if (x > MaxGain / 2.0)
    {
        reg0x0205Bit6to4 = 0x70;
        gainReminder = x / 8.0;
    }
    else if (x > MaxGain / 4.0)
    {
        reg0x0205Bit6to4 = 0x30;
        gainReminder = x / 4.0;
    }
    else if (x > MaxGain / 8.0)
    {
        reg0x0205Bit6to4 = 0x10;
        gainReminder = x / 2.0;
    }
    else
    {
        reg0x0205Bit6to4 = 0;
        gainReminder = x;
    }

    if (gainReminder > MaxGain / 8.0)
    {
        gainReminder = MaxGain / 8.0;
    }
    if (gainReminder < MinGain)
    {
        gainReminder = MinGain;
    }

    gainForBit0To3 = (NvF32)((gainReminder - MinGain) * 16.0);
    reg0x0205Bit0To3 = (NvU16)(gainForBit0To3);
    if ((gainForBit0To3 - (NvF32)reg0x0205Bit0To3 > 0.5) && (reg0x0205Bit0To3 != 0xf))
    {
        reg0x0205Bit0To3 += 1;
    }
    reg0x0205Bit0To3 &= 0x0f; // might not need this?

    NVDEBUG_LOG("x=%f, MaxGain=%f, reg0x0205Bit7to4=%x, reg0x0205Bit0To3=%d\n",
                 x, MaxGain, reg0x0205Bit6to4, reg0x0205Bit0To3);

    return (reg0x0205Bit6to4 | reg0x0205Bit0To3);
}

typedef struct SensorBayerov9726ModeDependentSettingsRec
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 MinFrameLength;
    NvBool SupportsFastMode;
    NvF32 InherentGain;
} SensorBayerov9726ModeDependentSettings;

static SensorBayerov9726ModeDependentSettings ModeDependentSettings_ov9726_1280X720 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL, // 00340 and 0x0341
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL, // -4
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL,
    NV_FALSE,
    1.0f, // No binning
};

static SensorBayerov9726ModeDependentSettings ModeDependentSettings_ov9726_1280X800 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL,
    NV_FALSE,
    1.0f, // No binning
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
   in the ov9726 kernel driver.
*/

static SensorSetModeSequence g_SensorBayerSetModeSequenceListCSISingle[] =
{
    [OV9726_SENSOR_MODE_1280X720] = {{{1280, 720}, {0, 0}, 30, 1.0, 0, NvOdmImagerPartialCropMode,
        {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL}, NULL, &ModeDependentSettings_ov9726_1280X720},
    [OV9726_SENSOR_MODE_1280X800] = {{{1280, 800}, {0, 0}, 30, 1.0, 0, NvOdmImagerNoCropMode,
        {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL}, NULL, &ModeDependentSettings_ov9726_1280X800},
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
    NvBool FastSetMode;

    NvBool showSkipWriteGains;
    NvBool showSkipWriteExposure;

    NvOdmImagerStereoCameraMode StereoCameraMode;
} SensorBayerContext;

typedef const char *CString;

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

static NV_INLINE
NvBool Sensorov9726_ChooseModeIndex(
    SensorBayerContext *pContext,
    NvSize Resolution,
    NvU32 *pIndex);

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
    NvF32 ExpTime = *pNewExposureTime;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;

    if (pContext->TestPatternMode)
        return NV_FALSE;

    NVDEBUG_LOG("new exptime from ISP = %f, freq = %d, line length = %d \n",
        ExpTime, Freq, (int)LineLength);

    if (ExpTime > pContext->MaxExposure)
    {
        NvOsDebugPrintf("new exptime over limit! New = %f out of (%f, %f)\n",
            ExpTime, pContext->MaxExposure, pContext->MinExposure);
        ExpTime = pContext->MaxExposure;
    }
    if (ExpTime < pContext->MinExposure)
    {
        NvOsDebugPrintf("new exptime over limit! New = %f out of (%f, %f)\n",
            ExpTime, pContext->MaxExposure, pContext->MinExposure);
        ExpTime = pContext->MinExposure;
    }

    // Here, we have to decide if the new exposure time is valid
    // based on the sensor and current sensor settings.
    // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
    NewCoarseTime = (NvU32)((Freq * ExpTime) / LineLength + DIFF_INTEGRATION_TIME_OF_MODE);
    if (NewCoarseTime == 0)
        NewCoarseTime = 1;
    NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;

    // Consider requested max frame rate,
    if (pContext->RequestedMaxFrameRate > 0.0f) // was 1.0
    {
        NvU32 RequestedMinFrameLengthLines = 0;
        RequestedMinFrameLengthLines =
        (NvU32)(Freq / (LineLength * pContext->RequestedMaxFrameRate));
        NVDEBUG_LOG("pContext->RequestedMaxFrameRate = %f\n",
                pContext->RequestedMaxFrameRate);
        if (NewFrameLength < RequestedMinFrameLengthLines)
            NewFrameLength = RequestedMinFrameLengthLines;

        NVDEBUG_LOG("FrameLength updated =%d\n",NewFrameLength);
    }

    // Clamp to sensor's limit
    if (NewFrameLength > pContext->MaxFrameLength)
    {
        NewFrameLength = pContext->MaxFrameLength;
        NewCoarseTime = NewFrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
    }
    else if (NewFrameLength < pContext->MinFrameLength)
        NewFrameLength = pContext->MinFrameLength;

    // return the new frame length
    if (pFrameLength)
    {
        *pFrameLength = NewFrameLength;
    }

    if (NewFrameLength != pContext->FrameLength)
    {
        // write new value only when pFrameLength is NULL
        if (!pFrameLength)
        {
            int ret = ioctl(pContext->camera_fd, OV9726_IOCTL_SET_FRAME_LENGTH, NewFrameLength);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set frame length failed %s\n", strerror(errno));
        }

        // Update variables depending on FrameLength.
        pContext->FrameLength = NewFrameLength;
        pContext->FrameRate = (NvF32)pContext->VtPixClkFreqHz /
                      (NvF32)(pContext->FrameLength * pContext->LineLength);
        if (pContext->FrameRate > pContext->RequestedMaxFrameRate)
        {
            pContext->FrameRate = pContext->RequestedMaxFrameRate;
        }
    }
    else
    {
        NVDEBUG_LOG("Frame length keep as before : %d \n", NewFrameLength);
    }

    // FrameLength should have been updated but we have to check again
    // in case FrameLength gets clamped.
    if (NewCoarseTime >= pContext->FrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
    {
        NewCoarseTime = pContext->FrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
    }

    // return the new coarse time
    if (pCoarseTime)
    {
        *pCoarseTime = NewCoarseTime;
    }

    if (pContext->CoarseTime != NewCoarseTime)
    {
        // write new coarse time only when pCoarseTime is NULL
        if (!pCoarseTime)
        {
            int ret = ioctl(pContext->camera_fd, OV9726_IOCTL_SET_COARSE_TIME, NewCoarseTime);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set coarse time failed %s\n", strerror(errno));
        }

        // Calculate the new exposure based on the sensor and sensor settings.
        pContext->Exposure = ((NewCoarseTime -
                               (NvF32)DIFF_INTEGRATION_TIME_OF_MODE) *
                              (NvF32)LineLength) / Freq;
        pContext->CoarseTime = NewCoarseTime;

    }

    NVDEBUG_LOG("new exposure = %f (%d lines)\n", pContext->Exposure,  pContext->CoarseTime);
    NVDEBUG_LOG("Freq = %f    FrameLen=0x%x(%d), NewCoarseTime=0x%x(%d), LineLength=0x%x(%d)\n",
                Freq,pContext->FrameLength, pContext->FrameLength,
                NewCoarseTime, NewCoarseTime,
                pContext->LineLength, pContext->LineLength);
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
// manual gain control, reg 0x0205, gain formula is below (one more than ov5650), and up to 2x2x2x2x(15/16+1)=31
//
//   Gain = (0205[7]+1) x (0205[6]+1) x (0205[5]+1) x (0205[4]+1) x (0205[3:0]/16+1)

#if (BUILD_FOR_AOS == 0)
    NvU16 NewGains = 0;
    NvU32 i = 1;
    int ret;

    if ((pGains[i] > pContext->MaxGain) ||
        (pGains[i] < pContext->MinGain))    {
        NvOsDebugPrintf("SensorBayer_WriteGains: gain exceeds limit!\n");
        return NV_FALSE;
    }

    // convert float to gain's unit
    NewGains = SENSOR_F32_TO_GAIN(pGains[i], pContext->MaxGain, pContext->MinGain);

    if (pGain)
    {
        *pGain = NewGains;
    }
    else
    {
        ret = ioctl(pContext->camera_fd, OV9726_IOCTL_SET_GAIN, NewGains);
        if (ret < 0)
            NvOsDebugPrintf("ioctl to set gain failed %s\n", strerror(errno));
    }

    NvOdmOsMemcpy(pContext->Gains, pGains, sizeof(NvF32)*4);

    NVDEBUG_LOG("SensorBayer_WriteGains = %f, %f, %f, %f, %x, %x\n\n", pGains[0],
            pGains[1], pGains[2], pGains[3], pGain, (int)NewGains);
#endif
    return NV_TRUE;
}

static NvBool
SensorBayer_GroupHold(
    SensorBayerContext *pContext,
    const NvOdmImagerSensorAE *sensorAE)
{
#if (BUILD_FOR_AOS == 0)
    NvBool Status = NV_TRUE;
    struct ov9726_ae ae;

    NvOdmOsMemset(&ae, 0, sizeof(struct ov9726_ae));

    if (sensorAE->gains_enable==NV_TRUE) {
        Status = SensorBayer_WriteGains(pContext, sensorAE->gains, &ae.gain);
        if (Status == NV_FALSE) {
            return NV_FALSE;
        }
        ae.gain_enable = NV_TRUE;
    }

    if (sensorAE->ET_enable==NV_TRUE) {
        Status = SensorBayer_WriteExposure(pContext, &sensorAE->ET, &ae.frame_length, &ae.coarse_time);
        if (Status == NV_FALSE) {
            return NV_FALSE;
        }
        ae.frame_length_enable = NV_TRUE;
        ae.coarse_time_enable = NV_TRUE;
    }

    NVDEBUG_LOG("%s %f %f: %04x %04x %04x\n", __func__, sensorAE->ET, sensorAE->gains[1],
                ae.frame_length, ae.coarse_time, ae.gain);
    if (ae.gain_enable==NV_TRUE || ae.coarse_time_enable==NV_TRUE ||
            ae.frame_length_enable==NV_TRUE) {
        int ret = ioctl(pContext->camera_fd, OV9726_IOCTL_SET_GROUP_HOLD, &ae);
        if (ret < 0)
        {
            NvOsDebugPrintf("ioctl to set group hold failed %s\n", strerror(errno));
            return NV_FALSE;
        }
    }
#endif
    return NV_TRUE;
}

/**
 * SensorBayer_Open. Phase 1.
 * Initialize sensor bayer and its private context
 */
static NvBool SensorBayer_Open(NvOdmImagerHandle hImager)
{
    SensorBayerContext *pSensorBayerContext = NULL;

    NVDEBUG_LOG("SensorBayer_Open\n");
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorBayerContext =
        (SensorBayerContext*)NvOdmOsAlloc(sizeof(SensorBayerContext));
    if (!pSensorBayerContext)
        goto fail;

    NvOdmOsMemset(pSensorBayerContext, 0, sizeof(SensorBayerContext));

    pSensorBayerContext->TestPatternMode = NV_FALSE;

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

     // no need to convert gain's min/max to float for ov9726, because they are defined as float already.
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

    NVDEBUG_LOG("SensorBayer_Close\n");
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
    NVDEBUG_LOG("SensorBayer_GetCapabilities\n");
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

    NVDEBUG_LOG("SensorBayer_ListModes\n");
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
    NvBool Status = NV_FALSE;
#if (BUILD_FOR_AOS == 0)
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
    NvU32 Index;
    SensorBayerov9726ModeDependentSettings *pModeSettings;
    NvU32 CoarseTime = 0;
    NvU32 FrameLength = 0;
    NvU16 Gain = 0;

    NvOsDebugPrintf("Setting resolution to %dx%d, exposure %f, gains %f\n",
            pParameters->Resolution.width, pParameters->Resolution.height,
            pParameters->Exposure, pParameters->Gains[0]);

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
        (SensorBayerov9726ModeDependentSettings*)
        g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

    // set initial line lenth, frame length, coarse time, and max/min of frame length
    pContext->LineLength = pModeSettings->LineLength;
    pContext->FrameLength = pModeSettings->FrameLength;
    pContext->CoarseTime = pModeSettings->CoarseTime;
    // normal way to set frame length limit that only impacted by sensor hw
    pContext->MaxFrameLength = SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH;
    // don't want a frame to be shorter than the preset value. So below is ok.
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

    // default gains are 1.0
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
            NvOsDebugPrintf("Sensorov9726_WriteExposure failed\n");
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
            NvOsDebugPrintf("Sensorov9726_WriteGains failed\n");
        }
    }

    struct ov9726_mode cust_mode = {
        Index,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.height,
        FrameLength, CoarseTime, Gain
    };

    int ret;

    ret = ioctl(pContext->camera_fd, OV9726_IOCTL_SET_MODE, &cust_mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__, strerror(errno));
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
    NvOsDebugPrintf("pixel clock : %d\n",
             pContext->VtPixClkFreqHz);

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
#endif
    return Status;
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

    NVDEBUG_LOG("SensorBayer_SetPowerLevel %d\n", PowerLevel);

    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    switch (PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
#ifdef O_CLOEXEC
            pContext->camera_fd = open("/dev/ov9726", O_RDWR|O_CLOEXEC);
#else
            pContext->camera_fd = open("/dev/ov9726", O_RDWR);
#endif
            if (pContext->camera_fd < 0) {
                NvOsDebugPrintf("Can not open camera device: %s\n", strerror(errno));
                Status = NV_FALSE;
            } else {
                NVDEBUG_LOG("ov9726 Camera fd open as: %d\n", pContext->camera_fd);
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

    switch (Param)
    {
        // Phase 2.
        case NvOdmImagerParameter_SensorExposure:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_SensorExposure\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            Status = SensorBayer_WriteExposure(pContext, (NvF32*)pValue, NULL, NULL);
            break;

        // Phase 2.
        case NvOdmImagerParameter_SensorGain:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_SensorGain\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));
            Status = SensorBayer_WriteGains(pContext, pValue, NULL);
            break;

        case NvOdmImagerParameter_SensorGroupHold:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorAE));
            Status = SensorBayer_GroupHold(pContext, (NvOdmImagerSensorAE *)pValue);
        }
        break;

        // Phase 3.
        case NvOdmImagerParameter_OptimizeResolutionChange:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_OptimizeResolutionChange\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            Status = NV_TRUE;
            break;

        // This is optional but nice to have.
        case NvOdmImagerParameter_SelfTest:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_SelfTest\r\n");
            // Not Implemented.
            break;

        // Phase 3.
        case NvOdmImagerParameter_MaxSensorFrameRate:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_MaxSensorFrameRate\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pContext->RequestedMaxFrameRate = *((NvF32*)pValue);
            break;

        case NvOdmImagerParameter_TestMode:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_TestMode\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            pContext->TestPatternMode = *((NvBool *)pValue);

            // If Sensor is initialized then only program the test mode registers
            // else just save the test mode in pContext->TestPatternMode and once
            // the sensor gets initialized in Sensorov9726_SetMode() there itself
            // program the test mode registers.
            if (pContext->SensorInitialized)
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
            break;

        case NvOdmImagerParameter_Reset:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_Reset\r\n");
            return NV_TRUE;
            break;

        case NvOdmImagerParameter_CustomizedBlockInfo:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_CustomizedBlockInfo\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerCustomInfo));

            if (pValue)
            {
                NvOdmImagerCustomInfo *pCustom =
                    (NvOdmImagerCustomInfo *)pValue;
                if (pCustom->pCustomData && pCustom->SizeOfData)
                {
                    NVDEBUG_LOG("nvodm_imager: (%d) %s\n",
                          pCustom->SizeOfData,
                          pCustom->pCustomData);
                    // one could parse and interpret the data packet here
                }
                else
                {
                    NVDEBUG_LOG("nvodm_imager: null packet\n");
                }
            }
            else
            {
                NvOsDebugPrintf("nvodm_imager: null pointer\n");
            }
            break;

        case NvOdmImagerParameter_StereoCameraMode:
            NVDEBUG_LOG("SensorBayer_SetParameter: NvOdmImagerParameter_StereoCameraMode\r\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerStereoCameraMode));
                    Status = NV_FALSE;
            break;

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

    switch (Param)
    {
        case NvOdmImagerParameter_SensorType:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorType\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorType));
            *((NvOdmImagerSensorType*)pValue) = NvOdmImager_SensorType_Raw;
            break;

        // Phase 1.
        case NvOdmImagerParameter_CalibrationData:
            NVDEBUG_LOG("NvOdmImagerParameter_CalibrationData\n");
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
            NVDEBUG_LOG("NvOdmImagerParameter_SensorExposureLimits\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 2 * sizeof(NvF32));

            pFloatValue[0] = pContext->MinExposure;
            pFloatValue[1] = pContext->MaxExposure;
            break;

        // Phase 1, it can return min = max = 1.0f
        // Phase 2, return the real numbers.
        case NvOdmImagerParameter_SensorGainLimits:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorGainLimits\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 2 * sizeof(NvF32));
            pFloatValue[0] = pContext->MinGain;
            pFloatValue[1] = pContext->MaxGain;
            break;

        case NvOdmImagerParameter_FocalLength:
            NVDEBUG_LOG("NvOdmImagerParameter_FocalLength\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            ((NvF32*)pValue)[0] = LENS_FOCAL_LENGTH;
            break;

        // Get the sensor status. This is optional but nice to have.
        case NvOdmImagerParameter_DeviceStatus:
            NVDEBUG_LOG("NvOdmImagerParameter_DeviceStatus\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerDeviceStatus));
            {
                NvOdmImagerDeviceStatus *pStatus;
                pStatus = (NvOdmImagerDeviceStatus *)pValue;
                {
                    int ret;

                    ret = ioctl(pContext->camera_fd, OV9726_IOCTL_GET_STATUS, pStatus->Values);
                    if (ret < 0)    {
                        NvOsDebugPrintf("ioctl to gets status failed %s\n", strerror(errno));
                    }

                    pStatus->Count = 1;
                }
            }
            break;

        // Phase 1, it can return min = max = 10.0f
        //          (the fps in g_pSensorBayerSetModeSequenceList)
        // Phase 2, return the real numbers
        case NvOdmImagerParameter_SensorFrameRateLimits:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorFrameRateLimits\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                2 * sizeof(NvF32));

            pFloatValue[0] = pContext->MinFrameRate;
            pFloatValue[1] = pContext->MaxFrameRate;

            break;

        // Phase 1, it can return 1.0f
        // Phase 2, return the real numbers.
        case NvOdmImagerParameter_SensorFrameRate:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorFrameRate\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));

            pFloatValue[0] = pContext->FrameRate;
            break;

        // Phase 3.
        // Get the override config data.
        case NvOdmImagerParameter_CalibrationOverrides:
            NVDEBUG_LOG("NvOdmImagerParameter_CalibrationOverrides\n");
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
            // code to read the ID from sensor
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
                ret = ioctl(pContext->camera_fd, OV9726_IOCTL_GET_FUSEID, &fuse_id);
                if (ret < 0)
                {
                    NvOsDebugPrintf("ioctl to get fuse id failed %s\n", strerror(errno));
                    Status = NV_FALSE;
                }
                else
                {
                    NvOdmOsMemset(pValue, 0, SizeOfValue);
                    NvOdmOsMemcpy(pValue, &fuse_id, sizeof(struct nvc_fuseid));
                    Status = NV_TRUE;
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
            NVDEBUG_LOG("NvOdmImagerParameter_RegionUsedByCurrentResolution\n");
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
            NVDEBUG_LOG("NvOdmImagerParameter_LinesPerSecond\n");
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
            SensorBayerov9726ModeDependentSettings *pModeSettings =
                        (SensorBayerov9726ModeDependentSettings*)g_pSensorBayerSetModeSequenceList
                        [pContext->ModeIndex].pModeDependentSettings;
            NvF32 height = (NvF32)(g_pSensorBayerSetModeSequenceList[pContext->ModeIndex]
                        .Mode.ActiveDimensions.height);
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *pFloatValue = ((NvF32)height * pModeSettings->LineLength) / (NvF32)pContext->VtPixClkFreqHz;
        }
        break;

        case NvOdmImagerParameter_MaxSensorFrameRate:
            NVDEBUG_LOG("NvOdmImagerParameter_MaxSensorFrameRate\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = pContext->RequestedMaxFrameRate;
            break;

        case NvOdmImagerParameter_ExpectedValues:
            NVDEBUG_LOG("NvOdmImagerParameter_ExpectedValues\n");
            break;

        case NvOdmImagerParameter_SensorGain:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorGain\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));

            NvOdmOsMemcpy(pValue, pContext->Gains, sizeof(NvF32) * 4);
            break;

        case NvOdmImagerParameter_SensorExposure:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorExposure\n");
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
            NVDEBUG_LOG("NvOdmImagerParameter_SensorFrameRateLimitsAtResolution\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerFrameRateLimitAtResolution));

            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);

            {
                NvOdmImagerFrameRateLimitAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                SensorBayerov9726ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerFrameRateLimitAtResolution *)pValue;
                pData->MinFrameRate = 0.0f;
                pData->MaxFrameRate = 0.0f;

                MatchFound =
                    Sensorov9726_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerov9726ModeDependentSettings*)
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
            NVDEBUG_LOG("NvOdmImagerParameter_SensorInherentGainAtResolution\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerInherentGainAtResolution));

            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);

            {
                NvOdmImagerInherentGainAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                SensorBayerov9726ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerInherentGainAtResolution *)pValue;
                pData->InherentGain = pContext->InherentGain;

                if (pData->Resolution.width == 0 && pData->Resolution.height == 0)
                    break; // They just wanted the current value

                MatchFound =
                    Sensorov9726_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerov9726ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

                pData->InherentGain = pModeSettings->InherentGain;
            }
            break;

        case NvOdmImagerParameter_SensorExposureLatchTime:
            NVDEBUG_LOG("NvOdmImagerParameter_SensorExposureLatchTime\n");
            {
                NvU32 *pUIntValue = (NvU32*)pValue;
                *pUIntValue = 2;
            }
            break;

        case NvOdmImagerParameter_HorizontalViewAngle:
            NVDEBUG_LOG("NvOdmImagerParameter_HorizontalViewAngle\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = LENS_HORIZONTAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            NVDEBUG_LOG("NvOdmImagerParameter_VerticalViewAngle\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = LENS_VERTICAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_GetBestSensorMode:
            NVDEBUG_LOG("NvOdmImagerParameter_GetBestSensorMode\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerSensorMode));
            {
                NvU32 modeIndex = NvOdmImagerGetBestSensorMode((const NvOdmImagerSensorMode*)pValue,
                                                                g_pSensorBayerSetModeSequenceList,
                                                                pContext->NumModes);
                NvOdmOsMemcpy(pValue,
                              &g_pSensorBayerSetModeSequenceList[modeIndex].Mode,
                              sizeof(NvOdmImagerSensorMode));
            }
            break;

        // Phase 3.
        default:
            NVDEBUG_LOG("SensorBayer_GetParameter(): %d not supported\n", Param);
            Status = NV_FALSE;
    }
#endif

    return Status;
}

static NV_INLINE NvBool
Sensorov9726_ChooseModeIndex(
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

/**
 * SensorBayer_GetHal. Phase 1.
 * return the hal functions associated with sensor bayer
 */
NvBool SensorBayerOV9726_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

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

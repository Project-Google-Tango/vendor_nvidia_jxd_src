/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#if (BUILD_FOR_AOS == 0)

#define NV_ENABLE_DEBUG_PRINTS (0)

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <asm/types.h>
#include "ar0833.h"
#endif

#include "sensor_bayer_ar0833.h"
#include "imager_util.h"
#include "nvodm_query_gpio.h"

#include "sensor_bayer_ar0833_config.h"

#define AR0833_HDR_ENABLE NV_TRUE

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
#define SENSOR_BAYER_DEFAULT_MAX_GAIN            (16.0)
#define SENSOR_BAYER_DEFAULT_MIN_GAIN            (1.0)

#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF     (1)     // Minmum margin between frame length and coarse integration time.

#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH  (0xFFFF)

#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE (0x002)

#define DIFF_INTEGRATION_TIME_OF_ADDITION_MODE    (0.0f)
#define DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE (0.0f)
#define DIFF_INTEGRATION_TIME_OF_MODE            DIFF_INTEGRATION_TIME_OF_ELIMINATION_MODE


#define DEBUG_PRINTS    (0)

#define HACT_START_FULL (0)
#define VACT_START_FULL (0)

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides.isp",
};

static const char *pFactoryBlobFiles[] =
{
    "/data/factory.bin",
    "/data/calibration.bin",
};

struct ar0833_modeinfo {
        int xres;
        int yres;
        NvU8 hdr;
        NvU8 lanes;
        NvU16 line_len;
        NvU16 frame_len;
        NvU16 coarse_time;
        NvU16 coarse_time_2nd;
        NvU16 xstart;
        NvU16 xend;
        NvU16 ystart;
        NvU16 yend;
        NvU16 xsize;
        NvU16 ysize;
        NvU16 gain;
        NvU8 x_flip;
        NvU8 y_flip;
        NvU8 x_bin;
        NvU8 y_bin;
        NvU16 vt_pix_clk_div;
        NvU16 vt_sys_clk_div;
        NvU16 pre_pll_clk_div;
        NvU16 pll_multi;
        NvU16 op_pix_clk_div;
        NvU16 op_sys_clk_div;
};

static NV_INLINE NvF32 SENSOR_GAIN_TO_F32(NvU32 Gain)
{
    return ((NvF32)(Gain & 0xFFE0) / (128 << 5) * ((Gain & 3) + 1) * (((Gain & 4) >> 2) + 1));
}

static NV_INLINE NvU16
SENSOR_F32_TO_GAIN(NvF32 x, NvF32 MaxGain, NvF32 MinGain)
{
    NvU32 total_gain = (NvU32)(x * 128);
    NvU16 ret = 0;
    NvU16 analogGain;

    NvU16 adcBit2;
    NvU16 colampBit1to0;
    NvU16 digital4p7;

    if(total_gain <= 1 * 128) {
        analogGain = 1;
        adcBit2 = 0;
        colampBit1to0 = 0;
    } else if(total_gain <= 2 * 128) {
        analogGain = 2;
        adcBit2 = 0;
        colampBit1to0 = 1;
    } else if(total_gain <= 3 * 128) {
        analogGain = 3;
        adcBit2 = 0;
        colampBit1to0 = 2;
    } else if(total_gain <= 4 * 128) {
        analogGain = 4;
        adcBit2 = 0;
        colampBit1to0 = 3;
    } else if(total_gain <= 6 * 128) {
        analogGain = 6;
        adcBit2 = 1;
        colampBit1to0 = 2;
    } else {
        analogGain = 8;
        adcBit2 = 1;
        colampBit1to0 = 3;
    }

    digital4p7 = total_gain / analogGain;
    ret = (digital4p7 << 5) | (adcBit2 << 2) | (colampBit1to0);

    NV_DEBUG_PRINTF(("%s: Requested %fx Gain\n", __func__, x));
    NV_DEBUG_PRINTF(("%s: result (%X)): Digital4p7[15-5] %X, ADC[4-2] %X, COLAMP[1-0] %X\n",
        __func__, ret, digital4p7, adcBit2, colampBit1to0));

    return ret;
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
    "AR0833",  // string identifier, used for debugging
    NvOdmImagerSensorInterface_SerialA,
    {
        NvOdmImagerPixelType_BayerRGGB,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
    6000, // initial sensor clock (in khz)
    // preferred clock profile: {mclk, ratio = max_output_pixelclock/mclk}
    // max_output_pixelclock should be >= max pixel clock in the mode table
    { {24000, 300.f/24.f} },
    //{ {48000, 350.f/24.f}},
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_A,
        NVODMSENSORMIPITIMINGS_DATALANES_FOUR,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        0x1, // USE CONTINUOUS_CLOCK,
        0x8  // THS_SETTLE is a receiver timing parameter and can vary with CSI clk.
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    FOCUSER_NVC_GUID1, // only set if focuser device is available
    TORCH_NVC_GUID, // only set if flash device is available
    NVODM_IMAGER_CAPABILITIES_END,
    0x00, // FlashControlEnabled
    0x00, // AdjustableFlashTiming
    0x01 // isHDR
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
   in the ar0833 kernel driver.
*/

static SensorSetModeSequence g_SensorBayerSetModeSequenceListCSISingle[] =
{
    // WxH           x,y    fps   ar    set mode sequence
    // full res 3264x2448, aspect ratio of full mode = 4:3
      {{{3264, 2448}, {0, 0}, 30, 1.0, 12.5, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, 4832}, NULL, NULL}, //HDR
    //{{{1920, 1080}, {0, 0}, 30, 1.0, 16.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, 4832}, NULL, NULL},   //HDR
    //{{{1632, 1224}, {0, 0}, 30, 1.0, 16.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, XXXX}, NULL, NULL},
      {{{3264, 1836}, {0, 0}, 30, 1.0, 12.5, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, 4832}, NULL, NULL}, //HDR

    //HDR enable temperatlly set in SensorBayer_SetMode function
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

    NvU32 CoarseTime;
    NvU32 CoarseTimeShort;
    NvU32 VtPixClkFreqHz;

    NvU32 LineLength;
    NvU32 FrameLength;

    NvU32 MaxFrameLength;
    NvU32 MinFrameLength;
    NvBool TestPatternMode;

    NvBool showSkipWriteGains;
    NvBool showSkipWriteExposure;

    NvOdmImagerStereoCameraMode StereoCameraMode;

    NvBool HDREnabled;
    NvF32 HDRRatio;
    struct ar0833_modeinfo CurrentModeInfo;
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
SensorAR0833_ChooseModeIndex(
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

static NvBool
SensorBayer_GetModeInfo(SensorBayerContext *pContext, struct ar0833_modeinfo *mi)
{
#if (BUILD_FOR_AOS == 0)
    int ret = ioctl(pContext->camera_fd, AR0833_IOCTL_GET_MODE, mi);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to get mode info failed %s\n", __func__, strerror(errno));
        return NV_FALSE;
    }
#endif
    NV_DEBUG_PRINTF(("mode %d x %d %s:\n", mi->xres, mi->yres, mi->hdr ? "HDR" : "REG"));
    NV_DEBUG_PRINTF(("line_len = %d\n", mi->line_len));
    NV_DEBUG_PRINTF(("frame_len = %d\n", mi->frame_len));
    NV_DEBUG_PRINTF(("xsize = %d\n", mi->xsize));
    NV_DEBUG_PRINTF(("ysize = %d\n", mi->ysize));
    NV_DEBUG_PRINTF(("vt_pix_clk_div = %d\n", mi->vt_pix_clk_div));
    NV_DEBUG_PRINTF(("vt_sys_clk_div = %d\n", mi->vt_sys_clk_div));
    NV_DEBUG_PRINTF(("pre_pll_clk_div = %d\n", mi->pre_pll_clk_div));
    NV_DEBUG_PRINTF(("pll_multi = %d\n", mi->pll_multi));
    NV_DEBUG_PRINTF(("op_pix_clk_div = %d\n", mi->op_pix_clk_div));
    NV_DEBUG_PRINTF(("op_sys_clk_div = %d\n", mi->op_sys_clk_div));

    return NV_TRUE;
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
    NvU32 NewCoarseTimeShort = 0;
    NvU32 NewFrameLength = 0;
    NvF32 NewFrameRate = 0.f;

    if (pContext->TestPatternMode)
        return NV_FALSE;

    if (!pContext->SensorInitialized)
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

    NV_DEBUG_PRINTF(("%s: Freq = %f, NewExpTime = %f, LineLength=%f\n", __func__, Freq, NewExpTime, LineLength));
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

    NV_DEBUG_PRINTF(("1st calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime));
#if DEBUG_PRINTS
    NV_DEBUG_PRINTF(("1st calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime)i);
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
            ret = ioctl(pContext->camera_fd, AR0833_IOCTL_SET_FRAME_LENGTH,
                        NewFrameLength);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set frame length failed %s\n", strerror(errno));
        }
    }
    else
    {
#if DEBUG_PRINTS
        NV_DEBUG_PRINTF(("Frame length keep as before : %d \n", NewFrameLength));
#endif
    }

    if (NewCoarseTime != pContext->CoarseTime || pContext->HDREnabled)
    {
        // write new coarse time only when pCoarseTime is NULL
        if (!pCoarseTime || pContext->HDREnabled)
        {
            int ret;
            if (pContext->HDREnabled == NV_TRUE)
            {
                struct ar0833_hdr values;
                NewCoarseTimeShort = (NvU32)(NewCoarseTime/pContext->HDRRatio);
                values.coarse_time_long = NewCoarseTime;
                values.coarse_time_short = NewCoarseTimeShort;
                ret = ioctl(pContext->camera_fd, AR0833_IOCTL_SET_HDR_COARSE_TIME,
                            &values);
            }
            else
            {
                NewCoarseTimeShort = NewCoarseTime;
                ret = ioctl(pContext->camera_fd, AR0833_IOCTL_SET_COARSE_TIME,
                            NewCoarseTime);
            }
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set coarse time failed %s\n", strerror(errno));
        }
    }

    // Update Context
    pContext->Exposure = NewExpTime;
    pContext->FrameRate = NewFrameRate;
    pContext->FrameLength = NewFrameLength;
    pContext->CoarseTime = NewCoarseTime;
    pContext->CoarseTimeShort = NewCoarseTimeShort;

    NV_DEBUG_PRINTF(("2nd calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime));
#if DEBUG_PRINTS
    NV_DEBUG_PRINTF(("2nd calc: ExpTime=%f, FrameRate=%f, FrameLength=%d, CoraseTime=%d\n",
                     NewExpTime, NewFrameRate, NewFrameLength, NewCoarseTime));
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
// AR0833's
// manual gain control, reg 0x305E, gain formula is
//
//   Gain = (1+0x305E[1:0]) x (1+0x305E[2]) x (0x305x[15:5]+1)/128
//   where x is 6, 8, A, C for Gr, B, R, Gb respectively
//
#if (BUILD_FOR_AOS == 0)
    NvU32 i = 1;
    NvU16 NewGains = 0;

    if (pGains[i] > pContext->MaxGain)
        return NV_FALSE;
    if (pGains[i] < pContext->MinGain)
        return NV_FALSE;

    NewGains = SENSOR_F32_TO_GAIN(pGains[i], pContext->MaxGain, pContext->MinGain);
    NV_DEBUG_PRINTF(("-- %s -- writing gain %f, or %x\n", __func__, pGains[i], NewGains));
    if (pGain)
    {
        *pGain = NewGains;
    }
    else
    {
        int ret;
        ret = ioctl(pContext->camera_fd, AR0833_IOCTL_SET_GAIN, NewGains);
        if (ret < 0)
            NvOsDebugPrintf("ioctl to set gain failed %s\n", strerror(errno));
    }

    NvOdmOsMemcpy(pContext->Gains, pGains, sizeof(NvF32)*4);

#if DEBUG_PRINTS
    NV_DEBUG_PRINTF(("new gains = %f, %f, %f, %f\n\n", pGains[0],
        pGains[1], pGains[2], pGains[3]));
#endif
#endif
    return NV_TRUE;
}

static NvBool
SensorBayer_GroupHold(
    SensorBayerContext *pContext,
    NvOdmImagerSensorAE *sensorAE)
{
#if (BUILD_FOR_AOS == 0)
    int ret;
    struct ar0833_ae ae;
    NvU32 i = 1;

    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 ExpTime = sensorAE->ET;
    NvU32 NewCoarseTime = 0;
    NvU32 NewCoarseTimeShort = 0;
    NvU32 NewFrameLength = 0;

    NvOdmOsMemset(&ae, 0, sizeof(struct ar0833_ae));

    if (sensorAE->HDRRatio_enable == NV_TRUE)
    {
        pContext->HDRRatio = sensorAE->HDRRatio;
        if (!sensorAE->ET_enable)
        {
            ExpTime = pContext->Exposure;
            sensorAE->ET_enable = NV_TRUE;
        }
    }

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

        if (ExpTime > pContext->MaxExposure)
            ExpTime = pContext->MaxExposure;
        if (ExpTime < pContext->MinExposure)
            ExpTime = pContext->MinExposure;

        // Here, we have to decide if the new exposure time is valid
        // based on the sensor and current sensor settings.
        // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
        NewCoarseTime = (NvU32)((Freq * ExpTime) / LineLength + DIFF_INTEGRATION_TIME_OF_MODE);
        if( NewCoarseTime == 0 )
            NewCoarseTime = 1;
        NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;

        if (pContext->RequestedMaxFrameRate > 0.0f)
        {
            NvU32 RequestedMinFrameLengthLines = 0;
            RequestedMinFrameLengthLines =
               (NvU32)(Freq / (LineLength * pContext->RequestedMaxFrameRate));
            if (NewFrameLength < RequestedMinFrameLengthLines)
                NewFrameLength = RequestedMinFrameLengthLines;
        }

        // Clamp to sensor's limit
        if (NewFrameLength > pContext->MaxFrameLength)
            NewFrameLength = pContext->MaxFrameLength;
        else if (NewFrameLength < pContext->MinFrameLength)
            NewFrameLength = pContext->MinFrameLength;

        if (NewFrameLength != pContext->FrameLength)
        {
            ae.frame_length = NewFrameLength;
            ae.frame_length_enable = NV_TRUE;

            // Update variables depending on FrameLength.
            pContext->FrameLength = NewFrameLength;
            pContext->FrameRate = (NvF32)pContext->VtPixClkFreqHz * 2/
                              (NvF32)(pContext->FrameLength * pContext->LineLength);
        }
        else
        {
            NV_DEBUG_PRINTF(("Frame length keep as before : %d \n", NewFrameLength));
        }

        // FrameLength should have been updated but we have to check again
        // in case FrameLength gets clamped.
        if (NewCoarseTime > pContext->FrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
        {
            NewCoarseTime = pContext->FrameLength - SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
        }

        if (pContext->CoarseTime != NewCoarseTime || pContext->HDREnabled)
        {
            ae.coarse_time_enable = NV_TRUE;
            if (pContext->HDREnabled == NV_TRUE)
            {
                NewCoarseTimeShort = (NvU32)(NewCoarseTime/pContext->HDRRatio);
                ae.coarse_time = NewCoarseTime;
                ae.coarse_time_short = NewCoarseTimeShort;
            }
            else
                ae.coarse_time = NewCoarseTime;

            // Calculate the new exposure based on the sensor and sensor settings.
            pContext->Exposure    =
              (NvF32)((NewCoarseTime - DIFF_INTEGRATION_TIME_OF_MODE) * LineLength) / (NvF32) pContext->VtPixClkFreqHz;

            pContext->CoarseTime = NewCoarseTime;
            pContext->CoarseTimeShort = NewCoarseTimeShort;
        }
    }

    if (ae.gain_enable==NV_TRUE || ae.coarse_time_enable==NV_TRUE ||
            ae.frame_length_enable==NV_TRUE) {
        ret = ioctl(pContext->camera_fd, AR0833_IOCTL_SET_GROUP_HOLD, &ae);
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

     // no need to convert gain's min/max to float for AR0833, because they are defined as float already.
    pSensorBayerContext->MaxGain = SENSOR_BAYER_DEFAULT_MAX_GAIN;
    pSensorBayerContext->MinGain = SENSOR_BAYER_DEFAULT_MIN_GAIN;
    /**
     * Phase 2 ends here.
     */

    pSensorBayerContext->PowerLevel = NvOdmImagerPowerLevel_Off;
    pSensorBayerContext->HDREnabled = AR0833_HDR_ENABLE;
    pSensorBayerContext->HDRRatio = 1;

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
    struct ar0833_modeinfo *mi = &pContext->CurrentModeInfo;
    NvU32 CoarseTime = 0;
    NvU32 CoarseTimeShort = 0;
    NvU32 FrameLength = 0;
    NvU16 Gain = 0;
    int ret;

    NV_DEBUG_PRINTF(("Setting resolution to %dx%d, exposure %f, gains %f\n",
        pParameters->Resolution.width, pParameters->Resolution.height,
        pParameters->Exposure, pParameters->Gains[0]));

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

    mi->xres = g_pSensorBayerSetModeSequenceList[Index].Mode.ActiveDimensions.width;
    mi->yres = g_pSensorBayerSetModeSequenceList[Index].Mode.ActiveDimensions.height;
    mi->hdr = pContext->HDREnabled ? 1 : 0;
    Status = SensorBayer_GetModeInfo(pContext, mi);
    if (!Status)
        return Status;

    pContext->VtPixClkFreqHz = (NvU32)((pContext->SensorInputClockkHz * 1000 * mi->pll_multi) /
                    (mi->pre_pll_clk_div * mi->vt_pix_clk_div * mi->vt_sys_clk_div * 2 * 1)) * mi->lanes;

    // set initial line length, frame length, coarse time, and max/min of frame length
    pContext->LineLength = mi->line_len;
    pContext->FrameLength = mi->frame_len;
    pContext->CoarseTime = mi->coarse_time;
    pContext->MaxFrameLength = SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH;
    pContext->MinFrameLength = mi->frame_len;
    pContext->InherentGain = SENSOR_GAIN_TO_F32(mi->gain);

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
            NV_DEBUG_PRINTF(("SensorAR0833_WriteExposure failed\n"));
        }
    }
    else
    {
        FrameLength = pContext->FrameLength;
        CoarseTime = pContext->CoarseTime;
    }

    if (pParameters->Gains[0] != 0.0 && pParameters->Gains[1] != 0.0 &&
        pParameters->Gains[2] != 0.0 && pParameters->Gains[3] != 0.0)
    {
        Status = SensorBayer_WriteGains(pContext, pParameters->Gains, &Gain);
        if (!Status)
        {
            NV_DEBUG_PRINTF(("SensorAR0833_WriteGains failed\n"));
        }
    }

    if (pContext->HDREnabled == NV_TRUE)
    {
        CoarseTimeShort = (NvU32)(CoarseTime/pContext->HDRRatio);
    }
    else
    {
        CoarseTimeShort = CoarseTime;
    }
    pContext->CoarseTimeShort = CoarseTimeShort;

    struct ar0833_mode mode = {
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.height,
            FrameLength, CoarseTime, CoarseTimeShort, Gain, pContext->HDREnabled
    };

    ret = ioctl(pContext->camera_fd, AR0833_IOCTL_SET_MODE, &mode);
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
    NV_DEBUG_PRINTF(("-------------SetMode---------------\n"));
    NV_DEBUG_PRINTF(("Exposure : %f (%f, %f))\n",
             pContext->Exposure,
             pContext->MinExposure,
             pContext->MaxExposure));
    NV_DEBUG_PRINTF(("Gain : %f (%f, %f))\n",
             pContext->Gains[1],
             pContext->MinGain,
             pContext->MaxGain));
    NV_DEBUG_PRINTF(("FrameRate : %f (%f, %f))\n",
             pContext->FrameRate,
             pContext->MinFrameRate,
             pContext->MaxFrameRate));
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
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("SensorBayer_SetPowerLevel %d\n", PowerLevel));

    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
#if (BUILD_FOR_AOS == 0)
#ifdef O_CLOEXEC
            pContext->camera_fd = open("/dev/ar0833", O_RDWR|O_CLOEXEC);
#else
            pContext->camera_fd = open("/dev/ar0833", O_RDWR);
#endif // O_CLOEXEC
            if (pContext->camera_fd < 0) {
                NvOsDebugPrintf("Can not open ar0833 camera device: %s\n",
                strerror(errno));
                Status = NV_FALSE;
            } else {
                NV_DEBUG_PRINTF(("AR0833 Camera fd open as: %d\n", pContext->camera_fd));
                Status = NV_TRUE;
            }
            if (!Status)
                return NV_FALSE;
#endif // BUILD_FOR_AOS
            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
#if (BUILD_FOR_AOS == 0)
            close(pContext->camera_fd);
#endif // BUILD_FOR_AOS
            pContext->camera_fd = -1;
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
    NV_DEBUG_PRINTF(("SetParameter(): %x\n", Param));
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

        case NvOdmImagerParameter_SensorGroupHold:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorAE));
            Status = SensorBayer_GroupHold(pContext, (NvOdmImagerSensorAE *)pValue);
            break;
        }

        case NvOdmImagerParameter_SensorHDRRatio:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pContext->HDREnabled = NV_TRUE;
            pContext->HDRRatio = *((NvF32*)pValue);

            if (pContext->HDRRatio < 1.0)
            {
                pContext->HDRRatio = 1.f;
                NvOsDebugPrintf("Error: HDRRatio requested < 1.0. Clamping request to 1.0 \n");
            }

            // force update, so that the new exposure ratio takes effect
            Status = SensorBayer_WriteExposure(pContext, &pContext->Exposure, NULL, NULL);
        }
        break;

        // Phase 2.
        case NvOdmImagerParameter_SensorGain:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));
            Status = SensorBayer_WriteGains(pContext, pValue, NULL);
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
            // the sensor gets initialized in SensorAR0833_SetMode() there itself
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

        case NvOdmImagerParameter_SensorTriggerStill:
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
            NV_DEBUG_PRINTF(("SetParameter(): %d not supported\n", Param));
            //{
            //  int *test = NULL;
            //  *test = 1;
            //}
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
    NV_DEBUG_PRINTF(("GetParameter(): %d\n", Param));
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

                    ret = ioctl(pContext->camera_fd, AR0833_IOCTL_GET_STATUS, &status);
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
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvU8) * 16);

            // add your code to read the ID from sensor
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

        case NvOdmImagerParameter_MaxSensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = pContext->RequestedMaxFrameRate;
            break;

        case NvOdmImagerParameter_ExpectedValues:
            break;

        case NvOdmImagerParameter_SensorGroupHold:
            {
                NvBool *grouphold = (NvBool *) pValue;
                CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
                *grouphold = NV_TRUE;
                break;
            }

        case NvOdmImagerParameter_SensorGain:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));

            NvOdmOsMemcpy(pValue, pContext->Gains, sizeof(NvF32) * 4);
            break;

        case NvOdmImagerParameter_SensorExposure:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));

            NvOdmOsMemcpy(pValue, &pContext->Exposure, sizeof(NvF32));
            break;

        case NvOdmImagerParameter_SensorFrameRateLimitsAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerFrameRateLimitAtResolution));

            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);

            NV_DEBUG_PRINTF(("%s: NvOdmImagerParameter_SensorFrameRateLimitsAtResolution\n", __func__));
            {
                NvOdmImagerFrameRateLimitAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;

                pData = (NvOdmImagerFrameRateLimitAtResolution *)pValue;
                pData->MinFrameRate = 0.0f;
                pData->MaxFrameRate = 0.0f;

                MatchFound =
                    SensorAR0833_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                struct ar0833_modeinfo mi;
                NvU32 Freq;
                mi.xres = pData->Resolution.width;
                mi.yres = pData->Resolution.height;
                mi.hdr = pContext->HDREnabled ? 1 : 0;
                Status = SensorBayer_GetModeInfo(pContext, &mi);

                if (!Status)
                {
                    NvOsDebugPrintf("%s: SensorFrameRateLimitsAtResolution Error: ModeInfo Not Available.\n", __func__);
                    Status = NV_FALSE;
                }
                Freq = (NvU32)((pContext->SensorInputClockkHz * 1000 * mi.pll_multi) /
                        (mi.pre_pll_clk_div * mi.vt_pix_clk_div * mi.vt_sys_clk_div * 2 * 1)) * mi.lanes;
                pData->MaxFrameRate = (NvF32)Freq / (NvF32)(mi.frame_len * mi.line_len);
                pData->MinFrameRate = (NvF32)Freq / (NvF32)(SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH * mi.line_len);
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

                pData = (NvOdmImagerInherentGainAtResolution *)pValue;

                NV_DEBUG_PRINTF(("%s: NvOdmImagerParameter_SensorInherentGainAtResolution %d x %d\n",
                    __func__, pData->Resolution.width, pData->Resolution.height));
                if (pData->Resolution.width == 0 && pData->Resolution.height == 0)
                {
                    pData->InherentGain = pContext->InherentGain;
                    break; // They just wanted the current value
                }

                MatchFound =
                    SensorAR0833_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }
                struct ar0833_modeinfo mi;
                mi.xres = pData->Resolution.width;
                mi.yres = pData->Resolution.height;
                mi.hdr = pContext->HDREnabled ? 1 : 0;
                Status = SensorBayer_GetModeInfo(pContext, &mi);

                pData->InherentGain = SENSOR_GAIN_TO_F32(mi.gain);
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
                pData->SteadyStateIntraFrameSkip = 0;
                pData->SteadyStateFrameNumer = 0;
            }
            break;

        // Phase 3.
        default:
            NV_DEBUG_PRINTF(("GetParameter(): %d not supported\n", Param));
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

/**
 * SensorBayer_GetHal. Phase 1.
 * return the hal functions associated with sensor bayer
 */
NvBool SensorBayerAR0833_GetHal(NvOdmImagerHandle hImager)
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

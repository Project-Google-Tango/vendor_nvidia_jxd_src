/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

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
#include <ov5693.h>
#endif
#include "sensor_bayer_ov5693.h"
#include "nvc.h"
#include "nvc_image.h"
#include "camera.h"
#include "imager_util.h"
#include "nvodm_query_gpio.h"
#include "nvodm_imager.h"
#include "sensor_bayer_ov5693_camera_config.h"
#include "sensor_bayer_ov5693_front_camera_config.h"
#include "nvodm_tags.h"
#include "nv_log.h"
#include "camera_trace.h"

//#define OV5693_HDR_ENABLE NV_TRUE
#define OV5693_HDR_ENABLE NV_FALSE

#define OV5693_USING_REAL_GAIN  1

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
#define SENSOR_BAYER_PLL_MULT_DEFAULT            (0x64)
#define SENSOR_BAYER_PLL_MULT_DEFAULT_HDR_FULL      (0x6e)
#define SENSOR_BAYER_PLL_PRE_DIV_DEFAULT         (0x3)
#define SENSOR_BAYER_PLL_POS_DIV_DEFAULT         (1)

#ifdef  OV5693_USING_REAL_GAIN
#define SENSOR_BAYER_DEFAULT_MAX_GAIN            (16.0f)
#else
#define SENSOR_BAYER_DEFAULT_MAX_GAIN            (15.5f)
#endif
#define SENSOR_BAYER_DEFAULT_MIN_GAIN            (1.0f)

#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF     6

#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH  (0x7FFF)

#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE (0x0002)

#define DIFF_INTEGRATION_TIME_OF_REGULAR_MODE (0.0f)
#define DIFF_INTEGRATION_TIME_OF_MODE  DIFF_INTEGRATION_TIME_OF_REGULAR_MODE

#define SENSOR_BAYER_EFFECTIVE_PIXEL_LENGTH      2624
#define SENSOR_BAYER_EFFECTIVE_LINE_HEIGHT       1956
#define SENSOR_BAYER_ACTIVE_X0                   16
#define SENSOR_BAYER_ACTIVE_Y0                   6
#define SENSOR_BAYER_ACTIVE_X1                   2607
#define SENSOR_BAYER_ACTIVE_Y1                   1949
#define SENSOR_BAYER_PHYSICAL_X                  3.674f //mm
#define SENSOR_BAYER_PHYSICAL_Y                  2.738f //mm


// Normal Sensor Mode
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH      (0x0A80)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH     (0x07C0)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1296x972  496
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH  SENSOR_BAYER_DEFAULT_FRAME_LENGTH

#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P  (0x06d8)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P (0x02f8)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_720P  SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P
#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH_720P  (0x0BE0)


// HDR Sensor Mode 2592X1944
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_2592x1944_HDR      (0x0E70)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2592x1944_HDR     (0x07C0)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_2592x1944_HDR  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2592x1944_HDR-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_2592x1944_HDR  SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2592x1944_HDR

// HDR Sensor Mode 1296X972, 1920X1080
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_HDR      (0x0B40)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_HDR     (0x073A)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_HDR  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_HDR-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_HDR  SENSOR_BAYER_DEFAULT_FRAME_LENGTH_HDR

// HDR Sensor Mode 1280X720
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P_HDR  (0x06d8)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P_HDR (0x02f8)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P_HDR  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P_HDR-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_720P_HDR  SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P_HDR


#define DEBUG_PRINTS    (0)

// below 2 values are temparary implementation, should be moved to config file later
#define LENS_HYPERFOCAL_DISTANCE (1.0f / 1.50f)
#define LENS_MINIMUM_FOCUS_DISTANCE (1.0f / 0.10f)

#define LENS_FOCAL_LENGTH (2.67f)
#define LENS_HORIZONTAL_VIEW_ANGLE (60.4f)
#define LENS_VERTICAL_VIEW_ANGLE   (60.4f)

#define SENSOR_MIPI_LANE_NUMBER    (2)

#ifdef  OV5693_USING_REAL_GAIN
static NV_INLINE NvF32 SENSOR_GAIN_TO_F32(NvU32 x)
{
    return (NvF32)x / 16.0f;
} //SENSOR_GAIN_TO_F32

static NV_INLINE NvU32 SENSOR_F32_TO_GAIN(NvF32 x, NvF32 MaxGain, NvF32 MinGain)
{
    if (x > MaxGain)
        return 0;
    if (x < MinGain)
        return 0;

    return (NvU32)(x * 16 + 0.5);
} //SENSOR_F32_TO_GAIN
#else
static NV_INLINE NvU16
SENSOR_F32_TO_GAIN(NvF32 x, NvF32 MaxGain, NvF32 MinGain)
{
    NvU16 reg0x350bBit6=0x0;
    NvU16 reg0x350bBit5=0x0;
    NvU16 reg0x350bBit4=0x0;
    NvU16 reg0x350bBit0To3=0x0;

    NvF32 gainForBit0To3;
    NvF32 gainReminder;

    // for OV5693, gain is up to x15.5
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

    NV_DEBUG_PRINTF(("x=%f, MaxGain=%f, reg0x350bBit6=%d, reg0x350bBit5=%d, reg0x350bBit4=%d, reg0x350bBit0To3=%d\n",
                     x, MaxGain, reg0x350bBit6, reg0x350bBit5, reg0x350bBit4, reg0x350bBit0To3));


    return (reg0x350bBit6<<6) | (reg0x350bBit5<<5) | (reg0x350bBit4<<4) | (reg0x350bBit0To3);
}
#endif

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides.isp",
    "/data/nvcam/camera_overrides.isp",
};

static const char *pDeviceCalibrationFactoryBlobFiles[] =
{
   "/mnt/factory/camera/factory.bin",
};

static const char *pFactoryBlobFiles[] =
{
   "/data/factory.bin",
   "/data/nvcam/factory.bin",
   "/data/calibration.bin",
   "/data/nvcam/calibration.bin",
};


static const char *pOverrideFiles_front[] =
{
    "/data/camera_overrides_front.isp",
    "/data/nvcam/camera_overrides_front.isp",
};

static const char *pDeviceCalibrationFactoryBlobFiles_front[] =
{
   "/mnt/factory/camera/factory_front.bin",
};

static const char *pFactoryBlobFiles_front[] =
{
   "/data/factory_front.bin",
   "/data/nvcam/factory_front.bin",
   "/data/calibration_front.bin",
   "/data/nvcam/calibration_front.bin",
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
    "OV5693",  // string identifier, used for debugging
    //NvOdmImagerSensorInterface_SerialA,
    NvOdmImagerSensorInterface_SerialB,
    {
        NvOdmImagerPixelType_BayerBGGR,
    },
    NvOdmImagerOrientation_90_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
    6000, // initial sensor clock (in khz)
    {{24000, 160.0f/24.f}}, // preferred clock profile
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_A,
        NVODMSENSORMIPITIMINGS_DATALANES_TWO,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        1, // USE !CONTINUOUS_CLOCK,
        0 // It will be calculated from MIPI clock.
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    FOCUSER_AD5816_GUID, // FOCUSER_GUID, only set if focuser code available
    0, // only set if flash device is available
    NVODM_IMAGER_CAPABILITIES_END,
    0x00, // FlashControlEnabled
    0x00, // AdjustableFlashTiming
    0x01  // isHDR
};

typedef struct SensorBayerOV5693ModeDependentSettingsRec
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 CoarseTimeShort;
    NvU32 MinFrameLength;
    NvU32 MaxFrameLength;
    NvF32 InherentGain;
    NvU32 PllMult;
    NvU32 PllPreDiv;
    NvU32 PllPosDiv;
} SensorBayerOV5693ModeDependentSettings;

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_2592x1944 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH,
    SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH,
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_1920x1080 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH,
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_1296x972 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1296x972,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1296x972,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH,
    SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH,
    2.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_1280x720_120FPS =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_720P,
    SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH_720P,
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_2592x1944_HDR =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_2592x1944_HDR,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2592x1944_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_2592x1944_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_2592x1944_HDR,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_2592x1944_HDR,
    SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH,
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT_HDR_FULL,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_1920x1080_HDR =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_HDR,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_HDR,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_HDR,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_HDR,
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_1296x972_HDR =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_HDR,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_HDR,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_HDR,
    SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH,
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

SensorBayerOV5693ModeDependentSettings OV5693_ModeDependentSettings_1280x720_60FPS_HDR =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P_HDR,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_720P_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P_HDR,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_720P_HDR,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_720P_HDR,
    (SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_720P_HDR*2),
    1.0f,
    SENSOR_BAYER_PLL_MULT_DEFAULT,
    SENSOR_BAYER_PLL_PRE_DIV_DEFAULT,
    SENSOR_BAYER_PLL_POS_DIV_DEFAULT,
};

/**
 * A list of sensor mode and its SetModeSequence. Phase 0. Sensor Dependent.
 * When SetMode is called, it searches the list for desired resolution and
 * frame rate and write the SetModeSequence through i2c.
 * For Phase 3, more resolutions will be added to this table, allowing
 * for peak performance during Preview, D1 and 720P encode.
 */
static SensorSetModeSequence *g_pSensorBayerSetModeSequenceList;

static SensorSetModeSequence g_SensorBayerSetModeSequenceList[] =
{
    {{{2592, 1944},  {0, 0}, 30, 1.0, 17.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {1.f, 1.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH}, NULL, &OV5693_ModeDependentSettings_2592x1944},
    {{{1296, 972 },  {0, 0}, 30, 1.0, 9.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {2.f, 2.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH}, NULL, &OV5693_ModeDependentSettings_1296x972},
    {{{1920, 1080},  {0, 0}, 30, 1.0, 17.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {1.f, 1.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH}, NULL, &OV5693_ModeDependentSettings_1920x1080},
    {{{1280, 720 },  {0, 0},120, 1.0, 17.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {1.f, 1.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P}, NULL, &OV5693_ModeDependentSettings_1280x720_120FPS},
};

static SensorSetModeSequence g_SensorBayerSetModeSequenceList_HDR[] =
{
    {{{2592, 1944},  {0, 0}, 24, 1.0, 19.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {1.f, 1.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_2592x1944_HDR}, NULL, &OV5693_ModeDependentSettings_2592x1944_HDR},
#if 0
    {{{1296, 972 },  {0, 0}, 30, 1.0, 10.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {2.f, 2.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_HDR}, NULL, &OV5693_ModeDependentSettings_1296x972_HDR},
    {{{1920, 1080},  {0, 0}, 30, 1.0, 19.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {1.f, 1.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_HDR}, NULL, &OV5693_ModeDependentSettings_1920x1080_HDR},
    {{{1280, 720 },  {0, 0},120, 1.0, 19.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {1.f, 1.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_720P_HDR}, NULL, &OV5693_ModeDependentSettings_1280x720_60FPS_HDR},
#endif
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

    NvU32 FrameErrorCount;

    NvF32 Gains[4];
    NvF32 MaxGain;
    NvF32 MinGain;
    NvF32 InherentGain;

    NvU32 FrameRateScheme;
    NvF32 FrameRate;
    NvF32 MaxFrameRate;
    NvF32 MinFrameRate;
    NvF32 RequestedMaxFrameRate;
    NvF32 RegionReadOutTime;
    NvF32 LinesPerSecond;

    // Phase 2 variables. Sensor Dependent.
    // These are used to set or get exposure, frame rate, and so on.
    NvU32 PllMult;
    NvU32 PllPreDiv;
    NvU32 PllPosDiv;

    NvU32 CoarseTime;
    NvU32 CoarseTimeShort;
    NvU32 VtPixClkFreqHz;

    NvU32 LineLength;
    NvU32 FrameLength;

    NvU32 MaxFrameLength;
    NvU32 MinFrameLength;
    NvBool TestPatternMode;

    NvOdmImagerTestPattern TestPattern;

    NvBool HDREnabled;
    NvF32 HDRRatio;

    char DevName[NVODMIMAGER_IDENTIFIER_MAX];

    NvOdmImagerCapabilities Capabilities;

    char *pSensorCalibrationData;
    const char **pOverrideFiles;
    NvU32 NumOverrideFiles;
    const char **pDeviceCalibrationFactoryBlobFiles;
    NvU32 NumDeviceCalibrationFactoryBlobFiles;
    const char **pFactoryBlobFiles;
    NvU32 NumFactoryBlobFiles;
    NvF32 HyperFocal;
    NvF32 MinFocusDistance;
}SensorBayerContext;

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

static void
SensorBayer_SetSensorModeList(SensorBayerContext *pContext, NvBool hdr_en)
{
    if(hdr_en)
    {
        g_pSensorBayerSetModeSequenceList = g_SensorBayerSetModeSequenceList_HDR;
        pContext->NumModes =
            NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceList_HDR);
    }
    else
    {
        g_pSensorBayerSetModeSequenceList = g_SensorBayerSetModeSequenceList;
        pContext->NumModes =
            NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceList);
    }
}

static void
SensorOV5693_UpdateLinesPerSecond(
    SensorBayerContext *pContext
)
{
    NvF32 height = (NvF32)(g_pSensorBayerSetModeSequenceList
                        [pContext->ModeIndex].Mode.ActiveDimensions.height);

    pContext->LinesPerSecond = (NvF32)pContext->VtPixClkFreqHz * 2 /
                    (pContext->FrameLength * pContext->LineLength) * height;
    NV_DEBUG_PRINTF(("LinesPerSecond: %d x 2 / (%d x %d) * %f = %f\n", pContext->VtPixClkFreqHz,
                    pContext->FrameLength, pContext->LineLength, height, pContext->LinesPerSecond));
}

static NV_INLINE NvBool
SensorOV5693_ChooseModeIndex(
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
    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 ExpTime = *pNewExposureTime;
    NvU32 NewCoarseTime = 0;
    NvU32 NewCoarseTimeShort = 0;
    NvU32 NewFrameLength = 0;

    if (pContext->TestPatternMode)
        return NV_FALSE;

    if (*pNewExposureTime > pContext->MaxExposure || *pNewExposureTime < pContext->MinExposure)
    {
        NV_DEBUG_PRINTF(("new exptime over limit! New = %f out of (%f, %f)\n",
                         *pNewExposureTime,
                         pContext->MaxExposure,
                         pContext->MinExposure));
        return NV_FALSE;
    }

    // Here, we have to decide if the new exposure time is valid
    // based on the sensor and current sensor settings.
    // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
    NewCoarseTime = (NvU32)((Freq * ExpTime) / LineLength-DIFF_INTEGRATION_TIME_OF_MODE);
    if( NewCoarseTime == 0 )
        NewCoarseTime = 1;

    if(pContext->FrameRateScheme == NvOdmImagerFrameRateScheme_Imager)
    {
        NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
    }
    else
    {
        if (pContext->FrameRate > pContext->MaxFrameRate)
            pContext->FrameRate = pContext->MaxFrameRate;
        if (pContext->FrameRate < pContext->MinFrameRate)
            pContext->FrameRate = pContext->MinFrameRate;

        NewFrameLength = (NvU32)(pContext->VtPixClkFreqHz /
                          (NvF32)(pContext->FrameRate* pContext->LineLength));
    }

    // Clamp to sensor's limit
    if (NewFrameLength > pContext->MaxFrameLength)
        NewFrameLength = pContext->MaxFrameLength;
    else if (NewFrameLength < pContext->MinFrameLength)
        NewFrameLength = pContext->MinFrameLength;

    // return the new frame length
    if (pFrameLength)
    {
        *pFrameLength = NewFrameLength;
    }

    if (NewFrameLength != pContext->FrameLength)
    {
#if (BUILD_FOR_AOS == 0)
        // write new value only when pFrameLength is NULL
        if (!pFrameLength)
        {
            int ret;
            ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_FRAME_LENGTH,
                        NewFrameLength);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set mode failed %s\n", strerror(errno));
        }
#endif

        // Update variables depending on FrameLength.
        pContext->FrameLength = NewFrameLength;
        pContext->FrameRate = (NvF32)pContext->VtPixClkFreqHz /
                              (NvF32)(pContext->FrameLength * pContext->LineLength);
        SensorOV5693_UpdateLinesPerSecond(pContext);
    }
    else
    {
#if DEBUG_PRINTS
        NvOsDebugPrintf("Frame length keep as before : %d \n", NewFrameLength);
#endif
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

    if (pContext->CoarseTime != NewCoarseTime || pContext->HDREnabled)
    {
#if (BUILD_FOR_AOS == 0)
        // write new coarse time only when pCoarseTime is NULL
        if (!pCoarseTime || pContext->HDREnabled)
        {
            int ret;
            if (pContext->HDREnabled == NV_TRUE)
            {
                struct ov5693_hdr values;
                NewCoarseTimeShort = (NvU32)(NewCoarseTime/pContext->HDRRatio);
                values.coarse_time_long = NewCoarseTime;
                values.coarse_time_short = NewCoarseTimeShort;
                NV_DEBUG_PRINTF(("[OV5693] SensorBayer_WriteExposure :: HDR :: long - 0x%x,  short - 0x%x\n",
                                values.coarse_time_long, values.coarse_time_short));

                ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_HDR_COARSE_TIME,
                            &values);

            }
            else
            {
                NV_DEBUG_PRINTF(("[OV5693] SensorBayer_WriteExposure :: No HDR :: NewCoarseTime - 0x%x\n", NewCoarseTime));
                NewCoarseTimeShort = OV5693_INVALID_COARSE_TIME;
                ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_COARSE_TIME,
                            NewCoarseTime);
            }
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set mode failed %s\n", strerror(errno));
        }
#endif

        // Calculate the new exposure based on the sensor and sensor settings.
        pContext->Exposure = ((NewCoarseTime +
                               (NvF32)DIFF_INTEGRATION_TIME_OF_MODE) *
                              (NvF32)LineLength) / Freq;
        pContext->CoarseTime = NewCoarseTime;
        pContext->CoarseTimeShort = NewCoarseTimeShort;
    }

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

    NvU32 i = 1;
    NvU16 NewGains = 0;

    if (pGains[i] > pContext->MaxGain || pGains[i] < pContext->MinGain)
    {
        NvOsDebugPrintf("Err:ov5693 odm:%s:gain %f is out of range (%f, %f)\n",
                         __func__, pGains[i], pContext->MinGain, pContext->MaxGain);
        return NV_FALSE;
    }

    // prepare and write register
    NewGains = (NvU16)SENSOR_F32_TO_GAIN(pGains[i], pContext->MaxGain, pContext->MinGain);

    if (pGain)
    {
        *pGain = NewGains;
    }
    else
    {
#if (BUILD_FOR_AOS == 0)
        int ret;
        ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_GAIN, NewGains);
        if (ret < 0)
            NvOsDebugPrintf("ioctl to set gain failed %s\n", strerror(errno));
#endif
    }

    NvOdmOsMemcpy(pContext->Gains, pGains, sizeof(NvF32)*4);

    NV_DEBUG_PRINTF(("new gains = %f, %f, %f, %f\n\n", pGains[0],
        pGains[1], pGains[2], pGains[3]));

    return NV_TRUE;
}

static NvBool
SensorBayer_GroupHold(
    SensorBayerContext *pContext,
    NvOdmImagerSensorAE *sensorAE)
{
    NV_TRACE_BEGIN_D(NV_ODM_SENSOR_TAG);
#if (BUILD_FOR_AOS == 0)
    NvU32 i = 1;

    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 ExpTime = sensorAE->ET;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;

    int ret;
    struct ov5693_ae ae;
    NvOdmOsMemset(&ae, 0, sizeof(struct ov5693_ae));

    if (sensorAE->HDRRatio_enable == NV_TRUE)
    {
        pContext->HDRRatio = sensorAE->HDRRatio;
        if (!sensorAE->ET_enable)
        {
            ExpTime = pContext->Exposure;
            sensorAE->ET_enable = NV_TRUE;
        }
    }

     if (pContext->Gains[i] != sensorAE->gains[i] && sensorAE->gains_enable==NV_TRUE){
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

        if (ExpTime > pContext->MaxExposure ||
            ExpTime < pContext->MinExposure)
        {
            NV_DEBUG_PRINTF(("new exptime over limit! New = %f out of (%f, %f)\n",
                             ExpTime,
                             pContext->MaxExposure,
                             pContext->MinExposure));
            return NV_FALSE;
        }

        // Here, we have to decide if the new exposure time is valid
        // based on the sensor and current sensor settings.
        // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
        NewCoarseTime = (NvU32)((Freq * ExpTime) / LineLength+DIFF_INTEGRATION_TIME_OF_MODE);
        if( NewCoarseTime == 0 )
            NewCoarseTime = 1;

        if(pContext->FrameRateScheme == NvOdmImagerFrameRateScheme_Imager)
        {
            NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;
        }
        else
        {
        if (pContext->FrameRate > pContext->MaxFrameRate)
            pContext->FrameRate = pContext->MaxFrameRate;
        if (pContext->FrameRate < pContext->MinFrameRate)
            pContext->FrameRate = pContext->MinFrameRate;

            NewFrameLength = (NvU32)(pContext->VtPixClkFreqHz /
                              (NvF32)(pContext->FrameRate* pContext->LineLength));
        }

        NV_DEBUG_PRINTF(("[OV5693] ExpTime = %.4f, coarseTime = %d\n", ExpTime, NewCoarseTime));

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
            pContext->FrameRate = (NvF32)pContext->VtPixClkFreqHz /
                              (NvF32)(pContext->FrameLength * pContext->LineLength);
            SensorOV5693_UpdateLinesPerSecond(pContext);
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

        if (pContext->CoarseTime != NewCoarseTime)
        {
            ae.coarse_time = NewCoarseTime;
            ae.coarse_time_enable = NV_TRUE;

            // Calculate the new exposure based on the sensor and sensor settings.
            pContext->Exposure = ((NewCoarseTime -
                                   (NvF32)DIFF_INTEGRATION_TIME_OF_MODE) *
                                  (NvF32)LineLength) / Freq;
            pContext->CoarseTime = NewCoarseTime;

            if (pContext->HDREnabled == NV_TRUE)
            {
                pContext->CoarseTimeShort =
                    ae.coarse_time_short = (NvU32)(NewCoarseTime/pContext->HDRRatio);
            } else {
                pContext->CoarseTimeShort =
                    ae.coarse_time_short = OV5693_INVALID_COARSE_TIME;
            }
        }
    }

    if (ae.gain_enable==NV_TRUE || ae.coarse_time_enable==NV_TRUE ||
            ae.frame_length_enable==NV_TRUE) {
        NV_DEBUG_PRINTF(("[OV5693] GroupHold :: long - 0x%x,  short - 0x%x, HDREnable - %d\n",
                        ae.coarse_time, ae.coarse_time_short, pContext->HDREnabled));

        ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_GROUP_HOLD, &ae);
        if (ret < 0)
        {
            NvOsDebugPrintf("ioctl to set group hold failed %s\n", strerror(errno));
            return NV_FALSE;
        }
    }
#endif
    NV_TRACE_END();
    return NV_TRUE;
}

/**
 * SensorBayer_Open. Phase 1.
 * Initialize sensor bayer and its private context
 */
static NvBool SensorBayer_Open(NvOdmImagerHandle hImager)
{
    SensorBayerContext *pSensorBayerContext = NULL;
    int err;
    NvU32 *ClockMultiplier;

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorBayerContext =
        (SensorBayerContext*)NvOdmOsAlloc(sizeof(SensorBayerContext));
    if (!pSensorBayerContext)
        goto fail;

    pSensorBayerContext->TestPatternMode = NV_FALSE;
    pSensorBayerContext->TestPattern = NvOdmImagerTestPattern_None;

    NvOdmOsMemset(pSensorBayerContext, 0, sizeof(SensorBayerContext));

    if (hImager->pSensor->GUID == SENSOR_BAYER_OV5693_GUID)
    {
        sprintf(pSensorBayerContext->DevName, "/dev/ov5693");
        pSensorBayerContext->pSensorCalibrationData =
            pSensorCalibrationData;
        pSensorBayerContext->pOverrideFiles =
            &pOverrideFiles[0];
        pSensorBayerContext->NumOverrideFiles =
            sizeof(pOverrideFiles) /
            sizeof(pOverrideFiles[0]);
        pSensorBayerContext->pDeviceCalibrationFactoryBlobFiles =
            &pDeviceCalibrationFactoryBlobFiles[0];
        pSensorBayerContext->NumDeviceCalibrationFactoryBlobFiles =
            sizeof(pDeviceCalibrationFactoryBlobFiles) /
            sizeof(pDeviceCalibrationFactoryBlobFiles[0]);
        pSensorBayerContext->pFactoryBlobFiles =
            &pFactoryBlobFiles[0];
        pSensorBayerContext->NumFactoryBlobFiles =
            sizeof(pFactoryBlobFiles) /
            sizeof(pFactoryBlobFiles[0]);
        pSensorBayerContext->HyperFocal =
            LENS_HYPERFOCAL_DISTANCE;
        pSensorBayerContext->MinFocusDistance =
            LENS_MINIMUM_FOCUS_DISTANCE;
    }
    else if (hImager->pSensor->GUID == SENSOR_BAYER_OV5693_FRONT_GUID)
    {
        sprintf(pSensorBayerContext->DevName, "/dev/ov5693.1");
        pSensorBayerContext->pSensorCalibrationData =
            pSensorCalibrationData_front;
        pSensorBayerContext->pOverrideFiles =
            &pOverrideFiles_front[0];
        pSensorBayerContext->NumOverrideFiles =
            sizeof(pOverrideFiles_front) /
            sizeof(pOverrideFiles_front[0]);
        pSensorBayerContext->pDeviceCalibrationFactoryBlobFiles =
            &pDeviceCalibrationFactoryBlobFiles_front[0];
        pSensorBayerContext->NumDeviceCalibrationFactoryBlobFiles =
            sizeof(pDeviceCalibrationFactoryBlobFiles_front) /
            sizeof(pDeviceCalibrationFactoryBlobFiles_front[0]);
        pSensorBayerContext->pFactoryBlobFiles =
            &pFactoryBlobFiles_front[0];
        pSensorBayerContext->NumFactoryBlobFiles =
            sizeof(pFactoryBlobFiles_front) /
            sizeof(pFactoryBlobFiles_front[0]);
        pSensorBayerContext->HyperFocal = 0;
        pSensorBayerContext->MinFocusDistance = 0;
    }

#ifdef O_CLOEXEC
    pSensorBayerContext->camera_fd = open(
        pSensorBayerContext->DevName,
        O_RDWR|O_CLOEXEC);
#else
    pSensorBayerContext->camera_fd = open(
        pSensorBayerContext->DevName,
        O_RDWR);
#endif // O_CLOEXEC

    if (pSensorBayerContext->camera_fd < 0)
    {
        NvOsDebugPrintf("%s: Can not open camera device: %s\n",
            pSensorBayerContext->DevName, strerror(errno));
        goto fail;
    }
    else
        NV_DEBUG_PRINTF(("%s: Camera fd open as: %d\n",
            pSensorBayerContext->DevName,
            pSensorBayerContext->camera_fd));

    // get sensor capabilities from kernel driver
    err = ioctl(pSensorBayerContext->camera_fd,
                OV5693_IOCTL_GET_CAPS,
                &pSensorBayerContext->Capabilities);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: Can not get camera capabilities from kernel: %s\n",
            pSensorBayerContext->DevName, strerror(errno));
        // Copy the sensor caps from g_SensorBayerCaps
        NvOdmOsMemcpy(&pSensorBayerContext->Capabilities,
            &g_SensorBayerCaps,
            sizeof(NvOdmImagerCapabilities));
    }

    // convert clock multiplier back to floating point
    ClockMultiplier = (NvU32 *)
        &pSensorBayerContext->Capabilities.ClockProfiles[0].ClockMultiplier;
    pSensorBayerContext->Capabilities.ClockProfiles[0].ClockMultiplier =
        *ClockMultiplier / 1000000;
    ClockMultiplier = (NvU32 *)
        &pSensorBayerContext->Capabilities.ClockProfiles[1].ClockMultiplier;
    pSensorBayerContext->Capabilities.ClockProfiles[1].ClockMultiplier =
        *ClockMultiplier / 1000000;

    // default use non-AOHDR modes
    pSensorBayerContext->HDREnabled = NV_FALSE;
    SensorBayer_SetSensorModeList(
            pSensorBayerContext, pSensorBayerContext->HDREnabled);

    pSensorBayerContext->ModeIndex =
        pSensorBayerContext->NumModes; // invalid mode

    /**
     * Phase 2. Initialize exposure and gains.
     */
    pSensorBayerContext->Exposure = -1.0; // invalid exposure

    // no need to convert gain's min/max to float for OV5693,
    // because they are defined as float already.
    pSensorBayerContext->MaxGain = SENSOR_BAYER_DEFAULT_MAX_GAIN;
    pSensorBayerContext->MinGain = SENSOR_BAYER_DEFAULT_MIN_GAIN;
    /**
     * Phase 2 ends here.
     */

    pSensorBayerContext->PowerLevel = NvOdmImagerPowerLevel_Off;
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

#if (BUILD_FOR_AOS == 0)
    close(pContext->camera_fd);
#endif

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
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;

    // Copy the sensor caps from g_SensorBayerCaps
    NvOdmOsMemcpy(pCapabilities, &pContext->Capabilities,
        sizeof(NvOdmImagerCapabilities));
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
    NvBool Status = NV_FALSE;
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
    NvU32 Index;
    SensorBayerOV5693ModeDependentSettings *pModeSettings;

    NvU32 CoarseTime = 0;
    NvU32 CoarseTimeShort = 0;
    NvU32 FrameLength = 0;
    NvU16 Gain = 0;

    NV_TRACE_BEGIN_D(NV_ODM_SENSOR_TAG);
    NV_DEBUG_PRINTF(("[OV5693] Setting resolution to %dx%d, exposure %f, gains %f\n",
        pParameters->Resolution.width, pParameters->Resolution.height,
        pParameters->Exposure, pParameters->Gains[0]));

    pContext->FrameErrorCount = 0;

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

     pModeSettings =
        (SensorBayerOV5693ModeDependentSettings*)
        g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

    // These hardcoded numbers are from the set mode sequence and this formula
    // is based on this sensor. Sensor Dependent.
    //
    pContext->PllMult = pModeSettings->PllMult;
    pContext->PllPreDiv = pModeSettings->PllPreDiv;
    pContext->PllPosDiv = pModeSettings->PllPosDiv;

    pContext->VtPixClkFreqHz =
            (NvU32)(pContext->SensorInputClockkHz *  pContext->PllMult*SENSOR_MIPI_LANE_NUMBER /
                    (pContext->PllPreDiv * pContext->PllPosDiv) / 10) * 1000;
    NV_DEBUG_PRINTF(("[OV5693] %s: output clock is %u\n", __func__, pContext->VtPixClkFreqHz));

    // set initial line lenth, frame length, coarse time, and max/min of frame length
    pContext->LineLength = pModeSettings->LineLength;
    pContext->FrameLength = pModeSettings->FrameLength;
    pContext->CoarseTime = pModeSettings->CoarseTime;
    pContext->MaxFrameLength = pModeSettings->MaxFrameLength;
    pContext->MinFrameLength = pModeSettings->MinFrameLength;
    pContext->InherentGain = pModeSettings->InherentGain;

    // default to use imager scheme
    pContext->FrameRateScheme = NvOdmImagerFrameRateScheme_Core;

    pContext->Exposure    =
              (((NvF32)pContext->CoarseTime + DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength) /(NvF32)pContext->VtPixClkFreqHz;
    pContext->MaxExposure =
        (((NvF32)SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE + DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength ) / (NvF32)pContext->VtPixClkFreqHz;
    pContext->MinExposure =
              (((NvF32)SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE + DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength ) / (NvF32)pContext->VtPixClkFreqHz;

    if (!pContext->SensorInitialized) {
        pContext->FrameRate =
              (NvF32)pContext->VtPixClkFreqHz /
              (NvF32)(pContext->FrameLength * pContext->LineLength);
    }
    pContext->MaxFrameRate =
              (NvF32)pContext->VtPixClkFreqHz /
              (NvF32)(pContext->MinFrameLength * pContext->LineLength);
    pContext->MinFrameRate =
              (NvF32)pContext->VtPixClkFreqHz /
              (NvF32)(pContext->MaxFrameLength * pContext->LineLength);


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
            NvOsDebugPrintf("SensorOV5693_WriteExposure failed\n");
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
            NvOsDebugPrintf("SensorOV5693_WriteGains failed\n");
        }
    }

#if (BUILD_FOR_AOS == 0)
    if (pContext->HDREnabled == NV_TRUE)
    {
        CoarseTimeShort = (NvU32)(CoarseTime/pContext->HDRRatio);
    }
    else
    {
        CoarseTimeShort = OV5693_INVALID_COARSE_TIME;
    }
    pContext->CoarseTimeShort = CoarseTimeShort;


    int ret;
    struct ov5693_mode mode = {
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.height,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            PeakFrameRate,
        FrameLength, CoarseTime, CoarseTimeShort, Gain,
        pContext->HDREnabled
    };

    NvOsDebugPrintf("Set resolution to %dx%d\n", mode.xres, mode.yres);
    ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_MODE, &mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__,
            strerror(errno));
        Status = NV_FALSE;
    } else {
        Status = NV_TRUE;
    }
#endif

    if (!Status)
        return NV_FALSE;

    /**
     * the following is Phase 2.
     */

    // Update sensor context after set mode
    NV_ASSERT(pContext->SensorInputClockkHz > 0); // UN-commented


    NV_DEBUG_PRINTF(("-------------SetMode---------------\n"));
    NV_DEBUG_PRINTF(("Exposure : %f (%f, %f)\n",
             pContext->Exposure,
             pContext->MinExposure,
             pContext->MaxExposure));
    NV_DEBUG_PRINTF(("Gain : %f (%f, %f)\n",
             pContext->Gains[1],
             pContext->MinGain,
             pContext->MaxGain));
    NV_DEBUG_PRINTF(("FrameRate : %f (%f, %f)\n",
             pContext->FrameRate,
             pContext->MinFrameRate,
             pContext->MaxFrameRate));

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
    NvF32 height = (NvF32)(g_pSensorBayerSetModeSequenceList[Index].Mode.ActiveDimensions.height);
    pContext->RegionReadOutTime = height * pModeSettings->LineLength / pContext->VtPixClkFreqHz;
    NV_DEBUG_PRINTF(("RegionReadOutTime: %f x %d / %d = %f\n",
                    height, pModeSettings->LineLength, pContext->VtPixClkFreqHz, pContext->RegionReadOutTime));
    SensorOV5693_UpdateLinesPerSecond(pContext);

    // Wait 1.5 frames for gains/exposure to take effect
    NvOsSleepMS((NvU32)(1500.0 / (NvF32)(pContext->FrameRate)));

    pContext->SensorInitialized = NV_TRUE;

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
    NV_TRACE_END();
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
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
    int ret;

    NV_DEBUG_PRINTF(("SensorBayer_SetPowerLevel %d\n", PowerLevel));

    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    NV_TRACE_BEGIN_D(NV_ODM_SENSOR_TAG);
    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
#if (BUILD_FOR_AOS == 0)
            ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_POWER, NVC_PWR_ON);
            if (ret < 0 && errno != EINVAL) {
                NvOsDebugPrintf("ioctl(%d) to set power failed %s\n",
                        pContext->camera_fd, strerror(errno));
                Status = NV_FALSE;
            }
#endif // BUILD_FOR_AOS
            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
#if (BUILD_FOR_AOS == 0)
            ret = ioctl(pContext->camera_fd, OV5693_IOCTL_SET_POWER, NVC_PWR_OFF);
            if (ret < 0 && errno != EINVAL) {
                NvOsDebugPrintf("ioctl(%d) to set power failed %s\n",
                        pContext->camera_fd, strerror(errno));
                Status = NV_FALSE;
            }
#endif
            break;

        default:
            NV_ASSERT("!Unknown power level\n");
            Status = NV_FALSE;
            break;
    }

    if (Status)
        pContext->PowerLevel = PowerLevel;

    NV_TRACE_END();
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
        case NvOdmImagerParameter_FrameRateScheme:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvU32));
            pContext->FrameRateScheme = *((NvU32*)pValue);
        }
        break;

        // Phase 3.
        case NvOdmImagerParameter_SensorFrameRate:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pContext->FrameRate = *((NvF32*)pValue);
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
        }
        break;

        case NvOdmImagerParameter_SensorGroupHold:
        {
          CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorAE));
          Status = SensorBayer_GroupHold(pContext, (NvOdmImagerSensorAE *)pValue);
          break;
        }
        case NvOdmImagerParameter_SensorHDREnable:
        {
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            if (pContext->HDREnabled != *((NvBool*)pValue))
            {
                pContext->HDREnabled = *((NvBool*)pValue);
                SensorBayer_SetSensorModeList(
                        pContext, pContext->HDREnabled);
                // change to invalid mode.
                // upper level should trigger a mode switch right away
                pContext->ModeIndex = pContext->NumModes;
                NvOsDebugPrintf("[OV5693] NvOdmImagerParameter_SensorHDREnable: %d\n", pContext->HDREnabled);
            }
        }
        break;
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
        default:
            NV_DEBUG_PRINTF(("[OV5693]:SetParameter(): %d not supported\n", Param));
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

    NvF32 *pFloatValue = pValue;
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;
#if DEBUG_PRINTS
    NvOsDebugPrintf("[OV5693]:GetParameter(): %d\n", Param);
#endif
    switch(Param)
    {
        // Phase 1.
        case NvOdmImagerParameter_SensorType:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorType));
            *((NvOdmImagerSensorType*)pValue) = NvOdmImager_SensorType_Raw;
            break;

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
                    SetModeParameters Parameters;

#if (BUILD_FOR_AOS == 0)
                    uint16_t status;
                    int ret;
                    ret = ioctl(pContext->camera_fd, OV5693_IOCTL_GET_STATUS, &status);
                    if (ret < 0)
                        NvOsDebugPrintf("ioctl to gets status failed "
                            "%s\n", strerror(errno));
#endif
                    /* Assume this is status request due to a frame timeout */
                    pContext->FrameErrorCount++;
                    /* Multiple errors (in succession?) */
                    if ( pContext->FrameErrorCount > 4) {
                        pContext->FrameErrorCount = 0;
                        /* sensor has reset or locked up (ESD discharge?) */
                        /* (status = reg 0x3103 = PWUP_DIS, 0x91 is reset state */
                        /* Hard reset the sensor and reconfigure it */
#if (BUILD_FOR_AOS == 0)
                        NvOsDebugPrintf("Bad sensor state, reset and reconfigure"
                            "%s\n", strerror(status));
#endif
                        SensorBayer_SetPowerLevel(hImager, NvOdmImagerPowerLevel_Off);
                        SensorBayer_SetPowerLevel(hImager, NvOdmImagerPowerLevel_On);
                        Parameters.Resolution.width =
                            g_pSensorBayerSetModeSequenceList[pContext->ModeIndex].Mode.ActiveDimensions.width;
                        Parameters.Resolution.height =
                            g_pSensorBayerSetModeSequenceList[pContext->ModeIndex].Mode.ActiveDimensions.height;
                        Parameters.Exposure = pContext->Exposure;
                        Parameters.Gains[0] = pContext->Gains[0];
                        Parameters.Gains[1] = pContext->Gains[1];
                        Parameters.Gains[2] = pContext->Gains[2];
                        Parameters.Gains[3] = pContext->Gains[3];
                        SensorBayer_SetMode(hImager,&Parameters,NULL,&Parameters);
                    }
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
                    LoadOverridesFile(pContext->pOverrideFiles,
                        pContext->NumOverrideFiles);
                pCalibration->NeedsFreeing = (pCalibration->CalibrationData != NULL);
                Status = pCalibration->NeedsFreeing;
            }
            break;

        case NvOdmImagerParameter_ModuleCalibrationData_OTP:
            {
                NvOsDebugPrintf("%s: OTP Module Calibration not supported.\n", __FUNCTION__);
                Status = NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_ModuleCalibrationData_EEPROM:
            {
                NvOdmImagerPowerLevel PreviousPowerLevel;
                int ret;

                SensorBayer_GetPowerLevel(hImager, &PreviousPowerLevel);
                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = SensorBayer_SetPowerLevel(hImager, NvOdmImagerPowerLevel_On);
                    if (!ret)
                    {
                        NvOsDebugPrintf("%s: failed to set power to ON. Aborting EEPROM read.\n",
                                        __FUNCTION__);
                        Status = NV_FALSE;
                        break;
                    }
                }
                Status = LoadFactoryFromEEPROM(hImager, pContext->camera_fd, OV5693_IOCTL_GET_EEPROM_DATA, pValue);
                if (!Status)
                    NvOsDebugPrintf("%s: ioctl to read EEPROM failed!\n", __FUNCTION__);
                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = SensorBayer_SetPowerLevel(hImager, PreviousPowerLevel);
                    if (!ret)
                        NvOsDebugPrintf("%s: failed to set power to previous state after EEPROM read.\n",
                                        __FUNCTION__);
                }
            }
            break;

        case NvOdmImagerParameter_DeviceCalibrationData:
            {
                Status = LoadBlobFile(pContext->pDeviceCalibrationFactoryBlobFiles,
                                    pContext->NumDeviceCalibrationFactoryBlobFiles,
                                    pValue, SizeOfValue);
            }
            break;

        // Get the factory calibration data.
        case NvOdmImagerParameter_FactoryCalibrationData:
            {
                Status = LoadBlobFile(pContext->pFactoryBlobFiles,
                                    pContext->NumFactoryBlobFiles,
                                    pValue, SizeOfValue);
            }
            break;

        case NvOdmImagerParameter_FuseID:
            {
                CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(struct nvc_fuseid));

#if (BUILD_FOR_AOS == 0)
                NvOdmImagerPowerLevel PreviousPowerLevel;
                struct nvc_fuseid fuseid;
                int ret;

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

                // code to read the ID from sensor
                ret = ioctl(pContext->camera_fd, OV5693_IOCTL_GET_FUSEID, &fuseid);
                if (ret < 0)
                {
                    NvOsDebugPrintf("OV5693: ioctl to get sensor data failed %s\n", strerror(errno));
                    Status = NV_FALSE;
                }
                else
                {
                    NvOdmOsMemset(pValue, 0, SizeOfValue);
                    NvOdmOsMemcpy(pValue, &fuseid, sizeof(struct nvc_fuseid));
                    Status = NV_TRUE;
                }

                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = SensorBayer_SetPowerLevel(hImager, PreviousPowerLevel);
                    if (!ret)
                        NvOsDebugPrintf("failed to set power to previous state after fuse id read.\n");
                }
#endif
            }
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
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32 *)pValue = pContext->LinesPerSecond;
            if (!pContext->LinesPerSecond)
            {
                Status = NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_SensorActiveRegionReadOutTime:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32 *)pValue = pContext->RegionReadOutTime;
            if (!pContext->RegionReadOutTime)
            {
                Status = NV_FALSE;
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

        case NvOdmImagerParameter_SensorGroupHold:
            {
                NvBool *grouphold = (NvBool *) pValue;
                CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
                *grouphold = NV_TRUE;
                break;
            }

        case NvOdmImagerParameter_SensorExposure:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));

            NvOdmOsMemcpy(pValue, &pContext->Exposure, sizeof(NvF32));
            break;

        case NvOdmImagerParameter_SensorFrameRateLimitsAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerFrameRateLimitAtResolution));

            {
                NvOdmImagerFrameRateLimitAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                SensorBayerOV5693ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerFrameRateLimitAtResolution *)pValue;
                pData->MinFrameRate = 0.0f;
                pData->MaxFrameRate = 0.0f;

                MatchFound =
                    SensorOV5693_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerOV5693ModeDependentSettings*)
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


        case NvOdmImagerParameter_HorizontalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = LENS_HORIZONTAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pFloatValue[0] = LENS_VERTICAL_VIEW_ANGLE;
            break;

        case NvOdmImagerParameter_SensorInherentGainAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerInherentGainAtResolution));

            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);

            {
                NvOdmImagerInherentGainAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                SensorBayerOV5693ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerInherentGainAtResolution *)pValue;
                pData->InherentGain = pContext->InherentGain;

                if (pData->Resolution.width == 0 && pData->Resolution.height == 0)
                    break; // They just wanted the current value

                MatchFound =
                    SensorOV5693_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerOV5693ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

                pData->InherentGain = pModeSettings->InherentGain;
            }
            break;

        // This parameter tells the autoexposure algorithm that the changes to gain
        // or exposure registers in OV5693 take 2 frames to take effect.
        case NvOdmImagerParameter_SensorExposureLatchTime:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                                             sizeof(NvU32));
            {
                NvU32 *pUIntValue = (NvU32*)pValue;
                *pUIntValue = 2;    /* default value */
            }
            break;

        case NvOdmImagerParameter_SensorIsHDRSensor:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            {
                NvBool *IsHDRSensor = (NvBool *)pValue;
                *IsHDRSensor = g_SensorBayerCaps.isHDRSensor;
                NV_DEBUG_PRINTF(("[OV5693] GetParam: IsHDRSensor = %d\n", *IsHDRSensor));
            }
            break;

        case NvOdmImagerParameter_SensorHDRRatio:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            {
                NvF32 *Ratio = (NvF32 *)pValue;
                *Ratio = pContext->HDRRatio;
                NV_DEBUG_PRINTF(("[OV5693] GetParam: HDRRatio = %f\n", *Ratio));
            }
            break;

        // Phase 3.
        default:
            NV_DEBUG_PRINTF(("[OV5693]:GetParameter(): %d not supported\n", Param));
            Status = NV_FALSE;
            break;
    }

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

static NvBool SensorBayer_GetStaticProperties(
    NvOdmImagerHandle hImager,
    SensorStaticProperties *pProperties)
{
    SensorBayerOV5693ModeDependentSettings *pModeSettings;
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
        Status = SensorBayer_Open(hImager);
        if (Status == NV_FALSE)
        {
            return NV_FALSE;
        }
        bNewInst = NV_TRUE;
    }

    pProperties->pCap = &g_SensorBayerCaps;

    // get num of modes supported
    SensorBayer_ListModes(hImager, NULL, &pProperties->NumOfModes);
    NV_DEBUG_PRINTF(("%s %d\n", __func__, pProperties->NumOfModes));

    if (pProperties->NumOfModes > NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceList))
    {
        ModeNum = NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceList);
    }
    else
    {
        ModeNum = pProperties->NumOfModes;
    }
    // get mode settings
    for (idx = 0; idx < ModeNum; idx++) {
        pModeSettings = (SensorBayerOV5693ModeDependentSettings*)
            g_pSensorBayerSetModeSequenceList[idx].pModeDependentSettings;
        pModeProperty = &pProperties->ModeList[idx];
        NvOdmOsMemcpy(&pModeProperty->ActiveSize,
            &g_SensorBayerSetModeSequenceList[idx].Mode.ActiveDimensions,
            sizeof(pModeProperty->ActiveSize));
        pModeProperty->PeakFrameRate =
            g_SensorBayerSetModeSequenceList[idx].Mode.PeakFrameRate;
        pModeProperty->LineLength = pModeSettings->LineLength;
        pModeProperty->FrameLength = pModeSettings->FrameLength;
        pModeProperty->CoarseTime = pModeSettings->CoarseTime;
        pModeProperty->MinFrameLength = pModeSettings->MinFrameLength;
        pModeProperty->InherentGain = pModeSettings->InherentGain;
        pModeProperty->PllMult = pModeSettings->PllMult;
        pModeProperty->PllPreDiv = pModeSettings->PllPreDiv;
        pModeProperty->PllPosDiv = pModeSettings->PllPosDiv;
        pModeProperty->PixClkAdjustment = 0.1f;
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
    pProperties->MinGain =
        ((SensorBayerContext*)hImager->pSensor->pPrivateContext)->MinGain;
    pProperties->MaxGain =
        ((SensorBayerContext*)hImager->pSensor->pPrivateContext)->MaxGain;

    // get lens focal settings
    pProperties->HyperFocal = LENS_HYPERFOCAL_DISTANCE;
    pProperties->MinFocusDistance = LENS_MINIMUM_FOCUS_DISTANCE;

    if (bNewInst)
    {
        SensorBayer_Close(hImager);
    }
    return Status;
}

/**
 * SensorBayer_GetHal. Phase 1.
 * return the hal functions associated with sensor bayer
 */
NvBool SensorBayerOV5693_GetHal(NvOdmImagerHandle hImager)
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
    hImager->pSensor->pfnStaticQuery = SensorBayer_GetStaticProperties;

    return NV_TRUE;
}

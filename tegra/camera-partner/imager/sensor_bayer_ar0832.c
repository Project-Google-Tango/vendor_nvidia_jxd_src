/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <stdint.h>
#include "ar0832_main.h"
#include "sensor_bayer_mt9e013_config.h"
#include "sensor_bayer_ar0832_config.h"
#include "sensor_focuser_ar0832_common.h"
#endif

#include "sensor_bayer_ar0832.h"
#include "nvc.h"
#include "imager_util.h"
#include "nvodm_query_gpio.h"

#define LENS_FOCAL_LENGTH   3.5f

#define CAL_DEBUG_FILE_IX 0

#define USE_CLOCK_MAXIMUM 1
#define USE_DIGITAL_GAIN 0
#define USE_BINNING_MODE 1
#define sizeof_field(s,f) sizeof(((s *) 0)->f)

// enable the real assert even in release mode
#undef NV_ASSERT
#define NV_ASSERT(x) \
    do{if(!(x)){NvOsDebugPrintf("ASSERT at %s:%d\n", __FILE__,__LINE__);}}while(0)

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)    (sizeof(a)) / (sizeof((a)[0]))
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
    "/data/camera_overrides.isp",
};

static const char *pFactoryBlobFiles[] =
{
    "/data/factory.bin",
    "/data/calibration.bin",
};

/**
 * Sensor default settings. Phase 2. Sensor Dependent.
 * #defines make the code more readable and helps maintainability
 * The template tries to keep the list reduced to just things that
 * are used more than once.  Ideally, all numbers in the code would use
 * a #define.
 */

// this part needs to match sequence!
//   pre_pll_clk_div     2     ==> register 0x0304
//   pll_multiplier      64    ==> register 0x0306
//   vt_sys_clk_div      1     ==> register 0x0302
//   vt_pix_clk_div      4     ==> register 0x0300
#define SENSOR_BAYER_DEFAULT_PLL_PRE_DIV         (2)  // 0x0304
#define SENSOR_BAYER_DEFAULT_PLL_MULT            (64) // 0x0306
#define SENSOR_BAYER_DEFAULT_PLL_POS_DIV         (1)  // 0x0302
#define SENSOR_BAYER_DEFAULT_PLL_VT_PIX_DIV      (4)  // 0x0300
#define SENSOR_BAYER_DEFAULT_PLL_VT_SYS_DIV      (1)  // not used for aptina 8140. Some other sensors have this sys div

// 2278 pixels. FINE integration set to fixed so far.
#define SENSOR_BAYER_DEFAULT_EXPOSURE_FINE       (0x08E6)

// In datasheet, there is following statement.
// Need to check hw version by reading its ID to make sure if it is rev2 /rev3 or other revisions.
// "for sensor's rev 2, the minimum gain is x1.5, rev 3 will be x1. Rev c need to divide 1.2 to get real gain"
//
// global analog gain register is 0x305e
// (we use per channel color gain 0x3056/0x3058/0x305a/0x305c in this code).
// per channel gain register 0x3056/0x3058/0x305a/0x305c, float (not 0x206/0x208/0x20a/0x20c).
#define SENSOR_BAYER_DEFAULT_MAX_ANALOG_GAIN            (15.875)
//  per channel gain register 0x3056/0x3058/0x305a/0x305c, float (0x206/0x208/0x20a/0x20c).
#define SENSOR_BAYER_DEFAULT_MIN_ANALOG_GAIN            (1.5)
// The additional digital gain to be used from the sensor.
#define SENSOR_BAYER_DIGITAL_GAIN                       (1.0)
// pg 9, coarse integration time max margin default value is 1
#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF            (1)
// pg 10, reg R0x1142, 32-bit 65535
#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH           (0xFFFF)
#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE        (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-\
                                                         SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
// pg 10, reg R0x1140
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE        (0x02)

//// FULL resolution
// 4924, need match sequence
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL           (0x133C)
// 2599, need match sequence
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL          (0xA27)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL       (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL-\
                                                        SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL      (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL)

//// 2880x1620
// 4536, need match sequence
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_2880x1620      (0x11B8)
// 1763, need match sequence
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2880x1620     (0x06E3)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_2880x1620  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2880x1620-\
                                                        SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_2880x1620 (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2880x1620)

//// 1920x1080
// 4536, need match sequence
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_1920x1080      (0x1139)
// 1763, need match sequence
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1920x1080     (0x05C4)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1920x1080  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1920x1080-\
                                                        SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_1920x1080 (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1920x1080)

//// 1632x1224
// 4122, need match sequence
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_1632x1224      (0x101A)
// 1552, need match sequence
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1632x1224     (0x0610)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1632x1224  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1632x1224-\
                                                        SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_1632x1224 (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1632x1224)

//// 800x600
// 2216, need match sequence
#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_800x600      (0x08A8)
// 743, need match sequence
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_800x600     (0x02E7)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_800x600  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_800x600-\
                                                        SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_800x600 (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_800x600)

#define LENS_HORIZONTAL_VIEW_ANGLE (50.1f)
#define LENS_VERTICAL_VIEW_ANGLE   (38.5f)

//
#define DIFF_INTEGRATION_TIME_OF_MODE                   (0.0f) // TO-CHECK.
/**
 * sensor register address
 */

#define VALUE_HIGH(v) (((v) >> 8) & 0xFF)
#define VALUE_LOW(v) ((v) & 0xFF)

#define SENSOR_CHECKERBOARD(r,c) \
        ((((r+2)/16)&1) == ((c/16)&1) ? 0x0000 : 0x03FF)

static NV_INLINE NvU16
SENSOR_F32_TO_GAIN(NvF32 x, NvF32 MaxGain, NvF32 MinGain, NvF32 DigitalGain)
{
    // 11/15
    // register for Aptina gain module is R0x305E
    //   R0x305E[10:0] analogue gain
    //   R0x305E[14:12] digital gain
    //
    // TABLE: desired analogue gain     ==> recommended gain register setting
    // 1x - 1.984375x   ==> 0x060 - 0x07F // this was 0x040 - 0x07F in one document,
    // which however, the recommend min is 1.5x, change 0x40=>0x60
    // 2x - 3.96875x    ==> 0x440 - 0x47F
    // 4x - 7.9375x     ==> 0x640 - 0x67F
    // 8x - 15.875x     ==> 0x6C0 - 0x6FF
    //
    // Please also note that bits[14:12] of R0x305E are the digital gains.
    // The host can simply write bits[14:12] =1 for unity digital gains to the above table.
    //
    // The minimum recommended gain is R0x305E=0x1060 (1.5x) and
    // the maximum recommended gain is R0x305E=0x16FF (15.875x).
    //

    // mapping of gain model from GAIN register "analogue_gain_code_<col>" to "total gain in float":
    //
    // (1) Gain = analogue_gain_code_<col> / 8
    //
    // (2)
    //   "Total gain" = (8/(8-gain[10:8])) x (gain[7]+1) x gain[6:0] / 64
    //    with max value is 8x2x127/64 = 31.75x
    //
    // The reverse mapping from "total gain in float" to "analogue_gain_code_<col>"
    // is suggested in the table above
    //
    // 1.984375x - 2x exclusive  ==> 0x07F
    // 3.96875x  - 4x exclusive  ==> 0x47F
    // 7.9375x   - 8x exclusive  ==> 0x67F
    //
    // TODO: question: sensor support up to 31.75x gain,
    // but shall we drive it that high is a question.
    // high gain range [16x - 31.75x] range ==> do we use this gain range?
    // Answer is NOT to use this range.

    NvU16 regBit0To6 = 0x0; // one more bit than ov5650
    NvU16 regBit7 = 0x0;
    NvU16 regBit8To10 = 0x0;
    NvU16 regBit12To14 = 0x1; // R0x305E[14:12] is for digital gain, always tie to x1 of digital gain so far.

    x = (x < MinGain) ? MinGain : x;
    x = (x > MaxGain) ? MaxGain : x;

    // If the digital gain is specified and we are crossing
    // Max analog gain for this sensor start using digital
    // gain and re adjust the analog gains.
    if (DigitalGain > 1.f)
    {
        if (x > SENSOR_BAYER_DEFAULT_MAX_ANALOG_GAIN)
        {
            regBit12To14=(NvU16)DigitalGain;
            x /= DigitalGain;
        }
    }
    // for this sensor, gain is up to x15.875, gain min is x1.5
    if (x < 1.5)
    {
        // tie value (including invalid negative input)
        regBit0To6 = 0x60;
    }
    else if (x >= 1.5 && x < 1.984375)
    {
        // map to 0x060 - 0x07F // 0x40 - 0x7F previously used was from datasheet,
        // corrected to 0x60 (to let mininum gain to be x1.5).
        // which has
        //
        // regBit8To10 = 0;
        // regBit7 = 0;
        //
        // equation becomes
        // "Total gain" = gain[6:0] / 64

        regBit0To6 = (NvU16)(x * 64.0);
    }
    else if (x >= 1.984375 && x < 2.0)
    {
        // tie value
        regBit0To6 = 0x7F;
    }
    else if (x >= 2.0 && x < 3.96875)
    {
        // map to 0x440 - 0x47F
        // this has
        regBit8To10 = 4;
        regBit7 = 0;

        // equation becomes
        // "Total gain" = (8/(8-4)) x (1) x gain[6:0] / 64 = gain[6:0]/32;
        regBit0To6 = (NvU16)(x * 32.0);
    }
    else if (x >= 3.96875 && x < 4.0)
    {
        // tie value
        regBit8To10 = 4;
        regBit7 = 0;
        regBit0To6 = 0x7F;
    }
    else if (x >=  4.0 && x < 7.9375)
    {
        // map to 0x640 - 0x67F
        // this has
        regBit8To10 = 6;
        regBit7 = 0;

        // equation becomes
        // "Total gain" = (8/(8-6)) x (1) x gain[6:0] / 64 = gain[6:0]/16;
        regBit0To6 = (NvU16)(x * 16.0);
    }
    else if (x >= 7.9375 && x < 8.0)
    {
        // tie value
        regBit8To10 = 6;
        regBit7 = 0;
        regBit0To6 = 0x7F;
    }
    else if (x >= 8.0 && x < 15.875)
    {
        // map to 0x6C0 - 0x6FF
        // this has
        regBit8To10 = 6;
        regBit7 = 1;

        // equation becomes
        // "Total gain" = (8/(8-6)) x (1+1) x gain[6:0] / 64 = gain[6:0]/8;
        regBit0To6 = (NvU16)(x * 8.0);
    }
    else // if (x>=15.875)
    {
        // tie value (and include invalid value over the bound)
        regBit8To10 = 6;
        regBit7 = 1;
        regBit0To6 = 0x7F;
    }

    NV_DEBUG_PRINTF(("x=%f, MaxGain=%f, regBit12To14=%d, regBit8To10=%d, regBit7=%d, regBit0To6=%d\n",
                     x, MaxGain, regBit12To14, regBit8To10, regBit7, regBit0To6));

    return (regBit12To14 << 12) | (regBit8To10 << 8) | (regBit7 << 7) | (regBit0To6);
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
#define SENSOR_BAYER_CAPABILITIES(id, port) {    \
    id,                                      \
    NvOdmImagerSensorInterface_Serial##port, \
    {                                        \
        NvOdmImagerPixelType_BayerGBRG,      \
    },                                       \
    NvOdmImagerOrientation_0_Degrees,        \
    NvOdmImagerDirection_Away,               \
    6000,                                    \
    { {24000, 134.f/24.f} },                 \
    {                                        \
        NvOdmImagerSyncEdge_Rising,          \
        NvOdmImagerSyncEdge_Rising,          \
        NV_FALSE,                            \
    },                                       \
    {                                        \
        NVODMSENSORMIPITIMINGS_PORT_CSI_##port, \
        NVODMSENSORMIPITIMINGS_DATALANES_TWO,   \
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE, \
        0x1,                                 \
        0x4                                  \
    },                                       \
    { 16, 16 },                              \
    0,                                       \
    FOCUSER_AR0832_GUID,                     \
    TORCH_NVC_GUID,                          \
    NVODM_IMAGER_CAPABILITIES_END            \
}

struct SensorBayerCapTable {
    NvU64                   ImagerGUID;
    NvOdmImagerCapabilities Caps;
};

static struct SensorBayerCapTable g_SensorBayerCaps[] =
{
    {
        SENSOR_BYRRI_AR0832_GUID,
        SENSOR_BAYER_CAPABILITIES("APTINA AR0832", A),
    },
    {
        SENSOR_BYRLE_AR0832_GUID,
        SENSOR_BAYER_CAPABILITIES("APTINA AR0832", B),
    },
    {
        SENSOR_BYRST_AR0832_GUID,
        SENSOR_BAYER_CAPABILITIES("APTINA AR0832_S", B),
    },
};
//In the case imager_hal request capabilities before openning the sensor,
//use this default guid index for return.
#define DEFAULT_SENSOR_GUID_INDEX   0

typedef struct ModeDependentSettingsRec
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 MinFrameLength;
    NvBool SupportsFastMode;
    NvF32 InherentGain;
} ModeDependentSettings;

static ModeDependentSettings ModeDependentSettings_3264x2448 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_FULL, //
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_FULL, // value of above register and minus 1
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_FULL,
    NV_TRUE,
    1.0f
};

static ModeDependentSettings ModeDependentSettings_2880x1620 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_2880x1620,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_2880x1620, //
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_2880x1620, // value of above register and minus 1
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_2880x1620,
    NV_TRUE,
    1.0f
};

static ModeDependentSettings ModeDependentSettings_1920x1080 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_1920x1080,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1920x1080, //
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1920x1080, // value of above register and minus 1
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_1920x1080,
    NV_TRUE,
    1.0f
};

static ModeDependentSettings ModeDependentSettings_1632x1224 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_1632x1224,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_1632x1224, //
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_1632x1224, // value of above register and minus 1
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_1632x1224,
    NV_TRUE,
    1.0f
};

static ModeDependentSettings ModeDependentSettings_800x600 =
{

    SENSOR_BAYER_DEFAULT_LINE_LENGTH_800x600,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_800x600, //
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_800x600, // value of above register and minus 1
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_800x600,
    NV_TRUE,
    1.0f
};

static SensorSetModeSequence *g_pSensorBayerSetModeSequenceList;

/**
 * A list of sensor mode and its SetModeSequence. Phase 0. Sensor Dependent.
 * When SetMode is called, it searches the list for desired resolution and
 * frame rate and write the SetModeSequence through i2c.
 * For Phase 3, more resolutions will be added to this table, allowing
 * for peak performance during Preview, D1 and 720P encode.
 */
static SensorSetModeSequence g_SensorBayerSetModeSequenceList[] =
{
    // WxH           x,y    fps   ar    set mode sequence
    // full res 3264x2448, aspect ratio of full mode = 4:3, can this reach 15fps?
    {{{3264, 2448}, {0, 0}, 15,   1.0,  16.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_FULL}, NULL, &ModeDependentSettings_3264x2448},
    // for 1080P encoding, aspect ratio of this mode = 16:9, no scale desired during encoding, so ar=1.0
    {{{2880, 1620}, {0, 0}, 24,   1.0,  16.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_2880x1620}, NULL, &ModeDependentSettings_2880x1620},
    // for 1080P encoding, aspect ratio of this mode = 16:9, no scale desired during encoding, so ar=1.0
    {{{1920, 1080}, {0, 0}, 30,   1.0,  16.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_1920x1080}, NULL, &ModeDependentSettings_1920x1080},
    // for preview @ 30fps), aspect ratio of this mode = 4:3, no scale desired during encoding, so ar=1.0
    {{{1632, 1224},  {0, 0}, 30,   1.0,  16.0, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_1632x1224}, NULL, &ModeDependentSettings_1632x1224},
    // for preview @ 30fps), aspect ratio of this mode = 4:3, no scale desired during encoding, so ar=1.0
    {{{800,  600},  {0, 0}, 120,   1.0, 16.0, NvOdmImagerPartialCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_800x600}, NULL, &ModeDependentSettings_800x600},
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
    NvU32 ModeIndex;
    NvU32 NumModes;
    NvOdmImagerPowerLevel PowerLevel;

    // Phase 2 variables.
    NvBool SensorInitialized;

    NvU32 SensorInputClockkHz;  // mclk (extclk)

    NvF32 Exposure;
    NvF32 MaxExposure;
    NvF32 MinExposure;

    NvF32 Gains[4];
    NvF32 DigitalGain;
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
    NvOdmImagerCalibrationStatus CalibrationStatus;
    NvOdmImagerTestPattern TestPattern;
#if (BUILD_FOR_AOS == 0)
    // OTP support.
    struct ar0832_otp_data otp_data;
#endif
    char sensor_serial[sizeof_field(NvOdmImagerModuleInfo,SensorSerialNum)];
    NvBool otp_valid;

    // DLI.
    NvU16 *DLIBuf;
    NvOdmOsMutexHandle mutex_id;

#if (BUILD_FOR_AOS == 0)
    AR0832_Stereo_Info *pStereo_Info;
#endif
    NvS32 ActiveGUIDIndex;
} SensorBayerContext;

typedef const char *CString;

/*
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
NvBool SensorBayer_ChooseModeIndex(
    SensorBayerContext *pContext,
    NvSize Resolution,
    NvU32 *pIndex);

#define SERIALNUM_LEN ((8*2)+1)
typedef struct {
    char SerialNum[SERIALNUM_LEN];
} SensorData;

#if (BUILD_FOR_AOS == 0)
static inline NvBool IsRightSensorOn(AR0832_Stereo_Info *pStereo_Info)
{
    return (pStereo_Info->StereoCameraMode == StereoCameraMode_Stereo) ||
        (pStereo_Info->StereoCameraMode == StereoCameraMode_Right) ||
        pStereo_Info->right_on;
}

static inline NvBool IsLeftSensorOn(AR0832_Stereo_Info *pStereo_Info)
{
    return (pStereo_Info->StereoCameraMode == StereoCameraMode_Left) ||
        pStereo_Info->left_on;
}

static int AR0832_Ioctl(
    AR0832_Stereo_Info *pStereo_Info,
    int request,
    void *arg)
{
    AR0832_Driver *pDriver;
    int ret = 0;

    pDriver = &pStereo_Info->right_drv;
    if (pDriver->fd >= 0)
    {
        ret = ioctl(pDriver->fd, request, arg);
        NV_DEBUG_PRINTF(("[%s] %d %d %x\n", __func__, pDriver->fd, request, arg));
    }

    pDriver = &pStereo_Info->left_drv;
    if (pDriver->fd >= 0)
    {
        ret = ioctl(pDriver->fd, request, arg);
        NV_DEBUG_PRINTF(("[%s] %d %d %x\n", __func__, pDriver->fd, request, arg));
    }

    return ret;
}

static int AR0832_device_open(AR0832_Stereo_Info *pStereo_Info)
{
    int Status = AR0832_DRIVER_OPEN_NONE;
    int err;
    AR0832_Driver *pDriver;

    if (IsRightSensorOn(pStereo_Info))
    {
        err = 0;
        pDriver = &pStereo_Info->right_drv;
        NvOdmOsMutexLock(pDriver->fd_mutex);

        if (pDriver->fd >= 0)
        {
            pDriver->open_cnt++;
        }
        else
        {
#ifdef O_CLOEXEC
            pDriver->fd = open("/dev/ar0832-right", O_RDWR|O_CLOEXEC);
#else
            pDriver->fd = open("/dev/ar0832-right", O_RDWR);
#endif
            if (pDriver->fd < 0)
            {
                NvOsDebugPrintf("Can not open right camera device: %s\n", strerror(errno));
                err = -errno;
            }
            else
            {
                pDriver->open_cnt = 1;
            }
        }
        NvOdmOsMutexUnlock(pDriver->fd_mutex);

        if (!err)
        {
            Status |= AR0832_DRIVER_OPEN_RIGHT;
            NV_DEBUG_PRINTF(("Right camera fd opened as: %d * %d\n",
                            pDriver->fd, pDriver->open_cnt));
        }
    }

    if (IsLeftSensorOn(pStereo_Info))
    {
        err = 0;
        pDriver = &pStereo_Info->left_drv;
        NvOdmOsMutexLock(pDriver->fd_mutex);

        if (pDriver->fd >= 0)
        {
            pDriver->open_cnt++;
        }
        else
        {
#ifdef O_CLOEXEC
            pDriver->fd = open("/dev/ar0832-left", O_RDWR|O_CLOEXEC);
#else
            pDriver->fd = open("/dev/ar0832-left", O_RDWR);
#endif
            if (pDriver->fd < 0)
            {
                NvOsDebugPrintf("Can not open left camera device: %s\n", strerror(errno));
                err = -errno;
            }
            else
            {
                pDriver->open_cnt = 1;
            }
        }

        NvOdmOsMutexUnlock(pDriver->fd_mutex);

        if (!err)
        {
            Status |= AR0832_DRIVER_OPEN_LEFT;
            NV_DEBUG_PRINTF(("Left camera fd opened as: %d * %d\n",
                            pDriver->fd, pDriver->open_cnt));
        }
    }

    return Status;
}

static void AR0832_device_close(AR0832_Stereo_Info *pStereo_Info)
{
    AR0832_Driver *pDriver;
    int ret = 0;

    if (IsRightSensorOn(pStereo_Info))
    {
        pDriver = &pStereo_Info->right_drv;
        NvOdmOsMutexLock(pDriver->fd_mutex);

        pDriver->open_cnt--;
        NV_DEBUG_PRINTF(("Right camera close %d\n", pDriver->open_cnt));
        if (pDriver->open_cnt <= 0)
        {
            ret = close(pDriver->fd);
            if (ret)
            {
                pDriver->open_cnt++;
                NvOsDebugPrintf("Right camera fd close failed. %s (%d)\n", strerror(errno), errno);
            }
            else
            {
                pDriver->open_cnt = 0;
                pDriver->fd = -1;
                NV_DEBUG_PRINTF(("Right camera fd closed.\n"));
            }
        }

        NvOdmOsMutexUnlock(pDriver->fd_mutex);
    }

    if (IsLeftSensorOn(pStereo_Info))
    {
        pDriver = &pStereo_Info->left_drv;
        NvOdmOsMutexLock(pDriver->fd_mutex);

        pDriver->open_cnt--;
        NV_DEBUG_PRINTF(("Left camera close %d\n", pDriver->open_cnt));
        if (pDriver->open_cnt <= 0)
        {
            ret = close(pDriver->fd);
            if (ret)
            {
                pDriver->open_cnt++;
                NvOsDebugPrintf("Left camera fd close failed. %s (%d)\n", strerror(errno), errno);
            }
            else
            {
                pDriver->open_cnt = 0;
                pDriver->fd = -1;
                NV_DEBUG_PRINTF(("Left camera fd closed\n"));
            }
        }

        NvOdmOsMutexUnlock(pDriver->fd_mutex);
    }
}

static AR0832_Stereo_Info *Stereo_Info_Create(NvU64 SensorGUID)
{
    AR0832_Stereo_Info *pStereo_Info;

    pStereo_Info = (AR0832_Stereo_Info*)NvOdmOsAlloc(sizeof(AR0832_Stereo_Info));
    if (!pStereo_Info)
        goto init_fail;
    NvOdmOsMemset(pStereo_Info, 0, sizeof(AR0832_Stereo_Info));

    pStereo_Info->left_drv.fd = -1;
    pStereo_Info->left_drv.fd_mutex = NvOdmOsMutexCreate();
    if (!pStereo_Info->left_drv.fd_mutex)
        goto init_fail;

    pStereo_Info->right_drv.fd = -1;
    pStereo_Info->right_drv.fd_mutex = NvOdmOsMutexCreate();
    if (!pStereo_Info->right_drv.fd_mutex)
    {
        NvOdmOsMutexDestroy(pStereo_Info->left_drv.fd_mutex);
        goto init_fail;
    }

    if (SensorGUID == SENSOR_BYRRI_AR0832_GUID)
    {
        pStereo_Info->right_on = NV_TRUE;
        pStereo_Info->left_on = NV_FALSE;
    }
    else if (SensorGUID == SENSOR_BYRLE_AR0832_GUID)
    {
        pStereo_Info->right_on = NV_FALSE;
        pStereo_Info->left_on = NV_TRUE;
    }
    else if (SensorGUID == SENSOR_BYRST_AR0832_GUID)
    {
        pStereo_Info->right_on = NV_TRUE;
        pStereo_Info->left_on = NV_TRUE;
    }
    else
    {
        NvOsDebugPrintf("%s: GUID is unknown.\n", __func__);
        goto init_fail;
    }

    pStereo_Info->drv_open = AR0832_device_open;
    pStereo_Info->drv_close = AR0832_device_close;
    pStereo_Info->drv_ioctl = AR0832_Ioctl;

    return pStereo_Info;

init_fail:
    NvOsDebugPrintf("%s: failed.\n", __func__);
    NvOdmOsFree(pStereo_Info);
    return NULL;
}

static AR0832_Stereo_Info *Stereo_Info_Destroy(AR0832_Stereo_Info *pStereo_Info)
{
    NV_ASSERT((pStereo_Info->left_drv.fd == -1) &&
              (pStereo_Info->right_drv.fd == -1));
    NvOdmOsMutexDestroy(pStereo_Info->left_drv.fd_mutex);
    NvOdmOsMutexDestroy(pStereo_Info->right_drv.fd_mutex);
    NvOdmOsFree(pStereo_Info);
    return NULL;
}

static NvS32 GetSensor_GUIDIndex(NvU64 SensorGUID)
{
    NvU32 idx;

    for (idx = 0; idx < ARRAYSIZE(g_SensorBayerCaps); idx++)
    {
        if ( SensorGUID == g_SensorBayerCaps[idx].ImagerGUID)
            break;
    }

    if (idx >= ARRAYSIZE(g_SensorBayerCaps))
    {
        NvOsDebugPrintf("%s GUID not supported!\n", __func__);
        return -1;
    }

    return idx;
}

static NvBool FetchOTP(SensorBayerContext *pContext)
{
    int ret;
    unsigned i;

    if (pContext->otp_valid)
        return NV_TRUE;

    NvOdmOsMemset(&pContext->otp_data, 0, sizeof(pContext->otp_data));
    ret = pContext->pStereo_Info->drv_ioctl(
            pContext->pStereo_Info, AR0832_IOCTL_GET_OTP, &pContext->otp_data);
    if (ret < 0)
    {
        NvOsDebugPrintf("%s: ioctl to get OTP data failed  %s\n", __FILE__,
        strerror(errno));
        return NV_FALSE;
    }

    pContext->otp_valid = NV_TRUE;

    //
    // Cache the serial# string, as it is used later to see if the calibration
    // file matches the hardware.
    //

    for(i = 0; i < 5; i++)
    {
        snprintf(&(pContext->sensor_serial[i*2]), 3, "%02x", pContext->otp_data.sensor_serial_num[i]);
    }
    return NV_TRUE;
}
#endif

/**
 * SensorBayer_WriteExposure. Phase 2. Sensor Dependent.
 * Calculate and write the values of the sensor registers for the new
 * exposure time.
 * Without this, calibration will not be able to capture images
 * at various brightness levels, and the final product won't be able
 * to compensate for bright or dark scenes.
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
    AR0832_Stereo_Info *pStereo_Info = pContext->pStereo_Info;
    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 ExpTime = *pNewExposureTime;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;

    if (pContext->TestPatternMode)
        return NV_FALSE;

    NV_DEBUG_PRINTF(("new exptime from ISP = %f \n",ExpTime));

    if (*pNewExposureTime > pContext->MaxExposure)
    {

        NV_DEBUG_PRINTF(("new exptime over limit! New = %f out of (%f, %f)\n",
            *pNewExposureTime, pContext->MaxExposure, pContext->MinExposure));
        ExpTime = pContext->MaxExposure;
    }
    if (*pNewExposureTime < pContext->MinExposure)
    {
        NV_DEBUG_PRINTF(("new exptime over limit! New = %f out of (%f, %f)\n",
            *pNewExposureTime, pContext->MaxExposure, pContext->MinExposure));
        ExpTime = pContext->MinExposure;
    }

    // Here, we have to decide if the new exposure time is valid
    // based on the sensor and current sensor settings.
    // Within smaller size mode, 0.23 should be changed to 0.11 if using V-addition calculation
    NewCoarseTime = (NvU32)((Freq * ExpTime) / LineLength + DIFF_INTEGRATION_TIME_OF_MODE);
    if (NewCoarseTime == 0) NewCoarseTime = 1;
    NewFrameLength = NewCoarseTime + SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF;


    NV_DEBUG_PRINTF(("NewCoarseTime = %d, NewFrameLength=%d\n",NewCoarseTime, NewFrameLength));

    // Consider requested max frame rate,
#if 1// REQUESTED MAX FRAME RATE VALUE from ISP is not normal
    if (pContext->RequestedMaxFrameRate > 0.0f) // was 1.0
    {
        NvU32 RequestedMinFrameLengthLines = 0;
        RequestedMinFrameLengthLines =
        (NvU32)(Freq / (LineLength * pContext->RequestedMaxFrameRate));
        NV_DEBUG_PRINTF(("pContext->RequestedMaxFrameRate = %f\n",
                pContext->RequestedMaxFrameRate));
        if (NewFrameLength < RequestedMinFrameLengthLines)
            NewFrameLength = RequestedMinFrameLengthLines;

        NV_DEBUG_PRINTF(("NewCoarseTime = %d, NewFrameLength=%d\n",NewCoarseTime,NewFrameLength));
    }
#endif
    // Clamp to sensor's limit

    if (NewFrameLength > pContext->MaxFrameLength)
        NewFrameLength = pContext->MaxFrameLength;
    else if (NewFrameLength < pContext->MinFrameLength)
        NewFrameLength = pContext->MinFrameLength;

    NV_DEBUG_PRINTF(("NewCoarseTime = %d, NewFrameLength=%d\n",NewCoarseTime, NewFrameLength));

    // return the new frame length
    if (pFrameLength)
    {
        *pFrameLength = NewFrameLength;
    }

    NV_DEBUG_PRINTF(("pContext->FrameLength = %d, NewFrameLength=%d\n",pContext->FrameLength, NewFrameLength));

    if (NewFrameLength != pContext->FrameLength)
    {
        // write new value only when pFrameLength is NULL
        if (!pFrameLength)
        {
            int ret;
            ret = pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_SET_FRAME_LENGTH, (void *)NewFrameLength);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set mode failed %s\n", strerror(errno));
        }

        // Update variables depending on FrameLength.
        // FIXME: If the call above was unsuccessful then
        // we should take care of updating the frameLength
        // with old one.
        pContext->FrameLength = NewFrameLength;
        pContext->FrameRate = (NvF32)pContext->VtPixClkFreqHz /
                              (NvF32)(pContext->FrameLength * pContext->LineLength);

        NV_DEBUG_PRINTF(("Frame rate: %f \n", pContext->FrameRate));
    }
    else
    {
        NV_DEBUG_PRINTF(("Frame length keep as before : %d \n", NewFrameLength));
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
            int ret;
            ret = pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_SET_COARSE_TIME, (void *)NewCoarseTime);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set mode failed %s\n", strerror(errno));
        }

        // Calculate the new exposure based on the sensor and sensor settings.
        // FIXME: if the above call fails. Fix this.
        pContext->Exposure = ((NewCoarseTime -
                               (NvF32)DIFF_INTEGRATION_TIME_OF_MODE) *
                              (NvF32)LineLength) / Freq;
        pContext->CoarseTime = NewCoarseTime;
    }
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
 */
static NvBool
SensorBayer_WriteGains(
    SensorBayerContext *pContext,
    const NvF32 *pGains,
    NvU16 *pGain)
{
#if (BUILD_FOR_AOS == 0)
    AR0832_Stereo_Info *pStereo_Info = pContext->pStereo_Info;
    NvU32 i = 1;
    NvU16 NewGains = 0;

    if (pGains[i] > pContext->MaxGain)
        return NV_FALSE;
    if (pGains[i] < pContext->MinGain)
        return NV_FALSE;

    NewGains = SENSOR_F32_TO_GAIN(pGains[i], pContext->MaxGain, pContext->MinGain, pContext->DigitalGain);

    if (pGain)
    {
        *pGain = NewGains;
    }
    else
    {
        // if this function programs register 0x3056, 0x3058, 0x305a, 0x305c??
        int ret;
        ret = pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_SET_GAIN, (void*)((int)NewGains));
        if (ret < 0)
            NvOsDebugPrintf("ioctl to set gain failed %s\n", strerror(errno));
    }

    NvOdmOsMemcpy(pContext->Gains, pGains, sizeof(NvF32)*4);

    NV_DEBUG_PRINTF(("new gains = %f, %f, %f, %f\n\n", pGains[0],
        pGains[1], pGains[2], pGains[3]));
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

    NV_DEBUG_PRINTF(("SensorBayer_Open ++\n"));

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorBayerContext =
            (SensorBayerContext*)NvOdmOsAlloc(sizeof(SensorBayerContext));
    if (!pSensorBayerContext)
        goto fail;

    NvOdmOsMemset(pSensorBayerContext, 0, sizeof(SensorBayerContext));

#if (BUILD_FOR_AOS == 0)
    if (!hImager->pPrivateContext)
    {
        hImager->pPrivateContext = Stereo_Info_Create(hImager->pSensor->GUID);
        if (!hImager->pPrivateContext)
            goto fail;
    }
    else
    {
        NvOsDebugPrintf("Conflict while alloc stereo info mem!\n");
        goto fail;
    }
    pSensorBayerContext->pStereo_Info = hImager->pPrivateContext;

    {
        NvOdmImagerCapabilities *pCap;
        NvS32 index = GetSensor_GUIDIndex(hImager->pSensor->GUID);

        if (index < 0)
        {
            hImager->pPrivateContext =
                Stereo_Info_Destroy((AR0832_Stereo_Info*)hImager->pPrivateContext);
            goto fail;
        }

        pSensorBayerContext->ActiveGUIDIndex = index;
        pCap = &g_SensorBayerCaps[index].Caps;
        NV_DEBUG_PRINTF(("%s active sensor: %s-%8.8s on %d\n",
                        __func__, pCap->identifier,
                        &hImager->pSensor->GUID,
                        pCap->SensorOdmInterface));

        pSensorBayerContext->SensorInputClockkHz = pCap->ClockProfiles[0].ExternalClockKHz;
    }
#endif

    pSensorBayerContext->mutex_id = NvOdmOsMutexCreate();
    pSensorBayerContext->TestPatternMode = NV_FALSE;
#if (BUILD_FOR_AOS == 0)
    pSensorBayerContext->TestPattern = TEST_PATTERN_NONE;
#endif
    g_pSensorBayerSetModeSequenceList = g_SensorBayerSetModeSequenceList;
    pSensorBayerContext->NumModes =
    NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceList);

    pSensorBayerContext->ModeIndex =
            pSensorBayerContext->NumModes; // invalid mode
    pSensorBayerContext->otp_valid = NV_FALSE;

    pSensorBayerContext->showSkipWriteGains = NV_TRUE;
    pSensorBayerContext->showSkipWriteExposure = NV_TRUE;

    /**
    * Phase 2. Initialize exposure and gains.
    */
    pSensorBayerContext->MaxGain = (SENSOR_BAYER_DEFAULT_MAX_ANALOG_GAIN *
                                                        SENSOR_BAYER_DIGITAL_GAIN);
    pSensorBayerContext->MinGain = SENSOR_BAYER_DEFAULT_MIN_ANALOG_GAIN;
    pSensorBayerContext->DigitalGain = SENSOR_BAYER_DIGITAL_GAIN;

    /**
    * Phase 2 ends here.
    */
    pSensorBayerContext->PowerLevel = NvOdmImagerPowerLevel_Off;
    pSensorBayerContext->CalibrationStatus = NvOdmImagerCal_Undetermined;
    pSensorBayerContext->TestPattern = NvOdmImagerTestPattern_None;

#if (BUILD_FOR_AOS == 0)
    {
        int drv_status = pSensorBayerContext->pStereo_Info->drv_open(
                            pSensorBayerContext->pStereo_Info);
        if (drv_status == AR0832_DRIVER_OPEN_NONE)
        {
            NvOdmOsMutexDestroy(pSensorBayerContext->mutex_id);
            hImager->pPrivateContext =
                Stereo_Info_Destroy((AR0832_Stereo_Info *)hImager->pPrivateContext);
            goto fail;
        }
    }
#endif

    hImager->pSensor->pPrivateContext = pSensorBayerContext;

    return NV_TRUE;

fail:
    NvOdmOsFree(pSensorBayerContext);
    return NV_FALSE;
}


/*
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
    {
        AR0832_Stereo_Info *pStereo_Info = pContext->pStereo_Info;

        if (pStereo_Info &&
            ((pStereo_Info->left_drv.fd != -1) || (pStereo_Info->right_drv.fd != -1))) {
            NV_DEBUG_PRINTF(("Sensor not closed. Closing...\n"));
            pStereo_Info->drv_close(pStereo_Info);
        }
   }
   if (hImager->pPrivateContext)
   {
        hImager->pPrivateContext =
            Stereo_Info_Destroy((AR0832_Stereo_Info*)hImager->pPrivateContext);
   }
#endif
    NvOdmOsMutexDestroy(pContext->mutex_id);
    NvOdmOsFree(pContext);
    hImager->pSensor->pPrivateContext = NULL;
}

/*
 * SensorBayer_GetCapabilities. Phase 1.
 * Returnt sensor bayer's capabilities
 */
static void
SensorBayer_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
#if (BUILD_FOR_AOS == 0)
    NvS32 Index = GetSensor_GUIDIndex(hImager->pSensor->GUID);
#else
    NvS32 Index = DEFAULT_SENSOR_GUID_INDEX;
#endif
    if (Index < 0)
        Index = DEFAULT_SENSOR_GUID_INDEX;

    NvOdmOsMemcpy(pCapabilities, &g_SensorBayerCaps[Index].Caps,
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

    NV_DEBUG_PRINTF(("SensorBayer_ListModes ++\n"));
    if (pNumberOfModes)
    {
        *pNumberOfModes = (NvS32)pContext->NumModes;
        if (pModes)
        {
            // Copy the modes from g_pSensorBayerSetModeSequenceList
            for (i = 0; i < *pNumberOfModes; i++)
            {
                pModes[i] = g_pSensorBayerSetModeSequenceList[i].Mode;

                if (pContext->StereoCameraMode == StereoCameraMode_Stereo)
                        pModes[i].PixelAspectRatio /= 2.0;
            }
        }
    }
}

/*
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
    AR0832_Stereo_Info *pStereo_Info = pContext->pStereo_Info;
    NvU32 Index;
    ModeDependentSettings *pModeSettings;
    NvU32 CoarseTime = 0;
    NvU32 FrameLength = 0;
    NvU16 Gain = 0;
    int ret;
    struct ar0832_mode mode;

    NvOsDebugPrintf("Setting resolution to %dx%d, exposure %f, gains %f\n",
        pParameters->Resolution.width, pParameters->Resolution.height,
        pParameters->Exposure, pParameters->Gains[0]);

    Status = SensorBayer_ChooseModeIndex(pContext,
                pParameters->Resolution, &Index);

    // No match found
    ////if (Index == pContext->NumModes)
    if (!Status)
    {
        NvOsDebugPrintf("-------------// No match found---------------\n");
        return NV_FALSE;
    }
    if (pSelectedMode)
    {
        *pSelectedMode = g_SensorBayerSetModeSequenceList[Index].Mode;
    }

    if (pContext->ModeIndex == Index)
        return NV_TRUE;


    // These hardcoded numbers are from the set mode sequence and this formula
    // is based on this sensor. Sensor Dependent.
    // FIXME: This is copied from OV5650
    if (hImager->pSensor->GUID == SENSOR_BYRST_AR0832_GUID) // Stereo board
    {
        pContext->PllMult = 64;
        pContext->PllPreDiv = 2;
        pContext->PllPosDiv = 1;
        pContext->PllVtPixDiv = 4;
    }
    else
    {
        pContext->PllMult = SENSOR_BAYER_DEFAULT_PLL_MULT;
        pContext->PllPreDiv = SENSOR_BAYER_DEFAULT_PLL_PRE_DIV;
        pContext->PllPosDiv = SENSOR_BAYER_DEFAULT_PLL_POS_DIV;
        pContext->PllVtPixDiv = SENSOR_BAYER_DEFAULT_PLL_VT_PIX_DIV;
    }

    pContext->PllVtSysDiv = SENSOR_BAYER_DEFAULT_PLL_VT_SYS_DIV;

    pContext->VtPixClkFreqHz =
        (NvU32)((pContext->SensorInputClockkHz * 1000 * pContext->PllMult) /
                (pContext->PllPreDiv * pContext->PllPosDiv)/pContext->PllVtPixDiv);
    pModeSettings =
        (ModeDependentSettings*)g_SensorBayerSetModeSequenceList[Index].pModeDependentSettings;

    pContext->LineLength = pModeSettings->LineLength;
    pContext->FrameLength = pModeSettings->FrameLength;
    pContext->CoarseTime = pModeSettings->CoarseTime;
    pContext->MaxFrameLength = SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH;
    pContext->MinFrameLength = pModeSettings->MinFrameLength;
    pContext->InherentGain = pModeSettings->InherentGain;

    pContext->Exposure    =
              (((NvF32)pContext->CoarseTime-DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength) /(NvF32)pContext->VtPixClkFreqHz;
    pContext->MaxExposure =
              (((NvF32)SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE-DIFF_INTEGRATION_TIME_OF_MODE) *
               (NvF32)pContext->LineLength ) / (NvF32)pContext->VtPixClkFreqHz;
    pContext->MinExposure =
              (((NvF32)SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE-DIFF_INTEGRATION_TIME_OF_MODE) *
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

    // default gains set to 1.0
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
            NvOsDebugPrintf("SensorAR0832_WriteExposure failed\n");
            return NV_FALSE;
        }
    }
    else
    {
        FrameLength = pModeSettings->MinFrameLength;
        CoarseTime = pModeSettings->CoarseTime;
    }

    if (pParameters->Gains[0] != 0.0 && pParameters->Gains[1] != 0.0 &&
        pParameters->Gains[2] != 0.0 && pParameters->Gains[3] != 0.0)
    {
        Status = SensorBayer_WriteGains(pContext, pParameters->Gains, &Gain);
        if (!Status)
        {
            NvOsDebugPrintf("SensorAR0832_WriteGains failed\n");
            return NV_FALSE;
        }
    }

    NvOdmOsMemset(&mode, 0, sizeof(struct ar0832_mode));
    mode.xres = g_pSensorBayerSetModeSequenceList[Index].Mode.ActiveDimensions.width;
    mode.yres = g_pSensorBayerSetModeSequenceList[Index].Mode.ActiveDimensions.height;
    mode.frame_length = FrameLength;
    mode.coarse_time = CoarseTime;
    mode.gain = Gain;

    NvOsDebugPrintf("Set Mode : Frame Length %d, Coarsetime %d, gain %d\n",
            mode.frame_length, mode.coarse_time, mode.gain);

    ret = pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_SET_MODE, (void *)&mode);
    if (ret < 0)
    {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__, strerror(errno));
        return NV_FALSE;
    }
    else
    {
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

    if (pResult)
    {
        pResult->Resolution = g_pSensorBayerSetModeSequenceList[Index].Mode.ActiveDimensions;
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

        if (pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_TEST_PATTERN, (void *)(pContext->TestPattern)) < 0)
            return NV_FALSE;

        // TODO: Make this more predictable (ideally, move into driver).
        NvOdmOsWaitUS(350 * 1000);
    }
    return Status;
#else
    return NV_TRUE;
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
    SensorBayerContext *pContext = (SensorBayerContext*)hImager->pSensor->pPrivateContext;
    AR0832_Stereo_Info *pStereo_Info = pContext->pStereo_Info;

    NV_DEBUG_PRINTF(("SensorBayer_SetPowerLevel %d\n", PowerLevel));

    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
        {
            {
                int ret;
                struct ar0832_mode mode;

                NvOdmOsMemset(&mode, 0, sizeof(struct ar0832_mode));
                if (pStereo_Info->StereoCameraMode == StereoCameraMode_Stereo)
                {
                    mode.stereo = 1;
                }
                ret = pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_SET_POWER_ON, &mode);
                if (ret < 0)
                {
                    NvOsDebugPrintf("ioctl to AR0832_IOCTL_SET_POWER_ON failed %s\n", strerror(errno));
                    pStereo_Info->drv_close(pStereo_Info);
                    Status = NV_FALSE;
                }
            }
            break;
        }

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_SyncSensors:
            // nothing to do at this point, update in the future
            /* TO DO */
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
        {
            pStereo_Info->drv_close(pStereo_Info);
            break;
        }

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


/*
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
    SensorBayerContext *pContext =  (SensorBayerContext*)hImager->pSensor->pPrivateContext;
#if (BUILD_FOR_AOS == 0)
    enum ar0832_test_pattern pattern;
    AR0832_Stereo_Info *pStereo_Info = pContext->pStereo_Info;
#endif

    NV_DEBUG_PRINTF(("%s: %d\n", __func__, Param));

    switch(Param)
    {
#if (BUILD_FOR_AOS == 0)
        case NvOdmImagerParameter_TestPattern:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvU32));
            switch (*(NvOdmImagerTestPattern*) pValue)
            {
                case NvOdmImagerTestPattern_None:
                    pattern = TEST_PATTERN_NONE;
                    break;
                case NvOdmImagerTestPattern_Colorbars:
                    pattern = TEST_PATTERN_COLORBARS;
                    break;
                case NvOdmImagerTestPattern_Checkerboard:
                    pattern = TEST_PATTERN_CHECKERBOARD;
                    break;
                case NvOdmImagerTestPattern_Walking1s:
                default:
                    NvOsDebugPrintf("unsupported test pattern\n");
                    Status = NV_FALSE;
                    if (!Status)
                    NvOsDebugPrintf("failed to set test pattern\n");
                    break;
            }
            if (Status)
            {
                if (pStereo_Info->drv_ioctl(pStereo_Info, AR0832_IOCTL_TEST_PATTERN, (void *)pattern) < 0)
                Status = NV_FALSE;
            }
            if (!Status)
            {
                NvOsDebugPrintf("failed to set test pattern\n");
                pContext->TestPatternMode = NV_FALSE;
                pContext->TestPattern = TEST_PATTERN_NONE;
            }
            else
            {
                pContext->TestPatternMode = NV_TRUE;
                pContext->TestPattern = pattern;
            }
            break;
#endif
        case NvOdmImagerParameter_StereoCameraMode:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerStereoCameraMode));

            if (hImager->pSensor->GUID == SENSOR_BYRST_AR0832_GUID)
            {
                pContext->StereoCameraMode = *((NvOdmImagerStereoCameraMode*)pValue);
#if (BUILD_FOR_AOS == 0)
                pStereo_Info->StereoCameraMode = pContext->StereoCameraMode;
                NvOsDebugPrintf("Sensor Stereo Mode = %02x\n", pStereo_Info->StereoCameraMode);
#endif
            }
            else
            {
                Status = NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_SensorExposure:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            Status = SensorBayer_WriteExposure(pContext, (NvF32*)pValue, NULL, NULL);
            break;

        case NvOdmImagerParameter_SensorGain:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 4 * sizeof(NvF32));
            Status = SensorBayer_WriteGains(pContext, pValue, NULL);
            break;

        case NvOdmImagerParameter_OptimizeResolutionChange:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            Status = NV_TRUE;
            break;

        // This is optional but nice to have.
        case NvOdmImagerParameter_SelfTest:
            // Not Implemented.
            break;

        case NvOdmImagerParameter_MaxSensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            pContext->RequestedMaxFrameRate = *((NvF32*)pValue);
            break;

        case NvOdmImagerParameter_TestMode:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvBool));
            pContext->TestPatternMode = *((NvBool *)pValue);

            // If Sensor is initialized then only program the test mode registers
            // else just save the test mode in pContext->TestPatternMode and once
            // the sensor gets initialized in SensorOV5650_SetMode() there itself
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
            break;

        case NvOdmImagerParameter_DLICheck:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(unsigned short *));
            pContext->DLIBuf = (unsigned short*) pValue;
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
    int c, r;
    NvF32 *pFloatValue = pValue;
    SensorBayerContext *pContext =
        (SensorBayerContext*)hImager->pSensor->pPrivateContext;

    NV_DEBUG_PRINTF(("%s: %d\n", __func__, Param));

    switch(Param)
    {
        case NvOdmImagerParameter_DLICheck:
            NvOsDebugPrintf("NvOdImagerParamer_DLICheck\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerDLICheck));
            ((NvOdmImagerDLICheck*)pValue)->correct_pixels = 0;
            ((NvOdmImagerDLICheck*)pValue)->incorrect_pixels = 0;

            if (pContext->DLIBuf == NULL)
            {
                ((NvOdmImagerDLICheck*)pValue)->incorrect_pixels =
                                ~((NvOdmImagerDLICheck*)pValue)->incorrect_pixels;
                return NV_FALSE;
            }
            // FIXME: ugly to use constant
            for (c = 0; c < 2592; c++)
            {
                for (r = 0; r < 1944; r++)
                {
                    if (pContext->DLIBuf[(r*2592)+c] == SENSOR_CHECKERBOARD(r,c))
                        ((NvOdmImagerDLICheck*)pValue)->correct_pixels++;
                    else
                        ((NvOdmImagerDLICheck*)pValue)->incorrect_pixels++;
                }
            }
            break;

        case NvOdmImagerParameter_StereoCapable:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));
            {
                NvBool *pBL = (NvBool*)pValue;
                *pBL = ((hImager->pSensor->GUID == SENSOR_BYRST_AR0832_GUID) ? NV_TRUE : NV_FALSE);
            }
            break;

        case NvOdmImagerParameter_StereoCameraMode:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerStereoCameraMode));
            if (hImager->pSensor->GUID == SENSOR_BYRST_AR0832_GUID)
            {
                NvOdmImagerStereoCameraMode *pMode = (NvOdmImagerStereoCameraMode*)pValue;
                *pMode = pContext->StereoCameraMode;
            }
            else
            {
                Status = NV_FALSE;
            }
            break;

        case NvOdmImagerParameter_SensorType:
            NvOsDebugPrintf("NvOdImagerParamer_SensorType\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerSensorType));
                    *((NvOdmImagerSensorType*)pValue) = NvOdmImager_SensorType_Raw;
            break;

        case NvOdmImagerParameter_CalibrationStatus:
            NvOsDebugPrintf("NvOdImagerParamer_CalibrationStatus\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerCalibrationStatus));
                *((NvOdmImagerCalibrationStatus*)pValue) = pContext->CalibrationStatus;
            break;

        case NvOdmImagerParameter_CalibrationData:
            NvOsDebugPrintf("NvOdImagerParamer_CalibrationData\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerCalibrationData));
            {
                NvOdmImagerCalibrationData *pCalibration = (NvOdmImagerCalibrationData*) pValue;
                NvU16 Sensor_ID;
                int ret = pContext->pStereo_Info->drv_ioctl(
                                pContext->pStereo_Info, AR0832_IOCTL_GET_SENSOR_ID, &Sensor_ID);
                if (ret < 0)
                {
                    NvOsDebugPrintf("ioctl to get sensor id failed %s\n", strerror(errno));
                    Status = NV_FALSE;
                }
                else
                {
                    NV_DEBUG_PRINTF(("Sensor_ID = %04x\n", Sensor_ID));
                    if (Sensor_ID == AR0832_SENSOR_ID_8141) {
                        NvOsDebugPrintf("Sensor is APTINA 8141\n");
                        pCalibration->CalibrationData = pSensorAR0832CalibrationData;
                    }
                    else
                    {
                        NvOsDebugPrintf("Sensor is APTINA 8140\n");
                        pCalibration->CalibrationData = pSensor9E013CalibrationData;
                    }
                    pCalibration->NeedsFreeing = NV_FALSE;
                }
            }
            break;

        case NvOdmImagerParameter_ModuleInfo:
            NvOsDebugPrintf("NvOdImagerParamer_ModuleInfo\n");
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerModuleInfo));
            if (!FetchOTP(pContext))
            {
                NvOsDebugPrintf("FAILED to fetch OTP data\n");
                Status = NV_FALSE;
            }
            else
            {
                unsigned i;
                NvOdmImagerModuleInfo *modinfo = (NvOdmImagerModuleInfo*) pValue;
                NvOdmOsMemset(modinfo, 0, sizeof(NvOdmImagerModuleInfo));

                NvOdmOsMemcpy(modinfo->SensorSerialNum,
                                pContext->sensor_serial,
                                sizeof(pContext->sensor_serial));
                NvOdmOsMemcpy(modinfo->ManufactureID,
                                pContext->otp_data.manufacture_id,
                                sizeof(pContext->otp_data.manufacture_id));
                NvOdmOsMemcpy(modinfo->FactoryID,
                                pContext->otp_data.factory_id,
                                sizeof(pContext->otp_data.factory_id));
                NvOdmOsMemcpy(modinfo->ManufactureDate,
                                pContext->otp_data.manufacture_date,
                                sizeof(pContext->otp_data.manufacture_date));
                NvOdmOsMemcpy(modinfo->ManufactureLine,
                                pContext->otp_data.manufacture_line,
                                sizeof(pContext->otp_data.manufacture_line));
                for (i=0; i<4; i++)
                {
                    snprintf(&(modinfo->ModuleSerialNum[i*2]), 3, "%02x",
                           ((unsigned char *)(&pContext->otp_data.module_serial_num))[i]);
                }
                for (i=0; i<2; i++)
                {
                    snprintf(&(modinfo->FocuserLiftoff[i*2]), 3, "%02x",
                            pContext->otp_data.focuser_liftoff[i]);
                }
                for (i=0; i<2; i++)
                {
                    snprintf(&(modinfo->FocuserMacro[i*2]), 3, "%02x",
                            pContext->otp_data.focuser_macro[i]);
                }
                for (i=0; i<16; i++)
                {
                    snprintf(&(modinfo->ShutterCal[i*2]), 3, "%02x",
                            pContext->otp_data.shutter_cal[i]);
                }
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
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerDeviceStatus));
            {
                NvOdmImagerDeviceStatus *pStatus;
                pStatus = (NvOdmImagerDeviceStatus *)pValue;
                {
                    uint16_t status;
                    int ret;

                    ret = pContext->pStereo_Info->drv_ioctl(
                            pContext->pStereo_Info, AR0832_IOCTL_GET_STATUS, &status);
                    if (ret < 0)
                        NvOsDebugPrintf("ioctl to gets status failed  %s\n", strerror(errno));
                    pStatus->Count = 1;
                }
            }
            break;

        // Phase 1, it can return min = max = 10.0f
        //    (the fps in g_pSensorBayerSetModeSequenceList)
        // Phase 2, return the real numbers
        case NvOdmImagerParameter_SensorFrameRateLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, 2 * sizeof(NvF32));
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

                pContext->CalibrationStatus = NvOdmImagerCal_Override;
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
                ret = pContext->pStereo_Info->drv_ioctl(
                    pContext->pStereo_Info, AR0832_IOCTL_GET_FUSEID, &fuse_id);
                if (ret < 0)
                {
                    NvOsDebugPrintf("ioctl to get sensor data failed %s\n", strerror(errno));
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
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerRegion));

            if (pContext->ModeIndex >= pContext->NumModes)
            {
                return NV_FALSE;
            }
            else
            {
                NvOdmImagerRegion *pRegion = (NvOdmImagerRegion*)pValue;
                pRegion->RegionStart.x = 0;
                pRegion->RegionStart.y = 0;

                // Quater mode
                //if (pContext->ModeIndex == 2)
                if (pContext->ModeIndex > 0) //fixed this value from 1 to 0.
                {
                    pRegion->xScale = 2;//Use the 1/2 scaling down LS
                    pRegion->yScale = 2;
                }
                else // TODO: What is the X/Y scale for 16:9 ???
                {
                    pRegion->xScale = 1;//Use original LS
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
                ModeDependentSettings *pModeSettings =
                (ModeDependentSettings*)
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

        case NvOdmImagerParameter_SensorFrameRateLimitsAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerFrameRateLimitAtResolution));
            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);
            {
                NvOdmImagerFrameRateLimitAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                ModeDependentSettings *pModeSettings = NULL;
                pData = (NvOdmImagerFrameRateLimitAtResolution *)pValue;
                pData->MinFrameRate = 0.0f;
                pData->MaxFrameRate = 0.0f;

                MatchFound = SensorBayer_ChooseModeIndex(pContext, pData->Resolution, &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

                pData->MaxFrameRate = (NvF32)pContext->VtPixClkFreqHz /
                    (NvF32)(pModeSettings->MinFrameLength *
                    pModeSettings->LineLength);
                    pData->MinFrameRate =
                    (NvF32)pContext->VtPixClkFreqHz /
                    (NvF32)(SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH *
                    pModeSettings->LineLength);
               }
            break;

        case NvOdmImagerParameter_SensorInherentGainAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvOdmImagerInherentGainAtResolution));
            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pContext);
            {
                NvOdmImagerInherentGainAtResolution *pData;
                NvU32 Index;
                NvBool MatchFound = NV_FALSE;
                ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerInherentGainAtResolution *)pValue;
                pData->InherentGain = pContext->InherentGain;
                if (pData->Resolution.width == 0 && pData->Resolution.height == 0)
                {
                    break; // They just wanted the current value
                }

                MatchFound =
                    SensorBayer_ChooseModeIndex(pContext, pData->Resolution, &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;
                pData->InherentGain = pModeSettings->InherentGain;
            }
                break;

        case NvOdmImagerParameter_SensorExposureLatchTime:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvU32));
            {
                NvU32 *pUIntValue = (NvU32*)pValue;
                *pUIntValue = 2;
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

        default:
            NvOsDebugPrintf("GetParameter(): %d not supported\n", Param);
            Status = NV_FALSE;
            break;
    }
#endif
    return Status;
}

static NV_INLINE NvBool
SensorBayer_ChooseModeIndex(
    SensorBayerContext *pContext,
    NvSize Resolution,
    NvU32 *pIndex)
{
    NvU32 Index;

    for (Index = 0; Index < pContext->NumModes; Index++)
    {
        if ((Resolution.width ==
             g_SensorBayerSetModeSequenceList[Index].Mode.
             ActiveDimensions.width) &&
            (Resolution.height ==
             g_SensorBayerSetModeSequenceList[Index].Mode.
             ActiveDimensions.height))
        {
            *pIndex = Index;
            return NV_TRUE;
        }
    }

    return NV_FALSE;
}

/*
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

/*
 * SensorBayer_GetHal. Phase 1.
 * return the hal functions associated with sensor bayer
 */
NvBool SensorBayerAR0832_GetHal(NvOdmImagerHandle hImager)
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

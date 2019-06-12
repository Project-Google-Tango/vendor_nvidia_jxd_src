/**
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
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
#include <imx179.h>
#endif

#include "sensor_bayer_imx179.h"
#include "imager_util.h"
#include "nvodm_query_gpio.h"
#include "nvodm_imager.h"
#include "sensor_bayer_imx179_camera_config.h"

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
/*#define SENSOR_BAYER_PLL_MULT_DEFAULT            (0x00A2)
#define SENSOR_BAYER_PLL_PRE_DIV_DEFAULT         (0x0001)
#define SENSOR_BAYER_PLL_POS_DIV_DEFAULT         (0x0006)*/

#define SENSOR_BAYER_PLL_MULT_SPECIAL            (0x00A2)
#define SENSOR_BAYER_PLL_PRE_DIV_SPECIAL         (0x0006)
#define SENSOR_BAYER_PLL_POS_DIV_SPECIAL         (0x0005)


#define SENSOR_BAYER_DEFAULT_MAX_GAIN            (0x00f0)
#define SENSOR_BAYER_DEFAULT_MIN_GAIN             (0x0000)

#define SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF     5

#define SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH  (0xFFFF)

#define SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE (SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE (0x0002)

#define DIFF_INTEGRATION_TIME_OF_MODE (0x01E8)

#define SENSOR_BAYER_EFFECTIVE_PIXEL_LENGTH      3280
#define SENSOR_BAYER_EFFECTIVE_LINE_HEIGHT       2464
#define SENSOR_BAYER_ACTIVE_X0                   0
#define SENSOR_BAYER_ACTIVE_Y0                   0
#define SENSOR_BAYER_ACTIVE_X1                   3279
#define SENSOR_BAYER_ACTIVE_Y1                   2463
#define SENSOR_BAYER_PHYSICAL_X                  4.713f //mm
#define SENSOR_BAYER_PHYSICAL_Y                  3.494f //mm

#define SENSOR_BAYER_DEFAULT_LINE_LENGTH_3280X2464       (0x0D70)
#define SENSOR_BAYER_DEFAULT_FRAME_LENGTH_3280X2464     (0x09D0)
#define SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_3280X2464  (SENSOR_BAYER_DEFAULT_FRAME_LENGTH_3280X2464-SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF)
#define SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_3280X2464  SENSOR_BAYER_DEFAULT_FRAME_LENGTH_3280X2464

#define DEBUG_PRINTS    (0)

// below 2 values are temparary implementation, should be moved to config file later
#define LENS_HYPERFOCAL_DISTANCE (1.0f / 3.63f)
#define LENS_MINIMUM_FOCUS_DISTANCE (1.0f / 0.10f)

#define LENS_FOCAL_LENGTH (4.76f)
#define LENS_HORIZONTAL_VIEW_ANGLE (60.4f)
#define LENS_VERTICAL_VIEW_ANGLE   (60.4f)

#define SENSOR_MIPI_LANE_NUMBER (4)
#define SENSOR_GAIN_TO_F32(x)   (256.f / (256.f - (NvF32)(x)))
#define SENSOR_F32_TO_GAIN(x)   ((NvU32)(256.f - (256.f / (x))))

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides.isp",
};

static const char *pDeviceCalibrationFactoryBlobFile[] =
{
    "/mnt/factory/camera/factory.bin",
};

static const char *pFactoryBlobFileList[] =
{
    "/data/factory.bin",
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
    "IMX179",  // string identifier, used for debugging
    NvOdmImagerSensorInterface_SerialA,
    {
        NvOdmImagerPixelType_BayerRGGB,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
    6000, // initial sensor clock (in khz)
    {{24000, 348.f/24.f}}, // preferred clock profile
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {
        NVODMSENSORMIPITIMINGS_PORT_CSI_A,
        NVODMSENSORMIPITIMINGS_DATALANES_FOUR,
        NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE,
        1, // USE !CONTINUOUS_CLOCK,
        0
    }, // serial settings (CSI)
    { 16, 16 }, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    FOCUSER_AD5823_GUID, // only set if focuser device is available
    TORCH_NVC_GUID, // only set if flash device is available
    NVODM_IMAGER_CAPABILITIES_END,
    0x00, // FlashControlEnabled
    0x00, // AdjustableFlashTiming
    0x00  // isHDR
};

typedef struct SensorBayerIMX179ModeDependentSettingsRec
{
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvU32 MinFrameLength;
    NvF32 InherentGain;
    NvU32 PllMult;
    NvU32 PllPreDiv;
    NvU32 PllPosDiv;
} SensorBayerIMX179ModeDependentSettings;

static SensorBayerIMX179ModeDependentSettings ModeDependentSettings_3280x2464 =
{
    SENSOR_BAYER_DEFAULT_LINE_LENGTH_3280X2464,
    SENSOR_BAYER_DEFAULT_FRAME_LENGTH_3280X2464,
    SENSOR_BAYER_DEFAULT_EXPOSURE_COARSE_3280X2464,
    SENSOR_BAYER_DEFAULT_MIN_FRAME_LENGTH_3280X2464,
    1.0f,
    SENSOR_BAYER_PLL_MULT_SPECIAL,
    SENSOR_BAYER_PLL_PRE_DIV_SPECIAL,
    SENSOR_BAYER_PLL_POS_DIV_SPECIAL,
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
    {{{3280, 2464},  {0, 0}, 30, 1.0, 20, NvOdmImagerNoCropMode, {{0, 0, 0, 0}, {0.f, 0.f}}, NvOdmImagerModeType_Normal, SENSOR_BAYER_DEFAULT_LINE_LENGTH_3280X2464}, NULL, &ModeDependentSettings_3280x2464},
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
    NvU32 VtPixClkFreqHz;

    NvU32 LineLength;
    NvU32 FrameLength;

    NvU32 MaxFrameLength;
    NvU32 MinFrameLength;
    NvBool TestPatternMode;

    NvBool showSkipWriteGains;
    NvBool showSkipWriteExposure;

    NvOdmImagerStereoCameraMode StereoCameraMode;

    NvOdmImagerTestPattern TestPattern;
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
SensorIMX179_UpdateLinesPerSecond(
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
SensorIMX179_ChooseModeIndex(
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
    NvU32 NewFrameLength = 0;

    NV_DEBUG_PRINTF(("%s\n", __func__));
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
    NewCoarseTime = (NvU32)(((Freq * ExpTime - DIFF_INTEGRATION_TIME_OF_MODE) * 2) / LineLength);
    if( NewCoarseTime == 0 ) NewCoarseTime = 1;

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

        NewFrameLength = (NvU32)(pContext->VtPixClkFreqHz * 2/
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
            ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_FRAME_LENGTH,
                        NewFrameLength);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set mode failed %s\n", strerror(errno));
        }
#endif
        // Update variables depending on FrameLength.
        pContext->FrameLength = NewFrameLength;
        pContext->FrameRate = (NvF32)pContext->VtPixClkFreqHz * 2/
                              (NvF32)(pContext->FrameLength * pContext->LineLength);
        SensorIMX179_UpdateLinesPerSecond(pContext);
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

    if (pContext->CoarseTime != NewCoarseTime)
    {
#if (BUILD_FOR_AOS == 0)
        // write new coarse time only when pCoarseTime is NULL
        if (!pCoarseTime)
        {
            int ret;
            NewCoarseTime = NewCoarseTime;
            ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_COARSE_TIME,
                        NewCoarseTime);
            if (ret < 0)
                NvOsDebugPrintf("ioctl to set coarse time failed %s\n", strerror(errno));
        }
#endif
        // Calculate the new exposure based on the sensor and sensor settings.
        pContext->Exposure =
            ((NvF32)NewCoarseTime * (NvF32)pContext->LineLength + DIFF_INTEGRATION_TIME_OF_MODE * 2)
              / ((NvF32)pContext->VtPixClkFreqHz * 2);
        pContext->CoarseTime = NewCoarseTime;
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

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (pGains[i] > pContext->MaxGain || pGains[i] < pContext->MinGain)
    {
        NvOsDebugPrintf("Err:imx179 odm:%s:gain %f is out of range (%f, %f)\n",
                         __func__, pGains[i], pContext->MinGain, pContext->MaxGain);
        return NV_FALSE;
    }

    // prepare and write register
    NewGains = (NvU16)SENSOR_F32_TO_GAIN(pGains[i]);

    if (pGain)
    {
        *pGain = NewGains;
    }
    else
    {
#if (BUILD_FOR_AOS == 0)
        int ret;
        ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_GAIN, NewGains);
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
#if (BUILD_FOR_AOS == 0)
    int ret;
    struct imx179_ae ae;
    NvU32 i = 1;

    NvF32 Freq = (NvF32)pContext->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pContext->LineLength;
    NvF32 ExpTime = sensorAE->ET;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;

    NvOdmOsMemset(&ae, 0, sizeof(struct imx179_ae));

    if (!sensorAE->ET_enable)
    {
        ExpTime = pContext->Exposure;
        sensorAE->ET_enable = NV_TRUE;
    }

    if (sensorAE->gains_enable==NV_TRUE) {
        if (sensorAE->gains[i] > pContext->MaxGain)
            return NV_FALSE;
        if (sensorAE->gains[i] < pContext->MinGain)
            return NV_FALSE;

        // prepare and write register 0x350b
        ae.gain = SENSOR_F32_TO_GAIN(sensorAE->gains[i]);
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
        NewCoarseTime = (NvU32)(((Freq * ExpTime - DIFF_INTEGRATION_TIME_OF_MODE) * 2) / LineLength);
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

            NewFrameLength = (NvU32)(pContext->VtPixClkFreqHz * 2/
                                (NvF32)(pContext->FrameRate* pContext->LineLength));
        }

        if (pContext->RequestedMaxFrameRate > 0.0f)
        {
            NvU32 RequestedMinFrameLengthLines = 0;
            RequestedMinFrameLengthLines =
               (NvU32)((Freq * 2) / (LineLength * pContext->RequestedMaxFrameRate));
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
            SensorIMX179_UpdateLinesPerSecond(pContext);
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
            ae.coarse_time_enable = NV_TRUE;
            ae.coarse_time = NewCoarseTime;

            // Calculate the new exposure based on the sensor and sensor settings.
            pContext->Exposure    =
              ((NvF32)NewCoarseTime * (NvF32)pContext->LineLength + DIFF_INTEGRATION_TIME_OF_MODE * 2)
               / ((NvF32)pContext->VtPixClkFreqHz * 2);

            pContext->CoarseTime = NewCoarseTime;
        }
    }

    if (ae.gain_enable==NV_TRUE || ae.coarse_time_enable==NV_TRUE ||
            ae.frame_length_enable==NV_TRUE) {
        ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_GROUP_HOLD, &ae);
        if (ret < 0)
        {
            NvOsDebugPrintf("ioctl to set group hold failed %s\n", strerror(errno));
            return NV_FALSE;
        }
    }
#endif
    return NV_TRUE;
}

static NvBool
NvOdmImager_TurnOnFlash(
    SensorBayerContext *pContext,
    NvOdmImagerSensorFlashControl *pmode)
{
#if (BUILD_FOR_AOS == 0)
    struct imx179_flash_control fc;
    int ret;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (pmode->FlashEnable)
    {
        fc.enable = 1;
        if (pmode->FlashPulseEnable)
        {
            fc.edge_trig_en = 1;
            if (pmode->PulseAtEndofFrame)
            {
                fc.start_edge = 1;
            }
            if (pmode->PulseRepeats)
            {
                fc.repeat = 1;
            }
            fc.delay_frm = pmode->PulseDelayFrames & 0x03;
        }
    }
    ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_FLASH_MODE, &fc);
    if (ret < 0)
    {
        NvOsDebugPrintf("ioctl to turn on/off flash failed: %s\n", strerror(errno));
        return NV_FALSE;
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
    int ret;

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorBayerContext =
        (SensorBayerContext*)NvOdmOsAlloc(sizeof(SensorBayerContext));
    if (!pSensorBayerContext)
        goto fail;

    NvOdmOsMemset(pSensorBayerContext, 0, sizeof(SensorBayerContext));

#if (BUILD_FOR_AOS == 0)
    pSensorBayerContext->camera_fd = open("/dev/imx179", O_RDWR);
    if (pSensorBayerContext->camera_fd < 0)
    {
        NvOsDebugPrintf("IMX179 ****  Can not open camera device: %s\n",
            strerror(errno));
        goto fail;
    }
    else
    {
        NV_DEBUG_PRINTF(("Camera fd open as: %d\n", pSensorBayerContext->camera_fd));
    }

    ret = ioctl(pSensorBayerContext->camera_fd, IMX179_IOCTL_GET_FLASH_CAP, 0);
    if (ret < 0)
    {
        NV_DEBUG_PRINTF(("On-sensor flash not supported.\n"));
    }
    else
    {
        g_SensorBayerCaps.FlashControlEnabled = 1;
        g_SensorBayerCaps.AdjustableFlashTiming = 3;
        NV_DEBUG_PRINTF(("On-sensor flash supported.\n"));
    }
#endif
    pSensorBayerContext->TestPatternMode = NV_FALSE;
    pSensorBayerContext->TestPattern = NvOdmImagerTestPattern_None;

    g_pSensorBayerSetModeSequenceList = g_SensorBayerSetModeSequenceList;
    pSensorBayerContext->NumModes =
        NV_ARRAY_SIZE(g_SensorBayerSetModeSequenceList);

    pSensorBayerContext->ModeIndex =
        pSensorBayerContext->NumModes; // invalid mode

    pSensorBayerContext->showSkipWriteGains = NV_TRUE;
    pSensorBayerContext->showSkipWriteExposure = NV_TRUE;

    /**
     * Phase 2. Initialize exposure and gains.
     */
    pSensorBayerContext->Exposure = -1.0; // invalid exposure

     // no need to convert gain's min/max to float for IMX179, because they are defined as float already.
    pSensorBayerContext->MaxGain = SENSOR_GAIN_TO_F32(SENSOR_BAYER_DEFAULT_MAX_GAIN);
    pSensorBayerContext->MinGain = SENSOR_GAIN_TO_F32(SENSOR_BAYER_DEFAULT_MIN_GAIN);
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

    NV_DEBUG_PRINTF(("%s\n", __func__));
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
    // Copy the sensor caps from g_SensorBayerCaps
    NvOdmOsMemcpy(pCapabilities, &g_SensorBayerCaps,
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

    NV_DEBUG_PRINTF(("%s %p %p num %d\n", __func__, pNumberOfModes, pModes, pContext->NumModes));
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
    SensorBayerIMX179ModeDependentSettings *pModeSettings;

    NvU32 CoarseTime = 0;
    NvU32 FrameLength = 0;
    NvU16 Gain = 0;

    NV_DEBUG_PRINTF(("Setting resolution to %dx%d, exposure %f, gains %f\n",
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
        (SensorBayerIMX179ModeDependentSettings*)
        g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

    // These hardcoded numbers are from the set mode sequence and this formula
    // is based on this sensor. Sensor Dependent.
    //
    pContext->PllMult = pModeSettings->PllMult;
    pContext->PllPreDiv = pModeSettings->PllPreDiv;
    pContext->PllPosDiv = pModeSettings->PllPosDiv;

    pContext->VtPixClkFreqHz =
        (NvU32)(pContext->SensorInputClockkHz *  pContext->PllMult /
                (pContext->PllPreDiv * pContext->PllPosDiv)) * 1000;

    // set initial line lenth, frame length, coarse time, and max/min of frame length
    pContext->LineLength = pModeSettings->LineLength;
    pContext->FrameLength = pModeSettings->FrameLength;
    pContext->CoarseTime = pModeSettings->CoarseTime;
    pContext->MaxFrameLength = SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH;
    pContext->MinFrameLength = pModeSettings->MinFrameLength;
    pContext->InherentGain = pModeSettings->InherentGain;

    // default to use imager scheme
    pContext->FrameRateScheme = NvOdmImagerFrameRateScheme_Imager;

    pContext->Exposure    =
              ((NvF32)pContext->CoarseTime * (NvF32)pContext->LineLength + DIFF_INTEGRATION_TIME_OF_MODE * 2)
               / ((NvF32)pContext->VtPixClkFreqHz * 2);
    pContext->MaxExposure =
              ((NvF32)SENSOR_BAYER_DEFAULT_MAX_EXPOSURE_COARSE * (NvF32)pContext->LineLength + DIFF_INTEGRATION_TIME_OF_MODE * 2)
               / ((NvF32)pContext->VtPixClkFreqHz * 2);
    pContext->MinExposure =
              ((NvF32)SENSOR_BAYER_DEFAULT_MIN_EXPOSURE_COARSE * (NvF32)pContext->LineLength + DIFF_INTEGRATION_TIME_OF_MODE * 2)
               / ((NvF32)pContext->VtPixClkFreqHz * 2);

    if (!pContext->SensorInitialized) {
        pContext->FrameRate =
              (NvF32)pContext->VtPixClkFreqHz * 2/
              (NvF32)(pContext->FrameLength * pContext->LineLength);
    }
    pContext->MaxFrameRate =
              (NvF32)pContext->VtPixClkFreqHz * 2/
              (NvF32)(pContext->MinFrameLength * pContext->LineLength);
    pContext->MinFrameRate =
              (NvF32)pContext->VtPixClkFreqHz * 2/
              (NvF32)(SENSOR_BAYER_DEFAULT_MAX_FRAME_LENGTH * pContext->LineLength);


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
            NvOsDebugPrintf("SensorIMX179_WriteExposure failed\n");
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
            NvOsDebugPrintf("SensorIMX179_WriteGains failed\n");
        }
    }

#if (BUILD_FOR_AOS == 0)
    int ret;
    struct imx179_mode mode = {
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.height,
            FrameLength, CoarseTime, Gain
    };
    ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_MODE, &mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__,
            strerror(errno));
        Status = NV_FALSE;
    } else {
        Status = NV_TRUE;
    }
#endif

    // At this stage if Status is false, we must return.
    if (!Status)
        return NV_FALSE;

    /**
     * the following is Phase 2.
     */
    pContext->SensorInitialized = NV_TRUE;

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
    pContext->RegionReadOutTime = height * pModeSettings->LineLength / (pContext->VtPixClkFreqHz * 2);
    NV_DEBUG_PRINTF(("RegionReadOutTime: %f x %d / %d = %f\n",
                    height, pModeSettings->LineLength, pContext->VtPixClkFreqHz, pContext->RegionReadOutTime));
    SensorIMX179_UpdateLinesPerSecond(pContext);

    // Wait one exposure time for gains/exposure to take effect.
    NvOsSleepMS((NvU32)(pContext->Exposure * 1000.0));
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

    NV_DEBUG_PRINTF(("%s %d\n", __func__, PowerLevel));

    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;
#if (BUILD_FOR_AOS == 0)
    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
            ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_POWER, 1);
            if (ret < 0) {
                NvOsDebugPrintf("ioctl(%d) to set power failed %s\n",
                        pContext->camera_fd, strerror(errno));
                Status = NV_FALSE;
            }
            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
            ret = ioctl(pContext->camera_fd, IMX179_IOCTL_SET_POWER, 0);
            if (ret < 0) {
                NvOsDebugPrintf("ioctl to clr power failed %s\n", strerror(errno));
                Status = NV_FALSE;
            }
            break;

        default:
            NV_ASSERT("!Unknown power level\n");
            Status = NV_FALSE;
            break;
    }
#endif
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
    NV_DEBUG_PRINTF(("%s %x\n", __func__, Param));
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

        case NvOdmImagerParameter_SensorFlashSet:
            Status = NvOdmImager_TurnOnFlash(pContext, (NvOdmImagerSensorFlashControl *)pValue);
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_SensorFlashSet %d\n", __func__, Status));
            break;

        default:
            NV_DEBUG_PRINTF(("[IMX179]:SetParameter(): %d not supported\n", Param));
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
    NV_DEBUG_PRINTF(("%s %x\n", __func__, Param));
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
                    NvU16 status;
                    int ret;
                    SetModeParameters Parameters;
#if (BUILD_FOR_AOS == 0)
                    ret = ioctl(pContext->camera_fd, IMX179_IOCTL_GET_STATUS, &status);
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
                        NvOsDebugPrintf("Bad sensor state, reset and reconfigure"
                            "%s\n", strerror(status));
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
                    LoadOverridesFile(pOverrideFiles,
                        (sizeof(pOverrideFiles)/sizeof(pOverrideFiles[0])));
                pCalibration->NeedsFreeing = (pCalibration->CalibrationData != NULL);
                Status = pCalibration->NeedsFreeing;
            }
            break;

        // Get the module calibration data stored in OTP (typically sensor's OTP).
        case NvOdmImagerParameter_ModuleCalibrationData_OTP:
            {
                Status = NV_FALSE;
            }
            break;

        // Get the module calibration data stored in EEPROM (typically inside of camera module).
        case NvOdmImagerParameter_ModuleCalibrationData_EEPROM:
            {
                Status = LoadFactoryFromEEPROM(hImager, pContext->camera_fd, (NvU32) NVC_IOCTL_GET_EEPROM_DATA, pValue);
            }
            break;

        // Get the device calibration data stored in /mnt/factory/camera/factory.bin
        case NvOdmImagerParameter_DeviceCalibrationData:
            {
                Status = LoadBlobFile(pDeviceCalibrationFactoryBlobFile,
                                    sizeof(pDeviceCalibrationFactoryBlobFile)/sizeof(pDeviceCalibrationFactoryBlobFile[0]),
                                    pValue, SizeOfValue);
            }
            break;

        // Get the factory calibration data from a factory file.
        case NvOdmImagerParameter_FactoryCalibrationData:
            {
                Status = LoadBlobFile(pFactoryBlobFileList,
                                    sizeof(pFactoryBlobFileList)/sizeof(pFactoryBlobFileList[0]),
                                    pValue, SizeOfValue);
            }
            break;

        case NvOdmImagerParameter_FuseID:
#if (BUILD_FOR_AOS == 0)
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(struct imx179_sensordata));

            // code to read the ID from sensor
            struct imx179_sensordata sensor_data;
            int ret, num;
            NvOdmImagerPowerLevel PreviousPowerLevel;
            SensorBayer_GetPowerLevel(hImager, &PreviousPowerLevel);
            if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
            {
                ret = SensorBayer_SetPowerLevel(hImager, NvOdmImagerPowerLevel_On);
                if (!ret)
                {
                    NvOsDebugPrintf("failed to set power to ON.  Aborting fuse ID read.\n");
                    Status = NV_FALSE;
                    break;
                }
            }

            ret = ioctl(pContext->camera_fd, IMX179_IOCTL_GET_SENSORDATA, &sensor_data);
            if (ret < 0)
            {
                NvOsDebugPrintf("IMX179: ioctl to get sensor data failed %s\n", strerror(errno));
                Status = NV_FALSE;
            }
            else
            {
                NvOdmOsMemset(pValue, 0, SizeOfValue);
                num = SizeOfValue > sensor_data.fuse_id_size ?
                                    sensor_data.fuse_id_size : SizeOfValue;
                NvOdmOsMemcpy(pValue, &sensor_data, sizeof(struct imx179_sensordata));
                Status = NV_TRUE;
            }

            if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
            {
                ret = SensorBayer_SetPowerLevel(hImager, PreviousPowerLevel);
                if (!ret)
                    NvOsDebugPrintf("failed to set power to previous state after fuse id read\n");
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
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32 *)pValue = pContext->LinesPerSecond;
            if (!pContext->LinesPerSecond)
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
                SensorBayerIMX179ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerFrameRateLimitAtResolution *)pValue;
                pData->MinFrameRate = 0.0f;
                pData->MaxFrameRate = 0.0f;

                MatchFound =
                    SensorIMX179_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerIMX179ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;


                pData->MaxFrameRate = (NvF32)pContext->VtPixClkFreqHz * 2/
                                      (NvF32)(pModeSettings->FrameLength *
                                              pModeSettings->LineLength);
                pData->MinFrameRate =
                    (NvF32)pContext->VtPixClkFreqHz * 2/
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
                SensorBayerIMX179ModeDependentSettings *pModeSettings = NULL;

                pData = (NvOdmImagerInherentGainAtResolution *)pValue;
                pData->InherentGain = pContext->InherentGain;

                if (pData->Resolution.width == 0 && pData->Resolution.height == 0)
                    break; // They just wanted the current value

                MatchFound =
                    SensorIMX179_ChooseModeIndex(pContext, pData->Resolution,
                    &Index);

                if (!MatchFound)
                {
                    Status = NV_FALSE;
                    break;
                }

                pModeSettings = (SensorBayerIMX179ModeDependentSettings*)
                    g_pSensorBayerSetModeSequenceList[Index].pModeDependentSettings;

                pData->InherentGain = pModeSettings->InherentGain;
            }
            break;

        case NvOdmImagerParameter_SensorExposureLatchTime:
            break;

        case NvOdmImagerParameter_SensorActiveRegionReadOutTime:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue, sizeof(NvF32));
            *(NvF32 *)pValue = pContext->RegionReadOutTime;
            if (!pContext->RegionReadOutTime)
            {
                Status = NV_FALSE;
            }
            break;

        // Phase 3.
        default:
            NV_DEBUG_PRINTF(("[IMX179]:GetParameter(): %d not supported\n", Param));
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
    NV_DEBUG_PRINTF(("%s %d\n", __func__, *pPowerLevel));
}

static NvBool SensorBayer_GetStaticProperties(
    NvOdmImagerHandle hImager,
    SensorStaticProperties *pProperties)
{
    SensorBayerIMX179ModeDependentSettings *pModeSettings;
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
        pModeSettings = (SensorBayerIMX179ModeDependentSettings*)
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
        pModeProperty->PixClkAdjustment = 1.f;
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
NvBool SensorBayerIMX179_GetHal(NvOdmImagerHandle hImager)
{
    NV_DEBUG_PRINTF(("%s\n", __func__));
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

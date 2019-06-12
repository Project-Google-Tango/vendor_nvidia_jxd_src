/*
 * Copyright (c) 2008-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define DEBUG_PRINTS 0

#include "nvassert.h"
#include "imager_util.h"
#include "sensor_yuv_soc380.h"

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
#include <soc380.h>
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

#define LENS_HORIZONTAL_VIEW_ANGLE (60.0f)  //These parameters are set to some value in order to pass CTS
#define LENS_VERTICAL_VIEW_ANGLE   (60.0f)  //We'll have to replace them with appropriate values if we get these sensor specs from some source.
#define LENS_FOCAL_LENGTH          (10.0f)

static NvOdmImagerCapabilities g_SensorYuvCaps =
{
    "soc380",  // string identifier, used for debugging
    NvOdmImagerSensorInterface_Parallel8, // interconnect type
    {
        NvOdmImagerPixelType_UYVY,
    },
    NvOdmImagerOrientation_0_Degrees, // rotation with respect to main display
    NvOdmImagerDirection_Away,          // toward or away the main display
#if (BUILD_FOR_ARUBA==1)
    13000, // initial sensor clock (in khz)
    { {13000, 13.f/13.f}}, // preferred clock profile
#else
    6000, // initial sensor clock (in khz)
    { {24000, 130.f/24.f}, {0,0} }, // preferred clock profile
#endif
    {
        NvOdmImagerSyncEdge_Rising, // hsync polarity
        NvOdmImagerSyncEdge_Rising, // vsync polarity
        NV_FALSE,                   // uses mclk on vgp0
    }, // parallel settings (VIP)
    {0,0,0,NV_FALSE,0}, // serial settings (CSI) (not used)
    {0, 0}, // min blank time, shouldn't need adjustment
    0, // preferred mode, which resolution to use on startup
    0, // FOCUSER_GUID, only set if focuser code available
    0, // FLASH_GUID, only set if flash device code is available
    NVODM_IMAGER_CAPABILITIES_END
};

static SensorSetModeSequence *g_pSensorBayerSetModeSequenceList;
static NvBool IspSupport = NV_FALSE;

/**
 * A list of sensor mode and its SetModeSequence. Phase 0. Sensor Dependent.
 * When SetMode is called, it searches the list for desired resolution and
 * frame rate and write the SetModeSequence through i2c.
 */
static SensorSetModeSequence g_SensorYuvSetModeSequenceList[] =
{
     // WxH        x, y    fps   set mode sequence
    {{{640, 480}, {0, 0},  30, 1.0, 20}, NULL, NULL},
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

    if (!hImager || !hImager->pSensor)
        return NV_FALSE;

    pSensorYuvContext =
        (SensorYuvContext*)NvOdmOsAlloc(sizeof(SensorYuvContext));
    if (!pSensorYuvContext)
        goto fail;

    NvOdmOsMemset(pSensorYuvContext, 0, sizeof(SensorYuvContext));

    g_pSensorBayerSetModeSequenceList = g_SensorYuvSetModeSequenceList;
    pSensorYuvContext->NumModes =
        NV_ARRAY_SIZE(g_SensorYuvSetModeSequenceList);
    pSensorYuvContext->ModeIndex =
        pSensorYuvContext->NumModes + 1; // invalid mode

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

#if DEBUG_PRINTS
    NvOdmOsDebugPrintf("Setting resolution to %dx%d\n",
        pParameters->Resolution.width, pParameters->Resolution.height);
#endif

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
        return NV_TRUE;

    int ret;
    struct soc380_mode mode = {
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.width,
        g_pSensorBayerSetModeSequenceList[Index].Mode.
            ActiveDimensions.height
    };
    ret = ioctl(pContext->camera_fd, SOC380_IOCTL_SET_MODE, &mode);
    if (ret < 0) {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n", __func__,
            strerror(errno));
        return NV_FALSE;
    } else {
        Status = NV_TRUE;
    }

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

    NvOsDebugPrintf("SensorYuv_SetPowerLevel %d\n", PowerLevel);
    if (pContext->PowerLevel == PowerLevel)
        return NV_TRUE;

    switch(PowerLevel)
    {
        case NvOdmImagerPowerLevel_On:
#ifdef O_CLOEXEC
            pContext->camera_fd = open("/dev/soc380", O_RDWR|O_CLOEXEC);
#else
            pContext->camera_fd = open("/dev/soc380", O_RDWR);
#endif
            if (pContext->camera_fd < 0) {
                NvOsDebugPrintf("Can not open camera device: %s\n",
                strerror(errno));
                Status = NV_FALSE;
            } else {
                NvOsDebugPrintf("SOC380 Camera fd open as: %d\n", pContext->camera_fd);
                Status = NV_TRUE;
            }
            if (!Status)
            {
                NvOdmOsDebugPrintf("NvOdmImagerPowerLevel_On failed.\n");
                return NV_FALSE;
            }

            break;

        case NvOdmImagerPowerLevel_Standby:
            Status = NV_TRUE;
            break;

        case NvOdmImagerPowerLevel_Off:
            close(pContext->camera_fd);
            pContext->camera_fd = -1;
            // wait for the standby 0.1 sec should be enough
            NvOdmOsWaitUS(100000);
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

#if DEBUG_PRINTS
    NvOsDebugPrintf("SetParameter(): %x\n", Param);
#endif

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

        case NvOdmImagerParameter_SelfTest:
        default:
            NvOsDebugPrintf("SetParameter(): %d not supported\n", Param);
            break;
    }


    return Status;
}

static int
SensorYuv_GetStatus(SensorYuvContext *pContext, NvOdmImagerDeviceStatus *pStatus)
{
#if (BUILD_FOR_AOS == 0)
    struct soc380_status dev_status;
    int ret;

    // Pick your own useful registers to use for debug.
    // If the camera hangs, these register values are printed
    // Sensor Dependent.
    dev_status.data = 0x0;
    dev_status.status = 0x0;
    ret = ioctl(pContext->camera_fd, SOC380_IOCTL_GET_STATUS, &dev_status);
    if (ret)
        return ret;
    pStatus->Values[0] = dev_status.status;

    dev_status.data = 0x2104;
    dev_status.status = 0x0;
    ret = ioctl(pContext->camera_fd, SOC380_IOCTL_GET_STATUS, &dev_status);
    if (ret)
        return ret;
    pStatus->Values[1] = dev_status.status;

    dev_status.data = 0x2703;
    dev_status.status = 0x0;
    ret = ioctl(pContext->camera_fd, SOC380_IOCTL_GET_STATUS, &dev_status);
    if (ret)
        return ret;
    pStatus->Values[2] = dev_status.status;

    dev_status.data = 0x2705;
    dev_status.status = 0x0;
    ret = ioctl(pContext->camera_fd, SOC380_IOCTL_GET_STATUS, &dev_status);
    if (ret)
        return ret;
    pStatus->Values[3] = dev_status.status;

    dev_status.data = 0x2737;
    dev_status.status = 0x0;
    ret = ioctl(pContext->camera_fd, SOC380_IOCTL_GET_STATUS, &dev_status);
    if (ret)
        return ret;
    pStatus->Values[4] = dev_status.status;

    pStatus->Count = 5;
#endif
    return 0;
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
#if (BUILD_FOR_AOS == 0)
    SensorYuvContext *pContext =
        (SensorYuvContext*)hImager->pSensor->pPrivateContext;

#if DEBUG_PRINTS
    NvOsDebugPrintf("GetParameter(): %d\n", Param);
#endif

    switch(Param)
    {
        case NvOdmImagerParameter_DeviceStatus:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvOdmImagerDeviceStatus));
            {
                NvOdmImagerDeviceStatus *pStatus;
                int ret;
                pStatus = (NvOdmImagerDeviceStatus *)pValue;
                ret = SensorYuv_GetStatus(pContext, pStatus);
                if (ret)
                    return NV_FALSE;
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

        case NvOdmImagerParameter_SensorIspSupport:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                sizeof(NvBool));

            if (pValue)
            {
                *(NvBool*)pValue = IspSupport;
            }
            else
            {
                NvOsDebugPrintf("cannot store value in NULL pointer for"\
                    "NvOdmImagerParameter_SensorIspSupport in %s:%d\n",__FILE__,__LINE__);
                return NV_FALSE;
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

        default:
            return NV_FALSE;
    }
#endif
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

/**
 * SensorYuv_GetHal. Phase 1.
 * return the hal functions associated with sensor yuv
 */
NvBool SensorYuvSOC380_GetHal(NvOdmImagerHandle hImager)
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

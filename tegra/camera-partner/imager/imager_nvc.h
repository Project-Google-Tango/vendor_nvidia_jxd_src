/**
 * Copyright (c) 2008-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVC_SENSOR_BAYER_H
#define NVC_SENSOR_BAYER_H

#include "imager_hal.h"

/* NvcParamRet notes:
 * Since the imager_nvc is a generic user driver for NVC kernel drivers, there
 * may be times when specific actions are needed for a specific device.  To
 * accomodate this scenario, the generic imager_nvc will call the device
 * specific files as part of processing a function.  The NvcImager_SetParameter
 * and NvcImager_GetParameter functions are two such functions that if have
 * registered device specific functions, will call these functions first.
 * The device specific funtions will return with one of the NvcParamRet*
 * defines below to tell imager_nvc how to continue.
 * NvcParamRetExitFalse: This tells imager_nvc to exit with NV_FALSE.
 *                       This would be used if there was a parameter match and
 *                       the parameter code failed.
 * NvcParamRetExitTrue: This tells imager_nvc to exit with NV_TRUE.
 *                      This would be used if there was a parameter match and
 *                      the parameter code succeeded.
 * NvcParamRetContinueFalse: This tells imager_nvc to continue processing the
 *                           parameter by looking for a match.
 *                           This is used when there was no parameter match.
 * NvcParamRetContinueTrue: This tells imager_nvc to continue processing the
 *                          parameter after a match.
 *                          This is used when there was a parameter match but
 *                          there is more actions needed by imager_nvc.  For
 *                          example, more action may be needed for a
 *                          parameter read: match done that sets up the IOCTL,
 *                          function returns to continue to execute the IOCTL.
 */
enum nvc_params_ret {
    NvcParamRetExitFalse = 0,
    NvcParamRetExitTrue,
    NvcParamRetContinueFalse,
    NvcParamRetContinueTrue,
    NvcParamRetForce32 = 0x7FFFFFFF
};

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvcImagerDynamicNvcRec
{
    NvU32 ApiVersion;
    NvS32 RegionStartX;
    NvS32 RegionStartY;
    NvU32 xScale;
    NvU32 yScale;
    NvU32 BracketCaps;
    NvU32 FlushCount;
    NvU32 InitialIntraFrameSkip;
    NvU32 SteadyStateIntraFrameSkip;
    NvU32 SteadyStateFrameNumber;
    NvU32 CoarseTime;
    NvU32 MaxCoarseDiff;
    NvU32 MinExposureCourse;
    NvU32 MaxExposureCourse;
    NvF32 DiffIntegrationTimeOfMode;
    NvU32 LineLength;
    NvU32 FrameLength;
    NvU32 MinFrameLength;
    NvU32 MaxFrameLength;
    NvF32 MinGain;
    NvF32 MaxGain;
    NvF32 InherentGain;
    NvF32 InherentGainBinEn;
    NvU8 SupportsBinningControl;
    NvU8 SupportFastMode;
    NvU8 NvU8Spare2;
    NvU8 NvU8Spare3;
    NvU32 PllMult;
    NvU32 PllDiv;
    NvF32 ModeSwWaitFrames;
    NvU32 PlaceHolder1;
    NvU32 PlaceHolder2;
    NvU32 PlaceHolder3;
} NvcImagerDynamicNvc;

/**
 * Sensor specific data that's static (doesn't change) that
 * comes from the kernel driver.  (
 */
typedef struct NvcImagerStaticNvcRec
{
    NvU32 ApiVersion;
    NvU32 SensorType;
    NvU32 BitsPerPixel;
    NvU32 SensorId;
    NvU32 SensorIdMinor;
    NvF32 FocalLength;
    NvF32 MaxAperture;
    NvF32 FNumber;
    NvF32 HorizontalViewAngle;
    NvF32 VerticalViewAngle;
    NvU32 StereoCapable;
    NvU32 SensorResChangeWaitTime;
    NvU8 SupportIsp;
    NvU8 NvU8Spare1;
    NvU8 NvU8Spare2;
    NvU8 NvU8Spare3;
    NvU8 FuseID[16];
    NvU32 PlaceHolder1;
    NvU32 PlaceHolder2;
    NvU32 PlaceHolder3;
    NvU32 PlaceHolder4;
} NvcImagerStaticNvc;

typedef struct NvcImagerClockVariablesRec
{
    NvU32 VtPixClkFreqHz;
    NvF32 MaxExposure;
    NvF32 MinExposure;
    NvF32 MaxFrameRate;
    NvF32 MinFrameRate;
} NvcImagerClockVariables;

/**
 * Sensor bayer's private context
 */
typedef struct NvcImagerContextRec
{
    int camera_fd;
    NvOdmImagerCapabilities Cap;
    NvOdmImagerSensorMode *pModeList;
    NvOdmImagerSensorMode Mode;
    NvOdmImagerSensorMode ModeTmp;
    NvcImagerHal Hal;
    NvcImagerStaticNvc SNvc;
    NvcImagerDynamicNvc DNvc;
    NvcImagerDynamicNvc DNvcTmp;
    NvcImagerClockVariables ClkV;
    NvcImagerClockVariables ClkVTmp;

    NvU32 NumModes;
    NvF32 InherentGain;
    NvF32 Gains[4];
    NvU32 Gain;
    NvU32 FrameLength;
    NvU32 CoarseTime;
    NvF32 Exposure;
    NvF32 FrameRate;
    NvF32 RequestedMaxFrameRate;
    NvU32 FrameErrorCount;
    NvBool BinningControlEnabled;
    NvU8 BinningControlDirty;
    NvBool TestPatternMode;
    NvBool SensorInitialized;
    NvOdmImagerStereoCameraMode StereoCameraMode;
} NvcImagerContext;

/**
 * Sensor's private context
 */
typedef struct NvcImagerAEContextRec
{
    NvU32 frame_length;
    NvU8  frame_length_enable;
    NvU32 coarse_time;
    NvU8  coarse_time_enable;
    NvU32 gain;
    NvU8  gain_enable;
} __attribute__((packed)) NvcImagerAEContext;

NvBool NvcImager_GetHal(NvOdmImagerHandle hImager);

#if defined(__cplusplus)
}
#endif

#endif // NVC_SENSOR_BAYER_H

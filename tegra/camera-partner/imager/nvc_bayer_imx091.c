/**
 * Copyright (c) 2011-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#if (BUILD_FOR_AOS == 0)
#include <sys/types.h>
#include <asm/types.h>
#endif

#include "nvc_bayer_imx091.h"
#include "imager_hal.h"

#include "sensor_bayer_imx091_camera_config.h"

#define SENSOR_GAIN_TO_F32(x)	(256.f / (256.f - (NvF32)(x)))
#define SENSOR_F32_TO_GAIN(x)	((NvU32)(256.f - (256.f / (x))))


static const char *pOverrideFiles[] =
{
    "/data/camera_overrides.isp",
};

static const char *pFactoryBlobFiles[] =
{
    "/data/factory.bin",
    "/data/calibration.bin",
};

static NV_INLINE NvBool
NvcIMX091_Float2Gain(NvF32 x, NvU32 *NewGain, NvF32 MinGain, NvF32 MaxGain)
{
    if (x > MaxGain)
        x = MaxGain;
    else if (x < MinGain)
        x = MinGain;
    *NewGain = SENSOR_F32_TO_GAIN(x);
    return NV_TRUE;
}

static NV_INLINE NvBool
NvcIMX091_Gain2Float(NvU32 x, NvF32 *NewGain, NvF32 MinGain, NvF32 MaxGain)
{
    if (x > 240)
    {
        NvOsDebugPrintf("[IMX091]:%s:Input value out of range\n", __func__);
        return NV_FALSE;
    }
    *NewGain = SENSOR_GAIN_TO_F32(x);
    return NV_TRUE;
}


/**
 * Nvc<DEVICE>_GetHal
 * return the hal functions associated with NVC device
 */
NvBool NvcIMX091_GetHal(pNvcImagerHal hNvcHal, NvU32 DevIdMinor)
{
    if (!hNvcHal)
        return NV_FALSE;

    hNvcHal->pfnFloat2Gain = NvcIMX091_Float2Gain;
    hNvcHal->pfnGain2Float = NvcIMX091_Gain2Float;
    hNvcHal->pCalibration = pSensorCalibrationData;
    hNvcHal->pOverrideFiles = pOverrideFiles;
    hNvcHal->OverrideFilesCount = sizeof(pOverrideFiles) /
                                  sizeof(pOverrideFiles[0]);
    hNvcHal->pFactoryBlobFiles = pFactoryBlobFiles;
    hNvcHal->FactoryBlobFilesCount = sizeof(pFactoryBlobFiles) /
                                     sizeof(pFactoryBlobFiles[0]);
    return NV_TRUE;
}

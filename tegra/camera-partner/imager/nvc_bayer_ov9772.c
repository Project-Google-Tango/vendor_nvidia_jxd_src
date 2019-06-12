/**
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include <sys/types.h>
#if (BUILD_FOR_AOS == 0)
#include <asm/types.h>
#endif

#include "nvc_bayer_ov9772.h"
#include "imager_hal.h"

#include "sensor_bayer_ov9772_camera_config.h"

static const char *pOverrideFiles[] =
{
    "/data/camera_overrides_front.isp",
};

static const char *pFactoryBlobFiles[] =
{
    "/data/factory_front.bin",
    "/data/calibration_front.bin",
};

static NV_INLINE NvBool
NvcOV9772_Float2Gain(NvF32 x, NvU32 *NewGain, NvF32 MinGain, NvF32 MaxGain)
{
    NvU32 Bit7to4 = 0;
    NvU32 Bit0To3 = 0;
    NvU32 BitsSet = 0;
    NvF32 gainForBit0To3;
    NvU32 i;

    if (x > MaxGain)
        x = MaxGain;
    if (x < MinGain)
        x = MinGain;
    if (x < 2.0)
    {
        BitsSet = 0;
        gainForBit0To3 = (x - 1.0) / 0.0625;
        if (!gainForBit0To3)
            gainForBit0To3 = 1;
    }
    else if (x < 4.0)
    {
        BitsSet = 1;
        gainForBit0To3 = (x - 2.0) / 0.125;
    }
    else if (x < 8.0)
    {
        BitsSet = 2;
        gainForBit0To3 = (x - 4.0) / 0.25;
    }
    else if (x < 16.0)
    {
        BitsSet = 3;
        gainForBit0To3 = (x - 8.0) / 0.5;
    }
    else
    {
        BitsSet = 4;
        gainForBit0To3 = (x - 16.0);
    }
    Bit0To3 = (NvU32)(gainForBit0To3);
    if((gainForBit0To3 - (NvF32)(Bit0To3) > 0.5) && (BitsSet <= 4))
    {
        Bit0To3 = (Bit0To3 + 1) & 0x0F;
        if (Bit0To3 == 0)
            BitsSet++;
    }
    for(i = 0; i < BitsSet; i++){
        Bit7to4 |= 1 << (i+4);
    }
    *NewGain = (Bit7to4 | Bit0To3);
    return NV_TRUE;
}

static NV_INLINE NvBool
NvcOV9772_Gain2Float(NvU32 x, NvF32 *NewGain, NvF32 MinGain, NvF32 MaxGain)
{
    NvU32 Bit7;
    NvU32 Bit6;
    NvU32 Bit5;
    NvU32 Bit4;
    NvU32 Bit0To3;
    NvF32 F32Result;

    Bit7 = (x >> 7) & 0x1;
    Bit6 = (x >> 6) & 0x1;
    Bit5 = (x >> 5) & 0x1;
    Bit4 = (x >> 4) & 0x1;
    Bit0To3 = (x & 0x0f);
    F32Result = (NvF32)(Bit0To3);
    F32Result /= 16;
    F32Result += 1.0;
    F32Result *= (NvF32)(Bit4 + 1);
    F32Result *= (NvF32)(Bit5 + 1);
    F32Result *= (NvF32)(Bit6 + 1);
    F32Result *= (NvF32)(Bit7 + 1);
    *NewGain = F32Result;
    return NV_TRUE;
}


/**
 * Nvc<DEVICE>_GetHal
 * return the hal functions associated with NVC device
 */
NvBool NvcOV9772_GetHal(pNvcImagerHal hNvcHal, NvU32 DevIdMinor)
{
    if (!hNvcHal)
        return NV_FALSE;

    hNvcHal->pfnFloat2Gain = NvcOV9772_Float2Gain;
    hNvcHal->pfnGain2Float = NvcOV9772_Gain2Float;
    hNvcHal->pCalibration = pSensorCalibrationData;
    hNvcHal->pOverrideFiles = pOverrideFiles;
    hNvcHal->OverrideFilesCount = sizeof(pOverrideFiles) / sizeof(pOverrideFiles[0]);
    hNvcHal->pFactoryBlobFiles = pFactoryBlobFiles;
    hNvcHal->FactoryBlobFilesCount = sizeof(pFactoryBlobFiles) /
                                     sizeof(pFactoryBlobFiles[0]);
    return NV_TRUE;
}

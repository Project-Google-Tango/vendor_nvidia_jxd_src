/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_DRIVER_FLASH_H
#define PCL_DRIVER_FLASH_H

#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PCL_FLASH_MAX_NUM_CURRENT_LEVELS (260)

typedef struct NvPclFlashObjectRec
{
    NvU32 Version;
    NvU32 Id;
    NvF32 CurrentLevel;
    NvPointF32 ColorTemperature;
    NvU8 NumFlashCurrentLevels;
    NvU8 NumTorchCurrentLevels;
    NvF32 FlashCurrentLevel[PCL_FLASH_MAX_NUM_CURRENT_LEVELS];
    NvF32 TorchCurrentLevel[PCL_FLASH_MAX_NUM_CURRENT_LEVELS];
    NvU64 FlashChargeDuration;
} NvPclFlashObject;

typedef struct NvPclFlashObjectRec *NvPclFlashObjectHandle;

#if defined(__cplusplus)
}
#endif



#endif  //PCL_DRIVER_FLASH_H


/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvddk_operatingmodes.h"
#include "nvddk_fuse.h"
#include "nvflash_util.h"

NvError NvDdkFuseGet(NvDdkFuseDataType Type, void *pData, NvU32 *pSize)
{
    NvU32 Size;

    // Default As of Now
    Size = sizeof(NvU32);
    if (*pSize == 0)
    {
        *pSize = Size;
        return NvError_Success;
    }

    if (*pSize < Size) return NvError_InsufficientMemory;
    if (pData == NULL) return NvError_BadParameter;

    switch (Type)
    {
        case NvDdkFuseDataType_OpMode:
            *((NvU32 *)pData) = nvflash_get_opmode();
            break;
        case NvDdkFuseDataType_SecBootDeviceSelect:
           *((NvU32 *)pData) =  4; //NvBootDevType_Sdmmc;
            break;
        case NvDdkFuseDataType_SecBootDevInst:
            *((NvU32 *)pData) = 3;
            break;
        default:
            return NvError_BadValue;
    }
    return NvError_Success;
}

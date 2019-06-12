/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_FUSE_READ_H
#define INCLUDED_NVDDK_FUSE_READ_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"
#include "nvddk_fuse.h"

NvError NvDdkFusePrivGetDataT30(
        NvDdkFuseDataType Type,
        void *pData,
        NvU32 *Size);

NvError NvDdkFusePrivGetTypeSizeT30(NvDdkFuseDataType Type,
                                    NvU32 *pSize);

NvError NvDdkFusePrivGetDataT11x(
        NvDdkFuseDataType Type,
        void *pData,
        NvU32 *Size);

NvError NvDdkFusePrivGetTypeSizeT11x(NvDdkFuseDataType Type,
                                     NvU32 *pSize);

NvError NvDdkFusePrivGetDataT12x(
        NvDdkFuseDataType Type,
        void *pData,
        NvU32 *Size);

NvError NvDdkFusePrivGetTypeSizeT12x(NvDdkFuseDataType Type,
                                     NvU32 *pSize);

NvError NvDdkFusePrivGetDataT14x(
        NvDdkFuseDataType Type,
        void *pData,
        NvU32 *Size);

NvError NvDdkFusePrivGetTypeSizeT14x(NvDdkFuseDataType Type,
                                     NvU32 *pSize);

#ifdef __cplusplus
}
#endif

#endif

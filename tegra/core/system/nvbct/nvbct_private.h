/*
 * Copyright (c) 2008 - 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvbct_private.h
 * @brief <b> Nv Boot Configuration Table Management Framework.</b>
 *
 * @b Description: This file declares chip specifc extention API's for interacting with
 *    the Boot Configuration Table (BCT).
 */

#ifndef INCLUDED_NVBCT_PRIVATE_H
#define INCLUDED_NVBCT_PRIVATE_H

#include "nvbct.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define UNPACK_NVU32(pData8, Data32) \
    do { \
       NV_ASSERT(sizeof(*(pData8))==1); \
       NV_ASSERT(sizeof(Data32)==4); \
       (pData8)[0] = (Data32) & 0xFF; \
       (pData8)[1] = ((Data32) >> 8) & 0xFF; \
       (pData8)[2] = ((Data32) >> 16) & 0xFF; \
       (pData8)[3] = ((Data32) >> 24) & 0xFF; \
    } while (0)

#define PACK_NVU32(Data32, pData8) \
    do { \
       NV_ASSERT(sizeof(*(pData8))==1); \
       NV_ASSERT(sizeof(Data32)==4); \
       (Data32) = ((pData8)[3] << 24) | \
                  ((pData8)[2] << 16) | \
                  ((pData8)[1] <<  8) | \
                  (pData8)[0]; \
    } while (0)

enum { NVBCT_MAX_BL_PARTITION_ID = 255 };

enum { NVBCT_MAX_BL_VERSION = 255 };

NvU32 NvBctGetBctSizeAp15(void);
NvU32 NvBctGetBctSizeAp20(void);
NvU32 NvBctGetBctSizeT30(void);

NvError NvBctPrivGetDataSizeAp15(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);
NvError NvBctPrivGetDataSizeAp20(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);
NvError NvBctPrivGetDataSizeT30(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError NvBctGetDataAp15(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);
NvError NvBctGetDataAp20(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);
NvError NvBctGetDataT30(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError NvBctSetDataAp15(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);
NvError NvBctSetDataAp20(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);
NvError NvBctSetDataT30(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvU32 NvBctGetBctSizeT11x(void);

NvError NvBctPrivGetDataSizeT11x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError NvBctGetDataT11x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError NvBctSetDataT11x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvU32 NvBctPrivGetSignatureOffsetT11x(void);

NvU32 NvBctPrivGetSignDataOffsetT11x(void);

NvU32 NvBctGetBctSizeT12x(void);

NvError NvBctPrivGetDataSizeT12x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError NvBctGetDataT12x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError NvBctSetDataT12x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvU32 NvBctGetBctSizeT14x(void);

NvError NvBctPrivGetDataSizeT14x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);


NvError NvBctGetDataT14x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);


NvError NvBctSetDataT14x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvU32 NvBctPrivGetSignatureOffsetT14x(void);

NvU32 NvBctPrivGetSignDataOffsetT14x(void);

NvU32 NvBctPrivGetSignatureOffsetT12x(void);

NvU32 NvBctPrivGetSignDataOffsetT12x(void);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBCT_PRIVATE_H

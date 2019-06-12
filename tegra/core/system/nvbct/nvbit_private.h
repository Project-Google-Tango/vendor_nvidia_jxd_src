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
 * @file nvbit_private.h
 * @brief <b> Nv Boot Information Table Management Framework.</b>
 *
 * @b Description: This file declares chip specific private extension API's for interacting with
 *    the Boot Information Table (BIT).
 */

#ifndef INCLUDED_NVBIT_PRIVATE_H
#define INCLUDED_NVBIT_PRIVATE_H

#include "nvbit.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvError NvBitInitAp15(NvBitHandle *phBit);
NvError NvBitInitAp20(NvBitHandle *phBit);
NvError NvBitInitT30(NvBitHandle *phBit);

NvError
NvBitGetDataAp15(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);
NvError
NvBitGetDataAp20(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);
NvError
NvBitGetDataT30(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError
NvBitPrivGetDataSizeAp15(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);
NvError
NvBitPrivGetDataSizeAp20(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError
NvBitPrivGetDataSizeT30(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError NvBitInitT11x(NvBitHandle *phBit);

NvError
NvBitGetDataT11x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError
NvBitPrivGetDataSizeT11x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError NvBitInitT12x(NvBitHandle *phBit);

NvError
NvBitGetDataT12x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError
NvBitPrivGetDataSizeT12x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvError NvBitInitT14x(NvBitHandle *phBit);

NvError
NvBitGetDataT14x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

NvError
NvBitPrivGetDataSizeT14x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

NvBitSecondaryBootDevice
    NvBitPrivGetBootDevTypeFromBitDataAp20(
    NvU32 BitDataDevice);

NvBitSecondaryBootDevice
    NvBitPrivGetBootDevTypeFromBitDataT30(
    NvU32 BitDataDevice);


NvBitSecondaryBootDevice
    NvBitPrivGetBootDevTypeFromBitDataT11x(
    NvU32 BitDataDevice);

NvBitSecondaryBootDevice
    NvBitPrivGetBootDevTypeFromBitDataT14x(
    NvU32 BitDataDevice);

NvBitSecondaryBootDevice
    NvBitPrivGetBootDevTypeFromBitDataT12x(
    NvU32 BitDataDevice);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBIT_PRIVATE_H

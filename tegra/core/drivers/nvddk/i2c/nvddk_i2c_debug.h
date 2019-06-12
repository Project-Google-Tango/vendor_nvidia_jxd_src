/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVDDK_DDK_I2C_DEBUG_H
#define NVDDK_DDK_I2C_DEBUG_H

#include "nvcommon.h"
#include "nvassert.h"
#include "nvuart.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define ENABLE_ERROR_PRINTS 0
#define ENABLE_DEBUG_PRINTS 0
#define ENABLE_INFO_PRINTS  0

#if ENABLE_ERROR_PRINTS
#define DDK_I2C_ERROR_PRINT(...) NvOsAvpDebugPrintf(__VA_ARGS__)
#else
#define DDK_I2C_ERROR_PRINT(...)
#endif

#if ENABLE_DEBUG_PRINTS
#define DDK_I2C_DEBUG_PRINT(...) NvOsAvpDebugPrintf(__VA_ARGS__)
#else
#define DDK_I2C_DEBUG_PRINT(...)
#endif

#if ENABLE_INFO_PRINTS
#define DDK_I2C_INFO_PRINT(...) NvOsAvpDebugPrintf(__VA_ARGS__)
#else
#define DDK_I2C_INFO_PRINT(...)
#endif

#if defined(__cplusplus)
}
#endif

#endif // NVDDK_DDK_I2C_DEBUG_H


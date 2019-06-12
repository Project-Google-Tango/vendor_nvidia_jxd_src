/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef NVDDK_SPI_DEBUG_H
#define NVDDK_SPI_DEBUG_H


#include "nvcommon.h"
#include "nvassert.h"


#define ENABLE_ERROR_PRINTS 1
#define ENABLE_DEBUG_PRINTS 0
#define ENABLE_INFO_PRINTS 0

#if ENABLE_ERROR_PRINTS
#define SPI_ERROR_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)
#else
#define SPI_ERROR_PRINT(...)
#endif

#if ENABLE_DEBUG_PRINTS
#define SPI_DEBUG_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)
#else
#define SPI_DEBUG_PRINT(...)
#endif

#if ENABLE_INFO_PRINTS
#define SPI_INFO_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)
#else
#define SPI_INFO_PRINT(...)
#endif

#endif // NVDDK_SPI_DEBUG_H
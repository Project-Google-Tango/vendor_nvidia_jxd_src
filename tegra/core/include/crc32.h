/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Cyclic Redundancy Check API</b>
 *
 * @b Description: This file declares the API for computing CRC32.
 */

#ifndef INCLUDED_CRC32_H
#define INCLUDED_CRC32_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"
/**
 * @defgroup nvcrc_boot Computes CRC32 Values
 *
 * @ingroup nvbdk_modules
 * @{
 */

/**
 * Computes CRC32 value of the submitted buffer.
 *
 * @param CrcInitVal Initial CRC value (If Any) otherwise zero.
 * @param buf A pointer to the buffer for which the CRC32 must be calculated.
 * @param size Size of the buffer.
 *
 * @retval Returns the CRC32 value of the buffer.
 */
NvU32 NvComputeCrc32(NvU32 CrcInitVal, const NvU8 *buf, NvU32 size);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_CRC32_H


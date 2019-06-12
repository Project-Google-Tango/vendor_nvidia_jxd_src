/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_MIC2826_I2C_H
#define INCLUDED_NVODM_PMU_MIC2826_I2C_H

#include "nvodm_pmu.h"
#include "max8907b.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Constant definition
#define MIC2826_SLAVE_ADDR      0xB4
#define MIC2826_I2C_SPEED_KHZ   400

// Function declaration
NvBool MIC2826I2cWrite8(
   NvOdmPmuDeviceHandle hPmu,
   NvU8 Addr,
   NvU8 Data);

NvBool MIC2826I2cRead8(
   NvOdmPmuDeviceHandle hPmu,
   NvU8 Addr,
   NvU8 *Data);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_PMU_MIC2826_I2C_H

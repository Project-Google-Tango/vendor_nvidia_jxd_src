/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_TPS6856X_I2C_H
#define INCLUDED_NVODM_PMU_TPS6856X_I2C_H

#include "nvodm_pmu_tps6586x.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Constant definition
#define TPS6586x_I2C_SPEED_KHZ   400

// Function declaration
NvBool Tps6586xI2cWrite8(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 Data);

NvBool Tps6586xI2cRead8(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 *Data);
    
NvBool Tps6586xI2cWrite32(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 Data);
    
NvBool Tps6586xI2cRead32(
    NvOdmPmuDeviceHandle hPmu,
    NvU32 Addr,
    NvU32 *Data);
    

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_PMU_TPS6856X_I2C_H

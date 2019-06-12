/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU-I2c Interface utility</b>
  *
  * @b Description: Defines the i2c interface for the pmu drivers.
  *
  */

#ifndef INCLUDED_NVODM_PMU_I2C_H
#define INCLUDED_NVODM_PMU_I2C_H

#include "nvodm_services.h"
#include "nvodm_pmu.h"

typedef struct NvOdmPmuI2cRec *NvOdmPmuI2cHandle;

#if defined(__cplusplus)
extern "C"
{
#endif

NvOdmPmuI2cHandle NvOdmPmuI2cOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance);

void NvOdmPmuI2cClose(NvOdmPmuI2cHandle hOdmPmuI2c);

void NvOdmPmuI2cDeInit(void);

NvBool NvOdmPmuI2cWrite8(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Data);

NvBool NvOdmPmuI2cRead8(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data);

NvBool NvOdmPmuI2cUpdate8(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Value,
    NvU8 Mask);

NvBool NvOdmPmuI2cSetBits(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask);

NvBool NvOdmPmuI2cClrBits(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask);

NvBool NvOdmPmuI2cWrite16(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 Data);

NvBool NvOdmPmuI2cRead16(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 *Data);

NvBool NvOdmPmuI2cWrite32(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 Data);

NvBool NvOdmPmuI2cRead32(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 *Data);

NvBool NvOdmPmuI2cWriteBlock(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data,
    NvU32 DataSize);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_PMU_I2C_H

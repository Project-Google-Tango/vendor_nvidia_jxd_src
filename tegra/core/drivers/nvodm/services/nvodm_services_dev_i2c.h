/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_DEV_I2C_H
#define INCLUDED_NVODM_DEV_I2C_H

#include "nvodm_services.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define I2C_DEV_TRANSACTION_TIMEOUT 1000

typedef struct NvOdmDevI2cRec {
    NvOdmServicesI2cHandle hOdmI2c;
    NvOdmOsMutexHandle hMutex;
    NvU32 OpenCount;
} NvOdmDevI2c;


typedef struct NvOdmDevI2cRec *NvOdmDevI2cHandle;


NvOdmDevI2cHandle NvOdmDevI2cOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance);

void NvOdmDevI2cClose(NvOdmDevI2cHandle hOdmDevI2c);

NvBool NvOdmDevI2cWrite8(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Data);

NvBool NvOdmDevI2cRead8(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data);

NvBool NvOdmDevI2cUpdate8(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Value,
    NvU8 Mask);

NvBool NvOdmDevI2cSetBits(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask);

NvBool NvOdmDevI2cClrBits(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask);

NvBool NvOdmDevI2cWrite16(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 Data);

NvBool NvOdmDevI2cRead16(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 *Data);

NvBool NvOdmDevI2cWrite32(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 Data);

NvBool NvOdmDevI2cRead32(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 *Data);

NvBool NvOdmDevI2cWriteBlock(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data,
    NvU32 DataSize);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVODM_DEV_I2C_H

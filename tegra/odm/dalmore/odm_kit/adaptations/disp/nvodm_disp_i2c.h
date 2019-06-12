/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


/* Implements I2C interface for the  Display driver */

#ifndef INCLUDED_NVODM_DISP_I2C_H
#define INCLUDED_NVODM_DISP_I2C_H

#include "nvodm_services.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define I2C_DISP_TRANSACTION_TIMEOUT 1000


NvBool NvOdmDispI2cWrite32(
    NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU16 RegAddr,
    NvU32 Data);

NvBool NvOdmDispI2cRead32(
    NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU16 RegAddr,
    NvU32 *Data);

#if defined(__cplusplus)
}
#endif

#endif


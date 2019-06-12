
#define NV_IDL_IS_STUB

/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_i2c.h"

NvError NvRmI2cTransaction(
    NvRmI2cHandle hI2c,
    NvU32 I2cPinMap,
    NvU32 WaitTimeoutInMilliSeconds,
    NvU32 ClockSpeedKHz,
    NvU8 *Data,
    NvU32 DataLen,
    NvRmI2cTransactionInfo *Transaction,
    NvU32 NumOfTransactions)
{
    return NvSuccess;
}

void NvRmI2cClose(NvRmI2cHandle hI2c)
{
}

NvError NvRmI2cOpen(NvRmDeviceHandle hDevice,
    NvU32 IoModule,
    NvU32 instance,
    NvRmI2cHandle *phI2c)
{
    if (!phI2c)
        return NvError_BadParameter;

    *phI2c = (void *)0xdeadbeef;
    return NvSuccess;
}

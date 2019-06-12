/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
 *
 * @b Description: Defines the interface for TPS8003X core driver.
 *
 */

#ifndef INCLUDED_TPS8003X_CORE_DRIVER_H
#define INCLUDED_TPS8003X_CORE_DRIVER_H

#include "nvodm_pmu.h"
typedef struct Tps8003xCoreDriverRec *Tps8003xCoreDriverHandle;

typedef enum {
    Tps8003xSlaveId_0 = 0,
    Tps8003xSlaveId_1 = 1,
    Tps8003xSlaveId_2 = 2,
    Tps8003xSlaveId_3 = 3,
} Tps8003xSlaveId;

#if defined(__cplusplus)
extern "C"
{
#endif

Tps8003xCoreDriverHandle Tps8003xCoreDriverOpen(NvOdmPmuDeviceHandle hDevice);

void Tps8003xCoreDriverClose(Tps8003xCoreDriverHandle hTps8003xCore);

NvBool Tps8003xRegisterWrite8(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 Data);

NvBool Tps8003xRegisterRead8(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 *Data);

NvBool Tps8003xRegisterUpdate8(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 Value,
    NvU8 Mask);

NvBool Tps8003xRegisterSetBits(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 BitMask);

NvBool Tps8003xRegisterClrBits(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 BitMask);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TPS8003X_CORE_DRIVER_H */

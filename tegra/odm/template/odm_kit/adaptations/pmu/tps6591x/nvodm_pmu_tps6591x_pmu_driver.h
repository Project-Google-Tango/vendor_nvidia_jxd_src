/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS6591X-driver Interface</b>
  *
  * @b Description: Defines the interface for pmu TPS6591X driver.
  *
  */

#ifndef ODM_PMU_TPS6591X_PMU_DRIVER_H
#define ODM_PMU_TPS6591X_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    TPS6591xPmuSupply_Invalid = 0x0,

    // VIO
    TPS6591xPmuSupply_VIO,

    // VDD1
    TPS6591xPmuSupply_VDD1,

    // VDD2
    TPS6591xPmuSupply_VDD2,

    // VDDCTRL
    TPS6591xPmuSupply_VDDCTRL,

    // LDO1
    TPS6591xPmuSupply_LDO1,

    // LDO2
    TPS6591xPmuSupply_LDO2,

    // LDO3
    TPS6591xPmuSupply_LDO3,

    // LDO4
    TPS6591xPmuSupply_LDO4,

    // LDO5
    TPS6591xPmuSupply_LDO5,

    // LDO6
    TPS6591xPmuSupply_LDO6,

    // LDO7
    TPS6591xPmuSupply_LDO7,

    // LDO8
    TPS6591xPmuSupply_LDO8,

    // LDO9
    TPS6591xPmuSupply_RTC_OUT,

    // PMU GPO0
    TPS6591xPmuSupply_GPO0,

    // PMU GPO1
    TPS6591xPmuSupply_GPO1,

    // PMU GPO2
    TPS6591xPmuSupply_GPO2,

    // PMU GPO3
    TPS6591xPmuSupply_GPO3,

    // PMU GPO4
    TPS6591xPmuSupply_GPO4,

    // PMU GPO5
    TPS6591xPmuSupply_GPO5,

    // PMU GPO6
    TPS6591xPmuSupply_GPO6,

    // PMU GPO7
    TPS6591xPmuSupply_GPO7,

    // PMU GPO8
    TPS6591xPmuSupply_GPO8,

    // Last entry to MAX
    TPS6591xPmuSupply_Num,

    TPS6591xPmuSupply_Force32 = 0x7FFFFFFF
} TPS6591xPmuSupply;

NvOdmPmuDriverInfoHandle Tps6591xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps);

void Tps6591xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps6591xPmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps6591xPmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps6591xPmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

void Tps6591xPmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS6591X_PMU_DRIVER_H */

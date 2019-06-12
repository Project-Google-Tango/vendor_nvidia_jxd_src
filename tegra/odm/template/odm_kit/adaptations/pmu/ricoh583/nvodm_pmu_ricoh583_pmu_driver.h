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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU Ricoh583-driver Interface</b>
  *
  * @b Description: Defines the interface for pmu ricoh583 driver.
  *
  */

#ifndef ODM_PMU_RICOH583_PMU_DRIVER_H
#define ODM_PMU_RICOH583_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    Ricoh583PmuSupply_Invalid = 0x0,

    // DC0
    Ricoh583PmuSupply_DC0,

    // DC1
    Ricoh583PmuSupply_DC1,

    // DC2
    Ricoh583PmuSupply_DC2,

    // DC3
    Ricoh583PmuSupply_DC3,

    // LDO0
    Ricoh583PmuSupply_LDO0,

    // LDO1
    Ricoh583PmuSupply_LDO1,

    // LDO2
    Ricoh583PmuSupply_LDO2,

    // LDO3
    Ricoh583PmuSupply_LDO3,

    // LDO4
    Ricoh583PmuSupply_LDO4,

    // LDO5
    Ricoh583PmuSupply_LDO5,

    // LDO6
    Ricoh583PmuSupply_LDO6,

    // LDO7
    Ricoh583PmuSupply_LDO7,

    // LDO8
    Ricoh583PmuSupply_LDO8,

    // LDO9
    Ricoh583PmuSupply_LDO9,

    // GPIO0
    Ricoh583PmuSupply_GPIO0,

    // GPIO1
    Ricoh583PmuSupply_GPIO1,

    // GPIO2
    Ricoh583PmuSupply_GPIO2,

    // GPIO3
    Ricoh583PmuSupply_GPIO3,

    // GPIO4
    Ricoh583PmuSupply_GPIO4,

    // GPIO5
    Ricoh583PmuSupply_GPIO5,

    // GPIO6
    Ricoh583PmuSupply_GPIO6,

    // GPIO7
    Ricoh583PmuSupply_GPIO7,

    // Last entry to MAX
    Ricoh583PmuSupply_Num,

    Ricoh583PmuSupply_Force32 = 0x7FFFFFFF
} Ricoh583PmuSupply;

NvOdmPmuDriverInfoHandle Ricoh583PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps);

void Ricoh583PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Ricoh583PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Ricoh583PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Ricoh583PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_RICOH583_PMU_DRIVER_H */

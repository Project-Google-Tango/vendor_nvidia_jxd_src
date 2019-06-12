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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU Max77663-driver Interface</b>
  *
  * @b Description: Defines the interface for pmu max77663 driver.
  *
  */

#ifndef ODM_PMU_MAX77663_PMU_DRIVER_H
#define ODM_PMU_MAX77663_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    Max77663PmuSupply_Invalid = 0x0,

    // SD0
    Max77663PmuSupply_SD0,

    // SD1
    Max77663PmuSupply_SD1,

    // SD2
    Max77663PmuSupply_SD2,

    // SD3
    Max77663PmuSupply_SD3,

    // LDO0
    Max77663PmuSupply_LDO0,

    // LDO1
    Max77663PmuSupply_LDO1,

    // LDO2
    Max77663PmuSupply_LDO2,

    // LDO3
    Max77663PmuSupply_LDO3,

    // LDO4
    Max77663PmuSupply_LDO4,

    // LDO5
    Max77663PmuSupply_LDO5,

    // LDO6
    Max77663PmuSupply_LDO6,

    // LDO7
    Max77663PmuSupply_LDO7,

    // LDO8
    Max77663PmuSupply_LDO8,

    // GPIO0
    Max77663PmuSupply_GPIO0,

    // GPIO1
    Max77663PmuSupply_GPIO1,

    // GPIO2
    Max77663PmuSupply_GPIO2,

    // GPIO3
    Max77663PmuSupply_GPIO3,

    // GPIO4
    Max77663PmuSupply_GPIO4,

    // GPIO5
    Max77663PmuSupply_GPIO5,

    // GPIO6
    Max77663PmuSupply_GPIO6,

    // GPIO7
    Max77663PmuSupply_GPIO7,

    // Last entry to MAX
    Max77663PmuSupply_Num,

    Max77663PmuSupply_Force32 = 0x7FFFFFFF
} Max77663PmuSupply;

NvOdmPmuDriverInfoHandle Max77663PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                                               NvOdmPmuRailProps *pOdmRailProps,
                                               int NrOfOdmRailProps);
void Max77663PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);
void Max77663PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                        NvOdmPmuVddRailCapabilities* pCapabilities);
NvBool Max77663PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32* pMilliVolts);
NvBool Max77663PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

void Max77663PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_MAX77663_PMU_DRIVER_H */

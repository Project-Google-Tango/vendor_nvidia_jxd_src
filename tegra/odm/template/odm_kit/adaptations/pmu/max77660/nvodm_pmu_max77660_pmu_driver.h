/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
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

#ifndef ODM_PMU_MAX77660_PMU_DRIVER_H
#define ODM_PMU_MAX77660_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    Max77660PmuSupply_Invalid = 0x0,
    Max77660PmuSupply_BUCK1,
    Max77660PmuSupply_BUCK2,
    Max77660PmuSupply_BUCK3,
    Max77660PmuSupply_BUCK4,
    Max77660PmuSupply_BUCK5,
    Max77660PmuSupply_BUCK6,
    Max77660PmuSupply_BUCK7,
    Max77660PmuSupply_LDO1,
    Max77660PmuSupply_LDO2,
    Max77660PmuSupply_LDO3,
    Max77660PmuSupply_LDO4,
    Max77660PmuSupply_LDO5,
    Max77660PmuSupply_LDO6,
    Max77660PmuSupply_LDO7,
    Max77660PmuSupply_LDO8,
    Max77660PmuSupply_LDO9,
    Max77660PmuSupply_LDO10,
    Max77660PmuSupply_LDO11,
    Max77660PmuSupply_LDO12,
    Max77660PmuSupply_LDO13,
    Max77660PmuSupply_LDO14,
    Max77660PmuSupply_LDO15,
    Max77660PmuSupply_LDO16,
    Max77660PmuSupply_LDO17,
    Max77660PmuSupply_LDO18,
    Max77660PmuSupply_SW1,
    Max77660PmuSupply_SW2,
    Max77660PmuSupply_SW3,
    Max77660PmuSupply_SW4,
    Max77660PmuSupply_SW5,

    // Last entry to MAX
    Max77660PmuSupply_Num,

    Max77660PmuSupply_Force32 = 0x7FFFFFFF
} Max77660PmuSupply;

NvOdmPmuDriverInfoHandle Max77660PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                                               NvOdmPmuRailProps *pOdmRailProps,
                                               int NrOfOdmRailProps);
void Max77660PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);
void Max77660PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                        NvOdmPmuVddRailCapabilities* pCapabilities);
NvBool Max77660PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32* pMilliVolts);
NvBool Max77660PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

void Max77660PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);
void Max77660SetChargingInterrupt(NvOdmPmuDriverInfoHandle hPmuDriver);
void Max77660UnSetChargingInterrupt(NvOdmPmuDriverInfoHandle hPmuDriver);

void Max77660PmuInterruptHandler(NvOdmPmuDeviceHandle hPmuDriver);

void Max77660PmuEnableWdt(NvOdmPmuDriverInfoHandle hPmuDriver);

void Max77660PmuDisableWdt(NvOdmPmuDriverInfoHandle hPmuDriver);
NvU8 Max77660GetOtpVersion(void);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_MAX77660_PMU_DRIVER_H */

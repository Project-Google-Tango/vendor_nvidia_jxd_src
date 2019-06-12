/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS6238x Interface</b>
  *
  * @b Description: Defines the interface for pmu tps6238x driver.
  *
  */

#ifndef ODM_PMU_TPS6238X_PMU_DRIVER_H
#define ODM_PMU_TPS6238X_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum {
    Tps6238xMode_Invalid = 0,
    Tps6238xMode_0,
    Tps6238xMode_1,
} Tps6238xMode;

typedef struct Tps6238xBoardDataRec {
    NvBool InternalPDEnable;
    Tps6238xMode VoltageMode;
} Tps6238xBoardData;

typedef enum {
    Tps6238xPmuSupply_Invalid = 0,
    Tps6238xPmuSupply_VDD = 1,
    Tps6238xPmuSupply_Num,
} Tps6238xPmuSupply;

NvOdmPmuDriverInfoHandle Tps6238xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void Tps6238xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps6238xPmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps6238xPmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps6238xPmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS6238X_PMU_DRIVER_H */

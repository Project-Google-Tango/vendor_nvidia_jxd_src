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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS6236x Interface</b>
  *
  * @b Description: Defines the interface for pmu tps6236x driver.
  *
  */

#ifndef ODM_PMU_TPS6236X_PMU_DRIVER_H
#define ODM_PMU_TPS6236X_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum {
    Tps6236xMode_Invalid = 0,
    Tps6236xMode_0,
    Tps6236xMode_1,
    Tps6236xMode_2,
    Tps6236xMode_3,
} Tps6236xMode;

typedef enum {
    Tps6236xChipId_TPS62360 = 0,
    Tps6236xChipId_TPS62361B,
    Tps6236xChipId_TPS62362,
    Tps6236xChipId_TPS62363,
    Tps6236xChipId_TPS62365,
    Tps6236xChipId_TPS62366A,
    Tps6236xChipId_TPS62366B,
} Tps6236xChipId;

typedef struct Tps6236xChipInfoRec {
   Tps6236xChipId ChipId;
} Tps6236xChipInfo;

typedef struct Tps6236xBoardDataRec {
    NvBool InternalPDEnable;
    Tps6236xMode VoltageMode;
} Tps6236xBoardData;

typedef enum {
    Tps6236xPmuSupply_Invalid = 0,
    Tps6236xPmuSupply_VDD = 1,
    Tps6236xPmuSupply_Num,
} Tps6236xPmuSupply;

NvOdmPmuDriverInfoHandle Tps6236xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    Tps6236xChipInfo *ChipInfo, NvOdmPmuRailProps *pOdmRailProps,
    int NrOfOdmRailProps);

void Tps6236xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps6236xPmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps6236xPmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps6236xPmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS6236X_PMU_DRIVER_H */

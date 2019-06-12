/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU MAX897X Interface</b>
  *
  * @b Description: Defines the interface for pmu max897x driver.
  *
  */

#ifndef ODM_PMU_MAX897X_PMU_DRIVER_H
#define ODM_PMU_MAX897X_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum {
    Max897xMode_Invalid = 0,
    Max897xMode_0,           //VOUT mode
    Max897xMode_1,           //VOUT_DVS mode
} Max897xMode;

typedef struct Max897xBoardDataRec {
    Max897xMode VoltageMode;
} Max897xBoardData;

typedef enum {
    Max897xPmuSupply_Invalid = 0,
    Max897xPmuSupply_VDD = 1,
    Max897xPmuSupply_Num,
} Max897xPmuSupply;

NvOdmPmuDriverInfoHandle Max897xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void Max897xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Max897xPmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Max897xPmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Max897xPmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_MAX897X_PMU_DRIVER_H */

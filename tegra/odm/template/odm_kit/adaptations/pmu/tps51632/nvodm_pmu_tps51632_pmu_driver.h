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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS51632 Interface</b>
  *
  * @b Description: Defines the interface for pmu tps51632 driver.
  *
  */

#ifndef ODM_PMU_TPS51632_PMU_DRIVER_H
#define ODM_PMU_TPS51632_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum {
    Tps51632Mode_Invalid = 0,
    Tps51632Mode_0, /* Normal mode */
    Tps51632Mode_1, /* DVFS mode */
} Tps51632Mode;


typedef struct Tps51632BoardDataRec {
    NvBool       enable_pwm;
    NvU32        slew_rate_uv_per_us;
    Tps51632Mode VoltageMode;
} Tps51632BoardData;

typedef enum {
    Tps51632PmuSupply_Invalid = 0,
    Tps51632PmuSupply_VDD = 1,
    Tps51632PmuSupply_Num,
} Tps51632PmuSupply;

NvOdmPmuDriverInfoHandle Tps51632PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void Tps51632PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps51632PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps51632PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps51632PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS51632_PMU_DRIVER_H */

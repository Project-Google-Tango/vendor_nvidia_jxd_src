/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU LP8755 Interface</b>
  *
  * @b Description: Defines the interface for pmu lp8755 driver.
  *
  */

#ifndef ODM_PMU_LP8755_PMU_DRIVER_H
#define ODM_PMU_LP8755_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"
#include "lp8755.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum {

    Lp8755PmuSupply_Invalid,
    Lp8755PmuSupply_BUCK0,

    Lp8755PmuSupply_Num,
} Lp8755PmuSupply;

NvOdmPmuDriverInfoHandle Lp8755PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void Lp8755PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Lp8755PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Lp8755PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Lp8755PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_LP8755_PMU_DRIVER_H */

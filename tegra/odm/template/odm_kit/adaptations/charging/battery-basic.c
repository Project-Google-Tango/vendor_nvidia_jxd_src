/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcommon.h"
#include "nvassert.h"
#include "nvodm_pmu.h"
#include "nvodm_charging.h"
#include "nvos.h"

#ifdef LPM_BATTERY_CHARGING
NvBool NvOdmBatteryInit(NvOdmPmuDeviceHandle pPmu);
NvU32 NvOdmBatteryGetCharge(NvOdmPmuDeviceHandle pPmu);
NvBool NvOdmBatteryGetChargingConfigData(NvOdmChargingConfigData *pData);

NvBool NvOdmBatteryInit(NvOdmPmuDeviceHandle pPmu)
{
    NvBool Status;
    NvU8   BatteryStatus = 0;

    Status = NvOdmPmuGetBatteryStatus(pPmu, NvOdmPmuBatteryInst_Main, &BatteryStatus);
    if(!Status)
      return NV_FALSE;
    NvOsDebugPrintf("  BatteryStatus:%x", (NvU32)BatteryStatus);
    if (BatteryStatus & NVODM_BATTERY_STATUS_NO_BATTERY)
    {
        // Battery not connected (most likely a development system)
        NvOsDebugPrintf(" (no battery)\n");
        return NV_FALSE;
    }

    // Battery is connected (show status)
    if (BatteryStatus & NVODM_BATTERY_STATUS_HIGH)
        NvOsDebugPrintf(" (battery high)\n");
    if (BatteryStatus & NVODM_BATTERY_STATUS_LOW)
        NvOsDebugPrintf(" (battery low)\n");
    if (BatteryStatus & NVODM_BATTERY_STATUS_CRITICAL)
        NvOsDebugPrintf(" (battery critical)\n");
    if (BatteryStatus & NVODM_BATTERY_STATUS_CHARGING)
        NvOsDebugPrintf(" (battery charging)\n");

    return NV_TRUE;
}

NvU32 NvOdmBatteryGetCharge(NvOdmPmuDeviceHandle pPmu)
{
    NvBool Status;
    NvOdmPmuBatteryData BatteryData;
    Status = NvOdmPmuGetBatteryData(pPmu, NvOdmPmuBatteryInst_Main, &BatteryData);
    NV_ASSERT(Status);

#ifdef READ_BATTERY_SOC
    if (!Status)
        BatteryData.batterySoc = 0;
    return BatteryData.batterySoc;
#else
    if (!Status)
        BatteryData.batteryVoltage = 0;
    return BatteryData.batteryVoltage;
#endif
}

NvBool NvOdmBatteryGetChargingConfigData(NvOdmChargingConfigData *pData)
{
    return NvOdmQueryChargingConfigData(pData);
}
#endif

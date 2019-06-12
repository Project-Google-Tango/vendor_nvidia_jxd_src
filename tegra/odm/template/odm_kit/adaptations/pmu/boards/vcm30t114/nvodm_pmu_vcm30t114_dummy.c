/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_vcm30t114.h"
#include "nvassert.h"

NvBool
VCM30T114PmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(pStatus);

    if (!hDevice || !pStatus)
        return NV_FALSE;

    // NOTE: Not implemented completely as it is only for bootloader.

    *pStatus = NvOdmPmuAcLine_Online;
    return NV_TRUE;
}

NvBool
VCM30T114PmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(pStatus);
    NV_ASSERT(batteryInst <= NvOdmPmuBatteryInst_Num);

    if (!hDevice || !pStatus || !(batteryInst <= NvOdmPmuBatteryInst_Num))
        return NV_FALSE;


   // NOTE: Not implemented completely as it is only for bootloader.
   *pStatus = NVODM_BATTERY_STATUS_NO_BATTERY;
    return NV_TRUE;
}

NvBool
VCM30T114PmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData)
{
    NvOdmPmuBatteryData batteryData;
    NV_ASSERT(hDevice);
    NV_ASSERT(pData);
    NV_ASSERT(batteryInst <= NvOdmPmuBatteryInst_Num);

    if (!hDevice || !pData || !(batteryInst <= NvOdmPmuBatteryInst_Num))
        return NV_FALSE;

    batteryData.batteryLifePercent     = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryLifeTime        = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryVoltage         = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryCurrent         = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryAverageCurrent  = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryAverageInterval = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryMahConsumed     = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryTemperature     = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batterySoc             = NVODM_BATTERY_DATA_UNKNOWN;


    // NOTE: Not implemented completely as it is only for bootloader.
    *pData = batteryData;

    return NV_TRUE;
}

void
VCM30T114PmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(pLifeTime);
    NV_ASSERT(batteryInst <= NvOdmPmuBatteryInst_Num);

    if (!hDevice || !pLifeTime || !(batteryInst <= NvOdmPmuBatteryInst_Num))
        return;

    *pLifeTime = NVODM_BATTERY_DATA_UNKNOWN;
}

void
VCM30T114PmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(pChemistry);
    NV_ASSERT(batteryInst <= NvOdmPmuBatteryInst_Num);

    if (!hDevice || !pChemistry || !(batteryInst <= NvOdmPmuBatteryInst_Num))
        return;

    *pChemistry = NvOdmPmuBatteryChemistry_LION;
}


NvBool
VCM30T114PmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return NV_FALSE;

    // NOTE: Not implemented completely as it is only for bootloader.
    return NV_TRUE;
}

void VCM30T114PmuInterruptHandler( NvOdmPmuDeviceHandle  hDevice)
{
    // NOTE: Not implemented completely as it is only for bootloader.
}

NvBool VCM30T114PmuReadRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 *Count)
{
    // NOTE: Not implemented completely as it is only for bootloader.
    *Count = 0;
    return NV_FALSE;
}

NvBool VCM30T114PmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count)
{
    // NOTE: Not implemented completely as it is only for bootloader.
    return NV_FALSE;
}

NvBool VCM30T114PmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice)
{
    // NOTE: Not implemented completely as it is only for bootloader.
    return NV_FALSE;
}

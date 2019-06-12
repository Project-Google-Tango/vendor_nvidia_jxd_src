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
#include "nvodm_pmu_pluto.h"
#include "nvassert.h"
#include "max77665a/nvodm_pmu_max77665a_pmu_driver.h"
#include "max77665a/nvodm_pmu_max17047_pmu_driver.h"

#define NVODM_BATTERY_FULL_VOLTAGE_MV      4200
#define NVODM_BATTERY_HIGH_VOLTAGE_MV      3900
#define NVODM_BATTERY_LOW_VOLTAGE_MV       3400
#define NVODM_BATTERY_CRITICAL_VOLTAGE_MV  3200

typedef struct PlutoBatDriverInfoRec
{
    NvOdmBatChargingDriverInfoHandle hBChargingDriver;
    NvOdmFuelGaugeDriverInfoHandle hFuelGaugeDriver;
} PlutoBatDriverInfo;

static PlutoBatDriverInfo s_PlutoBatInfo = {NULL, NULL};

NvBool
PlutoPmuGetAcLineStatus(
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
PlutoPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus)
{
    NvU16 VbatSense = 0;
    NvU16 Soc = 0;
    NvBool ret;

    NV_ASSERT(hDevice);
    NV_ASSERT(pStatus);
    NV_ASSERT(batteryInst <= NvOdmPmuBatteryInst_Num);

    if (!hDevice || !pStatus || !(batteryInst <= NvOdmPmuBatteryInst_Num))
        return NV_FALSE;
    if (batteryInst == NvOdmPmuBatteryInst_Main)
    {
        ret = Max17047FuelGaugeDriverGetData(s_PlutoBatInfo.hFuelGaugeDriver, &VbatSense, &Soc);
        if (!ret)
        {
            NvOdmOsPrintf("Get Data Failed\n");
            return NV_FALSE;
        }
        if (VbatSense > NVODM_BATTERY_LOW_VOLTAGE_MV)  // modify these parameters on need
            *pStatus |= NVODM_BATTERY_STATUS_HIGH;
        else if ((VbatSense <= NVODM_BATTERY_LOW_VOLTAGE_MV)&&
            (VbatSense > NVODM_BATTERY_CRITICAL_VOLTAGE_MV))
            *pStatus |= NVODM_BATTERY_STATUS_LOW;
        else
            *pStatus |= NVODM_BATTERY_STATUS_CRITICAL;
    }
    else
        *pStatus = NVODM_BATTERY_STATUS_NO_BATTERY;

    return NV_TRUE;
}

NvBool
PlutoPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData)
{
    NvU16 VbatSense = 0;
    NvU16 Soc = 0;
    NvBool ret;

    NV_ASSERT(hDevice);
    NV_ASSERT(pData);
    NV_ASSERT(batteryInst <= NvOdmPmuBatteryInst_Num);

    if (!hDevice || !pData || !(batteryInst <= NvOdmPmuBatteryInst_Num))
        return NV_FALSE;

    pData->batteryLifePercent     = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryLifeTime        = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryVoltage         = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryCurrent         = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryAverageCurrent  = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryAverageInterval = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryMahConsumed     = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryTemperature     = NVODM_BATTERY_DATA_UNKNOWN;

    ret = Max17047FuelGaugeDriverGetData(s_PlutoBatInfo.hFuelGaugeDriver, &VbatSense, &Soc);
    if (!ret)
    {
        NvOdmOsPrintf("Get data failed\n");
        return NV_FALSE;
    }
    pData->batteryVoltage = VbatSense;
    pData->batterySoc = Soc;

    return NV_TRUE;
}

void
PlutoPmuGetBatteryFullLifeTime(
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
PlutoPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry)
{
    *pChemistry = NvOdmPmuBatteryChemistry_LION;
}

NvBool
PlutoPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return NV_FALSE;

    return Max77665aBatChargingDriverSetCurrent(s_PlutoBatInfo.hBChargingDriver,
                             chargingPath, chargingCurrentLimitMa, ChargerType);
}

void
*PlutoPmuBatterySetup(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    if (s_PlutoBatInfo.hFuelGaugeDriver && s_PlutoBatInfo.hBChargingDriver)
        return &s_PlutoBatInfo;

    s_PlutoBatInfo.hBChargingDriver = Max77665aDriverOpen(hDevice);
    if (!s_PlutoBatInfo.hBChargingDriver)
    {
         NvOdmOsPrintf("%s(): Error in call Max77665aDriverOpen()\n");
         return NULL;
    }

    s_PlutoBatInfo.hFuelGaugeDriver = Max17047FuelGaugeDriverOpen(hDevice);
    if (!s_PlutoBatInfo.hFuelGaugeDriver)
    {
        NvOdmOsPrintf("%s(): Error in call Max17047FuelGaugeDriverOpen()\n");
        return NULL;
    }

    return &s_PlutoBatInfo;
}

void
PlutoPmuBatteryRelease(void)
{
    Max17047FuelGaugeDriverClose(s_PlutoBatInfo.hFuelGaugeDriver);
    s_PlutoBatInfo.hFuelGaugeDriver = NULL;

    Max77665aDriverClose(s_PlutoBatInfo.hBChargingDriver);
    s_PlutoBatInfo.hBChargingDriver = NULL;
}

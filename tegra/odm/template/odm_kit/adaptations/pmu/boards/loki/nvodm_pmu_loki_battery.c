/*
 * Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file
 * <b>NVIDIA Tegra ODM Kit: Implementation of public API of
 *                          Loki pmu driver interface.</b>
 *
 * @b Description: Implementation of the battery driver api of
                   public interface for the pmu driver for Loki.
 *
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_loki.h"
#include "loki_pmu.h"
#include "nvassert.h"
#include "bq27441/nvodm_pmu_bq27441_fuel_gauge_driver.h"
#include "lc709203f/nvodm_pmu_lc709203f_fuel_gauge_driver.h"
#include "bq2419x/nvodm_pmu_bq2419x_batterycharger_driver.h"
#include "nvodm_charging.h"

typedef struct LokiBatDriverInfoRec
{
    NvOdmBatChargingDriverInfoHandle hBChargingDriver;
    NvOdmFuelGaugeDriverInfoHandle hFuelGaugeDriver;
} LokiBatDriverInfo;

static LokiBatDriverInfo s_LokiBatInfo = {NULL, NULL};

static NvBool LokiFgReset(void);

NvBool
LokiPmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(pStatus);

    if(!hDevice || !pStatus)
        return NV_FALSE;
    // NOTE: Not implemented completely as it is only for bootloader.

    *pStatus = NvOdmPmuAcLine_Online;
    return NV_TRUE;
}

static NvBool LokiFgReset(void)
{
    /* Not Implemented yet */
    return NV_TRUE;
}

void LokiPmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
    /* Not Implemented yet */
}

NvBool
LokiPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus)
{
    NvU32 Soc = 0;
    NvBool ret;
    NvU32 voltage;

    NV_ASSERT(hDevice);
    NV_ASSERT(pStatus);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    if(!hDevice || !pStatus)
        return NV_FALSE;
    if(batteryInst >= NvOdmPmuBatteryInst_Num)
        return NV_FALSE;

    if (batteryInst == NvOdmPmuBatteryInst_Main)
    {
        // Start with Lc709203f FuelGauge. if its not present, try Bq27441
        ret = Lc709203fFuelGaugeDriverReadSoc(&Soc, &voltage);
        if (!ret)
            ret = Bq27441FuelGaugeDriverReadSoc(&Soc, &voltage);
        if (!ret)
        {
            *pStatus = NVODM_BATTERY_STATUS_UNKNOWN;
            NvOdmOsPrintf("Fuel guage Soc read failed \n");
            return NV_FALSE;
        }

        if (voltage > BQ27441_BATTERY_LOW)  // modify these parameters on need
        {
            *pStatus |= NVODM_BATTERY_STATUS_HIGH;
        }
        else if ((voltage <= BQ27441_BATTERY_LOW)&&
                 (voltage > BQ27441_BATTERY_CRITICAL))
            *pStatus |= NVODM_BATTERY_STATUS_LOW;
        else
            *pStatus |= NVODM_BATTERY_STATUS_CRITICAL;
    }
    else
    {
        *pStatus = NVODM_BATTERY_STATUS_UNKNOWN;
    }
    return NV_TRUE;
}

NvBool
LokiPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData)
{
    NvOdmPmuBatteryData batteryData;
    NvU32 soc = 0;
    NvU32 voltage = 0;
    NvBool ret;

    NV_ASSERT(hDevice);
    NV_ASSERT(pData);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    if(!hDevice || !pData)
        return NV_FALSE;
    if(batteryInst >= NvOdmPmuBatteryInst_Num)
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

    // Start with Lc709203f FuelGauge. if its not present, try Bq27441
    ret = Lc709203fFuelGaugeDriverReadSoc(&soc, &voltage);
    if (!ret)
    {
        ret = Bq27441FuelGaugeDriverReadSoc(&soc, &voltage);
    }
    if (!ret)
    {
        NvOdmOsPrintf("Fuel guage Soc read failed \n");
        return NV_FALSE;
    }
    batteryData.batterySoc = soc;
    batteryData.batteryVoltage = voltage;

#ifdef READ_BATTERY_SOC
    NvOdmOsPrintf("batteryData.batterySoc :%d percentage\n", \
                     batteryData.batterySoc);
#else
    NvOdmOsPrintf("batteryData.batteryVoltage :%d mvolts\n", \
                     batteryData.batteryVoltage);
#endif
    *pData = batteryData;
    return NV_TRUE;
}

void
LokiPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    *pLifeTime = NVODM_BATTERY_DATA_UNKNOWN;
}

void
LokiPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    *pChemistry = NvOdmPmuBatteryChemistry_LION;
}


NvBool
LokiPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NvBool ret;

    ret = BQ2419xBatChargingDriverSetCurrent(s_LokiBatInfo.hBChargingDriver, chargingPath,
                                            chargingCurrentLimitMa, ChargerType);
    return ret;
}

void
*LokiPmuBatterySetup(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    NvBool ret;
    NvU32 version = 0;

    NV_ASSERT(hDevice);

    if(!hDevice)
         return NULL;

    if (s_LokiBatInfo.hFuelGaugeDriver && s_LokiBatInfo.hBChargingDriver)
         return &s_LokiBatInfo;

    s_LokiBatInfo.hBChargingDriver = BQ2419xDriverOpen(hDevice);
    if (!s_LokiBatInfo.hBChargingDriver)
    {
          NvOdmOsPrintf("%s(): Error in call BQ2419xDriverOpen()\n", __func__);
          s_LokiBatInfo.hBChargingDriver = NULL;
          return NULL;
    }
    // Start with Lc709203f FuelGauge. if its not present, try Bq27441
    s_LokiBatInfo.hFuelGaugeDriver = Lc709203fFuelGaugeDriverOpen(hDevice);
    if (!s_LokiBatInfo.hFuelGaugeDriver)
    {
          NvOdmOsPrintf("%s(): Error in call \
                   Lc709203fFuelGaugeDriverOpen()\n", __func__);
          // Fall through to try and open Bq27441FuelGaugeDriver
    }
    else
    {
          ret = Lc709203fFuelGaugeDriverReadIcVersion(&version);
          // if we are able to read version, then Lc709203f is present
          // return data here
          if(ret == NV_TRUE)
              return &s_LokiBatInfo;
          else
          {
              s_LokiBatInfo.hFuelGaugeDriver = NULL;
          }
    }

    s_LokiBatInfo.hFuelGaugeDriver = Bq27441FuelGaugeDriverOpen(hDevice);
    if (!s_LokiBatInfo.hFuelGaugeDriver)
    {
          NvOdmOsPrintf("%s(): Error in call BQ27441FuelGaugeDriverOpen()\n", __func__);
          BQ2419xDriverClose(s_LokiBatInfo.hBChargingDriver);
          return NULL;
    }

   return &s_LokiBatInfo;
}

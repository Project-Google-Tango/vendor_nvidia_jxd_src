/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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
 *                          Ardbeg pmu driver interface.</b>
 *
 * @b Description: Implementation of the battery driver api of
                   public interface for the pmu driver for Ardbeg.
 *
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_ardbeg.h"
#include "ardbeg_pmu.h"
#include "nvassert.h"
#include "max17048/nvodm_pmu_max17048_fuel_gauge_driver.h"
#include "bq24z45/nvodm_pmu_bq24z45_fuel_gauge_driver.h"
#include "cw2015/nvodm_pmu_cw2015_fuel_gauge_driver.h"
#include "bq2419x/nvodm_pmu_bq2419x_batterycharger_driver.h"
#include "bq2477x/nvodm_pmu_bq2477x_batterycharger_driver.h"
#include "lc709203f/nvodm_pmu_lc709203f_fuel_gauge_driver.h"
#include "nvodm_charging.h"

typedef struct ArdbegBatDriverInfoRec
{
    NvOdmBatChargingDriverInfoHandle hBChargingDriver;
    NvOdmFuelGaugeDriverInfoHandle hFuelGaugeDriver;
} ArdbegBatDriverInfo;

static ArdbegBatDriverInfo s_ArdbegBatInfo = {NULL, NULL};
static NvU32 s_pmuBoardId = 0x0;

NvBool
ArdbegPmuGetAcLineStatus(
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

void ArdbegPmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
    /* Not Implemented yet */
}

NvBool
ArdbegPmuGetBatteryStatus(
        NvOdmPmuDeviceHandle hDevice,
        NvOdmPmuBatteryInstance batteryInst,
        NvU8 *pStatus)
{
    NvU32 Soc = 0;
    NvBool ret = NV_FALSE;
    NvU32 voltage = 0;

    NV_ASSERT(hDevice);
    NV_ASSERT(pStatus);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    if(!hDevice || !pStatus)
        return NV_FALSE;
    if(batteryInst >= NvOdmPmuBatteryInst_Num)
        return NV_FALSE;

    if (batteryInst == NvOdmPmuBatteryInst_Main)
    {
        if(s_pmuBoardId == PMU_E1735)
            ret = Bq24z45FuelGaugeDriverReadSoc(&Soc, &voltage);
        else if ((s_pmuBoardId == PMU_E1736)||(s_pmuBoardId == PMU_E1761)||(s_pmuBoardId == PMU_E1936)||(s_pmuBoardId == PMU_E1769))
        {
        
			NvOdmOsPrintf(" init fulel gaule lc70923 \n");
            ret = Lc709203fFuelGaugeDriverReadSoc(&Soc, &voltage);
            if (!ret)
                ret = Max17048FuelGaugeDriverReadSoc(&Soc, &voltage);
        }


        if (!ret)
        {
            *pStatus = NVODM_BATTERY_STATUS_UNKNOWN;
            NvOdmOsPrintf("Fuel guage Soc read failed \n");
            return NV_FALSE;
        }

        if(s_pmuBoardId == PMU_E1735)
        {
            if (Soc > BQ24Z45_BATTERY_LOW)  // modify these parameters on need
            {
                *pStatus |= NVODM_BATTERY_STATUS_HIGH;
            }
            else if ((Soc <= BQ24Z45_BATTERY_LOW)&&
                    (Soc > BQ24Z45_BATTERY_CRITICAL))
                *pStatus |= NVODM_BATTERY_STATUS_LOW;
            else
                *pStatus |= NVODM_BATTERY_STATUS_CRITICAL;
        }
        else if ((s_pmuBoardId == PMU_E1736)||(s_pmuBoardId == PMU_E1761)||(s_pmuBoardId == PMU_E1936))
        {
            if (Soc > MAX17048_BATTERY_LOW)  // modify these parameters on need
            {
                *pStatus |= NVODM_BATTERY_STATUS_HIGH;
            }
            else if ((Soc <= MAX17048_BATTERY_LOW)&&
                    (Soc > MAX17048_BATTERY_CRITICAL))
                *pStatus |= NVODM_BATTERY_STATUS_LOW;
            else
                *pStatus |= NVODM_BATTERY_STATUS_CRITICAL;
        }
        else if(s_pmuBoardId == PMU_E1769)
        {
            if (Soc > CW2015_BATTERY_LOW)  //modify these parameters on need
            {
                *pStatus |= NVODM_BATTERY_STATUS_HIGH;
            }
            else if ((Soc <= CW2015_BATTERY_LOW)&&
                    (Soc > CW2015_BATTERY_CRITICAL))
                *pStatus |= NVODM_BATTERY_STATUS_LOW;
            else
                *pStatus |= NVODM_BATTERY_STATUS_CRITICAL;
        }
    }
    else
    {
        *pStatus = NVODM_BATTERY_STATUS_UNKNOWN;
    }
    return NV_TRUE;
}

NvBool
ArdbegPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData)
{
    NvOdmPmuBatteryData batteryData;
    NvU32 Soc = 0;
    NvU32 voltage = 0;
    NvBool ret = NV_FALSE;

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

    if(s_pmuBoardId == PMU_E1735)
        ret = Bq24z45FuelGaugeDriverReadSoc(&Soc, &voltage);
    else if ((s_pmuBoardId == PMU_E1736)||(s_pmuBoardId == PMU_E1761)||(s_pmuBoardId == PMU_E1936)||(s_pmuBoardId == PMU_E1769))
    {
        ret = Lc709203fFuelGaugeDriverReadSoc(&Soc, &voltage);
        if (!ret)
            ret = Max17048FuelGaugeDriverReadSoc(&Soc, &voltage);
    }


    if (!ret)
    {
        NvOdmOsPrintf("Fuel guage Soc read failed \n");
        return NV_FALSE;
    }
    batteryData.batterySoc = Soc;
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
ArdbegPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    *pLifeTime = NVODM_BATTERY_DATA_UNKNOWN;
}

void
ArdbegPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(batteryInst < NvOdmPmuBatteryInst_Num);

    *pChemistry = NvOdmPmuBatteryChemistry_LION;
}

NvBool
ArdbegPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NvBool ret = NV_FALSE;

    if(s_pmuBoardId == PMU_E1735)
    {
        ret = BQ2477xBatChargingDriverSetCurrent(s_ArdbegBatInfo.hBChargingDriver, chargingPath,
                                            chargingCurrentLimitMa, ChargerType);
    }
    else if(s_pmuBoardId == PMU_E1736 ||
            s_pmuBoardId == PMU_E1761 ||
            s_pmuBoardId == PMU_E1936 ||
            s_pmuBoardId == PMU_E1769)
        ret = BQ2419xBatChargingDriverSetCurrent(s_ArdbegBatInfo.hBChargingDriver, chargingPath,
                                            chargingCurrentLimitMa, ChargerType);

    return ret;
}

void
*ArdbegPmuBatterySetup(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    NvBool ret = NV_FALSE;
    NvU32 version = 0;
    NV_ASSERT(hDevice);

    if(!hDevice)
         return NULL;

    if (s_ArdbegBatInfo.hFuelGaugeDriver && s_ArdbegBatInfo.hBChargingDriver)
         return &s_ArdbegBatInfo;

    s_pmuBoardId = pPmuBoard->BoardID;

    switch (s_pmuBoardId)
    {
        case PMU_E1735:
            s_ArdbegBatInfo.hBChargingDriver = BQ2477xDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hBChargingDriver)
            {
                NvOdmOsPrintf("%s(): Error in call BQ2477xDriverOpen()\n", __func__);
                s_ArdbegBatInfo.hBChargingDriver = NULL;
                return NULL;
            }
            s_ArdbegBatInfo.hFuelGaugeDriver = Bq24z45FuelGaugeDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hFuelGaugeDriver)
            {
                NvOdmOsPrintf("%s(): Error in call Bq24z45FuelGaugeDriverOpen()\n", __func__);
                BQ2477xDriverClose(s_ArdbegBatInfo.hBChargingDriver);
                return NULL;
            }
            break;
        case PMU_E1736:
        case PMU_E1936:
        case PMU_E1761:
		case PMU_E1769:
            s_ArdbegBatInfo.hBChargingDriver = BQ2419xDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hBChargingDriver)
            {
                NvOdmOsPrintf("%s(): Error in call BQ2419xDriverOpen()\n", __func__);
                s_ArdbegBatInfo.hBChargingDriver = NULL;
                return NULL;
            }

            // Start with Lc709203f(OnSemi) FuelGauge. if its not present, try max17048
            s_ArdbegBatInfo.hFuelGaugeDriver = Lc709203fFuelGaugeDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hFuelGaugeDriver)
            {
                NvOdmOsPrintf("%s(): Error in call \
                        Lc709203fFuelGaugeDriverOpen()\n", __func__);
                // Fall through to try and open Bq27441FuelGaugeDriver
            }
            else
            {
                ret = Lc709203fFuelGaugeDriverReadIcVersion(&version);
                NvOdmOsPrintf("Lc7009203f(OnSemi) Ic Version = %d\n", version);
                // if we are able to read version, then Lc709203f is present
                // return data here
                if(ret == NV_TRUE)
                    return &s_ArdbegBatInfo;
                else
                {
                    Lc709203fFuelGaugeDriverClose();
                    s_ArdbegBatInfo.hFuelGaugeDriver = NULL;
                }
            }
            s_ArdbegBatInfo.hFuelGaugeDriver = Max17048FuelGaugeDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hFuelGaugeDriver)
            {
                NvOdmOsPrintf("%s(): Error in call MAX17048FuelGaugeDriverOpen()\n", __func__);
                BQ2419xDriverClose(s_ArdbegBatInfo.hBChargingDriver);
                return NULL;
            }
            break;
        default:
            s_ArdbegBatInfo.hBChargingDriver = BQ2419xDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hBChargingDriver)
            {
                NvOdmOsPrintf("%s(): Error in call BQ2419xDriverOpen()\n", __func__);
                s_ArdbegBatInfo.hBChargingDriver = NULL;
                return NULL;
            }
            s_ArdbegBatInfo.hFuelGaugeDriver = CW2015FuelGaugeDriverOpen(hDevice);
            if (!s_ArdbegBatInfo.hFuelGaugeDriver)
            {
                NvOdmOsPrintf("%s(): Error in call FuelGaugeDriverOpen()\n", __func__);
                BQ2419xDriverClose(s_ArdbegBatInfo.hBChargingDriver);
                return NULL;
            }
            break;

    }
    return &s_ArdbegBatInfo;
}

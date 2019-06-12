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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
  *
  * @b Description: Implements the TPS8003x battery charging drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_tps8003x_battery_charging_driver.h"
#include "tps8003x_core_driver.h"


#define Tps80031x_PWDNSTATUS1 0x93
#define Tps80031x_CHARGERUSB_VICHRG_PC 0xDD
#define Tps80031x_CONTROLLER_CTRL1 0xE1
#define Tps80031x_CONTROLLER_WDG 0xE2
#define Tps80031x_CONTROLLER_STAT1 0XE3
#define Tps80031x_CHARGERUSB_VOREG 0xEC
#define Tps80031x_CHARGERUSB_VICHRG 0xED
#define Tps80031x_CHARGERUSB_CTRLLIMIT1 0xEF
#define Tps80031x_CHARGERUSB_CINLIMIT 0xEE
#define Tps80031x_CHARGERUSB_CTRLLIMIT2 0xF0

#define DUMB_CHARGER_LIMIT_MA      250
#define DEDICATED_CHARGER_LIMIT_MA 1000   // 1000mA for dedicated charger


NvOdmBatChargingDriverInfoHandle
Tps8003xBatChargingDriverOpen(
    NvOdmPmuDeviceHandle hDevice,
    OdmBatteryChargingProps *pBChargingProps)
{
    Tps8003xCoreDriverHandle hTps8003xCore = NULL;

    hTps8003xCore = Tps8003xCoreDriverOpen(hDevice);
    if (!hTps8003xCore)
    {
        NvOdmOsPrintf("%s: Error Open core driver.\n", __func__);
        return NULL;
    }
    return hTps8003xCore;
}

void Tps8003xBatChargingDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Tps8003xCoreDriverClose(hBChargingDriver);
}

NvBool
Tps8003xBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NvU32 CurrentLimit = 0;
    NvU32 CurrentReg = 0;
    NvBool ret;
    Tps8003xCoreDriverHandle hTps8003xCore =  (struct Tps8003xCoreDriverRec*) hBChargingDriverInfo;

    NV_ASSERT(hTps8003xCore);

    if (chargingCurrentLimitMa == 0)
    {
        NvOdmOsPrintf("MICROBOOT_MILLIVOLT_MIN reached, Disabling further charging ");
        ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CONTROLLER_CTRL1, 0x00);   //Disable the charging
        if (!ret)
        {
            NvOdmOsPrintf("%s(): Error in disabling  the charging\n", __func__);
            return NV_FALSE;
        }
        return NV_TRUE;
    }

    if (chargingCurrentLimitMa > DEDICATED_CHARGER_LIMIT_MA)
        chargingCurrentLimitMa = DEDICATED_CHARGER_LIMIT_MA;

    if (chargingPath == NvOdmPmuChargingPath_UsbBus)
    {
        if (ChargerType == NvOdmUsbChargerType_UsbHost)
        {
            CurrentReg = (chargingCurrentLimitMa / 100) - 1;
            CurrentLimit = 1 + (CurrentReg << 1);
        }
        else
        {
            switch (ChargerType)
            {
            case NvOdmUsbChargerType_SJ:
            case NvOdmUsbChargerType_SK:
            case NvOdmUsbChargerType_SE1:
            case NvOdmUsbChargerType_SE0:
                chargingCurrentLimitMa = DEDICATED_CHARGER_LIMIT_MA;
                CurrentLimit = 0x29;
                CurrentReg = 0x9;
                break;
            case NvOdmUsbChargerType_Dummy:
            default:
                chargingCurrentLimitMa = DUMB_CHARGER_LIMIT_MA;
                CurrentLimit = 0x04;
                CurrentReg = 0x01;
                break;
            }
        }
    }

    //Max Charge current limit
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_CTRLLIMIT2, 0x09);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Max charging current Limit\n", __func__);
        return NV_FALSE;
    }
    //Max System supply/battery regulation voltage limit
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_CTRLLIMIT1, 0x22);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the MaxSystem supply/battery regulation voltage limit\n", __func__);
        return NV_FALSE;
    }

    //Lock the above
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_CTRLLIMIT2, 0x19);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in locking  the Max charging current and voltage\n", __func__);
        return NV_FALSE;
    }

    //Set Pre-Charge current to 400mA
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_VICHRG_PC, 0x03);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Pre-charge current Limit\n", __func__);
        return NV_FALSE;
    }

    //Enable charging
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CONTROLLER_CTRL1, 0x30);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in Enabling charging \n", __func__);
        return NV_FALSE;
    }

    //Set Charging current  in full charge mode
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_VICHRG, CurrentReg);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the charging current \n", __func__);
        return NV_FALSE;
    }

    //Battery Regulation voltage
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_VOREG, 0x1d);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the charging voltage\n", __func__);
        return NV_FALSE;
    }

    // Current limitation on VBUS
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CHARGERUSB_CINLIMIT, CurrentLimit);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Max charging current Limit on VBUS\n", __func__);
        return NV_FALSE;
    }

    //Set Watch Dog
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CONTROLLER_WDG, 0x7f);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the WatchDog\n", __func__);
        return NV_FALSE;
    }

    return NV_TRUE;
}


NvBool
Tps8003xBatChargingDriverUpdateWatchDog(NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo, NvU32 value)
{
    Tps8003xCoreDriverHandle hTps8003xCore =  (struct Tps8003xCoreDriverRec*) hBChargingDriverInfo;
    NvBool ret;

    //Reset Watch Dog
    ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CONTROLLER_WDG, value);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the WatchDog\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

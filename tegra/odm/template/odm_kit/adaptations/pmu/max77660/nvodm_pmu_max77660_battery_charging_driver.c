/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU MAX77660-driver Interface</b>
  *
  * @b Description: Implements the MAX77660 battery charging drivers.
  *
  */


#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_max77660_battery_charging_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define MAX77660_PMUGUID NV_ODM_GUID('m','a','x','7','7','6','6','0')
#define MAX77660_I2C_SPEED 100
#define USB_CHARGER_LIMIT_MA 500
#define NONCOMPATIBLE_CHARGER_LIMIT_MA 1500
#define COMPATIBLE_CHARGER_LIMIT_MA 2000
#define DET_BAT 0x03

enum {
    slave_pmu = 0,
    slave_charger
};

typedef struct Max77660CoreDriverRec {
    NvBool Init;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr[2];
} Max77660CoreDriver;

static Max77660CoreDriver s_Max77660CoreDriver = {NV_FALSE, NULL, {0,0}};

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(MAX77660_PMUGUID);
    NV_ASSERT(pI2cModule);
    NV_ASSERT(pI2cInstance);
    NV_ASSERT(pI2cAddress);
    if (!pI2cModule || !pI2cInstance || !pI2cAddress)
        return NV_FALSE;

    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
            {
                *pI2cModule   = NvOdmIoModule_I2c;
                *pI2cInstance = pConnectivity->AddressList[i].Instance;
                *pI2cAddress  = pConnectivity->AddressList[i].Address;
            }
            pI2cAddress++;
            pI2cInstance++;
        }
        return NV_TRUE;
    }
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmBatChargingDriverInfoHandle
Max77660BatChargingDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance[2] = {0,0};
    NvU32 I2cAddress[2]  = {0,0};

    Max77660CoreDriverHandle hMax77660Core = &s_Max77660CoreDriver;

    if (!hMax77660Core->Init)
    {
        Status = GetInterfaceDetail(&I2cModule, I2cInstance, I2cAddress);
        if (!Status)
            return NULL;

        hMax77660Core->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance[slave_charger]);
        if (!hMax77660Core->hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
            return NULL;
        }
        hMax77660Core->DeviceAddr[slave_pmu] = I2cAddress[slave_pmu];
        hMax77660Core->DeviceAddr[slave_charger] = I2cAddress[slave_charger];
    }
    hMax77660Core->Init = NV_TRUE;

    return hMax77660Core;
}

void Max77660BatChargingDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Max77660CoreDriverHandle hMax77660Core =
                               (Max77660CoreDriverHandle)hBChargingDriver;
    NV_ASSERT(hMax77660Core);
    if (!hMax77660Core)
    {
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);
        return;
    }

    hMax77660Core->Init = NV_FALSE;

    NvOdmPmuI2cClose(hMax77660Core->hOdmPmuI2c);

}

static NvBool
Max77660IsBatteryPresent(Max77660CoreDriverHandle hMax77660Core)
{
    NvBool ret;
    NvU8 value = 0;

    NV_ASSERT(hMax77660Core);
    if (!hMax77660Core)
    {
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);
        return NV_FALSE;
    }

    ret = NvOdmPmuI2cRead8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                              MAX77660_I2C_SPEED, MAX77660_DETAILS1, &value);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error Reading MAX77660_DETAILS1 register\n", __func__);
        return NV_FALSE;
    }

    if ((value & DET_BAT) == 3)
    {
        NvOdmOsPrintf("%s():Battery Not Detected ", __func__);
        return NV_FALSE;
    }
    else
    {
        NvOdmOsPrintf("%s(): Battery Detected\n", __func__);
        return NV_TRUE;
    }
}

static NvBool
Max77660DisableCharging(Max77660CoreDriverHandle hMax77660Core)
{
     NvBool ret;

     NV_ASSERT(hMax77660Core);
     if (!hMax77660Core)
     {
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);
        return NV_FALSE;
     }

     //Disable Top level charging
     ret = NvOdmPmuI2cSetBits(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_pmu],
                          MAX77660_I2C_SPEED, MAX77660_GLBLCNFG1, 0x80);
     if (!ret)
     {
         NvOdmOsPrintf("%s(): Error in disabling charging \n", __func__);
         return NV_FALSE;
     }

     //Clear CEN
     ret = NvOdmPmuI2cClrBits(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                              MAX77660_I2C_SPEED, MAX77660_CHGCTRL2, 0x10);
     if (!ret)
     {
         NvOdmOsPrintf("%s(): Error in disabling charging \n", __func__);
         return NV_FALSE;
     }
     return NV_TRUE;
}

NvBool
Max77660BatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NvU32 CurrentLimit = 0;
    NvU32 FastChargeCurrent = 0;
    NvBool ret;

    Max77660CoreDriverHandle hMax77660Core =  (struct Max77660CoreDriverRec*) hBChargingDriverInfo;

    NV_ASSERT(hMax77660Core);
    if (!hMax77660Core)
    {
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);
        return NV_FALSE;
    }

    if (chargingCurrentLimitMa == 0)
    {
        NvOdmOsPrintf("MICROBOOT_MILLIVOLT_MIN reached, Disabling further charging ");
        if (!Max77660DisableCharging(hMax77660Core))
        {
            NvOdmOsPrintf("%s(): Error in disabling  the charging\n", __func__);
            return NV_FALSE;
        }
        return NV_TRUE;
    }

    if (Max77660IsBatteryPresent(hMax77660Core))
    {
        if (ChargerType == NvOdmUsbChargerType_UsbHost)
        {
            if (chargingCurrentLimitMa > USB_CHARGER_LIMIT_MA)
                chargingCurrentLimitMa = USB_CHARGER_LIMIT_MA;//500mA
        }
        else if ((ChargerType == NvOdmUsbChargerType_CDP)
                                       || (ChargerType == NvOdmUsbChargerType_DCP))
        {
            chargingCurrentLimitMa = NONCOMPATIBLE_CHARGER_LIMIT_MA;//1500mA
        }
        else if (ChargerType == NvOdmUsbChargerType_Proprietary)
        {
            chargingCurrentLimitMa = COMPATIBLE_CHARGER_LIMIT_MA;//2000mA
        }
        else
        {
            chargingCurrentLimitMa = USB_CHARGER_LIMIT_MA;//500mA default
        }
    }
    else
    {
        // Disable Charging since there is no battery detected
        NvOdmOsPrintf("%s(): Disabling charging since battery not detected\n", __func__);

        if (!Max77660DisableCharging(hMax77660Core))
        {
            NvOdmOsPrintf("%s(): Error in disabling  the charging\n", __func__);
            return NV_FALSE;
        }
        return NV_TRUE;
    }

    if (chargingCurrentLimitMa >= 275)
        CurrentLimit = 0x80 + (((chargingCurrentLimitMa - 275)/25) + 3);
    else
        CurrentLimit = 0x80; //corresponds to 100mA
    if (chargingCurrentLimitMa >= 250)
        FastChargeCurrent = 0x40 + (((chargingCurrentLimitMa - 250)/50) + 1);
    else
        FastChargeCurrent = 0x41; //have default of 200mA

    //Unclock RegisterAccess
    ret = NvOdmPmuI2cWrite8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                                   MAX77660_I2C_SPEED, MAX77660_CHGCTRL1, 0x0F);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in Unlocking Register Access \n", __func__);
        return NV_FALSE;
    }

    //Enable Top level charging
    ret = NvOdmPmuI2cSetBits(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_pmu],
                 MAX77660_I2C_SPEED, MAX77660_GLBLCNFG1, 0x02);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in enabling charging \n", __func__);
        return NV_FALSE;
    }

    //Set Fast Charge Current
    ret = NvOdmPmuI2cWrite8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                      MAX77660_I2C_SPEED, MAX77660_FCHGCRNT, FastChargeCurrent);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Fast Charge Current \n", __func__);
        return NV_FALSE;
    }

    //Set Battery charge Termination Voltage in mV
    ret =  NvOdmPmuI2cWrite8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                           MAX77660_I2C_SPEED,MAX77660_BATREGCTRL, 0x16);//set 4.2v
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error setting the Charge Termination Voltage\n", __func__);
        return NV_FALSE;
    }

    //Set Maximun Charge Terimination voltage
    ret =  NvOdmPmuI2cWrite8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                       MAX77660_I2C_SPEED,MAX77660_MBATREGMAX, 0x0B);//set 4.2v
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error setting Max Charge Termination Voltage\n", __func__);
        return NV_FALSE;
    }

    //Set Maximum Input Current Limit in mA
    ret =  NvOdmPmuI2cWrite8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                              MAX77660_I2C_SPEED, MAX77660_DCCRNT, CurrentLimit);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error setting the Max Input current Limit\n", __func__);
        return NV_FALSE;
    }

    //Enable charging/ Set prequal current/ Set system regulation voltage(3.6v)
    ret =  NvOdmPmuI2cWrite8(hMax77660Core->hOdmPmuI2c, hMax77660Core->DeviceAddr[slave_charger],
                                MAX77660_I2C_SPEED, MAX77660_CHGCTRL2, 0xF6);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error setting the Max Input current Limit\n", __func__);
        return NV_FALSE;
    }

    return NV_TRUE;
}

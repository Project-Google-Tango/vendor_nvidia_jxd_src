/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_query_discovery.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_max77665a_pmu_driver.h"

#define MAX77665A_I2C_SPEED 100
#define USB_CHARGER_LIMIT_MA 500
#define NONCOMPATIBLE_CHARGER_LIMIT_MA 1500
#define COMPATIBLE_CHARGER_LIMIT_MA 2000

#define MAX77665A_PMUGUID NV_ODM_GUID('m','a','x','7','7','6','6','5')

static NvBool initialized = NV_FALSE;

typedef struct Max77665aCoreDriverRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Max77665aCoreDriver;

static Max77665aCoreDriver s_Max77665aCoreDriver = {NULL, 0};

static
NvBool Max77665aRegisterWrite8(
    Max77665aCoreDriverHandle hMax77665aCore,
    NvU8 RegAddr,
    NvU8 Data)
{
    return NvOdmPmuI2cWrite8(hMax77665aCore->hOdmPmuI2c,
                hMax77665aCore->DeviceAddr,
                MAX77665A_I2C_SPEED, RegAddr, Data);
}

static
NvBool Max77665aRegisterRead8(
    Max77665aCoreDriverHandle hMax77665aCore,
    NvU8 RegAddr,
    NvU8 *Data)
{
    return NvOdmPmuI2cRead8(hMax77665aCore->hOdmPmuI2c,
                hMax77665aCore->DeviceAddr,
                MAX77665A_I2C_SPEED, RegAddr, Data);
}

static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(MAX77665A_PMUGUID);
    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
            {
                *pI2cModule   = NvOdmIoModule_I2c;
                *pI2cInstance = pConnectivity->AddressList[i].Instance;
                *pI2cAddress  = pConnectivity->AddressList[i].Address;
                return NV_TRUE;
            }
        }
    }
    NvOdmOsPrintf("%s: The system did not discover PMU-CHARGER from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

static NvBool
Max77665IsBatteryPresent(Max77665aCoreDriverHandle hMax77665aCore)
{
    NvBool ret;
    NvU8 value = 0;

    NV_ASSERT(hMax77665aCore);
    if (!hMax77665aCore)
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);

    ret = Max77665aRegisterRead8(hMax77665aCore, MAX77665_CHG_DTLS_01, &value);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in Reading the register\n", __func__);
        return NV_FALSE;
    }

    if (!(value & LEFT_MASK))
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

NvOdmBatChargingDriverInfoHandle
Max77665aDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    Max77665aCoreDriverHandle hMax77665aCore = NULL;
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    hMax77665aCore = &s_Max77665aCoreDriver;

    //Avoid opening the driver twice
    if (initialized)
        return hMax77665aCore;

    Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Status)
        return NULL;

    hMax77665aCore->hOdmPmuI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, I2cInstance);
    if (!hMax77665aCore->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        return NULL;
    }
    hMax77665aCore->DeviceAddr = I2cAddress;

    //enable write access
    Status = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_06, 0x0c);

    if (!Status)
    {
        NvOdmOsPrintf("%s: Error Enable Write Access\n", __func__);
        return NULL;
    }

    //modify OTP setting of input current limit to 100ma
    Status = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_09, 0x05);

    if (!Status)
    {
        NvOdmOsPrintf("%s: Error Setting input current limit\n", __func__);
        return NULL;
    }

    initialized = NV_TRUE;
    return hMax77665aCore;
}

void Max77665aDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Max77665aCoreDriverHandle hMax77665aCore =
                               (Max77665aCoreDriverHandle)hBChargingDriver;

    NV_ASSERT(hMax77665aCore);
    if (!hMax77665aCore)
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);

    initialized = NV_FALSE;

    NvOdmPmuI2cClose(hMax77665aCore->hOdmPmuI2c);
}

NvBool
Max77665aDisableBatteryCharging(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Max77665aCoreDriverHandle hMax77665aCore =
                               (Max77665aCoreDriverHandle)hBChargingDriver;
    NvBool status;

    NV_ASSERT(hMax77665aCore);
    if (!hMax77665aCore)
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);

    //disable charging
    status = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_00, 0x00);
    if (!status)
        NvOdmOsPrintf("%s: Error Disable charging. \n", __func__);

    //disable write acess
    status = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_06, 0x00);
    if (!status)
        NvOdmOsPrintf("%s: Error Disable write. \n", __func__);

    return status;
}

NvBool
Max77665aBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    Max77665aCoreDriverHandle hMax77665aCore =
                          (Max77665aCoreDriverHandle)hBChargingDriverInfo;
    NvBool ret;
    NvU32 val_fcc = 0;
    NvU32 val_cmax = 0;

    NV_ASSERT(hMax77665aCore);
    if (!hMax77665aCore)
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);

    if (chargingCurrentLimitMa == 0)
    {
        NvOdmOsPrintf("CHARGING CURRENT ZERO, Disable further charging\n ");
        ret = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_00, 0x00);
        if (!ret)
        {
            NvOdmOsPrintf("%s(): Error in disabling  the charging\n", __func__);
            return NV_FALSE;
        }
        return NV_TRUE;
    }

    if (Max77665IsBatteryPresent(hMax77665aCore))
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
        ret = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_00, 0x00);
        if (!ret)
            NvOdmOsPrintf("%s(): Error in disabling charging \n", __func__);

        return NV_FALSE;
    }

    val_fcc = (chargingCurrentLimitMa/33) - 1;
    val_cmax = chargingCurrentLimitMa/20;

    //Enable charging
    ret = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_00, 0x05);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in Enabling charging \n", __func__);
        return NV_FALSE;
    }

    //Set Fast Charge Current in mA
    ret = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_02, val_fcc);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Fast Charge Current \n", __func__);
        return NV_FALSE;
    }

    //Set Battery charge Termination Voltage in mV
    ret = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_04, 0x16);//set 4.2v
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Charge Termination Voltage\n", __func__);
        return NV_FALSE;
    }

    //Set Maximum Input Current Limit in mA
    ret = Max77665aRegisterWrite8(hMax77665aCore, MAX77665_CHG_CNFG_09, val_cmax);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in setting the Max Input current Limit\n", __func__);
        return NV_FALSE;
    }

    return NV_TRUE;
}

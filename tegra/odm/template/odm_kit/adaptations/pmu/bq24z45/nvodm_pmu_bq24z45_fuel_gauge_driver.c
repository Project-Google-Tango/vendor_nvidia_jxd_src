/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: BQ24Z45-driver Interface</b>
  *
  * @b Description: Implements the BQ24Z45 gas guage drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_query_gpio.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_bq24z45_fuel_gauge_driver.h"
#include "nvodm_services.h"

#define BQ24Z45_PMUGUID NV_ODM_GUID('b','q','2','4','z','4','5','*')
#define BQ24Z45_I2C_SPEED 100

#define BQ24Z45_VCELL      0x09
#define BQ24Z45_SOC        0x0D

typedef struct Bq24z45DriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Bq24z45CoreDriver;

static Bq24z45CoreDriver s_Bq24z45CoreDriver = {0, NULL, 0};

static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(BQ24Z45_PMUGUID);
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
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmFuelGaugeDriverInfoHandle
Bq24z45FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    if (!s_Bq24z45CoreDriver.OpenCount)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Bq24z45CoreDriver, 0, sizeof(Bq24z45CoreDriver));
        s_Bq24z45CoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);

        if (!s_Bq24z45CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error BQ24Z45 Open I2C device.\n", __func__);
            return NULL;
        }
        s_Bq24z45CoreDriver.DeviceAddr = I2cAddress;

    }
    s_Bq24z45CoreDriver.OpenCount++;
    NvOdmOsPrintf("%s: BQ24Z45 Open successful\n", __func__);

    return &s_Bq24z45CoreDriver;
}

void
Bq24z45FuelGaugeDriverClose(void)
{
    if (!s_Bq24z45CoreDriver.OpenCount)
        return;
    if (s_Bq24z45CoreDriver.OpenCount == 1)
    {
        NvOdmPmuI2cClose(s_Bq24z45CoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_Bq24z45CoreDriver, 0, sizeof(Bq24z45CoreDriver));
        return;
    }
    s_Bq24z45CoreDriver.OpenCount--;
}

static void SetGpio_Pk5(NvBool Enable)
{
    NvOdmGpioPinHandle hOdmPin = NULL;
    NvOdmServicesGpioHandle hOdmGpio = NULL;
    hOdmGpio  = NvOdmGpioOpen();
    hOdmPin = NvOdmGpioAcquirePinHandle(hOdmGpio, 'k' - 'a', 5);
    NvOdmGpioConfig(hOdmGpio, hOdmPin, NvOdmGpioPinMode_Output);
    NvOdmGpioSetState(hOdmGpio, hOdmPin, Enable);
    NvOdmGpioClose(hOdmGpio);
}

NvBool
Bq24z45FuelGaugeDriverReadSoc(NvU32 *soc, NvU32 *voltage)
{
    NvU16 vcell = 0;
    NvU8 capacity = 0;

    NV_ASSERT(soc);
    NV_ASSERT(voltage);
    if (!soc || !voltage)
        return NV_FALSE;

    // Enable SPDIF_OUT before read
    SetGpio_Pk5(NV_TRUE);

    if(!NvOdmPmuI2cRead16(s_Bq24z45CoreDriver.hOdmPmuI2c,
                s_Bq24z45CoreDriver.DeviceAddr,
                BQ24Z45_I2C_SPEED, BQ24Z45_VCELL, &vcell))
    {
        NvOdmOsPrintf("%s: Error BQ24Z45 soc read failed.\n", __func__);
        *voltage = 0;
        return NV_FALSE;
    }
    vcell = (vcell << 8) | (vcell >> 8);
    if(!NvOdmPmuI2cRead8(s_Bq24z45CoreDriver.hOdmPmuI2c,
            s_Bq24z45CoreDriver.DeviceAddr,
            BQ24Z45_I2C_SPEED, BQ24Z45_SOC, &capacity))
    {
        NvOdmOsPrintf("%s: Error BQ24Z45 soc read failed.\n", __func__);
        *soc = 0;
        return NV_FALSE;
    }

    // Disable SPDIF_OUT after read
    SetGpio_Pk5(NV_FALSE);

    *voltage = vcell;
    *soc = capacity;
    return NV_TRUE;
}

NvBool Bq24z45FgReset()
{
    // Not implemented yet
    return NV_TRUE;
}


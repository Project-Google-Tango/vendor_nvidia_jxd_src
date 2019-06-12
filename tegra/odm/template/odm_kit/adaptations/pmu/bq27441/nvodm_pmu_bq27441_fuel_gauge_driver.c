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
  * @file: <b>NVIDIA Tegra ODM Kit: BQ27441-driver Interface</b>
  *
  * @b Description: Implements the BQ27441 gas guage drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_bq27441_fuel_gauge_driver.h"


#define BQ27441_PMUGUID NV_ODM_GUID('b','q','2','7','4','4','1','*')
#define BQ27441_I2C_SPEED 400

#define BQ27441_VCELL      0x04
#define BQ27441_SOC        0x1C
typedef struct Bq27441DriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Bq27441CoreDriver;

static Bq27441CoreDriver s_Bq27441CoreDriver = {0, NULL, 0};

static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(BQ27441_PMUGUID);
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
Bq27441FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    if (!s_Bq27441CoreDriver.OpenCount)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Bq27441CoreDriver, 0, sizeof(Bq27441CoreDriver));
        s_Bq27441CoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);

        if (!s_Bq27441CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error BQ27441 Open I2C device.\n", __func__);
            return NULL;
        }
        s_Bq27441CoreDriver.DeviceAddr = I2cAddress;

    }
    s_Bq27441CoreDriver.OpenCount++;
    return &s_Bq27441CoreDriver;
}

void
Bq27441FuelGaugeDriverClose(void)
{
    if (!s_Bq27441CoreDriver.OpenCount)
        return;
    if (s_Bq27441CoreDriver.OpenCount == 1)
    {
        NvOdmPmuI2cClose(s_Bq27441CoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_Bq27441CoreDriver, 0, sizeof(Bq27441CoreDriver));
        return;
    }
    s_Bq27441CoreDriver.OpenCount--;
}

NvBool
Bq27441FuelGaugeDriverReadSoc(NvU32 *pSoc, NvU32 *pVoltage)
{
    NvU16 vcell = 0;
    NvU8 regval;

    NV_ASSERT(pSoc);
    NV_ASSERT(pVoltage);
    if (!pSoc || !pVoltage)
        return NV_FALSE;

    *pVoltage = 0;
    *pSoc = 0;

    if (!NvOdmPmuI2cRead16(s_Bq27441CoreDriver.hOdmPmuI2c,
                s_Bq27441CoreDriver.DeviceAddr,
                BQ27441_I2C_SPEED, BQ27441_VCELL, &vcell))
    {
        NvOdmOsPrintf("%s: Error BQ27441 voltage read failed.\n", __func__);
        return NV_FALSE;
    }
    *pVoltage = ((vcell & 0x00FF) << 8) | ((vcell & 0xFF00) >> 8);

    if (!NvOdmPmuI2cRead8(s_Bq27441CoreDriver.hOdmPmuI2c,
                s_Bq27441CoreDriver.DeviceAddr,
                BQ27441_I2C_SPEED, BQ27441_SOC, &regval))
    {
        NvOdmOsPrintf("%s: Error BQ27441 soc read failed.\n", __func__);
        return NV_FALSE;
    }
    *pSoc = regval;
    return NV_TRUE;
}

NvBool Bq27441FgReset()
{
    // Not implemented yet
    return NV_TRUE;
}


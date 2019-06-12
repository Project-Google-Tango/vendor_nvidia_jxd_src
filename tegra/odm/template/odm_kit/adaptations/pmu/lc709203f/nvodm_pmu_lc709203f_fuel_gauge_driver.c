/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: LC709203F-driver Interface</b>
  *
  * @b Description: Implements the LC709203F gas guage drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_lc709203f_fuel_gauge_driver.h"


#define LC709203F_PMUGUID NV_ODM_GUID('l','c','7','0','9','2','0','*')
#define LC709203F_I2C_SPEED 400

#define LC709203F_VOLTAGE      0x09
#define LC709203F_RSOC         0x0D
#define LC709203F_VERSION      0x11

typedef struct Lc709203fDriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Lc709203fCoreDriver;

static Lc709203fCoreDriver s_Lc709203fCoreDriver = {0, NULL, 0};

static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(LC709203F_PMUGUID);
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
    NvOdmOsPrintf("%s: The system did not discover LC709203F Fuel Gauge from"
                     " the data base OR the Interface entry is not available\n",
                     __func__);
    return NV_FALSE;
}

NvOdmFuelGaugeDriverInfoHandle
Lc709203fFuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    if (!s_Lc709203fCoreDriver.OpenCount)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Lc709203fCoreDriver, 0, sizeof(Lc709203fCoreDriver));
        s_Lc709203fCoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);

        if (!s_Lc709203fCoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error LC709203F Open I2C device.\n", __func__);
            return NULL;
        }
        s_Lc709203fCoreDriver.DeviceAddr = I2cAddress;

    }
    s_Lc709203fCoreDriver.OpenCount++;
    return &s_Lc709203fCoreDriver;
}

void
Lc709203fFuelGaugeDriverClose(void)
{
    if (!s_Lc709203fCoreDriver.OpenCount)
        return;
    if (s_Lc709203fCoreDriver.OpenCount == 1)
    {
        NvOdmPmuI2cClose(s_Lc709203fCoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_Lc709203fCoreDriver, 0, sizeof(Lc709203fCoreDriver));
        return;
    }
    s_Lc709203fCoreDriver.OpenCount--;
}

NvBool
Lc709203fFuelGaugeDriverReadSoc(NvU32 *pSoc, NvU32 *pVoltage)
{
    NvU16 vcell = 0;
    NvU8 regval;

    NV_ASSERT(pSoc);
    NV_ASSERT(pVoltage);
    if (!pSoc || !pVoltage)
        return NV_FALSE;

    *pVoltage = 0;
    *pSoc = 0;

    if (!s_Lc709203fCoreDriver.OpenCount)
    {
        NvOdmPmuDeviceHandle hDevice = NULL;
        Lc709203fFuelGaugeDriverOpen(hDevice);
    }

    if (!NvOdmPmuI2cRead16(s_Lc709203fCoreDriver.hOdmPmuI2c,
                s_Lc709203fCoreDriver.DeviceAddr,
                LC709203F_I2C_SPEED, LC709203F_VOLTAGE, &vcell))
    {
        NvOdmOsPrintf("%s: Error LC709203F voltage read failed.\n", __func__);
        return NV_FALSE;
    }
    *pVoltage = ((vcell & 0x00FF) << 8) | ((vcell & 0xFF00) >> 8);

    if (!NvOdmPmuI2cRead8(s_Lc709203fCoreDriver.hOdmPmuI2c,
                s_Lc709203fCoreDriver.DeviceAddr,
                LC709203F_I2C_SPEED, LC709203F_RSOC, &regval))
    {
        NvOdmOsPrintf("%s: Error LC709203F soc read failed.\n", __func__);
        return NV_FALSE;
    }
    *pSoc = regval;

    return NV_TRUE;
}

NvBool
Lc709203fFuelGaugeDriverReadIcVersion(NvU32 *pVersion)
{
    NvU8 regval;

    NV_ASSERT(pVersion);
    if (!pVersion)
        return NV_FALSE;

    *pVersion = 0;

    if (!NvOdmPmuI2cRead8(s_Lc709203fCoreDriver.hOdmPmuI2c,
                s_Lc709203fCoreDriver.DeviceAddr,
                LC709203F_I2C_SPEED, LC709203F_VERSION, &regval))
    {
        NvOdmOsPrintf("%s: Error LC709203F version read failed.\n", __func__);
        return NV_FALSE;
    }

    *pVersion = regval;
    return NV_TRUE;
}

NvBool Lc709203fFgReset()
{
    // Not implemented yet
    return NV_TRUE;
}


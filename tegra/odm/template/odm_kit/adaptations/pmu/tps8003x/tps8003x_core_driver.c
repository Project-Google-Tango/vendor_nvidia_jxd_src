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
  * @b Description: Implements the TPS8003x core drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps8003x_core_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define TPS8003X_PMUGUID NV_ODM_GUID('t','p','s','8','0','0','3','x')
#define TPS8003X_I2C_SPEED 100

enum {
    I2C_ID0_ADDR = 0x12,
    I2C_ID1_ADDR = 0x48,
    I2C_ID2_ADDR = 0x49,
    I2C_ID3_ADDR = 0x4A,
};

typedef struct Tps8003xCoreDriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr[4];
} Tps8003xCoreDriver;

static Tps8003xCoreDriver s_Tps8003xCoreDriver = {0, NULL, {0, 0, 0, 0}};

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(TPS8003X_PMUGUID);
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

Tps8003xCoreDriverHandle Tps8003xCoreDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    if (!s_Tps8003xCoreDriver.OpenCount)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Tps8003xCoreDriver, 0, sizeof(Tps8003xCoreDriver));

        s_Tps8003xCoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
        if (!s_Tps8003xCoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
            return NULL;
        }
        s_Tps8003xCoreDriver.DeviceAddr[Tps8003xSlaveId_0] = I2C_ID0_ADDR << 1;
        s_Tps8003xCoreDriver.DeviceAddr[Tps8003xSlaveId_1] = I2C_ID1_ADDR << 1;
        s_Tps8003xCoreDriver.DeviceAddr[Tps8003xSlaveId_2] = I2C_ID2_ADDR << 1;
        s_Tps8003xCoreDriver.DeviceAddr[Tps8003xSlaveId_3] = I2C_ID3_ADDR << 1;

    }
    s_Tps8003xCoreDriver.OpenCount++;
    return &s_Tps8003xCoreDriver;
}

void Tps8003xCoreDriverClose(Tps8003xCoreDriverHandle hTps8003xCore)
{
    if (!hTps8003xCore->OpenCount)
        return;
    if (hTps8003xCore->OpenCount == 1)
    {
        NvOdmPmuI2cClose(s_Tps8003xCoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_Tps8003xCoreDriver, 0, sizeof(Tps8003xCoreDriver));
        return;
    }
    hTps8003xCore->OpenCount--;
}

NvBool Tps8003xRegisterWrite8(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 Data)
{
    return NvOdmPmuI2cWrite8(hTps8003xCore->hOdmPmuI2c,
                hTps8003xCore->DeviceAddr[SlaveId],
                TPS8003X_I2C_SPEED, RegAddr, Data);
}

NvBool Tps8003xRegisterRead8(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 *Data)
{
    return NvOdmPmuI2cRead8(hTps8003xCore->hOdmPmuI2c,
                hTps8003xCore->DeviceAddr[SlaveId],
                TPS8003X_I2C_SPEED, RegAddr, Data);
}

NvBool Tps8003xRegisterUpdate8(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 Value,
    NvU8 Mask)
{
    return NvOdmPmuI2cUpdate8(hTps8003xCore->hOdmPmuI2c,
                hTps8003xCore->DeviceAddr[SlaveId],
                TPS8003X_I2C_SPEED, RegAddr, Value, Mask);
}

NvBool Tps8003xRegisterSetBits(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 BitMask)
{
    return NvOdmPmuI2cSetBits(hTps8003xCore->hOdmPmuI2c,
                hTps8003xCore->DeviceAddr[SlaveId],
                TPS8003X_I2C_SPEED, RegAddr, BitMask);
}

NvBool Tps8003xRegisterClrBits(
    Tps8003xCoreDriverHandle hTps8003xCore,
    Tps8003xSlaveId SlaveId,
    NvU8 RegAddr,
    NvU8 BitMask)
{
    return NvOdmPmuI2cClrBits(hTps8003xCore->hOdmPmuI2c,
                hTps8003xCore->DeviceAddr[SlaveId],
                TPS8003X_I2C_SPEED, RegAddr, BitMask);
}

/*
 * Copyright (c) 2013  NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_tps80036_adc_driver.h"

#define TPS80036_I2C_SPEED 100
#define TPS80036_GPADC_RT_SEL  0xC5
#define TPS80036_GPADC_SW_SEL  0xCD
#define TPS80036_GPADC_SW_CONV0_LSB 0xCE
#define TPS80036_GPADC_SW_CONV0_MSB 0xCF

#define TPS80036_GPADC_TRIM5 0xD1
#define TPS80036_GPADC_TRIM6 0xD2

#define TPS80036_PMUGUID NV_ODM_GUID('t','p','s','8','0','0','3','6')

typedef struct Tps80036CoreDriverRec {
    NvBool Init;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr[3];
} Tps80036CoreDriver;

static Tps80036CoreDriver s_Tps80036CoreDriver = {NV_FALSE, NULL, {0,0,0}};

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(TPS80036_PMUGUID);
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


NvOdmAdcDriverInfoHandle
Tps80036AdcDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvBool ret;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance[3] = {0,0,0};
    NvU32 I2cAddress[3]  = {0,0,0};

    if (!s_Tps80036CoreDriver.Init)
    {
        Status = GetInterfaceDetail(&I2cModule, I2cInstance, I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Tps80036CoreDriver, 0, sizeof(s_Tps80036CoreDriver));

        s_Tps80036CoreDriver.hOdmPmuI2c =
                              NvOdmPmuI2cOpen(I2cModule, I2cInstance[0]);
        if (!s_Tps80036CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
            return NULL;
        }
        s_Tps80036CoreDriver.DeviceAddr[0] = I2cAddress[0];
        s_Tps80036CoreDriver.DeviceAddr[1] = I2cAddress[1];
        s_Tps80036CoreDriver.DeviceAddr[2] = I2cAddress[2];

    //Start the conversion
    ret = NvOdmPmuI2cWrite8(s_Tps80036CoreDriver.hOdmPmuI2c,
                                     s_Tps80036CoreDriver.DeviceAddr[slave_1],
                             TPS80036_I2C_SPEED, TPS80036_GPADC_RT_SEL, 0x80);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error writing to GPADC_RT_SEL \n", __func__);
        return NV_FALSE;
    }

    ret = NvOdmPmuI2cWrite8(s_Tps80036CoreDriver.hOdmPmuI2c,
                                     s_Tps80036CoreDriver.DeviceAddr[slave_1],
                             TPS80036_I2C_SPEED, TPS80036_GPADC_SW_SEL, 0x96);
    if (!ret)
    {
        NvOdmOsPrintf("%s():Error writing to GPADC_SW_SEL\n", __func__);
        return NV_FALSE;
    }

    }
    s_Tps80036CoreDriver.Init = NV_TRUE;
    return &s_Tps80036CoreDriver;
}

void Tps80036AdcDriverClose(NvOdmAdcDriverInfoHandle hAdcDriverInfo)
{
    Tps80036CoreDriverHandle hTps80036Core = (Tps80036CoreDriverHandle)hAdcDriverInfo;

    NV_ASSERT(hTps80036Core);
    if (!hTps80036Core)
    {
        NvOdmOsPrintf("hTps80036Core is NULL \n");
        return ;
    }

    NvOdmPmuI2cClose(hTps80036Core->hOdmPmuI2c);

    hTps80036Core->Init = NV_FALSE;
    NvOdmOsMemset(hTps80036Core, 0, sizeof(Tps80036CoreDriver));
    return;
}

static NvU32 Tps80036Calibrate(NvU32 LSB, NvU32 MSB)
{
    NvU32 voltage = 0;
    NvU8 D1;
    NvU8 D2;
    NvU32 gain;
    NvU32 offset;
    NvU32 vol_gain;
    NvBool ret;

    Tps80036CoreDriverHandle hTps80036Core = &s_Tps80036CoreDriver;

    ret = NvOdmPmuI2cRead8(hTps80036Core->hOdmPmuI2c,
                    hTps80036Core->DeviceAddr[slave_2], TPS80036_I2C_SPEED,
                                                TPS80036_GPADC_TRIM5, &D1);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error Reading GPADC_TRIM5 \n", __func__);
    }

    ret = NvOdmPmuI2cRead8(hTps80036Core->hOdmPmuI2c,
                    hTps80036Core->DeviceAddr[slave_2], TPS80036_I2C_SPEED,
                                                TPS80036_GPADC_TRIM6, &D2);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error Reading  GPADC_TRIM6 \n", __func__);
    }
    vol_gain = (CAL_V2 - CAL_V1) * 1000 / (CAL_X2 - CAL_X1);
    gain = 1000 + ((((NvU32)D2 -(NvU32)D1) * 1000) / (CAL_X2 - CAL_X1));
    offset = ((NvU32)D1 * 1000 - (gain - 1000) * CAL_X1);

    voltage = ((MSB << 8) + LSB) * 1000;

    voltage = ((voltage - offset) / gain);

    voltage = voltage * vol_gain;
    voltage = voltage / 1000;

    return voltage;
}

NvBool
Tps80036AdcDriverReadData(
    NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId,
    NvU32 *pData)
{
    NvU8 RegMSB = 0;
    NvU8 RegLSB = 0;
    NvBool ret;

    Tps80036CoreDriverHandle hTps80036Core = (Tps80036CoreDriverHandle)hAdcDriverInfo;

    NV_ASSERT(hTps80036Core);
    NV_ASSERT(pData);
    if (!hTps80036Core)
    {
        NvOdmOsPrintf("%s(): hTps80036Core is NULL\n", __func__);
        return NV_FALSE;
    }
    if (!pData)
    {
        NvOdmOsPrintf("%s(): pData is NULL\n", __func__);
        return NV_FALSE;
    }

    //Start the conversion
    ret = NvOdmPmuI2cWrite8(hTps80036Core->hOdmPmuI2c,
                       hTps80036Core->DeviceAddr[slave_1], TPS80036_I2C_SPEED,
                                                 TPS80036_GPADC_RT_SEL, 0x80);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error writing to GPADC_RT_SEL\n", __func__);
        return NV_FALSE;
    }

    ret = NvOdmPmuI2cWrite8(hTps80036Core->hOdmPmuI2c,
                       hTps80036Core->DeviceAddr[slave_1], TPS80036_I2C_SPEED,
                                                 TPS80036_GPADC_SW_SEL, 0x96);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error writing to GPADC_SW_SEL\n", __func__);
        return NV_FALSE;
    }

    //Read the voltage values
    ret = NvOdmPmuI2cRead8(hTps80036Core->hOdmPmuI2c,
                             hTps80036Core->DeviceAddr[1], TPS80036_I2C_SPEED,
                                        TPS80036_GPADC_SW_CONV0_LSB, &RegLSB);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error reading GPADC_SW_CONV0_LSB \n", __func__);
        return NV_FALSE;
    }

    ret = NvOdmPmuI2cRead8(hTps80036Core->hOdmPmuI2c,
                             hTps80036Core->DeviceAddr[1], TPS80036_I2C_SPEED,
                                        TPS80036_GPADC_SW_CONV0_MSB, &RegMSB);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error reading GPADC_SW_CONV0_MSB \n", __func__);
        return NV_FALSE;
    }

    *pData = Tps80036Calibrate(RegLSB, RegMSB);

    return NV_TRUE;
}

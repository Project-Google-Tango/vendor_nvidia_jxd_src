/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_bq2477x_batterycharger_driver.h"
#include "nvodm_services.h"
#include "nvos.h"

#define BQ2477X_I2C_SPEED 100
#define BQ2477X_PMUGUID NV_ODM_GUID('b','q','2','4','7','7','x','*')
#define PROC_BOARD_E1780    0x06F4

typedef struct BQ2477xCoreDriverRec {
    NvBool BqInitDone;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
    NvU32 dac_v;
    NvU32 dac_minsv;
    NvU32 dac_iin;
    NvU32 dac_ichg;
} BQ2477xCoreDriver;

static BQ2477xCoreDriver s_BQ2477xCoreDriver = { NV_FALSE, NULL,
                                                    0, 0, 0, 0, 0};
static NvOdmBoardInfo s_ProcBoardInfo;
static NvOdmPmuDeviceHandle s_hPmu = NULL;

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(BQ2477X_PMUGUID);
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
BQ2477xDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    Bq2477xCoreDriverHandle hBQ2477xCore = NULL;
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress = 0;

    hBQ2477xCore = &s_BQ2477xCoreDriver;

    if (!hBQ2477xCore->BqInitDone)
    {
        s_hPmu = hDevice;
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
        {
            NvOdmOsPrintf("%s: Error in getting Interface details.\n", __func__);
            return NULL;
        }
        NvOdmOsMemset(hBQ2477xCore, 0, sizeof(BQ2477xCoreDriver));

        hBQ2477xCore->hOdmPmuI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, I2cInstance);
        if (!hBQ2477xCore->hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
            return NULL;
        }
        hBQ2477xCore->DeviceAddr = I2cAddress;

        NvOdmPeripheralGetBoardInfo(0,&s_ProcBoardInfo);
        if(s_ProcBoardInfo.BoardID == PROC_BOARD_E1780)
        {
            hBQ2477xCore->dac_v = 9008;
            hBQ2477xCore->dac_minsv = 4608;
            hBQ2477xCore->dac_iin = 4992;
            hBQ2477xCore->dac_ichg = 2240;
        }
        else
        {
            NvOdmOsPrintf("%s: Board 0x%04x does NOT use Bq2477x\n",
                __func__,s_ProcBoardInfo.BoardID);
        }

        /* Configure control */
        Status = NvOdmPmuI2cWrite8(hBQ2477xCore->hOdmPmuI2c,
                                    hBQ2477xCore->DeviceAddr,
                                    BQ2477X_I2C_SPEED,
                                    BQ2477X_CHARGE_OPTION_0_MSB,
                                    BQ2477X_CHARGE_OPTION_POR_MSB);
        if (!Status)
            NvOdmOsPrintf("%s: BQ2477X_CHARGE_OPTION_0_MSB write failed \n",
            __func__);
        Status = NvOdmPmuI2cWrite8(hBQ2477xCore->hOdmPmuI2c,
                                    hBQ2477xCore->DeviceAddr,
                                    BQ2477X_I2C_SPEED,
                                    BQ2477X_CHARGE_OPTION_0_LSB,
                                    BQ2477X_CHARGE_OPTION_POR_LSB);
        if (!Status)
            NvOdmOsPrintf("%s: BQ2477X_CHARGE_OPTION_0_LSB write failed \n",
            __func__);

        /* Charge Voltage */
        Status = NvOdmPmuI2cWrite16(hBQ2477xCore->hOdmPmuI2c,
                                    hBQ2477xCore->DeviceAddr,
                                    BQ2477X_I2C_SPEED,
                                    BQ2477X_MAX_CHARGE_VOLTAGE_LSB,
                (hBQ2477xCore->dac_v >> 8) | (hBQ2477xCore->dac_v << 8));
        if (!Status)
            NvOdmOsPrintf("%s: BQ2477X_MAX_CHARGE_VOLTAGE_LSB write failed \n",
            __func__);

        /* Minimum System Voltage */
        Status = NvOdmPmuI2cWrite8(hBQ2477xCore->hOdmPmuI2c,
                hBQ2477xCore->DeviceAddr,
                BQ2477X_I2C_SPEED,
                BQ2477X_MIN_SYS_VOLTAGE,
                hBQ2477xCore->dac_minsv >> BQ2477X_MIN_SYS_VOLTAGE_SHIFT);
        if (!Status)
            NvOdmOsPrintf("%s: BQ2477X_MIN_SYS_VOLTAGE write failed \n",
            __func__);

        /* Configure setting input current */
        Status = NvOdmPmuI2cWrite8(hBQ2477xCore->hOdmPmuI2c,
                hBQ2477xCore->DeviceAddr,
                BQ2477X_I2C_SPEED,
                BQ2477X_INPUT_CURRENT,
                hBQ2477xCore->dac_iin >> BQ2477X_INPUT_CURRENT_SHIFT);
        if (!Status)
            NvOdmOsPrintf("%s: BQ2477X_INPUT_CURRENT write failed \n",
            __func__);

        /* Initial charging current */
        Status = NvOdmPmuI2cWrite16(hBQ2477xCore->hOdmPmuI2c,
                                    hBQ2477xCore->DeviceAddr,
                                    BQ2477X_I2C_SPEED,
                                    BQ2477X_CHARGE_CURRENT_LSB,
        (hBQ2477xCore->dac_ichg >> 8) | (hBQ2477xCore->dac_ichg << 8));
        if (!Status)
            NvOdmOsPrintf("%s: BQ2477X_CHARGE_CURRENT_LSB write failed \n",
            __func__);
    }

    hBQ2477xCore->BqInitDone = NV_TRUE;

    return hBQ2477xCore;
}

void BQ2477xDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Bq2477xCoreDriverHandle hBQ2477xCore = NULL;

    NV_ASSERT(hBChargingDriver);
    if (!hBChargingDriver)
    {
        NvOdmOsPrintf("hBChargingDriver is NULL \n");
        return ;
    }

    hBQ2477xCore = (Bq2477xCoreDriverHandle)hBChargingDriver;

    if (hBQ2477xCore->BqInitDone)
    {
        //Setting charge current to 500ma
        NvOdmPmuI2cWrite16(hBQ2477xCore->hOdmPmuI2c,
            hBQ2477xCore->DeviceAddr,
            BQ2477X_I2C_SPEED,
            BQ2477X_CHARGE_CURRENT_LSB,
            (hBQ2477xCore->dac_ichg >> 8) | (hBQ2477xCore->dac_ichg << 8));

        NvOdmPmuI2cClose(hBQ2477xCore->hOdmPmuI2c);
        hBQ2477xCore->BqInitDone = NV_FALSE;
        NvOdmOsMemset(hBQ2477xCore, 0, sizeof(BQ2477xCoreDriver));
        return;
    }
}

static NvU16 CurrentFromUsb(NvOdmUsbChargerType ChargerType)
{
    switch (ChargerType){
        case NvOdmUsbChargerType_NVCharger:
        case NvOdmUsbChargerType_CDP:
        case NvOdmUsbChargerType_DCP:
            return 0x08C0;        // 2240 mA
        case NvOdmUsbChargerType_UsbHost:
        default:
            return 0x0200;      // 512mA
    }
}

NvBool
BQ2477xBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    Bq2477xCoreDriverHandle hBQ2477xCore = NULL;
    NvU16 val = 0;

    hBQ2477xCore =  (Bq2477xCoreDriverHandle)hBChargingDriverInfo;
    NV_ASSERT(hBQ2477xCore);

    if (!hBQ2477xCore)
    {
        NvOdmOsPrintf("hBQ2477xCore is NULL \n");
        return NV_FALSE;
    }

    //FIXME::Ignoring Charging Current Limit for now. Fix this

    val =  CurrentFromUsb(ChargerType);
    NvOdmPmuI2cWrite16(hBQ2477xCore->hOdmPmuI2c,
                       hBQ2477xCore->DeviceAddr,
                       BQ2477X_I2C_SPEED,
                       BQ2477X_CHARGE_CURRENT_LSB,
                       (val << 8) | (val >> 8));

    return NV_TRUE;
}

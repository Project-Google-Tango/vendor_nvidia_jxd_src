/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: MAX17048-driver Interface</b>
  *
  * @b Description: Implements the MAX17048 gas guage drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_max17048_fuel_gauge_driver.h"

#ifdef NV_USE_NCT
#include "nvnct.h"
#include "nct.h"
#endif

#define PMU_E1761          0x6e1
#define MAX17048_PMUGUID NV_ODM_GUID('m','a','x','1','7','0','4','8')
#define MAX17048_FFD_PMUGUID NV_ODM_GUID('f','f','d','1','7','0','4','8')
#define MAX17048_I2C_SPEED 400

#define MAX17048_VCELL      0x02
#define MAX17048_SOC        0x04
#define MAX17048_VER        0x08
#define MAX17048_HIBRT      0x0A
#define MAX17048_CONFIG     0x0C
#define MAX17048_OCV        0x0E
#define MAX17048_VLRT       0x14
#define MAX17048_VRESET     0x18
#define MAX17048_STATUS     0x1A
#define MAX17048_UNLOCK     0x3E
#define MAX17048_TABLE      0x40
#define MAX17048_RCOMPSEG1  0x80
#define MAX17048_RCOMPSEG2  0x90
#define MAX17048_CMD        0xFF
#define MAX17048_UNLOCK_VALUE   0x4a57
#define MAX17048_RESET_VALUE    0x5400
#define MAX17048_DELAY      1000
#define MAX17048_VERSION_NO 0x0011
#define MAX17048_VERSION_NUM 0x0012

static NvU8 max17048_custom_data[] = {
    0xAA, 0x00, 0xB1, 0xF0, 0xB7, 0xE0, 0xB9, 0x60, 0xBB, 0x80,
    0xBC, 0x40, 0xBD, 0x30, 0xBD, 0x50, 0xBD, 0xF0, 0xBE, 0x40,
    0xBF, 0xD0, 0xC0, 0x90, 0xC4, 0x30, 0xC7, 0xC0, 0xCA, 0x60,
    0xCF, 0x30, 0x01, 0x20, 0x09, 0xC0, 0x1F, 0xC0, 0x2B, 0xE0,
    0x4F, 0xC0, 0x30, 0x00, 0x47, 0x80, 0x4F, 0xE0, 0x77, 0x00,
    0x15, 0x60, 0x46, 0x20, 0x13, 0x80, 0x1A, 0x60, 0x12, 0x20,
    0x14, 0xA0, 0x14, 0xA0};

struct max17048_battery_model  max17048_mdata;

static NvBool Max17048MdataInit(void)
{
#ifdef NV_USE_NCT
    nct_item_type buf;
    NvError err;
#endif

    NvOdmOsPrintf("Default values are assigned for Battery model data\n");
    max17048_mdata.rcomp          = 170;
    max17048_mdata.soccheck_A     = 252;
    max17048_mdata.soccheck_B     = 254;
    max17048_mdata.bits           = 19;
    max17048_mdata.alert_threshold = 0x00;
    max17048_mdata.one_percent_alerts = 0x40;
    max17048_mdata.alert_on_reset = 0x40;
    max17048_mdata.rcomp_seg      = 0x0800;
    max17048_mdata.hibernate      = 0x3080;
    max17048_mdata.vreset         = 0x9696;
    max17048_mdata.valert         = 0xD4AA;
    max17048_mdata.ocvtest        = 55600;

#ifdef NV_USE_NCT
    err = NvNctReadItem(NCT_ID_BATTERY_MODEL_DATA, &buf);
    if (NvSuccess == err)
    {
        NvOdmOsPrintf("Initialise battery model data from NCT \n");
        max17048_mdata.rcomp             =  buf.battery_mdata.max17048_mdata.rcomp;
        max17048_mdata.soccheck_A        =  buf.battery_mdata.max17048_mdata.soccheck_A;
        max17048_mdata.soccheck_B        =  buf.battery_mdata.max17048_mdata.soccheck_B;
        max17048_mdata.bits              =  buf.battery_mdata.max17048_mdata.bits;
        max17048_mdata.alert_threshold   =  buf.battery_mdata.max17048_mdata.alert_threshold;
        max17048_mdata.one_percent_alerts=  buf.battery_mdata.max17048_mdata.one_percent_alerts;
        max17048_mdata.alert_on_reset    =  buf.battery_mdata.max17048_mdata.alert_on_reset;
        max17048_mdata.rcomp_seg         =  buf.battery_mdata.max17048_mdata.rcomp_seg;
        max17048_mdata.hibernate         =  buf.battery_mdata.max17048_mdata.hibernate;
        max17048_mdata.vreset            =  buf.battery_mdata.max17048_mdata.vreset;
        max17048_mdata.valert            =  buf.battery_mdata.max17048_mdata.valert;
        max17048_mdata.ocvtest           =  buf.battery_mdata.max17048_mdata.ocvtest;
        NvOsMemcpy(max17048_custom_data, buf.battery_mdata.max17048_mdata.custom_data,
               sizeof(max17048_custom_data));
    } else {
        NvOdmOsPrintf("Battery model data read from NCT failed %0x\n",err);
    }
#endif
    return NV_TRUE;
}

typedef struct Max17048DriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Max17048CoreDriver;

static Max17048CoreDriver s_Max17048CoreDriver = {0, NULL, 0};

static NvBool
max17048_write_rcomp_seg(void)
{
    NvU8 rs1, rs2;
    NvU8 rcomp_seg_table[16];

    rs2 = max17048_mdata.rcomp_seg & 0x00FF;
    rs1 = max17048_mdata.rcomp_seg >> 8;

    rcomp_seg_table[0] = rcomp_seg_table[2] = rcomp_seg_table[4] =
        rcomp_seg_table[6] = rcomp_seg_table[8] = rcomp_seg_table[10] =
            rcomp_seg_table[12] = rcomp_seg_table[14] = rs1;

    rcomp_seg_table[1] = rcomp_seg_table[3] = rcomp_seg_table[5] =
        rcomp_seg_table[7] = rcomp_seg_table[9] = rcomp_seg_table[11] =
            rcomp_seg_table[13] = rcomp_seg_table[15] = rs2;

    if (!NvOdmPmuI2cWriteBlock(s_Max17048CoreDriver.hOdmPmuI2c,
                s_Max17048CoreDriver.DeviceAddr,
                MAX17048_I2C_SPEED,
                MAX17048_RCOMPSEG1,
                rcomp_seg_table,16))
    {
        NvOdmOsPrintf("%s: Error MAX17048 Failed writing rcompseg data.\n", __func__);
        return NV_FALSE;
    }

    if (!NvOdmPmuI2cWriteBlock(s_Max17048CoreDriver.hOdmPmuI2c,
                s_Max17048CoreDriver.DeviceAddr,
                MAX17048_I2C_SPEED,
                MAX17048_RCOMPSEG2,
                rcomp_seg_table,16))
    {
        NvOdmOsPrintf("%s: Error MAX17048 Failed writing rcompseg data.\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity;
    NvOdmPmuBoardInfo PmuBoardInfo;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                                 &PmuBoardInfo, sizeof(PmuBoardInfo));
    if (PmuBoardInfo.BoardInfo.BoardID == PMU_E1761)
        pConnectivity = NvOdmPeripheralGetGuid(MAX17048_FFD_PMUGUID);
    else
        pConnectivity = NvOdmPeripheralGetGuid(MAX17048_PMUGUID);

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
Max17048FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvU32 i = 0;
    NvU16 Ocv = 0;
    NvU16 Data = 0;
    NvU16 Version = 0;
    NvBool ret = NV_FALSE;

    if (!s_Max17048CoreDriver.OpenCount)
    {
        //Init model data of the battery
        if (!Max17048MdataInit())
            return NULL;

        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Max17048CoreDriver, 0, sizeof(Max17048CoreDriver));
        s_Max17048CoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);

        if (!s_Max17048CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Open I2C device.\n", __func__);
            return NULL;
        }
        s_Max17048CoreDriver.DeviceAddr = I2cAddress;

        // Get the version number
        ret = NvOdmPmuI2cRead16(s_Max17048CoreDriver.hOdmPmuI2c,
                                    s_Max17048CoreDriver.DeviceAddr,
                                    MAX17048_I2C_SPEED, MAX17048_VER, &Version);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Device doesn't present.\n", __func__);
            return NULL;
        }
        if ((Version == MAX17048_VERSION_NUM) || (Version == MAX17048_VERSION_NO))
        {
            NvOdmOsPrintf("%s: MAX17048 version verified.\n", __func__);
        }
        else
        {
            NvOdmOsPrintf("%s: Error MAX17048 version No.\n", __func__);
            return NULL;
        }

        // Unlock model access
        ret = NvOdmPmuI2cWrite16(s_Max17048CoreDriver.hOdmPmuI2c,
                                    s_Max17048CoreDriver.DeviceAddr,
                                    MAX17048_I2C_SPEED, MAX17048_UNLOCK,
                                    MAX17048_UNLOCK_VALUE);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 unlocking model data.\n", __func__);
            return NULL;
        }

        // Read OCV value
        ret = NvOdmPmuI2cRead16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED, MAX17048_OCV, &Ocv);
        if (!ret || (Ocv == 0xFFFF))
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed in unlocking.\n", __func__);
            return NULL;
        }

        // Write Custom data
        for (i = 0; i < 4; i += 1)
        {
            if (!NvOdmPmuI2cWriteBlock(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED,
                    (MAX17048_TABLE + i * 16),
                    &max17048_custom_data[i * 16],16))
            {
                NvOdmOsPrintf("%s: Error MAX17048 Failed writing custom data.\n", __func__);
                return NULL;
            }
        }

        // Write OCV test value
        ret = NvOdmPmuI2cWrite16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED, MAX17048_OCV, max17048_mdata.ocvtest);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed to write OCV test value.\n", __func__);
            return NULL;
        }

        // Write RCOMPSEG
        ret = max17048_write_rcomp_seg();
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed to write rcomp_seg.\n", __func__);
            return NULL;
        }

        // Disable hibernate
        ret = NvOdmPmuI2cWrite16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED,
                    MAX17048_HIBRT,0x0000);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed to disable hibernate.\n", __func__);
            return NULL;
        }

        // Lock model access
        ret = NvOdmPmuI2cWrite16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED,
                    MAX17048_UNLOCK, 0x0000);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed to lock model access.\n", __func__);
            return NULL;
        }

        // Wait for 200ms
        NvOdmOsWaitUS(200*1000);

        ret = NvOdmPmuI2cRead16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED, MAX17048_SOC, &Data);
        if (!ret || (!((Data >> 8) >= max17048_mdata.soccheck_A &&
                    (Data >> 8) <=  max17048_mdata.soccheck_B)))
        {
            NvOdmOsPrintf("%s: Error MAX17048 soc comparision failed.\n", __func__);
            return NULL;
        }
        else
        {
            NvOdmOsPrintf("%s: MAX17048 soc comparision successfull.\n", __func__);
        }

        // UnLock model access
        ret = NvOdmPmuI2cWrite16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED,
                    MAX17048_UNLOCK, MAX17048_UNLOCK_VALUE);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed to Unlock model access.\n", __func__);
            return NULL;
        }

        // Restore OCV
        ret = NvOdmPmuI2cWrite16(s_Max17048CoreDriver.hOdmPmuI2c,
                    s_Max17048CoreDriver.DeviceAddr,
                    MAX17048_I2C_SPEED,
                    MAX17048_OCV, Ocv);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Error MAX17048 Failed to restore OCV .\n", __func__);
            return NULL;
        }
    }
    s_Max17048CoreDriver.OpenCount++;
    return &s_Max17048CoreDriver;
}

void
Max17048FuelGaugeDriverClose(void)
{
    if (!s_Max17048CoreDriver.OpenCount)
        return;
    if (s_Max17048CoreDriver.OpenCount == 1)
    {
        NvOdmPmuI2cClose(s_Max17048CoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_Max17048CoreDriver, 0, sizeof(Max17048CoreDriver));
        return;
    }
    s_Max17048CoreDriver.OpenCount--;
}

NvBool
Max17048FuelGaugeDriverReadSoc(NvU32 *pData, NvU32 *voltage)
{
    NvU16 soc = 0;
    NvU16 vcell = 0;

    NV_ASSERT(pData);
    NV_ASSERT(voltage);
    if (!pData || !voltage)
        return NV_FALSE;

    if (!s_Max17048CoreDriver.OpenCount)
    {
        NvOdmPmuDeviceHandle hDevice = NULL;
        Max17048FuelGaugeDriverOpen(hDevice);
    }

    if (!NvOdmPmuI2cRead16(s_Max17048CoreDriver.hOdmPmuI2c,
                s_Max17048CoreDriver.DeviceAddr,
                MAX17048_I2C_SPEED, MAX17048_SOC, &soc))
    {
        NvOdmOsPrintf("%s: Error MAX17048 soc read failed.\n", __func__);
        /* force system to charge when I2C communication is broken */
        *pData = 0;
        return NV_FALSE;
    }
    *pData = (NvU32)(soc >> 9);

    if (!NvOdmPmuI2cRead16(s_Max17048CoreDriver.hOdmPmuI2c,
                s_Max17048CoreDriver.DeviceAddr,
                MAX17048_I2C_SPEED, MAX17048_VCELL, &vcell))
    {
        NvOdmOsPrintf("%s: Error MAX17048 soc read failed.\n", __func__);
        /* force system to charge when I2C communication is broken */
        *voltage = 0;
        return NV_FALSE;
    }
    vcell =  (((vcell >> 4) * 125) / 100);
    *voltage = (NvU32)vcell;
    return NV_TRUE;
}

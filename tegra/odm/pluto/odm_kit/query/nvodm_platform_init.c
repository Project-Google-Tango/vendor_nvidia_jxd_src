/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query.h"
#include "nvrm_module.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "pmu/boards/pluto/nvodm_pmu_pluto_supply_info_table.h"

static NvBool NvOdmVddCoreInit(void)
{
     NvOdmPmuBoardInfo PmuInfo;

     NvBool Status;
     NvU32 SettlingTime;
     NvU32 VddId;
     static NvOdmServicesPmuHandle hPmu = NULL;

     Status = NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                    &PmuInfo, sizeof(PmuInfo));
     if (!Status)
          return NV_FALSE;

     if (!PmuInfo.core_edp_mv)
          return NV_TRUE;

     VddId = PlutoPowerRails_VDD_CORE;

     if (!hPmu)
          hPmu = NvOdmServicesPmuOpen();

     if (!hPmu)
     {
          NvOdmOsDebugPrintf("PMU Device can not open\n");
          return NV_FALSE;
     }

     NvOdmOsDebugPrintf("Setting VddId=%d to %d mV\n", VddId, PmuInfo.core_edp_mv);
     NvOdmServicesPmuSetVoltage(hPmu, VddId, PmuInfo.core_edp_mv, &SettlingTime);
     if (SettlingTime)
         NvOdmOsWaitUS(SettlingTime);

     return NV_TRUE;
}

#define I2C_TRANS_TIMEOUT 5000
static NvBool ReadI2cRegister8(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data)
{
    NvU8 ReadBuffer=0;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer;
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;
    TransactionInfo[1].Address = DevAddr | 1;
    TransactionInfo[1].Buf = &ReadBuffer;
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 1;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(hOdmI2c, &TransactionInfo[0], 2,
                SpeedKHz, I2C_TRANS_TIMEOUT);

    if (status != NvOdmI2cStatus_Success)
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NvOdmOsPrintf("%s(): Failed: Timeout\n", __func__);
                break;
             case NvOdmI2cStatus_SlaveNotFound:
                NvOdmOsPrintf("%s(): Failed: SlaveNotFound slave Add 0x%x\n",
                          __func__, DevAddr);
                break;
             default:
                NvOdmOsPrintf("%s() Failed: status 0x%x\n", status, __func__);
                break;
        }
        return NV_FALSE;
    }

    *Data = ReadBuffer;
    return NV_TRUE;
}

static void ClearMax77665Interrupts(void)
{
    NvOdmServicesI2cHandle hOdmPmuI2c;
    NvU8 Data;
    NvU32 DevAddr = 0xCC;

    hOdmPmuI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, 0x4);
    if (!hOdmPmuI2c)
    {
        NvOdmOsPrintf("I2C open failed - ERROR \n");
        return;
    }

     NvOdmOsPrintf("Clearing charger interrups\n");

    /* Reading 0xB0 to clear Charger interrup status */
    ReadI2cRegister8(hOdmPmuI2c, DevAddr, 100, 0xB0, &Data);
    NvOdmI2cClose(hOdmPmuI2c);
}

NvBool NvOdmEnableCpuPowerRail(void)
{
    NvOdmServicesPmuHandle hPmu = NULL;
    NvU32 SettlingTime = 0;
    NvU8 i;
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvOdmServicesPmuVddRailCapabilities RailCaps;

    pConnectivity = NvOdmPeripheralGetGuid(NV_VDD_CPU_ODM_ID);

    hPmu = NvOdmServicesPmuOpen();
    if (!hPmu)
        goto fail;

    for (i = 0; i < pConnectivity->NumAddress; i++)
    {
        if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
        {
            NvOdmServicesPmuGetCapabilities(hPmu,
                pConnectivity->AddressList[i].Address, &RailCaps);

            NvOdmServicesPmuSetVoltage(hPmu,
                pConnectivity->AddressList[i].Address,
                RailCaps.requestMilliVolts, &SettlingTime);
        }
    }
    if (SettlingTime)
        NvOdmOsWaitUS(SettlingTime);

    NvOdmServicesPmuClose(hPmu);
    return NV_TRUE;
fail:
    NvOdmOsDebugPrintf("%s: failed!\n", __func__);
    return NV_FALSE;
}

static NvBool NvOdmPlatformConfigure_BlStart(void)
{
    NvOdmVddCoreInit();
    return NV_TRUE;
}

static NvBool NvOdmPlatformConfigure_OsLoad(void)
{
    ClearMax77665Interrupts();
    return NV_TRUE;
}

NvBool NvOdmPlatformConfigure(int Config)
{
    if (Config == NvOdmPlatformConfig_OsLoad)
         return NvOdmPlatformConfigure_OsLoad();

    if (Config == NvOdmPlatformConfig_BlStart)
         return NvOdmPlatformConfigure_BlStart();

    return NV_TRUE;
}


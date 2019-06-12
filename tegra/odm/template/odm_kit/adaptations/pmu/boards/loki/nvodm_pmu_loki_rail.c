/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_loki.h"
#include "loki_pmu.h"
#include "nvodm_pmu_loki_supply_info_table.h"
#include "nvodm_services.h"
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"
#include "pmu_hal.h"

#define FOSTER_SKU_ID 0x384

typedef struct LokiPmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} LokiPmuRailMap;

static LokiPmuRailMap s_LokiRail_PmuBoard[LokiPowerRails_Num];

void LokiPmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{
    NV_ASSERT((VddRail != LokiPowerRails_Invalid) &&
                    (VddRail < LokiPowerRails_Num));
    NV_ASSERT(s_LokiRail_PmuBoard[VddRail].PmuVddId != LokiPowerRails_Invalid);

    if ((s_LokiRail_PmuBoard[VddRail].PmuVddId == LokiPowerRails_Invalid) ||
            (VddRail == LokiPowerRails_Invalid) || (VddRail > LokiPowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                        __func__, VddRail);
         NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
         return;
    }

    s_LokiRail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
             s_LokiRail_PmuBoard[VddRail].hPmuDriver,
            s_LokiRail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool LokiPmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    NV_ASSERT((VddRail != LokiPowerRails_Invalid) &&
                (VddRail < LokiPowerRails_Num));
    NV_ASSERT(s_LokiRail_PmuBoard[VddRail].PmuVddId != LokiPowerRails_Invalid);

    if ((s_LokiRail_PmuBoard[VddRail].PmuVddId == LokiPowerRails_Invalid) ||
            (VddRail == LokiPowerRails_Invalid) || (VddRail > LokiPowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                       __func__, VddRail);
         return NV_FALSE;
    }

    return s_LokiRail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_LokiRail_PmuBoard[VddRail].hPmuDriver,
            s_LokiRail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool LokiPmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    NV_ASSERT((VddRail != LokiPowerRails_Invalid) &&
                (VddRail < LokiPowerRails_Num));
    NV_ASSERT(s_LokiRail_PmuBoard[VddRail].PmuVddId != LokiPowerRails_Invalid);

    if ((s_LokiRail_PmuBoard[VddRail].PmuVddId == LokiPowerRails_Invalid) ||
            (VddRail == LokiPowerRails_Invalid) || (VddRail > LokiPowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                      __func__, VddRail);
         return NV_FALSE;
    }

    return s_LokiRail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_LokiRail_PmuBoard[VddRail].hPmuDriver,
            s_LokiRail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

static
NvBool InitTps65914BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[LokiPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < LokiPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = LokiPmu_GetTps65914RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call LokiPmu_GetTps65914RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < LokiPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_LokiRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_LokiRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_LokiRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }

    return NV_TRUE;
}

static NvBool InitApGpioBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[LokiPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < LokiPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = LokiPmu_GetApGpioRailMap(hPmuDevice, &hDriverOps, RailMap,
                    pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call LokiPmu_GetApGpioRailMap is failed\n", __func__);
        return NV_FALSE;
    }
    for (i =0; i < LokiPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_LokiRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_LokiRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_LokiRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool LokiPmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvU32 i;
    NvOdmBoardInfo ProcBoard;
    NvOdmBoardInfo PmuBoard;
    NvBool status;
    void *hbat = NULL;
    NV_ASSERT(hPmuDevice);

    if (!hPmuDevice)
        return NV_FALSE;

    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

    /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if (status)
    {
        NvOdmOsMemcpy(&PmuBoard, &ProcBoard, sizeof(NvOdmBoardInfo));
    }
    else
    {
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x \n",
                       ProcBoard.BoardID);
        return NV_FALSE;
    }

    NvOdmOsMemset(&s_LokiRail_PmuBoard, 0, sizeof(s_LokiRail_PmuBoard));

    for (i = 0; i < LokiPowerRails_Num; ++i)
        s_LokiRail_PmuBoard[i].PmuVddId = LokiPowerRails_Invalid;

    InitTps65914BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);

    /* GPIO based power rail control */
    InitApGpioBasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);

    //foster doesn't have charger. Bug 1407827
    if (ProcBoard.SKU != FOSTER_SKU_ID )
    {
        /* Charger Fuel-Gauge Setup*/
        hbat = LokiPmuBatterySetup(hPmuDevice, &ProcBoard, &PmuBoard);
        if (!hbat)
            NvOdmOsPrintf("%s(): Error Charger- FG Setup\n", __func__);
    }

    return NV_TRUE;
}

NvBool LokiPmuPowerOff(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvOdmBoardInfo ProcBoard;
    NvBool status;

    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return NV_FALSE;
    }
    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));

    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if (status)
    {
        if (ProcBoard.BoardID == PROC_BOARD_E2548 ||
            ProcBoard.BoardID == PROC_BOARD_E2549 ||
            ProcBoard.BoardID == PROC_BOARD_E2530)
        {
            Tps65914PmuPowerOff(s_LokiRail_PmuBoard[2].hPmuDriver);
        }
    }
    else
    {
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x \n",
                       ProcBoard.BoardID);
        return NV_FALSE;
    }

    return NV_TRUE;
}

NvU8 LokiPmuGetPowerOnSource(NvOdmPmuDeviceHandle hPmuDevice)
{
    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return 0;
    }
    return Tps65914PmuPowerOnSource(s_LokiRail_PmuBoard[2].hPmuDriver);
}

NvBool LokiPmuClearAlarm(NvOdmPmuDeviceHandle hPmuDevice)
{
    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return NV_FALSE;
    }
    return Tps65914PmuClearAlarm(s_LokiRail_PmuBoard[2].hPmuDriver);
}

NvBool LokiPmuGetDate(NvOdmPmuDeviceHandle hPmuDevice, DateTime* time)
{
    if(!hPmuDevice || !time)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu/datetime handle\n",__func__);
        return NV_FALSE;
    }
    Tps65914PmuGetDate(s_LokiRail_PmuBoard[2].hPmuDriver, time);
    return NV_TRUE;
}

NvBool LokiPmuSetAlarm(NvOdmPmuDeviceHandle hPmuDevice, DateTime time)
{
    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu/datetime handle\n",__func__);
        return NV_FALSE;
    }

    Tps65914PmuSetAlarm(s_LokiRail_PmuBoard[2].hPmuDriver, time);
    return NV_TRUE;
}

void LokiPmuInterruptHandler(NvOdmPmuDeviceHandle hDevice)
{
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return;
    }
    if(Tps65914PmuInterruptHandler(s_LokiRail_PmuBoard[2].hPmuDriver) &&
        hDevice->hReturnSemaphore)
    {
        NvOdmOsSemaphoreSignal(hDevice->hReturnSemaphore);
    }
}

void LokiPmuEnableInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return;
    }
    Tps65914SetInterruptMasks(s_LokiRail_PmuBoard[2].hPmuDriver);
}

void LokiPmuDisableInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return;
    }
    NV_ASSERT(hDevice);
    Tps65914DisableInterrupts(s_LokiRail_PmuBoard[2].hPmuDriver);
}

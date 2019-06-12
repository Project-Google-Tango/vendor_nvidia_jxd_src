/*
 * Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_ardbeg.h"
#include "ardbeg_pmu.h"
#include "nvodm_pmu_ardbeg_supply_info_table.h"
#include "nvodm_services.h"
#include "tps80036/nvodm_pmu_tps80036_pmu_driver.h"
#include "tps51632/nvodm_pmu_tps51632_pmu_driver.h"
#include "as3722/nvodm_pmu_as3722_pmu_driver.h"
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"
#include "pmu_hal.h"

typedef struct ArdbegPmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} ArdbegPmuRailMap;

static ArdbegPmuRailMap s_ArdbegRail_PmuBoard[ArdbegPowerRails_Num];

void ArdbegPmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{
    NV_ASSERT((VddRail != ArdbegPowerRails_Invalid) &&
                    (VddRail < ArdbegPowerRails_Num));
    NV_ASSERT(s_ArdbegRail_PmuBoard[VddRail].PmuVddId != ArdbegPowerRails_Invalid);

    if ((s_ArdbegRail_PmuBoard[VddRail].PmuVddId == ArdbegPowerRails_Invalid) ||
            (VddRail == ArdbegPowerRails_Invalid) || (VddRail > ArdbegPowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                        __func__, VddRail);
         NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
         return;
    }

    s_ArdbegRail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
             s_ArdbegRail_PmuBoard[VddRail].hPmuDriver,
            s_ArdbegRail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool ArdbegPmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    NV_ASSERT((VddRail != ArdbegPowerRails_Invalid) &&
                (VddRail < ArdbegPowerRails_Num));
    NV_ASSERT(s_ArdbegRail_PmuBoard[VddRail].PmuVddId != ArdbegPowerRails_Invalid);

    if ((s_ArdbegRail_PmuBoard[VddRail].PmuVddId == ArdbegPowerRails_Invalid) ||
            (VddRail == ArdbegPowerRails_Invalid) || (VddRail > ArdbegPowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                       __func__, VddRail);
         return NV_FALSE;
    }

    return s_ArdbegRail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_ArdbegRail_PmuBoard[VddRail].hPmuDriver,
            s_ArdbegRail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool ArdbegPmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    NV_ASSERT((VddRail != ArdbegPowerRails_Invalid) &&
                (VddRail < ArdbegPowerRails_Num));
    NV_ASSERT(s_ArdbegRail_PmuBoard[VddRail].PmuVddId != ArdbegPowerRails_Invalid);

    if ((s_ArdbegRail_PmuBoard[VddRail].PmuVddId == ArdbegPowerRails_Invalid) ||
            (VddRail == ArdbegPowerRails_Invalid) || (VddRail > ArdbegPowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                      __func__, VddRail);
         return NV_FALSE;
    }

    return s_ArdbegRail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_ArdbegRail_PmuBoard[VddRail].hPmuDriver,
            s_ArdbegRail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

static
NvBool InitTps51632BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[ArdbegPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < ArdbegPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = ArdbegPmu_GetTps51632RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call ArdbegPmu_GetTps51632RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }

    for (i =0; i < ArdbegPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_ArdbegRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_ArdbegRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_ArdbegRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static
NvBool InitTps80036BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[ArdbegPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < ArdbegPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = ArdbegPmu_GetTps80036RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call ArdbegPmu_GetTps80036RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < ArdbegPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_ArdbegRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_ArdbegRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_ArdbegRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static
NvBool InitTps65914BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[ArdbegPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < ArdbegPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = ArdbegPmu_GetTps65914RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);

    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call ArdbegPmu_GetTps65914RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < ArdbegPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_ArdbegRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_ArdbegRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_ArdbegRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }

    return NV_TRUE;
}

static
NvBool InitAs3722BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[ArdbegPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < ArdbegPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = ArdbegPmu_GetAs3722RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call ArdbegPmu_GetTps80036RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < ArdbegPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_ArdbegRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_ArdbegRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_ArdbegRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }

    return NV_TRUE;
}

static NvBool InitApGpioBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[ArdbegPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < ArdbegPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = ArdbegPmu_GetApGpioRailMap(hPmuDevice, &hDriverOps, RailMap,
                    pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call ArdbegPmu_GetApGpioRailMap is failed\n", __func__);
        return NV_FALSE;
    }
    for (i =0; i < ArdbegPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_ArdbegRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_ArdbegRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_ArdbegRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool ArdbegPmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvU32 i;
    NvOdmBoardInfo ProcBoard;
    NvOdmBoardInfo PmuBoard;
    NvBool status;
    void *hbat;

    NV_ASSERT(hPmuDevice);

    if (!hPmuDevice)
        return NV_FALSE;

    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

    /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if (status)
    {
        if ((ProcBoard.BoardID == PROC_BOARD_PM358) ||
            (ProcBoard.BoardID == PROC_BOARD_PM359) ||
            (ProcBoard.BoardID == PROC_BOARD_PM374) ||
            (ProcBoard.BoardID == PROC_BOARD_PM370) ||
            (ProcBoard.BoardID == PROC_BOARD_PM363))
        {
            NvOdmOsMemcpy(&PmuBoard, &ProcBoard, sizeof(NvOdmBoardInfo));
        }
        else
        {
            /* E1780, E1781 */
            status = NvOdmPeripheralGetBoardInfo(PMU_E1731, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1733, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1734, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1735, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1736, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1769, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1761, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1936, &PmuBoard);
            if (!status)
            {
                NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x and Pmu Board 0x%04x\n",
                               ProcBoard.BoardID, PmuBoard.BoardID);
                return NV_FALSE;
            }
        }
    }
    else
    {
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x \n",
                       ProcBoard.BoardID);
        return NV_FALSE;
    }

    NvOdmOsMemset(&s_ArdbegRail_PmuBoard, 0, sizeof(s_ArdbegRail_PmuBoard));

    for (i = 0; i < ArdbegPowerRails_Num; ++i)
        s_ArdbegRail_PmuBoard[i].PmuVddId = ArdbegPowerRails_Invalid;

    if (PmuBoard.BoardID == PMU_E1731)
    {
        InitTps51632BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
        InitTps80036BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
    }
    else if((PmuBoard.BoardID == PMU_E1733) ||
            (PmuBoard.BoardID == PMU_E1734) ||
            (ProcBoard.BoardID == PROC_BOARD_PM358) ||
            (ProcBoard.BoardID == PROC_BOARD_PM359) ||
            (ProcBoard.BoardID == PROC_BOARD_PM374) ||
            (ProcBoard.BoardID == PROC_BOARD_PM370) ||
            (ProcBoard.BoardID == PROC_BOARD_PM363))
    {
        InitAs3722BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
    }
    else if (PmuBoard.BoardID == PMU_E1735 ||
             PmuBoard.BoardID == PMU_E1736 ||
             PmuBoard.BoardID == PMU_E1769 ||
             PmuBoard.BoardID == PMU_E1936 ||
             PmuBoard.BoardID == PMU_E1761)
    {
        InitTps65914BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
    }
    else
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x and Pmu Board 0x%04x\n",
                       ProcBoard.BoardID, PmuBoard.BoardID);

    /* GPIO based power rail control */
    InitApGpioBasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);

    /* Charger Fuel-Gauge Setup*/
    if (PmuBoard.BoardID == PMU_E1736 ||
        PmuBoard.BoardID == PMU_E1769 ||
        PmuBoard.BoardID == PMU_E1735 ||
        PmuBoard.BoardID == PMU_E1936 ||
        PmuBoard.BoardID == PMU_E1761)
    {
        hbat = ArdbegPmuBatterySetup(hPmuDevice, &ProcBoard, &PmuBoard);
        if (!hbat)
            NvOdmOsPrintf("%s(): Error Charger- FG Setup\n", __func__);
    }

    return NV_TRUE;
}

NvBool ArdbegPmuPowerOff(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvOdmBoardInfo ProcBoard;
    NvOdmBoardInfo PmuBoard;
    NvBool status;

    NV_ASSERT(hPmuDevice);
    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

     /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if (status)
    {
        if ((ProcBoard.BoardID == PROC_BOARD_PM358) ||
            (ProcBoard.BoardID == PROC_BOARD_PM359) ||
            (ProcBoard.BoardID == PROC_BOARD_PM374) ||
            (ProcBoard.BoardID == PROC_BOARD_PM370) ||
            (ProcBoard.BoardID == PROC_BOARD_PM363))
        {
            NvOdmOsMemcpy(&PmuBoard, &ProcBoard, sizeof(NvOdmBoardInfo));
        }
        else
        {
            /* E1780, E1781 */
            status = NvOdmPeripheralGetBoardInfo(PMU_E1731, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1733, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1734, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1735, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1736, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1936, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1769, &PmuBoard);
            if (!status)
                status = NvOdmPeripheralGetBoardInfo(PMU_E1761, &PmuBoard);
            if (!status)
            {
                NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x and Pmu Board 0x%04x\n",
                               ProcBoard.BoardID, PmuBoard.BoardID);
                return NV_FALSE;
            }

        }
    }
    else
    {
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x \n",
                       ProcBoard.BoardID);
        return NV_FALSE;
    }

    if (PmuBoard.BoardID == PMU_E1731)
        Tps80036PmuPowerOff(s_ArdbegRail_PmuBoard[2].hPmuDriver);
    else if ((ProcBoard.BoardID == PROC_BOARD_PM358) ||
             (ProcBoard.BoardID == PROC_BOARD_PM359) ||
             (ProcBoard.BoardID == PROC_BOARD_PM370) ||
             (ProcBoard.BoardID == PROC_BOARD_PM374) ||
             (ProcBoard.BoardID == PROC_BOARD_PM363) ||
             (PmuBoard.BoardID == PMU_E1733) ||
             (PmuBoard.BoardID == PMU_E1734))
        As3722PmuPowerOff(s_ArdbegRail_PmuBoard[2].hPmuDriver);
    else if (PmuBoard.BoardID == PMU_E1735 ||
             PmuBoard.BoardID == PMU_E1736 ||
             PmuBoard.BoardID == PMU_E1936 ||
             PmuBoard.BoardID == PMU_E1761 ||
             PmuBoard.BoardID == PMU_E1769)
    {
        Tps65914PmuPowerOff(s_ArdbegRail_PmuBoard[2].hPmuDriver);
    }
    //shouldn't reach here
    NV_ASSERT(0);
    return NV_FALSE;
}

NvU8 ArdbegPmuGetPowerOnSource(NvOdmPmuDeviceHandle hPmuDevice)
{
    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return 0;
    }
    return Tps65914PmuPowerOnSource(s_ArdbegRail_PmuBoard[2].hPmuDriver);
}

NvBool ArdbegPmuClearAlarm(NvOdmPmuDeviceHandle hPmuDevice)
{
    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return NV_FALSE;
    }
    return Tps65914PmuClearAlarm(s_ArdbegRail_PmuBoard[2].hPmuDriver);
}

NvBool ArdbegPmuGetDate(NvOdmPmuDeviceHandle hPmuDevice, DateTime* time)
{
    if(!hPmuDevice || !time)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu/datetime handle\n",__func__);
        return NV_FALSE;
    }
    Tps65914PmuGetDate(s_ArdbegRail_PmuBoard[2].hPmuDriver, time);
    return NV_TRUE;
}

NvBool ArdbegPmuSetAlarm(NvOdmPmuDeviceHandle hPmuDevice, DateTime time)
{
    if(!hPmuDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu/datetime handle\n",__func__);
        return NV_FALSE;
    }

    Tps65914PmuSetAlarm(s_ArdbegRail_PmuBoard[2].hPmuDriver, time);
    return NV_TRUE;
}

void ArdbegPmuInterruptHandler(NvOdmPmuDeviceHandle hDevice)
{
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return;
    }
    if(Tps65914PmuInterruptHandler(s_ArdbegRail_PmuBoard[2].hPmuDriver) &&
        hDevice->hReturnSemaphore)
    {
        NvOdmOsSemaphoreSignal(hDevice->hReturnSemaphore);
    }
}

void ArdbegPmuEnableInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return;
    }
    Tps65914SetInterruptMasks(s_ArdbegRail_PmuBoard[2].hPmuDriver);
}

void ArdbegPmuDisableInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return;
    }
    NV_ASSERT(hDevice);
    Tps65914DisableInterrupts(s_ArdbegRail_PmuBoard[2].hPmuDriver);
}

NvBool ArdbegPmuGetBatteryCapacity(NvOdmPmuDeviceHandle hPmuDevice, NvU32* BatCapacity)
{
    if(!hPmuDevice || !BatCapacity)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu/battery handle\n",__func__);
        return NV_FALSE;
    }
    Tps65914PmuGetBatCapacity(s_ArdbegRail_PmuBoard[2].hPmuDriver, BatCapacity);
    return NV_TRUE;
}

NvBool ArdbegPmuGetBatteryAdcVal(NvOdmPmuDeviceHandle hPmuDevice, NvU32 AdcChannelId, NvU32* BatAdcVal)
{
    if(!hPmuDevice || !BatAdcVal)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu/battery handle\n",__func__);
        return NV_FALSE;
    }
    Tps65914AdcDriverReadData(s_ArdbegRail_PmuBoard[2].hPmuDriver, AdcChannelId, BatAdcVal);
    return NV_TRUE;
}

NvBool ArdbegPmuReadRstReason(NvOdmPmuDeviceHandle hPmuDevice, NvOdmPmuResetReason *rst_reason)
{
    NV_ASSERT(hPmuDevice);
    if (!s_ArdbegRail_PmuBoard[2].hPmuDriver)
        return NV_FALSE;

    NV_ASSERT(rst_reason);
    if (!rst_reason)
        return NV_FALSE;

    return Tps65914PmuReadRstReason(s_ArdbegRail_PmuBoard[2].hPmuDriver, rst_reason);
}


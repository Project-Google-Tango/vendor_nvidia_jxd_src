/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_pluto.h"
#include "pluto_pmu.h"
#include "nvodm_pmu_pluto_supply_info_table.h"
#include "nvodm_services.h"
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"

typedef struct PlutoPmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} PlutoPmuRailMap;

static PlutoPmuRailMap s_PlutoRail_PmuBoard[PlutoPowerRails_Num];

void PlutoPmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{

    NV_ASSERT((VddRail != PlutoPowerRails_Invalid) &&
            (VddRail < PlutoPowerRails_Num));
    NV_ASSERT(s_PlutoRail_PmuBoard[VddRail].PmuVddId != PlutoPowerRails_Invalid);

    if ((s_PlutoRail_PmuBoard[VddRail].PmuVddId == PlutoPowerRails_Invalid) ||
            (VddRail == PlutoPowerRails_Invalid) || (VddRail > PlutoPowerRails_Num))
    {
        NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                __func__, VddRail);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    s_PlutoRail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
            s_PlutoRail_PmuBoard[VddRail].hPmuDriver,
            s_PlutoRail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool PlutoPmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    NV_ASSERT((VddRail != PlutoPowerRails_Invalid) &&
            (VddRail < PlutoPowerRails_Num));
    NV_ASSERT(s_PlutoRail_PmuBoard[VddRail].PmuVddId != PlutoPowerRails_Invalid);

    if ((s_PlutoRail_PmuBoard[VddRail].PmuVddId == PlutoPowerRails_Invalid) ||
            (VddRail == PlutoPowerRails_Invalid) || (VddRail > PlutoPowerRails_Num))
    {
        NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                __func__, VddRail);
         return NV_FALSE;
    }

    return s_PlutoRail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_PlutoRail_PmuBoard[VddRail].hPmuDriver,
            s_PlutoRail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool PlutoPmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    NV_ASSERT((VddRail != PlutoPowerRails_Invalid) &&
            (VddRail < PlutoPowerRails_Num));
    NV_ASSERT(s_PlutoRail_PmuBoard[VddRail].PmuVddId != PlutoPowerRails_Invalid);

    if ((s_PlutoRail_PmuBoard[VddRail].PmuVddId == PlutoPowerRails_Invalid) ||
            (VddRail == PlutoPowerRails_Invalid) || (VddRail > PlutoPowerRails_Num))
    {
        NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                __func__, VddRail);
        return NV_FALSE;
    }

    return s_PlutoRail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_PlutoRail_PmuBoard[VddRail].hPmuDriver,
            s_PlutoRail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

NvBool PlutoPmuReadRstReason(NvOdmPmuDeviceHandle hPmuDevice, NvOdmPmuResetReason *rst_reason)
{
    NV_ASSERT(hPmuDevice);
    if (!s_PlutoRail_PmuBoard[1].hPmuDriver)
        return NV_FALSE;

    NV_ASSERT(rst_reason);
    if (!rst_reason)
        return NV_FALSE;

    return Tps65914PmuReadRstReason(s_PlutoRail_PmuBoard[1].hPmuDriver, rst_reason);
}

static
NvBool InitTps65914BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[PlutoPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < PlutoPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = PlutoPmu_GetTps65914Map(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call PlutoPmu_GetTps65914Map is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < PlutoPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_PlutoRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_PlutoRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_PlutoRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static NvBool InitApGpioBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[PlutoPowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < PlutoPowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = PlutoPmu_GetApGpioRailMap(hPmuDevice, &hDriverOps, RailMap,
                    pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call PlutoPmu_GetApGpioRailMap is failed\n", __func__);
        return NV_FALSE;
    }
    for (i =0; i < PlutoPowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_PlutoRail_PmuBoard[i].PmuVddId = RailMap[i];
        s_PlutoRail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_PlutoRail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool PlutoPmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvU32 i;
    NvOdmBoardInfo ProcBoard;
    NvOdmBoardInfo PmuBoard;
    NvBool status;
    void *hBatDriver;

    NV_ASSERT(hPmuDevice);

    if (!hPmuDevice)
        return NV_FALSE;

    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

    /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if (status)
    {
        if ((ProcBoard.BoardID == PROC_BOARD_E1580) ||
            (ProcBoard.BoardID == PROC_BOARD_E1575) ||
            (ProcBoard.BoardID == PROC_BOARD_E1577))
            NvOdmOsMemcpy(&PmuBoard, &ProcBoard, sizeof(NvOdmBoardInfo));
    }

    NvOdmOsMemset(&s_PlutoRail_PmuBoard, 0, sizeof(s_PlutoRail_PmuBoard));

    for (i =0; i < PlutoPowerRails_Num; ++i)
        s_PlutoRail_PmuBoard[i].PmuVddId = PlutoPowerRails_Invalid;

    if ((ProcBoard.BoardID == PROC_BOARD_E1580) ||
        (ProcBoard.BoardID == PROC_BOARD_E1575) ||
        (ProcBoard.BoardID == PROC_BOARD_E1577))
        InitTps65914BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
    else
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x and Pmu Board 0x%04x\n",
              ProcBoard.BoardID, PmuBoard.BoardID);

    /* GPIO based power rail control */
    InitApGpioBasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);

    /*Battery-Fuel gauge setup */
    hBatDriver = PlutoPmuBatterySetup(hPmuDevice, &ProcBoard, &PmuBoard);

    if (!hBatDriver)
        NvOdmOsPrintf("%s(): Not able to do BatterySetup\n", __func__);

    return NV_TRUE;
}

void PlutoPmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
    NV_ASSERT(hPmuDevice);
    PlutoPmuBatteryRelease();
    if (!s_PlutoRail_PmuBoard[1].hPmuDriver)
        return;

    Tps65914PmuDriverClose(s_PlutoRail_PmuBoard[1].hPmuDriver);
    NvOdmOsMemset(&s_PlutoRail_PmuBoard, 0, sizeof(s_PlutoRail_PmuBoard));
}

void PlutoPmuSuspend(void)
{
    Tps65914PmuDriverSuspend();
}

NvBool PlutoPmuPowerOff(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvOdmBoardInfo ProcBoard;
    NvOdmBoardInfo PmuBoard;
    NvBool status;

    NV_ASSERT(hPmuDevice);

    if (!hPmuDevice)
        return NV_FALSE;

    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

    /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if(!status)
        return NV_FALSE;

    if ((ProcBoard.BoardID == PROC_BOARD_E1580) ||
        (ProcBoard.BoardID == PROC_BOARD_E1575) ||
        (ProcBoard.BoardID == PROC_BOARD_E1577))
        Tps65914PmuPowerOff(s_PlutoRail_PmuBoard[1].hPmuDriver);

    //shouldn't reach here
    NV_ASSERT(0);
    return NV_FALSE;
}

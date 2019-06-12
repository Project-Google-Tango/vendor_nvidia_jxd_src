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
#include "nvodm_pmu_vcm30t114.h"
#include "vcm30t114_pmu.h"
#include "nvodm_pmu_vcm30t114_supply_info_table.h"
#include "nvodm_services.h"
#include "max77663/nvodm_pmu_max77663_pmu_driver.h"
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"
#include "tps65090/nvodm_pmu_tps65090_pmu_driver.h"

typedef struct VCM30T114PmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} VCM30T114PmuRailMap;

static VCM30T114PmuRailMap s_VCM30T114Rail_PmuBoard[VCM30T114PowerRails_Num];

void VCM30T114PmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{
    NV_ASSERT((VddRail != VCM30T114PowerRails_Invalid) &&
                    (VddRail < VCM30T114PowerRails_Num));
    NV_ASSERT(s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId != VCM30T114PowerRails_Invalid);

    if ((s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId == VCM30T114PowerRails_Invalid) ||
            (VddRail == VCM30T114PowerRails_Invalid) || (VddRail > VCM30T114PowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                        __func__, VddRail);
         NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
         return;
    }

    s_VCM30T114Rail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
             s_VCM30T114Rail_PmuBoard[VddRail].hPmuDriver,
            s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool VCM30T114PmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    NV_ASSERT((VddRail != VCM30T114PowerRails_Invalid) &&
                (VddRail < VCM30T114PowerRails_Num));
    NV_ASSERT(s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId != VCM30T114PowerRails_Invalid);

    if ((s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId == VCM30T114PowerRails_Invalid) ||
            (VddRail == VCM30T114PowerRails_Invalid) || (VddRail > VCM30T114PowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                       __func__, VddRail);
         return NV_FALSE;
    }

    return s_VCM30T114Rail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_VCM30T114Rail_PmuBoard[VddRail].hPmuDriver,
            s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool VCM30T114PmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    NV_ASSERT((VddRail != VCM30T114PowerRails_Invalid) &&
                (VddRail < VCM30T114PowerRails_Num));
    NV_ASSERT(s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId != VCM30T114PowerRails_Invalid);

    if ((s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId == VCM30T114PowerRails_Invalid) ||
            (VddRail == VCM30T114PowerRails_Invalid) || (VddRail > VCM30T114PowerRails_Num))
    {
         NvOdmOsPrintf("%s(): Either the power rail %d is not mapped properly or its invalid\n",
                      __func__, VddRail);
         return NV_FALSE;
    }

    return s_VCM30T114Rail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_VCM30T114Rail_PmuBoard[VddRail].hPmuDriver,
            s_VCM30T114Rail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

static
NvBool InitTps51632BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[VCM30T114PowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < VCM30T114PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = VCM30T114Pmu_GetTps51632RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call VCM30T114Pmu_GetTps51632RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }

    for (i =0; i < VCM30T114PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_VCM30T114Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_VCM30T114Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_VCM30T114Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static
NvBool InitTps65090BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[VCM30T114PowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < VCM30T114PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = VCM30T114Pmu_GetTps65090RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call VCM30T114Pmu_GetTps65090RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < VCM30T114PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_VCM30T114Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_VCM30T114Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_VCM30T114Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static
NvBool InitTps65914BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[VCM30T114PowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < VCM30T114PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = VCM30T114Pmu_GetTps65914RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call VCM30T114Pmu_GetTps65914RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < VCM30T114PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_VCM30T114Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_VCM30T114Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_VCM30T114Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static
NvBool InitMax77663BasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[VCM30T114PowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < VCM30T114PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = VCM30T114Pmu_GetMax77663RailMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call VCM30T114Pmu_GetMax77663RailMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < VCM30T114PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_VCM30T114Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_VCM30T114Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_VCM30T114Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

static NvBool InitApGpioBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[VCM30T114PowerRails_Num];
    NvU32 i;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NV_FALSE;

    for (i = 0; i < VCM30T114PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = VCM30T114Pmu_GetApGpioRailMap(hPmuDevice, &hDriverOps, RailMap,
                    pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call VCM30T114Pmu_GetApGpioRailMap is failed\n", __func__);
        return NV_FALSE;
    }
    for (i =0; i < VCM30T114PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_VCM30T114Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_VCM30T114Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_VCM30T114Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool VCM30T114PmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvU32 i;
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
    if (status)
    {
        if ((ProcBoard.BoardID == PROC_BOARD_E1612))
            NvOdmOsMemcpy(&PmuBoard, &ProcBoard, sizeof(NvOdmBoardInfo));
    }

    NvOdmOsMemset(&s_VCM30T114Rail_PmuBoard, 0, sizeof(s_VCM30T114Rail_PmuBoard));

    for (i = 0; i < VCM30T114PowerRails_Num; ++i)
        s_VCM30T114Rail_PmuBoard[i].PmuVddId = VCM30T114PowerRails_Invalid;

    if ((ProcBoard.BoardID == PROC_BOARD_E1612) ||
          (ProcBoard.BoardID == PROC_BOARD_E1613))
    {
        InitTps51632BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
        InitTps65090BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
        InitMax77663BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
    }
    else if ((ProcBoard.BoardID == PROC_BOARD_E1611 ||
                ProcBoard.BoardID == PROC_BOARD_P2454))
    {
        InitTps51632BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
        InitTps65090BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
        InitTps65914BasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);
    }
    else
        NvOdmOsPrintf("BoardId is not supported ProcBoard 0x%04x and Pmu Board 0x%04x\n",
              ProcBoard.BoardID, PmuBoard.BoardID);

    /* GPIO based power rail control */
    InitApGpioBasedRailMapping(hPmuDevice, &ProcBoard, &PmuBoard);

    return NV_TRUE;
}

void VCM30T114PmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
}

void VCM30T114PmuSuspend(void)
{
    Tps65914PmuDriverSuspend();
}

NvBool VCM30T114PmuPowerOff(NvOdmPmuDeviceHandle hPmuDevice)
{
    NvOdmBoardInfo ProcBoard;
    NvOdmBoardInfo PmuBoard;
    NvBool status;
    Tps65090PmuInfoHandle hTps65090PmuInfo;
    Tps65090PmuProps    *pPmuProps;

    NV_ASSERT(hPmuDevice);
    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

    /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if(!status)
        return NV_FALSE;
    if ((ProcBoard.BoardID == PROC_BOARD_E1612) ||
          (ProcBoard.BoardID == PROC_BOARD_E1613))
        Max77663PmuPowerOff(s_VCM30T114Rail_PmuBoard[2].hPmuDriver);

    if (ProcBoard.BoardID == PROC_BOARD_E1611)
       {
          NvU32  pDelayMicroSeconds = 100;
          hTps65090PmuInfo = (Tps65090PmuInfoHandle)s_VCM30T114Rail_PmuBoard[VCM30T114PowerRails_VDD_LCD_BL].hPmuDriver;
          pPmuProps = hTps65090PmuInfo->RailInfo[TPS65090PmuSupply_FET1].pPmuProps;
          pPmuProps->pOps->DisableRail(hTps65090PmuInfo, TPS65090PmuSupply_FET1, &pDelayMicroSeconds);
          Tps65914PmuPowerOff(s_VCM30T114Rail_PmuBoard[2].hPmuDriver);
       }

    //shouldn't reach here
    NV_ASSERT(0);
    return NV_FALSE;
}

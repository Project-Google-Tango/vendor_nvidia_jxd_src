/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_vcm30t30.h"
#include "vcm30t30_pmu.h"
#include "nvodm_pmu_vcm30t30_supply_info_table.h"
#include "nvodm_services.h"
#include "pmu/nvodm_pmu_driver.h"

typedef struct Vcm30T30PmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} Vcm30T30PmuRailMap;

static Vcm30T30PmuRailMap s_Vcm30T30Rail_PmuBoard[Vcm30T30PowerRails_Num];

void Vcm30T30PmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{
    PmuDebugPrintf("%s(): The power rail %d \n", __func__, VddRail);
    NV_ASSERT((VddRail != Vcm30T30PowerRails_Invalid) &&
                    (VddRail < Vcm30T30PowerRails_Num));
    NV_ASSERT(s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId != Vcm30T30PowerRails_Invalid);
    if (s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId == Vcm30T30PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                        __func__, VddRail);
         NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
         return;
    }

    s_Vcm30T30Rail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
             s_Vcm30T30Rail_PmuBoard[VddRail].hPmuDriver,
            s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool Vcm30T30PmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    PmuDebugPrintf("%s(): The power rail %d \n", __func__, VddRail);
    NV_ASSERT((VddRail != Vcm30T30PowerRails_Invalid) &&
                (VddRail < Vcm30T30PowerRails_Num));
    NV_ASSERT(s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId != Vcm30T30PowerRails_Invalid);
    if (s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId == Vcm30T30PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                       __func__, VddRail);
         return NV_FALSE;
    }

    return s_Vcm30T30Rail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_Vcm30T30Rail_PmuBoard[VddRail].hPmuDriver,
            s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool Vcm30T30PmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    PmuDebugPrintf("%s(): The power rail %d 0x%02x\n", __func__, VddRail,
                  MilliVolts);
    NV_ASSERT((VddRail != Vcm30T30PowerRails_Invalid) &&
                (VddRail < Vcm30T30PowerRails_Num));
    NV_ASSERT(s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId != Vcm30T30PowerRails_Invalid);
    if (s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId == Vcm30T30PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                      __func__, VddRail);
         return NV_FALSE;
    }

    return s_Vcm30T30Rail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_Vcm30T30Rail_PmuBoard[VddRail].hPmuDriver,
            s_Vcm30T30Rail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

static NvBool InitTps6591xBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[Vcm30T30PowerRails_Num];
    int i;

    for (i =0; i < Vcm30T30PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = Vcm30T30Pmu_GetTps6591xMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call Vcm30T30Pmu_GetTps6591xMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < Vcm30T30PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_Vcm30T30Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_Vcm30T30Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_Vcm30T30Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool Vcm30T30PmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    int i;

    NvOdmOsMemset(&s_Vcm30T30Rail_PmuBoard, 0, sizeof(s_Vcm30T30Rail_PmuBoard));

    for (i =0; i < Vcm30T30PowerRails_Num; ++i)
        s_Vcm30T30Rail_PmuBoard[i].PmuVddId = Vcm30T30PowerRails_Invalid;

    InitTps6591xBasedRailMapping(hPmuDevice, NULL, NULL);

    return NV_TRUE;
}

void Vcm30T30PmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
}

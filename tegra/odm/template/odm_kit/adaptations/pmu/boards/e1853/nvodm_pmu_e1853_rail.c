/*
 * Copyright (c) 2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_e1853.h"
#include "e1853_pmu.h"
#include "nvodm_pmu_e1853_supply_info_table.h"
#include "nvodm_services.h"
#include "pmu/nvodm_pmu_driver.h"

typedef struct E1853PmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} E1853PmuRailMap;

static E1853PmuRailMap s_E1853Rail_PmuBoard[E1853PowerRails_Num];

void E1853PmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{
    PmuDebugPrintf("%s(): The power rail %d \n", __func__, VddRail);
    NV_ASSERT((VddRail != E1853PowerRails_Invalid) &&
                    (VddRail < E1853PowerRails_Num));
    NV_ASSERT(s_E1853Rail_PmuBoard[VddRail].PmuVddId != E1853PowerRails_Invalid);
    if (s_E1853Rail_PmuBoard[VddRail].PmuVddId == E1853PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                        __func__, VddRail);
         NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
         return;
    }

    s_E1853Rail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
             s_E1853Rail_PmuBoard[VddRail].hPmuDriver,
            s_E1853Rail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool E1853PmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    PmuDebugPrintf("%s(): The power rail %d \n", __func__, VddRail);
    NV_ASSERT((VddRail != E1853PowerRails_Invalid) &&
                (VddRail < E1853PowerRails_Num));
    NV_ASSERT(s_E1853Rail_PmuBoard[VddRail].PmuVddId != E1853PowerRails_Invalid);
    if (s_E1853Rail_PmuBoard[VddRail].PmuVddId == E1853PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                       __func__, VddRail);
         return NV_FALSE;
    }

    return s_E1853Rail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_E1853Rail_PmuBoard[VddRail].hPmuDriver,
            s_E1853Rail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool E1853PmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    PmuDebugPrintf("%s(): The power rail %d 0x%02x\n", __func__, VddRail,
                  MilliVolts);
    NV_ASSERT((VddRail != E1853PowerRails_Invalid) &&
                (VddRail < E1853PowerRails_Num));
    NV_ASSERT(s_E1853Rail_PmuBoard[VddRail].PmuVddId != E1853PowerRails_Invalid);
    if (s_E1853Rail_PmuBoard[VddRail].PmuVddId == E1853PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                      __func__, VddRail);
         return NV_FALSE;
    }

    return s_E1853Rail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_E1853Rail_PmuBoard[VddRail].hPmuDriver,
            s_E1853Rail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

static NvBool InitTps6591xBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[E1853PowerRails_Num];
    int i;

    for (i =0; i < E1853PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = E1853Pmu_GetTps6591xMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call E1853Pmu_GetTps6591xMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < E1853PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_E1853Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_E1853Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_E1853Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool E1853PmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    int i;

    NvOdmOsMemset(&s_E1853Rail_PmuBoard, 0, sizeof(s_E1853Rail_PmuBoard));

    for (i =0; i < E1853PowerRails_Num; ++i)
        s_E1853Rail_PmuBoard[i].PmuVddId = E1853PowerRails_Invalid;

    InitTps6591xBasedRailMapping(hPmuDevice, NULL, NULL);

    return NV_TRUE;
}

void E1853PmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
}

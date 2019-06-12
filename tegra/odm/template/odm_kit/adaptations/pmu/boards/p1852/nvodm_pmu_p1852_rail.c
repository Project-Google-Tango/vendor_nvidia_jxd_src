/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_p1852.h"
#include "p1852_pmu.h"
#include "nvodm_pmu_p1852_supply_info_table.h"
#include "nvodm_services.h"
#include "pmu/nvodm_pmu_driver.h"

typedef struct P1852PmuRailMapRec {
    NvU32 PmuVddId;
    PmuChipDriverOpsHandle hPmuDriverOps;
    void *hPmuDriver;
} P1852PmuRailMap;

static P1852PmuRailMap s_P1852Rail_PmuBoard[P1852PowerRails_Num];

void P1852PmuGetCapabilities(NvU32 VddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities)
{
    PmuDebugPrintf("%s(): The power rail %d \n", __func__, VddRail);
    NV_ASSERT((VddRail != P1852PowerRails_Invalid) &&
                    (VddRail < P1852PowerRails_Num));
    NV_ASSERT(s_P1852Rail_PmuBoard[VddRail].PmuVddId != P1852PowerRails_Invalid);
    if (s_P1852Rail_PmuBoard[VddRail].PmuVddId == P1852PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                        __func__, VddRail);
         NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
         return;
    }

    s_P1852Rail_PmuBoard[VddRail].hPmuDriverOps->GetCaps(
             s_P1852Rail_PmuBoard[VddRail].hPmuDriver,
            s_P1852Rail_PmuBoard[VddRail].PmuVddId, pCapabilities);
}

NvBool P1852PmuGetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32* pMilliVolts)
{
    PmuDebugPrintf("%s(): The power rail %d \n", __func__, VddRail);
    NV_ASSERT((VddRail != P1852PowerRails_Invalid) &&
                (VddRail < P1852PowerRails_Num));
    NV_ASSERT(s_P1852Rail_PmuBoard[VddRail].PmuVddId != P1852PowerRails_Invalid);
    if (s_P1852Rail_PmuBoard[VddRail].PmuVddId == P1852PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                       __func__, VddRail);
         return NV_FALSE;
    }

    return s_P1852Rail_PmuBoard[VddRail].hPmuDriverOps->GetVoltage(
            s_P1852Rail_PmuBoard[VddRail].hPmuDriver,
            s_P1852Rail_PmuBoard[VddRail].PmuVddId, pMilliVolts);
}

NvBool P1852PmuSetVoltage(NvOdmPmuDeviceHandle hPmuDevice, NvU32 VddRail,
            NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    PmuDebugPrintf("%s(): The power rail %d 0x%02x\n", __func__, VddRail,
                  MilliVolts);
    NV_ASSERT((VddRail != P1852PowerRails_Invalid) &&
                (VddRail < P1852PowerRails_Num));
    NV_ASSERT(s_P1852Rail_PmuBoard[VddRail].PmuVddId != P1852PowerRails_Invalid);
    if (s_P1852Rail_PmuBoard[VddRail].PmuVddId == P1852PowerRails_Invalid)
    {
         NvOdmOsPrintf("%s(): The power rail %d is not mapped properly\n",
                      __func__, VddRail);
         return NV_FALSE;
    }

    return s_P1852Rail_PmuBoard[VddRail].hPmuDriverOps->SetVoltage(
            s_P1852Rail_PmuBoard[VddRail].hPmuDriver,
            s_P1852Rail_PmuBoard[VddRail].PmuVddId, MilliVolts,
            pSettleMicroSeconds);
}

static NvBool InitTps6591xBasedRailMapping(NvOdmPmuDeviceHandle hPmuDevice,
        NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    void *hPmuDriver;
    PmuChipDriverOpsHandle hDriverOps;
    NvU32 RailMap[P1852PowerRails_Num];
    int i;

    for (i =0; i < P1852PowerRails_Num; ++i)
        RailMap[i] = 0xFFFF;

    hPmuDriver = P1852Pmu_GetTps6591xMap(hPmuDevice, &hDriverOps, RailMap,
                        pProcBoard, pPmuBoard);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): Call P1852Pmu_GetTps6591xMap is failed\n",
                    __func__);
        return NV_FALSE;
    }
    for (i =0; i < P1852PowerRails_Num; ++i)
    {
        if ((RailMap[i] == 0xFFFF) || (RailMap[i] == 0))
            continue;

        s_P1852Rail_PmuBoard[i].PmuVddId = RailMap[i];
        s_P1852Rail_PmuBoard[i].hPmuDriverOps = hDriverOps;
        s_P1852Rail_PmuBoard[i].hPmuDriver = hPmuDriver;
    }
    return NV_TRUE;
}

NvBool P1852PmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    int i;

    NvOdmOsMemset(&s_P1852Rail_PmuBoard, 0, sizeof(s_P1852Rail_PmuBoard));

    for (i =0; i < P1852PowerRails_Num; ++i)
        s_P1852Rail_PmuBoard[i].PmuVddId = P1852PowerRails_Invalid;

    InitTps6591xBasedRailMapping(hPmuDevice, NULL, NULL);

    return NV_TRUE;
}

void P1852PmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
}

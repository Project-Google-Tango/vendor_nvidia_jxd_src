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
#include "tps51632/nvodm_pmu_tps51632_pmu_driver.h"

static Tps51632BoardData s_tps51632_bdata = {NV_FALSE, Tps51632Mode_0};

static NvOdmPmuRailProps s_OdmTps51632RailProps[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {Tps51632PmuSupply_Invalid, VCM30T114PowerRails_Invalid,      0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Tps51632PmuSupply_VDD,     VCM30T114PowerRails_VDD_CPU,     0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, &s_tps51632_bdata},
};

static PmuChipDriverOps s_Tps51632OdmProps = {
    Tps51632PmuGetCaps, Tps51632PmuGetVoltage, Tps51632PmuSetVoltage,
};

static void InitMapTps51632(NvU32 *pRailMap)
{
        pRailMap[VCM30T114PowerRails_VDD_CPU] = Tps51632PmuSupply_VDD;
}

void
*VCM30T114Pmu_GetTps51632RailMap(
    NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    NvOdmPmuDriverInfoHandle hPmuDriver = NULL;
    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);
    NV_ASSERT(pRailMap);
    NV_ASSERT(hDriverOps);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NULL;

    if (!pRailMap || !hDriverOps)
        return NULL;

    hPmuDriver = Tps51632PmuDriverOpen(hPmuDevice, s_OdmTps51632RailProps,
                                       Tps51632PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps51632PmuDriverOpen() fails\n", __func__);
        return NULL;
    }

    InitMapTps51632(pRailMap);

    *hDriverOps = &s_Tps51632OdmProps;
    return hPmuDriver;
}

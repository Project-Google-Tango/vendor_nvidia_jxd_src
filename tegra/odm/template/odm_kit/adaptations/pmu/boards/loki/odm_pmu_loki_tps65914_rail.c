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
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"

static Tps65914BoardData BoardData = { NV_TRUE };

static NvOdmPmuRailProps s_OdmTps65914RailProps[] = {
     /*VddId                                            Min_mV    Setup_mV         IsAlwaysOn       BoardData
                                SystemRailId                  Max_mV      IsEnable        IsOdmProtected      */
    {TPS65914PmuSupply_Invalid, LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS45,  LokiPowerRails_VDD_CORE,   0,    0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   LokiPowerRails_VDD_3V3_SYS,0,    0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS7,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   LokiPowerRails_VDDIO_SDMMC4, 0,  0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS9,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS10,  LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    LokiPowerRails_AVDD_LCD,   0,    0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    LokiPowerRails_AVDD_DSI_CSI, 0,  0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    LokiPowerRails_VPP_FUSE,   0,    0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    LokiPowerRails_VDDIO_SDMMC3, 0,  0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   LokiPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static PmuChipDriverOps s_Tps65914Ops = {
    Tps65914PmuGetCaps, Tps65914PmuGetVoltage, Tps65914PmuSetVoltage,
};

static void InitMapTps65914(NvU32 *pRailMap, NvOdmBoardInfo *pPmuBoard)
{
    pRailMap[LokiPowerRails_VDD_CORE]     = TPS65914PmuSupply_SMPS45;
    pRailMap[LokiPowerRails_VDDIO_SDMMC4] = TPS65914PmuSupply_SMPS8;
    pRailMap[LokiPowerRails_VDDIO_UART]   = TPS65914PmuSupply_SMPS8;
    pRailMap[LokiPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO3;
    pRailMap[LokiPowerRails_AVDD_LCD]     = TPS65914PmuSupply_LDO2;
    pRailMap[LokiPowerRails_VPP_FUSE]     = TPS65914PmuSupply_LDO4;
    pRailMap[LokiPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
    pRailMap[LokiPowerRails_VDD_3V3_SYS] = TPS65914PmuSupply_SMPS6;
}

void
*LokiPmu_GetTps65914RailMap(
    NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    NvOdmPmuDriverInfoHandle hPmuDriver = NULL;
    NvOdmDisplayBoardInfo DisplayBoardInfo;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);
    NV_ASSERT(pRailMap);
    NV_ASSERT(hDriverOps);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NULL;

    if (!pRailMap || !hDriverOps)
        return NULL;

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
            &DisplayBoardInfo, sizeof(DisplayBoardInfo)))
    {
        //JDI panel
        if (DisplayBoardInfo.BoardInfo.Fab == 0x01 ||
                DisplayBoardInfo.BoardInfo.Fab == 0x02 )
        {
            s_OdmTps65914RailProps[TPS65914PmuSupply_LDO2].Setup_mV = 3000;
        }
    }

    hPmuDriver = Tps65914PmuDriverOpen(hPmuDevice, s_OdmTps65914RailProps,
                TPS65914PmuSupply_Num);

    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps65914PmuDriverOpen() fails\n", __func__);
        return NULL;
    }

    //bug 1367567
    if(!Tps65914PmuSetLongPressKeyDuration(hPmuDriver, KeyDuration_EightSecs))
    {
        NvOdmOsPrintf("%s(): Failed to set long press key duration\n", __func__);
    }

    InitMapTps65914(pRailMap, pPmuBoard);

    *hDriverOps = &s_Tps65914Ops;
    return hPmuDriver;
}

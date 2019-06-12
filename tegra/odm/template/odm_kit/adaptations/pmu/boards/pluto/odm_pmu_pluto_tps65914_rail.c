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
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"

static Tps65914BoardData BoardData = { NV_TRUE };

static NvOdmPmuRailProps s_OdmRailProps[] = {
    {TPS65914PmuSupply_Invalid, PlutoPowerRails_Invalid, 0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, PlutoPowerRails_VDD_CPU,      0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS45,  PlutoPowerRails_VDD_CORE,     0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   PlutoPowerRails_VDD_CORE_BB,  0,     0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   PlutoPowerRails_VDDIO_DDR,    0,     0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   PlutoPowerRails_VDDIO_UART,   0,     0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS9,   PlutoPowerRails_VDD_EMMC,     0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS10,  PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    PlutoPowerRails_AVDD_PLLM,    0,     0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    PlutoPowerRails_AVDD_LCD,     0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    PlutoPowerRails_AVDD_DSI_CSI, 0,     0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    PlutoPowerRails_VDD_RTC,      0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    PlutoPowerRails_VDDIO_SDMMC3, 0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  PlutoPowerRails_AVDD_USB,     0,     0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   PlutoPowerRails_EN_VDD_1V8_LCD, 0,     0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   PlutoPowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
};

static PmuChipDriverOps s_tps65914Ops = {
    Tps65914PmuGetCaps, Tps65914PmuGetVoltage, Tps65914PmuSetVoltage,
};

static void InitMap(NvU32 *pRailMap)
{
    pRailMap[PlutoPowerRails_VDD_CPU] = TPS65914PmuSupply_SMPS123;
    pRailMap[PlutoPowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS45;
    pRailMap[PlutoPowerRails_VDD_CORE_BB] = TPS65914PmuSupply_SMPS6;
    pRailMap[PlutoPowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS7;
    pRailMap[PlutoPowerRails_VDDIO_UART] = TPS65914PmuSupply_SMPS8;
    pRailMap[PlutoPowerRails_VDD_EMMC] = TPS65914PmuSupply_SMPS9;
    pRailMap[PlutoPowerRails_VDD_LCD] = TPS65914PmuSupply_SMPS8;
    pRailMap[PlutoPowerRails_VPP_FUSE] = TPS65914PmuSupply_SMPS8;
    pRailMap[PlutoPowerRails_AVDD_CSI_DSI_PLL] = TPS65914PmuSupply_LDO1;
    pRailMap[PlutoPowerRails_AVDD_PLLM] = TPS65914PmuSupply_LDO1;
    pRailMap[PlutoPowerRails_AVDD_PLLX] = TPS65914PmuSupply_LDO1;
    pRailMap[PlutoPowerRails_AVDD_LCD] = TPS65914PmuSupply_LDO2;
    pRailMap[PlutoPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO3;
    pRailMap[PlutoPowerRails_VDD_RTC] = TPS65914PmuSupply_LDO8;
    pRailMap[PlutoPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
    pRailMap[PlutoPowerRails_AVDD_USB] = TPS65914PmuSupply_LDOUSB;
    pRailMap[PlutoPowerRails_EN_VDD_1V8_LCD] = TPS65914PmuSupply_GPIO4;
}

void *PlutoPmu_GetTps65914Map(NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps, NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
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

    hPmuDriver = Tps65914PmuDriverOpen(hPmuDevice, s_OdmRailProps, TPS65914PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps65914Setup() fails\n", __func__);
        return NULL;
    }

    InitMap(pRailMap);

    *hDriverOps = &s_tps65914Ops;
    return hPmuDriver;
}

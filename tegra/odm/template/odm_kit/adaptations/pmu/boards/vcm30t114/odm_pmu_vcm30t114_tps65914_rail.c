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
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"

static Tps65914BoardData BoardData = { NV_TRUE };

static NvOdmPmuRailProps s_OdmTps65914RailProps[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */

    {TPS65914PmuSupply_Invalid, VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  VCM30T114PowerRails_VDDIO_DDR,    0,     0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   VCM30T114PowerRails_VDDIO_SDMMC1, 0,     0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS45,  VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, VCM30T114PowerRails_VDD_CORE,     0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   VCM30T114PowerRails_AVDD_PLLM,    0,     0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS9,   VCM30T114PowerRails_VDD_EMMC_CORE,0,     0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS10,  VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    VCM30T114PowerRails_AVDD_CAM1,    0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    VCM30T114PowerRails_AVDD_CAM2,    0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    VCM30T114PowerRails_VDDIO_MIPI,   0,     0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    VCM30T114PowerRails_VPP_FUSE,      0,     0,    1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    VCM30T114PowerRails_Invalid,      0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    VCM30T114PowerRails_VDD_SENSOR_2V8,0,    0, 2900, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    VCM30T114PowerRails_Invalid,      0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    VCM30T114PowerRails_VDD_RTC,      0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    VCM30T114PowerRails_VDDIO_SDMMC3, 0,     0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  VCM30T114PowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static PmuChipDriverOps s_Tps65914Ops = {
    Tps65914PmuGetCaps, Tps65914PmuGetVoltage, Tps65914PmuSetVoltage,
};

static void InitMapTps65914(NvU32 *pRailMap)
{
    pRailMap[VCM30T114PowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS12;
    pRailMap[VCM30T114PowerRails_VDDIO_SDMMC1] = TPS65914PmuSupply_SMPS3;
    pRailMap[VCM30T114PowerRails_VDDIO_UART] = TPS65914PmuSupply_SMPS3;
    pRailMap[VCM30T114PowerRails_DVDD_LCD] = TPS65914PmuSupply_SMPS3;
    pRailMap[VCM30T114PowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS457;
    pRailMap[VCM30T114PowerRails_AVDD_PLLA_P_C] = TPS65914PmuSupply_SMPS8;
    pRailMap[VCM30T114PowerRails_AVDD_PLLM] = TPS65914PmuSupply_SMPS8;
    pRailMap[VCM30T114PowerRails_VDD_DDR_HS] = TPS65914PmuSupply_SMPS8;
    pRailMap[VCM30T114PowerRails_AVDDIO_USB3] = TPS65914PmuSupply_SMPS8;
    pRailMap[VCM30T114PowerRails_VDD_EMMC_CORE] = TPS65914PmuSupply_SMPS9;
    pRailMap[VCM30T114PowerRails_AVDD_CAM1] = TPS65914PmuSupply_LDO1;
    pRailMap[VCM30T114PowerRails_AVDD_CAM2] = TPS65914PmuSupply_LDO2;
    pRailMap[VCM30T114PowerRails_VDDIO_MIPI] = TPS65914PmuSupply_LDO3;
    pRailMap[VCM30T114PowerRails_VPP_FUSE] = TPS65914PmuSupply_LDO4;
    pRailMap[VCM30T114PowerRails_VDD_SENSOR_2V8] = TPS65914PmuSupply_LDO6;
    pRailMap[VCM30T114PowerRails_VDD_RTC] = TPS65914PmuSupply_LDO8;
    pRailMap[VCM30T114PowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
}

void
*VCM30T114Pmu_GetTps65914RailMap(
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

    hPmuDriver = Tps65914PmuDriverOpen(hPmuDevice, s_OdmTps65914RailProps,
                                       TPS65914PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps65914PmuDriverOpen() fails\n", __func__);
        return NULL;
    }

    InitMapTps65914(pRailMap);

    *hDriverOps = &s_Tps65914Ops;
    return hPmuDriver;
}

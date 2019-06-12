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
#include "nvodm_pmu_dalmore.h"
#include "dalmore_pmu.h"
#include "nvodm_pmu_dalmore_supply_info_table.h"
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"

static Tps65914BoardData BoardData = { NV_TRUE };

static NvOdmPmuRailProps s_OdmTps65914RailProps[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */

    {TPS65914PmuSupply_Invalid, DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  DalmorePowerRails_VDDIO_DDR,    0,     0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   DalmorePowerRails_VDDIO_SDMMC1, 0,     0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS45,  DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, DalmorePowerRails_VDD_CORE,     0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   DalmorePowerRails_AVDD_PLLM,    0,     0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS9,   DalmorePowerRails_VDD_EMMC_CORE,0,     0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS10,  DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    DalmorePowerRails_AVDD_CAM1,    0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    DalmorePowerRails_AVDD_CAM2,    0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    DalmorePowerRails_VDDIO_MIPI,   0,     0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    DalmorePowerRails_VPP_FUSE,      0,     0,    1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    DalmorePowerRails_Invalid,      0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    DalmorePowerRails_VDD_SENSOR_2V8,0,    0, 2900, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    DalmorePowerRails_Invalid,      0,     0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    DalmorePowerRails_VDD_RTC,      0,     0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    DalmorePowerRails_VDDIO_SDMMC3, 0,     0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  DalmorePowerRails_Invalid,      0,     0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   DalmorePowerRails_Invalid,      0,      0,        0,   NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static PmuChipDriverOps s_Tps65914Ops = {
    Tps65914PmuGetCaps, Tps65914PmuGetVoltage, Tps65914PmuSetVoltage,
};

static void InitMapTps65914(NvU32 *pRailMap)
{
    pRailMap[DalmorePowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS12;
    pRailMap[DalmorePowerRails_VDDIO_SDMMC1] = TPS65914PmuSupply_SMPS3;
    pRailMap[DalmorePowerRails_VDDIO_UART] = TPS65914PmuSupply_SMPS3;
    pRailMap[DalmorePowerRails_DVDD_LCD] = TPS65914PmuSupply_SMPS3;
    pRailMap[DalmorePowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS457;
    pRailMap[DalmorePowerRails_AVDD_PLLA_P_C] = TPS65914PmuSupply_SMPS8;
    pRailMap[DalmorePowerRails_AVDD_PLLM] = TPS65914PmuSupply_SMPS8;
    pRailMap[DalmorePowerRails_VDD_DDR_HS] = TPS65914PmuSupply_SMPS8;
    pRailMap[DalmorePowerRails_AVDDIO_USB3] = TPS65914PmuSupply_SMPS8;
    pRailMap[DalmorePowerRails_VDD_EMMC_CORE] = TPS65914PmuSupply_SMPS9;
    pRailMap[DalmorePowerRails_AVDD_CAM1] = TPS65914PmuSupply_LDO1;
    pRailMap[DalmorePowerRails_AVDD_CAM2] = TPS65914PmuSupply_LDO2;
    pRailMap[DalmorePowerRails_VDDIO_MIPI] = TPS65914PmuSupply_LDO3;
    pRailMap[DalmorePowerRails_VPP_FUSE] = TPS65914PmuSupply_LDO4;
    pRailMap[DalmorePowerRails_VDD_SENSOR_2V8] = TPS65914PmuSupply_LDO6;
    pRailMap[DalmorePowerRails_VDD_RTC] = TPS65914PmuSupply_LDO8;
    pRailMap[DalmorePowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
}

void
*DalmorePmu_GetTps65914RailMap(
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

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
#include "nvodm_pmu_ardbeg.h"
#include "ardbeg_pmu.h"
#include "nvodm_pmu_ardbeg_supply_info_table.h"
#include "tps80036/nvodm_pmu_tps80036_pmu_driver.h"

static NvOdmPmuRailProps s_OdmRailProps[] = {
     /*VddId                                                    Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                           Max_mV      IsEnable        IsOdmProtected         */
    {TPS80036PmuSupply_Invalid, ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_SMPS1_2, ArdbegPowerRails_VDD_CORE,      0, 0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_SMPS3,   ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_SMPS6,   ArdbegPowerRails_VDDIO_DDR,     0, 0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_SMPS7,   ArdbegPowerRails_VDD_EMMC_CORE, 0, 0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_SMPS8,   ArdbegPowerRails_Invalid,       0, 0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_SMPS9,   ArdbegPowerRails_VDDIO_SYS,      0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO1,    ArdbegPowerRails_VD_LCD_HV,     0, 0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO2,    ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO3,    ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO4,    ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO5,    ArdbegPowerRails_VDD_RTC,       0, 0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO6,    ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO7,    ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO8,    ArdbegPowerRails_Invalid,       0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO9,    ArdbegPowerRails_AVDD_DSI_CSI,  0, 0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO10,   ArdbegPowerRails_VDDIO_SDMMC3,  0, 0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO11,   ArdbegPowerRails_AVDD_USB,      0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO12,   ArdbegPowerRails_VDD_LCD_DIS, 0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO13,   ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDO14,   ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDOLN,   ArdbegPowerRails_Invalid,       0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS80036PmuSupply_LDOUSB,  ArdbegPowerRails_VPP_FUSE,      0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
};

static PmuChipDriverOps s_tps80036Ops = {
    Tps80036PmuGetCaps, Tps80036PmuGetVoltage, Tps80036PmuSetVoltage,
};

static void InitMap(NvU32 *pRailMap)
{
    pRailMap[ArdbegPowerRails_VDD_CORE] = TPS80036PmuSupply_SMPS1_2;
    pRailMap[ArdbegPowerRails_VDDIO_DDR] = TPS80036PmuSupply_SMPS6;
    pRailMap[ArdbegPowerRails_VDD_EMMC_CORE] = TPS80036PmuSupply_SMPS7;
    pRailMap[ArdbegPowerRails_VDDIO_SYS] = TPS80036PmuSupply_SMPS9;
    pRailMap[ArdbegPowerRails_VDDIO_SDMMC4] = TPS80036PmuSupply_SMPS9;
    pRailMap[ArdbegPowerRails_VDDIO_UART] = TPS80036PmuSupply_SMPS9;
    pRailMap[ArdbegPowerRails_VDDIO_BB] = TPS80036PmuSupply_SMPS9;
    pRailMap[ArdbegPowerRails_VD_LCD_HV] = TPS80036PmuSupply_LDO1;
    pRailMap[ArdbegPowerRails_VDD_RTC] = TPS80036PmuSupply_LDO5;
    pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = TPS80036PmuSupply_LDO9;
    pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = TPS80036PmuSupply_LDO10;
    pRailMap[ArdbegPowerRails_AVDD_USB] = TPS80036PmuSupply_LDO11;
    pRailMap[ArdbegPowerRails_VDD_LCD_DIS] = TPS80036PmuSupply_LDO12;
    pRailMap[ArdbegPowerRails_VPP_FUSE] = TPS80036PmuSupply_LDOUSB;
}

void *ArdbegPmu_GetTps80036RailMap(NvOdmPmuDeviceHandle hPmuDevice,
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

    hPmuDriver = Tps80036PmuDriverOpen(hPmuDevice, s_OdmRailProps, TPS80036PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps80036Setup() fails\n", __func__);
        return NULL;
    }

    InitMap(pRailMap);

    *hDriverOps = &s_tps80036Ops;
    return hPmuDriver;
}

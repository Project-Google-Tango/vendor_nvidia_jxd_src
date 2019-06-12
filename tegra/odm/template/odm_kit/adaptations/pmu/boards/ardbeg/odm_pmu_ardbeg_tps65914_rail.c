/*
 * Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
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
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"

static Tps65914BoardData BoardData = { NV_TRUE };

static NvOdmPmuRailProps s_OdmTps65914RailProps_e1735[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {TPS65914PmuSupply_Invalid, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS45,  ArdbegPowerRails_VDD_CORE,   0,    0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   ArdbegPowerRails_VDDIO_UART, 0,    0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS9,   ArdbegPowerRails_VDDIO_SATA, 0,    0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS10,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    ArdbegPowerRails_AVDD_DSI_CSI,0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    ArdbegPowerRails_VDDIO_SDMMC3, 0,  0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  ArdbegPowerRails_VPP_FUSE,   0,    0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static NvOdmPmuRailProps s_OdmTps65914RailProps_e1736[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {TPS65914PmuSupply_Invalid, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, ArdbegPowerRails_VDD_CPU,    0,    0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS45,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   ArdbegPowerRails_VDDIO_DDR,  0,    0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   ArdbegPowerRails_VDD_CORE,   0,    0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS9,   ArdbegPowerRails_VD_LCD_HV,  0,    0, 3000, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS10,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    ArdbegPowerRails_AVDD_DSI_CSI,0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    ArdbegPowerRails_VDDIO_SDMMC3, 0,  0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  ArdbegPowerRails_VPP_FUSE,   0,    0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static NvOdmPmuRailProps s_OdmTps65914RailProps_e1936[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {TPS65914PmuSupply_Invalid, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, ArdbegPowerRails_VDD_CPU,    0,    0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS45,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   ArdbegPowerRails_VDDIO_DDR,  0,    0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   ArdbegPowerRails_VDD_CORE,   0,    0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS9,   ArdbegPowerRails_VD_LCD_HV,  0,    0, 3000, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS10,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    ArdbegPowerRails_AVDD_DSI_CSI,0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    ArdbegPowerRails_VDDIO_SDMMC3, 0,  0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  ArdbegPowerRails_VPP_FUSE,   0,    0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static NvOdmPmuRailProps s_OdmTps65914RailProps_e1769[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {TPS65914PmuSupply_Invalid, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS12,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS123, ArdbegPowerRails_VDD_CPU,    0,    0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS45,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS457, ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS6,   ArdbegPowerRails_VDDIO_DDR,  0,    0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS7,   ArdbegPowerRails_VDD_CORE,   0,    0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS8,   ArdbegPowerRails_LCD_1V8,    0,    0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, &BoardData},
    {TPS65914PmuSupply_SMPS9,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_SMPS10,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO1,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO2,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO3,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO4,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO5,    ArdbegPowerRails_AVDD_DSI_CSI,0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO6,    ArdbegPowerRails_VD_LCD_HV,  0,    0, 3000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO7,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO8,    ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDO9,    ArdbegPowerRails_VDDIO_SDMMC3, 0,  0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOLN,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_LDOUSB,  ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO0,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO1,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO2,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO3,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO4,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO5,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO6,   ArdbegPowerRails_EN_LCD_1V8, 0,    0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65914PmuSupply_GPIO7,   ArdbegPowerRails_Invalid,    0,    0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

};

static PmuChipDriverOps s_Tps65914Ops = {
    Tps65914PmuGetCaps, Tps65914PmuGetVoltage, Tps65914PmuSetVoltage,
};

static void InitMapTps65914(NvU32 *pRailMap, NvOdmBoardInfo *pPmuBoard)
{

    if (pPmuBoard->BoardID == PMU_E1735)
    {
        pRailMap[ArdbegPowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS45;
        pRailMap[ArdbegPowerRails_VDDIO_SDMMC4] = TPS65914PmuSupply_SMPS6;
        pRailMap[ArdbegPowerRails_VDDIO_UART] = TPS65914PmuSupply_SMPS6;
        pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO4;
        pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
        pRailMap[ArdbegPowerRails_VPP_FUSE] = TPS65914PmuSupply_LDOUSB;
        pRailMap[ArdbegPowerRails_VDDIO_SATA] = TPS65914PmuSupply_SMPS9;
        pRailMap[ArdbegPowerRails_AVDD_HDMI] = TPS65914PmuSupply_SMPS9;
    }
    else if (pPmuBoard->BoardID == PMU_E1736)
    {
        pRailMap[ArdbegPowerRails_VDD_CPU] = TPS65914PmuSupply_SMPS123;
        pRailMap[ArdbegPowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS7;
        pRailMap[ArdbegPowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS6;
        pRailMap[ArdbegPowerRails_VD_LCD_HV] = TPS65914PmuSupply_SMPS9;
        pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO5;
        pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
        pRailMap[ArdbegPowerRails_VPP_FUSE] = TPS65914PmuSupply_LDOUSB;
    }
    else if (pPmuBoard->BoardID == PMU_E1769)
    {
        pRailMap[ArdbegPowerRails_VDD_CPU] = TPS65914PmuSupply_SMPS123;
        pRailMap[ArdbegPowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS7;
        pRailMap[ArdbegPowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS6;
        pRailMap[ArdbegPowerRails_LCD_1V8] = TPS65914PmuSupply_SMPS8;
        pRailMap[ArdbegPowerRails_VD_LCD_HV] = TPS65914PmuSupply_LDO6;
        pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO5;
        pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
        pRailMap[ArdbegPowerRails_EN_LCD_1V8] = TPS65914PmuSupply_GPIO6;
    }
    else if (pPmuBoard->BoardID == PMU_E1761)
    {
        pRailMap[ArdbegPowerRails_VDD_CPU] = TPS65914PmuSupply_SMPS123;
        pRailMap[ArdbegPowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS7;
        pRailMap[ArdbegPowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS6;
        pRailMap[ArdbegPowerRails_LCD_1V8] = TPS65914PmuSupply_SMPS8;
        pRailMap[ArdbegPowerRails_VDD_LCD_DIS] = TPS65914PmuSupply_LDO2;
        pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO5;
        pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
        pRailMap[ArdbegPowerRails_EN_LCD_1V8] = TPS65914PmuSupply_GPIO6;
        pRailMap[ArdbegPowerRails_VPP_FUSE] = TPS65914PmuSupply_LDO6;
    }
    else if (pPmuBoard->BoardID == PMU_E1936)
    {
        pRailMap[ArdbegPowerRails_VDD_CPU] = TPS65914PmuSupply_SMPS123;
        pRailMap[ArdbegPowerRails_VDD_CORE] = TPS65914PmuSupply_SMPS7;
        pRailMap[ArdbegPowerRails_VDDIO_DDR] = TPS65914PmuSupply_SMPS6;
        pRailMap[ArdbegPowerRails_VD_LCD_HV] = TPS65914PmuSupply_SMPS9;
        pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = TPS65914PmuSupply_LDO5;
        pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = TPS65914PmuSupply_LDO9;
        pRailMap[ArdbegPowerRails_VPP_FUSE] = TPS65914PmuSupply_LDOUSB;
    }
}

void
*ArdbegPmu_GetTps65914RailMap(
    NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    NvOdmPmuDriverInfoHandle hPmuDriver = NULL;
    NvU32 i;
    NvOdmPmuRailProps *s_OdmRailProps = NULL;

    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);
    NV_ASSERT(pRailMap);
    NV_ASSERT(hDriverOps);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NULL;

    if (!pRailMap || !hDriverOps)
        return NULL;

    if (pPmuBoard->BoardID == PMU_E1735)
    {
        if (pProcBoard->BoardID == PROC_BOARD_E1792)
        {
        //LPDDR3 supports nominal 1.2V DDR voltage
            for (i = 0; i < ArdbegPowerRails_Num; ++i)
            {
                if(s_OdmTps65914RailProps_e1735[i].SystemRailId ==
                                              ArdbegPowerRails_VDDIO_DDR)
                {
                    s_OdmTps65914RailProps_e1735[i].Setup_mV = 1200;
                    break;
                }
            }
        }
        s_OdmRailProps = s_OdmTps65914RailProps_e1735;
    }
    else if (pPmuBoard->BoardID == PMU_E1736)
    {
        if (pProcBoard->BoardID == PROC_BOARD_E1791)
        {
            //LPDDR3 supports nominal 1.2V DDR voltage
            for (i = 0; i < ArdbegPowerRails_Num; ++i)
            {
                if(s_OdmTps65914RailProps_e1736[i].SystemRailId ==
                                              ArdbegPowerRails_VDDIO_DDR)
                {
                    s_OdmTps65914RailProps_e1736[i].Setup_mV = 1200;
                    break;
                }
            }
        }
        s_OdmRailProps = s_OdmTps65914RailProps_e1736;
    }

    else if (pPmuBoard->BoardID == PMU_E1761)
    {
        //LPDDR3 supports nominal 1.2V DDR voltage
        //Add LCD_DIS in LDO2
        for (i = 0; i < ArdbegPowerRails_Num; ++i)
        {
            if(s_OdmTps65914RailProps_e1769[i].SystemRailId ==
                                           ArdbegPowerRails_VDDIO_DDR)
            {
                s_OdmTps65914RailProps_e1769[i].Setup_mV = 1200;
            }
            else if (s_OdmTps65914RailProps_e1769[i].VddId ==
                                           TPS65914PmuSupply_LDO2)
            {
                s_OdmTps65914RailProps_e1769[i].SystemRailId = ArdbegPowerRails_VDD_LCD_DIS;
                s_OdmTps65914RailProps_e1769[i].Setup_mV = 3300;
            }
            else if (s_OdmTps65914RailProps_e1769[i].VddId ==
                                           TPS65914PmuSupply_LDO6)
            {
                s_OdmTps65914RailProps_e1769[i].SystemRailId = ArdbegPowerRails_VPP_FUSE;
                s_OdmTps65914RailProps_e1769[i].Setup_mV = 1800;
            }
        }
        s_OdmRailProps = s_OdmTps65914RailProps_e1769;
    }

    else if (pPmuBoard->BoardID == PMU_E1769)
    {
        s_OdmRailProps = s_OdmTps65914RailProps_e1769;
    }
    else if (pPmuBoard->BoardID == PMU_E1936)
    {
        s_OdmRailProps = s_OdmTps65914RailProps_e1936;
    }

    hPmuDriver = Tps65914PmuDriverOpen(hPmuDevice, s_OdmRailProps, TPS65914PmuSupply_Num);
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

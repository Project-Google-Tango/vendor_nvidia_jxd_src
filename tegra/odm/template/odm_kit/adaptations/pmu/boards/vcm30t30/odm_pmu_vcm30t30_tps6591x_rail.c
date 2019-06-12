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
#include "tps6591x/nvodm_pmu_tps6591x_pmu_driver.h"

static NvOdmPmuRailProps s_OdmRailProps[] = {
    {TPS6591xPmuSupply_Invalid, Vcm30T30PowerRails_Invalid, 0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_VIO, Vcm30T30PowerRails_VDDIO_SYS, 0, 0, 1800, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_VDD1, Vcm30T30PowerRails_VDD_PEXB, 0, 0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_VDD2, Vcm30T30PowerRails_MEM_VDDIO_DDR, 0, 0, 1500, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_VDDCTRL, Vcm30T30PowerRails_VDD_CORE, 0, 0, 1250, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_LDO1, Vcm30T30PowerRails_AVDD_PLLE, 0, 0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_LDO2, Vcm30T30PowerRails_AVDD_DSI_CSI, 0, 0, 1200, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_LDO3, Vcm30T30PowerRails_Invalid, 0, 0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_LDO4, Vcm30T30PowerRails_Invalid, 0, 0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_LDO5, Vcm30T30PowerRails_VDD_DDR_HS, 0, 0, 1000, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_LDO6, Vcm30T30PowerRails_VDD_1V8_PLL, 0, 0, 1800, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_LDO7, Vcm30T30PowerRails_VDD_DDR_RX, 0, 0, 2850, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_LDO8, Vcm30T30PowerRails_Invalid, 0, 0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_RTC_OUT, Vcm30T30PowerRails_Invalid, 0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO0, Vcm30T30PowerRails_EN_PMU_3V3, 0, 0, 3300, NV_FALSE, NV_FALSE, NV_TRUE, NULL},
    {TPS6591xPmuSupply_GPO1, Vcm30T30PowerRails_EN_VDD_CPU_DVS, 0, 0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO2, Vcm30T30PowerRails_Invalid, 0, 0, 5000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO3, Vcm30T30PowerRails_Invalid, 0, 0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO4, Vcm30T30PowerRails_Invalid, 0, 0, 5000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO5, Vcm30T30PowerRails_Invalid, 0, 0, 5000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO6, Vcm30T30PowerRails_Invalid, 0, 0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO7, Vcm30T30PowerRails_Invalid, 0, 0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS6591xPmuSupply_GPO8, Vcm30T30PowerRails_Invalid, 0, 0, 5000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
};

static PmuChipDriverOps s_tps6591xOps = {
    Tps6591xPmuGetCaps, Tps6591xPmuGetVoltage, Tps6591xPmuSetVoltage,
};

static void InitMap(NvU32 *pRailMap)
{
    pRailMap[Vcm30T30PowerRails_VDD_CORE] = TPS6591xPmuSupply_VDDCTRL;
    pRailMap[Vcm30T30PowerRails_VDD_RTC] =  TPS6591xPmuSupply_VDDCTRL;
    pRailMap[Vcm30T30PowerRails_AVDD_PLLU_D] = TPS6591xPmuSupply_LDO2;
    pRailMap[Vcm30T30PowerRails_AVDD_PLLU_D2] = TPS6591xPmuSupply_LDO2;
    pRailMap[Vcm30T30PowerRails_AVDD_PLLX] = TPS6591xPmuSupply_LDO2;
    pRailMap[Vcm30T30PowerRails_AVDD_PLLM] = TPS6591xPmuSupply_LDO2;
    pRailMap[Vcm30T30PowerRails_AVDD_DSI_CSI] = TPS6591xPmuSupply_LDO2;
    pRailMap[Vcm30T30PowerRails_AVDD_OSC] = TPS6591xPmuSupply_LDO6;
    pRailMap[Vcm30T30PowerRails_AVDD_USB] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_AVDD_USB_PLL] = TPS6591xPmuSupply_LDO6;
    pRailMap[Vcm30T30PowerRails_AVDD_HDMI] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_AVDD_HDMI_PLL] = TPS6591xPmuSupply_LDO6;
    pRailMap[Vcm30T30PowerRails_AVDD_PEXB] = TPS6591xPmuSupply_VDD1;
    pRailMap[Vcm30T30PowerRails_AVDD_PEXA] = TPS6591xPmuSupply_VDD1;
    pRailMap[Vcm30T30PowerRails_AVDD_PEX_PLL] = TPS6591xPmuSupply_LDO1;
    pRailMap[Vcm30T30PowerRails_AVDD_PLLE] = TPS6591xPmuSupply_LDO1;
    pRailMap[Vcm30T30PowerRails_VDD_PEXB] = TPS6591xPmuSupply_VDD1;
    pRailMap[Vcm30T30PowerRails_VDD_PEXA] = TPS6591xPmuSupply_VDD1;
    pRailMap[Vcm30T30PowerRails_VDDIO_PEXCTL] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_HVDD_PEX] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_MEM_VDDIO_DDR] = TPS6591xPmuSupply_VDD2;
    pRailMap[Vcm30T30PowerRails_VDD_DDR_RX] = TPS6591xPmuSupply_LDO7;
    pRailMap[Vcm30T30PowerRails_VDD_DDR_HS] = TPS6591xPmuSupply_LDO5;
    pRailMap[Vcm30T30PowerRails_VDDIO_GMI_PMU] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_SYS] = TPS6591xPmuSupply_VIO;
    pRailMap[Vcm30T30PowerRails_VDDIO_CAM] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_BB] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_SDMMC4] = TPS6591xPmuSupply_VIO;
    pRailMap[Vcm30T30PowerRails_VDDIO_SDMMC1] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_SDMMC3] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_LCD_PMU] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_UART] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_VDDIO_VI] = TPS6591xPmuSupply_VIO;
    pRailMap[Vcm30T30PowerRails_VDDIO_AUDIO] = TPS6591xPmuSupply_GPO0;
    pRailMap[Vcm30T30PowerRails_AVDD_PLL_A_P_C_S] = TPS6591xPmuSupply_LDO2;
}

void *Vcm30T30Pmu_GetTps6591xMap(NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps, NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{

    NvOdmPmuDriverInfoHandle hPmuDriver = NULL;

    hPmuDriver = Tps6591xPmuDriverOpen(hDevice, s_OdmRailProps, TPS6591xPmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps6591xSetup() fails\n", __func__);
        return NULL;
    }

    InitMap(pRailMap);

    *hDriverOps = &s_tps6591xOps;
    return hPmuDriver;
}

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
#include "as3722/nvodm_pmu_as3722_pmu_driver.h"

static NvOdmPmuRailProps s_OdmRailProps[] = {
     /*VddId                                              Min_mV    Setup_mV      IsAlwaysOn         BoardData */
     /*                         SystemRailId                   Max_mV    IsEnable        IsOdmProtected         */
    {AS3722PmuSupply_Invalid, ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO0,    ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO1,    ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO2,    ArdbegPowerRails_AVDD_DSI_CSI, 0, 0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO3,    ArdbegPowerRails_VDD_RTC,      0, 0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO4,    ArdbegPowerRails_Invalid,      0, 0,    0,  NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO5,    ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO6,    ArdbegPowerRails_VDDIO_SDMMC3, 0, 0, 3300, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO7,    ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO9,    ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO10,   ArdbegPowerRails_VDD_LCD_DIS,  0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_LDO11,   ArdbegPowerRails_VPP_FUSE,     0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD0,     ArdbegPowerRails_VDD_CPU,      0, 0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD1,     ArdbegPowerRails_VDD_CORE,     0, 0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD2,     ArdbegPowerRails_VDDIO_DDR,    0, 0, 1350, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD3,     ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD4,     ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD5,     ArdbegPowerRails_VDDIO_UART,   0, 0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {AS3722PmuSupply_SD6,     ArdbegPowerRails_Invalid,      0, 0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
};

static PmuChipDriverOps s_As3722Ops = {
    As3722PmuGetCaps, As3722PmuGetVoltage, As3722PmuSetVoltage,
};


#define PROC_BOARD_E1780    0x06F4 // ERS_Main
#define PROC_BOARD_E1781    0x06F5 //ERS-s_Main

/* Laguna Processor Board ID */
#define PROC_BOARD_PM358    0x0166 //ERS_Main
#define PROC_BOARD_PM359    0x0167 //ERS-s_Main
#define PROC_BOARD_PM363    0x016B // FFD
/* Norrin Processor Board ID */
#define PROC_BOARD_PM374    0x0176 // FFD
#define PROC_BOARD_PM370    0x0172 // FFD


static void InitMap(NvU32 *pRailMap)
{
    pRailMap[ArdbegPowerRails_VDD_CPU]      = AS3722PmuSupply_SD0;
    pRailMap[ArdbegPowerRails_VDD_CORE]     = AS3722PmuSupply_SD1;
    pRailMap[ArdbegPowerRails_VDDIO_DDR]    = AS3722PmuSupply_SD2;
    pRailMap[ArdbegPowerRails_VDDIO_SDMMC4] = AS3722PmuSupply_SD5;
    pRailMap[ArdbegPowerRails_VDDIO_SYS]    = AS3722PmuSupply_SD5;
    pRailMap[ArdbegPowerRails_VDDIO_UART]   = AS3722PmuSupply_SD5;
    pRailMap[ArdbegPowerRails_VDDIO_BB]     = AS3722PmuSupply_SD5;
    pRailMap[ArdbegPowerRails_AVDD_DSI_CSI] = AS3722PmuSupply_LDO2;
    pRailMap[ArdbegPowerRails_VDD_RTC]      = AS3722PmuSupply_LDO3;
    pRailMap[ArdbegPowerRails_VDDIO_SDMMC3] = AS3722PmuSupply_LDO6;
    pRailMap[ArdbegPowerRails_VDD_LCD_DIS]  = AS3722PmuSupply_LDO10;
    pRailMap[ArdbegPowerRails_VPP_FUSE]     = AS3722PmuSupply_LDO11;
}

void *ArdbegPmu_GetAs3722RailMap(NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps, NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard, NvOdmBoardInfo *pPmuBoard)
{
    NvOdmPmuDriverInfoHandle hPmuDriver = NULL;
    NvU32 i;

     NV_ASSERT(hPmuDevice);
     NV_ASSERT(pProcBoard);
     NV_ASSERT(pPmuBoard);
     NV_ASSERT(pRailMap);
     NV_ASSERT(hDriverOps);

     if (!hPmuDevice || !pProcBoard || !pPmuBoard)
         return NULL;

     if (!pRailMap || !hDriverOps)
         return NULL;

     if (pProcBoard->BoardID == PROC_BOARD_E1792)
     {
         //LPDDR3 supports nominal 1.2V DDR voltage
         for (i = 0; i < ArdbegPowerRails_Num; ++i)
         {
             if(s_OdmRailProps[i].SystemRailId ==
                                           ArdbegPowerRails_VDDIO_DDR)
             {
                 s_OdmRailProps[i].Setup_mV = 1200;
                 break;
             }
         }
     }
    hPmuDriver = As3722PmuDriverOpen(hPmuDevice, s_OdmRailProps, AS3722PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The AS3722Setup() fails\n", __func__);
        return NULL;
    }

    InitMap(pRailMap);

    *hDriverOps = &s_As3722Ops;
    return hPmuDriver;
}


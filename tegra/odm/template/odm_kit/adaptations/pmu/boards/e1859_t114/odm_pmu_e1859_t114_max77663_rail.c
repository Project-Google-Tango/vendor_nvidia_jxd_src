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
#include "nvodm_pmu_e1859_t114.h"
#include "e1859_t114_pmu.h"
#include "nvodm_pmu_e1859_t114_supply_info_table.h"
#include "max77663/nvodm_pmu_max77663_pmu_driver.h"

static NvOdmPmuRailProps s_OdmMax77663RailProps[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {Max77663PmuSupply_Invalid, E1859PowerRails_Invalid,      0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD0,  E1859PowerRails_VDD_CORE,        0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD1,  E1859PowerRails_VDDIO_DDR,       0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD2,  E1859PowerRails_VDDIO_SDMMC1,    0,   0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD3,  E1859PowerRails_VDD_EMMC_CORE,   0,   0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO0, E1859PowerRails_AVDD_PLLA_P_C,   0,   0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO1, E1859PowerRails_VDD_DDR_HS,      0,   0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO2, E1859PowerRails_VDD_SENSOR_2V8,  0,   0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO3, E1859PowerRails_AVDDIO_USB3,     0,   0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO4, E1859PowerRails_VDD_RTC,         0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO5, E1859PowerRails_VDDIO_MIPI,      0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO6, E1859PowerRails_VDDIO_SDMMC3,    0,   0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO7, E1859PowerRails_AVDD_CAM1,       0,   0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO8, E1859PowerRails_AVDD_CAM2,       0,   0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

    {Max77663PmuSupply_GPIO0, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO1, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO2, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO3, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO4, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO5, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO6, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO7, E1859PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},


};

static PmuChipDriverOps s_Max77663OdmProps = {
    Max77663PmuGetCaps, Max77663PmuGetVoltage, Max77663PmuSetVoltage,
};

static void InitMapMax77663(NvU32 *pRailMap)
{
    pRailMap[E1859PowerRails_VDD_CORE] = Max77663PmuSupply_SD0;
        pRailMap[E1859PowerRails_VDDIO_DDR] = Max77663PmuSupply_SD1;
        pRailMap[E1859PowerRails_VDDIO_SDMMC1] = Max77663PmuSupply_SD2;
        pRailMap[E1859PowerRails_VDDIO_UART] = Max77663PmuSupply_SD2;
        pRailMap[E1859PowerRails_DVDD_LCD] = Max77663PmuSupply_SD2;
        pRailMap[E1859PowerRails_VPP_FUSE] = Max77663PmuSupply_SD2;

        pRailMap[E1859PowerRails_VDD_EMMC_CORE] = Max77663PmuSupply_SD3;
        pRailMap[E1859PowerRails_AVDD_PLLA_P_C] = Max77663PmuSupply_LDO0;
        pRailMap[E1859PowerRails_AVDD_PLLM] = Max77663PmuSupply_LDO0;
        pRailMap[E1859PowerRails_VDD_DDR_HS] = Max77663PmuSupply_LDO1;
        pRailMap[E1859PowerRails_VDD_SENSOR_2V8] = Max77663PmuSupply_LDO2;
        pRailMap[E1859PowerRails_AVDDIO_USB3] = Max77663PmuSupply_LDO3;
        pRailMap[E1859PowerRails_VDDIO_MIPI] = Max77663PmuSupply_LDO5;
        pRailMap[E1859PowerRails_VDD_RTC] = Max77663PmuSupply_LDO4;
        pRailMap[E1859PowerRails_VDDIO_SDMMC3] = Max77663PmuSupply_LDO6;
        pRailMap[E1859PowerRails_AVDD_CAM1] = Max77663PmuSupply_LDO7;
        pRailMap[E1859PowerRails_AVDD_CAM2] = Max77663PmuSupply_LDO8;
}

void
*E1859Pmu_GetMax77663RailMap(
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

    hPmuDriver = Max77663PmuDriverOpen(hPmuDevice, s_OdmMax77663RailProps,
                                       Max77663PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Max77663PmuDriverOpen() fails\n", __func__);
        return NULL;
    }

    InitMapMax77663(pRailMap);

    *hDriverOps = &s_Max77663OdmProps;
    return hPmuDriver;
}

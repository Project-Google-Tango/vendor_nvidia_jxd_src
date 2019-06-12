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
#include "max77663/nvodm_pmu_max77663_pmu_driver.h"

static NvOdmPmuRailProps s_OdmMax77663RailProps[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {Max77663PmuSupply_Invalid, VCM30T114PowerRails_Invalid,      0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD0,  VCM30T114PowerRails_VDD_CORE,        0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD1,  VCM30T114PowerRails_VDDIO_DDR,       0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD2,  VCM30T114PowerRails_VDDIO_SDMMC1,    0,   0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD3,  VCM30T114PowerRails_VDD_EMMC_CORE,   0,   0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO0, VCM30T114PowerRails_AVDD_PLLA_P_C,   0,   0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO1, VCM30T114PowerRails_VDD_DDR_HS,      0,   0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO2, VCM30T114PowerRails_VDD_SENSOR_2V8,  0,   0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO3, VCM30T114PowerRails_AVDDIO_USB3,     0,   0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO4, VCM30T114PowerRails_VDD_RTC,         0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO5, VCM30T114PowerRails_VDDIO_MIPI,      0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO6, VCM30T114PowerRails_VDDIO_SDMMC3,    0,   0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO7, VCM30T114PowerRails_AVDD_CAM1,       0,   0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO8, VCM30T114PowerRails_AVDD_CAM2,       0,   0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

    {Max77663PmuSupply_GPIO0, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO1, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO2, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO3, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO4, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO5, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO6, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO7, VCM30T114PowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},


};

static PmuChipDriverOps s_Max77663OdmProps = {
    Max77663PmuGetCaps, Max77663PmuGetVoltage, Max77663PmuSetVoltage,
};

static void InitMapMax77663(NvU32 *pRailMap)
{
    pRailMap[VCM30T114PowerRails_VDD_CORE] = Max77663PmuSupply_SD0;
        pRailMap[VCM30T114PowerRails_VDDIO_DDR] = Max77663PmuSupply_SD1;
        pRailMap[VCM30T114PowerRails_VDDIO_SDMMC1] = Max77663PmuSupply_SD2;
        pRailMap[VCM30T114PowerRails_VDDIO_UART] = Max77663PmuSupply_SD2;
        pRailMap[VCM30T114PowerRails_DVDD_LCD] = Max77663PmuSupply_SD2;
        pRailMap[VCM30T114PowerRails_VPP_FUSE] = Max77663PmuSupply_SD2;

        pRailMap[VCM30T114PowerRails_VDD_EMMC_CORE] = Max77663PmuSupply_SD3;
        pRailMap[VCM30T114PowerRails_AVDD_PLLA_P_C] = Max77663PmuSupply_LDO0;
        pRailMap[VCM30T114PowerRails_AVDD_PLLM] = Max77663PmuSupply_LDO0;
        pRailMap[VCM30T114PowerRails_VDD_DDR_HS] = Max77663PmuSupply_LDO1;
        pRailMap[VCM30T114PowerRails_VDD_SENSOR_2V8] = Max77663PmuSupply_LDO2;
        pRailMap[VCM30T114PowerRails_AVDDIO_USB3] = Max77663PmuSupply_LDO3;
        pRailMap[VCM30T114PowerRails_VDDIO_MIPI] = Max77663PmuSupply_LDO5;
        pRailMap[VCM30T114PowerRails_VDD_RTC] = Max77663PmuSupply_LDO4;
        pRailMap[VCM30T114PowerRails_VDDIO_SDMMC3] = Max77663PmuSupply_LDO6;
        pRailMap[VCM30T114PowerRails_AVDD_CAM1] = Max77663PmuSupply_LDO7;
        pRailMap[VCM30T114PowerRails_AVDD_CAM2] = Max77663PmuSupply_LDO8;
}

void
*VCM30T114Pmu_GetMax77663RailMap(
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

/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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
#include "max77663/nvodm_pmu_max77663_pmu_driver.h"

static NvOdmPmuRailProps s_OdmMax77663RailProps[] = {
     /*VddId                                                Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                      Max_mV      IsEnable        IsOdmProtected         */
    {Max77663PmuSupply_Invalid, DalmorePowerRails_Invalid,      0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD0,  DalmorePowerRails_VDD_CORE,        0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD1,  DalmorePowerRails_VDDIO_DDR,       0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD2,  DalmorePowerRails_VDDIO_SDMMC1,    0,   0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_SD3,  DalmorePowerRails_VDD_EMMC_CORE,   0,   0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO0, DalmorePowerRails_AVDD_PLLA_P_C,   0,   0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO1, DalmorePowerRails_VDD_DDR_HS,      0,   0, 1000, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO2, DalmorePowerRails_VDD_SENSOR_2V8,  0,   0, 2850, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO3, DalmorePowerRails_AVDDIO_USB3,     0,   0, 1050, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO4, DalmorePowerRails_VDD_RTC,         0,   0, 1100, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO5, DalmorePowerRails_VDDIO_MIPI,      0,   0, 1200, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO6, DalmorePowerRails_VDDIO_SDMMC3,    0,   0, 1800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO7, DalmorePowerRails_AVDD_CAM1,       0,   0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_LDO8, DalmorePowerRails_AVDD_CAM2,       0,   0, 2800, NV_FALSE, NV_FALSE, NV_FALSE, NULL},

    {Max77663PmuSupply_GPIO0, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO1, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO2, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO3, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO4, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO5, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO6, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {Max77663PmuSupply_GPIO7, DalmorePowerRails_Invalid,        0, 0, 0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},


};

static PmuChipDriverOps s_Max77663OdmProps = {
    Max77663PmuGetCaps, Max77663PmuGetVoltage, Max77663PmuSetVoltage,
};

static void InitMapMax77663(NvU32 *pRailMap)
{
    pRailMap[DalmorePowerRails_VDD_CORE] = Max77663PmuSupply_SD0;
        pRailMap[DalmorePowerRails_VDDIO_DDR] = Max77663PmuSupply_SD1;
        pRailMap[DalmorePowerRails_VDDIO_SDMMC1] = Max77663PmuSupply_SD2;
        pRailMap[DalmorePowerRails_VDDIO_UART] = Max77663PmuSupply_SD2;
        pRailMap[DalmorePowerRails_DVDD_LCD] = Max77663PmuSupply_SD2;
        pRailMap[DalmorePowerRails_VPP_FUSE] = Max77663PmuSupply_SD2;

        pRailMap[DalmorePowerRails_VDD_EMMC_CORE] = Max77663PmuSupply_SD3;
        pRailMap[DalmorePowerRails_AVDD_PLLA_P_C] = Max77663PmuSupply_LDO0;
        pRailMap[DalmorePowerRails_AVDD_PLLM] = Max77663PmuSupply_LDO0;
        pRailMap[DalmorePowerRails_VDD_DDR_HS] = Max77663PmuSupply_LDO1;
        pRailMap[DalmorePowerRails_VDD_SENSOR_2V8] = Max77663PmuSupply_LDO2;
        pRailMap[DalmorePowerRails_AVDDIO_USB3] = Max77663PmuSupply_LDO3;
        pRailMap[DalmorePowerRails_VDDIO_MIPI] = Max77663PmuSupply_LDO5;
        pRailMap[DalmorePowerRails_VDD_RTC] = Max77663PmuSupply_LDO4;
        pRailMap[DalmorePowerRails_VDDIO_SDMMC3] = Max77663PmuSupply_LDO6;
        pRailMap[DalmorePowerRails_AVDD_CAM1] = Max77663PmuSupply_LDO7;
        pRailMap[DalmorePowerRails_AVDD_CAM2] = Max77663PmuSupply_LDO8;
}

void
*DalmorePmu_GetMax77663RailMap(
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

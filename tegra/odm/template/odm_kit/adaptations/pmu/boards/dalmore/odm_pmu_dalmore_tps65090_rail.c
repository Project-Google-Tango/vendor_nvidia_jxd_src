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
#include "tps65090/nvodm_pmu_tps65090_pmu_driver.h"

static NvOdmPmuRailProps s_OdmTps65090RailProps[] = {
     /*VddId                                                    Min_mV    Setup_mV         IsAlwaysOn          BoardData */
     /*                         SystemRailId                           Max_mV      IsEnable        IsOdmProtected         */
    {TPS65090PmuSupply_Invalid, DalmorePowerRails_Invalid,          0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_DCDC1,   DalmorePowerRails_Invalid,          0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_DCDC2,   DalmorePowerRails_Invalid,          0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_DCDC3,   DalmorePowerRails_Invalid,          0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET1,    DalmorePowerRails_VDD_LCD_BL,       0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET2,    DalmorePowerRails_Invalid,          0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET3,    DalmorePowerRails_VDD_3V3_MINICARD, 0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET4,    DalmorePowerRails_AVDD_LCD,         0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET5,    DalmorePowerRails_VDD_LVDS,         0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET6,    DalmorePowerRails_VDD_3V3_SD,       0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
    {TPS65090PmuSupply_FET7,    DalmorePowerRails_VDD_3V3_COM,      0,   0,    0, NV_FALSE, NV_FALSE, NV_FALSE, NULL},
};

static PmuChipDriverOps s_Tps65090OdmProps = {
    Tps65090PmuGetCaps, Tps65090PmuGetVoltage, Tps65090PmuSetVoltage,
};

static void InitMapTps65090(NvU32 *pRailMap)
{
    pRailMap[DalmorePowerRails_VDD_LCD_BL] = TPS65090PmuSupply_FET1;
    pRailMap[DalmorePowerRails_VDD_3V3_MINICARD] = TPS65090PmuSupply_FET3;
    pRailMap[DalmorePowerRails_AVDD_LCD] = TPS65090PmuSupply_FET4;
    pRailMap[DalmorePowerRails_VDD_LVDS] = TPS65090PmuSupply_FET5;
    pRailMap[DalmorePowerRails_VDD_3V3_SD] = TPS65090PmuSupply_FET6;
    pRailMap[DalmorePowerRails_VDD_3V3_COM] = TPS65090PmuSupply_FET7;

}

void
*DalmorePmu_GetTps65090RailMap(
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

    hPmuDriver = Tps65090PmuDriverOpen(hPmuDevice, s_OdmTps65090RailProps,
            TPS65090PmuSupply_Num);
    if (!hPmuDriver)
    {
        NvOdmOsPrintf("%s(): The Tps65090PmuDriverOpen() fails\n", __func__);
        return NULL;
    }

    InitMapTps65090(pRailMap);

    *hDriverOps = &s_Tps65090OdmProps;
    return hPmuDriver;
}

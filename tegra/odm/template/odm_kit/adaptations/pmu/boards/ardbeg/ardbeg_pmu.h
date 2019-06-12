/*
 * Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_Ardbeg_PMU_H
#define INCLUDED_Ardbeg_PMU_H

#include "nvodm_query_discovery.h"
#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Processor Board  ID */
#define PROC_BOARD_E1780    0x06F4 //ERS_Main
#define PROC_BOARD_E1781    0x06F5 //ERS-s_Main
#define PROC_BOARD_E1784    0x06F8
#define PROC_BOARD_E1790    0x06FE //TN8 ERS
#define PROC_BOARD_E1792    0x0700 //ERS_Main with LPDDR3
#define PROC_BOARD_E1791    0x06ff
#define PROC_BOARD_P1761    0x06E1 // TN8 FFD
#define PROC_BOARD_E1922    0x0782 //TN8 POP ERS
#define PROC_BOARD_E1923    0x0783 //TN8 POP ERS-s

/* Laguna Processor Board ID */
#define PROC_BOARD_PM358    0x0166 //ERS_Main
#define PROC_BOARD_PM359    0x0167 //ERS-s_Main
#define PROC_BOARD_PM363    0x016B // FFD

/* Norrin Processor Board ID */
#define PROC_BOARD_PM374    0x0176 // ERS
#define PROC_BOARD_PM370    0x0172 // FFD

/* PMU Board ID */
#define PMU_E1731           0x06c3 //TI PMU
#define PMU_E1733           0x06c5 //AMS3722/29 BGA
#define PMU_E1734           0x06c6 //AMS3722 CSP
#define PMU_E1735           0x06c7 //TI 913/4
#define PMU_E1736           0x06c8 //TI 913/4
#define PMU_E1769           0x06e9 //TI 913/4
#define PMU_E1936           0x0790 //TI 913/4
#define PMU_E1761           0x06e1 //TI 913/4

typedef struct PmuChipDriverOpsRec {
    void (*GetCaps)(void *pDriverData, NvU32 vddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities);
    NvBool (*GetVoltage)(void *pDriverData,
            NvU32 vddRail, NvU32* pMilliVolts);
    NvBool (*SetVoltage) (void *pDriverData,
            NvU32 vddRail, NvU32 MilliVolts, NvU32* pSettleMicroSeconds);
} PmuChipDriverOps, *PmuChipDriverOpsHandle;

void
*ArdbegPmu_GetTps51632RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*ArdbegPmu_GetTps80036RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*ArdbegPmu_GetAs3722RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*ArdbegPmu_GetTps65914RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);


void
*ArdbegPmu_GetApGpioRailMap(
    NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*ArdbegPmuBatterySetup(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_Ardbeg_PMU_H */

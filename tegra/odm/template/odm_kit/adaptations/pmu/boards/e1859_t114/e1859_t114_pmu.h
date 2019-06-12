/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_E1859_PMU_H
#define INCLUDED_E1859_PMU_H

#include "nvodm_query_discovery.h"
#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Processor Board  ID */
#define PROC_BOARD_E1611    0x064B
#define PROC_BOARD_E1612    0x064C
#define PROC_BOARD_E1613    0x064D
#define PROC_BOARD_P2454    0x0996

typedef struct PmuChipDriverOpsRec {
    void (*GetCaps)(void *pDriverData, NvU32 vddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities);
    NvBool (*GetVoltage)(void *pDriverData,
            NvU32 vddRail, NvU32* pMilliVolts);
    NvBool (*SetVoltage) (void *pDriverData,
            NvU32 vddRail, NvU32 MilliVolts, NvU32* pSettleMicroSeconds);
} PmuChipDriverOps, *PmuChipDriverOpsHandle;

void
*E1859Pmu_GetTps51632RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*E1859Pmu_GetTps65090RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*E1859Pmu_GetMax77663RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*E1859Pmu_GetTps65914RailMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void
*E1859Pmu_GetApGpioRailMap(
    NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_E1859_PMU_H */

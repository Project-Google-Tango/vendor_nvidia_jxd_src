/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_P1852_PMU_H
#define INCLUDED_P1852_PMU_H

#include "nvodm_query_discovery.h"
#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct PmuChipDriverOpsRec {
    void (*GetCaps)(void *pDriverData, NvU32 vddRail,
            NvOdmPmuVddRailCapabilities* pCapabilities);
    NvBool (*GetVoltage)(void *pDriverData,
            NvU32 vddRail, NvU32* pMilliVolts);
    NvBool (*SetVoltage) (void *pDriverData,
            NvU32 vddRail, NvU32 MilliVolts, NvU32* pSettleMicroSeconds);
} PmuChipDriverOps, *PmuChipDriverOpsHandle;

void
*P1852Pmu_GetTps6591xMap(
    NvOdmPmuDeviceHandle hDevice,
    PmuChipDriverOpsHandle *hDriverOps,
    NvU32 *pRailMap,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);


#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_P1852_PMU_H */

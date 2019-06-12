/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU-driver Interface</b>
  *
  * @b Description: Defines the typical interface for pmu driver and some common
  * structure related to pmu driver initialization.
  *
  */

#ifndef INCLUDED_NVODM_PMU_DRIVER_H
#define INCLUDED_NVODM_PMU_DRIVER_H

#include "nvodm_services.h"
#include "nvodm_pmu.h"

#define PMU_DEBUG_ENABLE 0
#if PMU_DEBUG_ENABLE
#define PmuDebugPrintf(...) NvOdmOsPrintf(__VA_ARGS__)
#else
#define PmuDebugPrintf(...)
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

typedef void *NvOdmPmuDriverInfoHandle;

typedef struct {
    NvU32  VddId;
    NvU32  SystemRailId;
    NvU32   Min_mV;
    NvU32   Max_mV;
    NvU32   Setup_mV;
    NvBool IsEnable;
    NvBool IsAlwaysOn;
    NvBool IsOdmProtected;
    void *pBoardData;
} NvOdmPmuRailProps;

/* Typical Interface for the pmu drivers, pmu driver needs to implement
 * following function with same set of argument.*/
typedef NvOdmPmuDriverInfoHandle (*PmuDriverOpen)(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps);

typedef void (*PmuDriverClose)(NvOdmPmuDriverInfoHandle hPmuDriverInfo);

typedef void (*PmuGetCaps)(NvOdmPmuDriverInfoHandle hPmuDriverInfo, NvU32 vddId,
        NvOdmPmuVddRailCapabilities* pCapabilities);

typedef NvBool (*PmuGetVoltage)(NvOdmPmuDriverInfoHandle hPmuDriverInfo, NvU32 vddId,
        NvU32* pMilliVolts);

typedef NvBool (*PmuSetVoltage)(NvOdmPmuDriverInfoHandle hPmuDriverInfo, NvU32 vddId,
        NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVODM_PMU_DRIVER_H

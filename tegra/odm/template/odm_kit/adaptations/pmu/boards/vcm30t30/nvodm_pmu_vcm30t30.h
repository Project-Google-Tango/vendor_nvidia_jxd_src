/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_VCM30T30_H
#define INCLUDED_NVODM_PMU_VCM30T30_H

#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void
Vcm30T30PmuGetCapabilities(
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
Vcm30T30PmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
Vcm30T30PmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

NvBool Vcm30T30PmuSetup(NvOdmPmuDeviceHandle hDevice);

void Vcm30T30PmuRelease(NvOdmPmuDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_VCM30T30_H */

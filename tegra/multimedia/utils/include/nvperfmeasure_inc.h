/* Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

 /**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Video Perf Measurement API </b>
 *
 * @b Description:
 *           Defines the API for Perf Measurement using EMC_STAT Register
 */

#ifndef INCLUDED_NVPERFMEASURE_INC_H
#define INCLUDED_NVPERFMEASURE_INC_H

#ifdef __cplusplus
extern "C" {
#endif

/* This API ONLY works from a root shell or an app that runs as root
 * TODO : Fix API to run for for non-root apps
 */

#ifndef NV_IS_AVP
#define NV_IS_AVP 0
#endif

#ifndef RUNNING_IN_SIMULATION
#define RUNNING_IN_SIMULATION 0
#endif

#include "nvrm_module.h"
#include "nvrm_hardware_access.h"
#include "arclk_rst.h"
#include "aremc.h"
#if NV_IS_AVP
#include "os/avp.h"
#endif

#define NV_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)
#define EMC_REGW(b,a,d)       NV_WRITE32((b+a),(d))
#define CLK_RST_REGW(b,a,d)   NV_WRITE32((b+a),(d))

#define NV_READ32(a)        *((const volatile NvU32 *)(a))
#define EMC_REGR(b,a)         NV_READ32(b+a)
#define CLK_RST_REGR(b,a)     NV_READ32(b+a)

#define INVALID_PLLM            -1

typedef struct NvPerfMeasureInfo
{
    NvU32 EMC_base;
    NvU32 CLK_RST_base;
#if !NV_IS_AVP
    NvU32 EMCRegBaseAdd;
    NvU32 CARRegBaseAdd;
    NvU32 HwUnitClockResetRegOffset;
    NvU32 HwUnitClockDivisor;
    NvRmModuleID ModuleId;
#endif
    NvRmDeviceHandle hRmDevice;
} NvPerfMeasureInfo;

NvError NvPerfMeasureInit(
#if RUNNING_IN_SIMULATION
     NvU32 HwUnitClockResetRegOffset,
     NvU32 HwUnitClockDivisor,
#endif
     NvPerfMeasureInfo *pPerfMeasureInfo);
NvError NvPerfMeasureStart(NvPerfMeasureInfo *pPerfMeasureInfo);
NvError NvPerfMeasureStop(NvPerfMeasureInfo *pPerfMeasureInfo,NvU64 *HwCycleCount);
NvError NvPerfMeasureDeInit(NvPerfMeasureInfo *pPerfMeasureInfo);

#ifdef __cplusplus
}
#endif

#endif

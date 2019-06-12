/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVBOOT_MISC_T1XX_H
#define INCLUDED_NVBOOT_MISC_T1XX_H

#include "nvcommon.h"
#ifdef ENABLE_TXX
#include "t114/include/nvboot_clocks_int.h"
#include "t114/include/nvboot_reset_int.h"
#else
#include "include/nvboot_clocks_int.h"
#include "include/nvboot_reset_int.h"
#endif

#ifdef ENABLE_T148
#define CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIV2_RANGE \
           CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIVP_RANGE
#endif

#define NV_ADDRESS_MAP_CAR_BASE             0x60006000

#ifndef NV_ADDRESS_MAP_TMRUS_BASE
#define NV_ADDRESS_MAP_TMRUS_BASE           0x60005010
#endif

#define NV_ADDRESS_MAP_FUSE_BASE            0x7000F800
#define NV_ADDRESS_MAP_APB_MISC_BASE        0x70000000

NvBootClocksOscFreq NvBootClocksGetOscFreqT1xx(void);
void
NvBootClocksStartPllT1xx(
                     NvBootClocksPllId PllId,
                     NvU32 M,
                     NvU32 N,
                     NvU32 P,
                     NvU32 CPCON,
                     NvU32 LFCON,
                     NvU32 *StableTime);

void NvBootClocksSetEnableT1xx(NvBootClocksClockId ClockId, NvBool Enable);

void NvBootUtilWaitUST1xx(NvU32 usec);

NvU32 NvBootUtilGetTimeUST1xx(void);

void NvBootResetSetEnableT1xx(const NvBootResetDeviceId DeviceId,
            const NvBool Enable);

#ifdef ENABLE_T124
void NvBootResetSetEnableT124(const NvBootResetDeviceId DeviceId,
            const NvBool Enable);
#endif
void NvBootFuseGetSkuRawT1xx(NvU32 *pSku);

#endif //INCLUDED_NVBOOT_MISC_T1xx.h

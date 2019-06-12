/*
 * Copyright (c)  2010 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 #ifndef INCLUDED_NVBOOT_MISC_T30_H
 #define INCLUDED_NVBOOT_MISC_T30_H

#include "nvcommon.h"
#include "t30/include/nvboot_clocks_int.h"
#include "t30/include/nvboot_reset_int.h"

#define NV_ADDRESS_MAP_CAR_BASE             0x60006000

#ifndef NV_ADDRESS_MAP_TMRUS_BASE
#define NV_ADDRESS_MAP_TMRUS_BASE           0x60005010
#endif

#define NV_ADDRESS_MAP_FUSE_BASE            0x7000F800
#define NV_ADDRESS_MAP_APB_MISC_BASE        0x70000000

NvBootClocksOscFreq NvBootClocksGetOscFreqT30(void);
void
NvBootClocksStartPllT30(
                     NvBootClocksPllId PllId,
                     NvU32 M,
                     NvU32 N,
                     NvU32 P,
                     NvU32 CPCON,
                     NvU32 LFCON,
                     NvU32 *StableTime);

void NvBootClocksSetEnableT30(NvBootClocksClockId ClockId, NvBool Enable);

void NvBootUtilWaitUST30(NvU32 usec);

NvU32 NvBootUtilGetTimeUST30(void);

void NvBootResetSetEnableT30(const NvBootResetDeviceId DeviceId, const NvBool Enable);

void NvBootFuseGetSkuRawT30(NvU32 *pSku);

#endif //INCLUDED_NVBOOT_MISC_T30_H


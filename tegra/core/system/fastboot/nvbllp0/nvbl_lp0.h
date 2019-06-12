/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef BOOTLOADER_LP0_H
#define BOOTLOADER_LP0_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"
#include "nvaboot.h"

/* all defines */
#define DEBUG_TEST_PWR_GATE 0

/* base address */
#define PMC_RST_SOURCE_LP0             0x04

#define TEGRA_IRAM_BASE                0x40000000
#define TEGRA_IRAM_BCT_SPACE           0x20F0
#define TEGRA_IRAM_CODE_AREA           (TEGRA_IRAM_BASE + 4096)

#define FLOW_CTLR_CSR_WFI_BITMAP       (0xF << 8)
#define FLOW_CTLR_CSR_INTR_FLAG        (1 << 15)
#define FLOW_CTLR_CSR_EVENT_FLAG       (1 << 14)
#define FLOW_CTLR_CSR_WFI_CPU0         (1 <<  8)
#define FLOW_CTLR_CSR_ENABLE           (1 <<  0)
#define FLOW_CTLR_CSR_ENABLE_EXT_CRAIL (1 << 13)
#define FLOW_CTLR_CSR_ENABLE_EXT_NCPU  (1 << 12)

#define FLOW_MODE_STOP_UNTIL_IRQ (4 << 29)
#define FLOW_CTLR_HALT_IRQ_0 (1 << 10)
#define FLOW_CTLR_HALT_IRQ_1 (1 << 11)

#define PLLP_FIXED_FREQ_KHZ_408000     408000

extern NvU32 g_NvBlLp0IramStart;
extern NvU32 g_NvBlLp0IramEnd;

void NvBlLp0CoreSuspend(void);
NvError NvBlLp0StartSuspend(NvAbootHandle hAboot);
void NvBlLp0StartResume(void);
void NvBlLp0CoreResume(void);

#if defined(__cplusplus)
}
#endif
#endif

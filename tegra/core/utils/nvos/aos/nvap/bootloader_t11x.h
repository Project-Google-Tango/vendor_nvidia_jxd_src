/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define ASSEMBLY_SOURCE_FILE 1

#ifndef INCLUDED_BOOTLOADER_T11X_H
#define INCLUDED_BOOTLOADER_T11X_H

#include "bootloader.h"
#include "t11x/arpg.h"
#include "t11x/arclk_rst.h"
#include "t11x/artimerus.h"
#include "t11x/arflow_ctlr.h"
#include "t11x/arapb_misc.h"
#include "t11x/arapbpm.h"
#include "t11x/armc.h"
#include "t11x/aremc.h"
#include "t11x/arahb_arbc.h"
#include "t11x/arevp.h"
#include "t11x/arcsite.h"
#include "t11x/arapb_misc.h"
#include "t11x/ari2c.h"
#include "t11x/arfuse.h"
#include "t11x/argpio.h"
#include "t11x/arsysctr0.h"

#include "t11x/nvboot_bit.h"
#include "t11x/nvboot_version.h"
#include "t11x/nvboot_pmc_scratch_map.h"
#include "t11x/nvboot_osc.h"
#include "t11x/nvboot_clocks.h"

#define AHB_PA_BASE         0x6000C000  // Base address for arahb_arbc.h registers
#define MC_PA_BASE          0x70019000  // Base address for armc.h registers

#define FUSE_SKU_DIRECT_CONFIG_0_NUM_DISABLED_CPUS_RANGE  4:3
#define FUSE_SKU_DIRECT_CONFIG_0_DISABLE_ALL_RANGE        5:5

#define NVBL_PLL_BYPASS             0
#define NVBL_PLLP_SUBCLOCKS_FIXED   1
#define NVBL_USE_PLL_LOCK_BITS      1

#define NVBL_PLLP_KHZ               408000

extern NvBool s_FpgaEmulation;
extern NvU32 s_Netlist;
extern NvU32 s_NetlistPatch;

static NvU32  NvBlAvpQueryOscillatorFrequency( void );
static NvBlCpuClusterId NvBlAvpQueryFlowControllerClusterId(void);
NvBool NvBlAvpInit_T11x(NvBool IsRunningFromSdram);
void NvBlStartCpu_T11x(NvU32 ResetVector);
NV_NAKED void NvBlStartUpAvp_T11x( void );
void NvBlConfigJtagAccess_T11x(void);
void SetAvpClockToClkM(void);
void NvBlMemoryControllerInit(NvBool IsChipInitialized);

#endif // INCLUDED_BOOTLOADER_T11X_H

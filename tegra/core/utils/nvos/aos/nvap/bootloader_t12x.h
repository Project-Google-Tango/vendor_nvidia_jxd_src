/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define ASSEMBLY_SOURCE_FILE 1

#ifndef INCLUDED_BOOTLOADER_T12X_H
#define INCLUDED_BOOTLOADER_T12X_H

#include "bootloader.h"
#include "t12x/arpg.h"
#include "t12x/nvboot_bit.h"
#include "t12x/nvboot_version.h"
#include "t12x/nvboot_pmc_scratch_map.h"
#include "t12x/nvboot_osc.h"
#include "t12x/nvboot_clocks.h"
#include "t12x/arclk_rst.h"
#include "t12x/artimerus.h"
#include "t12x/arflow_ctlr.h"
#include "t12x/arapb_misc.h"
#include "t12x/arapbpm.h"
#include "t12x/armc.h"
#include "t12x/aremc.h"
#include "t12x/arahb_arbc.h"
#include "t12x/arevp.h"
#include "t12x/arcsite.h"
#include "t12x/arapb_misc.h"
#include "t12x/ari2c.h"
#include "t12x/arfuse.h"

#define AHB_PA_BASE         0x6000C000  // Base address for arahb_arbc.h registers
#define MC_PA_BASE          0x70018000  // Base address for armc.h registers
#define PWRI2C_PA_BASE      0x7000D000  // Base address for ari2c.h registers

#define FUSE_SKU_DIRECT_CONFIG_0_NUM_DISABLED_CPUS_RANGE  4:3
#define FUSE_SKU_DIRECT_CONFIG_0_DISABLE_ALL_RANGE        5:5

#define USE_PLLC_OUT1               0       // 0 ==> PLLP_OUT4, 1 ==> PLLC_OUT1
#define NVBL_PLL_BYPASS 0
#define NVBL_PLLP_SUBCLOCKS_FIXED   (1)

#define NVBL_PLLC_KHZ               0
#define NVBL_PLLM_KHZ               167000
#define NVBL_PLLP_KHZ               408000

#define NVBL_USE_PLL_LOCK_BITS      1

extern NvBool s_FpgaEmulation;
extern NvU32 s_Netlist;
extern NvU32 s_NetlistPatch;

static NvU32  NvBlAvpQueryOscillatorFrequency( void );
static NvBlCpuClusterId NvBlAvpQueryFlowControllerClusterId(void);
NvBool NvBlAvpInit_T12x(NvBool IsRunningFromSdram);
void NvBlStartCpu_T12x(NvU32 ResetVector);
NV_NAKED void NvBlStartUpAvp_T12x(void);
void SetAvpClockToClkM(void);
void NvBlMemoryControllerInit(NvBool IsChipInitialized);

#endif // INCLUDED_BOOTLOADER_T12X_H

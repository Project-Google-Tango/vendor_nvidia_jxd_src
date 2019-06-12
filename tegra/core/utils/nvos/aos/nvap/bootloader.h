/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_BOOTLOADER_H
#define INCLUDED_BOOTLOADER_H

#define ASSEMBLY_SOURCE_FILE 1

#include "aos_common.h"
#include "nvassert.h"
#include "nvbl_memmap_nvap.h"
#include "nvbl_assembly.h"
#include "nvbl_arm_cpsr.h"
#include "nvbl_arm_cp15.h"
#include "nvrm_arm_cp.h"
#include "aos_avp_board_info.h"

//------------------------------------------------------------------------------
// Provide missing enumerators for spec files.
//------------------------------------------------------------------------------

#define NV_BIT_ADDRESS IRAM_PA_BASE
#define NV3P_SIGNATURE 0x5AFEADD8
#define MINILOADER_SIGNATURE 0x5AFEADD8
#define EEPROM_I2C_SLAVE_ADDRESS  0xAC
#define EEPROM_BOARDID_LSB_ADDRESS 0x4
#define PINMUX_BASE 0x70000000

NvBool NvBlIsMmuEnabled(void);
void NvBlAvpStallUs(NvU32 MicroSec);
void NvBlAvpStallMs(NvU32 MilliSec);
void NvBlAvpSetCpuResetVector(NvU32 reset);
void NvBlAvpUnHaltCpu(void);
void NvBlAvpHalt(void);
void NvOsAvpMemset(void* s, NvU8 c, NvU32 n);
void NvBlConfigJtagAccess(void);
void NvBlConfigFusePrivateTZ(void);
void NvGetBoardInfoFromNCT(NvBoardType BoardType, NvBoardInfo *pBoardInfo);
NvError NvBlInitGPIOExpander(void);

NV_NAKED void ColdBoot(void);

void ARM_ERRATA_743622(void);
void ARM_ERRATA_751472(void);

void bootloader(void);

#endif

// Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation
// is strictly prohibited.

/**
 * @file
 * <b>NVIDIA Tegra Boot Loader API</b>
 *
 */

#ifndef INCLUDED_NVBL_MEMMAP_NVAP_H
#define INCLUDED_NVBL_MEMMAP_NVAP_H

/**
 * @defgroup nvbl_memmap_group NvBL Preprocessor Directives
 *
 * @ingroup nvbdk_modules
 * @{
 */

// Defines for base address and size of IRAM, as well as the location of the BIT
// (which, in all SOCs, currently lives at the base of IRAM).
// XXX Note that the size is actually wrong -- this indicates 148KiB, it's actually 256KiB.
// Unclear what the implications would be of fixing this.
// XXX Probably could collapse out a bunch of these T30, etc. defines
#define NVAP_BASE_PA_SRAM       0x40000000
#define NVAP_BASE_PA_SRAM_SIZE  0x00025000
#define T30_BASE_PA_BOOT_INFO   NVAP_BASE_PA_SRAM
#define T11X_BASE_PA_BOOT_INFO  NVAP_BASE_PA_SRAM
#define T12X_BASE_PA_BOOT_INFO  NVAP_BASE_PA_SRAM

//-------------------------------------------------------------------------------
// Super-temporary stacks for EXTREMELY early startup. The values chosen for
// these addresses must be valid on ALL SOCs because this value is used before
// we are able to differentiate between the SOC types.
//
// NOTE: The since CPU's stack will eventually be moved from IRAM to SDRAM, its
//       stack is placed below the AVP stack. Once the CPU stack has been moved,
//       the AVP is free to use the IRAM the CPU stack previously occupied if
//       it should need to do so.
//
// NOTE: In multi-processor CPU complex configurations, each processor will have
//       its own stack of size NVAP_EARLY_BOOT_IRAM_CPU_STACK_SIZE. CPU 0 will have
//       a limit of NVAP_LIMIT_PA_IRAM_CPU_EARLY_BOOT_STACK. Each successive CPU
//       will have a stack limit that is NVAP_EARLY_BOOT_IRAM_CPU_STACK_SIZE less
//       than the previous CPU.
//-------------------------------------------------------------------------------

#define NVAP_LIMIT_PA_IRAM_AVP_EARLY_BOOT_STACK (NVAP_BASE_PA_SRAM + NVAP_BASE_PA_SRAM_SIZE)    /**< Common AVP early boot stack limit */
#define NVAP_EARLY_BOOT_IRAM_AVP_STACK_SIZE     0x1000                                          /**< Common AVP early boot stack size */
#define NVAP_LIMIT_PA_IRAM_CPU_EARLY_BOOT_STACK (NVAP_LIMIT_PA_IRAM_AVP_EARLY_BOOT_STACK - NVAP_EARLY_BOOT_IRAM_AVP_STACK_SIZE) /**< Common CPU early boot stack limit */
#define NVAP_EARLY_BOOT_IRAM_CPU_STACK_SIZE     0x1000                                          /**< Common CPU early boot stack size */


/** @} */

#endif  // INCLUDED_NVBL_MEMMAP_NVAP_H


//
// Copyright (c) 2007-2008 NVIDIA Corporation.  All Rights Reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation
// is strictly prohibited.
//

/**
 * @file
 * <b>NVIDIA Tegra Boot Loader API</b>
 *
 */

#ifndef INCLUDED_NVBL_ARM_CPSR_H
#define INCLUDED_NVBL_ARM_CPSR_H

/**
 * @defgroup nvbl_arm_cpsr_group NvBL ARM CPSR API
 *
 * 
 *
 * @ingroup nvbl_group
 * @{
 */

//==========================================================================
/**  @name ARM CPSR/SPSR Definitions
 */
/*@{*/

#define PSR_MODE_MASK   0x1F
#define PSR_MODE_USR    0x10
#define PSR_MODE_FIQ    0x11
#define PSR_MODE_IRQ    0x12
#define PSR_MODE_SVC    0x13
#define PSR_MODE_ABT    0x17
#define PSR_MODE_UND    0x1B
#define PSR_MODE_SYS    0x1F    /**< Only available on ARM Arch v4 and higher. */
#define PSR_MODE_MON    0x16    /**< only available on ARM Arch v6 and higher with TrustZone extension. */

#define PSR_F_BIT       0x40    /**< FIQ disable. */
#define PSR_I_BIT       0x80    /**< IRQ disable. */
#define PSR_A_BIT       0x100   /**< Imprecise data abort disable (only available on ARM Arch v6 and higher). */
/*@}*/

/** @} */

#endif // INCLUDED_NVBL_ARM_CPSR_H

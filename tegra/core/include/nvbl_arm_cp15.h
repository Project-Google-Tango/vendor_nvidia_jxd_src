//
// Copyright (c) 2007-2009 NVIDIA Corporation.  All Rights Reserved.
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

#ifndef INCLUDED_NVBL_ARM_CP15_H
#define INCLUDED_NVBL_ARM_CP15_H 1

#include "nvbl_assembly.h"

/**
 * @defgroup nvbl_arm_cp15_group NvBL ARM CP15 API
 *
 *
 *
 * @ingroup nvbl_group
 * @{
 */

//==========================================================================
// Common ARM CP15 Register Definitions
//==========================================================================

//-----------------------------------------------------------------------------
/**  @name c0: Control Register - MRC/MCR p15,0,<Rd>,c1,c0,0
 */
/*@{*/

#define M_CP15_C1_C0_0_M            0x00000001  /**< MMU: 0 = disable (default), 1 = enable */
#define M_CP15_C1_C0_0_A            0x00000002  /**< Strict alignment emforcement: 0 = disable (default), 1 = enable */
#define M_CP15_C1_C0_0_C            0x00000004  /**< L1 data cache: 0 = disable (default), 1 = enable */
#define M_CP15_C1_C0_0_SBO          0x00000078  /**< Should Be One */
#define M_CP15_C1_C0_0_Z            0x00000800  /**< Branch prediction: 0 = disable (default), 1 = enable */
#define M_CP15_C1_C0_0_I            0x00001000  /**< L1 instruction cache: 0 = disable (default), 1 = enable */
#define M_CP15_C1_C0_0_V            0x00002000  /**< Vector table placement: 0=low (0x00000000), 1=high (0xFFFF0000) */
#define M_CP15_C1_C0_0_TR           0x10000000  /**< TEX remap: 0 = disable, 1= enable */
#define M_CP15_C1_C0_0_MASK         (M_CP15_C1_C0_0_SBO _OR_ M_CP15_C1_C0_0_V)
#define M_CP15_C1_C0_0_OFF          M_CP15_C1_C0_0_SBO
#define M_CP15_C1_C0_0_CACHE_ON     (M_CP15_C1_C0_0_SBO _OR_ M_CP15_C1_C0_0_C _OR_ M_CP15_C1_C0_0_I _OR_ M_CP15_C1_C0_0_Z)
/**alignment  shift*/
#define S_CP15_C1_C0_0_U            22
/**alignment mask */
#define M_CP15_C1_C0_0_U            (1 << S_CP15_C1_C0_0_U)

/// Virtual Mode: Control register MMU, I-Cache, D-Cache, and Branch Prediction flags
#define M_CP15_C1_C0_0_VIRT         (M_CP15_C1_C0_0_M _OR_ M_CP15_C1_C0_0_I _OR_ M_CP15_C1_C0_0_C _OR_ M_CP15_C1_C0_0_Z)

/*@}*/

//-----------------------------------------------------------------------------
/**  @name c7: PA Register - MRC p15,0,<Rd>,c7,c4,0
 */
/*@{*/

#define M_CP15_C7_C4_0_FAIL         0x00000001  /**< Translation failure mask */
#define M_CP15_C7_C4_0_PA           0xFFFFF000  /**< Physical address mask */

/*@}*/

//-----------------------------------------------------------------------------
/**  @name PTE - Page Table Entry Definitions. Refer to ARM Architecture
     Reference Manual (the ARM ARM) for details and interpretion of PTE fields.
 */
/*@{*/

#define M_PTE_L2_SP_XN              0x00000001  /**< Level 2, Small Page, PTE XN mask */
#define M_PTE_L2_SP_B               0x00000004  /**< Level 2, Small Page, PTE B mask */
#define M_PTE_L2_SP_C               0x00000008  /**< Level 2, Small Page, PTE C mask */
#define M_PTE_L2_SP_TEX             0x000001C0  /**< Level 2, Small Page, PTE TEX mask */
#define S_PTE_L2_SP_TEX             6           /**< Level 2, Small Page, PTE TEX shift */
#define M_PTE_L2_SP_S               0x00000400  /**< Level 2, Small Page, PTE S mask */

/*@}*/


/** @} */

#endif // INCLUDED_NVBL_ARM_CP15_H


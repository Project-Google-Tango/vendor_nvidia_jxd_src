/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All rights reserved.
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
 *           Power Management Controller (PMC) scratch registers fields
 *           definitions</b>
 *
 * @b Description: Defines SW-allocated fields in the PMC scratch registers
 *  shared by boot and power management code in RM and OAL.
 *
 */


#ifndef INCLUDED_AP15RM_PMC_SCRATCH_MAP_H
#define INCLUDED_AP15RM_PMC_SCRATCH_MAP_H

/*
 * Scratch registers offsets are part of the HW specification in the below
 * include file. Scratch registers fields are defined in this header via
 * bit ranges compatible with nvrm_drf macros.
 */
#include "ap20/arapbpm.h"

// Register APBDEV_PMC_SCRATCH0_0 (this is the only scratch register cleared on reset)
//

// RM clients combined power state (bits 4-7)
#define APBDEV_PMC_SCRATCH0_0_RM_PWR_STATE_RANGE        11:8
#define APBDEV_PMC_SCRATCH0_0_RM_LOAD_TRANSPORT_RANGE   15:12
#define APBDEV_PMC_SCRATCH0_0_RM_DFS_FLAG_RANGE         27:16
#define APBDEV_PMC_SCRATCH0_0_UPDATE_MODE_FLAG_RANGE     29:28
#define APBDEV_PMC_SCRATCH0_0_OAL_RTC_INIT_RANGE        30:30
#define APBDEV_PMC_SCRATCH0_0_RST_PWR_DET_RANGE         31:31

// Register APBDEV_PMC_SCRATCH20_0, used to store the ODM customer data from the BCT
#define APBDEV_PMC_SCRATCH20_0_BCT_ODM_DATA_RANGE       31:0

// Register APBDEV_PMC_SCRATCH21_0
//
#define APBDEV_PMC_SCRATCH21_0_LP2_TIME_US              31:0

#endif // INCLUDED_AP15RM_PMC_SCRATCH_MAP_H

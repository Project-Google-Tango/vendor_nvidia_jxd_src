/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvbl_arm_cpsr.h"
#include "nvbl_arm_cp15.h"

#include "aos_mon_mode.h"
#include "aos_ns_mode.h"

void nvaosNSModeInit(void)
{
#if AOS_MON_MODE_ENABLE
    nvaosSetNonSecureStateMMU();                // This function switch to non-secure state
    nvaosConfigureGrp1InterruptController();    // Enable the CPU interface to get interrupts in non-secure state
    nvaosSwitchToSecureState(SMC_DFC_NONE, 0);  // Switch back to secure state
#endif
}


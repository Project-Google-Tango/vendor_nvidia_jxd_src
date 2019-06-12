/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_MON_MODE_H
#define INCLUDED_AOS_MON_MODE_H

#include "nvcommon.h"
#include "aos.h"
#include "cpu.h"

#define NVAOS_MON_STACK_SIZE            512

#define SMC_TYPE_TO_S                   0
#define SMC_TYPE_TO_NS                  1

// No particular dispatch function is called, used to switch state
#define SMC_DFC_NONE                    0x000

// The following is for service in non-secure state
#define SMC_DFC_ENABLE_MMU              0x001

// The following is for service in secure state
#define SMC_DFC_TEST                    0x100

// Monitor Mode Initialization
void    nvaosMonModeInit(void);

// SMC Handler, it takes care the switch between secure and non-secure state
NV_NAKED void SmcHandler(void);

// Issue SMC instruction which switches cpu between secure or non-secure state
NV_NAKED void nvaosSendSMC(NvU32 smc_type, NvU32 smc_dfc, NvU32 smc_param);

// Set up monitor stack
NV_NAKED void    SetUpMonStack(NvU32 stack);

/*
 * Function to switch to secure state
 * Input: smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
void    nvaosSwitchToSecureState(NvU32 smc_dfc, NvU32 smc_param);

/*
 * Function to switch to non-secure state
 * Input: smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
void    nvaosSwitchToNonSecureState(NvU32 smc_dfc, NvU32 smc_param);

/*
 * Function to dispatch SMC Dispatched Function Call
 * Input: smc_type - indicates switch to either secure or non-secure state
 *        smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
void    SmcFuncDispatch(NvU32 smc_type, NvU32 smc_dfc, NvU32 smc_param);

#endif

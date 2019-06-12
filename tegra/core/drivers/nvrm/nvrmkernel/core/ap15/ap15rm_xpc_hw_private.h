/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
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
 *           Priate Hw access function for XPC driver </b>
 *
 * @b Description: Defines the private interface functions for the xpc 
 * 
 */

#ifndef INCLUDED_RM_XPC_HW_PRIVATE_H
#define INCLUDED_RM_XPC_HW_PRIVATE_H


#include "nvcommon.h"
#include "nvrm_init.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Combines the cpu avp hw mail baox system information.
typedef struct CpuAvpHwMailBoxRegRec
{
    // Hw mail box register virtual base address.
    NvU32 *pHwMailBoxRegBaseVirtAddr;
    
    // Bank size of the hw regsiter.
    NvU32 BankSize;

    // Tells whether this is on cpu or on Avp
    NvBool IsCpu;

    // Mail box data which was read last time.
    NvU32 MailBoxData;
} CpuAvpHwMailBoxReg;

void NvRmPrivXpcHwResetOutbox(CpuAvpHwMailBoxReg *pHwMailBoxReg);

/**
 * Send message to the target.
 */
void
NvRmPrivXpcHwSendMessageToTarget(
    CpuAvpHwMailBoxReg *pHwMailBoxReg,
    NvRmPhysAddr MessageAddress);

/**
 * Receive message from the target.
 */
void
NvRmPrivXpcHwReceiveMessageFromTarget(
    CpuAvpHwMailBoxReg *pHwMailBoxReg,
    NvRmPhysAddr *pMessageAddress);


#if defined(__cplusplus)
 }
#endif

#endif  // INCLUDED_RM_XPC_HW_PRIVATE_H

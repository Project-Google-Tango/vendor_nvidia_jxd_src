/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_MAX8907B_INTERRUPT_HEADER
#define INCLUDED_MAX8907B_INTERRUPT_HEADER

#include "max8907b.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool 
Max8907bSetupInterrupt(
    NvOdmPmuDeviceHandle hDevice,
    Max8907bStatus *pmuStatus);

void 
Max8907bInterruptHandler_int(
    NvOdmPmuDeviceHandle hDevice,
    Max8907bStatus *pmuStatus);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_MAX8907B_INTERRUPT_HEADER


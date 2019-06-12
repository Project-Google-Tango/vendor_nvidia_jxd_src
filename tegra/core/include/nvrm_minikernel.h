/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_MINIKERNEL_H
#define INCLUDED_NVRM_MINIKERNEL_H

#include "nvrm_init.h"

/**
 * Called by the secure OS code to initialize the Rm. Usage and
 * implementation of this API is platform specific.
 *
 * This APIs should not be called by the non secure clients of the Rm.
 *
 * This APIs is guaranteed to succeed on the supported platforms.
 *
 * @param pHandle the RM handle is stored here.
 */
void NvRmBasicInit( NvRmDeviceHandle *pHandle );

/**
 * Closes the Resource Manager for secure os.
 *
 * @param hDevice The RM handle.  If hDevice is NULL, this API has no effect.
 */
void NvRmBasicClose( NvRmDeviceHandle hDevice );

#endif // INCLUDED_NVRM_MINIKERNEL_H

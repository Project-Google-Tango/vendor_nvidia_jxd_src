/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_AVP_H
#define INCLUDED_NVDDK_AVP_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_module.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * The NVOS AVP shell thread entry point. This thread will be launched
 * by NvOsAvpStart after NVOS initializes. This thread then takes over
 * the management of the AVP.
 */
void NvOsAvpShellThread(int argc, char **argv);

/**
 * Allows the debug output path to be customized at runtime.
 */
void nvaosSemiSetDebugString(void (*DebugString)(const char *msg));

/** @} */  // End documentation group

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVDDK_AVP_H

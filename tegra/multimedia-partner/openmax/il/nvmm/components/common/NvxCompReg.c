/* Copyright (c) 2006-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxCompReg.c
 *
 *  NVIDIA's implementation of the OMX Component Registration.
 */

#include <NvxCompReg.h>

/* Statically registered component list                        */
/* Add an entry for each component to be statically registered */

OMX_COMPONENTREGISTERTYPE OMX_ComponentRegistered[] =
{
#ifdef BUILD_GOOGLETV
    // these need to come first so the proper scheduler is found
    NVX_GOOGLETV_COMPONENTS,
#endif
    NVX_COMMON_NVIDIA_COMPONENTS,
    NVX_NVMM_COMPONENTS,
    NVX_NEW_NVMM_COMPONENTS,
    { NULL, NULL }
};


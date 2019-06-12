/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra: OpenMAX Graph Helper Library Framework Interface/b>
 *
 */


#ifndef NVX_FRAMEWORK_H
#define NVX_FRAMEWORK_H

/** defgroup nv_omx_graphhelper_framework Framework Interface
 * 
 * This is the NVIDIA OpenMAX Graph Helper Library, Framework Interface.
 *
 * The framework interface loads and wraps the actual NVIDIA OpenMAX IL
 * library.
 * 
 * @ingroup nvomx_graphhelper
 * @{
 */

#include "OMX_Core.h"
#include "NVOMX_CustomProtocol.h"

/** Opaque type to store internal framework data */
struct NvxFrameworkRec;

typedef struct NvxFrameworkRec *NvxFramework; 

/** Initialize the Framework (and OMX)
 * 
 * Must be the first call of any user of this library
 *
 * @param framework  Pointer to a framework structure to allocate internally
 */
OMX_ERRORTYPE NvxFrameworkInit(NvxFramework *framework);

/** Deinitialize the framework
 * 
 * Must be the last call of any user of this library
 *
 * @param framework  Framework structure
 */
OMX_ERRORTYPE NvxFrameworkDeinit(NvxFramework *framework);

/** Register a custom protocol interface with the framework (and OMX)
 *
 * See NVOMX_RegisterNVCustomProtocol
 *
 * @param framework Framework structure
 * @param protoname The protocol name to register
 * @param protocol  Pointer to an NV_CUSTOM_PROTOCOL implementation
 */
OMX_ERRORTYPE NvxFrameworkRegisterProtocol(NvxFramework framework,
                                           OMX_STRING protoname,
                                           NV_CUSTOM_PROTOCOL *protocol);

/** Register a custom component
 *
 * fixme
 */
OMX_ERRORTYPE NvxFrameworkRegisterComponent(NvxFramework framework,
                                            OMX_COMPONENTREGISTERTYPE *reg);

/** Return the framework's IL version
 * 
 * @param framework  Framework structure
 */
OMX_VERSIONTYPE NvxFrameworkGetOMXILVersion(NvxFramework framework);

/** Return the OMX IL shared library handle
 */
NvOsLibraryHandle NvxFrameworkGetILLibraryHandle(NvxFramework framework);

#endif

/** @} */
/* File EOF */


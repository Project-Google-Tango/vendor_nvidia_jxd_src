/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVTNR_H
#define INCLUDED_NVTNR_H

#include "nvos.h"
#include "nvcommon.h"
#include "nvrm_surface.h"
#include "nvmm_buffertype.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvTnrGlesContextRec *NvTnrGlesContextHandle;

NvError
NvTnrGlesOpen(void **pContext);

void
NvTnrGlesClose(void *pContext);

NvError
NvTnrGlesDoFilter(
    void *pContext,
    NvMMSurfaceDescriptor *pInSurfaceDesc,
    NvMMSurfaceDescriptor *pRefSurfaceDesc,
    NvMMSurfaceDescriptor *pOutSurfaceDesc,
    NvU8 LightISP,
    NvU8 IIR_Strength,
    NvBool Complexity,
    NvBool Filter);

NvError
NvTnrTvmrOpen(
    void **pContext);

void
NvTnrTvmrClose(
    void *pContext);

NvError
NvTnrTvmrDoFilter(
    void *pContext,
    NvMMSurfaceDescriptor *pInSurfaceDesc,
    NvMMSurfaceDescriptor *pRefSurfaceDesc,
    NvMMSurfaceDescriptor *pOutSurfaceDesc,
    NvU8 LightISP,
    NvU8 IIR_Strength,
    NvBool Complexity,
    NvBool Filter);

#if defined(__cplusplus)
}
#endif

#endif

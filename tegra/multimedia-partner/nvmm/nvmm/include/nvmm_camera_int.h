/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


/** nvmm_camera_int.h
 *
 * This NvMM block is provided as a reference to block writers. As such it is
 * a demonstration of correct semantics not a template for the authoring of
 * blocks. Actual implementation may vary. For instance it may create a worker
 * thread rather than handle processing synchronously.
 */

#ifndef INCLUDED_NVMM_CAMERA_INT_H
#define INCLUDED_NVMM_CAMERA_INT_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"

#if defined(__cplusplus)
extern "C"
{
#endif  

NvError
NvMMCameraOpen(
    NvMMBlockHandle *hBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle BlockSemaphore,
    NvMMDoWorkFunction *pDoWorkFunction);

void
NvMMCameraClose(
    NvMMBlockHandle hBlock,
    NvMMInternalDestructionParameters *pParams);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_CAMERA_INT_H 

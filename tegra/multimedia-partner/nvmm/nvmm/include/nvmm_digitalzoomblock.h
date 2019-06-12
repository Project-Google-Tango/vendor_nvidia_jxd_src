/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVMM_DIGITALZOOMBLOCK_H
#define INCLUDED_NVMM_DIGITALZOOMBLOCK_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"

#if defined(__cplusplus)
extern "C"
{
#endif  

NvError
NvMMDigitalZoomOpen(
    NvMMBlockHandle *hBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle BlockSemaphore,
    NvMMDoWorkFunction *pDoWorkFunction);

void
NvMMDigitalZoomClose(
    NvMMBlockHandle hBlock,
    NvMMInternalDestructionParameters *pParams);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_DIGITALZOOMBLOCK_H 

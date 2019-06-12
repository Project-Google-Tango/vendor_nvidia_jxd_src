/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_MP2DEC_H
#define INCLUDED_NVMM_MP2DEC_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"

NvError
NvMMMP2DecOpen(
    NvMMBlockHandle *phBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle BlockEventSema,
    NvMMDoWorkFunction *pDoWorkFunction);

void NvMMMP2DecClose(NvMMBlockHandle hBlock, NvMMInternalDestructionParameters *pParams);

#endif  // INCLUDED_NVMM_MP2DEC_H

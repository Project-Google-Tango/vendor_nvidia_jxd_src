/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @brief Defines the entry points for the super audio block decoder
 */

#ifndef INCLUDED_NVMM_SUPERAUDIODEC_H
#define INCLUDED_NVMM_SUPERAUDIODEC_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"
#include "nvmm_block.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvError NvMMSuperAudioDecOpen(NvMMBlockHandle *phBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle BlockEventSema,
    NvMMDoWorkFunction *pDoWorkFunction);

void NvMMSuperAudioDecClose(NvMMBlockHandle hBlock,
    NvMMInternalDestructionParameters *pParams);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_SUPERAUDIODEC_H

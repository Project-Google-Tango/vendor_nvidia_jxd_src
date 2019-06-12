/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_MIXER_INT_H
#define INCLUDED_NVMM_MIXER_INT_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"

/**
 * @brief Open function for the mixer block. This function is
 * called by the owner of the block's thread to create the block.
 *
 * @param [in] hBlock
 *      Block handle
 * @param [in] semaphore
 *      Block's semaphore used as a trigger when for any external event
 *      which requires the block to do more work, e.g. an interupt fired
 *      upon the hardware's completion of work.
 * @param [out] pDoWorkFunction
 *      Function for the block's thread owner to call to facilitate the
 *      block doing either critical or non-critical work.
 */
NvError
NvMMMixerOpen(
    NvMMBlockHandle *phBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle semaphore,
    NvMMDoWorkFunction *pDoWorkFunction);

void
NvMMMixerClose(
    NvMMBlockHandle hBlock,
    NvMMInternalDestructionParameters *pParams);

#endif  // INCLUDED_NVMM_MIXER_INT_H

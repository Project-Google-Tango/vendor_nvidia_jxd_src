/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_M2V_PARSER_BLOCK_H
#define INCLUDED_NVMM_M2V_PARSER_BLOCK_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvmm_queue.h"
#include "nvassert.h"
#include "nvrm_transport.h"
#include "nvrm_init.h"
#include "nvmm_block.h"

/**
 * @brief Open function for the reference block. This function is
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



NvError NvMMM2vParserBlockOpen(NvMMBlockHandle *phBlock,
                               NvMMInternalCreationParameters *pParams,
                               NvOsSemaphoreHandle semaphore,
                               NvMMDoWorkFunction *pDoWorkFunction);

void NvMMM2vParserBlockClose (NvMMBlockHandle phBlock,
                              NvMMInternalDestructionParameters *pParams);

#endif // INCLUDED_NVMM_AMR_PARSER_BLOCK_H






























































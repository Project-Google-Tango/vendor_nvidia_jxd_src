/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVMM_BASEWRITERBLOCK_H
#define INCLUDED_NVMM_BASEWRITERBLOCK_H

#include "nvmm.h"
#include "nvos.h"
#include "nvmm_block.h"
#include "nvmm_writer.h"
#include "nvmm_core_writer_interface.h"
#include "nvmm_common.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief This function is implementation of NvMMOpenBlockFunction
 * mentioned in nvmm.h.* Please refer to nvmm.h for details.
 *
 */

NvError
NvMMBaseWriterBlockOpen(
    NvMMBlockHandle *pBlockHandle,
    NvMMInternalCreationParameters *CreationParams,
    NvOsSemaphoreHandle BlockSemaHandle,
    NvMMDoWorkFunction *pDoWorkFunction);

/** 
 * Close function which is called by NvMMCloseBlock()
 * If possible block will be closed here, otherwise it will be marked for close.
 * Block will be closed whenever possible in future.
 *
 */

void
NvMMBaseWriterBlockClose(
    NvMMBlockHandle BlockHandle,
    NvMMInternalDestructionParameters *DestructionParams);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NVMM_BASEWRITERBLOCK_H


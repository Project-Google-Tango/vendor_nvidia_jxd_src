/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVMM_3GP_WRITER_BLOCK_H
#define INCLUDED_NVMM_3GP_WRITER_BLOCK_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvmm_queue.h"
#include "nvassert.h"
#include "nvrm_transport.h"
#include "nvrm_init.h"
#include "nvmm_block.h"
#include "nvmm_core_writer_interface.h"

NvError
NvMMCreate3gpWriterContext(
    NvmmCoreWriterOps *CoreWriterOps,
    NvmmWriterContext *pWriterHandle);

#endif // INCLUDED_NVMM_3GP_WRITER_BLOCK_H


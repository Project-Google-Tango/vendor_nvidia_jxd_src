/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm.h"
#include "nvmm_transport.h"
#include "nvassert.h"
#include "stdio.h"

NvError NvMMOpenBlock(NvMMBlockHandle *phBlock, NvMMCreationParameters *pParams)
{
    NvError status = NvSuccess;

    // Sanity check
    NV_ASSERT(phBlock && pParams);

    status = NvMMTransOpenBlock(phBlock, pParams);
    return status;
}

void NvMMCloseBlock(NvMMBlockHandle hBlock)
{
    if (!hBlock)
        return;
    NvMMTransCloseBlock(hBlock);
}


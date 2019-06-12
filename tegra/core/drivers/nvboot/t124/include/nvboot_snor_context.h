/**
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvboot_snor_context.h - Definitions for the SNOR context structure.
 */

#ifndef INCLUDED_NVBOOT_SNOR_CONTEXT_H
#define INCLUDED_NVBOOT_SNOR_CONTEXT_H

#include "t12x/nvboot_snor_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * NvBootSnorContext - The context structure for the SNOR driver.
 * A pointer to this structure is passed into the driver's Init() routine.
 * This pointer is the only data that can be kept in a global variable for
 * later reference.
 */
typedef struct NvBootSnorContextRec
{
    NvU32 PageSizeLog2;

    /// Specifies the mode of data transfer.
    SnorDataXferMode DataXferMode;

    /// Specifies the external memory address to which data should be
    /// transferred from nor.
    NvU32 DmaDestMemAddress;

    /// Maximum time for which driver should wait for controller busy
    /// Value specified in microseconds
    NvU32 CntrllerBsyTimeout;

    /// Maximum time for which driver should wait for dma transfer to complete
    /// Value specified in microseconds
    NvU32 DmaTransferTimeout;

} NvBootSnorContext;


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SNOR_CONTEXT_H */

/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           DMA Resource manager private API for Hw access </b>
 *
 * @b Description: Implements the private interface of the hw access NvRM DMA.
 * This files implements the API for accessing the register of the AP15 Dma
 * controller.
 *
 */

#include "nvcommon.h"
#include "nvrm_interrupt.h"
#include "nvassert.h"
#include "nvrm_hwintf.h"
#include "nvrm_processor.h"
#include "nvrm_drf.h"
#include "t30/arapbdma.h"
#include "rm_dma_hw_private.h"

#define NV_APB_DMA_REGR(rm,reg) \
    NV_REGR(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ApbDma, 0 ), APBDMA_##reg##_0)

#define NV_APB_DMA_REGW(rm,reg,data) \
    NV_REGW(rm, NVRM_MODULE_ID( NvRmPrivModuleID_ApbDma, 0 ), \
        APBDMA_##reg##_0, data)

NvU32 NvRmPrivDmaInterruptDecode(NvRmDeviceHandle hRmDevice )
{
    NvU32   Channel;
    NvU32   Reg;

    // Read the APB DMA channel interrupt status register.
    Reg = NV_APB_DMA_REGR(hRmDevice, IRQ_STA_CPU);

    // Get the interrupting channel number.
    Channel = 31 - CountLeadingZeros(Reg);

    // Get the interrupt disable mask.
    Reg = 1 << Channel;

    // Disable the source.
    NV_APB_DMA_REGW(hRmDevice, IRQ_MASK_CLR, Reg);

    return Channel;
}

void NvRmPrivDmaInterruptEnable(NvRmDeviceHandle hRmDevice, NvU32 Channel,
    NvBool Enable )
{
    NvU32   Reg;

    // Generate the channel mask.
    Reg = 1 << Channel;

    if (Enable)
    {
        // Enable the channel interrupt.
        NV_APB_DMA_REGW(hRmDevice, IRQ_MASK_SET, Reg);
    }
    else
    {
        // Disable the channel interrupt.
        NV_APB_DMA_REGW(hRmDevice, IRQ_MASK_CLR, Reg);
    }
}


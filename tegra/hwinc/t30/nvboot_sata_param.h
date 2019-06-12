/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the parameters and data structure for SATA devices.
 */

#ifndef INCLUDED_NVBOOT_SATA_PARAM_H
#define INCLUDED_NVBOOT_SATA_PARAM_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif


typedef enum
{
    /// Specifies Legacy mode
    NvBootSataMode_Legacy = 1,

    /// Specifies AHCI mode compliant with AHCI 1.3 spec
    NvBootSataMode_Ahci,

    NvBootSataMode_Num,

    NvBootSataMode_Force32 = 0x7FFFFFF
} NvBootSataMode;

typedef enum
{
    /// Specifies Pio mode transfer from device to memory by SATA controller
    NvBootSataTransferMode_Pio = 1,

    /// Specifies DMA transfer from device to memory by SATA controller
    NvBootSataTransferMode_Dma,

    NvBootSataTransferMode_Num,

    NvBootSataTransferMode_Force32 = 0x7FFFFFF
} NvBootSataTransferMode;

typedef enum
{
    /// Specifies SATA clock source to be PLLC.
    NvBootSataClockSource_ClkM = 1,

    /// Specifies SATA clock source to be PLLP.
    NvBootSataClockSource_PllPOut0,

    /// Specifies SATA clock source to be PLLM.
    NvBootSataClockSource_PllMOut0,

    NvBootSataClockSource_Num,

    /// Specifies SATA clock source to be PLLC.
    /// Supported as clock source for SATA but not suppoted in bootrom because
    /// PLLC is not enabled in bootrom.
    NvBootSataClockSource_PllCOut0,

    NvBootSataClockSource_Force32 = 0x7FFFFFF
} NvBootSataClockSource;


/**
 * Defines the parameters SATA devices.
 */
typedef struct NvBootSataParamsRec
{
    /// Specifies the clock source for SATA controller.
    NvBootSataClockSource SataClockSource;

    /// Specifes the clock divider to be used.
    NvU32 SataClockDivider;

    /// Specifies the clock source for SATA controller.
    NvBootSataClockSource SataOobClockSource;

    /// Specifes the clock divider to be used.
    NvU32 SataOobClockDivider;

    /// Specifies the SATA mode to be used.
    NvBootSataMode SataMode;

    /// Specifies mode of transfer from device to memory via controller.
    /// Note: transfer mode doesn't imply SW intervention.
    NvBootSataTransferMode TransferMode;

    /// If SDRAM is initialized, then AHCI DMA mode can be used.
    /// If SDRAM is not initialized, then always use legacy pio mode.
    NvBool isSdramInitialized;

   /// Start address(in SDRAM) for command and FIS structures.
    NvU32 SataBuffersBase;

   /// Start address(in SDRAM) for data buffers.
    NvU32 DataBufferBase;

    /// Plle ref clk divisors
    NvU8 PlleDivM;

    NvU8 PlleDivN;

    NvU8 PlleDivPl;

    NvU8 PlleDivPlCml;

    /// Plle SS coefficients whcih will take effect only when
    /// Plle SS is enabled through the boot dev config fuse
    NvU16 PlleSSCoeffSscmax;

    NvU8 PlleSSCoeffSscinc;

    NvU8 PlleSSCoeffSscincintrvl;

} NvBootSataParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SATA_PARAM_H */

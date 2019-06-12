/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the parameters and data structure for NAND devices.
 */

#ifndef INCLUDED_NVBOOT_NAND_PARAM_H
#define INCLUDED_NVBOOT_NAND_PARAM_H

#if defined(__cplusplus)
extern "C"
{
#endif

/// Defines the params that can be configured for NAND devices.
typedef struct NvBootNandParamsRec
{
    /**
     * Specifies the clock divider for the PLL_P 432MHz source. 
     * If it is set to 18, then clock source to Nand controller is 
     * 432 / 18 = 24MHz.
     */

    NvU8 ClockDivider;

    /// Specifies the value to be programmed to Nand Async Timing Register 0
    NvU32 NandAsyncTiming0;

    /// Specifies the value to be programmed to Nand Async Timing Register 1
    NvU32 NandAsyncTiming1;

    /// Specifies the value to be programmed to Nand Async Timing Register 2
    NvU32 NandAsyncTiming2;

    /// Specifies the value to be programmed to Nand Async Timing Register 3
    NvU32 NandAsyncTiming3;

    /// Specifies the value to be programmed to Nand Sync DDR Timing Register 0
    NvU32 NandSDDRTiming0;

    /// Specifies the value to be programmed to Nand Sync DDR Timing Register 1
    NvU32 NandSDDRTiming1;
    
    /// Specifies the value to be programmed to Nand Toggle DDR Timing Register 0
    NvU32 NandTDDRTiming0;

    /// Specifies the value to be programmed to Nand Toggle DDR Timing Register 1
    NvU32 NandTDDRTiming1;

    /// Specifies the value to be programmed to FBIO_DQSIB_DELAY register
    NvU8 NandFbioDqsibDlyByte;

    /// Specifies the value to be programmed to FBIO_DQUSE_DELAY register
    NvU8 NandFbioQuseDlyByte;

    /// Specifies the CFG_QUSE_LATE value to be programmed to FBIO configuration register
    NvU8 NandFbioCfgQuseLate;

    /// Specifies whether to enable sync DDR more or not
    NvBool DisableSyncDDR;

    /// Specifies the block size in log2 bytes
    NvU8 BlockSizeLog2;

    /// Specifies the page size in log2 bytes
    NvU8 PageSizeLog2;


} NvBootNandParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_NAND_PARAM_H */

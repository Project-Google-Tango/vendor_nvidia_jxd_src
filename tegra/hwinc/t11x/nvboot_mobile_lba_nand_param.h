/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the parameters and data structure for mobileLBA NAND devices.
 */

#ifndef INCLUDED_NVBOOT_MOBILE_LBA_NAND_PARAM_H
#define INCLUDED_NVBOOT_MOBILE_LBA_NAND_PARAM_H

#if defined(__cplusplus)
extern "C"
{
#endif

/// Params that are used to config and initialize MobileLbaNand driver.
typedef struct NvBootMobileLbaNandParamsRec
{
    /**
     * Specifies the clock divider for the PLL_P 432MHz source. 
     * If it is set to 18, then clock source to Nand controller is 
     * 432 / 18 = 24MHz.
     */
    NvU8 ClockDivider;

    /// Specifies the value to be programmed to Nand Timing Register 1
    NvU32 NandTiming;

    /// Specifies the value to be programmed to Nand Timing Register 2
    NvU32 NandTiming2;

    /**
     * Specifies the number of sectors that can be used from Sda region. If the
     * read request falls beyond this sector count, the request will be 
     * diverted to Mba region.
     */
    NvU32 SdaSectorCount;
} NvBootMobileLbaNandParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_MOBILE_LBA_NAND_PARAM_H */

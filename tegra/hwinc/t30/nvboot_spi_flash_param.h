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
 * Defines the parameters and data structure for SPI FLASH devices.
 */

#ifndef INCLUDED_NVBOOT_SPI_FLASH_PARAM_H
#define INCLUDED_NVBOOT_SPI_FLASH_PARAM_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum 
{
    /// Specifies SPI clock source to be PLLP.
    NvBootSpiClockSource_PllPOut0 = 0,

    /// Specifies SPI clock source to be PLLC.
    NvBootSpiClockSource_PllCOut0,

    /// Specifies SPI clock source to be PLLM.
    NvBootSpiClockSource_PllMOut0,

    /// Specifies SPI clock source to be ClockM.
    NvBootSpiClockSource_ClockM,

    NvBootSpiClockSource_Num,
    NvBootSpiClockSource_Force32 = 0x7FFFFFF
} NvBootSpiClockSource;


/**
 * Defines the parameters SPI FLASH devices.
 */
typedef struct NvBootSpiFlashParamsRec
{
    /**
     * Specifies the clock source to use.
     */
    NvBootSpiClockSource ClockSource;

    /**
     * Specifes the clock divider to use.
     * The value is a 7-bit value based on an input clock of 432Mhz.
     * Divider = (432+ DesiredFrequency-1)/DesiredFrequency;
     * Typical values:
     *     NORMAL_READ at 20MHz: 22
     *     FAST_READ   at 33MHz: 14
     *     FAST_READ   at 40MHz: 11
     *     FAST_READ   at 50MHz:  9
     */
    NvU8 ClockDivider;
    
    /**
     * Specifies the type of command for read operations.
     * NV_FALSE specifies a NORMAL_READ Command
     * NV_TRUE  specifies a FAST_READ   Command
     */
    NvBool ReadCommandTypeFast;
} NvBootSpiFlashParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SPI_FLASH_PARAM_H */


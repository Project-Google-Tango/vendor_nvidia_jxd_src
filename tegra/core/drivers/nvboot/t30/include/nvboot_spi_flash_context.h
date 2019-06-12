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
 * nvboot_spi_flash_context.h - Definitions for the SPI_FLASH context
 * structure.
 */

#ifndef INCLUDED_NVBOOT_SPI_FLASH_CONTEXT_H
#define INCLUDED_NVBOOT_SPI_FLASH_CONTEXT_H

#include "t30/nvboot_spi_flash_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * NvBootSpiFlashContext - The context structure for the SPI_FLASH driver.
 * A pointer to this structure is passed into the driver's Init() routine.
 * This pointer is the only data that can be kept in a global variable for
 * later reference.
 */
typedef struct NvBootSpiFlashContextRec
{
    /// Read Command  Type to be used for Read
    NvBool ReadCommandTypeFast;
    /// Page Read Start time in Micro Seconds, Used for finding out timeout.
    NvU32 ReadStartTimeinUs;
    /// Store the current address of the external memory buffer being used.
    NvU8 *CurrentReadBufferAddress;
 } NvBootSpiFlashContext;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SPI_FLASH_CONTEXT_H */


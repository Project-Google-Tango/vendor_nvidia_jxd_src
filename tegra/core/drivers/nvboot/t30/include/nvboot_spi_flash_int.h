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
  * nvboot_spi_flash_int.h - Public definitions for using SPI_FLASH as the
  * second level boot device.
  */

#ifndef INCLUDED_NVBOOT_SPI_FLASH_INT_H
#define INCLUDED_NVBOOT_SPI_FLASH_INT_H

#include "t30/nvboot_error.h"
#include "nvboot_device_int.h"
#include "nvboot_spi_flash_context.h"
#include "t30/nvboot_spi_flash_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// ----------- FUNCTION PROTOTYPES  -------------------------

/**
  * NvBootSpiFlashGetParams(): Returns a pointer to a device-specific
  * structure of device parameters in the FLASH.  Later, the init routine will
  * use them to configure the device. This routine doesn't care for ParamIndex
  * as the fuses are not used by SpiFlash boot device
  *
  */
void
NvBootSpiFlashGetParams(
    const NvU32 ParamIndex,
    NvBootSpiFlashParams **Params);

/**
  * NvBootSpiFlashValidateParams(): Checks the contents of the parameter
  * structure and returns NV_TRUE if the parameters are all legal,
  * NV_FALSE otherwise.
  *
  * @retval  NV_TRUE  - The parameters are valid.
  * @retva  NV_FALSE - The parameters would cause an error if used.
  */

NvBool
NvBootSpiFlashValidateParams(
    const NvBootSpiFlashParams *Params);


/**
  * NvBootDeviceGetBlockSizes(): Queries the block and page sizes for the
  * device.  Character devices should return BlockSize = PageSize = 1.
  *
  * @param  Params Input Parameters
  * @param  BlockSizeLog2 A pointer to the BlokSize mentioned interms of Log2
  * @param  PageSizeLog2 A pointer to the PageSize mentioned interms of Log2
  */
void
NvBootSpiFlashGetBlockSizes(
    const NvBootSpiFlashParams *Params,
    NvU32 *BlockSizeLog2,
    NvU32 *PageSizeLog2);

/**
  * NvBootSpiFlashInit(): Use the data pointed to by DevParams to initialize
  * the device for reading.  Note that the routine will likely be called
  * multiple times - once for initially gaining access to the device,
  * and once to use better parameters stored in the device itself.
  *
  * DriverContext is provided as space for storing device-specific global
  * state. Drivers should keep this pointer around for reference during
  * later calls.
  *
  * Note that the implementation of this function will declare its
  * arguments as pointers to specific structure types, not as void *.
  *
  * @param  Params Input Parameters
  * @param  Context  A pointer to the Context for global state
  *
  * @retval  NvBootError_Success   No Error
  *
  */
NvBootError
NvBootSpiFlashInit(
    const NvBootSpiFlashParams *Params,
    NvBootSpiFlashContext *Context);


/**
  * NvBootSpiFlashReadPage(): Read a page of data into Dest.
  * This entry point is used with block devices.
  *
  * @param  Params Input Parameters
  * @param  Context  A pointer to the Context for global state
  *
  * @retval  NvBootError_Success   No Error
  * @retval  NvBootError_HwTimeOut When H/w doesn't respond or wasn't able
  *          to write
  *
  */
NvBootError
NvBootSpiFlashReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Dest);

/**
  * NvBootSpiFlashQueryStatus(): Queries the status for the last read operation
  *
  * @retval  NvBootDeviceStatus_Idle when Driver is free
  * @retval  NvBootDeviceStatus_ReadFailure when the previous transfer
  *          could not complete in time.
  * @retval  NvBootDeviceStatus_ReadInProgress when the previous transfer is
  *          in progress
  *
  *
  */
NvBootDeviceStatus
NvBootSpiFlashQueryStatus(void);

/**
  * NvBootSpiFlashShutdown(): Shutdown device and cleanup state.
  * This Disables the Clock and keeps the controller in reset
  */
void
NvBootSpiFlashShutdown(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SPI_FLASH_INT_H */


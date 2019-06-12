/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvboot_snor.h - Public definitions for using SNOR as the second level boot
 * device.
 */

#ifndef INCLUDED_NVBOOT_NOR_INT_H
#define INCLUDED_NVBOOT_NOR_INT_H

#include "nvboot_device_int.h"
#include "t30/nvboot_error.h"
#include "nvboot_snor_context.h"
#include "t30/nvboot_snor_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Returns a pointer to a device-specific structure of device parameters
 * in the ROM.  Later, the init routine will use them to configure the device.
 *
 * @param ParamIndex Parma Index that comes from Fuse values.
 * @param Params double pointer to retunr Param info based on
 *          the param index value.
 *
 */
void
NvBootSnorGetParams(
    const NvU32 ParamIndex,
    NvBootSnorParams **Params);

/**
 * Checks the contents of the parameter structure and returns NV_TRUE
 * if the parameters are all legal, NV_FALSE otherwise.
 *
 * @param Params Pointer to Param info that needs validation.
 *
 * @retval NV_TRUE The parameters are valid.
 * @retval NV_FALSE The parameters would cause an error if used.
 */
NvBool
NvBootSnorValidateParams(
    const NvBootSnorParams *Params);

/**
 * Queries the block and page sizes for the device in units of log2(bytes).
 * Thus, a 1KB block size is reported as 10.
 *
 * @param Params Pointer to Param info.
 * @param BlockSizeLog2 returns block size in log2 scale.
 * @param PageSizeLog2 returns page size in log2 scale.
 *
 */
void
NvBootSnorGetBlockSizes(
    const NvBootSnorParams *Params,
    NvU32 *BlockSizeLog2,
    NvU32 *PageSizeLog2);

/**
 * Uses the data pointed to by DeviceParams to initialize
 * the device for reading.  Note that the routine will likely be called
 * multiple times - once for initially gaining access to the device,
 * and once to use better parameters stored in the device itself.
 *
 * DriverContext is provided as space for storing device-specific global state.
 * Drivers should keep this pointer around for reference during later calls.
 *
 * @param ParamData Pointer to Param info to initialize the Nand with.
 * @param Context Pointer to memory, where nand state is saved to.
 *
 * @retval NvBootError_Success No Error
 */
NvBootError
NvBootSnorInit(
    const NvBootSnorParams *ParamData,
    NvBootSnorContext *Context);

/**
 * Initiate the reading of a page of data into Dest.buffer.
 *
 * @param Block Block number to read from.
 * @param Page Page number in the block to read from.
 *          valid range is 0 <= Page < PagesPerBlock.
 * @param Dest Buffer to rad the data into.
 *
 * @retval NvBootError_Success No Error
 */
NvBootError
NvBootSnorReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Dest);

/**
 * Check the status of read operation that is launched with
 *  API NvBootNandReadPage, if it is pending.
 *
 * @retval NvBootDeviceStatus_Idle - Read operation is complete.
 */
NvBootDeviceStatus NvBootSnorQueryStatus(void);

/**
 * Shutdowns device and cleanup the state.
 *
 */
void NvBootSnorShutdown(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_NOR_INT_H */

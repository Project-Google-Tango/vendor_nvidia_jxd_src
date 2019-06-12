/*
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/**
 * nvboot_usb3_int.h - Public definitions for using Usb3
 * as the second level boot device.
 */

#ifndef INCLUDED_NVBOOT_USB3_INT_H
#define INCLUDED_NVBOOT_USB3_INT_H

#include "nvboot_device_int.h"
#include "nvboot_error.h"
#include "nvboot_usb3_context.h"
#include "nvboot_usb3_param.h"

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
NvBootUsb3GetParams(
    const NvU32 ParamIndex,
    NvBootUsb3Params **Params);

/**
 * Checks the contents of the parameter structure and returns NV_TRUE
 * if the parameters are all legal, NV_FLASE otherwise.
 *
 *@param Params Pointer to Param info that needs validation.
 *
 * @retval NV_TRUE The parameters are valid.
 * @retval NV_FALSE The parameters would cause an error if used.
 */
NvBool
NvBootUsb3ValidateParams(
    const NvBootUsb3Params *Params);

/**
 * Queries the block and page sizes for the device in units of log2(bytes).
 * Thus, a 1KB block size is reported as 10.
 *
 * @note NvBootUsb3Init() must have beeen called atleast once before
 *          this API and the call must have returned NvBootError_Success.
 *
 * @param Params Pointer to Param info.
 * @param BlockSizeLog2 returns block size in log2 scale.
 * @param PageSizeLog2 returns page size in log2 scale.
 *
 */
void
NvBootUsb3GetBlockSizes(
    const NvBootUsb3Params *Params,
    NvU32 *BlockSizeLog2,
    NvU32 *PageSizeLog2);

/**
 * Uses the data pointed to by DeviceParams to initialize a Usb3
 * device for reading.  Note that the routine will likely be called
 * multiple times - once for initially gaining access to the device,
 * and once to use better parameters stored in the device itself.
 *
 * DriverContext is provided as space for storing device-specific global state.
 * Drivers should keep this pointer around for reference during later calls.
 *
 * @param ParamData Pointer to Param info to initialize the MiobileLbaNand with.
 * @param Context Pointer to memory, where nand state is saved to.
 *
 * @retval NvBootError_Success Initialization is successful.
 * @retval NvBootError_HwTimeOut Device is not responding.
 * @retval NvBootError_DeviceUnsupported Device is not supported.
 */
NvBootError
NvBootUsb3Init(
    const NvBootUsb3Params *ParamData,
    NvBootUsb3Context *Context);

/**
 * Initiate the reading of a page of data into Dest.buffer.
 *
 * @param Block Block number to read from.
 * @param Page Page number in the block to read from.
 *          valid range is 0 <= Page < PagesPerBlock.
 * @param Dest Buffer to rad the data into. The buffer must be atleast 4 byte
 *          aligned.
 *
 * @retval NvBootError_Success Read operation is launched successfully.
 * @retval NvBootError_HwTimeOut Device is not responding.
 */
NvBootError
NvBootUsb3ReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Dest);

/**
 * Check the status of read operation that is launched with
 *  API NvBootUsb3ReadPage, if it is pending.
 *
 * @retval NvBootDeviceStatus_ReadInProgress - Reading in progress.
 * @retval NvBootDeviceStatus_EccFailure - Data is corrupted and couldn't be
 *          recovered back with Ecc.
 * @retval NvBootDeviceStatus_ReadFailure - Read operation failed.
 * @retval NvBootDeviceStatus_Idle - Read operation is complete and successful.
 */
NvBootDeviceStatus NvBootUsb3QueryStatus(void);

/**
 * Shutdowns the Usb3 device and cleansup the state.
 *
 */
void NvBootUsb3Shutdown(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_USB3_INT_H */

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
 * nvboot_sdmmc_int.h - Public definitions for using Sdmmc as the second
 * level boot device.
 */

#ifndef INCLUDED_NVBOOT_SDMMC_INT_H
#define INCLUDED_NVBOOT_SDMMC_INT_H

#include "nvboot_device_int.h"
#include "t30/nvboot_error.h"
#include "nvboot_sdmmc_context.h"
#include "t30/nvboot_sdmmc_param.h"

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
void NvBootSdmmcGetParams(const NvU32 ParamIndex, NvBootSdmmcParams **Params);

/**
 * Checks the contents of the parameter structure and returns NV_TRUE
 * if the parameters are all legal, NV_FLASE otherwise.
 *
 *@param Params Pointer to Param info that needs validation.
 *
 * @retval NV_TRUE The parameters are valid.
 * @retval NV_FALSE The parameters would cause an error if used.
 */
NvBool NvBootSdmmcValidateParams(const NvBootSdmmcParams *Params);

/**
 * Queries the block and page sizes for the device in units of log2(bytes).
 * Thus, a 1KB block size is reported as 10.
 *
 * @note NvBootSdmmcInit() must have beeen called at least once before
 * calling this API and the call must have returned NvBootError_Success.
 *
 * @param Params Pointer to Param info.
 * @param BlockSizeLog2 returns block size in log2 scale.
 * @param PageSizeLog2 returns page size in log2 scale.
 *
 * @retval NvBootError_Success No Error
 */
void
NvBootSdmmcGetBlockSizes(
    const NvBootSdmmcParams *Params,
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
 * @retval NvBootError_Success Initialization is successful.
 * @retval NvBootError_HwTimeOut Device is not responding.
 * @retval NvBootError_DeviceResponseError Response Recevied from device
 *          indicated that operation had failed.
 * @retval NvBootError_DeviceError Error in communicating with device. No need
 *          to retry. It is taken care internally.
 */
NvBootError
NvBootSdmmcInit(
    const NvBootSdmmcParams *ParamData,
    NvBootSdmmcContext *Context);

/**
 * Initiate the reading of a page of data into Dest.buffer.
 *
 * @param Block Block number to read from.
 * @param Page Page number in the block to read from.
 *          valid range is 0 <= Page < PagesPerBlock.
 * @param Dest Buffer to rad the data into.
 *
 * @retval NvBootError_Success Read operation is launched successfully.
 * @retval NvBootError_HwTimeOut Device is not responding.
 * @retval NvBootError_DeviceResponseError Response Recevied from device
 *          indicated that operation had failed.
 * @retval NvBootError_DeviceError Error in communicating with device. No need
 *          to retry. It is taken care internally.
 */
NvBootError
NvBootSdmmcReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Dest);

/**
 * Check the status of read operation that is launched with
 *  API NvBootSdmmcReadPage, if it is pending.
 *
 * @retval NvBootDeviceStatus_ReadInProgress - Reading is in progress.
 * @retval NvBootDeviceStatus_CrcFailure - Data received is corrupted. Client
 *          should try to read again and should be upto 3 times.
 * @retval NvBootDeviceStatus_DataTimeout Data is not received from device.
 * @retval NvBootDeviceStatus_ReadFailure - Read operation failed.
 * @retval NvBootDeviceStatus_Idle - Read operation is complete and successful.
 */
NvBootDeviceStatus NvBootSdmmcQueryStatus(void);

/**
 * Shutdowns device and cleanup the state.
 *
 */
void NvBootSdmmcShutdown(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SDMMC_INT_H */

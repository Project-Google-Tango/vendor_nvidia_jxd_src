/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvbit.h
 * @brief <b> Nv Boot Information Table Management Framework.</b>
 *
 * @b Description: This file declares the API's for interacting with
 *    the Boot Information Table (BIT).
 */

#ifndef INCLUDED_NVBIT_H
#define INCLUDED_NVBIT_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

//FixMe: Extend this enum to support AP20 bit feilds

/**
 * Data elements of the BIT which can be queried and set via
 * the NvBit API
 */
typedef enum
{
    /// Version of Boot ROM code
    NvBitDataType_BootRomVersion,

    /// Version of Boot ROM data structures
    NvBitDataType_BootDataVersion,

    /// Version of Recovery Mode protocol
    NvBitDataType_RcmVersion,

    /// Specifies the type of boot.
    NvBitDataType_BootType,

    /// Specifies the primary boot device.
    NvBitDataType_PrimaryDevice,

    /// Specifies the secondary boot device.
    NvBitDataType_SecondaryDevice,

    /// Specifies the measured oscillator frequency.
    NvBitDataType_OscFrequency,

    /// Specifies if a valid BCT was found.
    NvBitDataType_IsValidBct,

    /// Specifies the block number in which the BCT was found.
    NvBitDataType_ActiveBctBlock,

    /// Specifies the page number of the start of the BCT that was found.
    NvBitDataType_ActiveBctSector,

    /// Specifies the size of the BCT in bytes.  It is 0 until BCT loading
    /// is attempted.
    NvBitDataType_BctSize,

    /// Specifies a pointer to the BCT in memory.  It is NULL until BCT
    /// loading is attempted.  The BCT in memory is the last BCT that
    /// the BR tried to load, regardless of whether the operation was
    /// successful.
    NvBitDataType_ActiveBctPtr,

    /// Specifies the outcome of attempting to load the specified BL.
    NvBitDataType_BlStatus,

    /// Specifies whether info from BL was used to fill in missing data for
    /// an earlier BL in the load order
    NvBitDataType_BlUsedForEccRecovery,

    /// Specifies the lowest iRAM address that preserves communicated data.
    /// SafeStartAddr starts out with the address of memory following
    /// the BIT.  When BCT loading starts, it is bumped up to the
    /// memory following the BCT.
    NvBitDataType_SafeStartAddr,


    NvBitDataType_Num,
    NvBitDataType_Max = 0x7fffffff
} NvBitDataType;


typedef enum
{
    NvBitStatus_None = 0,
    NvBitStatus_Success,
    NvBitStatus_ValidationFailure,
    NvBitStatus_DeviceReadError,
    NvBitStatus_Force32 = 0x7fffffff
} NvBitRdrStatus;

typedef enum
{
    NvBitBootDevice_None = 0,
    NvBitBootDevice_Nand8,
    NvBitBootDevice_Emmc,
    NvBitBootDevice_SpiNor,
    NvBitBootDevice_Nand16,
    NvBitBootDevice_Nor,
    NvBitBootDevice_MobileLbaNand,
    NvBitBootDevice_MuxOneNand,
    NvBitBootDevice_Sata,
    NvBitBootDevice_Current,
    NvBitBootDevice_Num,
    NvBitBootDevice_Max = 0x7fffffff
} NvBitSecondaryBootDevice;


typedef struct NvBitRec *NvBitHandle;

/**
 * Create a handle to the BIT.
 *
 * This routine must be called before any of the other NvBit*() API's.  The
 * handle must be passed as an argument to all subsequent NvBit*() invocations.
 *
 * @param phBit pointer to handle for invoking subsequent operations on the BIT;
 *        can be specified as NULL to query BIT size
 *
 * @retval NvSuccess BIT handle creation successful
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter No valid BIT exists
 */

NvError
NvBitInit(
    NvBitHandle *phBit);

/**
 * Release BIT handle and associated resources (but not the BIT itself).
 *
 * @param hBit handle to BIT instance
 */

void
NvBitDeinit(
    NvBitHandle hBit);

/**
 * Retrieve data from the BIT.
 *
 * This API allows the size of the data element to be retrieved, as well as the
 * number of instances of the data element present in the BIT.
 *
 * To retrieve that value of a given data element, allocate a Data buffer large
 * enough to hold the requested data, set *Size to the size of the buffer, and
 * set *Instance to the instance number of interest.
 *
 * To retrieve the size and number of instances of a given type of data element,
 * specified a *Size of zero and a non-NULL Instance (pointer).  As a result,
 * NvBitGetData() will set *Size to the actual size of the data element and
 * *Instance to the number of available instances of the data element in the
 * BIT, then report NvSuccess.
 *
 * @param hBit handle to BIT instance
 * @param DataType type of data to obtain from BIT
 * @param Size pointer to size of Data buffer; set *Size to zero in order to
 *        retrieve the data element size and number of instances
 * @param Instance pointer to instance number of the data element to retrieve
 * @param Data buffer for storing the retrieved instance of the data element
 *
 * @retval NvSuccess BIT data element retrieved successfully
 * @retval NvError_InsufficientMemory data buffer (*Size) too small
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Unknown data type, or illegal instance number
 */

NvError
NvBitGetData(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

/**
 * Query whether a BCT update is in progress, but not yet completed
 *
 * @param hBit handle to BIT instance
 * @param pIsInterruptedUpdate pointer to boolean, set to NV_TRUE if a BCT
 *        update was interrupted; else NV_FALSE
 *
 * @retval NvSuccess Interrupt status reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 */

NvError
NvBitQueryIsInterruptedUpdate(
    NvBitHandle hBit,
    NvBool *pIsInterruptedUpdate);

/**
 * Remaps from the chip-specific boot device value stored in the BIT to the
 * common NvBitSecondaryBootDevice type for use by higher-level software
 *
 * @param BitDataDevice The value returned by a call to NvBitGetData for the
 *        NvBitDataType_SecondaryBootDevice data type.
 *
 * @returns NvBitSecondaryBootDevice value corresponding to BitDataDevice
 */
NvBitSecondaryBootDevice
NvBitGetBootDevTypeFromBitData(
    NvU32 BitDataDevice);
/**
 * Remaps from the chip-specific Boot Reading status stored in the BIT to the
 * common NvBitReading status type for use by higher-level software
 *
 * @param BootRdrStatus The Boot reading status value returned by a call to NvBitGetData for the
 * NvBitDataType_BlStatus data type.
 *
 * @returns NvBitReading status value corresponding to BootReading status
 */
NvBitRdrStatus NvBitGetBitStatusFromBootRdrStatus(NvU32 BootRdrStatus);
#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBIT_H


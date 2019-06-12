/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_owr_H
#define INCLUDED_nvrm_owr_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_pinmux.h"
#include "nvrm_module.h"
#include "nvrm_init.h"

#include "nvos.h"
#include "nvcommon.h"

/**
 * NvRmOwrHandle is an opaque handle for the RM OWR driver.
 */

typedef struct NvRmOwrRec *NvRmOwrHandle;

/**
 * @brief Open the OWR driver. This function allocates the
 * RM OWR handle.
 *
 * Assert encountered in debug mode if passed parameter is invalid.
 *
 * @param hDevice Handle to the Rm device which is required by Rm to acquire
 * the resources from RM.
 * @param instance Instance of the OWR controller to be opened. Starts from 0.
 * @param phOwr Points to the location where the OWR handle shall be stored.
 *
 * @retval NvSuccess OWR driver opened successfully.
 * @retval NvError_InsufficientMemory Indicates that function fails to allocate
 * the memory.
 */

 NvError NvRmOwrOpen(
    NvRmDeviceHandle hDevice,
    NvU32 instance,
    NvRmOwrHandle * hOwr );

/**
 * @brief Closes the OWR driver. Disables the clock and invalidates the OWR handle.
 * This API never fails.
 *
 * @param hOwr A handle from NvRmOwrOpen().  If hOwr is NULL, this API does
 *     nothing.
 */

 void NvRmOwrClose(
    NvRmOwrHandle hOwr );

/**
 * Defines OWR transaction flags.
 */

typedef enum
{

      /// OWR read the unique address of the device.
          NvRmOwr_ReadAddress = 1,

      /// OWR memory read transaction.
          NvRmOwr_MemRead,

      /// OWR memory write transaction.
          NvRmOwr_MemWrite,

      /// OWR memory readbyte transaction.
      /// Reads single byte on the OWR bus from the device.
          NvRmOwr_ReadByte,

      /// OWR writebyte transaction.
      /// Writes single byte on the OWR bus to the device.
          NvRmOwr_WriteByte,

      /// OWR memory Check Presence.
      /// Sends Reset Presence Pulse to the deivce on the OWR bus.
          NvRmOwr_CheckPresence,

      /// OWR readbit transaction.
      /// Reads single bit on the OWR bus from the device.
      /// The LSB will be received first.
          NvRmOwr_ReadBit,

      /// OWR writebit transaction.
      /// Writes single bit on the OWR bus to the device.
      /// The LSB will be transmitted first.
          NvRmOwr_WriteBit,
    NvRmOwrTransactionFlags_Num,
    NvRmOwrTransactionFlags_Force32 = 0x7FFFFFFF
} NvRmOwrTransactionFlags;

/**
 * Defines OWR transaction info structure. Contains details of the transaction.
 */

typedef struct NvRmOwrTransactionInfoRec
{

    /// Transaction type flags. See @NvRmOwrTransactionFlags
        NvU32 Flags;

    /// Offset in the OWR device where Memory read/write operations need to be performed.
        NvU32 Offset;

    /// Number of bytes to read/write.
        NvU32 NumBytes;

    /// OWR device ROM Id. This can be zero, if there is a single OWR device on the bus.
        NvU32 Address;
} NvRmOwrTransactionInfo;

/**
 * @brief Does multiple OWR transactions. Each transaction can be a read or write.
 *
 * @param hOwr Handle to the OWR channel.
 * @param OwrPinMap for OWR controllers which are being multiplexed across
 *        multiple pin mux configurations, this specifies which pin mux configuration
 *        should be used for the transaction.  Must be 0 when the ODM pin mux query
 *        specifies a non-multiplexed configuration for the controller.
 * @param Data Pointer to the buffer for all the required read, write transactions.
 * @param DataLength Length of the data buffer.
 * @param Transcations Pointer to the NvRmOwrTransactionInfo structure.
 * See @NvRmOwrTransactionInfo
 * @param NumOfTransactions Number of transcations
 *
 *
 * @retval NvSuccess OWR Transaction succeeded.
 * @retval NvError_NotSupported Indicates assumption on parameter values violated.
 * @retval NvError_InvalidState Indicates that the last read or write call is not
 * completed.
 * @retval NvError_ControllerBusy Indicates controller is presently busy with an
 * OWR transaction.
 */

 NvError NvRmOwrTransaction(
    NvRmOwrHandle hOwr,
    NvU32 OwrPinMap,
    NvU8 * Data,
    NvU32 DataLen,
    NvRmOwrTransactionInfo * Transaction,
    NvU32 NumOfTransactions );

#if defined(__cplusplus)
}
#endif

#endif

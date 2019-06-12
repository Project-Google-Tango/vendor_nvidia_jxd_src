/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_xpc_H
#define INCLUDED_nvrm_xpc_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_module.h"
#include "nvrm_init.h"

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_init.h"
#include "nvrm_interrupt.h"

/**
 * @brief 16 Byte allignment for the shared memory message transfer.
 */

typedef enum
{
    XPC_MESSAGE_ALIGNMENT_SIZE = 0x10,
    Xpc_Alignment_Num,
    Xpc_Alignment_Force32 = 0x7FFFFFFF
} Xpc_Alignment;

/**
 * NvRmPrivXpcMessageHandle is an opaque handle to NvRmPrivXpcMessage.
 *
 * @ingroup nvrm_xpc
 */

typedef struct NvRmPrivXpcMessageRec *NvRmPrivXpcMessageHandle;

/**
 * Create the xpc message handles for sending/receiving the message to/from
 * target processor.
 * This function allocates the memory (from multiprocessor shared memory
 * region) and os resources for the message transfer and synchrnoisation.
 *
 * @see NvRmPrivXpcSendMessage()
 * @see NvRmPrivXpcGetMessage()
 *
 * @param hDevice Handle to the Rm device which is required by Ddk to acquire
 * the resources from RM.
 * @param phXpcMessage Pointer to the handle to Xpc message where created
 * Xpc message handle is stored.
 *
 * @retval NvSuccess Indicates the message queue is successfully created.
 * @retval NvError_BadValue The parameter passed are incorrect.
 * @retval NvError_InsufficientMemory Indicates that function fails to allocate the
 * memory for message queue.
 * @retval NvError_MemoryMapFailed Indicates that the memory mapping for xpc
 * controller register failed.
 * @retval NvError_NotSupported Indicates that the requested operation is not
 * supported for the given target processor/Instance.
 *
 */

 NvError NvRmPrivXpcCreate(
    NvRmDeviceHandle hDevice,
    NvRmPrivXpcMessageHandle * phXpcMessage );

/**
 * Destroy the created Xpc message handle. This frees all the resources
 * allocated for the xpc message handle.
 *
 * @note After calling this function client will not able to  send/receive any
 * message.
 *
 * @see NvRmPrivXpcMessageCreate()
 *
 * @param hXpcMessage Xpc message queue handle which need to be destroy.
 * This cas created when function NvRmPrivXpcMessageCreate() was called.
 *
 */

 void NvRmPrivXpcDestroy(
    NvRmPrivXpcMessageHandle hXpcMessage );

 NvError NvRmPrivXpcSendMessage(
    NvRmPrivXpcMessageHandle hXpcMessage,
    NvU32 data );

 NvU32 NvRmPrivXpcGetMessage(
    NvRmPrivXpcMessageHandle hXpcMessage );

/**
 * Initializes the Arbitration semaphore system for cross processor synchronization.
 *
 * @param hDevice The RM handle.
 *
 * @retval "NvError_IrqRegistrationFailed" if interupt is already registred.
 * @retval "NvSuccess" if successfull.
 */

 NvError NvRmXpcInitArbSemaSystem(
    NvRmDeviceHandle hDevice );

/**
 * De-Initializes the Arbitration semaphore system for cross processor synchronization.
 *
 * @param hDevice The RM handle.
 *
 */

 void NvRmXpcDeinitArbSemaSystem(
    NvRmDeviceHandle hDevice );

/**
 * Tries to obtain a hw arbitration semaphore. This API is used to
 * synchronize access to hw blocks across processors.
 *
 * @param modId The module that we need to cross-processor safe access to.
 */

 void NvRmXpcModuleAcquire(
    NvRmModuleID modId );

/**
 * Releases the arbitration semaphore corresponding to the given module id.
 *
 * @param modId The module that we are releasing.
 */

 void NvRmXpcModuleRelease(
    NvRmModuleID modId );

#if defined(__cplusplus)
}
#endif

#endif

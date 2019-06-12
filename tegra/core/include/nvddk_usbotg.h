/*
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Driver Development Kit: USB On-The-Go
 * Interface</b>
 *
 * @b Description: This header declares an interface for the USB
 * OTG driver.
 *
 */

#ifndef INCLUDED_NVDDK_USBOTG_H
#define INCLUDED_NVDDK_USBOTG_H

#include "nvcommon.h"
#include "nvrm_memmgr.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif
/**
 * @defgroup nvddk_usbotg USB Host OTG Interface
 * 
 * This file defines the interface to the USB 
 * OTG driver.
 * 
 * @ingroup nvddk_modules
 * @{
 */


/** Opaque handle to a USB OTG driver.*/
typedef struct NvDdkUsbOtgRec* NvDdkUsbOtgHandle;


/**
 * Defines the USB OTG operation mode type.
 */
typedef enum 
{
    NvDdkUsbOtgOperationModeType_None = 0,
    NvDdkUsbOtgOperationModeType_Function,
    NvDdkUsbOtgOperationModeType_Host,

    NvDdkUsbOtgOperationModeType_Force32 = 0x7FFFFFFF

} NvDdkUsbOtgOperationModeType;


/**
 * Opens the USB OTG module and returns its handle to the caller.
 *
 * @param hRmDevice Handle to the Rm device which is required by DDK to
 *                               acquire the resources from RM.
 * @param hUsbClientSema Semaphore to get client from the DDK.
 * @param hUsbOtg Where data specific to this instantiation of the USB Otg module 
 *              will be stored. If the open is successful, 
 *              this handle is passed to all USB OTG APIs.
 * @retval NvSuccess. On success.
 * @retval NvError_InsufficientMemory   In case of insufficient memory.
 */
NvError NvDdkUsbOtgOpen(NvRmDeviceHandle hRmDevice, NvOsSemaphoreHandle hUsbClientSema,NvDdkUsbOtgHandle* hUsbOtg);


/**
 * Closes the USB OTG module.
 *
 * @param hUsbOtg The USB Otg module handle that was returned by NvDdkUsbOtgOpen().
 */
void NvDdkUsbOtgClose(NvDdkUsbOtgHandle hUsbOtg);


/**
 * Suspends mode entry function for USB OTG driver. 
 * Powers down the controller to enter suspend state. 
 * The system is going down here and any error returned cannot be interpreted 
 * to stop the system from going down.
 *
 * @param hUsbOtg The USB handle that is allocated by the driver after calling
 * the NvDdkUsbOtgOpen().
 * @param IsDpd NV_TRUE indicates suspend called when entering to deep power 
 * down state. NV_FALSE indicates suspend called for ID pin detection.
 */
void NvDdkUsbOtgSuspend(NvDdkUsbOtgHandle hUsbOtg, NvBool IsDpd);

/**
 * Suspend mode exit function for USB OTG driver. 
 * This resumes the controller from the suspend state. Here the system 
 * must come up quickly, and we cannot afford to interpret the error and 
 * stop system from coming up.
 *
 * @param hUsbOtg The USB handle that is allocated by the driver after calling
 * the NvDdkUsbOtgOpen().
 * @param IsDpd NV_TRUE indicates resume called when exiting from deep power 
 * down state. NV_FALSE indicates resume called for ID pin detection.
 */
void NvDdkUsbOtgResume(NvDdkUsbOtgHandle hUsbOtg, NvBool IsDpd);

/**
 * Register/unregister interrupts for USB OTG module.
 *
 * @param hUsbOtg The USB OTG module handle that was returned by NvDdkUsbOtgOpen().
 * @param enable NV_TRUE to register interrupts, NV_FALSE to unregister.
 * @retval NvSuccess On success.
 */
NvError NvDdkUsbOtgRegisterInterrupts(NvDdkUsbOtgHandle hUsbOtg,NvBool enable);


/**
 * Gets the USB operation mode type.
 *
 * @param hUsbOtg The USB OTG module handle that was returned by NvDdkUsbOtgOpen().
 * @return The USB operation mode type.
 */
NvDdkUsbOtgOperationModeType 
    NvDdkUsbOtgGetOperationMode(
    NvDdkUsbOtgHandle hUsbOtg);


#if defined(__cplusplus)
}
#endif
/** @} */

#endif  // INCLUDED_NVDDK_USBOTG_H


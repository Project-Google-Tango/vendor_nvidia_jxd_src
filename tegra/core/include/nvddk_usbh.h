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
 * <b>NVIDIA Driver Development Kit: USB Host
 * Controller Interface</b>
 *
 * @b Description: This header declares an interface for the USB
 * host controller driver.
 *
 */

#ifndef INCLUDED_NVDDK_USBH_H
#define INCLUDED_NVDDK_USBH_H

#include "nvcommon.h"
#include "nvrm_memmgr.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvddk_usbh USB Host Controller Interface
 * 
 * This file defines the interface to the USB 
 * host controller driver.
 * 
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Opaque handle to a USB host
 * controller driver.
 */
typedef struct NvDdkUsbHcdRec* NvDdkUsbhHandle;

/** Size of the setup packet as per the USB spec. */
 enum {NVDDK_USBH_SETUP_PKT_SIZE = 0x8};

/** Defines the pipe transfer type.*/
typedef enum
{
    /// This represents control endpoint.
    NvDdkUsbhPipeType_Control = 1,

    /// This represents isochronous endpoint.
    NvDdkUsbhPipeType_Isoc,

    /// This represents bulk endpoint.
    NvDdkUsbhPipeType_Bulk,

    /// This represents interrupt endpoint.
    NvDdkUsbhPipeType_Interrupt,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkUsbhPipeType_Force32 = 0x7FFFFFFF
} NvDdkUsbhPipeType;


/**
 * Defines the direction of the endpoint pipe.
 */
typedef enum
{
    /// This is the value to represent direction of the Pipe is OUT.
    NvDdkUsbhDir_Out = 1,

    /// This is the value to represent direction of the pipe is IN.
    NvDdkUsbhDir_In,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkUsbhDir_Force32 = 0x7FFFFFFF
} NvDdkUsbhDir;

/**
 * Defines possible USB Port Speed types.
 */
typedef enum
{
    /// Defines the port full speed.
    NvDdkUsbPortSpeedType_Full = 1,

    /// Defines the port low speed.
    NvDdkUsbPortSpeedType_Low,

    /// Defines the port high speed.
    NvDdkUsbPortSpeedType_High,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkUsbPortSpeedType_Force32 = 0x7FFFFFF
} NvDdkUsbPortSpeedType;

/**
 * Defines USB event types used to pass
 * out-of-band control and status between the module and its
 * clients.
 *
 * USB events are the events generated in the USB host DDK, which will be
 * intimated to the client through ::NvDdkUsbhGetEvents.
 * Individual events are explained below
 */
typedef enum
{
    /// A value that should never be used.
    NvDdkUsbhEvent_None = 0x00000000,

    /// USB cable connected.
    NvDdkUsbhEvent_CableConnect = 0x00000001,

    /// USB cable disconnected.
    NvDdkUsbhEvent_CableDisConnect = 0x00000002,

    /// Port enabled by the host controller after RESET the port.
    NvDdkUsbhEvent_PortEnabled = 0x00000004,

    /// USB bus has transitioned to suspended state.
    NvDdkUsbhEvent_BusSuspend = 0x000000008,

    /// USB bus has transitioned out of suspend state.
    NvDdkUsbhEvent_BusResume = 0x000000010,

    /// Represents the transfer is completed 
    NvDdkUsbhEvent_TransferDone = 0x000000020,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkUsbhEvent_Force32 = 0x7FFFFFFF
} NvDdkUsbhEvent;


/**
 * Defines possible USB transfer status.
 */
typedef enum
{
    /// Requested transfer is successful.
    NvDdkUsbhTransferStatus_Success = 0x00000000,
    /// Ping State
    NvDdkUsbhTransferStatus_PingState = 0x00000001,

    /// Split transaction state.
    NvDdkUsbhTransferStatus_SplitXstate = 0x00000002,

    /// Missed micro frame.
    NvDdkUsbhTransferStatus_MissedUFrame = 0x00000004,

    /// Transaction error.
    NvDdkUsbhTransferStatus_TransactionError = 0x00000008,

    /// Babble detected.
    NvDdkUsbhTransferStatus_BabbleDetected = 0x00000010,

    /// Data buffer error.
    NvDdkUsbhTransferStatus_DataBufferError = 0x00000020,

    /// Halted.
    NvDdkUsbhTransferStatus_Halted = 0x00000040,

    /// Transfer is still in active state.
    NvDdkUsbhTransferStatus_Active = 0x00000080,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkUsbhTransferStatus_Force32 = 0x7FFFFFFF
} NvDdkUsbhTransferStatus;

/**
 * Contains all information required to
 * create the PIPE. It contains the endpoint handler, which will be
 * returned after pipe has been created successfully.
 *                      
 */
typedef struct
{
    /// Device address to which this pipe belongs.
    NvU32 DeviceAddr;

    /// Type of endpoint type.  See ::NvDdkUsbhPipeType.
    NvDdkUsbhPipeType EndpointType;

    /// Direction of the pipe. See ::NvDdkUsbhDir.
    NvDdkUsbhDir PipeDirection;

    /// Endpoint number.
    NvU32 EpNumber;

    /// Max packet length for this endpoint.
    NvU32 MaxPktLength;

    /// Speed of the device to which it belongs.
    NvU32 SpeedOfDevice;

    /// EP handler returned by the DDK driver after pipe has been successfully created.
    /// This handler is used for all further transfers that will happen on that pipe.
    NvU32 EpPipeHandler;
} NvDdkUsbhPipeInfo;


/**
 * Sends or receives data from the USB host.
 * Client must fill this structure with proper transfer parameters and pass it to 
 * the DDK through the ::NvDdkUsbhSubmitTransfer API.
 */
typedef struct
{
    /// (in) This is the handler returned with NvDdkUsbhAddPipe API.
    NvU32 EpPipeHandler;

    /// (in) Buffer address containing the data to transfer.
    NvU8 *pTransferBuffer;

    /// (in) Length of the data to transfer.
    NvU32 TransferBufferLength;

    /// (in) In case of control pipe, client should fill setup packet buffer.
    NvU8 SetupPacket[NVDDK_USBH_SETUP_PKT_SIZE];

    /// (in) This semaphore will be signaled by the DDK driver after 
    /// completion of the transfer.
    NvOsSemaphoreHandle TransferSema;

    /// (return) Filled by the DDK driver, after transfer completion. 
    /// Client can use this information to know how many bytes transferred.
    NvU32 ActualLength;

    /// (return) Status of the transfer.
    NvU32 Status;
} NvDdkUsbhTransfer;

/**
 * Opens the USB host controller module
 * and returns its handle to the caller.
 *
 * @param hRmDevice Handle to the Rm device which is required by DDK to
 *                               acquire the resources from RM.
 * @param hUsbh Where data specific to this instantiation of the USBH module 
 *              will be stored. If the open is successful, this handle is passed
 *              to all USBH APIs.
 * @param UsbEventSema Semaphore to get events from the DDK.
 *.@param Instance USB host instance.
 * @retval NvSuccess On success.
 * @retval NvError_InsufficientMemory     Incase of insufficient Memory
 */
NvError
NvDdkUsbhOpen(
    NvRmDeviceHandle hRmDevice,
    NvDdkUsbhHandle* hUsbh,
    NvOsSemaphoreHandle UsbEventSema,
    NvU32 Instance);

/**
 * Call only after open 
 * gets called. This funciton performs enabling of interrupts, setting 
 * clocks, and initilization of controller sheduling. 
 * This should get called only after Cable has been connected. 
 *
 * @param hUsbh Handle to the Rm device, which is required by DDK to 
 * acquire the resources from RM.
 * @param IsInitialize Indicates whether or not host controller is in full initilization mode.
 *
 * @retval NvSuccess on success
 * @retval NvError_UsbhCableDisConnected  If cable has not been connected yet.
 * @retval NvError_NotInitialized The module handle
 * is not valid.
 */

NvError NvDdkUsbhStart(NvDdkUsbhHandle hUsbh, NvBool IsInitialize);

/**
 * Closes the USBH module.
 *
 * @param hUsbh The USB host module handle that was returned by NvDdkUsbhOpen().
 * @param IsInitialize Indicates whether or not host controller is in full initilization mode.
 *
 */
void NvDdkUsbhClose(NvDdkUsbhHandle hUsbh, NvBool IsInitialize);

/**
 * Adds the control or bulk endpoint QH to the list.
 * 
 * @param hUsbh The USB function module handle that was returned
 *                         by NvDdkUsbhOpen().
 * 
 * @param AddPipe This is the pointer to the ::NvDdkUsbhPipeInfo structure.
 * 
 * @retval NvSuccess On success.
 * @retval NvError_NotInitialized  The module handle
 * is not valid.
 * @retval NvError_InsufficientMemory     Incase of insufficient memory.
 */
NvError NvDdkUsbhAddPipe(NvDdkUsbhHandle hUsbh, NvDdkUsbhPipeInfo *AddPipe);

/**
 * Deletes the pipe from the list.
 * 
 * @param hUsbh: The USB function module handle that was returned
 *                         by NvDdkUsbhOpen().
 * 
 * @param DeleteEpPipeHandler This is the handler to the endpoint that needs 
 *                                            to be removed from the list.
 * 
 * @retval NvSuccess On success.
 * @retval NvError_NotInitialized  The module handle
 * is not valid.
 */
NvError NvDdkUsbhDeletePipe(NvDdkUsbhHandle hUsbh, NvU32 DeleteEpPipeHandler);

/**
 * Gets the port speed.
 * 
 * @param hUsbh The USB function module handle that was returned
 *                         by NvDdkUsbhOpen().
 * 
 * @param pGetPortSpeed This is the pointer that holds the port speed.
 *                                   This will be filled by DDK driver.
 * 
 * @retval NvSuccess on success
 * @retval NvError_NotInitialized In case if port is not enabled.
 */
NvError NvDdkUsbhGetPortSpeed(NvDdkUsbhHandle hUsbh, NvU32 *pGetPortSpeed);

/**
 * Transmits or receives the data over the specified EP pipe(OUT or IN).
 * 
 * @param hUsbh The USB function module handle that was returned
 * by NvDdkUsbhOpen().
 * 
 * @param pTransfer Pointer to the ::NvDdkUsbhTransfer structure, which
 * contains all required parameters to transmit data.
 * 
 * @retval NvSuccess on success
 * @retval NvError_InsufficientMemory     In case of insufficient memory.
 * @retval NvError_UsbhTxfrFail     In case transfer failed. For more details of
 * the failure, go to the Status of \c NvDdkUsbhTransfer structure.
 */
NvError
NvDdkUsbhSubmitTransfer(
    NvDdkUsbhHandle hUsbh,
    NvDdkUsbhTransfer* pTransfer);

/**
 * Aborts the transfer specified in \a pTransfer pointer.
 *
 * @param hUsbh The USB function module handle that was returned
 * by NvDdkUsbhOpen().
 *
 * @param pTransfer The pointer to the ::NvDdkUsbhTransfer structure.
 *
 * @retval NvSuccess on success
 */
NvError
NvDdkUsbhAbortTransfer(
    NvDdkUsbhHandle hUsbh,
    NvDdkUsbhTransfer* pTransfer);

/**
 * Gets USB events generated by the host controller, to the client.
 * 
 * @param hUsbh USB host controller DDK handler.
 * @param pTransfer Double pointer of type ::NvDdkUsbhTransfer. If event is
 *                 ::NvDdkUsbhEvent_TransferDone, DDK  returns the 
 *                 pointer to the completed transfer structure.
 * 
 * @retval Combination of the USB events at that point; refer to the ::NvDdkUsbhEvent.
 */
NvU32 NvDdkUsbhGetEvents(NvDdkUsbhHandle hUsbh, NvDdkUsbhTransfer **pTransfer);

/**
 * Suspends the host controller.
 * @param hUsbh The USB function module handle that was returned by NvDdkUsbhOpen().
 * 
 */
void NvDdkUsbhSuspend(NvDdkUsbhHandle hUsbh);

/**
 * Resumes  the host controller.
 * @param hUsbh The USB function module handle that was returned by NvDdkUsbhOpen().
 * 
 */
void NvDdkUsbhResume(NvDdkUsbhHandle hUsbh);


/**
 * Controls the busy hints for the host controller
 * @param hUsbh The USB function module handle that was returned by NvDdkUsbhOpen().
 * @param On NV_TRUE specifies to turn on the busy hints, or NV_FALSE to turn off.
 */
void NvDdkUsbhBusyHintsOnOff(NvDdkUsbhHandle hUsbh, NvBool On);


#if defined(__cplusplus)
}
#endif
/** @} */

#endif  /// INCLUDED_NVDDK_USBH_H


/*
 * Copyright (c) 2007-2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Driver Development Kit: USB Function Interface</b>
 *
 * @b Description: This file defines the interface to the USB 
 * function controller class.
 */

#ifndef INCLUDED_NVDDK_USBF_H
#define INCLUDED_NVDDK_USBF_H

#if defined(__cplusplus)
extern "C"
{
#endif


#include "nvcommon.h"
#include "nvrm_init.h"
#include "nvos.h"
#include "nvodm_query.h"

/**
 * @defgroup nvddk_usbf USB Function Interface
 * 
 * This file defines the interface to the USB 
 * function controller class.
 * 
 * @ingroup nvddk_modules
 * @{
 */

/**
 * @brief Opaque handle to a USB function
 * controller device.
 */
typedef struct NvDdkUsbFunctionRec* NvDdkUsbfHandle;


/** Contains attributes for a single USB endpoint. Used
 *  both to query and select endpoint configurations.
 */
typedef struct 
{
    /// The handle value used to access a USBF endpoint.
    NvU32 handle;   

    /// NV_TRUE if endpoint data transfer direction is IN (function to 
    /// host), NV_FALSE if endpoint data direction is OUT (host to function).
    NvBool isIn;

    /// NV_TRUE if endpoint is a control endpoint.
    NvBool isControl;

    /// NV_TRUE if endpoint is or can be a bulk endpoint.
    NvBool isBulk;

    /// NV_TRUE if endpoint is or can be an interrupt endpoint.
    NvBool isInterrupt;

    /// NV_TRUE if endpoint is or can be an isoc endpoint.
    NvBool isIsoc;

} NvDdkUsbfEndpoint;

typedef struct NvDdkUsbfCapabilitiesRec
{
    /// Number of endpoints supported.
    NvU32 EndPointCount;
}NvDdkUsbfCapabilities;


/**
 *  Defines USB event types used to pass
 *  out-of-band control and status between the module and its
 *  clients.
 * 
 *  USB endpoints are the mechanism used to pass information to
 *  and from this module by its clients. Typically, a client
 *  will perform transactions on control endpoints to manage the
 *  USBF connection and status. In some cases, there is
 *  out-of-band status information for which control packets are
 *  not defined in the USB specification. This enumeration
 *  defines control packet types that can be synthesized or
 *  received by the USBF module to manage these out-of-band
 *  operations and events.
 */
typedef enum
{
    /// A value that should never be used.
    NvDdkUsbfEvent_None = 0x00000000,

    /// Charger type 0, USB compliant charger, when D+ and D- are at low voltage
    NvDdkUsbfEvent_ChargerTypeSE0 = 0x00000001,

    /// Charger type 1, when D+ is high and D- is low.
    NvDdkUsbfEvent_ChargerTypeSJ = 0x00000002,

    /// Charger type 2, when D+ is low and D- is high.
    NvDdkUsbfEvent_ChargerTypeSK = 0x00000004,

    /// Charger type 3, when D+ and D- are at high voltage. 
    NvDdkUsbfEvent_ChargerTypeSE1 = 0x00000008,

    /// USB cable connected.
    NvDdkUsbfEvent_CableConnect = 0x00000010,

    /// USB setup packet detected.
    NvDdkUsbfEvent_SetupPacket = 0x000000020,

    /// USB bus has transitioned to reset state.
    NvDdkUsbfEvent_BusReset = 0x000000040,

    /// USB bus has transitioned to suspended state.
    NvDdkUsbfEvent_BusSuspend = 0x000000080,

    /// USB bus has transitioned out of suspend state.
    NvDdkUsbfEvent_BusResume = 0x000000100,

    /// USB vbus power detected.
    NvDdkUsbfEvent_BusPower = 0x000000200,

    /// Full-speed (12 Mbps) host detected.
    NvDdkUsbfEvent_PortFullSpeed = 0x000000400,

    /// High-speed (480 Mbps) host detected.
    NvDdkUsbfEvent_PortHighSpeed = 0x00000800,

    /// Top 32 bits indicate transfer completed on an endpoint.
    NvDdkUsbfEvent_EpTxfrCompleteBit = 32,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkUsbfEvent_Force32 = 0x7FFFFFFF,
} NvDdkUsbfEvents;


/**
 * This function opens the USB function controller module
 * and returns its handle to the caller.
 *
 * @param hRmDevice Handle to the Rm device, which is required by DDK to 
 * acquire the resources from RM.
 * 
 * @param hUsbf Where data specific to this instantiation of the
 * USBF module will be stored if the open is successful. This
 * handle is passed to all USBF APIs. This pointer is made NULL if this 
 * API fails.
 *
 * @param pMaxTransferSize Maximum possible buffer size for USB transfers.
 *
 * @param hUsbClientSema Semaphore used to signal on USB events such as 
 * transfer completion or transfer errors, setup packet arrival, or bus events, etc.
 * NvDdkUsbfGetEvents() returns the current events generated. 
 *
 * @param Instance Controller instance.
 * @retval NvSuccess On success.
 */
NvError
NvDdkUsbfOpen(
    NvRmDeviceHandle hRmDevice,
    NvDdkUsbfHandle* hUsbf,
    NvU32* pMaxTransferSize,
    NvOsSemaphoreHandle hUsbClientSema,
    NvU32 Instance);


/**
 * Closes the USBF module.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 *
 */
void NvDdkUsbfClose(NvDdkUsbfHandle hUsbf);


/**
 * Returns the endpoint information list that describes
 * the endpoints supported by a USB function module.
 *
 * @param hUsbf the USB function module handle that was returned
 * by NvDdkUsbfOpen().
 *
 * @param epCount Set to the number of endpoints in the endpoint
 * information list that is returned by this API.
 *
 * @returns A pointer to an array of \a epCount entries describing
 * each endpoint supported by the module and their attributes.
 * The first 2 entries of the array will always be the control
 * out, followed by the control in endpoints. The rest of the
 * non-control endpoints will be listed in no particular order.
 */
const NvDdkUsbfEndpoint* 
NvDdkUsbfGetEndpointList(
    NvDdkUsbfHandle hUsbf,
    NvU32* epCount);


/**
 * Activates or deactivates an endpoint configuration.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param Endpoint Endpoint that is to  be activated. 
 * 
 * @param Configure If NV_TRUE configure the specified endpoint,
 * if NV_FALSE then the endpoint is deactivated.
 * 
 * @return NvSuccess If all requested endpoints were
 * successfully activated in their requested
 * modes.
 */
NvError 
NvDdkUsbfConfigureEndpoint(
    NvDdkUsbfHandle hUsbf,
    const NvDdkUsbfEndpoint *Endpoint,
    NvBool Configure);


/**
 * Performs an endpoint transfer that
 * transmits data to the USB host on an IN endpoint. This
 * function blocks until the transfer completes, or an error occurs 
 * in transfer, or a timeout.
 *
 * @param hUsbf the USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param EpIn The endpoint on which the transfer will take
 * place.
 *
 * @param DataBuf The memory buffer from which data will be
 * read. The buffer belongs to the client, and the module will
 * attempt to transfer from it directly, but might need to copy
 * it to an internal buffer.
 * 
 * @param TxfrBytes The number of bytes to transfer from
 * \a DataBuf. All bytes will be transferred unless a timeout or
 * other failure occurs. Maximum data transfer in one call would be
 * maximum USB transfer size returned in the \c NvDdkUsbfOpen call.
 * 
 * @param EndTransfer Set to NV_TRUE, if this is the end of the
 * USB transfer and the USB module will finalize the transfer.
 * If there is more data to be sent for the current transfer,
 * set this to NV_FALSE.
 * 
 * @param WaitTimeoutMs Timeout value for this function to return after specific 
 * time or transaction completes. If passed ::NV_WAIT_INFINITE as argument, function
 * is blocked until transaction completes or there is any error in the transfer.
 * 
 * @returns Final status of transfer operation.
 */
NvError 
NvDdkUsbfTransmit(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint EpIn, 
    NvU8* DataBuf, 
    NvU32* TxfrBytes, 
    NvBool EndTransfer,
    NvU32 WaitTimeoutMs); 


/**
 * Performs an endpoint transfer that 
 * receives data from a USB host on an OUT endpoint. This
 * function can signal a semaphore or block until the transfer
 * completes.
 *
 * @param hUsbf the USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param EpOut The endpoint on which the transfer will take place.    
 *
 * @param DataBuf The physical memory buffer into which data
 * from the host will be stored. This buffer belongs to the
 * client, and the module will attempt to transfer to it
 * directly, but might need to copy to it from an internal
 * transfer buffer.
 * 
 * @param RcvdBytes On entry, contains the maximum number of
 * bytes that can be stored in \a DataBuf. On exit, contains the
 * number of bytes that were received from the host into
 * \a DataBuf. Maximum data transfer in one call would be maximum USB
 * transfer size returned in the \c NvDdkUsbfOpen call.
 * 
 * @param EnableShortPacketTermination If NV_TRUE, transfers that
 * are an even multiple of the USB packet length will be
 * terminated by a zero-length packet sent from the host.
 * 
 * @param WaitTimeoutMs Timeout value for this function to return after specific 
 * time or transaction completes. If passed ::NV_WAIT_INFINITE as argument, function
 * is blocked until transaction completes or there is any error in the transfer.
 * 
 * @returns Final status of transfer operation.
 */
NvError 
NvDdkUsbfReceive(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint EpOut, 
    NvU8* DataBuf, 
    NvU32 *RcvdBytes, 
    NvBool EnableShortPacketTermination,
    NvU32 WaitTimeoutMs); 


/**
 * An asyncronus call that initiates a 
 * transfer on a USB OUT endpoint. Data will be recieved by the USB 
 * internal memory buffers. Once data is recieved on the OUT endpoint, the semaphore 
 * passed in the NvDdkUsbfOpen() will be signaled to indicate the transfer is complete.
 * Calls to NvDdkUsbfGetEvents() returns the events generated.
 * If there is any event for this endpoint completion then 
 * calls to NvDdkUsbfReceive() will copy the data from the USB buffer to the client buffer.
 *
 * @param hUsbf The USB function module handle that was returned
 * by \c NvDdkUsbfOpen.
 * 
 * @param EpOut The out endpoint for which the transfer will be started.
 *
 * @param MaxTxfrBytes The maximum number of bytes to transfer.
 * For an OUT transfer (from host to function), the actual
 * number of bytes transferred may be less than or equal to this
 * value.
 * 
 * @param EnableShortPacketTermination If NV_TRUE, transfers that
 * are an even multiple of the USB packet length will be
 * terminated by a zero-length packet sent from the host.
 * 
 * @returns NvSuccess if endpoint transfer can be started, else
 * an error code.
 */
NvError
NvDdkUsbfReceiveStart(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint EpOut, 
    NvU32 MaxTxfrBytes, 
    NvBool EnableShortPacketTermination);


/**
 * An asyncronus call that initiates a 
 * transfer on a USB IN endpoint. Once data is transmited on the IN endpoint, 
 * semaphore passed in the NvDdkUsbfOpen() will be signaled to indicate the 
 * transfer is complete. Calls to NvDdkUsbfGetEvents() return the events generated.
 * Check the USB event status for this endpoint completion.
 *
 * @param hUsbf The USB function module handle that was returned
 * by \c NvDdkUsbfOpen.
 * 
 * @param EpIn The endpoint for which the transfer will be
 * started.
 *
 * @param maxTxfrBytes The maximum number of bytes to transfer.
 * For an OUT transfer (from host to function), the actual
 * number of bytes transferred may be less than or equal to this
 * value.
 * 
 * @param pBuffer The memory buffer from which data will be
 * read and passed to the USB internal buffer. 
 * 
 * @param EndTransfer Set to NV_TRUE, if this is the end of the
 * USB transfer and the USB module will finalize the transfer.
 * If there is more data to be sent for the current transfer,
 * set this to NV_FALSE.
 *
 * @returns NvSuccess if endpoint transfer can be started, else
 * an error code.
 */
NvError
NvDdkUsbfTransmitStart(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint EpIn, 
    NvU32 maxTxfrBytes, 
    NvU8 *pBuffer,
    NvBool EndTransfer);


/**
 * Waits for an endpoint transfer
 * to complete, or fail, or timeout.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param Ep The endpoint for which to wait for status.   
 * 
 * @param TimeoutMs The milliseconds to wait for endpoint activity
 * before returning.
 * 
 * @return current endpoint status.
 */
NvError
NvDdkUsbfTransferWait(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint Ep,
    NvU32 TimeoutMs);
/**
 * Cancels endpoint transfers for all the EP.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @returns NvSuccess if endpoint transfer is cancled, else
 * an error code.
 */

NvError 
NvDdkUsbfTransferCancelAll(
    NvDdkUsbfHandle hUsbf); 

/**
 * Cancels endpoint transfers and 
 * discards leftover buffered data.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param Ep The endpoint on which to cancel a transfer.
 *
 * @returns NvSuccess if endpoint transfer is cancled, else
 * an error code.
 */
NvError NvDdkUsbfTransferCancel(NvDdkUsbfHandle hUsbf, NvDdkUsbfEndpoint Ep); 


/**
 * Returns current status for an endpoint.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param Ep The endpoint for which to return status.    
 *
 * @return Endpoint status, or error status.
 */
NvError NvDdkUsbfEndpointStatus(NvDdkUsbfHandle hUsbf, NvDdkUsbfEndpoint Ep);


/**
 * Sets the USB stalled state for a particular endpoint. If endpoint has an
 * active transaction, this function will also clear that transaction.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * 
 * @param ep The endpoint for which to set the stalled state.
 *
 * @param StallEndpoint If NV_TRUE, endpoint is stalled. If NV_FALSE,
 * endpoint is unstalled.
 */
void 
NvDdkUsbfEpSetStalledState(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint ep, 
    NvBool StallEndpoint);

/**
 * Acknowledges endpoints as specified by client.
 * 
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 *
 * @param Endpoint The endpoint for which to send the Ack.
 * 
 * @retval NvSuccess if enpoint is Acked properly, else error status.
 */
NvError NvDdkUsbfAckEndpoint(NvDdkUsbfHandle hUsbf, NvDdkUsbfEndpoint Endpoint);


/**
 * Sets the USB device address in the controller.
 * 
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 *
 * @param Address endpoint address that needs to set in the USB controller.
 */
void NvDdkUsbfSetAddress(NvDdkUsbfHandle hUsbf, NvU32 Address);


/**
 * Starts the USB controller.
 * @pre This must be called 
 * after USB cable is detected. This functiona also must be called whenever
 * there is any USB bus reset event from the host.
 * 
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 *
 * @retval NvSuccess if USB function controllers starts ok, else an error.
 */
NvError NvDdkUsbfStart(NvDdkUsbfHandle hUsbf);


/**
 * Gets the combination of USB events that are 
 * generated by the USB bus to the client to handle accordingly in 
 * the protocol used for USB function.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 *
 * @retval Combination of the USB events at that point, 
 * see ::NvDdkUsbfEvents.
 */
NvU64 NvDdkUsbfGetEvents(NvDdkUsbfHandle hUsbf);

/**
 * Suspend mode entry function for USB driver. 
 * Powers down the controller to enter suspend state. 
 * The system is going down here and any error returned cannot be interpreted 
 * to stop the system from going down.
 *
 * @param hUsbf The USB handle that is allocated by the driver after calling
 * the NvDdkUsbfOpen() .
 * @param IsDpd NV_TRUE indicates suspend called when entering to deep power
 * down state. NV_FALSE indicates suspend called on cable removal.
 */
void NvDdkUsbfSuspend(NvDdkUsbfHandle hUsbf, NvBool IsDpd);

/**
 * Suspend mode exit function for USB driver. 
 * This resumes the controller from the suspend state. Here the system 
 * must come up quickly and we cannot afford to interpret the error and 
 * stop system from coming up.
 *
 * @param hUsbf The USB handle that is allocated by the driver after calling
 * the NvDdkUsbfOpen() .
 * @param IsDpd NV_TRUE indicates resume called when exiting from deep power 
 * down state. NV_FALSE indicates resume called on cable connection.
 *
 */
void NvDdkUsbfResume(NvDdkUsbfHandle hUsbf, NvBool IsDpd);

/**
 * Sets the USB charging limit current. 
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * @param ChargingCurrentMa The charging current limit in mA. 
 * @param ChargerType The type of the charger detected.
 */
void NvDdkUsbfSetPowerLimit(NvDdkUsbfHandle hUsbf, NvU32 ChargingCurrentMa, NvU32 ChargerType);
/**
 * Sets the USB test mode. 
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * @param TestModeSelector The test mode selector; the possible test supported 
 * by Usb2.0 specs are Test_J, Test_K, Test_SE0_NAK, Test_Packet and Test_Force_Enable. 
 */
void NvDdkUsbfSetTestMode(NvDdkUsbfHandle hUsbf, NvU32 TestModeSelector);

/**
 * Fills up the device capabilities.
 *
 * @param hUsbf The USB function module handle that was returned
 * by NvDdkUsbfOpen().
 * @param pUsbCap   A pointer to NvDdkUsbfCapabilities struct.
 * @retval NvSuccess                 Success
 */
NvError
NvDdkUsbfGetCapabilities(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfCapabilities *pUsbCap);

/** 
 * Returns the charger type, including the dummy charger.
 * 
 * This API is expected to be called from the thread context as it will sleep. 
 * This API should be called after the OS driver detects a cable connect. 
 * 
 * @return Charger type that is connected to a specific USB port.
 */
NvOdmUsbChargerType NvDdkUsbfDetectCharger(NvDdkUsbfHandle hUsbf);


/**
 * Controls the busy hints for the device controller.
 *
 * @param hUsbf The USB function module handle that was returned by NvDdkUsbfOpen().
 * @param On NV_TRUE specifies to turn on the busy hints, or NV_FALSE to turn off.
 * @param BoostDurationMs Requested boost duration in milliseconds.
 *        if BoostDurationMs = NV_WAIT_INFINITE, then busy hints will be on untill
 *        busy hints are off.
 */
void 
NvDdkUsbfBusyHintsOnOff(
    NvDdkUsbfHandle hUsbf, 
    NvBool On, 
    NvU32 BoostDurationMs);


/** @} */
#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVDDK_USBF_H


/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_usbf_int.h
 *
 * NvBootUsbf interface for NvBoot
 *
 * NvBootUsbf is NVIDIA's interface for Usb function driver. Supports only
 * Bulk end point transactions to the clients, accesing this driver.
 * All control endpoints are handled by the driver itself.
 * This makes driver simple for Boot Rom.
 */

#ifndef INCLUDED_NVBOOT_USBF_INT_H
#define INCLUDED_NVBOOT_USBF_INT_H


#include "nvcommon.h"
#include "t30/nvboot_error.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * Maximum data transfer allowed on Bulk IN/OUT endpoint
 * USB needs 4K bytes data buffer for data transfers on the USB
 */
enum {NVBOOT_USB_MAX_TXFR_SIZE_BYTES = 4096};

/*
 * USB buffers alignment is 4K Bytes and this must be allocated in IRAM
 */
enum {NVBOOT_USB_BUFFER_ALIGNMENT = 4096};


/*
 * NvBootUsbfEndpoint -- Defines the BulK endpoints that are supported.
 * Only Two Bulk endpoints, IN and OUT, are supported for Boot ROM.
 * All control endpoints are taken care by the driver.
 */
typedef enum
{
    /* Endpoint for recieving data from the Host */
    NvBootUsbfEndPoint_BulkOut,
    /* Endpoint for Transmitting data to the Host */
    NvBootUsbfEndPoint_BulkIn,
    NvBootUsbfEndPoint_ControlOut,
    NvBootUsbfEndPoint_ControlIn,
    NvBootUsbfEndPoint_Num,
    NvBootUsbfEndpoint_Force32 = 0x7fffffff

} NvBootUsbfEndpoint;

/*
 * NvBootUsbfEpStatus -- Defines Bulk Endpoint status
 */
typedef enum
{
    /* Indicates Transfer on the endpoint is complete */
    NvBootUsbfEpStatus_TxfrComplete,
    /* Indicates Transfer on the endpoint is Still Active */
    NvBootUsbfEpStatus_TxfrActive,
    /* Indicates Transfer on the endpoint is Failed */
    NvBootUsbfEpStatus_TxfrFail,
    /* Indicates endpoint is Idle and ready for starting data transfers*/
    NvBootUsbfEpStatus_TxfrIdle,
    /* Indicates endpoint is stalled */
    NvBootUsbfEpStatus_Stalled,
    /* Indicates endpoint is not configured yet */
    NvBootUsbfEpStatus_NotConfigured,
    NvBootUsbfEpStatus_Force32 = 0x7fffffff

} NvBootUsbfEpStatus;


/**
 * NvBootUsbfInit(): Initializes the USB driver context.
 * Any API of the USBfunction must be called only after Initialization.
 *
 *
 * This function sets the USB phy delay settings for stebilization of
 * B Session sensor to detect the valid cable connection.
 *
 * @param None
 *
 * @return None
 */
void NvBootUsbfInit(void);

/**
 * NvBootUsbfIsCableConnected(): Returns the cable connection status.
 *
 * @param None
 *
 * @return NV_TRUE if cable is connected else returns NV_FALSE.
 */
NvBool NvBootUsbfIsCableConnected(void);

/**
 * NvBootUsbfStartEnumeration(): Enables the controller and starts enumeration.
 * This API needs a buffer of 4K bytes, NVBOOT_USB_MAX_TXFR_SIZE_BYTES, size
 * for transfering the descriptor data on the USB bus during the enumeration.
 * This buffer must be aligned to 4K bytes and must be allocated in IRAM.
 *
 * This is blocking API, it will come out if there is any error or enumeration
 * is done successfully.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfInit()
 *
 * Generally the steps are --
 * 1.Checks the cable connection status before intializing the HW.
 * 2.Initializes the H/W (USB phy, USB controller)
 * 3.Handles the BUS events and setup packet on the control endpoint.
 * 4.Process the setup packet and does the device enumeration.
 * 5.Once enumeration is compelete, configures the Bulk endpoints and changes
 *   the UsbStatus to EnumerationComplete.
 * 6.If enumeration fails then USB status will be EnumerationFail
 * 7 if cable is removed any time during the enumeration process,
 *   Usb status will be NvBootUsbfStatus_CableNotConnected.
 *
 * @param pBuffer pointer to a buffer of 4K bytes, aligned to 4K bytes in IRAM.
 *
 * @return NvBootError_Success
 *          - if API executes Successfully and ready for bullk transfers.
 * @return NvBootError_HwTimeOut
 *          - if HW does not respond in time..
 * @return NvBootError_CableNotConnected
 *          - if cable is removed during the enumeration.
 */
NvBootError NvBootUsbfStartEnumeration(NvU8 *pBuffer);

/**
 * NvBootUsbfTransmit(): Performs an endpoint transfer which
 * transmits data to a USB host on an IN endpoint.
 * This API must be called only after enumeration is done.
 *
 * This is blocking API, blocks until there is an error or  complete
 * trasaction is done.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfInit()
 * 2.NvBootUsbfStartEnumeration()
 *
 * Generally the steps are --
 * Blocking mode transfer:
 *      NvBootUsbfTransmit(pDataBuf, TxfrSizeBytes,&BytesTxfered );
 *      if return Status is NvBootError_Success then transfer complete.
 *      else if return status is NvBootError_TxferFailed then transfer failed.
 *      else
 *      See other return codes for the error status.
 *
 * @param pDataBuf pointer to a memory buffer into which data to the host
 *        will be stored. This buffer must be 4K bytes in IRAM and must be
 *        aligned to 4K bytes.
 * @param TxfrSizeBytes Contains the maximum number (4K) of bytes that can be
 *        stored in pDatabuf.
 * @param pBytesTxfred Contains the number of bytes that were transfered
 *        to the host from pDataBuf.
 *
 * @return NvBootError_Success
 *          - if API executes Successfully.
 * @return NvBootError_TxferFailed
 *          - if transfer is failed.
 * @return NvBootError_HwTimeOut
 *          - if HW does not respond in time..
 */
NvBootError
NvBootUsbfTransmit(
    NvU8 *pDataBuf,
    NvU32 TxfrSizeBytes,
    NvU32 *pBytesTxfred);

/*
 * NvBootUsbfTransmitStart(): Performs an endpoint transfer start, which
 * transmits data to a USB host on an IN endpoint.
 * This API must be called only after enumeration is done.
 * This is non-blocking API, this will intiate transfer and comes out of the
 * API. Client must call NvBootUsbfQueryEpStatus() to know the transfer status
 * on the endpoint. Once endpoint status is complete then call
 * NvBootUsbfGetTransmitSize()for Number of bytes transmitted.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfInit()
 * 2.NvBootUsbfStartEnumeration()
 * 3.NvBootUsbfQueryEpStatus()
 * 4 NvBootUsbfGetBytesTransmitted()
 *
 * Generally the steps are --
 * Non-Blocking mode transfer:
 *      NvBootUsbfTransmitStart(pDataBuf, TxfrSizeBytes);
 *      USB transfer is initiated on endpoint.
 *      Get the EP status to find whether Txfer is success or not
 *      by calling NvBootUsbfHwEpGetStatus(endPoint);
 *      poll continuesly while (EpStatus == NvBootUsbfEpStatus_TxfrActive)
 *      if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
 *         data transfer is succesfull on the endpoint.
 *      else
 *          refer to the ep status codes.
 *
 * @param pDataBuf pointer to a memory buffer into which data
 *        to the host will be stored. This buffer must be 4K bytes in IRAM
 8        and must be aligned to 4K bytes.
 * @param TxfrSizeBytes Contains the maximum number of bytes
 *        that can be stored in pDatabuf.
 *
 * @return None
 */
void NvBootUsbfTransmitStart(NvU8 *pDataBuf, NvU32 TxfrSizeBytes);

/*
 * NvBootUsbfGetBytesTransmitted(): Gets the data transmited on the USB Bus.
 * This API must be called only after NvBootUsbfTransmitStart() is issued.
 * To know the correct status of the bytes transmited this must be called
 * after NvBootUsbfQueryEpStatus().when endpoint status is transfer complete.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfTransmitStart()
 * 2.NvBootUsbfQueryEpStatus()
 *
 * Generally the steps are --
 *      NvBootUsbfTransmitStart(EpIn, pDataBuf, TxfrSizeBytes);
 *      USB transfer is initiated on endpoint.
 *      Get the EP status to find wether Txfer is success or not
 *      by calling NvBootUsbfHwEpGetStatus(endPoint);
 *      poll continuesly while (EpStatus == NvBootUsbfEpStatus_TxfrActive)
 *      if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
 *         data transfer is succesfull on the endpoint.
 *         TransmitBytes = NvBootUsbfGetBytesTransmitted( )
 *      else
 *          refer to the ep status codes.
 *
 * @param None
 *
 * @return the actual bytes transfered on USB bus.
 */
NvU32 NvBootUsbfGetBytesTransmitted(void);

/**
 * NvBootUsbfReceive(): Performs an endpoint transfer which receives data
 * from a USB host on an OUT endpoint.
 * This API will be blocked for complete trasaction is done
 * This API must be called only after enumeration is done.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfInit()
 * 2.NvBootUsbfStartEnumeration()
 *
 * Generally the steps are --
 * Blocking mode transfer:
 *      NvBootUsbfReceive(pDataBuf, ReceiveSizeBytes, &BytesReceived, NV_TRUE);
 *      if return Status is NvBootError_Success then transfer complete.
 *      else if return status is NvBootError_TxferFailed then transfer failed.
 *      else
 *       See other return codes for the error status.
 *
 * @param pDataBuf pointer to a memory buffer into which data
 *        from the host will be stored. This buffer must be 4K bytes in IRAM
 8        and must be aligned to 4K bytes.
 * @param ReceiveSizeBytes Contains the maximum number of
 *        bytes that can be stored in pDatabuf.
 * @param pBytesReceived Contains the number of bytes that were received
 *        from the host into pDataBuf.
 *
 * @return NvBootError_Success
 *          - if API executes Successfully.
 * @return NvBootError_TxferFailed
 *          - if transfer is failed.
 * @return NvBootError_HwTimeOut
 *          - if HW does not respond in time..
 */
NvBootError
NvBootUsbfReceive(
    NvU8 *pDataBuf,
    NvU32 ReceiveSizeBytes,
    NvU32 *pBytesReceived);

/**
 * NvBootUsbfReceiveStart(): Performs an endpoint transfer which
 * receives data from a USB host on an OUT endpoint.
 * This API is non blocking, intiates the transfer and comes out of the API.
 * Client must call NvBootUsbfQueryEpStatus() to know the
 * transfer status on the endpoint. If transfer is success on the endpoint,
 * then NvBootUsbfGetBytesReceived() must be called for knowing the number of
 * bytes recieved on the USB bus.
 * This API must be called only after enumeration is done.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfInit()
 * 2.NvBootUsbfStartEnumeration()
 * 3.NvBootUsbfQueryEpStatus()
 * 4 NvBootUsbfGetBytesReceived()
 *
 * Generally the steps are --
 * Non-Blocking mode transfer:
 *      NvBootUsbfReceive(pDataBuf, ReceiveSizeBytes);
 *      initiates the recive on endpoint.
 *      Get once the EP status to find wether Txfer is success or not by
 *      calling NvBootUsbfHwEpGetStatus(endPoint);
 *      poll continuesly while (EpStatus == NvBootUsbfEpStatus_TxfrActive)
 *      if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
 *          data recieve is succesfull on the endpoint.
 *          call NvBootUsbfGetBytesReceived();
 *           to know the exact number of bytes recieved.
 *      else
 *           refer to the ep status codes.
 *
 * @param pDataBuf pointer to a memory buffer into which data
 *        from the host will be stored. This buffer must be 4K bytes in IRAM
 8        and must be aligned to 4K bytes.
 * @param ReceiveSizeBytes Contains the maximum number of
 *        bytes that can be stored in pDatabuf.
 *
 * @return None
 */
void NvBootUsbfReceiveStart( NvU8 *pDataBuf, NvU32 ReceiveSizeBytes);

/**
 * NvBootUsbfReceiveData(): Copies data from USB buffer to the client buffer.
 * This API must be called only after recive is intiated and transfer on
 * the OUT endpoint is success. To know the exact bytes recieved this API
 * must be called after NvBootUsbfQueryEpStatus() changes from active
 * to other state.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfQueryEpStatus()
 * 2 NvBootUsbfReceiveStart()
 *
 * Generally the steps are --
 *      NvBootUsbfReceive(pDataBuf, ReceiveSizeBytes);
 *      initiates the recive on endpoint.
 *      Get once the EP status to find wether Txfer is success or not by
 *      calling NvBootUsbfHwEpGetStatus(endPoint);
 *      poll continuesly while (EpStatus == NvBootUsbfEpStatus_TxfrActive)
 *      if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
 *          data recieve is succesfull on the endpoint.
 *          call NvBootUsbfGetBytesReceived();
 *           to know the exact number of bytes recieved.
 *      else
 *           refer to the ep status codes.
 *
 * @param None.
 *
 * @return Number of bytes that are arecived on USB bus
 */
NvU32 NvBootUsbfGetBytesReceived(void);

/**
 * NvBootUsbfQueryEpStatus(): Queries the endpoint status.
 *
 * Also refer other APIs:
 * 1.NvBootUsbfInit()
 * 2.NvBootUsbfTransmit()
 * 3.NvBootUsbfReceive()
 *
 * Also refer to the NvBootUsbfEpStatus: for USB endpoint status.
 *
 * @param EndPoint The endpoint for which to resturn the status.
 *
 * @return the curent status of the endpoint see @NvBootUsbfEpStatus
 */
NvBootUsbfEpStatus NvBootUsbfQueryEpStatus(NvBootUsbfEndpoint EndPoint);

/**
 * NvBootUsbfTransferCancel(): Cancels endpoint transfers and discards
 * leftover buffered data.
 *
 * @param Endpoint The endpoint on which to cancel a transfer.
 *
 * @return None
 */
void NvBootUsbfTransferCancel(NvBootUsbfEndpoint Endpoint);

/**
 * NvBootUsbfEpSetStalledState(): This is used to set the USB stalled
 * state for a particular endpoint. If endpoint has an active transaction,
 * this function will also clear that transaction and release any outstanding
 * endpoint buffer memory.
 *
 * @param EndPoint The endpoint for which to set the stalled state.
 * @param StallState If NV_TRUE, endpoint will be stalled. If NV_FALSE,
 *        endpoint will be unstalled.
 *
 * @return None
 */
void NvBootUsbfEpSetStalledState(NvBootUsbfEndpoint EndPoint, NvBool StallState);

/**
 * NvBootUsbfHandlePendingControlTransfers(): This function is used to handle
 * pending control tranfers.
 *
 * @param pBuffer pointer to a buffer of 4K bytes, aligned to 4K bytes in IRAM.
 *
 * @return NvBootError_Success
 *          - if API executes Successfully and ready for bullk transfers.
 * @return NvBootError_HwTimeOut
 *          - if HW does not respond in time..
 * @return NvBootError_CableNotConnected
 *          - if cable is removed during the enumeration.
 */
NvBootError NvBootUsbfHandlePendingControlTransfers(NvU8 *pBuffer);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_USBF_INT_H

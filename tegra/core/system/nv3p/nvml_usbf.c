/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nv3p.h"
#include "nv3p_bytestream.h"
#include "nv3p_private.h"
#include "nv3p_transport.h"
#include "nv3p_transport_usb_descriptors.h"
#include "iram_address_allocation.h"

/** Nv3p Transport wrapper follows */
typedef struct Nv3pTransportRec
{
    NvU8 *buffer;
    NvU32 size;
    /* receive buffering */
    NvU8 *read;
    NvU32 remaining;
    NvU8 *ep0Buffer;
} Nv3pTransport;

/* Definitions of macros required by nvml*/
#define USB_HW_QUEUE_HEAD_SIZE_BYTES    48
/* Can use this if the length of the transfer is not known */
#define NV3P_UNKNOWN_LENGTH 0xFFFFFFFF

#define   MASK_ALL_PULLUP_PULLDOWN (0xff << 8)
#define   DISABLE_PULLUP_DP (1 << 15)
#define   DISABLE_PULLUP_DM (1 << 14)
#define   DISABLE_PULLDN_DP (1 << 13)
#define   DISABLE_PULLDN_DM (1 << 12)
#define   FORCE_PULLUP_DP   (1 << 11)
#define   FORCE_PULLUP_DM   (1 << 10)
#define   FORCE_PULLDN_DP   (1 << 9)
#define   FORCE_PULLDN_DM   (1 << 8)
#define   MASK_ALL_PULLUP_PULLDOWN (0xff << 8)
#define   USB_PORTSC_LINE_STATE(x) (((x) & (0x3 << 10)) >> 10)
#define   UTIMIP_OP_I_SRC_EN   (1 << 5)
#define   USB_PORTSC_LINE_DM_SET   (1 << 0)
#define   USB_PORTSC_LINE_DP_SET   (1 << 1)

/* Golbal variables*/
static Nv3pTransport s_Transport;
static NvOdmUsbChargerType s_ChargerType;
static NvBool s_ChargingMode = NV_FALSE;

/* Other speed configuration descriptor */

NV_ALIGN(4) static NvU8 s_OtherSpeedConfigDesc[32] =
{

    /** Other speed Configuration Descriptor */
    0x09,   /// bLength - Size of this descriptor in bytes
    0x07,   /// bDescriptorType - Other speed Configuration Descriptor Type
    0x20,   /// WTotalLength (LSB) - Total length of data for this configuration
    0x00,   /// WTotalLength (MSB) - Total length of data for this configuration
    0x01,   /// bNumInterface - Nos of Interface supported by this configuration
    0x01,   /// bConfigurationValue
    0x00,   /// iConfiguration - Index of String descriptor describing this configuration
    0xc0,   /// bmAttributes - Config Characteristcs bitmap "D4-D0: Res, D6: Self Powered,D5: Remote Wakeup
    0x10,   /// MaxPower in mA - Max Power Consumption of the USB device

    /**Interface Descriptor */
    0x09,   /// bLength - Size of this descriptor in bytes
    0x04,   /// bDescriptorType - Interface Descriptor Type
    0x00,   /// bInterfaceNumber - Number of Interface
    0x00,   /// bAlternateSetting - Value used to select alternate setting
    0x02,   /// bNumEndpoints - Nos of Endpoints used by this Interface
    0x08,   /// bInterfaceClass - Class code "MASS STORAGE Class."
    0x06,   /// bInterfaceSubClass - Subclass code "SCSI transparent command set"
    0x50,   /// bInterfaceProtocol - Protocol code "BULK-ONLY TRANSPORT."
    0x00,   /// iInterface - Index of String descriptor describing Interface

    /** Endpoint Descriptor IN EP2 */
    0x07,   /// bLength - Size of this descriptor in bytes
    0x05,   /// bDescriptorType - ENDPOINT Descriptor Type
    BULK_IN,   /// bEndpointAddress - The address of EP on the USB device " Bit 7: Direction(0:OUT, 1: IN),Bit 6-4:Res,Bit 3-0:EP no"
    0x02,   /// bmAttributes - Bit 1-0: Transfer Type 00:Control,01:Isochronous,10: Bulk, 11: Interrupt
    0x40,   /// wMaxPacketSize(LSB) - Maximum Packet Size for this EP
    0x00,   /// wMaxPacketSize(MSB) - Maximum Packet Size for this EP
    0x00,   /// bIntervel - Interval for polling EP, applicable for Interrupt and Isochronous data transfer only

    /** Endpoint Descriptor OUT EP1 */
    0x07,   /// bLength - Size of this descriptor in bytes
    0x05,   /// bDescriptorType - ENDPOINT Descriptor Type
    BULK_OUT,   /// bEndpointAddress - The address of EP on the USB device "Bit 7: Direction(0:OUT, 1: IN),Bit 6-4:Res,Bit 3-0:EP no"
    0x02,   /// bmAttributes - Bit 1-0: Transfer Type 00:Control,01:Isochronous,10: Bulk, 11: Interrupt
    0x40,   /// wMaxPacketSize(LSB) - Maximum Packet Size for this EP
    0x00,   /// wMaxPacketSize(MSB) - Maximum Packet Size for this EP
    0x00    /// bIntervel - Interval for polling EP, applicable for Interrupt and Isochronous data transfer only
};

/**
 * Defines USB Test mode Selectors
 * As per USB2.0 Specification.
 */
typedef enum
{
    TEST_MODE_SELECTOR_RESERVED = 0,
    // Specifies  Test_J
    TEST_MODE_SELECTOR_TEST_J,
    // Specifies  Test_K
    TEST_MODE_SELECTOR_TEST_K,
    // Specifies  Test_SE0_NAK
    TEST_MODE_SELECTOR_TEST_SE0_NAK,
    // Specifies  Test_PACKET
    TEST_MODE_SELECTOR_TEST_PACKET,
    // Specifies  Test_FORCE_ENABLE
    TEST_MODE_SELECTOR_TEST_FORCE_ENABLE
} TestModeSelectors;

///Signifies the length of the test packet in the TEST_MODE of USB
#define USB_TEST_PKT_LENGTH     53

//Define the test packet for the test mode
static const NvU8 s_TestPacket[USB_TEST_PKT_LENGTH] =
{
    // Prepare the test packet as specified by USB 2.0 spec
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0xAA,0xAA,0xAA,
    0xAA,0xAA,0xAA,0xAA,
    0xAA,0xEE,0xEE,0xEE,
    0xEE,0xEE,0xEE,0xEE,
    0xEE,0xFE,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,
    0xFF,0x7F,0xBF,0xDF,
    0xEF,0xF7,0xFB,0xFD,
    0xFC,0x7E,0xBF,0xDF,
    0xEF,0xF7,0xFB,0xFD,
    0x7E
};

/* Function forward declarations*/
static void NVML_FUN_SOC(Nv3pTransportPrivInit,( void ));
static void NVML_FUN_SOC(NvMlPrivInit,(void));
static NvBootError NVML_FUN_SOC(NvBootUsbHandleControlRequest,( void* arg));
static void NVML_FUN_SOC(NvMlUsbfDetectCharger, (void));
void NVML_FUN_SOC(NvMlUsbfSetVbusOverride,(NvBool Val));

static NvBootUsbfEpStatus
NVML_FUN_SOC(
    NvMlPrivQueryEpStatus,
    (NvBootUsbfEndpoint EndPoint));
static void
NVML_FUN_SOC(
    NvMlPrivEpSetStalledState,
    (NvBootUsbfEndpoint Endpoint,
    NvBool StallState));
static void NVML_FUN_SOC(NvMlPrivTransmitStart,(NvU8 *pDataBuf, NvU32 TxfrSizeBytes));
static NvU32 NVML_FUN_SOC(NvMlPrivGetBytesTransmitted,(void));
static void NVML_FUN_SOC(NvMlPrivReceiveStart,(NvU8 *pDataBuf, NvU32 DataSize));
static NvBootError NVML_FUN_SOC(NvMlPrivProcessSetupPacket,(NvU8 *pXferBuffer));
static NvU32 NVML_FUN_SOC(NvMlPrivGetBytesReceived,(void));
static NvBootError
NVML_FUN_SOC(
    NvMlPrivProcessHost2DevDevicePacket,
    (NvBool *pIsEndPointStall));
static void
NVML_FUN_SOC(
    GetStandardConfigIfc,
    (NvU32 *pIfcClass,
    NvU32 *pIfcSubclass,
    NvU32 *pIfcProtocol));
static NvU8 *NVML_FUN_SOC(GetStandardProductId,(void));
static void
NVML_FUN_SOC(
    NvMlPrivProcessDeviceDescriptor,(
    NvBool *pTransmitData,
    NvU8 **pDataBuffer,
    NvU32 *pDataSize,
    NvBool *pIsEndPointStall));
static NvBootError
NVML_FUN_SOC(NvMlPrivHwEnableController,(void));
static NvBootError
NVML_FUN_SOC(
    NvMlPrivHwIntializeController,
    (NvBootUsbfContext *pUsbFuncCtxt));
static NvU32
NVML_FUN_SOC(
    NvMlPrivHwGetPacketSize,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint));
static NvBootError
NVML_FUN_SOC(
    NvMlPrivHwTxfrStart,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU8 *pDataBuf,
    NvU32 maxTxfrBytes,
    NvBool WaitForTxfrComplete));
static NvBootError
NVML_FUN_SOC(
    NvMlPrivHwTxfrWait,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 timeoutMs));
static void
NVML_FUN_SOC(
    NvMlPrivHwInitalizeEndpoints,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint));
static NvU32 NVML_FUN_SOC(NvMlPrivHwGetEvent,(void));
static NvBootError NVML_FUN_SOC(NvMlPrivHwEndPointFlush,(NvU32 EndPoint));
static void
NVML_FUN_SOC(
    NvMlPrivHwTxfrClear,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint));
static NvBootUsbfEpStatus
NVML_FUN_SOC(
    NvMlPrivHwEpGetStatus,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint));
static void NVML_FUN_SOC(NvBootConfigureDevDescriptor,(NvU8 *pDevDescriptor));
static void NVML_FUN_SOC(NvMlPrivReinit,(void));
static NvBool NVML_FUN_SOC(NvMlPrivDetectNvCharger,(void));
void NVML_FUN_SOC(NvMlUsbfSetBSessSwOverride,(void));

extern NvU32 Nv3pPrivGetChipId(void);

/* Function Definitions */
void NVML_FUN_SOC(Nv3pTransportSetChargingMode,( NvBool ChargingMode ))
{
    s_ChargingMode = ChargingMode;
}

void NVML_FUN_SOC(Nv3pTransportPrivInit,( void ))
{
    /* the usb descriptor must be 2048 byte aligned, setup a buffer in IRAM,
     * which is always uncached.
     */
    s_pUsbDescriptorBuf = (NvBootUsbDescriptorData *)(
        NV_ADDRESS_MAP_DATAMEM_BASE +
        NV3P_USB_BUFFER_OFFSET
    );
    return;
}

void NVML_FUN_SOC(NvMlPrivInit,( void ))
{
    NVML_FUN_SOC(Nv3pTransportPrivInit,());
    NVML_FUN_SOC(NvMlPrivUsbInit,());
    // Store the context for future use.
    s_pUsbfCtxt = &s_UsbfContext;

    // Assign the Hardware Q head address
    s_pUsbDescriptorBuf->pQueueHead = (NvBootUsbDevQueueHead *)
                                    (NV_ADDRESS_MAP_USB_BASE +
                                    USB2_QH_USB2D_QH_EP_0_OUT_0);

    // Set controller enabled to NV FLASE untill we start the controller.
    s_pUsbfCtxt->UsbControllerEnabled = NV_FALSE;
    NVML_FUN_SOC(NvMlPrivInitBaseAddress,(s_pUsbfCtxt));
}


NvBootError NVML_FUN_SOC(NvBootUsbHandleControlRequest,( void* arg ))
{
    NvBootError BootError = NvBootError_Success;
    NvU32 EpSetupStatus = 0;
    NvU32 UsbfEvents = 0;
    NvU8 *pXferBuffer = arg;
    NvU32 currentTime = NvOsGetTimeMS();

    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(pXferBuffer);

    // Address must be 4K bytes aligned
    NV_ASSERT(!(PTR_TO_ADDR(pXferBuffer)&(NVBOOT_USB_BUFFER_ALIGNMENT-1)));

    do
    {
        // Get the USB events from the H/W
        UsbfEvents = NVML_FUN_SOC(NvMlPrivHwGetEvent,());

        // Look for the cable connection status
        if (!NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
        {
            s_pUsbfCtxt->EnumerationDone = NV_FALSE;
            return NvBootError_CableNotConnected;
        }

        // Check if there is any BUS reset
        if (UsbfEvents & USB_DRF_DEF(USBSTS, URI, USB_RESET))
        {
            // Clear the HW status registers on reset
            // Clear device address by writing the reset value
            USB_REG_WR(PERIODICLISTBASE, USB_DEF_RESET_VAL(PERIODICLISTBASE));
            // Clear setup token, by reading and wrting back the same value
            USB_REG_WR(ENDPTSETUPSTAT, USB_REG_RD(ENDPTSETUPSTAT));
            // Clear endpoint complete status bits.by reading and writing back
            USB_REG_WR(ENDPTCOMPLETE, USB_REG_RD(ENDPTCOMPLETE));
            // Flush the endpoints
            BootError = NVML_FUN_SOC(NvMlPrivHwEndPointFlush,(USB_FLUSH_ALL_EPS));

            /* Clear buld endpoint sw state */
            NVML_FUN_SOC(NvMlPrivHwTxfrClear,(s_pUsbfCtxt, USB_EP_BULK_OUT));
            NVML_FUN_SOC(NvMlPrivHwTxfrClear,(s_pUsbfCtxt, USB_EP_BULK_IN));

            s_pUsbfCtxt->EnumerationDone = NV_FALSE;
        }

        // Look for port change status
        if (UsbfEvents & USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE))
        {
            // If port change detected then get the current speed of operation
            // get the USB port speed for transmiting the packet size
           NVML_FUN_SOC(NvMlPrivGetPortSpeed,(s_pUsbfCtxt));
        }

        // Get the USB Setup packet status
        EpSetupStatus = USB_REG_RD(ENDPTSETUPSTAT);

        // Check for setup packet arrival. Store it and clear the setup
        // lockout.
        if (EpSetupStatus & USB_DRF_DEF(ENDPTSETUPSTAT,
                                        ENDPTSETUPSTAT0, SETUP_RCVD))
        {
            // Write back the same value to clear the register
            USB_REG_WR(ENDPTSETUPSTAT,EpSetupStatus);

            // Read the setup packet from the Control OUT endpoint Queue head
            NvOsMemcpy(&s_pUsbfCtxt->setupPkt[0],
                (void *)&s_pUsbDescriptorBuf->pQueueHead
                    [USB_EP_CTRL_OUT].setupBuffer0,
                USB_SETUP_PKT_SIZE);

            // Process the setup packet
            BootError = NVML_FUN_SOC(NvMlPrivProcessSetupPacket,(pXferBuffer));
            // Ignore error as all the setup packets are not handled.
            // NV_ASSERT(BootError != NvBootError_Success)
        }

    } while ((!s_pUsbfCtxt->EnumerationDone) && ((NvOsGetTimeMS() - currentTime) < 1000));

    if (s_ChargerType == NvOdmUsbChargerType_NonCompliant)
    {
        if (s_pUsbfCtxt->EnumerationDone)
        {
            s_ChargerType = NvOdmUsbChargerType_UsbHost;
            NvOsDebugPrintf("Connected to PC HOST\n");
        }
        else
        {
            s_ChargerType = NvOdmUsbChargerType_NonCompliant;
            NvOsDebugPrintf("Connected to non-compliant charger \n");
        }
    }

    return BootError;
}

NvBootUsbfEpStatus
    NVML_FUN_SOC(
    NvMlPrivQueryEpStatus,
    (NvBootUsbfEndpoint EndPoint))
{
    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(s_pUsbfCtxt->UsbControllerEnabled);
    NV_ASSERT(EndPoint < NvBootUsbfEndPoint_Num);

    // Get the cable connection status
    if (!NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        // If cable is disconnected in between USB Transfers clear the pending
        // transfers and then return the EP status
        if (s_pUsbfCtxt->UsbControllerEnabled)
        {
            // Clear leftover transfer, if any.
            NVML_FUN_SOC(NvMlPrivHwTxfrClear,(s_pUsbfCtxt,
                                  (EndPoint == NvBootUsbfEndPoint_BulkIn) ?
                                  USB_EP_BULK_IN : USB_EP_BULK_OUT));
        }
        // return transfer fail on EP if there is no cable.
        return NvBootUsbfEpStatus_TxfrFail;
    }

    // Get the endpoint status from the hardware
    return NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt,
                                (EndPoint == NvBootUsbfEndPoint_BulkIn) ?
                                USB_EP_BULK_IN : USB_EP_BULK_OUT));

}

void
NVML_FUN_SOC(
    NvMlPrivEpSetStalledState,
    (NvBootUsbfEndpoint Endpoint,
    NvBool StallState))
{
    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(s_pUsbfCtxt->UsbControllerEnabled);
    NV_ASSERT(Endpoint < NvBootUsbfEndPoint_Num);

    // RX/TX Endpoint Stall: Read/Write
    // stallState = 0 = End Point OK
    // stallState = 1 = End Point Stalled
    if (Endpoint == NvBootUsbfEndPoint_BulkIn)
    {
        USB_REG_UPDATE_NUM(ENDPTCTRL1, TXS, StallState);
        if (!StallState)
            USB_REG_UPDATE_DEF(ENDPTCTRL1, TXR, RESET_PID_SEQ);
    }
    else
    {
        USB_REG_UPDATE_NUM(ENDPTCTRL1, RXS, StallState);
        if (!StallState)
            USB_REG_UPDATE_DEF(ENDPTCTRL1, RXR, RESET_PID_SEQ);
    }
}

void NVML_FUN_SOC(NvMlPrivTransmitStart,(NvU8 *pDataBuf, NvU32 TxfrSizeBytes))
{
    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(s_pUsbfCtxt->UsbControllerEnabled);
    NV_ASSERT(pDataBuf);
    // Address must be 4K bytes aligned
    NV_ASSERT(!(PTR_TO_ADDR(pDataBuf)&(NVBOOT_USB_BUFFER_ALIGNMENT-1)));

    // Check resquest size is more than the USB buffer size
    if (TxfrSizeBytes > NVBOOT_USB_MAX_TXFR_SIZE_BYTES)
    {
        TxfrSizeBytes = NVBOOT_USB_MAX_TXFR_SIZE_BYTES;
    }

    // Initiate the transfer and come out don't wait for transfer complete.
    NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_BULK_IN,
                          pDataBuf, TxfrSizeBytes, NV_FALSE));

}

NvU32 NVML_FUN_SOC(NvMlPrivGetBytesTransmitted,(void))
{
    NvBootUsbfEpStatus EpStatus;
    NvBootUsbDevQueueHead *pQueueHead = NULL;

    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(s_pUsbfCtxt->UsbControllerEnabled);

    // Get once the EP status, to find wether Txfer is success or not
    EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt, USB_EP_BULK_IN));

    // Update bytes recieved only if transfer is complete
    if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
    {
        // Temporary pointer to queue head.
        pQueueHead = &s_pUsbDescriptorBuf->pQueueHead[USB_EP_BULK_IN];
        // Get the actual bytes transfered by USB controller
        return (s_pUsbDescriptorBuf->BytesRequestedForEp[USB_EP_BULK_IN] -
                USB_DQH_DRF_VAL(TOTAL_BYTES, pQueueHead->DtdToken));
    }

    // return 0 if transfer is not happend
    return 0;
}

void NVML_FUN_SOC(NvMlPrivReceiveStart,(NvU8 *pDataBuf, NvU32 DataSize))
{
    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(s_pUsbfCtxt->UsbControllerEnabled);
    NV_ASSERT(pDataBuf);
    // Address must be 4K bytes aligned
    NV_ASSERT(!(PTR_TO_ADDR(pDataBuf)&(NVBOOT_USB_BUFFER_ALIGNMENT-1)));

    // Check resquest size is more than the USB buffer size
    if (DataSize > NVBOOT_USB_MAX_TXFR_SIZE_BYTES)
    {
        DataSize = NVBOOT_USB_MAX_TXFR_SIZE_BYTES;
    }

    // Initiate the recieve operation and come out
    NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_BULK_OUT,
                          pDataBuf, DataSize, NV_FALSE));
}

NvU32 NVML_FUN_SOC(NvMlPrivGetBytesReceived,(void))
{
    NvBootUsbfEpStatus EpStatus;
    NvBootUsbDevQueueHead *pQueueHead = NULL;

    // Validating the parameters
    NV_ASSERT(s_pUsbfCtxt);
    NV_ASSERT(s_pUsbfCtxt->UsbControllerEnabled);

    // Get once the EP status, to find wether Txfer is success or not
    EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt, USB_EP_BULK_OUT));

    // Update the buffer and bytes recived only if transfer is completed
    if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
    {
        // Temporary pointer to queue head.
        pQueueHead = &s_pUsbDescriptorBuf->pQueueHead[USB_EP_BULK_OUT];
        // Get the actual bytes transfered by USB controller
        return (s_pUsbDescriptorBuf->BytesRequestedForEp[USB_EP_BULK_OUT] -
                USB_DQH_DRF_VAL(TOTAL_BYTES, pQueueHead->DtdToken));
    }

    // return 0 if transfer is not happend
    return 0;
}

NvBootError NVML_FUN_SOC(NvMlPrivProcessSetupPacket,(NvU8 *pXferBuffer))
{
    NvBootUsbfEpStatus EpStatus = NvBootUsbfEpStatus_Stalled;
    NvBootError ErrStatus = NvBootError_Success;
    NvBool transmitData = NV_FALSE;
    NvU8 *pDataBuffer = NULL;
    NvU32 DataSize = 0;
    NvU32 SetupPktLength = 0, EndpointAddr = 0;
    NvBool AckEp = NV_FALSE;

    //GetStatus() Request to an Interface is always 0
    NvU8 InterfaceStatus[2] = {0, 0};
    NvU8 EndpointStatus[2] = {0, 0}; // GetStatus() Request to an Interface is always 0

    // Set default to Not to stall controll endpoint
    NvBool IsEndPointStall = NV_FALSE;

    // This field specifies the length of the data transferred during the
    // second phase of the control transfer. (2 bytes in the setup packet)
    SetupPktLength = (s_pUsbfCtxt->setupPkt[USB_SETUP_LENGTH]) |
                     (s_pUsbfCtxt->setupPkt[USB_SETUP_LENGTH + 1] << 8);

    // switch to the request direction
    switch (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST_TYPE])
    {
        case HOST2DEV_DEVICE:
            // Process the Host -> device Device descriptor
            ErrStatus =
                NVML_FUN_SOC(NvMlPrivProcessHost2DevDevicePacket,(&IsEndPointStall));
            break;

        case HOST2DEV_INTERFACE:
            // Start the endpoint for zero packet acknowledgment
            /* Flush the bulk endpoints and re-configure them. Any active
             * trasnfers will fail and the client code should retry.
             */

            s_pUsbDescriptorBuf->EpConfigured[USB_EP_BULK_OUT] = NV_FALSE;
            s_pUsbDescriptorBuf->EpConfigured[USB_EP_BULK_IN] = NV_FALSE;

            NVML_FUN_SOC(NvMlPrivHwEndPointFlush,(USB_EP_BULK_OUT));
            NVML_FUN_SOC(NvMlPrivHwEndPointFlush,(USB_EP_BULK_IN));

            USB_REG_UPDATE_DEF(ENDPTCTRL1, RXR, RESET_PID_SEQ);
            USB_REG_UPDATE_DEF(ENDPTCTRL1, TXR, RESET_PID_SEQ);

            ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                                              NULL, 0, NV_TRUE));
            if (ErrStatus == NvBootError_Success)
            {
                // Set the Interface number sent by the host
                s_pUsbfCtxt->UsbInterfaceNo =
                    s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE];
            }
            break;

        case DEV2HOST_DEVICE:
            switch(s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST])
            {
                case GET_STATUS:
                    transmitData = NV_TRUE;
                    pDataBuffer = &s_UsbDeviceStatus[0];
                    DataSize = USB_DEV_STATUS_LENGTH;
                    break;
                case GET_CONFIGURATION:
                    transmitData = NV_TRUE;
                    pDataBuffer = &s_pUsbfCtxt->UsbConfigurationNo;
                    DataSize = SetupPktLength;
                    break;
                case GET_DESCRIPTOR:
                    // Get Descriptor Request
                    NVML_FUN_SOC(NvMlPrivProcessDeviceDescriptor,
                                                     (&transmitData,
                                                      &pDataBuffer,
                                                      &DataSize,
                                                      &IsEndPointStall));
                    break;
                default:
                    // Stall if any Un supported request comes
                    IsEndPointStall = NV_TRUE;
                    break;
            }
            break;

        case DEV2HOST_INTERFACE:
            switch (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST])
            {
                case GET_STATUS: // Get Status/Interface Request
                    transmitData = NV_TRUE;
                    pDataBuffer = &InterfaceStatus[0];
                    DataSize = SetupPktLength;
                    break;
                case GET_INTERFACE: // Get Interface Request
                    transmitData = NV_TRUE;
                    pDataBuffer = &s_pUsbfCtxt->UsbInterfaceNo;
                    DataSize = SetupPktLength;
                    break;
                default:
                    // Stall if any Un supported request comes
                    IsEndPointStall = NV_TRUE;
                    break;
            }
            break;
        case DEV2HOST_ENDPOINT:
            switch (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST])
            {
                case GET_STATUS: // Get Status/Interface Request
                    transmitData = NV_TRUE;
                    // Get the Ep number from 'wIndex' field and  get the
                    // Halt status and update the data buffer. Temporarily
                    // sending 0 ( not haltted for all ep).
                    // Read the Ep status, If ep is STALLED, return 1 other
                    // wise 0
                    // Get once the EP status, to find wether Txfer is success or not
                    EndpointAddr = ((s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX]) |
                                     (s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX + 1] << 8));

                    switch(EndpointAddr)
                    {
                        case CTRL_OUT:
                            EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt, USB_EP_CTRL_OUT));
                            break;
                        case CTRL_IN:
                            EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt, USB_EP_CTRL_IN));
                            break;
                        case BULK_OUT:
                            EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt, USB_EP_BULK_OUT));
                            break;
                        case BULK_IN:
                            EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(s_pUsbfCtxt, USB_EP_BULK_IN));
                            break;
                        default:
                            transmitData = NV_FALSE;
                            NVML_FUN_SOC(NvMlPrivEpSetStalledState,( NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                            break;
                    }
                    if (EpStatus == NvBootUsbfEpStatus_Stalled)
                        EndpointStatus[0] = 1;
                    else
                        EndpointStatus[0] = 0;
                    pDataBuffer = &EndpointStatus[0];
                    DataSize = SetupPktLength;
                    break;
                default:
                    transmitData = NV_FALSE;
                    NVML_FUN_SOC(NvMlPrivEpSetStalledState,( NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                    // Stall if any unsupported request comes
                    break;
            }
            break;
            // Stall here, as we don't support endpoint requests here
        case HOST2DEV_ENDPOINT:
            switch (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST])
            {
                case SET_FEATURE:
                    switch (s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE])
                    {
                        case ENDPOINT_HALT:
                            AckEp = NV_TRUE;
                            // Get once the EP status, to find wether Txfer is success or not
                            EndpointAddr = (s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX]) |
                                                    (s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX + 1] << 8);

                            switch(EndpointAddr)
                            {
                                case CTRL_OUT:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlOut, NV_TRUE ));
                                    break;
                                case CTRL_IN:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                                    break;
                                case BULK_OUT:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_BulkOut, NV_TRUE ));
                                    break;
                                case BULK_IN:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_BulkIn, NV_TRUE ));
                                    break;
                                default:
                                    AckEp = NV_FALSE;
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                                    break;
                            }

                            if (AckEp)
                            {
                                // ACK the endpoint
                                // Start the endpoint for zero packet acknowledgment
                                ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                                                                                              NULL, 0, NV_TRUE));
                            }
                            break;
                        default:
                            NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                            break;
                    }
                    break;
                case CLEAR_FEATURE:
                    switch (s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE])
                    {
                        case ENDPOINT_HALT:
                            AckEp = NV_TRUE;
                            // Get once the EP status, to find wether Txfer is success or not
                            EndpointAddr = ((s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX]) |
                                                    (s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX + 1] << 8));

                            switch (EndpointAddr)
                            {
                                case CTRL_OUT:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlOut, NV_FALSE ));
                                    break;
                                case CTRL_IN:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlIn, NV_FALSE ));
                                    break;
                                case BULK_OUT:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_BulkOut, NV_FALSE ));
                                    break;
                                case BULK_IN:
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_BulkIn, NV_FALSE ));
                                    break;
                                default:
                                    AckEp = NV_FALSE;
                                    NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                                    break;
                            }

                            if (AckEp)
                            {
                                // ACK the endpoint
                                // Start the endpoint for zero packet acknowledgment
                                ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                                                                                              NULL, 0, NV_TRUE));
                            }
                            break;
                        default:
                            NVML_FUN_SOC(NvMlPrivEpSetStalledState, (NvBootUsbfEndPoint_ControlIn, NV_TRUE ));
                            break;
                    }
                    break;
                default:
                    // Stall if any Un supported request comes
                   IsEndPointStall = NV_TRUE;
                    break;
            }
            break;
        default:
            // Stall if any Un supported request comes
            IsEndPointStall = NV_TRUE;
            break;
    }

    // Transmit data to the Host if any descriptors need to send to Host
    if (transmitData)
    {
        // Copy descriptor data to the USB buffer to transmit over USB
        NvOsMemcpy(pXferBuffer, pDataBuffer, DataSize);
        // Transmit the data to the HOST
        ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(
                    s_pUsbfCtxt,
                    USB_EP_CTRL_IN,
                    pXferBuffer,
                    SetupPktLength >= DataSize? DataSize: SetupPktLength,
                    NV_TRUE));
        // Ack on OUT end point after data is sent
        if (ErrStatus == NvBootError_Success)
        {
            // Start the endpoint for zero packet acknowledgment
            ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_OUT,
                                              NULL, 0, NV_TRUE));
        }
    }

    if (IsEndPointStall)
    {
        NvU32 regVal = 0;

        // Control endpoints can only be stalled as a pair. They unstall
        // automatically when another setup packet arrives.
        regVal = USB_DRF_DEF(ENDPTCTRL0, TXT, CTRL) |
                 USB_DRF_DEF(ENDPTCTRL0, TXE, ENABLE) |
                 USB_DRF_DEF(ENDPTCTRL0, TXS, EP_STALL) |
                 USB_DRF_DEF(ENDPTCTRL0, RXT, CTRL) |
                 USB_DRF_DEF(ENDPTCTRL0, RXE, ENABLE) |
                 USB_DRF_DEF(ENDPTCTRL0, RXS, EP_STALL);
        USB_REG_WR(ENDPTCTRL0, regVal);
    }

    return ErrStatus;
}

NvBootError
NVML_FUN_SOC(
    NvMlPrivProcessHost2DevDevicePacket,
    (NvBool *pIsEndPointStall))
{
    NvBootError ErrStatus = NvBootError_Success;

    if (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST] == SET_ADDRESS)
    {
        // ACK the endpoint
        // Start the endpoint for zero packet acknowledgment
        ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                                          NULL, 0, NV_TRUE));

        if (ErrStatus == NvBootError_Success)
        {
            // Set device address in the USB controller
            USB_REG_UPDATE_NUM(PERIODICLISTBASE, USBADR,
                               s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE]);
        }
    }
    else if (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST] == SET_CONFIGURATION)
    {
        // ACK the endpoint
        // Start the endpoint for zero packet acknowledgment
        ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                                          NULL, 0, NV_TRUE));

        if (ErrStatus == NvBootError_Success)
        {
            s_pUsbfCtxt->UsbConfigurationNo =
                                s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE];
            // Configuring Bulk Out and IN endpoints
            NVML_FUN_SOC(NvMlPrivHwInitalizeEndpoints,(s_pUsbfCtxt, USB_EP_BULK_OUT));
            NVML_FUN_SOC(NvMlPrivHwInitalizeEndpoints,(s_pUsbfCtxt, USB_EP_BULK_IN));
            // enumeration is done now hand over to Bulk
            s_pUsbfCtxt->EnumerationDone = NV_TRUE;
        }
    }
    else if (s_pUsbfCtxt->setupPkt[USB_SETUP_REQUEST] == SET_FEATURE)
    {
        if (s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE] == TEST_MODE)
        {
            // ACK the endpoint
            // Start the endpoint for zero packet acknowledgment
            ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                    NULL, 0, NV_TRUE));

            if (ErrStatus == NvBootError_Success)
            {
                NvU32 TestSelecter = ((s_pUsbfCtxt->setupPkt[USB_SETUP_INDEX + 1]) & 0xF);
                if (TestSelecter == TEST_MODE_SELECTOR_TEST_PACKET)
                {
                    NvOsMemcpy((void *)s_Transport.ep0Buffer,
                            (void *) &s_TestPacket[0], USB_TEST_PKT_LENGTH);

                    ErrStatus = NVML_FUN_SOC(NvMlPrivHwTxfrStart,(s_pUsbfCtxt, USB_EP_CTRL_IN,
                            s_Transport.ep0Buffer, USB_TEST_PKT_LENGTH, NV_FALSE));
                }

                // Set device address in the USB controller
                USB_REG_UPDATE_NUM(PORTSC1, PTC, TestSelecter);
                // Spin forever don't do anything
                // Device must be power cycled to ext test mode
                while (1);
            }
        }
    }
    else
    {
        // Stall if we don't support any other feature
        *pIsEndPointStall = NV_TRUE;
    }
    return ErrStatus;
}

void
NVML_FUN_SOC(
    GetStandardConfigIfc,
    (NvU32 *pIfcClass,
    NvU32 *pIfcSubclass,
    NvU32 *pIfcProtocol))
{
    *pIfcClass = 0xFF;
    *pIfcSubclass = 0xFF;
    *pIfcProtocol = 0xFF;
}

NvU8 *NVML_FUN_SOC(GetStandardProductId,(void))
{
    return s_UsbProductID;
}

void
NVML_FUN_SOC(
    NvMlPrivProcessDeviceDescriptor,(
    NvBool *pTransmitData,
    NvU8 **pDataBuffer,
    NvU32 *pDataSize,
    NvBool *pIsEndPointStall))
{
    // Get Descriptor Request
    NvU32 IfcClass = 0;
    NvU32 IfcSubclass = 0;
    NvU32 IfcProtocol = 0;
    NvOdmUsbConfigDescFn OdmConfigFn = Nv3pUsbGetConfigDescFn();
    NvOdmUsbProductIdFn OdmProductFn = Nv3pUsbGetProductIdFn();
    NvOdmUsbSerialNumFn OdmSerialFn = Nv3pUsbGetSerialNumFn();
    NvU8 *pProductId = NULL;
    NvU8 *pSerialNum = NULL;
    switch(s_pUsbfCtxt->setupPkt[USB_SETUP_DESCRIPTOR])
    {
        case USB_DT_DEVICE:
            *pTransmitData = NV_TRUE;
            NVML_FUN_SOC(NvBootConfigureDevDescriptor,(&s_UsbDeviceDescriptor[0]));
            *pDataBuffer = &s_UsbDeviceDescriptor[0];
            *pDataSize = USB_DEV_DESCRIPTOR_SIZE;
            break;

        case USB_DT_CONFIG:
            *pTransmitData = NV_TRUE;
            if (!OdmConfigFn ||
                !(*OdmConfigFn)(&IfcClass, &IfcSubclass, &IfcProtocol))
                NVML_FUN_SOC(GetStandardConfigIfc,(&IfcClass, &IfcSubclass, &IfcProtocol));
            s_UsbConfigDescriptor[14] = (NvU8)(IfcClass);
            s_UsbConfigDescriptor[15] = (NvU8)(IfcSubclass);
            s_UsbConfigDescriptor[16] = (NvU8)(IfcProtocol);
            if (s_pUsbfCtxt->UsbPortSpeed == NvBootUsbfPortSpeed_High)
            {
                s_UsbConfigDescriptor[22] =
                    (USB_HIGH_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_UsbConfigDescriptor[23] =
                    ((USB_HIGH_SPEED_PKT_SIZE_BYTES>>8) & 0xFF);
                s_UsbConfigDescriptor[29] =
                    (USB_HIGH_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_UsbConfigDescriptor[30] =
                    ((USB_HIGH_SPEED_PKT_SIZE_BYTES>>8) & 0xFF);
            }
            else
            {
                s_UsbConfigDescriptor[22] =
                    (USB_FULL_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_UsbConfigDescriptor[23] =
                    ((USB_FULL_SPEED_PKT_SIZE_BYTES>>8) & 0xFF);
                s_UsbConfigDescriptor[29] =
                    (USB_FULL_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_UsbConfigDescriptor[30] =
                    ((USB_FULL_SPEED_PKT_SIZE_BYTES>>8) & 0xFF);
            }
            *pDataBuffer = &s_UsbConfigDescriptor[0];
            *pDataSize = USB_CONFIG_DESCRIPTOR_SIZE;
            break;

        case USB_DT_STRING:
            *pTransmitData = NV_TRUE;
            switch (s_pUsbfCtxt->setupPkt[USB_SETUP_VALUE])
            {
                case USB_MANF_ID: //Manufacture ID
                    *pDataSize = (s_UsbManufacturerID[0] <=
                                USB_MANF_STRING_LENGTH) ?
                                s_UsbManufacturerID[0] :
                                USB_MANF_STRING_LENGTH;
                    *pDataBuffer = &s_UsbManufacturerID[0];
                    break;
                case USB_PROD_ID:    // Product ID
                    if (OdmProductFn)
                        pProductId = (*OdmProductFn)();
                    if (!pProductId)
                        pProductId = NVML_FUN_SOC(GetStandardProductId,());
                    *pDataSize = (pProductId[0]);
                    *pDataBuffer = pProductId;
                    break;
                case USB_SERIAL_ID:    // Serial Number
                    if (OdmSerialFn)
                        pSerialNum = (*OdmSerialFn)();
                    if (!pSerialNum)
                        pSerialNum = s_UsbSerialNumber;
                    *pDataBuffer = pSerialNum;
                    *pDataSize = pSerialNum[0];
                    break;
                case USB_LANGUAGE_ID:    //Language ID
                //Default case return Language ID
                default: //Language ID
                    *pDataSize = USB_LANGUAGE_ID_LENGTH;
                    *pDataBuffer = &s_UsbLanguageID[0];
                    break;
            }
            break;
        case USB_DT_DEVICE_QUALIFIER:
            if (!OdmConfigFn ||
                !(*OdmConfigFn)(&IfcClass, &IfcSubclass, &IfcProtocol))
                NVML_FUN_SOC(GetStandardConfigIfc,(&IfcClass, &IfcSubclass, &IfcProtocol));
            s_UsbDeviceQualifier[4] = IfcClass;
            s_UsbDeviceQualifier[5] = IfcSubclass;
            s_UsbDeviceQualifier[6] = IfcProtocol;
            *pTransmitData = NV_TRUE;
            *pDataBuffer = &s_UsbDeviceQualifier[0];
            *pDataSize = USB_DEV_QUALIFIER_LENGTH;
            break;
        case USB_DT_OTHER_SPEED_CONFIG:
            *pTransmitData = NV_TRUE;
            if (!OdmConfigFn ||
                !(*OdmConfigFn)(&IfcClass, &IfcSubclass, &IfcProtocol))
                NVML_FUN_SOC(GetStandardConfigIfc,(&IfcClass, &IfcSubclass, &IfcProtocol));
            s_OtherSpeedConfigDesc[14] = (NvU8)(IfcClass);
            s_OtherSpeedConfigDesc[15] = (NvU8)(IfcSubclass);
            s_OtherSpeedConfigDesc[16] = (NvU8)(IfcProtocol);
            if (s_pUsbfCtxt->UsbPortSpeed == NvBootUsbfPortSpeed_High)
            {
                s_OtherSpeedConfigDesc[22] = (USB_FULL_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_OtherSpeedConfigDesc[23] = ((USB_FULL_SPEED_PKT_SIZE_BYTES >> 8) & 0xFF);
                s_OtherSpeedConfigDesc[29] = (USB_FULL_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_OtherSpeedConfigDesc[30] = ((USB_FULL_SPEED_PKT_SIZE_BYTES >> 8) & 0xFF);
            }
            else
            {
                s_OtherSpeedConfigDesc[22] = (USB_HIGH_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_OtherSpeedConfigDesc[23] = ((USB_HIGH_SPEED_PKT_SIZE_BYTES >> 8) & 0xFF);
                s_OtherSpeedConfigDesc[29] = (USB_HIGH_SPEED_PKT_SIZE_BYTES & 0xFF);
                s_OtherSpeedConfigDesc[30] = ((USB_HIGH_SPEED_PKT_SIZE_BYTES >> 8) & 0xFF);
            }
            *pDataBuffer = &s_OtherSpeedConfigDesc[0];
            *pDataSize = sizeof(s_OtherSpeedConfigDesc);
            break;
        default:
            // Stall if any Un supported request comes
            *pIsEndPointStall = NV_TRUE;
            break;
    }
}

NvBootError
NVML_FUN_SOC(NvMlPrivHwEnableController,(void))
{
    return NVML_FUN_SOC(NvMlPrivUsbfHwEnableController,());
}

NvBootError
NVML_FUN_SOC(
    NvMlPrivHwIntializeController,
    (NvBootUsbfContext *pUsbFuncCtxt))
{
    NvBootError ErrValue = NvBootError_Success;
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US;
    NvU32 regValue = 0;

    // USB controller is not enabled yet, set to FALSE
    pUsbFuncCtxt->UsbControllerEnabled = NV_FALSE;

    ErrValue = NVML_FUN_SOC(NvMlPrivHwEnableController,());

    if (ErrValue != NvBootError_Success)
    {
        return ErrValue;
    }

    //By now USB controller is enabled set to TRUE
    pUsbFuncCtxt->UsbControllerEnabled = NV_TRUE;

    // Clear device address
    USB_REG_WR(PERIODICLISTBASE, USB_DEF_RESET_VAL(PERIODICLISTBASE));
    // Clear setup token, by reading and wrting back the same value
    USB_REG_WR(ENDPTSETUPSTAT, USB_REG_RD(ENDPTSETUPSTAT));
    // Clear endpoint complete status bits.by reading and writing back
    USB_REG_WR(ENDPTCOMPLETE, USB_REG_RD(ENDPTCOMPLETE));

    // STOP the USB controller
    USB_REG_UPDATE_DEF(USBCMD, RS, STOP);
    // Set the USB mode to the IDLE before reset
    USB_REG_UPDATE_DEF(USBMODE, CM, IDLE);
    // Reset the controller
    USB_REG_UPDATE_DEF(USBCMD, RST, SET);

    do {
        // Wait till bus reset clears.
        regValue = USB_REG_READ_VAL(USBCMD, RST);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (regValue);

    ErrValue = NVML_FUN_SOC(NvMlPrivUsbWaitClock,());
    if (ErrValue != NvBootError_Success)
    {
        return ErrValue;
    }

    // set the controller to device controller mode
    USB_REG_UPDATE_DEF( USBMODE, CM, DEVICE_MODE);

    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        //wait till device mode change is finished.
        regValue = USB_REG_READ_VAL(USBMODE, CM);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (regValue != USB_DRF_DEF_VAL(USBMODE, CM, DEVICE_MODE));

    // Disable all USB interrupts
    USB_REG_WR(USBINTR, USB_DEF_RESET_VAL(USBINTR));
    USB_REG_WR(OTGSC, USB_DEF_RESET_VAL(OTGSC));

    // clear all pending interrupts, if any
    regValue = USB_DRF_DEF(USBSTS, SLI, SUSPENDED) |
               USB_DRF_DEF(USBSTS, SRI, SOF_RCVD) |
               USB_DRF_DEF(USBSTS, URI, USB_RESET) |
               USB_DRF_DEF(USBSTS, AAI, ADVANCED) |
               USB_DRF_DEF(USBSTS, SEI, ERROR) |
               USB_DRF_DEF(USBSTS, FRI, ROLLOVER) |
               USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE) |
               USB_DRF_DEF(USBSTS, UEI, ERROR) |
               USB_DRF_DEF(USBSTS, UI, INT);
    USB_REG_WR(USBSTS, regValue);

    // clear OTGSC interupts
    regValue = USB_DRF_DEF(OTGSC, DPIS, INT_SET) |
               USB_DRF_DEF(OTGSC, ONEMSS, INT_SET) |
               USB_DRF_DEF(OTGSC, BSEIS, INT_SET) |
               USB_DRF_DEF(OTGSC, BSVIS, INT_SET) |
               USB_DRF_DEF(OTGSC, ASVIS, INT_SET) |
               USB_DRF_DEF(OTGSC, AVVIS, INT_SET) |
               USB_DRF_DEF(OTGSC, IDIS, INT_SET);
    USB_REG_WR(OTGSC, regValue);

    // Clear setup packet status
    regValue = USB_DRF_DEF(ENDPTSETUPSTAT, ENDPTSETUPSTAT0, SETUP_RCVD) |
               USB_DRF_DEF(ENDPTSETUPSTAT, ENDPTSETUPSTAT1, SETUP_RCVD) |
               USB_DRF_DEF(ENDPTSETUPSTAT, ENDPTSETUPSTAT2, SETUP_RCVD);
    USB_REG_WR(ENDPTSETUPSTAT, regValue);

    // Set interrupts to occur immediately
    USB_REG_UPDATE_DEF(USBCMD, ITC, IMMEDIATE);

    // Enable USB OTG interrupts.
    regValue = USB_DRF_DEF(OTGSC, BSEIE, ENABLE) |
               USB_DRF_DEF(OTGSC, BSVIE, ENABLE) |
               USB_DRF_DEF(OTGSC, ASVIE, ENABLE) |
               USB_DRF_DEF(OTGSC, AVVIE, ENABLE) |
               USB_DRF_DEF(OTGSC, IDIE, ENABLE);
    USB_REG_WR(OTGSC, regValue);

    // Enable USB interrupts
    regValue = USB_DRF_DEF(USBINTR, SLE, ENABLE) |
               USB_DRF_DEF(USBINTR, URE, ENABLE) |
               USB_DRF_DEF(USBINTR, SEE, ENABLE) |
               USB_DRF_DEF(USBINTR, PCE, ENABLE) |
               USB_DRF_DEF(USBINTR, UEE, ENABLE) |
               USB_DRF_DEF(USBINTR, UE, ENABLE);
    USB_REG_WR(USBINTR, regValue);

    // Clear Queuehead descriptors
    NvOsMemset(&s_pUsbDescriptorBuf->pQueueHead[0], 0,
       sizeof(NvBootUsbDevQueueHead) * USBF_MAX_EP_COUNT);

    // Clear Transfer descriptors
    NvOsMemset(&s_pUsbDescriptorBuf->pDataTransDesc[0], 0,
        sizeof(NvBootUsbDevTransDesc) * USBF_MAX_DTDS);

    // Initialize the EndPoint Base Addr with the Queue Head base address
    USB_REG_WR(ASYNCLISTADDR,
        PTR_TO_ADDR(&s_pUsbDescriptorBuf->pQueueHead[0]));

    // Initaialize the control Out endpoint
    NVML_FUN_SOC(NvMlPrivHwInitalizeEndpoints,(pUsbFuncCtxt, USB_EP_CTRL_OUT));
    // Initaialize the control IN endpoint
    NVML_FUN_SOC(NvMlPrivHwInitalizeEndpoints,(pUsbFuncCtxt, USB_EP_CTRL_IN));

    // Start up controller.
    USB_REG_UPDATE_DEF(USBCMD, RS, RUN);
    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        // wait till device starts running.
        regValue = USB_REG_READ_VAL(USBCMD, RS);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (regValue != USB_DRF_DEF_VAL(USBCMD, RS, RUN));

    return NvBootError_Success;
}

NvU32
NVML_FUN_SOC(
    NvMlPrivHwGetPacketSize,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint))
{
    NvU32 PacketSize = USB_FULL_SPEED_PKT_SIZE_BYTES;
    NvBool HighSpeed = NV_FALSE;

    // Return packet size based on speed and endpoint type.
    switch (EndPoint)
    {
        // If endpoint is control IN or OUT return FULL speed pkt size.
        case USB_EP_CTRL_OUT:
        case USB_EP_CTRL_IN:
            PacketSize = USB_FULL_SPEED_PKT_SIZE_BYTES;
            break;
        // If bulk IN or OUT return speed depending on the port status
        case USB_EP_BULK_OUT:
        case USB_EP_BULK_IN:
            HighSpeed = (pUsbFuncCtxt->UsbPortSpeed ==
                         NvBootUsbfPortSpeed_High);
            // Return the speed based on the port speed for bulk endpoints
            // fall through the default for returning the Packet size.
        default:
            // In the default return the packet size as FULL speed packet
            PacketSize = HighSpeed ?
                         USB_HIGH_SPEED_PKT_SIZE_BYTES :
                         USB_FULL_SPEED_PKT_SIZE_BYTES;
            break;
    }

    return PacketSize;
}

NvBootError
NVML_FUN_SOC(
    NvMlPrivHwTxfrStart,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU8 *pDataBuf,
    NvU32 maxTxfrBytes,
    NvBool WaitForTxfrComplete))
{
    NvBootError BootError = NvBootError_Success;
    NvBootUsbDevTransDesc *pUsbDevTxfrDesc;
    NvBootUsbDevQueueHead *pUsbDevQueueHead;
    NvBootUsbfEpStatus EpStatus;
    NvU32 regVal;

    // Clear leftover transfer, if any.before starting the transaction
    NVML_FUN_SOC(NvMlPrivHwTxfrClear,(pUsbFuncCtxt, EndPoint));

    // Reference to queue head for configureation.
    pUsbDevQueueHead = &s_pUsbDescriptorBuf->pQueueHead[EndPoint];
    // Clear the Queue Head before configuration
    NvOsMemset(pUsbDevQueueHead, 0, USB_HW_QUEUE_HEAD_SIZE_BYTES);

    // Interrupt on setup if it is endpoint 0_OUT.
    if (EndPoint == USB_EP_CTRL_OUT)
    {
        pUsbDevQueueHead->EpCapabilities = USB_DQH_DRF_DEF(IOC, ENABLE);
    }
    // setup the Q head with Max packet length and zero length terminate
    pUsbDevQueueHead->EpCapabilities |= (USB_DQH_DRF_NUM(MAX_PACKET_LENGTH,
                        NVML_FUN_SOC(NvMlPrivHwGetPacketSize,( pUsbFuncCtxt, EndPoint)))|
                        USB_DQH_DRF_DEF(ZLT, ZERO_LENGTH_TERM_DISABLED));

    // Don't to terminate the next DTD pointer by writing 0 = Clear
    // we will assign the DTD pointer after configuring DTD
    pUsbDevQueueHead->NextDTDPtr = USB_DQH_DRF_DEF(TERMINATE, CLEAR);
    // Indicate EP is configured
    s_pUsbDescriptorBuf->EpConfigured[EndPoint] = NV_TRUE;
    //Store the Bytes requested by client
    s_pUsbDescriptorBuf->BytesRequestedForEp[EndPoint] = maxTxfrBytes;

    // Reference to DTD for configureation.
    pUsbDevTxfrDesc = &s_pUsbDescriptorBuf->pDataTransDesc[EndPoint];
    // clear the DTD before configuration
    NvOsMemset(pUsbDevTxfrDesc, 0, sizeof(NvBootUsbDevTransDesc));

    // Setup the DTD
    pUsbDevTxfrDesc->NextDtd = USB_DTD_DRF_DEF(TERMINATE, SET);
    // Set number of bytes to transfer in DTD.and set status to active
    pUsbDevTxfrDesc->DtdToken = USB_DTD_DRF_DEF(ACTIVE, SET)|
                                USB_DTD_DRF_NUM(TOTAL_BYTES, maxTxfrBytes);
    // Assign buffer pointer to DTD.
    pUsbDevTxfrDesc->BufPtrs[0] = PTR_TO_ADDR(pDataBuf);

    ///Next DTD address need to program only upper 27 bits
    pUsbDevQueueHead->NextDTDPtr |= USB_DQH_DRF_NUM(NEXT_DTD_PTR,
                                    (PTR_TO_ADDR(pUsbDevTxfrDesc) >>
                                    USB_DQH_FLD_SHIFT_VAL(NEXT_DTD_PTR)));

    // Start the transaction on the USB Bus by priming the endpoint
    regVal = USB_REG_RD(ENDPTPRIME);


    regVal |= USB_EP_NUM_TO_WORD_MASK(EndPoint);

    USB_REG_WR(ENDPTPRIME, regVal);

    if (WaitForTxfrComplete)
    {
        // wait for transfer complete on the USB bus
        BootError = NVML_FUN_SOC(NvMlPrivHwTxfrWait,(pUsbFuncCtxt,
                                EndPoint, USB_MAX_TXFR_WAIT_TIME_MS));
        if (BootError != NvBootError_Success)
        {
            // there is some error in the transmit operation
            // clear the pending transfer and return the error info.
            NVML_FUN_SOC(NvMlPrivHwTxfrClear,(pUsbFuncCtxt, EndPoint));
            return BootError;
        }
        ///Get once the EP status to find wether Txfer is success or not
        EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(pUsbFuncCtxt, EndPoint));

        if (EpStatus == NvBootUsbfEpStatus_TxfrComplete)
            BootError = NvBootError_Success;
        else
            BootError = NvBootError_TxferFailed;
    }

    return  BootError;
}

NvBootError
NVML_FUN_SOC(
    NvMlPrivHwTxfrWait,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint,
    NvU32 timeoutMs))
{
    NvBootError retStatus = NvBootError_Success;
    NvBootUsbfEpStatus EpStatus;
    NvU32 time;

    // Get atleaset once the EP status
    EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(pUsbFuncCtxt, EndPoint));

    // If transfer is active wit till the time out or transfer status change
    if (EpStatus == NvBootUsbfEpStatus_TxfrActive)
    {
        // Multiplying by 1000 to make delay wait time in microseconds
        for (time = 0; time < (timeoutMs*1000); time++)
        {
            NvOsWaitUS(1);
            EpStatus = NVML_FUN_SOC(NvMlPrivHwEpGetStatus,(pUsbFuncCtxt, EndPoint));
            if (EpStatus == NvBootUsbfEpStatus_TxfrActive)
            {
                // continue till time out or transfer is active
                continue;
            }
            else
            {
                // either success or error break here and return the status
                break;
            }
        }
    }
    // Still transfer is active then it means HW is timed out
    if (EpStatus == NvBootUsbfEpStatus_TxfrActive)
    {
        retStatus = NvBootError_HwTimeOut;
    }

    return retStatus;
}

void
NVML_FUN_SOC(
    NvMlPrivHwInitalizeEndpoints,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint))
{
    NvBootUsbDevQueueHead *pQueueHead;

    // Set up Queue Head for endpoint.
    pQueueHead = &s_pUsbDescriptorBuf->pQueueHead[EndPoint];
    NvOsMemset((void*) pQueueHead, 0, USB_HW_QUEUE_HEAD_SIZE_BYTES);

    // Interrupt on setup if it is endpoint 0_OUT.
    if (EndPoint == USB_EP_CTRL_OUT)
    {
        pQueueHead->EpCapabilities = USB_DQH_DRF_DEF(IOC, ENABLE);
    }

    // terminate the next DTD pointer by writing 1 = SET
    pQueueHead->NextDTDPtr = USB_DQH_DRF_DEF(TERMINATE, SET);

    pQueueHead->EpCapabilities |= USB_DQH_DRF_NUM(MAX_PACKET_LENGTH,
                           NVML_FUN_SOC(NvMlPrivHwGetPacketSize,( pUsbFuncCtxt, EndPoint)));

    if (USB_IS_EP_IN(EndPoint))
    {
        // Configure transmit endpoints.
        // Check endpoint type and configure corresponding register
        // endpoint# 0 and 1 is control OUT and IN
        // and endpoint# 2 and 3 Bulk OUT and IN
        if (EndPoint >> 1)
        {
            // Configuring BULK IN endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL1, TXT, BULK);
            USB_REG_UPDATE_DEF(ENDPTCTRL1, TXS, EP_OK);
            // Whenever a configuration event is received for  this Endpoint,
            // software must write a one to this bit in order to synchronize
            // the data PIDs between the host and device.
            USB_REG_UPDATE_DEF(ENDPTCTRL1, TXR, RESET_PID_SEQ);
            // Enable transmit endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL1, TXE, ENABLE);
        }
        else
        {
            // Configuring CONTROL IN endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL0, TXT, CTRL);
            USB_REG_UPDATE_DEF(ENDPTCTRL0, TXS, EP_OK);
            // Enable transmit endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL0, TXE, ENABLE);
        }
    }
    else
    {
        // Configure Recieve endpoints.
        // Check endpoint type and configure corresponding register
        // endpoint# 0 and 1 is control OUT and IN
        // and endpoint# 2 and 3 Bulk OUT and IN
        if (EndPoint >> 1)
        {
            // Configuring BULK OUT endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL1, RXT, BULK);
            USB_REG_UPDATE_DEF(ENDPTCTRL1, RXS, EP_OK);
            // Whenever a configuration event is received for  this Endpoint,
            // software must write a one to this bit in order to synchronize
            // the data PIDs between the host and device.
            USB_REG_UPDATE_DEF(ENDPTCTRL1, RXR, RESET_PID_SEQ);
            // Enable recieve endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL1, RXE, ENABLE);
        }
        else
        {
            // Configuring CONTROL OUT endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL0, RXT, CTRL);
            USB_REG_UPDATE_DEF(ENDPTCTRL0, RXS, EP_OK);
            // Enable recieve endpoint
            USB_REG_UPDATE_DEF(ENDPTCTRL0, RXE, ENABLE);
        }
    }

}

NvU32 NVML_FUN_SOC(NvMlPrivHwGetEvent,(void))
{
    NvU32 NewUsbInts = 0;
    NvU32 regVal = 0;

    // Get bitmask of status for new USB ints that we have enabled.
    regVal = USB_REG_RD(USBSTS);
    // prepare the Interupt status mask and get the interrupted bits value
    NewUsbInts = (regVal & (USB_DRF_DEF(USBSTS, SLI, SUSPENDED) |
                                 USB_DRF_DEF(USBSTS, SRI, SOF_RCVD) |
                                 USB_DRF_DEF(USBSTS, URI, USB_RESET) |
                                 USB_DRF_DEF(USBSTS, AAI, ADVANCED) |
                                 USB_DRF_DEF(USBSTS, SEI, ERROR) |
                                 USB_DRF_DEF(USBSTS, FRI, ROLLOVER) |
                                 USB_DRF_DEF(USBSTS, PCI, PORT_CHANGE) |
                                 USB_DRF_DEF(USBSTS, UEI, ERROR) |
                                 USB_DRF_DEF(USBSTS, UI, INT)));
    // Clear the interrupted bits by writing back the staus value to register
    regVal |= NewUsbInts;
    USB_REG_WR(USBSTS,regVal);

    return NewUsbInts;
}

NvBootError NVML_FUN_SOC(NvMlPrivHwEndPointFlush,(NvU32 EndPoint))
{
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US;
    NvU32 EndpointMask;
    NvU32 RegValue;

    // Get the Endpoint mask depending on the endpoin number
    if(EndPoint == USB_FLUSH_ALL_EPS)
        EndpointMask = EndPoint;
    else
        EndpointMask = USB_EP_NUM_TO_WORD_MASK(EndPoint);

    // Flush endpoints
    USB_REG_WR(ENDPTFLUSH, EndpointMask);
    do {
        RegValue = USB_REG_RD(ENDPTFLUSH);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (RegValue & EndpointMask);

    // Wait for status bits to clear as well.
    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        RegValue = USB_REG_RD(ENDPTSTATUS);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (RegValue & EndpointMask);

    // Wait till all primed endpoints clear.
    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        RegValue = USB_REG_RD(ENDPTPRIME);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (RegValue & EndpointMask);

    return NvBootError_Success;
}

void
NVML_FUN_SOC(
    NvMlPrivHwTxfrClear,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint))
{
    NvU32 regVal;

    // flush the endpoint
    NVML_FUN_SOC(NvMlPrivHwEndPointFlush,(EndPoint));

    // clear the Queue head for this endpoint
    NvOsMemset( &s_pUsbDescriptorBuf->pDataTransDesc[EndPoint], 0,
        sizeof(NvBootUsbDevTransDesc));
    // clear the transfer descriptor for this endpoint
    NvOsMemset(&s_pUsbDescriptorBuf->pQueueHead[EndPoint],
        0, USB_HW_QUEUE_HEAD_SIZE_BYTES );

    // Indicate the endpoint is cleared.
    s_pUsbDescriptorBuf->EpConfigured[EndPoint] = NV_FALSE;

    // Clear the endpoint bytes requested
    s_pUsbDescriptorBuf->BytesRequestedForEp[EndPoint] = 0;

    // Clear endpoint complete status bits.
    regVal = USB_REG_RD(ENDPTCOMPLETE);
    regVal |= USB_EP_NUM_TO_WORD_MASK(EndPoint);
    USB_REG_WR(ENDPTCOMPLETE, regVal);
}

NvBootUsbfEpStatus
NVML_FUN_SOC(
    NvMlPrivHwEpGetStatus,
    (NvBootUsbfContext *pUsbFuncCtxt,
    NvU32 EndPoint))
{
    NvBootUsbfEpStatus EpStatus = NvBootUsbfEpStatus_NotConfigured;
    NvU32 EpCtrlRegValue = 0;
    NvBootUsbDevQueueHead *pQueueHead;

    // Check endpoint type and read corresponding register
    // endpoint# 0 and 1 is control OUT and IN
    // and endpoint# 2 and 3 Bulk OUT and IN
    if (EndPoint >> 1)
    {
        EpCtrlRegValue = USB_REG_RD(ENDPTCTRL1);
    }
    else
    {
        EpCtrlRegValue = USB_REG_RD(ENDPTCTRL0);
    }

    // If endpoint is stalled, report that
    // TXS, TXE, RXS and RXE are at the same location in ENDPTCTRL0
    // and ENDPTCTRL1 so reading from the same DRF register is OK
    if ((USB_IS_EP_IN(EndPoint)) ?
         USB_DRF_VAL(ENDPTCTRL0, TXS, EpCtrlRegValue) :
         USB_DRF_VAL(ENDPTCTRL0, RXS, EpCtrlRegValue))
    {
        EpStatus = NvBootUsbfEpStatus_Stalled;
        goto Exit;
    }

        // If port is not enabled, return end point is not configured.
    if ((USB_IS_EP_IN(EndPoint)) ?
         USB_DRF_VAL(ENDPTCTRL0, TXE, EpCtrlRegValue) :
         USB_DRF_VAL(ENDPTCTRL0, RXE, EpCtrlRegValue))
    {
        // Temporary pointer to queue head.
        pQueueHead = &s_pUsbDescriptorBuf->pQueueHead[EndPoint];
        // Look for an error by inspecting the DTD fields stored in the DQH.
        if (pQueueHead->DtdToken & ( USB_DQH_DRF_DEF(HALTED,SET)|
                              USB_DQH_DRF_DEF(DATA_BUFFER_ERROR,SET)|
                              USB_DQH_DRF_DEF(TRANSACTION_ERROR,SET)))
        {
            EpStatus = NvBootUsbfEpStatus_TxfrFail;
            goto Exit;
        }
        // If endpoint active, check to see if it has completed an operation.
        if ((USB_REG_RD(ENDPTPRIME) & USB_EP_NUM_TO_WORD_MASK(EndPoint)) ||
            (USB_REG_RD(ENDPTSTATUS) & USB_EP_NUM_TO_WORD_MASK(EndPoint)) )
        {
            EpStatus = NvBootUsbfEpStatus_TxfrActive;
            goto Exit;
        }
        // Check for a complete transaction.
        if (s_pUsbDescriptorBuf->EpConfigured[EndPoint])
        {
            EpStatus = NvBootUsbfEpStatus_TxfrComplete;
            goto Exit;
        }
        // Return saying endpoint is IDLE and ready for Txfer
        EpStatus = NvBootUsbfEpStatus_TxfrIdle;
    }
    else
    {
        // Return as endpoint is not configured
        EpStatus = NvBootUsbfEpStatus_NotConfigured;
    }

Exit:
    return EpStatus;
}

void NVML_FUN_SOC(NvBootConfigureDevDescriptor,(NvU8 *pDevDescriptor))
{
    NvU32 OdmProductId;
    NvU32 OdmVendorId;
    NvU32 OdmBcdDeviceId;
    NvOdmUsbDeviceDescFn OdmDeviceFn = Nv3pUsbGetDeviceDescFn();

    if (!OdmDeviceFn ||
        !(*OdmDeviceFn)(&OdmVendorId, &OdmProductId, &OdmBcdDeviceId))
    {
        NvU32 RegValue = 0;
        NvU32 Sku;
#define NVBOOT_USBF_DESCRIPTOR_SKU_MASK  0xF

        RegValue = APB_MISC_REG_RD(GP_HIDREV);
        NVBOOT_FUN_SOC(NvBootFuseGetSkuRaw,(&Sku));

        OdmVendorId = 0x955;
        OdmProductId = (NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegValue) |
            (NV_DRF_VAL(APB_MISC, GP_HIDREV, HIDFAM, RegValue)<<12) |
            ((Sku & NVBOOT_USBF_DESCRIPTOR_SKU_MASK)<<8));
        OdmBcdDeviceId = (NV_DRF_VAL(APB_MISC, GP_HIDREV, MINORREV, RegValue) |
            (NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, RegValue)<<8));
    }

    pDevDescriptor[8] = (NvU8)(OdmVendorId & 0xff);
    pDevDescriptor[9] = (NvU8)((OdmVendorId>>8) & 0xff);
    pDevDescriptor[10] = (NvU8)(OdmProductId & 0xff);
    pDevDescriptor[11] = (NvU8)((OdmProductId>>8) & 0xff);
    pDevDescriptor[12] = (NvU8)(OdmBcdDeviceId & 0xff);
    pDevDescriptor[13] = (NvU8)((OdmBcdDeviceId>>8)&0xff);
}



void NVML_FUN_SOC(NvMlPrivReinit,(void))
{
    NVML_FUN_SOC(Nv3pTransportPrivInit,());

    // Store the context for future use.
    s_pUsbfCtxt = &s_UsbfContext;

    /* Bootrom already did the enumeration */
    s_pUsbfCtxt->EnumerationDone = NV_TRUE;
    // Assign the Hardware Q head address
    s_pUsbDescriptorBuf->pQueueHead = (NvBootUsbDevQueueHead *)
                                    (NV_ADDRESS_MAP_USB_BASE +
                                    USB2_QH_USB2D_QH_EP_0_OUT_0);

    // Set controller enabled to NV_TRUE untill as we already started the
    // controller.
    s_pUsbfCtxt->UsbControllerEnabled = NV_TRUE;

   NVML_FUN_SOC(NvMlPrivInitBaseAddress,(s_pUsbfCtxt));

    // If port change detected then get the current speed of operation
    // get the USB port speed for transmiting the packet size
   NVML_FUN_SOC(NvMlPrivGetPortSpeed,(s_pUsbfCtxt));
    s_pUsbfCtxt->InitializationDone = NV_TRUE;
}

NvError
NVML_FUN_SOC(Nv3pTransportReopen, (Nv3pTransportHandle *hTrans))
{
    Nv3pTransport *trans = &s_Transport;

    /* put the transfer buffer into IRAM. must be aligned to
     * NVBOOT_USB_BUFFER_ALIGNMENT-size.
     */
    trans->ep0Buffer = (NvU8 *)(
            NV_ADDRESS_MAP_DATAMEM_BASE +
            NV3P_TRANSFER_EP0_BUFFER_OFFSET);

    trans->buffer =  (NvU8 *)(
            NV_ADDRESS_MAP_DATAMEM_BASE +
            NV3P_TRANSFER_EP_BULK_BUFFER_OFFSET);

    trans->size = NVBOOT_USB_MAX_TXFR_SIZE_BYTES;
    trans->read = trans->buffer;
    trans->remaining = 0;

    NVML_FUN_SOC(NvMlPrivReinit,());

    *hTrans = trans;
    return NvSuccess;
}

NvOdmUsbChargerType
NVML_FUN_SOC(Nv3pTransportUsbfGetChargerType, (Nv3pTransportHandle * hTrans,NvU32 instance))
{
    if(s_ChargingMode)
        NVML_FUN_SOC(NvMlUsbfDetectCharger,());
    return s_ChargerType;
}

static NvU32 NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (NvU32 Val))
{
    NvU32 OrgRegVal;
    NvU32 RegVal;
    OrgRegVal = UTMIP_REG_RD(NV_ADDRESS_MAP_USB_BASE, MISC_CFG0);
    RegVal = OrgRegVal;
    RegVal &= ~MASK_ALL_PULLUP_PULLDOWN;
    RegVal |= Val;
    UTMIP_REG_WR(NV_ADDRESS_MAP_USB_BASE, MISC_CFG0, RegVal);
    NvOsSleepMS(2);
    return OrgRegVal;
}

static NvU32 NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (NvU32 Val))
{
    NvU32 OrgRegVal;
    NvU32 RetVal;
    NvU32 reg;
    OrgRegVal = NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (Val));
    reg = USB_REG_RD(PORTSC1);
    RetVal = USB_PORTSC_LINE_STATE(reg);
    NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (OrgRegVal));
    return RetVal;
}

static NvBool NVML_FUN_SOC(NvMlPrivDetectNonStdCharger, (void))
{

    NvU32 status0;
          /*
         * non std charger has D+/D- line float, we can apply pull up/down on
         * each line and verify if line status change.
         */

    /* pull up DP only */
    status0 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (FORCE_PULLUP_DP | DISABLE_PULLUP_DM
                                        | DISABLE_PULLDN_DP | DISABLE_PULLDN_DM));
    if (USB_PORTSC_LINE_DP_SET != status0)
        goto NOT_NON_STD_CHARGER;

    /* pull down DP only */
    status0 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (DISABLE_PULLUP_DP | DISABLE_PULLUP_DM
                                        | FORCE_PULLDN_DP | DISABLE_PULLDN_DM));
    if (0x0 != status0)
        goto NOT_NON_STD_CHARGER;

    /* pull up DM only */
    status0 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (DISABLE_PULLUP_DP | FORCE_PULLUP_DM
                                        | DISABLE_PULLDN_DP | DISABLE_PULLDN_DM));
    if (USB_PORTSC_LINE_DM_SET != status0)
        goto NOT_NON_STD_CHARGER;

    /* pull down DM only */
    status0 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (DISABLE_PULLUP_DP | DISABLE_PULLUP_DM
                                        | DISABLE_PULLDN_DP | FORCE_PULLDN_DM));
    if (0x0 != status0)
        goto NOT_NON_STD_CHARGER;

    NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (0));
    return    NV_TRUE;

NOT_NON_STD_CHARGER:
    NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (0));
    return NV_FALSE;
}


static NvBool NVML_FUN_SOC(NvMlPrivDetectNvCharger, (void))
{
    NvBool ret;
    NvU32 status1, status2, status3;

    if(NVML_FUN_SOC(NvMlPrivDetectNonStdCharger, ()))
    {
        return NV_FALSE;
    }
    ret = NV_FALSE;

    /* Turn off all terminations so we see DPDN states clearly,
       and only leave on DM pulldown */
    status1 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (DISABLE_PULLUP_DP | DISABLE_PULLUP_DM
                                        | FORCE_PULLDN_DP | DISABLE_PULLDN_DM));

    /* Turn off all terminations except for DP pullup */
    status2 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (FORCE_PULLUP_DP | DISABLE_PULLUP_DM
                                        | DISABLE_PULLDN_DP | DISABLE_PULLDN_DM));
    /* Check for NV charger DISABLE all terminations */

    status3 = NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (DISABLE_PULLUP_DP | DISABLE_PULLUP_DM
                                        | DISABLE_PULLDN_DP | DISABLE_PULLDN_DM));

    if ((status1 == (USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET)) &&
            (status2 == (USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET)) &&
            (status3 == (USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET)))
            ret =    NV_TRUE;

    /* Restore standard termination by hardware. */
    NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (0));
    return ret;
}

static NvBool NVML_FUN_SOC(NvMlUsbfDCDChargerDetect, (void))
{
    NvU32 RegVal;
    NvU32 OrgRegVal;
    NvU32 reg;
    NvBool ret = NV_FALSE;
    RegVal = UTMIP_REG_RD(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0);
    RegVal = RegVal |  UTIMIP_OP_I_SRC_EN;
    UTMIP_REG_WR(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0, RegVal);
    OrgRegVal = NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus,(DISABLE_PULLUP_DP | DISABLE_PULLUP_DM
       | DISABLE_PULLDN_DP | FORCE_PULLDN_DM));
    NvOsSleepMS(30);
    reg = USB_REG_RD(PORTSC1);
    if(0 == USB_PORTSC_LINE_STATE(reg) ){
        NvOsSleepMS(12);
        /* minimum debounce time is 10mS per TDCD_DBNC */
        reg = USB_REG_RD(PORTSC1);
        if(0 == USB_PORTSC_LINE_STATE(reg) ){
            ret =  NV_TRUE;
            }
        }
    RegVal = RegVal & (~UTIMIP_OP_I_SRC_EN);
    UTMIP_REG_WR(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0, RegVal);
    NVML_FUN_SOC(NvMlUsbfUtmipSetDPDMStatus, (OrgRegVal));
    return ret;
}

void NVML_FUN_SOC(NvMlUsbfDetectCharger, (void))
{
    NvU32 LineState, RegVal;
    NvBool NvCharger;
    NvU32 startTimeMs = NvOsGetTimeMS();
    s_ChargerType = NvOdmUsbChargerType_Dummy;

    // STOP the USB controller
    USB_REG_UPDATE_DEF(USBCMD, RS, STOP);
    // Set the USB mode to the IDLE before reset
    USB_REG_UPDATE_DEF(USBMODE, CM, IDLE);

    // USB charger detection spec
#define TDPSRC_ON_MS 10

    //DCD Timeout Mac Value is 900
    while((NvOsGetTimeMS() - startTimeMs) < 900)
    {
        /*Standard DCD detect for SDP/DCp/CDP */
        if(NVML_FUN_SOC(NvMlUsbfDCDChargerDetect, ()))
        {
            break;
        }
        if(((USB_PORTSC_LINE_DM_SET) | (USB_PORTSC_LINE_DP_SET)) ==
                (NVML_FUN_SOC(NvMlUsbfUtmipGetDPDMStatus, (DISABLE_PULLUP_DP | DISABLE_PULLUP_DM
                  | DISABLE_PULLDN_DP | DISABLE_PULLDN_DM))))
        {
            break;
        }
        NvOsSleepMS(22);
    }

    //Enable charger detection circuit
    UTMIP_REG_WR(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0, 0);

    RegVal = UTMIP_REG_RD(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_OP_SRC_EN, 1, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_OP_SINK_EN, 0, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_ON_SINK_EN, 1, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_ON_SRC_EN, 0, RegVal);
    UTMIP_REG_WR(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0, RegVal);

    // Wait according to USB charging spec.
    NvOsSleepMS(TDPSRC_ON_MS);

    // See if VDP_SRC is looped back on DM.
    RegVal = USBIF_REG_RD(NV_ADDRESS_MAP_USB_BASE, USB_PHY_VBUS_WAKEUP_ID);
    if (USBIF_DRF_VAL(USB_PHY_VBUS_WAKEUP_ID, VDAT_DET_CHG_DET, RegVal) && \
        USBIF_DRF_VAL(USB_PHY_VBUS_WAKEUP_ID, VDAT_DET_STS, RegVal))
    {
        NvOsDebugPrintf("Connected to a USB complaint charger or CDP port\n");
    }
    else
    {
        NvOsDebugPrintf("Connected to a PC HOST or non-compliant or NV_Charger charger\n");
        s_ChargerType = NvOdmUsbChargerType_NonCompliant;
    }

    RegVal = UTMIP_REG_RD(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_OP_SRC_EN, 0, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_OP_SINK_EN, 0, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_ON_SINK_EN, 0, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
        UTMIP_ON_SRC_EN, 0, RegVal);
    UTMIP_REG_WR(NV_ADDRESS_MAP_USB_BASE, BAT_CHRG_CFG0, RegVal);

    if (s_ChargerType == NvOdmUsbChargerType_NonCompliant)
    {
        NvCharger = NVML_FUN_SOC(NvMlPrivDetectNvCharger, ());
        if (NvCharger)
        {
            s_ChargerType = NvOdmUsbChargerType_NVCharger;
            NvOsDebugPrintf("Connected to an NvCharger \n");
        }
        else
        {
            NvOsDebugPrintf("Connected to PC HOST/non-compliant charger \n");
        }
        return;
    }

    // set the controller to device controller mode
    USB_REG_UPDATE_DEF( USBMODE, CM, DEVICE_MODE);

     // RUN the USB controller
    USB_REG_UPDATE_DEF(USBCMD, RS, RUN);
    NvOsWaitUS(20);   //As per spec

    // Differentiate compilant chargers type
    LineState = USB_REG_READ_VAL(PORTSC1, LS);
    switch (LineState)
    {
        case 3:
            NvOsDebugPrintf("Connected to Dedicated charger port \n");
            // STOP the USB controller
            USB_REG_UPDATE_DEF(USBCMD, RS, STOP);
            s_ChargerType = NvOdmUsbChargerType_DCP;
            return;
        case 2:
            NvOsDebugPrintf("Connected to Charging Downstream port \n");
            s_ChargerType = NvOdmUsbChargerType_CDP;
            return;
        default:
            NV_ASSERT(!"Unknown line state.");
    }
}

NvBool
NVML_FUN_SOC(Nv3pTransportUsbfCableConnected, (Nv3pTransportHandle * hTrans,NvU32 instance))
{
    if(!s_UsbfContext.InitializationDone)
    {
        NVML_FUN_SOC(NvMlPrivInit,());
        s_UsbfContext.InitializationDone = NV_TRUE;
        NvOsSleepMS(10);
    }
    return NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt));
}

NvError
NVML_FUN_SOC(Nv3pTransportOpen,( Nv3pTransportHandle *hTrans, NvU32 TimeOutMs))
{
    NvError e = NvSuccess;
    Nv3pTransport *trans;
    NvU64 curTime;
    NvU64 endTimeout = 0;

    trans = &s_Transport;

    /* put the transfer buffer into IRAM. must be aligned to
     * NVBOOT_USB_BUFFER_ALIGNMENT-size.
     */
    trans->ep0Buffer = (NvU8 *)(
            NV_ADDRESS_MAP_DATAMEM_BASE +
            NV3P_TRANSFER_EP0_BUFFER_OFFSET);

    trans->buffer =  (NvU8 *)(
            NV_ADDRESS_MAP_DATAMEM_BASE +
            NV3P_TRANSFER_EP_BULK_BUFFER_OFFSET
            );

    trans->size = NVBOOT_USB_MAX_TXFR_SIZE_BYTES;
    trans->read = trans->buffer;
    trans->remaining = 0;

    /* Buffer used by the control requests */

    if(!s_UsbfContext.InitializationDone)
    {
        NVML_FUN_SOC(NvMlPrivInit,());
        s_pUsbfCtxt->InitializationDone = NV_TRUE;
    }


    // Intialize the USB controller
    if (NVML_FUN_SOC(NvMlPrivHwIntializeController,(s_pUsbfCtxt)) != NvBootError_Success)
        return NvError_ModuleNotPresent;

    if (TimeOutMs != NV_WAIT_INFINITE)
        endTimeout  =  NvOsGetTimeUS() + (((NvU64)TimeOutMs) * 1000);

    if(!IsVBUSOverrideSet)
    {
        USBIF_REG_UPDATE_DEF(NV_ADDRESS_MAP_USB_BASE,
                             USB_PHY_VBUS_SENSORS,
                             B_SESS_VLD_SW_EN,
                             DISABLE);
    }
    while(!NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        NvOsSleepMS(10);
        if (TimeOutMs != NV_WAIT_INFINITE)
        {
            curTime = NvOsGetTimeUS();
            if (curTime >= endTimeout)
            {
                e = NvError_UsbfCableDisConnected;
                goto end;
            }
        }
    }

    s_pUsbfCtxt->EnumerationDone = NV_FALSE;
    if(s_ChargingMode)
    {
        if(!((s_ChargerType == NvOdmUsbChargerType_UsbHost) || \
                (s_ChargerType == NvOdmUsbChargerType_CDP) || \
                (s_ChargerType == NvOdmUsbChargerType_NonCompliant)) )
            goto end;
    }
    // Enumerate Only if Usb or CDP
    //Use Enumeration to differentiate SDP from NonCompliantChargers
    (void)NVML_FUN_SOC(NvBootUsbHandleControlRequest,(trans->ep0Buffer));

end:
    *hTrans = trans;
    return e;
}

void
NVML_FUN_SOC(Nv3pTransportClose,(Nv3pTransportHandle hTrans))
{
    // Clear device address
    USB_REG_WR(PERIODICLISTBASE, USB_DEF_RESET_VAL(PERIODICLISTBASE));
    // Clear setup token, by reading and wrting back the same value
    USB_REG_WR(ENDPTSETUPSTAT, USB_REG_RD(ENDPTSETUPSTAT));
    // Clear endpoint complete status bits.by reading and writing back
    USB_REG_WR(ENDPTCOMPLETE, USB_REG_RD(ENDPTCOMPLETE));

    // STOP the USB controller
    USB_REG_UPDATE_DEF(USBCMD, RS, STOP);
    // Set the USB mode to the IDLE before reset
    USB_REG_UPDATE_DEF(USBMODE, CM, IDLE);

    // Disable all USB interrupts
    USB_REG_WR(USBINTR, USB_DEF_RESET_VAL(USBINTR));
    USB_REG_WR(OTGSC, USB_DEF_RESET_VAL(OTGSC));

    /* deallocate memory */
    s_pUsbDescriptorBuf = 0;
    s_pUsbfCtxt->InitializationDone = NV_FALSE;

    NvOsMemset( hTrans, 0, sizeof(Nv3pTransport) );
}


NvError
NVML_FUN_SOC(
    Nv3pTransportSend,
    (Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags,
    NvU32 TimeOutMs))
{
    NvBootUsbfEpStatus ep_stat = NvBootUsbfEpStatus_NotConfigured;
    NvU32 bytes;
    NvU8 *tmp;
    NvU64 curTime;
    NvU64 endTimeout = 0;

    if(!NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        s_pUsbfCtxt->EnumerationDone = NV_FALSE;
        return NvError_UsbfCableDisConnected;
    }

    (void)NVML_FUN_SOC(NvBootUsbHandleControlRequest,(hTrans->ep0Buffer));

    tmp = data;

    while(length)
    {
        bytes = (length < hTrans->size) ? length : hTrans->size;

        NvOsMemcpy(hTrans->buffer, tmp, bytes);

        NVML_FUN_SOC(NvMlPrivTransmitStart,(hTrans->buffer, bytes));

        if (TimeOutMs != NV_WAIT_INFINITE)
        {
            endTimeout  =  NvOsGetTimeUS() + (((NvU64)TimeOutMs) * 1000);
        }

        do
        {
            ep_stat = NVML_FUN_SOC(NvMlPrivQueryEpStatus,(NvBootUsbfEndPoint_BulkIn));

            if (ep_stat != NvBootUsbfEpStatus_TxfrComplete)
            {
                (void)NVML_FUN_SOC(NvBootUsbHandleControlRequest,(hTrans->ep0Buffer));

                // If TimeOutMs is infinite, wait until transfer completes.
                if (TimeOutMs != NV_WAIT_INFINITE)
                {
                     NvOsWaitUS(1);
                    // Wait for TimeOutMs msec.
                    curTime = NvOsGetTimeUS();
                    if (curTime >= endTimeout)
                    {
                        return NvError_Timeout;
                    }
                }
            }
        } while((ep_stat == NvBootUsbfEpStatus_TxfrActive) || (ep_stat == NvBootUsbfEpStatus_Stalled));

        if (ep_stat != NvBootUsbfEpStatus_TxfrComplete)
        {
            goto fail;
        }

        bytes = NVML_FUN_SOC(NvMlPrivGetBytesTransmitted,());
        length -= bytes;
        tmp += bytes;
    }

    return NvSuccess;

fail:
    /* stall the connection if the cable is still connected */
    if (NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        NVML_FUN_SOC(NvMlPrivEpSetStalledState,(NvBootUsbfEndPoint_BulkOut, NV_TRUE));
        return NvError_UsbfEpStalled;
    }
    else
    {
        s_pUsbfCtxt->EnumerationDone = NV_FALSE;
        return NvError_UsbfCableDisConnected;
    }
}

NvError
NVML_FUN_SOC(
    Nv3pTransportReceive,
    (Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags,
    NvU32 TimeOutMs))
{
    NvBootError e;
    NvBootUsbfEpStatus ep_stat = NvBootUsbfEpStatus_NotConfigured;
    NvU32 bytes;
    NvU8 *tmp;
    NvU32 maxPacketSize;
    NvU64 curTime;
    NvU64 endTimeout = 0;

    NvOsFlushWriteCombineBuffer();
    if(!NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        s_pUsbfCtxt->EnumerationDone = NV_FALSE;
        return NvError_UsbfCableDisConnected;
    }

    tmp = data;
    (void)NVML_FUN_SOC(NvBootUsbHandleControlRequest,(hTrans->ep0Buffer));

    if(received)
    {
        *received = 0;
    }

    /* usb receive seems to fail if buffer length is small, do some read
     * buffering to compensate.
     */

    if(hTrans->remaining)
    {
        bytes = (hTrans->remaining > length) ? length : hTrans->remaining;
        NvOsMemcpy( tmp, hTrans->read, bytes );
        tmp += bytes;
        hTrans->read += bytes;
        hTrans->remaining -= bytes;

        if(received)
        {
            *received += bytes;
        }

        if( hTrans->remaining )
        {
            return NvSuccess;
        }

        hTrans->read = hTrans->buffer;
        length -= bytes;
    }

    maxPacketSize = NVML_FUN_SOC(NvMlPrivHwGetPacketSize,(s_pUsbfCtxt, USB_EP_BULK_OUT));

    while(length)
    {
        NvU32 transferSize = hTrans->size;

        //Conditions for USB transfer termination:
        //A) You receive exactly as much data as you asked for.
        //B) Short packets (ie,data received is a non-multiple of maxPacketSize
        //C) A zero length packet
        //Note: Neither Fastboot nor Nv3p use ZLTs.
        //When the data Fastboot or Nv3p sends is a multiple
        //of maxPacketSize, we MUST request exactly that many bytes. Failing
        //to do so will cause the USB to hang as no termination condition
        //will be met (Look at conditions above). Example: If Fastboot/Nv3p
        //sends 2k bytes we must receive EXACTLY 2k bytes (not 4k bytes or
        //anything else).
        //Problem 2) Due to it's bytestream nature, Nv3p protocol can
        //potentially ask for fewer bytes than you actually receive.
        //This will cause the USB to hang due to buffer overrun.
        //We can resolve this by asking for a multiple
        //of maxPacketSize that is greater than what Nv3p actually wants.
        //What this means is we need to satisfy these constraints:
        //A) Exact data size requests for multiples of maxPacketSize
        //B) Larger than requested data size requests for everything else.
        //The if condition below should account for both.
        if ((length <= hTrans->size) && (length % maxPacketSize == 0))
        {
            transferSize = length;
        }
        do
        {
            /*
             * if tx size > 4KB and tmp buffer is aligned use tmp as txBuffer directly
             * else use usb buffers for transfer and then copy usb buffers to tmp
             */
            if ((transferSize >= 4096) && (((int)tmp & 0xFFF) == 0))
            {
                NVML_FUN_SOC(NvMlPrivReceiveStart,(tmp, transferSize));
            }
            else
            {
                NVML_FUN_SOC(NvMlPrivReceiveStart,(hTrans->buffer, transferSize));
            }
            if (TimeOutMs != NV_WAIT_INFINITE)
            {
                endTimeout  =  NvOsGetTimeUS() + (((NvU64)TimeOutMs) * 1000);
            }

            if (!TimeOutMs)
            {
                return NvSuccess;
            }

            do
            {
                ep_stat = NVML_FUN_SOC(NvMlPrivQueryEpStatus,(NvBootUsbfEndPoint_BulkOut));
                if (ep_stat != NvBootUsbfEpStatus_TxfrComplete)
                {
                    e = NVML_FUN_SOC(NvBootUsbHandleControlRequest,(hTrans->ep0Buffer));
                    if (e == NvBootError_CableNotConnected)
                    {
                        s_pUsbfCtxt->EnumerationDone = NV_FALSE;
                        return NvError_UsbfCableDisConnected;
                    }

                    // If TimeOutMs is not infinite, wait for time "TimeOutMs"
                    // to complete the transfer.
                    if (TimeOutMs != NV_WAIT_INFINITE)
                    {
                         NvOsWaitUS(1);
                        // Wait for TimeOutMs msec.
                        curTime = NvOsGetTimeUS();
                        if (curTime >= endTimeout)
                        {
                            return NvError_Timeout;
                        }
                    }
                }
            } while((ep_stat == NvBootUsbfEpStatus_TxfrActive ) || (ep_stat == NvBootUsbfEpStatus_Stalled));

            /* Looks like the endpoint is reset - retrying */
        } while (ep_stat != NvBootUsbfEpStatus_TxfrComplete);

        if (ep_stat != NvBootUsbfEpStatus_TxfrComplete)
        {
            goto fail;
        }

        bytes = NVML_FUN_SOC(NvMlPrivGetBytesReceived,());
        if (bytes <= length)
        {
            hTrans->remaining = 0;
        }
        else
        {
            hTrans->remaining = bytes - length;
            bytes = length;
        }

        hTrans->read = hTrans->buffer + bytes;
        if (!((transferSize >= 4096) && (((int)tmp & 0xFFF) == 0)))
        {
            NvOsMemcpy(tmp, hTrans->buffer, bytes);
        }
        if (received)
        {
            *received += bytes;
        }

        //If the user wasn't sure how many bytes to receive,
        //abort after the first transfer is complete.
        // Or If bytes received is less than transfer size, return as transfer complete
        if ((length == NV3P_UNKNOWN_LENGTH) || (bytes < hTrans->size))
            break;

        length -= bytes;
        tmp += bytes;

    }
    return NvSuccess;

fail:
    /* stall the connection if the cable is still connected */
    if (NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        NVML_FUN_SOC(NvMlPrivEpSetStalledState,(NvBootUsbfEndPoint_BulkOut, NV_TRUE));
        return NvError_UsbfEpStalled;
    }
    else
    {
        s_pUsbfCtxt->EnumerationDone = NV_FALSE;
        return NvError_UsbfCableDisConnected;
    }
}

NvError
NVML_FUN_SOC(
    Nv3pTransportReceiveIfComplete,
    (Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received))
{
    NvError e = NvSuccess;
    NvU32 bytes = 0;

    if(!NVML_FUN_SOC(NvMlPrivUsbfIsCableConnected,(s_pUsbfCtxt)))
    {
        s_pUsbfCtxt->EnumerationDone = NV_FALSE;
        e = NvError_UsbfCableDisConnected;
        goto fail;
    }
    if(received)
    {
        *received = 0;
    }

    bytes = NVML_FUN_SOC(NvMlPrivGetBytesReceived,());
    if (bytes)
    {
        if (bytes <= length)
        {
            hTrans->remaining = 0;
        }
        else
        {
            hTrans->remaining = bytes - length;
            bytes = length;
        }

        hTrans->read = hTrans->buffer + bytes;
        NvOsMemcpy(data, hTrans->buffer, bytes);

        if (received)
        {
            *received += bytes;
        }
    }
    else
    {
        (void)NVML_FUN_SOC(NvBootUsbHandleControlRequest,(hTrans->ep0Buffer));
    }

fail:
    return e;
}

void NVML_FUN_SOC(NvMlUsbfSetBSessSwOverride,(void))
{
    NvU32 UsbBase = NV_ADDRESS_MAP_USB_BASE;
    USBIF_REG_UPDATE_DEF(UsbBase,
                         USB_PHY_VBUS_SENSORS,
                         B_SESS_VLD_SW_VALUE,
                         SET);
    USBIF_REG_UPDATE_DEF(UsbBase,
                         USB_PHY_VBUS_SENSORS,
                         B_SESS_VLD_SW_EN,
                         ENABLE);
}

void NVML_FUN_SOC(NvMlUsbfSetVbusOverride,(NvBool Val))
{
    NvU32 UsbBase = NV_ADDRESS_MAP_USB_BASE;
    USBIF_REG_UPDATE_DEF(UsbBase,
                         USB_PHY_VBUS_SENSORS,
                         B_SESS_VLD_SW_EN,
                         ENABLE);
    if(Val)
    {
        USBIF_REG_UPDATE_DEF(UsbBase,
                             USB_PHY_VBUS_SENSORS,
                             B_SESS_VLD_SW_VALUE,
                             SET);
    }
    else
    {
            USBIF_REG_UPDATE_DEF(UsbBase,
                             USB_PHY_VBUS_SENSORS,
                             B_SESS_VLD_SW_VALUE,
                             UNSET);

    }
    IsVBUSOverrideSet = NV_TRUE;
}

void NVML_FUN_SOC(NvMlUsbfDisableInterrupts,(void))
{
    if(NULL == s_pUsbfCtxt)
    {
        s_pUsbfCtxt = &s_UsbfContext;
        NVML_FUN_SOC(NvMlPrivInitBaseAddress,(s_pUsbfCtxt));
    }

    /* Disable USB interrupts */
    USB_REG_WR(USBINTR, 0);
    USB_REG_WR(OTGSC, 0);
}

/*
 * Copyright (c) 2007 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK USB F private functions</b>
 *
 * @b Description: Defines USB functuion controller private functions
 *
 */

#ifndef INCLUDED_NVDDK_USBF_PRIV_H
#define INCLUDED_NVDDK_USBF_PRIV_H

#include "nvddk_usbf.h"
#include "nvrm_gpio.h"
#include "nvrm_interrupt.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_usbulpi.h"
#include "nvrm_memmgr.h"
#include "nvddk_usbphy.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * Max USB buffer size that can be txfered in one shot = 64 Kilo Bytes
 */
enum {USB_MAX_TXFR_SIZE_BYTES = 0x10000};
/* maximum H/W buffers pointers in the descriptors */
enum {USB_MAX_BUFFER_PTRS = 5};

/**
 * Number of supported USB endpoints
 */
#define USB_MAX_EP_COUNT    32
#define USB_MAX_DTDS_PER_EP 4
#define USB_MAX_DTDS(_ctx_) (USB_MAX_DTDS_PER_EP * (_ctx_)->UsbfEndpointCount)

/*
 * Size of data buffers = 4 Kilo Bytes.
 * This is a controller-defined value and should not be changed.
 */
enum {USB_MAX_MEM_BUF_SIZE = 0x1000};

/* maximum mem buffers for DTD is 4 */
enum {USB_MAX_MEM_BUF_PER_DTD = 4};

/*
 * Maximum buffer capacity of the each DTD is 16 Kilo Bytes
 */
enum {USB_MAX_DTD_BUF_SIZE = (USB_MAX_MEM_BUF_SIZE * USB_MAX_MEM_BUF_PER_DTD)};

/*
 * Size of USB buffer size is 2 * 64 Kbytes.
 * Allocating two buffers, if there is any simultaneous transfers for IN and OUT
 * at maximum.
 */
enum {USB_MAX_USB_BUF_SIZE = (USB_MAX_TXFR_SIZE_BYTES*2)};

/* Maximum Memory buffer counter */
enum {USB_MAX_MEM_BUF_COUNT = (USB_MAX_USB_BUF_SIZE/USB_MAX_MEM_BUF_SIZE)};

// Mask for ALL end points in the controller
static const NvU32 s_USB_FLUSH_ALL_EPS = 0xFFFFFFFF;

// Wait time for controller H/W status to change before giving up.
#define CONTROLLER_HW_TIMEOUT_US  1000   // 1 Second

/* Maximum wait time for transfers on IN/OUT endpoint = 1 Second*/
#define USB_MAX_TXFR_WAIT_TIME_MS  1000

// USB HW buffers alignment must be 4096 bytes
#define USB_HW_BUFFER_ALIGNMENT 0x1000

// Maximum number of interupts that USB can Handle
#define MAX_USBF_IRQS 1

// USB Setup Packet size
#define USB_SETUP_PKT_SIZE  8

// Minimum system frequency required for USB to work in High speed mode
#define USB_HW_MIN_SYSTEM_FREQ_KH   100000

// status stage wait time 50 ms as per USB2.0
#define USB_ACK_WAIT_TIME_MS   50

/**
 *  These are simple min function, which return
 *  the minimum of a and b for any type defined
 *  over the '<' operator.
 */
#define USB_GET_MIN(a, b) ((a)<(b)?(a):(b))

/**
 *  These are simple max function, which return
 *  the maximum of a and b for any type defined
 *  over the '<' operator.
 */
#define USB_GET_MAX(a, b) ((a)<(b)?(b):(a))


/* Defines for USB endpoint masks */
enum
{
    // USB EP Out mask for finding the OUT endpoint
    USB_EP_OUT_MASK = 0x00000001,
    // USB EP In mask for finding the IN endpoint
    USB_EP_IN_MASK = 0x00010000,
};

/* Defines for the USB Packet size */
enum
{
    // usb packet size in Full speed = 64 bytes
    USB_FULL_SPEED_PKT_SIZE_BYTES = 0x40,
    // usb packet size in High speed = 512 bytes
    USB_HIGH_SPEED_PKT_SIZE_BYTES = 0x200
};

/* Defines the USB Line state */
typedef enum
{
    UsbLineStateType_SE0 = 0,
    UsbLineStateType_SJ = 1,
    UsbLineStateType_SK = 2,
    UsbLineStateType_SE1 = 3,

    UsbLineStateType_Force32 = 0x7FFFFFFF,
} UsbLineStateType;


/**
 * Defines for controller endpoint number. This define is mapping to the
 * USB controller endpoints. Each endpoint defined in the controller
 * supports endpoint IN and endpoint out transfers. These IN and OUT
 * endpoints can be configured to control, bulk, isoc or interrupt endpoints.
 * Control OUT and IN are mapped to endpoint0 in the H/W
 */

#define USB_EP_CTRL_OUT 0
#define USB_EP_CTRL_IN  1
#define USB_EP_MISC     2

// Define for finding the endpoint is IN endpoint
#define USB_IS_EP_IN(Endpoint) (((Endpoint & 1) != 0))

/**
 * @brief Defines the USB Port States.
 */
typedef enum UsbPortSpeed_t
{
    // Defines the port Full speed
    UsbPortSpeedType_Full,
    // Defines the port full speed
    UsbPortSpeedType_Low,
    // Defines the port high speed
    UsbPortSpeedType_High,

    UsbPortSpeedType_Force32 = 0x7FFFFFFF,
} UsbPortSpeedType;

/**
 * @brief Defines the USB Internal queue head size
 * Note: Arc controller defines queue head to be 64 bytes but
 *       controller does not use the last 16 bytes in the registers.
 */
enum {USB_HW_QUEUE_HEAD_SIZE_BYTES = 48};

/*
 * @brief Defines USB logical interrupts aand its corresponding ISR handler
 */
typedef struct UsbInterruptsRec
{
    // Logical interupt struct
    NvRmLogicalIntr LogicalIntr;
    // Pointer to the ISR handler to be called for the logical interrupt
    NvOsInterruptHandler IntrHandler;
} UsbInterrupts;

/**
 * @brief Structure defining the fields for device transfer descriptor
 * (32 byte structure).
 */
typedef struct UsbDevTransDescRec
{
    // Next Link pointer
    volatile NvU32 NextDtd;
    // Stores Status, Interrupt on complete and bytes to go field.
    volatile NvU32 DtdToken;
    // Memory buffer pointers.
    volatile NvU32 BufPtrs[USB_MAX_BUFFER_PTRS];
    // Reserved
    volatile NvU32 Reserved;
}UsbDevTransDesc;

/**
 * @brief Structure defining the fields for device queue head descriptor
 * (64 byte structure)
 */
typedef struct UsbDevQueueHeadRec
{
    // EP capabilities
    NvU32 EpCapabilities;
    // current DTD pointer
    NvU32 CurrentDTDPtr;
    // Next DTD pointer
    NvU32 NextDTDPtr;
    // Stores Status, Interrupt on complete and bytes to go fields
    NvU32 DtdToken;
    // Buffer Pointers
    NvU32 BufferPtrs[USB_MAX_BUFFER_PTRS];
    // reserved
    NvU32 Reserved;
    // Setup Packet Buffer (0 - 3 bytes)
    NvU32 setupBuffer0;
    // Setup Packet Buffer (4 - 7 bytes)
    NvU32 setupBuffer1;
    // reserved
    NvU32 Reserved0;
    // reserved
    NvU32 Reserved1;
    // Reserved
    NvU32 Reserved2;
    // Reserved
    NvU32 Reserved3;
}UsbDevQueueHead;

/**
 * @brief Defines the USB data Store consiting of
 * Qhead and Device transfer decriptors buffer
 */
typedef struct UsbDescriptorDataRec
{
    // queue head pointer
    volatile UsbDevQueueHead *pQHeads;
    // device transfer decriptor pointer.
    UsbDevTransDesc *pDataTransDesc;
    // This variable is used to indicate EP is configured or not
    NvU32 EpConfigured[USB_MAX_EP_COUNT];
    // This variable is used for storing the bytes requested
    NvU32 BytesRequestedForEp[USB_MAX_EP_COUNT];
} UsbDescriptorData;

// Usb function record definition
typedef struct NvDdkUsbFunctionRec
{
    NvRmDeviceHandle hRmDevice;
    volatile NvU32 *UsbOtgVirAdr;
    NvU32 UsbOtgBankSize;
    NvU32  RefCount;
    volatile NvBool PortChangeDetected;
    volatile NvBool UsbResetDetected;
    volatile NvBool SetupPacketDetected;
    // This variable is used to indicate error status, if there is any error
    volatile NvBool EpTxfrError[USB_MAX_EP_COUNT];
    volatile NvBool EpTxferCompleted[USB_MAX_EP_COUNT];
    volatile NvBool SignalTxferSema[USB_MAX_EP_COUNT];
    // used to handle bus resets after the controller start up
    volatile NvBool ControllerStarted;
    // holds the port suspend status
    volatile NvBool PortSuspend;
    // indicates Reset notification requirement for client
    volatile NvBool NotifyReset;
    // USB data store consists of Q head and device transfer descriptors.
    UsbDescriptorData UsbDescriptorBuf;
    // USB data store consists of Q head and device transfer descriptors.
    NvU32 UsbTxferDescriptorPhyBufAddr;
    // Q head physical address
    NvU32 QHeadsPhyAddr;
    // USB transfer buffer used for IN and Out transactions
    NvU8 *pUsbXferBuf;
    // USB transfer buffer used for IN and Out transactions
    NvRmPhysAddr pUsbXferPhyBuf;
    // USB transfer buffer to DTD conversion table.
    UsbDevTransDesc *pUsbXferBufForDtd[USB_MAX_MEM_BUF_COUNT];
    // handle to the USB memory
    NvRmMemHandle  hUsbBuffMem;
    // controller endpoint configuration
    NvDdkUsbfEndpoint UsbfEndpoint[USB_MAX_EP_COUNT];
    // Endpont count curently configured
    NvU32 UsbfEndpointCount;
    // Usb port speed
    volatile UsbPortSpeedType UsbPortSpeed;
    // Holds USB Cable Connect status
    volatile NvBool UsbfCableConnected;
    volatile NvOdmUsbChargerType UsbfChargerType;
    // Internal buffer size
    NvU32 UsbMaxTransferSize;
    // Client semaphore
    NvOsSemaphoreHandle hClientSema;
    // Setup packet data if USB_SETUP_PKT set in event field.
    volatile NvU8 setupPkt[USB_SETUP_PKT_SIZE];
    // Holds the logical interrupt list
    UsbInterrupts UsbfLogicalIntList[MAX_USBF_IRQS];
    // Holds the virtual buffer address created for USB operations
    NvU8 * UsbVirtualBuffer;
    // Holds the virtual buffer size created for USB function operation
    NvU32 VirtualBufferSize;
    volatile NvBool CheckForCharger;
    volatile NvBool ChargerDetected;
    UsbLineStateType LineState;
    // ISR / Suspend mutex
    NvOsIntrMutexHandle hIntrMutex;
    NvOsInterruptHandle InterruptHandle;
    NvOsSemaphoreHandle hTxferSema;
    volatile NvBool IsUsbSuspend;
    NvU32 Instance;
    NvOdmUsbInterfaceType UsbInterfaceType;
    NvBool DummyChargerSupported;
    NvDdkUsbPhyHandle hUsbPhy;
} NvDdkUsbFunction;


/***********************************************************
 ************* definitions for private functions ***********
 ***********************************************************/

// Allocates the buffers required for the USB
NvError // returns the error status
UsbPrivAllocateBuffers(NvDdkUsbFunction *pUsbFuncCtxt,
                       NvRmPhysAddr OtgPhysAddr);

// Releases the buffers allocated for the USB
void
UsbPrivDeAllocateBuffers(
    NvDdkUsbFunction *pUsbFuncCtxt);// pointer to the USB function context

// Configures the Endpoints supportable by the controller
void
UsbPrivConfigureEndpoints(
    NvDdkUsbFunction *pUsbFuncCtxt);// pointer to the USB function context

// Initializes the controller
NvError // returns the error status
UsbPrivIntializeController(
    NvDdkUsbFunction *pUsbFuncCtxt);// pointer to the USB function context

// Initalizes the USB controller with endpoints configuration
void
UsbPrivInitalizeEndpoints(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    const NvDdkUsbfEndpoint *endPointInfo, // endpoint info structure
    NvBool config); // Boolean to Indicate configure/un-configure the endpoint

// Trnamits the data on OUT endpoint
NvError // returns the error status
UsbPrivEpTransmit(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 endPoint, // Out endpoint number
    NvU8* bufferPtr, // physical buffer containing data to be transfered
    NvU32 *pBytesToTransfer, // pointer to the bytes to be transfered
    NvBool zeroTerminate,  // Indicates this is last transaction on this endpoint
    NvU32 WiatTimeoutMs);

// Clears all the pending transfers on the end point
NvError // returns the error status
UsbPrivTxfrClearAll (
    NvDdkUsbFunction *pUsbFuncCtxt);
// Clears the any pending transfers on the end point and frees the DTD
NvError // returns the error status
UsbPrivTxfrClear(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    NvU32 endPoint);// endpoint number for which active transfer to be cancled

// Starts the transfer, this is asyncronus call and can wait for interupt
// to know the Txfr complete statuas and read the data
void
UsbPrivTxfrStart(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 endPoint, // endpoint number on wich Txfr needs to initiate
    NvU32 maxTxfrBytes, // number of bytes for the Txfr
    NvU32 PhyBuffer,
    NvBool SignalSema,
    NvBool EnableZlt,
    NvBool EnableIntr);

// waits on the endpoint till the transfer completes are timeout
NvError // returns the error status
UsbPrivTxfrWait(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 endPoint, // endpoint number top wiat on
    NvU32 timeoutMs); // time out for the wait

// returns the endpoint curent status any time.
NvError // returns the error status
UsbPrivEpGetStatus(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 endPoint);// endpoint number

// set/unset the endpoint stalled state
void
UsbPrivEpSetStalledState(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    NvU32 endPoint, // endpoint number to change the stall state
    NvBool stallState); // boolen to set stall or unstall the endpoint

// Handles the BUS reset situation
void
UsbPrivStatusClearOnReset(
    NvDdkUsbFunction *pUsbFuncCtxt);// pointer to the USB function context

// flushes the endpoint to clear the endpoint complete
NvError // returns the error status
UsbPrivEndPointFlush(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 endPoint);// endpouint number to be flushed

// gets the cable status or wehter Bsession is valid or not
NvBool // returns NV_TRUE if cable is connected
UsbPrivBSessionValid(
    NvDdkUsbFunction *pUsbFuncCtxt); // pointer to the USB function context

// Recives the data on specified endpoint over USB BUS
NvError // returns the error status
UsbPrivReceive(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    NvU32 endPoint, // out endpoint on which we need to recieve the data
    NvU8* dataBuf, // physical address of the data buffer pointer
    NvU32* rcvdBytes, // bytes to be recived on this endpoint
    NvBool enableShortPacketTermination,// to endicate if there is any shortpacket
    NvU32 WiatTimeoutMs);

// This will send the 0 byte acknowledgement on the spwecified endpoint
// mostly on control endpoints
NvError // returns the error status
UsbPrivAckEndpoint(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    NvU32 endPoint);// endpoint number that needs ack

// This will set the USB device address in the controller hardware
void
UsbPrivSetAddress(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    NvU32 Address); // USB device address sent by the host

// Handles the USB ISR function
void
UsbPrivISR(void *arg);

// Registers the USB interupts
NvError // returns the error status
UsbPrivRegisterInterrupts(
    NvDdkUsbFunction *pUsbFuncCtxt);// pointer to the USB function context

/*
 * Gets the USB virtual buffer address for the USB Txfer Buffer
 */
NvU8*
UsbPrivGetVirtualBufferPtr(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvRmPhysAddr  phyAddress);  // physical address of the Txfer buffer

/*
 * Gets the free memeroy block address index for USB data transfers.
 * returns -1 if all USB buffers are already occupied
 */
NvU32 // memeory block index
UsbPrivGetFreeMemoryBlock(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 BytesRequired, // minimum size bytes required for transfer
    NvU32 EndPoint);

/*
 * Locks the buffer that is allocated by the Free memory block
 */
void
UsbPrivTxfrLockBuffer(
    NvDdkUsbFunction *pUsbFuncCtxt, // pointer to the USB function context
    NvU32 EndPoint, // endpoint number
    NvU32 MemBufferAdr, // Memory buffer address
    NvU32 SizeBytes); // number of bytes to be locked

/*
 * Unlocks the buffer that is locked for endpoint transfers.
 */
void
UsbPrivTxfrUnLockBuffer(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 EndPoint);

/*
 * Gets the USB packet size for the endpoint based on the speed
 */
NvU32 // returns packet size
UsbPrivGetPacketSize(
    NvDdkUsbFunction *pUsbFuncCtxt,// pointer to the USB function context
    NvU32 EndPoint); // endpoint number

/*
.* Sets the specified test mode
 */
void
UsbPrivSetTestMode(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvU32 TestModeSelector);

/*
 * Gets the number of endpoints supported by the controller.
 */
NvU32
UsbfPrivGetEndpointCount(
    NvDdkUsbFunction *pUsbFuncCtxt);


/*
 * Returns the USB cable connection status
 */
NvBool
UsbfPrivIsCableConnected(
    NvDdkUsbFunction *pUsbFuncCtxt);

/*
 * Configures the dedicated charger detection logic
 */
void
UsbfPrivConfigureChargerDetection(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvBool EnableDetection,
    NvBool EnableInterrupt);


/*
 * Returns dedicated charger connection status
 */
NvBool
UsbfPrivIsChargerDetected(
    NvDdkUsbFunction *pUsbFuncCtxt);

/*
 * Configures the VBus Interrupt
 */
void
UsbfPrivConfigVbusInterrupt(
    NvDdkUsbFunction *pUsbFuncCtxt,
    NvBool Enable);

/*
 * Disables the controller Interrupts
 */
void
UsbfPrivDisableControllerInterrupts(
    NvDdkUsbFunction *pUsbFuncCtxt);


#if defined(__cplusplus)
}
#endif

/** @}*/
#endif // INCLUDED_NVDDK_NAND_H


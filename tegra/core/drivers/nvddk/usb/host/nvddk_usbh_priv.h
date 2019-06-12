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
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK USB Host  private functions</b>
 *
 * @b Description: Defines USB Host  controller private functions
 *
 */

#ifndef INCLUDED_NVDDK_USBH_PRIV_H
#define INCLUDED_NVDDK_USBH_PRIV_H

#include "nvddk_usbh.h"
#include "nvrm_gpio.h"
#include "nvrm_interrupt.h"
#include "nvrm_drf.h"
#include "nvodm_query.h"
#include "nvodm_query_discovery.h"
#include "nvddk_usbphy.h"


/// ULPI instance is 1
#define ULPI_INSTANCE 1

// UTMI instance is 0
#define UTMI_INSTANCE 0

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    UsbhRegId_HCCPARAMS,
    UsbhRegId_USBCMD,
    UsbhRegId_USBSTS,
    UsbhRegId_USBINTR,
    UsbhRegId_ASYNCLISTADDR,
    UsbhRegId_PORTSC1,
    UsbhRegId_OTGSC,
    UsbhRegId_USBMODE,
    UsbhRegId_HCSPARAMS,
    UsbhRegId_Num,
    // Ignore -- Forces compilers to make 32-bit enums.
    UsbhRegId_Force32 = 0x7FFFFFFF
} UsbhRegId;

/*
 * Max USB buffer size that can be txfered in one shot = 64 Kilo Bytes
 */
enum {USB_MAX_TXFR_SIZE_BYTES = 0x10000};

/* maximum H/W buffers pointers in the descriptors */
enum {USB_MAX_BUFFER_PTRS = 5};

/*
 * Number of supported USB endpoints
 */
enum {USB_MAX_EP_COUNT = 8};

/* maximum DTDs possible based on the configuration */
enum {USB_MAX_DTDS_PER_EP = 4};

/* maximum DTDs possible based on the configuration */
enum {USB_MAX_DTDS = (USB_MAX_EP_COUNT * USB_MAX_DTDS_PER_EP)};

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

// Wait time for controller H/W status to change before giving up.
#define CONTROLLER_HW_TIMEOUT_US  1000000   // 1 second
// USB HW buffers alignment must be 4096 bytes
#define USB_HW_BUFFER_ALIGNMENT 0x1000


// Minimum system frequency required for USB to work in High speed mode
#define USB_HW_MIN_SYSTEM_FREQ_KH   100000

/*
.*                   PIPE  Defination
 *  - direction:    0th word(1 = Host-to-Device [Out], 2 = Device-to-Host  [In])
 *  - pipe type:   1st word(1 = control, 2 = isochronous, 3 = bulk, 01 = interrupt)
 *  - endpoint Nuber:   2nd word
 *  - device address:   3rd  word
 *
 */

/*
 * @brief:These are the macros to create complete pipe for perticular EP with direction
 * @peram
 * devNumber: This is the device number This is used to create
 *           pipe for that perticular device
 * endpoint:  Represents the endpoint number for which pipe need to be created.
 */

// This macro  will be doing  pipe creation with out direction
#define CREATE_PIPE(devAddr, epNumber, epType, direction)  \
    (((devAddr) << 24) | ((epNumber) << 16) | ((epType) << 8) | (direction))

/* Macros to  extract information from given PIPE */

// If given pipe's direction is IN, returns TRUE otherwise FALSE
#define IS_PIPEIN(pipe) (((pipe) & 0x3) == NvDdkUsbhDir_In)

// If given pipe's direction is OUT, returns TRUE otherwise FALSE
#define IS_PIPEOUT(pipe)   (((pipe) & 0x3) == NvDdkUsbhDir_Out)

// Gives the type of the pipe
#define USB_PIPETYPE(pipe)  (((pipe) >> 8) & 0x7)

// This macro will returns TRUE if given pipe is Control endpoit.
#define IS_PIPE_CONTROL(pipe)   ((USB_PIPETYPE(pipe)) == NvDdkUsbhPipeType_Control)

// This macro will return TRUE, if given pipe is Isochronous endpoit.
#define IS_PIPE_ISOC(pipe)  ((USB_PIPETYPE(pipe)) == NvDdkUsbhPipeType_Isoc)

// This macro will returns TRUE if given pipe is Bulk endpoit.
#define IS_PIPE_BULK(pipe) ((USB_PIPETYPE(pipe)) == NvDdkUsbhPipeType_Bulk)

// This macro will return TRUE, if given pipe is Interrupt endpoit.
#define IS_PIPE_INT(pipe)   ((USB_PIPETYPE(pipe)) == NvDdkUsbhPipeType_Interrupt)

// Returns the endpoint number of the given pipe
#define USB_PIPEENDPOINT(pipe)  (((pipe) >> 16) & 0xF)

// Returns the device Address of the given pipe
#define USB_PIPEDEVICE(pipe)    (((pipe) >> 24) & 0x7F)

typedef struct
{
    NvU32 ChipId;

    NvBool IsPortSpeedFromPortSC;
}NvDdkUsbhCapabilities;

typedef struct UsbhTdRec
{
    // Next DTD pointer
    volatile NvU32 NextDTDPtr;

    // Next DTD pointer
    volatile NvU32 AltNextDTDPtr;
    // Stores  Total bytes to tranfer, Status, Interrupt on complete and bytes to go fields
    volatile NvU32 DtdToken;
    // Buffer Pointers
    volatile NvU32 BufferPtrs[USB_MAX_BUFFER_PTRS];
}UsbhTd;

typedef struct UsbHcdTdRec
{
    // Actual Q Td structure.
    UsbhTd *pHcTd;

    // Transfer pointer represents the  for which tarnsfer this  TD has been  allocated  */
    NvDdkUsbhTransfer *pTransfer;

    // Bytes required to transfer in this  QTD.
    NvU32 bytesToTransfer;

    // Buffer pointer allocated to this TD
    NvU8 *pBuffer;

    // Length of the buffer allocated for this TD
    // Next HcdTD allocated for same tranfer
    struct UsbHcdTdRec *pNxtHcdTd;

    volatile NvU32 Used;
}UsbHcdTd;

typedef struct UsbhQhRec
{
    volatile NvU32 QhHLinkPtr;

    volatile NvU32 EpCapChar0;

    volatile NvU32 EpCapChar1;

    // Current DTD pointer
    volatile NvU32 CurrentDTDPtr;
    // Next DTD pointer
    volatile NvU32 NextDTDPtr;

    // Next DTD pointer
    volatile NvU32 AltNextDTDPtr;
    // Stores Status, Interrupt on complete and bytes to go fields
    volatile NvU32 DtdToken;
    // Buffer Pointers
    volatile NvU32 BufferPtrs[USB_MAX_BUFFER_PTRS];

    // Reserved
    volatile NvU32 Reserved0;
    // Reserved
    volatile NvU32 Reserved1;
    // Reserved
    volatile NvU32 Reserved2;
    // Reserved
    volatile NvU32 Reserved3;
}UsbhQh;

// HC Driver Queue Head struture to manage  Actual QHs
typedef struct UsbHcdQhRec
{
    UsbhQh *pHcQh;

    volatile NvU32 Pipe;

    volatile NvU32 Used;

    UsbHcdTd* Td_Head;
}UsbHcdQh;

// Host Controller Driver  Structure
typedef struct NvDdkUsbHcdRec
{
    // RM device handle
    NvRmDeviceHandle hRmDevice;
    // USB Host Instance 
    NvU32 Instance;
    // Holds the virtual address for accessing registers.
    NvU32 *UsbOtgVirAdr;
    // Holds the register map size.
    NvU32 UsbOtgBankSize;
    // Usb Port Speed
    NvDdkUsbPortSpeedType UsbPortSpeed;

    // Used to handle bus resets after the controller start up
    volatile NvBool ControllerStarted;
    // Holds USB Cable Connect status
    volatile NvBool UsbhCableConnected;
    // Holds the Usb Cable disconnect status
    volatile NvBool UsbhCableDisConnected;
    // After  Reset and enabling the port, HC sets the Port Enable to true
    volatile NvBool PortEnabled;
    // Represents the transfer completion Event
    volatile NvBool TranferDone;
    // is suspended
    volatile NvBool IsUsbSuspend;
    // Flag to represent  Address 0 control endpoint has been added or not
    NvBool Addr0ControlPipeAdded;

    // Pointer to the Head of Transfers list linked  to Async list
    NvDdkUsbhTransfer *pTransferListHead;

    // Pointer to the Head of Transfers which are done with tranfer
    NvDdkUsbhTransfer *pCompeltedTransferListHead;

    // Client semaphore
    NvOsSemaphoreHandle EventSema;

    // Handle to the RM gpio Handle
    NvRmGpioHandle hUsbHcdGpioHandle;
    // GPIO pin to detect cable ID.
    NvRmGpioPinHandle CableIdGpioPin;

    // Holds the virtual buffer size created for USB Host Driver
    NvU32 VirtualBufferSize;
    // Holds the virtual buffer address created for USB operations
    NvU8 * UsbVirtualBuffer;

    // Handle to the USB memory
    NvRmMemHandle  hUsbBuffMemHandle;

    // USB transfer buffer used for IN and Out transactions
    NvRmPhysAddr UsbXferPhyBuf;
    // USB transfer buffer (physically mapped Virtual address)used for IN and Out transactions
    NvU8 *pUsbXferBuf;
    // Pointer to Buffer Index used to manage buffer is in use of not
    NvU8 *pUsbXferBufIndex;

    // Actual Q head physical address
    NvU32 UsbDevHcQhPhyAddr;
    // HC Q head virtual address
    UsbhQh *pUsbDevHcQhAddr;

    // Actual TD phy address
    NvU32 UsbDevHcTdPhyAddr;
    // Actual TD virtual address
    UsbhTd *pUsbDevHcTdAddr;

    // HcdQh Phycical address for Hcd Qh array
    NvU32  UsbHcdQhPhyAddr;
    // Pointer to hold virtual address for HcdQh array
    UsbHcdQh *pUsbHcdQhAddr;

    // Physical address for Hcd TD array
    NvU32 UsbHcdTdPhyAddr;
    // Pointer to hold virtual address of HcdTd array
    UsbHcdTd *pUsbHcdTdAddr;

    // Async List head address
    UsbHcdQh *pAsyncListHead;

    // Power Manager registration API takes a
    // semaphore. This is used to wait for power Manager events
    NvOsSemaphoreHandle hPwrEventSem;
    // Id returned from driver's registration with Power Manager
    NvU32 RmPowerClientId;

    NvOdmPeripheralConnectivity const *pConnectivity;
    NvU64 Guid;

    // Interrupt handle
    NvOsInterruptHandle InterruptHandle;
    NvOdmUsbInterfaceType UsbInterfaceType;

    // Partial/Full Intilized mode
    NvBool  InitializeModeFull;

    NvDdkUsbPhyHandle hUsbPhy;
    NvDdkUsbhCapabilities Caps;

    NvU32 RegOffsetArray[UsbhRegId_Num];

    void (*UsbhGetPortSpeed)(NvDdkUsbhHandle pUsbHcdCtxt);

   }NvDdkUsbHcd;


/***********************************************************
 ************* definitions for private functions ***********
 ***********************************************************/

// Allocates the buffers required for the USB
NvError UsbhPrivAllocateBuffers(NvDdkUsbhHandle hUsbh);

// Releases the buffers allocated for the USB
void UsbhPrivDeAllocateBuffers(NvDdkUsbhHandle hUsbh);

// Initializes the controller
NvError UsbhPrivInitializeController(NvDdkUsbhHandle hUsbh);

//Enable VBUS
NvError UsbhPrivEnableVbus(NvDdkUsbhHandle pUsbhHandle, NvU32 Instance);

// gets the cable status or wehter A session is valid or not
NvBool UsbPrivIsACableConnected(NvDdkUsbhHandle hUsbh);
/* Returns true incase Async shedule is enabled  */
NvBool UsbhPrivIsAsyncSheduleEnabled(NvDdkUsbhHandle hUsbh);

NvError UsbhPrivRunController(NvDdkUsbhHandle hUsbh);

NvError UsbhPrivStopController(NvDdkUsbhHandle hUsbh);

void UsbhPrivEnableAsyncShedule(NvDdkUsbhHandle hUsbh);

void UsbhPrivDisableAsyncShedule(NvDdkUsbhHandle hUsbh);

NvError UsbhPrivAllocateQueueHead(NvDdkUsbhHandle hUsbh, UsbHcdQh **pNewHcdQh);

NvError
UsbhPrivConfigureQH(
    NvDdkUsbhHandle hUsbh,
    UsbHcdQh *pNQh,
    NvU32 pipe,
    NvU16 SpeedOfDevice,
    NvU32 MaxPktLength);

void UsbhPrivAsyncSubmitTransfer(NvDdkUsbhHandle hUsbh, NvDdkUsbhTransfer* pTransfer, NvU32 pipe);

void UsbhPrivAddToAsyncList(NvDdkUsbhHandle hUsbh, UsbHcdQh *pNQh);

void UsbhPrivDeleteFromAsyncList(NvDdkUsbhHandle hUsbh, UsbHcdQh *pDQh);

void usbhPrivConfigPortPower(NvDdkUsbhHandle hUsbh, NvBool set);

void UsbhPrivEnableAsyncScheduleParkMode(NvDdkUsbhHandle hUsbh);

void UsbHcdPrivISR(void *arg);

NvError UsbhPrivRegisterInterrupts(NvDdkUsbhHandle pUsbHcdCtxt);

void UsbhPrivEnableInterrupts(NvDdkUsbhHandle hUsbh);
void UsbhPrivClearInterrupts(NvDdkUsbhHandle hUsbh);

void UsbhPrivSuspendPort(NvDdkUsbhHandle pUsbHcdCtxt, NvBool enable);

NvError UsbhPrivWaitForPhysicalClock(NvDdkUsbhHandle hUsbh);

void
UsbhPrivConfigVbusInterrupt(
    NvDdkUsbhHandle pUsbHcdCtxt,
    NvBool Enable);

NvBool UsbhPrivIsIdPinSetToLow(NvDdkUsbhHandle pUsbHcdCtxt);

void UsbhPrivGetCapabilities(NvDdkUsbhHandle pUsbHcdCtxt);

void UsbhPrivT124StoreRegOffsets(NvDdkUsbhHandle pUsbHcdCtxt);

void UsbhPrivT124GetPortSpeed(NvDdkUsbhHandle pUsbHcdCtxt);

#if defined(__cplusplus)
}
#endif

#endif   // INCLUDED_NVDDK_USBH_PRIV_H


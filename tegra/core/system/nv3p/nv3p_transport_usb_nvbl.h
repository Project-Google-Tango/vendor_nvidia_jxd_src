/*
 * Copyright (c) 2008 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_drf.h"

#ifndef INCLUDED_NV3P_TRANSPORT_USB_NVBL_H
#define INCLUDED_NV3P_TRANSPORT_USB_NVBL_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define USE_SDRAM_BUFFERS 0
#define PTR_TO_ADDR(x) ((NvU32)(x))
#define CLK_RST_BASE 0x60006000
#define NVBL_CLOCKS_PLL_STABILIZATION_DELAY (300)
#define NVBL_IRAM_USB_BUFFER_LOCATION 0x40003000

/* This implements the USB version of the Nv3p transport layer. This is
 * designed to be compiled into the image (transport layer implementations
 * are not run-time selectable).
 */


/* Defines for USB register read and writes */
#define USB_REG_RD(reg)\
    NV_READ32(NV_ADDRESS_MAP_USB_BASE + USB2_CONTROLLER_USB2D_##reg##_0)

#define USB_REG_WR(reg, data)\
    NV_WRITE32((NV_ADDRESS_MAP_USB_BASE + USB2_CONTROLLER_USB2D_##reg##_0), data)

#define USB_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, reg, field, value)

#define USB_DRF_DEF(reg, field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, reg, field, define)

#define USB_DRF_DEF_VAL(reg, field, define) \
    (USB2_CONTROLLER_USB2D##_##reg##_0_##field##_##define)

#define USB_REG_READ_VAL(reg, field) \
    USB_DRF_VAL(reg, field, USB_REG_RD(reg))

#define USB_DEF_RESET_VAL(reg) \
    NV_RESETVAL(USB2_CONTROLLER_USB2D, reg)

#define USB_REG_SET_DRF_NUM(reg, field, num) \
    NV_FLD_SET_DRF_NUM(USB2_CONTROLLER_USB2D, reg, field, num, USB_REG_RD(reg))

#define USB_REG_SET_DRF_DEF(reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_USB2D, reg, field, define, USB_REG_RD(reg))

#define USB_REG_UPDATE_NUM(reg, field, num) \
    USB_REG_WR(reg, USB_REG_SET_DRF_NUM(reg, field, num))

#define USB_REG_UPDATE_DEF(reg, field, define) \
    USB_REG_WR(reg, USB_REG_SET_DRF_DEF(reg, field, define))

/* Defines for USB Device Queue Head read and writes */
#define USB_DQH_DRF_DEF(field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, DEVICE_QUEUE_HEAD, field, define)

#define USB_DQH_DRF_NUM(field, number) \
    NV_DRF_NUM(USB2_CONTROLLER_USB2D, DEVICE_QUEUE_HEAD, field, number)

#define USB_DQH_DRF_VAL(field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, DEVICE_QUEUE_HEAD, field, value)

#define USB_DQH_FLD_SHIFT_VAL(field) \
    NV_FIELD_SHIFT(USB2_CONTROLLER_USB2D_DEVICE_QUEUE_HEAD_0_##field##_RANGE)

/* Defines for USB Device Transfer Desctiptor read and writes */
#define USB_DTD_DRF_DEF(field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_USB2D, DEVICE_TRANSFER_DESCRIPTOR, field, define)

#define USB_DTD_DRF_NUM(field, number) \
    NV_DRF_NUM(USB2_CONTROLLER_USB2D, DEVICE_TRANSFER_DESCRIPTOR, field, number)

/* Defines for APB Misc register read and writes */
#define APB_MISC_REG_RD(reg)\
    NV_READ32(NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##reg##_0)

#define APB_MISC_REG_WR(reg, data)\
    NV_WRITE32((NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##reg##_0), data)

#define APB_MISC_SET_DRF_NUM(reg, field, num, value) \
    NV_FLD_SET_DRF_NUM(APB_MISC, reg, field, num, value)

#define APB_MISC_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(APB_MISC, reg, field, value)

// Define for finding the endpoint is IN endpoint
#define USB_IS_EP_IN(Endpoint) (((Endpoint & 1) != 0))

//Define for converting endpoint number to the word mask
#define USB_EP_NUM_TO_WORD_MASK(EndPoint) \
    (((USB_IS_EP_IN(EndPoint)) ? USB_EP_IN_MASK : USB_EP_OUT_MASK) << \
    (EndPoint >> 1))

#define NV_READ32(a)        *((const volatile NvU32 *)(a))
#define NV_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)

// Wait time for controller H/W status to change before giving up.
#define CONTROLLER_HW_TIMEOUT_US  100000   //100 milli seconds

/* Maximum wait time for transfers on IN/OUT endpoint = 1 Second*/
#define USB_MAX_TXFR_WAIT_TIME_MS  9000

#define NV_ADDRESS_MAP_APB_MISC_BASE                1879048192
#define NV_ADDRESS_MAP_USB_BASE                     2097152000
#define NVBL_CLOCKS_SYS_STATE_RUN (0x2)
#define NVBL_CLOCKS_SCLK_CLOCK_SOURCE_PLLP_OUT4 (2)

//-------------------------Private ENUM Decleration---------------------------


/* USB device decriptor size */
enum { USB_DEV_DESCRIPTOR_SIZE = 18 };

/* USB Configuration decriptor size */
enum { USB_CONFIG_DESCRIPTOR_SIZE = 32 };

/* Define for maximum usb Manufacturer String Length.*/
enum { USB_MANF_STRING_LENGTH = 26 };

/* Define for maximum usb Product String Length */
enum { USB_PRODUCT_STRING_LENGTH = 8 };

/* Define for maximum usb Product String Length */
enum { USB_SERIAL_NUM_LENGTH = 12 };

/* Define for Language ID String Length */
enum { USB_LANGUAGE_ID_LENGTH = 4 };

/* Define for Device Qualifier String Length */
enum { USB_DEV_QUALIFIER_LENGTH = 10 };

/* Define for Device Status Length */
enum { USB_DEV_STATUS_LENGTH = 2 };

/*
 * Maximum data transfer allowed on Bulk IN/OUT endpoint
 * USB needs 4K bytes data buffer for data transfers on the USB
 */
enum {NVBL_USB_MAX_TXFR_SIZE_BYTES = 4096};

/*
 * USB buffers alignment is 4K Bytes and this must be allocated in IRAM
 */
enum {NVBL_USB_BUFFER_ALIGNMENT = 4096};


/**
 * If Bit B0 is reset to zero, the device is bus-powered.
 * If Bit B0 is set to one, the device is self-powered.
 */
enum { USB_DEVICE_SELF_POWERED = 1 };



/**
 * Defines the request type in standard device requests.
 * Specifies the data transfer direction.
 * As per USB2.0 Specification.
 */
typedef enum
{
    // host-to-device, recipient: device.
    HOST2DEV_DEVICE     = 0x00,
    // host-to-device, recipient: interface.
    HOST2DEV_INTERFACE  = 0x01,
    // host-to-device, recipient: endpoint.
    HOST2DEV_ENDPOINT   = 0x02,
    // device-to-host, transmitter: device.
    DEV2HOST_DEVICE     = 0x80,
    // device-to-host, transmitter: interface.
    DEV2HOST_INTERFACE  = 0x81,
    // device-to-host, transmitter: endpoint.
    DEV2HOST_ENDPOINT   = 0x82,
} UsbSetupRequestType;

/**
 * Specifies the device request type to be sent across the USB.
 * As per USB2.0 Specification.
 */
typedef enum
{
    // Specifies the Get Status request
    GET_STATUS        = 0,
    // Specifies the clear feature request
    CLEAR_FEATURE     = 1,
    // Specifies the set feature request
    SET_FEATURE       = 3,
    // Specifies the set address request
    SET_ADDRESS       = 5,
    // Specifies the device descriptor request
    GET_DESCRIPTOR    = 6,
    // Specifies the set descriptor request
    SET_DESCRIPTOR    = 7,
    // Specifies the set configuration request
    GET_CONFIGURATION = 8,
    // Specifies the set configuration request
    SET_CONFIGURATION = 9,
    // Specifies the Get Interface request.
    GET_INTERFACE     = 10
} DeviceRequestTypes;

/**
 * Defines USB descriptor types.
 * As per USB2.0 Specification.
 */
typedef enum
{
    // Specifies a device descriptor type
    USB_DT_DEVICE          = 1,
    // Specifies a device configuration type
    USB_DT_CONFIG          = 2,
    // Specifies a device string type
    USB_DT_STRING          = 3,
    // Specifies a device interface type
    USB_DT_INTERFACE       = 4,
    // Specifies a device endpoint type
    USB_DT_ENDPOINT        = 5,
    // Specifies a device qualifier type
    USB_DT_DEVICE_QUALIFIER= 6
} UsbDevDescriptorType;

/**
 * Defines Setup Packet field offsets as per USB2.0
 * As per USB2.0 Specification.
 */
typedef enum
{
    // Data transfer direction field Offset
    USB_SETUP_REQUEST_TYPE  = 0,
    // Specific request field offset
    USB_SETUP_REQUEST       = 1,
    // Specifies value field offset
    USB_SETUP_VALUE         = 2,
    // Specific DESRIPTOR request field offset
    USB_SETUP_DESCRIPTOR    = 3,
    // Specifies Index field offset
    USB_SETUP_INDEX         = 4,
    // Specifies an endpoint
    USB_SETUP_LENGTH        = 6
} SetupPacketOffset;

/**
 * Defines USB sring descriptor Index
 * As per USB2.0 Specification.
 */
typedef enum
{
    // Specifies a Language ID string descriptor index
    USB_LANGUAGE_ID = 0,
    // Specifies a Manufacturer ID string descriptor index
    USB_MANF_ID = 1,
    // Specifies a Product ID string descriptor index
    USB_PROD_ID = 2,
    // Specifies a Serial No string descriptor index
    USB_SERIAL_ID = 3
} StringDescriptorIndex;

/* Defines for the USB Packet size */
enum
{
    // usb packet size in Full speed = 64 bytes
    USB_FULL_SPEED_PKT_SIZE_BYTES = 0x40,
    // usb packet size in High speed = 512 bytes
    USB_HIGH_SPEED_PKT_SIZE_BYTES = 0x200
};


/*
 * NvBlUsbfEndpoint -- Defines the BulK endpoints that are supported.
 * Only Two Bulk endpoints, IN and OUT, are supported for Boot ROM.
 * All control endpoints are taken care by the driver.
 */
typedef enum
{
    /* Endpoint for recieving data from the Host */
    NvBlUsbfEndPoint_BulkOut,
    /* Endpoint for Transmitting data to the Host */
    NvBlUsbfEndPoint_BulkIn,
    NvBlUsbfEndPoint_Num,
    NvBlUsbfEndpoint_Force32 = 0x7fffffff

} NvBlUsbfEndpoint;

/*
 * NvBlUsbfEpStatus -- Defines Bulk Endpoint status
 */
typedef enum
{
    /* Indicates Transfer on the endpoint is complete */
    NvBlUsbfEpStatus_TxfrComplete,
    /* Indicates Transfer on the endpoint is Still Active */
    NvBlUsbfEpStatus_TxfrActive,
    /* Indicates Transfer on the endpoint is Failed */
    NvBlUsbfEpStatus_TxfrFail,
    /* Indicates endpoint is Idle and ready for starting data transfers*/
    NvBlUsbfEpStatus_TxfrIdle,
    /* Indicates endpoint is stalled */
    NvBlUsbfEpStatus_Stalled,
    /* Indicates endpoint is not configured yet */
    NvBlUsbfEpStatus_NotConfigured,
    NvBlUsbfEpStatus_Force32 = 0x7fffffff

} NvBlUsbfEpStatus;

/* Defines for USB endpoint masks */
enum
{
    // USB EP Out mask for finding the OUT endpoint
    USB_EP_OUT_MASK = 0x00000001,
    // USB EP In mask for finding the IN endpoint
    USB_EP_IN_MASK  = 0x00010000,
};

/**
 * Defines for controller endpoint number. This define is mapping to the
 * USB controller endpoints. Each endpoint defined in the controller
 * supports endpoint IN and endpoint out transfers. These IN and OUT
 * endpoints can be configured to control, bulk, isoc or interrupt endpoints.
 * In the bootrom this mapping is fixed as below.
 * Control OUT and IN are mapped to endpoint0 in the H/W
 * Bulk Out and IN are mapped to the endpoint1 in the H/W
 * endpoint2 in the H/W is not used here.
 */
enum
{
    //Control Out endpoint number, mapped to endpoint0
    USB_EP_CTRL_OUT = 0,
    //Control In endpoint number, mapped to endpoint0
    USB_EP_CTRL_IN  = 1,
    //Bulk out endpoint number, mapped to endpoint1
    USB_EP_BULK_OUT = 2,
    //Bulk In endpoint number, mapped to endpoint1
    USB_EP_BULK_IN  = 3
};

/**
 * Number of supported USB endpoints
 * includes control and bulk endpoint 0.IN and OUT
 */
enum {USBF_MAX_EP_COUNT = 4};

/* maximum DTDs possible based on the configuration */
enum {USBF_MAX_DTDS = 4};

/* maximum H/W buffers pointers in the descriptors */
enum {USBF_MAX_BUFFER_PTRS = 5};

typedef enum
{
    NV_ALIGN2K=2048,
    NV_ALIGN4K=4096
}NvAlign;

//---------------------Private Structure Decleration---------------------------

/**
 * Structure defining the fields for device transfer descriptor(DTD)
 * (32 byte structure).
 */
typedef struct NvBlUsbDevTransDescRec
{
    // Next Link pointer
    volatile NvU32 NextDtd;
    // Stores Status, Interrupt on complete and bytes to go field.
    volatile NvU32 DtdToken;
    // Memory buffer pointers.
    volatile NvU32 BufPtrs[USBF_MAX_BUFFER_PTRS];
    // Reserved
    volatile NvU32 Reserved;

} NvBlUsbDevTransDesc;

/**
 * Structure defining the fields for device queue head descriptor
 * (64 byte structure)
 */
typedef struct NvBlUsbDevQueueHeadRec
{
    // EP capabilities
    volatile NvU32 EpCapabilities;
    // current DTD pointer
    volatile NvU32 CurrentDTDPtr;
    // Next DTD pointer
    volatile NvU32 NextDTDPtr;
    // Stores Status, Interrupt on complete and bytes to go fields
    volatile NvU32 DtdToken;
    // Buffer Pointers
    volatile NvU32 BufferPtrs[USBF_MAX_BUFFER_PTRS];
    // reserved
    volatile NvU32 Reserved;
    // Setup Packet Buffer ( 0 - 3 bytes )
    volatile NvU32 setupBuffer0;
    // Setup Packet Buffer ( 4 - 7 bytes )
    volatile NvU32 setupBuffer1;
    // reserved
    volatile NvU32 Reserved0;
    // reserved
    volatile NvU32 Reserved1;
    // Reserved
    volatile NvU32 Reserved2;
    // Reserved
    volatile NvU32 Reserved3;
}NvBlUsbDevQueueHead;

/**
 * Defines the USB data Store consiting of
 * Queue Head and Device transfer decriptors buffer
 */
typedef struct NvBlUsbDescriptorDataRec
{
    // device transfer decriptor pointer.
    // All the Transfer Descriptors must be 32byte aligned
    NvBlUsbDevTransDesc pDataTransDesc[USBF_MAX_DTDS];
    // The EndPoint Queue Head List MUST be aligned to a 2k boundary.
    NvBlUsbDevQueueHead *pQueueHead;
    // This variable is used to indicate EP is configured or not
    volatile NvU32 EpConfigured[USBF_MAX_EP_COUNT];
    // This variable is used for storing the bytes requested
    volatile NvU32 BytesRequestedForEp[USBF_MAX_EP_COUNT];

} NvBlUsbDescriptorData;



typedef struct Nv3pTransportRec
{
    NvRmMemHandle hBufferMem;
    NvU8 *buffer;
    NvU32 size;

    /* receive buffering */
    NvU8 *read;
    NvU32 remaining;
} Nv3pTransport;

Nv3pTransport s_Transport;


typedef struct _NvBuffer
{
    NvU8 * allocatedBuffer;
    NvU8 * alignedBuffer;
}NvBuffer;

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NV3P_TRANSPORT_USB_NVBL_H

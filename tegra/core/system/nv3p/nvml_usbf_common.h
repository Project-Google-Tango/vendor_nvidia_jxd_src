/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVML_USBF_COMMON_H
#define INCLUDED_NVML_USBF_COMMON_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

//-------------------------Private ENUM Decleration---------------------------

/* USB Setup Packet size */
enum { USB_SETUP_PKT_SIZE = 8};

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
    // device-to-host, transmitter: device
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
    // Specifies the Get Interface request
    GET_INTERFACE     = 10
} DeviceRequestTypes;

/**
 * Defines USB descriptor types.
 * As per USB2.0 Specification.
 */
typedef enum
{
    // Specifies a device descriptor type
    USB_DT_DEVICE             = 1,
    // Specifies a device configuration type
    USB_DT_CONFIG             = 2,
    // Specifies a device string type
    USB_DT_STRING             = 3,
    // Specifies a device interface type
    USB_DT_INTERFACE          = 4,
    // Specifies a device endpoint type
    USB_DT_ENDPOINT           = 5,
    // Specifies a device qualifier type
    USB_DT_DEVICE_QUALIFIER   = 6,
    // Specifies the other speed configuration type
    USB_DT_OTHER_SPEED_CONFIG = 7
} UsbDevDescriptorType;


typedef enum FeatureSelector_t
{
    /// Specifies the endpoint halt.
    ENDPOINT_HALT        = 0,
    /// Specifies the device remote wake up.
    DEVICE_REMOTE_WAKEUP = 1,
    /// Specifies test mode.
    TEST_MODE            = 2
}FeatureSelector;


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

/* Defines for USB endpoint masks */
enum
{
    // USB EP Out mask for finding the OUT endpoint
    USB_EP_OUT_MASK = 0x00000001,
    // USB EP In mask for finding the IN endpoint
    USB_EP_IN_MASK  = 0x00010000
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
 * Defines for controller endpoint address
 */
enum
{
    // Control Out endpoint address
    CTRL_OUT = 0x00,
    // Control In endpoint address
    CTRL_IN  = 0x80,
    // Bulk out endpoint address
    BULK_OUT = 0x01,
    // Bulk In endpoint address
    BULK_IN  = 0x81
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
 * NvBootUsbfPortSpeed -- Defines the USB Port Speed
 */
typedef enum
{
    // Defines the port Full speed
    NvBootUsbfPortSpeed_Full,
    // Defines the port full speed
    NvBootUsbfPortSpeed_Low,
    // Defines the port high speed
    NvBootUsbfPortSpeed_High,

    NvBootUsbfPortSpeed_Force32 = 0x7FFFFFF
} NvBootUsbfPortSpeed;


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
    /* Control Endpoint for receiving data from the Host */
    NvBootUsbfEndPoint_ControlOut,
    /* Control Endpoint for Transmitting data to the Host */
    NvBootUsbfEndPoint_ControlIn,
    /* Defines the number of EP supported */
    NvBootUsbfEndPoint_Num,
    NvBootUsbfEndpoint_Force32 = 0x7FFFFFFF

} NvBootUsbfEndpoint;


//---------------------Private Structure Decleration---------------------------

/**
 * Structure defining the fields for USB UTMI clocks delay Parameters.
 */
typedef struct UsbPllDelayParamsRec
{
    // Pll-U Enable Delay Count
    NvU16 EnableDelayCount;
    //PLL-U Stable count
    NvU16 StableCount;
    //Pll-U Active delay count
    NvU16 ActiveDelayCount;
    //PLL-U Xtal frequency count
    NvU16 XtalFreqCount;
} UsbPllDelayParams;

/**
 * Structure defining the fields for USB PLLU configuration Parameters.
 */
typedef struct UsbPllClockParamsRec
{
    //PLL feedback divider.
    NvU32 N;
    //PLL input divider.
    NvU32 M;
    //post divider (2^n)
    NvU32 P;
    //Base PLLC charge pump setup control
    NvU32 CPCON;
    //Base PLLC loop filter setup control.
    NvU32 LFCON;
} UsbPllClockParams;

/**
 * Structure defining the fields for device transfer descriptor(DTD)
 * (32 byte structure).
 */
typedef struct NvBootUsbDevTransDescRec
{
    // Next Link pointer
    volatile NvU32 NextDtd;
    // Stores Status, Interrupt on complete and bytes to go field.
    volatile NvU32 DtdToken;
    // Memory buffer pointers.
    volatile NvU32 BufPtrs[USBF_MAX_BUFFER_PTRS];
    // Reserved
    volatile NvU32 Reserved;

} NvBootUsbDevTransDesc;

/**
 * Structure defining the fields for device queue head descriptor
 * (64 byte structure)
 */
typedef struct NvBootUsbDevQueueHeadRec
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
}NvBootUsbDevQueueHead;

/**
 * Defines the USB data Store consiting of
 * Queue Head and Device transfer decriptors buffer
 */
typedef struct NvBootUsbDescriptorDataRec
{
    // device transfer decriptor pointer.
    // All the Transfer Descriptors must be 32byte aligned
    NvBootUsbDevTransDesc pDataTransDesc[USBF_MAX_DTDS];
    // The EndPoint Queue Head List MUST be aligned to a 2k boundary.
    NvBootUsbDevQueueHead *pQueueHead;
    // This variable is used to indicate EP is configured or not
    volatile NvU32 EpConfigured[USBF_MAX_EP_COUNT];
    // This variable is used for storing the bytes requested
    volatile NvU32 BytesRequestedForEp[USBF_MAX_EP_COUNT];

} NvBootUsbDescriptorData;

/*
 * NvBootUsbfContext - The context structure for the Usbf driver.
 */
typedef struct NvBootUsbfContextRec
{
    // Usb port speed
    NvBootUsbfPortSpeed UsbPortSpeed;
    // UsbBaseAddr is used to store the base address of the controller
    NvU32 UsbBaseAddr;
    // Setup packet data if USB_SETUP_PKT set in event field.
    NvU8 setupPkt[USB_SETUP_PKT_SIZE];
    // indicates wether enumeration is done or not
    NvBool EnumerationDone;
    // indicates USB controller is enabled or not
    NvBool UsbControllerEnabled;
    // Stores the current configuration number
    NvU8 UsbConfigurationNo;
    // Stores Curent interface number
    NvU8 UsbInterfaceNo;
    // indicates whether initializations were done
    NvBool InitializationDone;
} NvBootUsbfContext;


/*
 * NvBootUsbfEpStatus -- Defines Bulk Endpoint status.
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
    NvBootUsbfEpStatus_Force32 = 0x7FFFFFFF

} NvBootUsbfEpStatus;


//---------------------------MACRO DEFINITIONS--------------------------------

/* Defines for USB register read and writes */
#define USB_REG_RD(reg)\
    NV_READ32(s_pUsbfCtxt->UsbBaseAddr + USB2_CONTROLLER_USB2D_##reg##_0)

#define USB_REG_WR(reg, data)\
    NV_WRITE32((s_pUsbfCtxt->UsbBaseAddr + USB2_CONTROLLER_USB2D_##reg##_0), data)

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


/* Defines for USB IF registers read and writes */
#define USBIF_REG_RD(base, reg)\
    NV_READ32(base + USB1_IF_##reg##_0)

#define USBIF_REG_WR(base, reg, data)\
    NV_WRITE32(base + USB1_IF_##reg##_0, data)

#define USBIF_REG_SET_DRF_DEF(base, reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB1_IF, reg, field, define, USBIF_REG_RD(base, reg))

#define USBIF_REG_UPDATE_DEF(base, reg, field, define) \
    USBIF_REG_WR(base, reg, USBIF_REG_SET_DRF_DEF(base, reg, field, define))

#define USBIF_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB1_IF, reg, field, value)

#define USBIF_REG_READ_VAL(base, reg, field) \
    USBIF_DRF_VAL(reg, field, USBIF_REG_RD(base, reg))

/* Defines for UTMIP MISC registers */

#define UTMIP_REG_RD(base, reg) \
    NV_READ32(base + USB1_UTMIP_##reg##_0)

#define UTMIP_REG_WR(base, reg, data) \
    NV_WRITE32(base + USB1_UTMIP_##reg##_0, data)

#define UTMIP_REG_SET_DRF_DEF(base, reg, field, def) \
    NV_FLD_SET_DRF_DEF(USB1_UTMIP, reg, field, def, UTMIP_REG_RD(base, reg))

#define UTMIP_REG_UPDATE_DEF(base, reg, field, define) \
    UTMIP_REG_WR(base, reg, UTMIP_REG_SET_DRF_DEF(base, reg, field, define))

#define UTMIP_REG_SET_DRF_NUM(base, reg, field, num) \
    NV_FLD_SET_DRF_NUM(USB1_UTMIP, reg, field, num, UTMIP_REG_RD(base, reg))

#define UTMIP_REG_UPDATE_NUM(base, reg, field, num) \
    UTMIP_REG_WR(base, reg, UTMIP_REG_SET_DRF_NUM(base, reg, field, num))

/* Defines for APB Misc register read and writes */
#define APB_MISC_REG_RD(reg)\
    NV_READ32(NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##reg##_0)

// Define for finding the endpoint is IN endpoint
#define USB_IS_EP_IN(Endpoint) (((Endpoint & 1) != 0))

//Define for converting endpoint number to the word mask
#define USB_EP_NUM_TO_WORD_MASK(EndPoint) \
    (((USB_IS_EP_IN(EndPoint)) ? USB_EP_IN_MASK : USB_EP_OUT_MASK) << \
    (EndPoint >> 1))

// Wait time for controller H/W status to change before giving up.
#define CONTROLLER_HW_TIMEOUT_US  100000   //100 milli seconds

/* Maximum wait time for transfers on IN/OUT endpoint = 1 Second*/
#define USB_MAX_TXFR_WAIT_TIME_MS  1000


//-------------------Common USB Definitions------------------------------------------------

/**
 * USB Device Descriptor: 12 bytes as per the USB2.0 Specification
 * Stores the Device descriptor data must be word aligned
 */
NV_ALIGN(4) static NvU8 s_UsbDeviceDescriptor[USB_DEV_DESCRIPTOR_SIZE] =
{
    USB_DEV_DESCRIPTOR_SIZE,   // bLength - Size of this descriptor in bytes
    0x01,   // bDescriptorType - Device Descriptor Type
    0x00,   // bcd USB (LSB) - USB Spec. Release number
    0x02,   // bcd USB (MSB) - USB Spec. Release number (2.0)
    0x00,   // bDeviceClass - Class is specified in the interface descriptor
    0x00,   // bDeviceSubClass - SubClass is specified in the interface descriptor
    0x00,   // bDeviceProtocol - Protocol is specified in the interface descriptor
    0x40,   // bMaxPacketSize0 - Maximum packet size for EP0
    0x55,   // idVendor(LSB) - Vendor ID assigned by USB forum
    0x09,   // idVendor(MSB) - Vendor ID assigned by USB forum
    0x15,   // idProduct(LSB) - Product ID assigned by Organization
    0x71,   // idProduct(MSB) - Product ID assigned by Organization
    0x01,   // bcd Device (LSB) - Device Release number in BCD
    0x01,   // bcd Device (MSB) - Device Release number in BCD
    USB_MANF_ID,   // Index of String descriptor describing Manufacturer
    USB_PROD_ID,   // Index of String descriptor describing Product
    USB_SERIAL_ID,    // Index of String descriptor describing Serial number
    0x01   // bNumConfigurations - Number of possible configuration
};

/**
 * USB Device Configuration Descriptors:
 * 32 bytes as per the USB2.0 Specification. This contains
 * Configuration descriptor, Interface descriptor and endpoint descriptors.
 */
NV_ALIGN(4) static NvU8 s_UsbConfigDescriptor[USB_CONFIG_DESCRIPTOR_SIZE] =
{
    /* Configuration Descriptor 32 bytes  */
    0x09,   // bLength - Size of this descriptor in bytes
    0x02,   // bDescriptorType - Configuration Descriptor Type
    0x20,   // WTotalLength (LSB) - Total length of data for this configuration
    0x00,   // WTotalLength (MSB) - Total length of data for this configuration
    0x01,   // bNumInterface - Nos of Interface supported by this configuration
    0x01,   // bConfigurationValue
    0x00,   // iConfiguration - Index of descriptor describing this configuration
    0xc0,   // bmAttributes - bitmap "D4-D0: Res, D6: Self Powered,D5: Remote Wakeup
    0xFA,   // MaxPower in mA - Max Power Consumption of the USB device

    /* Interface Descriptor */
    0x09,   // bLength - Size of this descriptor in bytes
    0x04,   // bDescriptorType - Interface Descriptor Type
    0x00,   // bInterfaceNumber - Number of Interface
    0x00,   // bAlternateSetting - Value used to select alternate setting
    0x02,   // bNumEndpoints - Nos of Endpoints used by this Interface
    0xFF,   // bInterfaceClass - Class code "Vendor Specific Class."
    0xFF,   // bInterfaceSubClass - Subclass code "Vendor specific"
    0xFF,   // bInterfaceProtocol - Protocol code "Vendor specific"
    0x00,   // iInterface - Index of String descriptor describing Interface

    /* Endpoint Descriptor IN EP2 */
    0x07,   // bLength - Size of this descriptor in bytes
    0x05,   // bDescriptorType - ENDPOINT Descriptor Type
    0x81,   // bEndpointAddress - The address of EP on the USB device
    0x02,   // bmAttributes - Bit 1-0: Transfer Type 10: Bulk,
    0x40,   // wMaxPacketSize(LSB) - Maximum Packet Size for this EP
    0x00,   // wMaxPacketSize(MSB) - Maximum Packet Size for this EP
    0x00,   // bIntervel - Interval for polling EP, for Interrupt and Isochronous

    /** Endpoint Descriptor OUT EP1 */
    0x07,   // bLength - Size of this descriptor in bytes
    0x05,   // bDescriptorType - ENDPOINT Descriptor Type
    0x01,   // bEndpointAddress - The address of EP on the USB device
    0x02,   // bmAttributes - Bit 1-0: Transfer Type 10: Bulk,
    0x40,   // wMaxPacketSize(LSB) - Maximum Packet Size for this EP
    0x00,   // wMaxPacketSize(MSB) - Maximum Packet Size for this EP
    0x00    // bIntervel - Interval for polling EP, for Interrupt and Isochronous
};

// Stores the Manufactures ID sting descriptor data
NV_ALIGN(4) static NvU8 s_UsbManufacturerID[USB_MANF_STRING_LENGTH] =
{
    USB_MANF_STRING_LENGTH,  // Length of descriptor
    0x03,                    // STRING descriptor type.
    'N', 0,
    'V', 0,
    'I', 0,
    'D', 0,
    'I', 0,
    'A', 0,
    ' ', 0,
    'C', 0,
    'o', 0,
    'r', 0,
    'p', 0,
    '.', 0

};

// Stores the Product ID string descriptor data
NV_ALIGN(4) static NvU8 s_UsbProductID[USB_PRODUCT_STRING_LENGTH] =
{
    USB_PRODUCT_STRING_LENGTH, // Length of descriptor
    0x03,                      // STRING descriptor type.
    'A', 0x00,
    'P', 0x00,
    'X', 0x00
};

// Stores the Serial Number String descriptor data
NV_ALIGN(4) static NvU8 s_UsbSerialNumber[USB_SERIAL_NUM_LENGTH] =
{
    USB_SERIAL_NUM_LENGTH, // Length of descriptor
    0x03,                  // STRING descriptor type.
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00
};

// Stores the Language ID Descriptor data
NV_ALIGN(4) static NvU8 s_UsbLanguageID[USB_LANGUAGE_ID_LENGTH] =
{
    /* Language Id string descriptor */
    USB_LANGUAGE_ID_LENGTH,  // Length of descriptor
    0x03,                    // STRING descriptor type.
    0x09, 0x04               // LANGID Code 0: American English 0x409
};

// Stores the Device Qualifier Desriptor data
NV_ALIGN(4) static NvU8 s_UsbDeviceQualifier[USB_DEV_QUALIFIER_LENGTH] =
{
    /* Device Qualifier descriptor */
    USB_DEV_QUALIFIER_LENGTH,   // Size of the descriptor
    6,    // Device Qualifier Type
    0x00, // USB specification version number: LSB
    0x02, //  USB specification version number: MSB
    0xFF, // Class Code
    0xFF, // Subclass Code
    0xFF, // Protocol Code
    0x40, //Maximum packet size for other speed
    0x01, //Number of Other-speed Configurations
    0x00  // Reserved for future use, must be zero
};

// Stores the Device Status descriptor data
NV_ALIGN(4) static NvU8 s_UsbDeviceStatus[USB_DEV_STATUS_LENGTH] =
{
    USB_DEVICE_SELF_POWERED,
    0,
};

// Mask for ALL end points in the controller
static const NvU32 USB_FLUSH_ALL_EPS = 0xFFFFFFFF;
static NvBool IsVBUSOverrideSet = NV_FALSE;
#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NVML_USBF_COMMON_H


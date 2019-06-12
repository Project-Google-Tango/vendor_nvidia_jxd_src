/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVBOOT_USBF_PRIV_H
#define INCLUDED_NVBOOT_USBF_PRIV_H

#include "nvcommon.h"
#include "t30/nvboot_osc.h"
#include "t30/nvboot_error.h"

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


#define CTRL_OUT   0x00
#define CTRL_IN    0x80
#define BULK_OUT   0x01
#define BULK_IN    0x81

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
    USB_DT_DEVICE_QUALIFIER   = 6 ,
    // Specifies the other speed configuration type
    USB_DT_OTHER_SPEED_CONFIG = 7
} UsbDevDescriptorType;

typedef enum FeatureSelector_t
{
    /// Specifies the endpoint halt.
    ENDPOINT_HALT               = 0,
    /// Specifies the device remote wake up.
    DEVICE_REMOTE_WAKEUP = 1,
    /// Specifies test mode.
    TEST_MODE                      = 2
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

    NvBootUsbfPortSpeed_Force32 = 0x7FFFFFF,
} NvBootUsbfPortSpeed;


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


#define UTMIP_PLL_RD(base, reg) \
    NV_READ32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0)

#define UTMIP_PLL_WR(base, reg, data) \
    NV_WRITE32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0, data)

#define UTMIP_PLL_SET_DRF_DEF(base, reg, field, def) \
    NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER_UTMIP, reg, field, def, UTMIP_PLL_RD(base, reg))

#define UTMIP_PLL_UPDATE_DEF(base, reg, field, define) \
    UTMIP_PLL_WR(base, reg, UTMIP_PLL_SET_DRF_DEF(base, reg, field, define))

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
} NvBootUsbfContext;



#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVBOOT_USBF_PRIV_H


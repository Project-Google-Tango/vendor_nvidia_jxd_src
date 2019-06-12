/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

 /**
  * Internal definitions for using usb3 as a boot device.
  */

#ifndef INCLUDED_NVDDK_XUSBH_H
#define INCLUDED_NVDDK_XUSBH_H


#include "nvcommon.h"

#if defined(__cplusplus)
     extern "C"
     {
#endif

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

#define UTMIP_PLL_RD(base, reg) \
         NV_READ32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0)

#define UTMIP_PLL_WR(base, reg, data) \
         NV_WRITE32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0, data)

#define UTMIP_PLL_SET_DRF_DEF(base, reg, field, def) \
         NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER_UTMIP, reg, field, def, UTMIP_PLL_RD(base, reg))

#define UTMIP_PLL_UPDATE_DEF(base, reg, field, define) \
         UTMIP_PLL_WR(base, reg, UTMIP_PLL_SET_DRF_DEF(base, reg, field, define))

/* USB Setup Packet size */
enum { USB_SETUP_PKT_SIZE = 8};

/* USB device decriptor size */
enum { USB_DEV_DESCRIPTOR_SIZE = 18 };

/* USB Configuration decriptor size */
enum { USB_CONFIG_DESCRIPTOR_SIZE = 9 };

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

/* Define for usb logical unit Length.*/
enum { USB_GET_MAX_LUN_LENGTH = 1 };

/* Define for HS control bulk max packetsize Length */
enum { USB_HS_CONTROL_MAX_PACKETSIZE = 64,
       USB_HS_BULK_MAX_PACKETSIZE = 512};

/* Define for TRB average Length */
enum { USB_TRB_AVERAGE_CONTROL_LENGTH = 8,
       USB_TRB_AVERAGE_BULK_LENGTH = 512};

/* Define for TR and data bufer memory location */
enum { SYSTEM_MEM = 0, // iram
       DDIRECT = 1}; // dram

/* Define for Boot Port enum */
enum { USB_BOOT_PORT_OTG0 = 0,
       USB_BOOT_PORT_OTG1 = 1,
       USB_BOOT_PORT_OTG2 = 2, // t124 only..
       USB_BOOT_PORT_ULPI = 7,
       USB_BOOT_PORT_HSIC0 = 9,
       USB_BOOT_PORT_HSIC1 = 0xA};

/* Define for Usb Host Controller Port enum */
enum { T114_USB_HOST_PORT_OTG0 = 0,
       T114_USB_HOST_PORT_OTG1 = 1,
       T114_USB_HOST_PORT_ULPI = 2,
       T114_USB_HOST_PORT_HSIC0 = 3,
       T114_USB_HOST_PORT_HSIC1 = 4,
       T114_RESERVED = 0xF};

enum { VBUS_ENABLE_0 = 0,
       VBUS_ENABLE_1 = 1};

enum { CTRL_OUT = 0x00,
       DCI_CTRL = 0x01,
       CTRL_IN = 0x80};

enum { BULK_OUT = 0x01,
       BULK_IN = 0x81};

enum { HIGH_SPEED = 3};

enum { USB_MAX_TXFR_RETRIES = 3};
enum { XUSB_CFG_PAGE_SIZE = 0x200 };
enum { XUSB_READ = 0,
       XUSB_WRITE = 1};

enum { XUSB_DEV_ADDR = 1};

/// This needs to be more than the BCT struct info.
enum { MSC_MAX_PAGE_SIZE_LOG_2 = 9,
       XUSB_MSC_BOT_PAGE_SIZE_LOG2 = 11,  // 2 K
       NVBOOT_MSC_BLOCK_SIZE_LOG2 = 14};

//For High-Speed control and bulk endpoints this field shall be cleared to ‘0’.
enum { MAX_BURST_SIZE = 0};
enum { ONE_MILISEC = 3};           // 2^3 * 125us = 8 * 125us = 1ms
enum { PORT_INSTANCE_OFFSET = 0x10};

// retries
enum { CONTROLLER_HW_RETRIES_100   = 100,
       CONTROLLER_HW_RETRIES_2000  = 2000,
       CONTROLLER_HW_RETRIES_1SEC  = 1000000};

// Wait time for controller H/W status to change before giving up in terms of mircoseconds
enum { TIMEOUT_1US     = 1, // 1 micro seconds
       TIMEOUT_10US    = 10, // 10 micro seconds
       TIMEOUT_1MS     = 1000, // 1 milli sec
       TIMEOUT_10MS    = 10000,    // 10 milli sec
       TIMEOUT_50MS    = 50000,    // 50 milli sec
       TIMEOUT_100MS   = 100000, // 100 milli sec
       TIMEOUT_1S      = 1000000};   //1000 milli seconds

typedef enum
{
    USB_DIR_OUT = 0, // matches HOST2DEV enum
    USB_DIR_IN,      // matches DEV2HOST enum
}UsbDirectionType;

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

/**
* Specifies the device request type to be sent across the USB.
*
*/
typedef enum {
    GET_STATUS             = 0,
    CLEAR_FEATURE          = 1,   /*!<clear feature request.*/
    ENDPOINT_CLEAR_FEATURE = 1,   /*!<end point clear feature request.*/
    SET_FEATURE            = 3,   /*!<set feature request.*/
    SET_ADDRESS            = 5,   /*!<set address request.*/
    GET_DESCRIPTOR         = 6,   /*!<device descriptor request.*/
    SET_DESCRIPTOR         = 7,   /*!<set descriptor request.*/
    GET_CONFIGURATION      = 8,   /*!<get configuration request.*/
    SET_CONFIGURATION      = 9,   /*!<set configuration request.*/
    GET_INTERFACE          = 10,  /*!<get interface request.*/
    SET_INTERFACE          = 11,  /*!<set interface request.*/
    SYNC_FRAME             = 12,  /*!<sync frame request.*/
    RESET_PORT             = 13,  /*!<reset port request -- Not available before 5.40. */
    POWERON_VBUS           = 14,  /*!<power on VBUS request -- Not available before 5.40.*/
    POWEROFF_VBUS          = 15,  /*!<power oFF VBUS request -- Not available before 5.40.*/
    PORT_SPEED             = 16,  /*!<port speed request -- Not available before 5.40.*/
    PORT_STATUS            = 17,  /*!<port status request -- Not available before 5.40.*/
    GET_MAX_LUN            = 0xFE /* CLASS specific request for GET_MAX_LUN*/
}DeviceRequestTypes;

/**
 *
 * @brief Defines the direction field in USB device request codes.
 */
typedef enum {
/// Specifies the data transfer direction: host-to-device.
HOST2DEV     = 0,

/// Specifies the data transfer direction: device-to-host.
DEV2HOST     = 1
}BmRequestCodeDir;

/**
 * @brief Defines the type field in USB device request codes.
 *
 */
typedef enum {
/// Specifies the standard request type. Required in all devices.
REQ_STANDARD  = 0,

/// Specifies the class-specific request type.
REQ_CLASS     = 1,

/// Specifies the vendor-specific request type.
REQ_VENDOR    = 2,

/// Reserved request.
REQ_RESERVED  = 3
}BmRequestCodeType;


/**
 * @brief Defines the recipient field in USB device request codes.
 *
 */
typedef enum {
/// Specifies the  USB device is the target of the request.
TARGET_DEVICE   = 0,

/// Specifies an interface on the USB device is the target of the request.
TARGET_INTERFACE    = 1,

/// Specifies an endpoint on the USB device is the target of the request.
TARGET_ENDPOINT     = 2,

/// Specifies an unspecified target on the USB device is specified by the request.
TARGET_OTHER        = 3,

/// Specifies all other targets are reserved.
TARGET_RESERVED     = 4
}BmRequestCodeTarget ;


/**
* @brief Defines the request type in standard device requests.
*/
typedef enum {
/// Specifies the data transfer direction: host-to-device, recipient: device.
HOST2DEV_DEVICE     = 0x00,

/// Specifies the data transfer direction: host-to-device, recipient: interface.
HOST2DEV_INTERFACE  = 0x01,

/// Specifies the data transfer direction: host-to-device, recipient: endpoint.
HOST2DEV_ENDPOINT   = 0x02,

/// Specifies the data transfer direction: host-to-device, recipient: endpoint
///        -- Not available before 5.20.
HOST2DEV_OTHER   = 0x03,

/// Specifies the data transfer direction: device-to-host, transmitter: device.
DEV2HOST_DEVICE     = 0x80,

/// Specifies the data transfer direction: device-to-host, transmitter: interface.
DEV2HOST_INTERFACE  = 0x81,

/// Specifies the data transfer direction: device-to-host, transmitter: endpoint.
DEV2HOST_ENDPOINT   = 0x82,

/// Specifies the data transfer direction: device-to-host, transmitter: endpoint
/// -- Not available before 5.20.
DEV2HOST_OTHER   = 0x83,

/// Specifies the data transfer direction: device-to-host, transmitter: interface.
CLASS_SPECIFIC_REQUEST = 0xA1
}BmRequest;

//Specifies the device request strucutre to send the set up data across the USB.

typedef struct DeviceRequestRec
{
    union
    {
        NvU32   bmRequestType   :8;
        struct
        {
            NvU32 recipient             :5; /*!<Specifies the recipient of the request.*/
            NvU32 type                  :2; /*!<Specifies the type, whether this
                                               request goes on device, endpoint,
                                               interface or other.*/
            NvU32 dataTransferDirection :1; /*!<Specifies the data transfer direction.*/
        }stbitfield;
    }bmRequestTypeUnion;
    NvU32 bRequest              :8;         /*!<Specifies the specific request.*/
    NvU32 wValue                :16;        /*!<Specifies the field that varies
                                                according to request.*/
    NvU32 wIndex                :16;        /*!<Specifies the field that varies
                                                according to request; typically used
                                                to pass an index or offset.*/
    NvU32 wLength               :16;        /*!<Specifies the number of bytes to
                                                transfer if there is a data state.*/
}DeviceRequest;

// packing is required to avoid structure padding by compiler
typedef struct UsbDeviceDescriptorRec
{
    NvU8    bLength        ;    //12h Size of this descriptor in bytes.
    NvU8    bDescriptorType;    //01h DEVICE descriptor type.
    NvU16   bcdUSB;             //USB Specification Release Number in Binary-Coded Decimal
                                //(i.e. 2.10 = 210h). This field identifies the
                                //release of the USB Specification with which the device
                                //and its descriptors are compliant.
    NvU8    bDeviceClass;       //00h Class is specified in the interface descriptor.
    NvU8    bDeviceSubClass;    //00h Subclass is specified in the interface descriptor.
    NvU8    bDeviceProtocol;    //00h Protocol is specified in the interface descriptor.
    NvU8    bMaxPacketSize0;    //??h Maximum packet size for endpoint zero.
                                //(only 8, 16, 32, or 64 are valid (08h, 10h, 20h, 40h)).
    NvU16   idVendor;           //????h Vendor ID (assigned by the USB-IF).
    NvU16   idProduct;          //????h Product ID (assigned by the manufacturer).
    NvU16   bcdDevice;          //????h Device release number in binary-coded decimal.
    NvU8    iManufacturer;      //??h Index of string descriptor describing the manufacturer.
    NvU8    iProduct;           //??h Index of string descriptor describing this product.
    NvU8    iSerialNumber;      //??h Index of string descriptor describing the
                                //                       device’s serial number.
    NvU8    bNumConfigurations; //??h Number of possible configurations.
}__attribute__ ((packed))UsbDeviceDescriptor;

/* USB_DT_CONFIG: Configuration descriptor information.
 *
 * USB_DT_OTHER_SPEED_CONFIG is the same descriptor, except that the
 * descriptor type is different.  Highspeed-capable devices can look
 * different depending on what speed they're currently running.  Only
 * devices with a USB_DT_DEVICE_QUALIFIER have any OTHER_SPEED_CONFIG
 * descriptors.
 */
// packing is required to avoid structure padding by compiler
typedef struct UsbConfigDescriptorRec
{
    NvU8  bLength;
    NvU8  bDescriptorType;

    NvU16 wTotalLength;
    NvU8  bNumInterfaces;
    NvU8  bConfigurationValue;
    NvU8  iConfiguration;
    NvU8  bmAttributes;
    NvU8  bMaxPower;
}__attribute__ ((packed))UsbConfigDescriptor;

enum {  UsbConfigDescriptorStructSz     = 9};

/* USB_DT_INTERFACE: Interface descriptor */
// packing is required to avoid structure padding by compiler
typedef struct UsbInterfaceDescriptorRec
{
    NvU8  bLength;
    NvU8  bDescriptorType;

    NvU8  bInterfaceNumber;
    NvU8  bAlternateSetting;
    NvU8  bNumEndpoints;
    NvU8  bInterfaceClass;
    NvU8  bInterfaceSubClass;
    NvU8  bInterfaceProtocol;
    NvU8  iInterface;
}__attribute__ ((packed))UsbInterfaceDescriptor;

enum {  INTERFACE_CLASS_MASS_STORAGE           = 0x08,
        INTERFACE_PROTOCOL_BULK_ONLY_TRANSPORT = 0x50,
        INTERFACE_PROTOCOL_BULK_ZIP_TRANSPORT  = 0x80};

enum {  UsbInterfaceDescriptorStructSz          = 9,
         UsbEndpointDescriptorStructSz          = 7};

/* USB_DT_ENDPOINT: Endpoint descriptor */
// packing is required to avoid structure padding by compiler
typedef struct UsbEndpointDescriptorRec
{
    NvU8  bLength;
    NvU8  bDescriptorType;

    NvU8  bEndpointAddress;
    NvU8  bmAttributes;
    NvU16 wMaxPacketSize;
    NvU8  bInterval;
}__attribute__ ((packed))UsbEndpointDescriptor;

#define ENDPOINT_DESC_ATTRIBUTES_BULK_TYPE             0x2
#define ENDPOINT_DESC_ADDRESS_DIR_MASK                 0x80
#define ENDPOINT_DESC_ADDRESS_DIR_OUT                  0x00
#define ENDPOINT_DESC_ADDRESS_DIR_IN                   0x80
#define ENDPOINT_DESC_ADDRESS_ENDPOINT_MASK            0x0f

/// Defines TRB Types for Command, Event or Transfer
/// Only required TRB Types defined
typedef enum
{
    NvBootTRB_Reserved = 0,
    NvBootTRB_Normal,
    NvBootTRB_SetupStage,
    NvBootTRB_DataStage,
    NvBootTRB_StatusStage,
    NvBootTRB_Isoch,
    NvBootTRB_Link,
    NvBootTRB_EventData,
    NvBootTRB_NOOP,
    NvBootTRB_EnableSlotCommand,
    NvBootTRB_DisableSlotCommand,
    NvBootTRB_TransferEvent = 32,
    NvBootTRB_CommandCompletionEvent,
    NvBootTRB_PortStatusChangeEvent,
}NvBootTRBType;

/// Defines TRT Types
typedef enum
{
    NvBootTRT_NoDataStage = 0,
    NvBootTRT_Rsvd,
    NvBootTRT_OutDataStage,
    NvBootTRT_InDataStage ,
}NvBootTRTType;

/// Defines TRB Completion Codes
typedef enum
{
   NvBootComplqCode_INVALID = 0,
   NvBootComplqCode_SUCCESS,
   NvBootComplqCode_DATA_BUF_ERR,
   NvBootComplqCode_BABBLE_ERR,
   NvBootComplqCode_USB_TRANS_ERR,
   NvBootComplqCode_TRB_ERR,
   NvBootComplqCode_STALL_ERR,
   NvBootComplqCode_RESOURCE_ERR,
   NvBootComplqCode_BW_ERR,
   NvBootComplqCode_NO_SLOTS_ERR,
   NvBootComplqCode_INV_STREAM_TYPE_ERR,
   NvBootComplqCode_SLOT_NOT_EN_ERR,
   NvBootComplqCode_EP_NOT_EN_ERR,
   NvBootComplqCode_SHORT_PKT,
   NvBootComplqCode_RING_UNDERRUN,
   NvBootComplqCode_RING_OVERRUN,
   NvBootComplqCode_VF_EVT_RING_FULL_ERR,
   NvBootComplqCode_PARAM_ERROR,
   NvBootComplqCode_BW_OVERRUN,
   NvBootComplqCode_CTX_STATE_ERR,
   NvBootComplqCode_NO_PING_RSP_ERR,
   NvBootComplqCode_EVT_RING_FULL_ERR,
   NvBootComplqCode_MISS_SVC_ERR,
   NvBootComplqCode_CMD_RING_STOPPED,
   NvBootComplqCode_CMD_ABRT,
   NvBootComplqCode_STOPPED,
   NvBootComplqCode_STOPPED_LEN_INV,
   NvBootComplqCode_CTRL_ABRT_ERR,
   NvBootComplqCode_ISOCH_BUF_OVERRUN,
   NvBootComplqCode_EVT_LOST_ERR,
   NvBootComplqCode_UNDEF_ERR,
   NvBootComplqCode_INV_STREAM_ID_ERR,
   NvBootComplqCode_SEC_BW_ERR,
   NvBootComplqCode_SPLIT_TRANS_ERR,
   NvBootComplqCode_VENDOR_ERR = 192,
   NvBootComplqCode_CTRL_SEQ_ERR = 223,
   NvBootComplqCode_VENDOR_STATUS = 224,
}NvBootComplqCode;

typedef struct SetupTRBRec{
    NvU32 bmRequestType         :8;         /* Request Type */
    NvU32 bmRequest             :8;         /* Request */
    NvU32 wValue                :16;        /* value */
    NvU32 wIndex                :16;        /* Index */
    NvU32 wLength               :16;        /* Length */
    NvU32 TRBTfrLen             :17;        /* Always 8.*/
    NvU32 Rsvd0                 :5;         /* Reserved */
    NvU32 InterrupterTarget     :10;        /* Interrupter Target */
    NvU32 CycleBit              :1;         /* cycle bit */
    NvU32 Rsvd1                 :4;         /* Reserved */
    NvU32 IOC                   :1;         /* Interrupt On Completion */
    NvU32 IDT                   :1;         /* Immediate Data */
    NvU32 Rsvd2                 :3;         /* Reserved */
    NvU32 TRBType               :6;         /* TRB Type */
    NvU32 TRT                   :2;         /* Transfer Type */
    NvU32 Rsvd3                 :14;        /* Reserved */
}SetupTRB;

typedef struct DataStageTRBRec{
    NvU32 DataBufferLo          :32;        /* Data buffer Lo ptr */
    NvU32 DataBufferHi          :32;        /* Data buffer Hi Ptr */
    NvU32 TRBTfrLen             :17;        /* Always 8.*/
    NvU32 TDSize                :5;         /* TD Size */
    NvU32 InterrupterTarget     :10;        /* Interrupter Target */
    NvU32 CycleBit              :1;         /* cycle bit */
    NvU32 ENT                   :1;         /* Evaluate Next TRB */
    NvU32 ISP                   :1;         /* Interrupt-on Short Packet */
    NvU32 NS                    :1;         /* No Snoop */
    NvU32 CH                    :1;         /* Chain bit */
    NvU32 IOC                   :1;         /* Interrupt On Completion */
    NvU32 IDT                   :1;         /* Immediate Data */
    NvU32 Rsvd2                 :3;         /* Reserved */
    NvU32 TRBType               :6;         /* TRB Type */
    NvU32 DIR                   :1;         /* Data transfer direction */
    NvU32 Rsvd3                 :15;        /* Reserved */
}DataStageTRB;


typedef struct StatusStageTRBRec{
    NvU32 Rsvd0                 :32;        /* Rsvd */
    NvU32 Rsvd1                 :32;        /* Rsvd */
    NvU32 Rsvd2                 :22;        /* Rsvd .*/
    NvU32 InterrupterTarget     :10;        /* Interrupter Target */
    NvU32 CycleBit              :1;         /* cycle bit */
    NvU32 ENT                   :1;         /* Evaluate Next TRB */
    NvU32 Rsvd3                 :2;         /* Rsvd */
    NvU32 CH                    :1;         /* Chain bit */
    NvU32 IOC                   :1;         /* Interrupt On Completion */
    NvU32 Rsvd4                 :4;         /* Reserved */
    NvU32 TRBType               :6;         /* TRB Type */
    NvU32 DIR                   :1;         /* Data transfer direction */
    NvU32 Rsvd5                 :15;        /* Reserved */
}StatusStageTRB;

typedef struct NormalTRBRec{
    NvU32 DataBufferLo          :32;        /* Data buffer Lo ptr */
    NvU32 DataBufferHi          :32;        /* Data buffer Hi Ptr */
    NvU32 TRBTfrLen             :17;        /* Always 8.*/
    NvU32 TDSize                :5;         /* TD Size */
    NvU32 InterrupterTarget     :10;        /* Interrupter Target */
    NvU32 CycleBit              :1;         /* cycle bit */
    NvU32 ENT                   :1;         /* Evaluate Next TRB */
    NvU32 ISP                   :1;         /* Interrupt-on Short Packet */
    NvU32 NS                    :1;         /* No Snoop */
    NvU32 CH                    :1;         /* Chain bit */
    NvU32 IOC                   :1;         /* Interrupt On Completion */
    NvU32 IDT                   :1;         /* Immediate Data */
    NvU32 Rsvd2                 :2;         /* Reserved */
    NvU32 BEI                   :1;         /* Block Event Interrupt */
    NvU32 TRBType               :6;         /* TRB Type */
    NvU32 Rsvd3                 :16;        /* Reserved */
}NormalTRB;

typedef struct EventTRBRec{
    NvU32 DataBufferLo          :32;        /* Data buffer Lo ptr */
    NvU32 DataBufferHi          :32;        /* Data buffer Hi Ptr */
    NvU32 TRBTfrLen             :24;        /* TRB Transfer Length.*/
    NvU32 ComplCode             :8;         /* Completion Code */
    NvU32 CycleBit              :1;         /* cycle bit */
    NvU32 Rsvd1                 :1;         /* Reserved */
    NvU32 EvtData               :1;         /* Event Data, when set generated by event TRB */
    NvU32 Rsvd2                 :7;         /* Reserved */
    NvU32 TRBType               :6;         /* TRB Type */
    NvU32 EpId                  :5;         /* Endpoint ID. */
    NvU32 Rsvd3                 :3;         /* Reserved */
    NvU32 SlotId                :8;         /* Slot ID */
}EventTRB;

typedef struct NvddkXusbBootParamsRec
{
    NvU8 ClockDivider;
    /// Specifies the usbh root port number device is attached to
    NvU8 RootPortNumber;

    NvU8 PageSize2kor16k;

    /// used to config the OC pin on the port
    NvU8 OCPin;

    /// used to tri-state the associated vbus pad
    NvU8 VBusEnable;
} NvddkXusbBootParams;

#define EP_CTX_OFFSET       (0)
#define TRBRING_OFFSET      (0x100)
#define TRBDEQ_OFFSET       (0x200)
#define CMD_BUF_OFFSET      (0x300)
#define STS_BUF_OFFSET      (0x400)
#define DATA_BUF_OFFSET     (0x500)
#define BUF_OFFSET          (0x600)
#define CTX_SIZE            ((1*1024*1024) + BUF_OFFSET)
#define CTX_ALIGNMENT       (0x100)

NvError
Nvddk_xusbh_init(NvddkXusbContext* Context);
void
Nvddk_xusbh_deinit(void);

NvError
NvddkXusbReadPage(NvddkXusbContext* Context,
                  const NvU32 Block,
                  const NvU32 Page,
                  NvU8 *Dest,
                  NvU32 Size);
NvError
NvddkXusbWritePage(NvddkXusbContext* Context,
                   const NvU32 Block,
                   const NvU32 Page,
                   NvU8 *Src,
                   NvU32 Size);

void
NvBootXusbUpdateEpContext(NvddkXusbContext *Context,
                          USB_Endpoint_Type EpType);
NvError NvBootXusbUpdWorkQAndChkCompQ(NvddkXusbContext *Context);
NvError NvBootXusbProcessEpStallClearRequest(NvddkXusbContext *Context,
                                             USB_Endpoint_Type EpType);
void NvBootXusbPrepareEndTRB(NvddkXusbContext *Context);


#if defined(__cplusplus)
    }
#endif

#endif /* #ifndef INCLUDED_NVDDK_XUSBH_H */


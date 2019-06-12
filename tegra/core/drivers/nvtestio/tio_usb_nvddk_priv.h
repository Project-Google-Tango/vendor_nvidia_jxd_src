/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TIO_USB_NVDDK_PRIV_H
#define INCLUDED_TIO_USB_NVDDK_PRIV_H


#if defined(__cplusplus)
extern "C"
{
#endif

//-------------------------Private ENUM Declaration---------------------------

/* USB Setup Packet size */
enum { USB_SETUP_PKT_SIZE = 8};

/* USB device decriptor size */
enum { USB_DEV_DESCRIPTOR_SIZE = 18 };

/* USB Configuration decriptor size */
enum { USB_CONFIG_DESCRIPTOR_SIZE = 32 };

/* Define for maximum usb Manufacturer String Length.*/
enum { USB_MANF_STRING_LENGTH = 26 };

/* Define for maximum usb Product String Length */
enum { USB_PRODUCT_STRING_LENGTH = 16 };

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
typedef enum UsbSetupRequestType_t 
{
    /// Specifies the data transfer direction: host-to-device, recipient: device.
    HOST2DEV_DEVICE    = 0x00, 

    /// Specifies the data transfer direction: host-to-device, recipient: interface.
    HOST2DEV_INTERFACE = 0x01,

    /// Specifies the data transfer direction: host-to-device, recipient: endpoint.
    HOST2DEV_ENDPOINT  = 0x02,

    /// Specifies the data transfer direction: host-to-device, recipient: endpoint -- Not available before 5.20.
    HOST2DEV_OTHER     = 0x03,        

    /// Specifies the data transfer direction: device-to-host, transmitter: device.
    DEV2HOST_DEVICE    = 0x80,

    /// Specifies the data transfer direction: device-to-host, transmitter: interface.
    DEV2HOST_INTERFACE = 0x81,

    /// Specifies the data transfer direction: device-to-host, transmitter: endpoint.
    DEV2HOST_ENDPOINT  = 0x82,

    /// Specifies the data transfer direction: device-to-host, transmitter: endpoint. -- Not available before 5.20.
    DEV2HOST_OTHER     = 0x83,        
} UsbSetupRequestType;

/**
 * Specifies the device request type to be sent across the USB.
 * As per USB2.0 Specification.
 */
typedef enum DeviceRequestTypes_t
{
    GET_STATUS                = 0,
    CLEAR_FEATURE             = 1,  /*!< Specifies the clear feature request.*/
    ENDPOINT_CLEAR_FEATURE    = 1,  /*!<Specifies the end point clear feature request.*/
    SET_FEATURE               = 3,  /*!<Specifies the set feature request.*/
    SET_ADDRESS               = 5,  /*!<Specifies the set address request.*/
    GET_DESCRIPTOR            = 6,  /*!<Specifies the device descriptor request.*/
    SET_DESCRIPTOR            = 7,  /*!<Specifies the set descriptor request.*/
    GET_CONFIGURATION         = 8,  /*!<Specifies the set configuration request.*/
    SET_CONFIGURATION         = 9,  /*!<Specifies the set configuration request.*/
    GET_INTERFACE             = 10, /*!<Specifies the get interface request.*/
    SET_INTERFACE             = 11, /*!<Specifies the set interface request.*/
    SYNC_FRAME                = 12, /*!<Specifies the sync frame request.*/

    RESET_PORT                = 13, /*!<Specifies the reset port request -- Not available before 5.40. */
    POWERON_VBUS              = 14, /*!<Specifies the power on VBUS request -- Not available before 5.40.*/
    POWEROFF_VBUS             = 15, /*!<Specifies the power oFF VBUS request -- Not available before 5.40.*/
    PORT_SPEED                = 16, /*!<Specifies the port speed request -- Not available before 5.40.*/
    PORT_STATUS               = 17  /*!<Specifies the port status request -- Not available before 5.40.*/
} DeviceRequestTypes;

/**
 * Defines USB descriptor types.
 * As per USB2.0 Specification.
 */
typedef enum UsbDevDescriptorType_t 
{
    USB_DT_DEVICE             = 1,  /*!<Specifies a device.*/
    USB_DT_CONFIG             = 2,  /*!<Specifies configuration.*/
    USB_DT_STRING             = 3,  /*!<Specifies a string.*/
    USB_DT_INTERFACE          = 4,  /*!<Specifies an interface.*/
    USB_DT_ENDPOINT           = 5,  /*!<Specifies an endpoint.*/
    USB_DT_DEVICE_QUALIFIER   = 6,  /*!<Specifies a device qualifier.*/
    USB_DT_OTHER_SPEED_CONFIG = 7,  /*!<Specifies other speed configuration.*/
    USB_DT_INTERFACE_POWER    = 8,  /*!<Specifies interface power.*/

    USB_DT_HUB,                     /*!<Specifies the hub.*/
    USB_DT_LANGUAGE,                /*!<Specifies the language. */
    USB_DT_PRODUCT,                 /*!<Specifies the product. */
    USB_DT_MANUFACTURER,            /*!<Specifies the manufacturer. */
    USB_DT_FULL_CONFIG,             /*!<Specifies a full speed configuration.*/
    USB_DT_HIGH_CONFIG,             /*!<Specifies a Hi-Speed configuration.*/ 
    USB_DT_OTHER_SPEED_FULL_CONFIG, /*!<Specifies other speed(Full-Speed) configuration.*/
    USB_DT_OTHER_SPEED_HIGH_CONFIG  /*!<Specifies other speed(Hi-Speed) configuration.*/    
} UsbDevDescriptorType;

/**
* Defines USB string descriptor Index
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

typedef enum FeatureSelector_t 
{
    /// Specifies the endpoint halt.
    ENDPOINT_HALT       = 0,
    /// Specifies the device remote wake up.
    DEVICE_REMOTE_WAKEUP= 1,
    /// Specifies test mode.
    TEST_MODE           = 2
} FeatureSelector;

enum 
{
    EP_CTRL_OUT = 0,
    EP_CTRL_IN,
    EP_BLK_OUT,
    EP_BLK_IN
};

/**
* @brief Defines the recipient field in USB device request codes.
*/
typedef enum BmRequestCodeTarget_t
{
    /// Specifies the  USB device is the target of the request.
    TARGET_DEVICE       = 0, 
    /// Specifies an interface on the USB device is the target of the request.
    TARGET_INTERFACE    = 1, 
    /// Specifies an endpoint on the USB device is the target of the request.
    TARGET_ENDPOINT     = 2, 
    /// Specifies an unspecified target on the USB device is specified by the request.
    TARGET_OTHER        = 3, 
    /// Specifies all other targets are reserved.
    TARGET_RESERVED     = 4 
} BmRequestCodeTarget;

/**
* @brief Defines the direction field in USB device request codes.
*/
typedef enum BmRequestCodeDir_t 
{
    /// Specifies the data transfer direction: host-to-device.
    HOST2DEV     = 0, 
    /// Specifies the data transfer direction: device-to-host.
    DEV2HOST     = 1 
} BmRequestCodeDir;

/**
* @brief Defines the type field in USB device request codes.
*/
typedef enum BmRequestCodeType_t 
{
    /// Specifies the standard request type. Required in all devices.
    REQ_STANDARD  = 0, 
    /// Specifies the class-specific request type.
    REQ_CLASS     = 1, 
    /// Specifies the vendor-specific request type.
    REQ_VENDOR    = 2, 
    /// Reserved request.
    REQ_RESERVED  = 3 
} BmRequestCodeType;

/**
* Defines the class-specific request.
*/
typedef enum ClassSpecificRequest_t 
{
    /// ADSC CB 
    ADSC_CB      = 0x21,
    /// Specifies the maximum logical units.
    GET_MAX_LUNS  = 0xFE,
    /// Specifies the bulk reset -- Not available in 5.00.
    BULK_RESET = 0xFF,    
    /// Specifies unknown.
    UNKNOWN        
} ClassSpecificRequest;

/**
* Defines the structure of the setup packet.
*
* Characteristics of request:D7: Data transfer direction 0 = Host-to-device, 1 = Device-to-host
* D6...5: Type: 0 = Standard, 1 = Class, 2 = Vendor, 3 = Reserved
* D4...0: Recipient:0 = Device, 1 = Interface, 2 = Endpoint, 3 = Other
* 4...31 = Reserved
*/
typedef struct SetUpPacket_t 
{
    /// Holds the target.
    BmRequestCodeTarget target:5;
    /// Holds the code type.
    BmRequestCodeType     codeType:2;
    /// Holds the direction in flag.
    BmRequestCodeDir     dirInFlag:1;
    /// Holds the specific request.
    unsigned int request:8;
    /// Word-sized field that varies according to request.
    unsigned int value:16;
    /// Offset: Word-sized field that varies according to request; typically used to pass an index or offset.
    unsigned int index:16;
    /// Holds the number of bytes to transfer if there is a data stage.
    unsigned int length:16;    
} SetupPacket;


//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

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
    0x00,   // bDeviceClass - Class is specified in the interface descriptor.
    0x00,   // bDeviceSubClass - SubClass is specified in the interface descriptor.
    0x00,   // bDeviceProtocol - Protocol is specified in the interface descriptor.
    0x40,   // bMaxPacketSize0 - Maximum packet size for EP0
    0x55,   // idVendor(LSB) - Vendor ID assigned by USB forum
    0x09,   // idVendor(MSB) - Vendor ID assigned by USB forum
    0x15,   // idProduct(LSB) - Product ID assigned by Organization
    0x71,   // idProduct(MSB) - Product ID assigned by Organization
    0x01,   // bcd Device (LSB) - Device Release number in BCD
    0x01,   // bcd Device (MSB) - Device Release number in BCD
    USB_MANF_ID,   // Index of String descriptor describing Manufacturer
    USB_PROD_ID,   // Index of String descriptor describing Product
    0,   // Index of String descriptor describing Serial number
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
    0x10,   // MaxPower in mA - Max Power Consumption of the USB device

    /* Interface Descriptor */
    0x09,   // bLength - Size of this descriptor in bytes
    0x04,   // bDescriptorType - Interface Descriptor Type
    0x00,   // bInterfaceNumber - Number of Interface
    0x00,   // bAlternateSetting - Value used to select alternate setting
    0x02,   // bNumEndpoints - Nos of Endpoints used by this Interface
    0xFF,   // bInterfaceClass - Class code "Vendor Specific Class."
    0xFF,   // bInterfaceSubClass - Subclass code "Vendor specific".
    0xFF,   // bInterfaceProtocol - Protocol code "Vendor specific".
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
    'G', 0x00,
    'o', 0x00,
    'F', 0x00,
    'o', 0x00,
    'r', 0x00,
    'c', 0x00,
    'e', 0x00
};

// Stores the Serial Number String descriptor data (Not used for AP15 Bootrom)
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

// Stores the Device Qualifier Descriptor data
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

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_TIO_USB_NVDDK_PRIV_H

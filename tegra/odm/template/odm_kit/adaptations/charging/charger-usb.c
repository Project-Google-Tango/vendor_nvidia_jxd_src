/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcommon.h"
#include "nvassert.h"
#include "nvodm_pmu.h"
#include "nvodm_charging.h"
#include "stdarg.h"
#include "nvaboot_usbf.h"

#ifdef LPM_BATTERY_CHARGING
// Currents for different charger types
enum {
    IDEV_DCHGR = 1800,  // device current from dedicated charger
    IHCHGR = 500,       // host charger output current
    IHCHGR_MILD = 300,  // with some margin for system power
    IUNIT = 100,        // USB unit current
    IZERO = 0           // 0 mA
};

/* USB Setup Packet size */
enum { USB_SETUP_PKT_SIZE = 8};

/* USB device decriptor size */
enum { USB_DEV_DESCRIPTOR_SIZE = 18 };

/* USB Configuration decriptor size */
enum { USB_CONFIG_DESCRIPTOR_SIZE = 32 };

/* Define for maximum usb Manufacturer String Length.*/
enum { USB_MANF_STRING_LENGTH = 26 };

/* Define for maximum usb Product String Length */
enum { USB_PRODUCT_STRING_LENGTH = 20 };

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

#if 0
/**
 * USB Device Descriptor: 12 bytes as per the USB2.0 Specification
 * Stores the Device descriptor data must be word aligned
 */
NV_ALIGN(4) NvU8 g_NvMicrobootUsbDeviceDescriptor[USB_DEV_DESCRIPTOR_SIZE] =
{
    USB_DEV_DESCRIPTOR_SIZE,   // bLength - Size of this descriptor in bytes
    0x01,   // bDescriptorType - Device Descriptor Type
    0x00,   // bcd USB (LSB) - USB Spec. Release number
    0x02,   // bcd USB (MSB) - USB Spec. Release number (2.0)
    0x00,   // bDeviceClass
    0x00,   // bDeviceSubClass
    0x00,   // bDeviceProtocol
    0x40,   // bMaxPacketSize0 - Maximum packet size for EP0
    0x55,   // idVendor(LSB) - Vendor ID assigned by USB forum
    0x09,   // idVendor(MSB) - Vendor ID assigned by USB forum
    0x01,   // idProduct(LSB) - Product ID
    0x01,   // idProduct(MSB) - Product ID
    0x01,   // bcdDevice (LSB) - Device Release number
    0x01,   // bcdDevice (MSB) - Device Release number
    USB_MANF_ID,   // Index of String descriptor describing Manufacturer
    USB_PROD_ID,   // Index of String descriptor describing Product
    0x00,   // Index of String descriptor describing Serial number
    0x01    // bNumConfigurations - Number of possible configuration
};

/**
 * USB Device Configuration Descriptors:
 * 32 bytes as per the USB2.0 Specification. This contains
 * Configuration descriptor, Interface descriptor and endpoint descriptors.
 */
NV_ALIGN(4) NvU8 g_NvMicrobootUsbConfigDescriptor[USB_CONFIG_DESCRIPTOR_SIZE] =
{
    /* Configuration Descriptor 32 bytes  */
    0x09,   // bLength - Size of this descriptor in bytes
    0x02,   // bDescriptorType - Configuration Descriptor Type
    0x20,   // WTotalLength (LSB) - Total length of data for this configuration
    0x00,   // WTotalLength (MSB) - Total length of data for this configuration
    0x01,   // bNumInterface - Nos of Interface supported by this configuration
    0x01,   // bConfigurationValue
    0x00,   // iConfiguration - Index of this configuration
    0xc0,   // bmAttributes - bit 6: Self Powered, bit 5: Remote Wakeup
    IHCHGR/2, // MaxPower in the unit of 2 mA

    /* Interface Descriptor */
    0x09,   // bLength - Size of this descriptor in bytes
    0x04,   // bDescriptorType - Interface Descriptor Type
    0x00,   // bInterfaceNumber - Number of Interface
    0x00,   // bAlternateSetting - Value used to select alternate setting
    0x02,   // bNumEndpoints - Nos of Endpoints used by this Interface
    0x08,   // bInterfaceClass - Class code
    0x06,   // bInterfaceSubClass - Subclass code
    0x50,   // bInterfaceProtocol - Protocol code
    0x00,   // iInterface - Index of String descriptor describing Interface

    /* Endpoint Descriptor IN EP2 */
    0x07,   // bLength - Size of this descriptor in bytes
    0x05,   // bDescriptorType - ENDPOINT Descriptor Type
    0x81,   // bEndpointAddress - The address of EP on the USB device
    0x02,   // bmAttributes - Bit 1-0: Transfer Type 10: Bulk,
    0x00,   // wMaxPacketSize(LSB) - Maximum Packet Size for this EP (*2)
    0x00,   // wMaxPacketSize(MSB) - Maximum Packet Size for this EP (*2)
    0x00,   // bIntervel - Interval for polling EP

    /** Endpoint Descriptor OUT EP1 */
    0x07,   // bLength - Size of this descriptor in bytes
    0x05,   // bDescriptorType - ENDPOINT Descriptor Type
    0x01,   // bEndpointAddress - The address of EP on the USB device
    0x02,   // bmAttributes - Bit 1-0: Transfer Type 10: Bulk,
    0x00,   // wMaxPacketSize(LSB) - Maximum Packet Size for this EP (*2)
    0x00,   // wMaxPacketSize(MSB) - Maximum Packet Size for this EP (*2)
    0x00    // bIntervel - Interval for polling EP
    // *2: Those are overwritten by NvMicrobootUsbfProcessDeviceDescriptor().
};

// Stores the Manufactures ID sting descriptor data
NV_ALIGN(4) NvU8 g_NvMicrobootUsbManufacturerID[USB_MANF_STRING_LENGTH] =
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
NV_ALIGN(4) NvU8 g_NvMicrobootUsbProductID[USB_PRODUCT_STRING_LENGTH] =
{
    USB_PRODUCT_STRING_LENGTH, // Length of descriptor
    0x03,                      // STRING descriptor type.
    'M', 0x00,
    'I', 0x00,
    'C', 0x00,
    'R', 0x00,
    'O', 0x00,
    'B', 0x00,
    'O', 0x00,
    'O', 0x00,
    'T', 0x00
};

// Stores the Serial Number String descriptor data (Not used for AP15 Bootrom)
NV_ALIGN(4) NvU8 g_NvMicrobootUsbSerialNumber[USB_SERIAL_NUM_LENGTH] =
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
NV_ALIGN(4) NvU8 g_NvMicrobootUsbLanguageID[USB_LANGUAGE_ID_LENGTH] =
{
    /* Language Id string descriptor */
    USB_LANGUAGE_ID_LENGTH,  // Length of descriptor
    0x03,                    // STRING descriptor type.
    0x09, 0x04               // LANGID Code 0: American English 0x409
};

// Stores the Device Qualifier Desriptor data
NV_ALIGN(4) NvU8 g_NvMicrobootUsbDeviceQualifier[USB_DEV_QUALIFIER_LENGTH] =
{
    /* Device Qualifier descriptor */
    USB_DEV_QUALIFIER_LENGTH,   // Size of the descriptor
    6,    // Device Qualifier Type
    0x00, // USB specification version number: LSB
    0x02, // USB specification version number: MSB
    0xFF, // Class Code
    0xFF, // Subclass Code
    0xFF, // Protocol Code
    0x40, // Maximum packet size for other speed
    0x01, // Number of Other-speed Configurations
    0x00  // Reserved for future use, must be zero
};

// Stores the Device Status descriptor data
NV_ALIGN(4) NvU8 g_NvMicrobootUsbDeviceStatus[USB_DEV_STATUS_LENGTH] =
{
    USB_DEVICE_SELF_POWERED,
    0,
};
#endif

NvBool NvOdmIsChargerConnected (void);
NvBool NvOdmStartCharging(NvOdmPmuDeviceHandle pPmu);
NvBool NvOdmStopCharging(NvOdmPmuDeviceHandle pPmu);

NvOdmUsbChargerType ChargerType = NvOdmUsbChargerType_Dummy;
NvU32               s_UsbHostCurrentMax = IUNIT;

NvBool NvOdmIsChargerConnected (void)
{
    //Detects the charger and enumerates
    NvBool ChargerConnected = NvBlDetectUsbCharger();

    if (ChargerConnected)
    {
        //Charger connected, get charger type and handle special cases
        NvBlUsbfGetChargerType(&ChargerType);

        if (ChargerType == NvOdmUsbChargerType_UsbHost)
        {
            // If IHCHGR is assigned here, Whistler PMU lose power
            // sometimes. We have to leave some margin for system power.
            s_UsbHostCurrentMax = 3000;
        }
    }
    return ChargerConnected;
}

NvBool NvOdmStartCharging(NvOdmPmuDeviceHandle pPmu)
{
    NvBool Status;

    switch (ChargerType)
    {
        case NvOdmUsbChargerType_SE0:
        case NvOdmUsbChargerType_SJ:
        case NvOdmUsbChargerType_SK:
        case NvOdmUsbChargerType_SE1:
            Status = NvOdmPmuSetChargingCurrent(pPmu,
                                                NvOdmPmuChargingPath_UsbBus,
                                                3000,
                                                ChargerType);
            NV_ASSERT(Status);
            break;
        case NvOdmUsbChargerType_UsbHost:
        default:
            Status = NvOdmPmuSetChargingCurrent(pPmu,
                                                NvOdmPmuChargingPath_UsbBus,
                                                3000,
                                                ChargerType);
            NV_ASSERT(Status);
            break;
    }
    return Status;
}

NvBool NvOdmStopCharging(NvOdmPmuDeviceHandle pPmu)
{
    NvBool Status;

    Status = NvOdmPmuSetChargingCurrent(pPmu,
                                        NvOdmPmuChargingPath_UsbBus,
                                        IZERO,
                                        NvOdmUsbChargerType_UsbHost);
    NV_ASSERT(Status);
    return Status;
}
#endif

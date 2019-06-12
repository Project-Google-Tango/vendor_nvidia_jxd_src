/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_USBH_SCSI_DRIVER_H
#define INCLUDED_NVDDK_USBH_SCSI_DRIVER_H

#include "nvddk_usbh.h"
#include "nvrm_power.h"
#include "nvassert.h"
#include "nvos.h"

// The response we give to SCSI inquiries.
typedef struct
{
    NvU8 PeripheralQualAndDevType; // Unused
    NvU8 RmbAndReserved; // Removable Media Indicator
    NvU8 Version; // Unused fields
    NvU8 Flags1; // Unused fields
    NvU8 AdditionalLength; // Length of the parms data we support. Just the usual.
    NvU8 SCCS;
    NvU8 Flags2;// More flags
    NvU8 Flags3;
    NvU8 VendorID[8]; // Vendor ID  Place Holder
    NvU8 ProductID[16]; // Product ID  Place Holder
    NvU8 RevisionLevel[4]; // Revision level
    NvU8 SerialNo[20]; //Serial Number Place Holder
}__attribute__ ((packed))Scsi_Inquiry_Response;

typedef struct
{
    NvU8 Reserved[3];
    NvU8 CapacityListLength;// number of bytes in capacity list below, always be 8
    struct
    {
        NvU32 LogicalBlocks; // number of blocks on the device
        NvU8 BlockBytes[4];  // number of bytes in each logical block.
    } capacityList[1];
}__attribute__ ((packed))Scsi_Read_Cap_Response;

/**
 * @brief SCSI Inquiry(0x12h) Command structure, used by the MS subClass Driver
 */
typedef struct
{
    NvU8 OpCode;       /*!<The Op code is the SCSI value which is 0x12h*/
    NvU8 LogicalUnitNum;       /*!<The Logical Unit Number*/
    NvU8 PageCode;       /*!<cmd length=0x11*/
    NvU8 Reserved2;       /*!<reserved 2*/
    NvU8 AllocationLength ;      /*!<allocation length*/
    NvU8 Control ;       /*!<control*/
}__attribute__ ((packed))Inquiry;

/**
* @brief SCSI Inquiry(response) data structure, used by the MS subClass Driver
*/
typedef struct
{
    NvU8    PeripheralQualifier_devtype;
    NvU8    RmbAndReserve;
    NvU8    Version ;                 /*!<ISO version(6-7)   |ECMA version(3-5) |  ANSI-approved version(0-2)*/
    NvU8    AencTrmRespDataFormaT ;       /*!<AENC (bit 7) | TrmIOP(bit6) |     Reserved (4&5)   |  Response data format(0-3)*/
    NvU8    AdditionalLength ;            /*!<*response length -4*/
    NvU8    Reserved1;
    NvU8    Reserved2;
    NvU8    RelAddWbusSyncLinResCmdSft;      /*!<RelAdr(7 bit) | WBus32(6bit) | WBus16(5bit) |  Sync(4bit)  | Linked(3bit) |Reserved(2bit)| CmdQue(1bit) | SftRe(0bit)*/
    NvU8    VendorIdentification[8];     /*!<The Vendor Identification*/
    NvU8    ProductIdentification[16];       /*!<The Product Identification*/
    NvU8    ProductRevisionLevel[4];     /*!<The Product Revision Level*/
}__attribute__ ((packed))InquiryData;

/**
 * @brief SCSI Request Sense(0x03h) Command structure, used by the MS subClass Driver
 */
typedef struct
{
    NvU8 OpCode;       /*!<The Op code is the SCSI value which is 0x03h*/
    NvU8 LogicalUnitNum;       /*!<The Logical Unit Number*/
    NvU8 Reserved[2]         ;       /*!<cmd length=0x11*/
    NvU8 AllocationLength;       /*!<allocation length*/
    NvU8 Control;       /*!<control*/
}__attribute__ ((packed))RequestSense;

/**
* @brief SCSI Request Sense(response) data structure, used by the MS subClass Driver
*/
typedef struct
{
    NvU8       ResponseCode;
    NvU8       Obsolete;
    NvU8       SenseKey;
    NvU8       Info[4];
    NvU8       AdditionalSenseLength;
    NvU8       CommandSpecificInfo[4];
    NvU8       AdditionalSenseCode;
    NvU8       AdditionalSenseCodeQualifier;
    NvU8       FieldReplaceableUnitCode;
    NvU8       SenseKeySpecific[3];
}__attribute__ ((packed))RequestSenseData;

typedef struct
{
    NvU8 OpCode;
    NvU8 Reserved[3];
    NvU8 ControlByte;
}__attribute__ ((packed))PreventAllowMediumRemoval;

/**
* @brief SCSI TestUnitReady(0x00h) Command structure, used by the MS subClass Driver
*/
typedef struct
{
    NvU8 OpCode;                  /*!<Op code value of 00h*/
    NvU8 LogicalUnitNum;                  /*!<Logical unit Number to talk to*/
    NvU8 Reserved[2];
    NvU8 Control;                  /*!<control*/
}__attribute__ ((packed))TestUnitReady;

/**
* @brief SCSI ReadCapacity(0x25h) Command structure, used by the MS subClass Driver
*/
typedef struct
{
    NvU8 OpCode;       /*!<Op Code of 0x25h*/
    NvU8 Lun;       /*!<Logical unit Number*/
    NvU8 LogicalBlockAddress[4];       /*!<Logical Block Adress to read from*/
    NvU8 Reserved[2];
    NvU8 Control;       /*!<control*/
}__attribute__ ((packed))ReadCapacity;

/**
* @brief SCSI FormatUnit(0x04h) Command structure, used by the MS subClass Driver
*/
typedef struct
{
    NvU8 OpCode;       /*!<Op Code of 0x25h*/
    NvU8 SpecialParams;       /*!<Logical unit Number and Defect Mgmt Data*/
    NvU8 VendorSpecific;       /*!<Vendor Specific Data*/
    NvU8 Control;       /*!<control*/
__attribute__ ((packed)) NvU16 Interleave;       /*!<Interleave*/
}__attribute__ ((packed))FormatUnit;

/**
* @brief SCSI ReadCapacity data structure, used by the MS subClass Driver
*/
typedef struct
{
    NvU32 RLogicalBlockAddress;           /*!<The last block address */
    NvU32 BlockLengthInBytes;           /*!<The size of a block.These give the total capacity of the media*/
}__attribute__ ((packed))ReadCapacityData;

/**
 * @brief SCSI Read10(0x28) / Write10(0x2A)Command structure, used by the MS subClass Driver
 */
typedef struct
{
    NvU8 OpCode;       /*!<Op Code of 0x28 for Read*/
    NvU8 LogicalUnitNum;       /*!<Logical Unit Number to Read from*/
    NvU8 LogicalBlockAddress[4];       /*!<Logical Block Address*/
    NvU8 Reserved;
    NvU8 TransferLen[2];           /*!<The transfer Length in bytes*/
    NvU8 Control;       /*!<control*/
}__attribute__ ((packed)) ReadWrite10;


// Command block word for MSD protocol
typedef struct
{
    NvU32 Signature;        // Must be 0x43425355 "USBC"
    NvU32 Tag;
    NvU32 DataTransferLength;
    NvU8 Flags;
    NvU8 Lun;
    NvU8 Length;
    NvU8 Cmd[16];
}__attribute__ ((packed))Usb_Msd_Cbw;

// Command status wrapper for MSD protocol
typedef struct
{
    NvU32 Signature;        // Must be 0x53425355 "USBS"
    NvU32 Tag;
    NvU32 DataResidue;
    NvU8 Status;
}__attribute__ ((packed))Usb_Msd_Csw;

/**
 * Defines the structure of the setup packet.
 *
 * Characteristics of request:D7: Data transfer direction 0 = Host-to-device, 1 = Device-to-host
 * D6...5: Type: 0 = Standard, 1 = Class, 2 = Vendor, 3 = Reserved
 * D4...0: Recipient:0 = Device, 1 = Interface, 2 = Endpoint, 3 = Other
 * 4...31 = Reserved
 */
typedef struct
{
    NvU8 ReqType;// direction << 7 | type << 5 | recipient
    /// Holds the specific request.
    NvU8 Request;
    /// Word-sized field that varies according to request.
    NvU16 Value;
    /// Offset: Word-sized field that varies according to request; typically used to pass an index or offset.
    NvU16 Index;
    /// Holds the number of bytes to transfer if there is a data stage.
    NvU16 Length;
} SetupPacket;

/**
* @brief Defines the USB device descriptor structure.
*
*
*/
typedef struct
{
    NvU8   BLength;             /*!<Specifies the size of the descriptor - 1 byte.  */
    NvU8   BDescriptorType;     /*!<Specifies the DEVICE descriptor type - 1 byte   */
    NvU16   BcdUSB;              /*!<Specifies the USB special rel. number in bcd - 2 bytes.    */
    NvU8   BDeviceClass;        /*!<Specifies the class code - 1 byte.  */
    NvU8   BDeviceSubClass;     /*!<Specifies the sub class code - 1 byte.      */
    NvU8   BDeviceProtocol;     /*!<Specifies the protocol code - 1 byte.       */
    NvU8   BMaxPacketSize0;     /*!<Specifies the maximum packet size of e/p 0 - 1 byte.    */
    NvU16  IdVendor;            /*!<Specifies the vendor ID - 2 bytes.  */
    NvU16  IdProduct;           /*!<Specifies the product ID - 2 bytes. */
    NvU16  BcdDevice;           /*!<Specifies the device release number in bcd - 2 bytes. */
    NvU8   IManufacturer;       /*!<Specifies the manufacturer  string - 1 byte.    */
    NvU8   IProduct;            /*!<Specifies the product string - 1 byte.  */
    NvU8   ISerialNumber;       /*!<Specifies the serial number string index - 1 byte.  */
    NvU8   BNumConfigurations;  /*!<Specifies the number of possible configurations - 1 byte.   */
}__attribute__ ((packed))UsbDeviceDescriptor;

/**
 * @brief Specifies the USB configuration descriptor structure.
 *
 *
 */
typedef struct
{
    NvU8       BLength;                /*!<Specifies the size of this descriptor in bytes - 1 byte.     */
    NvU8       BDescriptorType;        /*!<Specifies the CONFIGURATION descriptor type - 1 byte.    */
    NvU16      WTotalLength;           /*!<Specifies the total length of data returned for this configuration. */
    NvU8       BNumInterfaces;         /*!<Specifies the number of interfaces supported in this configuration. */
    NvU8       BConfigurationValue;    /*!<Specifies the argument for SetConfiguration() requests. */
    NvU8       IConfiguration;         /*!<Specifies the configuration string.      */
    NvU8       BmAttributes;           /*!<Specifies the configuration characteristics. */
    NvU8       BMaxPower;               /*!<Specifies the maximum power consumption.        */
}__attribute__ ((packed))UsbConfigDescriptor;

/**
 * @brief Specifis the USB interface descriptor structure.
 *
 *
 */
typedef struct
{
    NvU8           BLength;            /*!<Specifies the size of descriptor in bytes.*/
    NvU8           BDescriptorType;    /*!<Specifies the INTERFACE descriptor type.*/
    NvU8           BInterfaceNumber;   /*!<Specifies the number of interfaces.  */
    NvU8           BAlternateSetting;  /*!<Specifies the value used to select alternate setting. */
    NvU8           BNumEndpoints;      /*!<Specifies the number of endpoints used in this interface. */
    NvU8           BInterfaceClass;    /*!<Specifies the class code between between 0x0 and 0xFFh excluding these two.*/
    NvU8           BInterfaceSubClass; /*!<Specifies the sub class code.    */
    NvU8           BInterfaceProtocol; /*!<Specifies the protocol code. */
    NvU8           Interface;         /*!<Specifies the description string.    */
}__attribute__ ((packed))UsbInterfaceDescriptor;

/**
 * @brief Specifies the USB endpoint descriptor structure.
 *
 *
 */
typedef struct
{
    NvU8       BLength;            /*!<Specifies the size of descriptor in bytes.*/
    NvU8       BDescriptorType;    /*!<Specifies the ENDPOINT descriptor type.  */
    NvU8       BEndpointAddress;   /*!<Specifies the address of this endpoint. */
    NvU8       BmAttributes;       /*!<Specifies the endpoint attributes.   */
    NvU16      BMaxPacketSize;     /*!<Specifies the maximum packet size.  */
    NvU8       BInterval;          /*!<Specifies the interval for polling the endpoint. */
}__attribute__ ((packed))UsbEndPointDescriptor;

/**
 * @brief Specifies the USB string descriptor structure.
 *
 *
 */
typedef struct
{
    NvU8 BLength;                /*!<Specifies the size of this descriptor. */
    NvU8 BDescriptorType;        /*!<Specifies the STRING descriptor type.*/
    NvU16 BString[1];                 /*!<Specifies the variable length. */
}__attribute__ ((packed))UsbStringDescriptor;

typedef struct DeviceInfoRec
{
    UsbDeviceDescriptor         DevDesc;
    UsbConfigDescriptor         CfgDesc;
    UsbStringDescriptor         StrDesc;
    UsbInterfaceDescriptor      IntfDesc;
    UsbEndPointDescriptor       InEndpoint;
    UsbEndPointDescriptor       OutEndpoint;
    ReadCapacityData            RdCapacityData[6];
    RequestSenseData            CommandSenseData;
    Usb_Msd_Cbw                 Cbw;
    Usb_Msd_Csw                 Csw;
    NvU32                       CBW_tag;
    NvU32                       CntrlEp0Hndle;
    NvU32                       CntrlEpHndle;
    NvU8                        DevAddr;
    NvU8                        MediaPresent;
    NvU8                        MaxLuns;
    NvU8                        MediaLun;
}__attribute__ ((packed))DeviceInfo;

typedef struct IOEndpointRec
{
    NvU32 inEndpoint;
    NvU32 OutEndpoint;
} IOEndpoint;

typedef IOEndpoint* IOEndpointHandle;

typedef struct
{
    NvRmDeviceHandle RmHandle;
    NvDdkUsbhHandle hUsbHost;
    NvOsSemaphoreHandle Semaphore;
} UsbHostContext;

/**
 * @brief Defines the recipient field in USB device request codes.
 */
typedef enum
{
    /// Specifies the  USB device is the target of the request.
    BmRequestCodeTarget_Device          = 0,
    /// Specifies an interface on the USB device is the target of the request.
    BmRequestCodeTarget_Interface    = 1,
    /// Specifies an endpoint on the USB device is the target of the request.
    BmRequestCodeTarget_Endpoint        = 2,
    /// Specifies an unspecified target on the USB device is specified by the request.
    BmRequestCodeTarget_Other          = 3,
    /// Specifies all other targets are reserved.
    BmRequestCodeTarget_Reserved        = 4,
    BmRequestCodeTarget_Force32        = 0x7FFFFFFF
} BmRequestCodeTarget;

/**
 * @brief Defines the direction field in USB device request codes.
 */
typedef enum
{
    /// Specifies the data transfer direction: host-to-device.
    BmRequestCodeDir_Host2Dev = 0,
    /// Specifies the data transfer direction: device-to-host.
    BmRequestCodeDir_Dev2Host = 1,
    //   code direction bit
    BmRequestCodeDir_Shift = 8,
    BmRequestCodeDir_Force32 = 0x7FFFFFFF
} BmRequestCodeDir;

/**
 * @brief Defines the type field in USB device request codes.
 */
typedef enum
{
    /// Specifies the standard request type. Required in all devices.
    BmRequestCodeType_Standard = 0,
    /// Specifies the class-specific request type.
    BmRequestCodeType_Class = 1,
    /// Specifies the vendor-specific request type.
    RBmRequestCodeType_Vendor = 2,
    /// Reserved request.
    BmRequestCodeType_Reserved = 3,
    // code type bit
    BmRequestCodeType_Type_Shift = 8,
    BmRequestCodeType_Force32 = 0x7FFFFFFF
 } BmRequestCodeType;

/**
 * Defines the class-specific request.
 */
typedef enum
{
    /// ADSC CB
    ClassSpecificRequest_Adsc_Cb      = 0x21,
    /// specifies the maximum logical units.
    ClassSpecificRequest_Get_Max_Luns  = 0xFE,
    /// specifies the bulk reset -- not available in 5.00.
    ClassSpecificRequest_Bulk_Reset = 0xFF,
    ClassSpecificRequest_Force32 = 0x7FFFFFFF
}ClassSpecificRequest;

/* Defines for the USB Packet size */
typedef enum
{
    // usb packet size in Full speed = 64 bytes
    Usb_Speed_Pkt_Size_Full = 0x40,
    // usb packet size in High speed = 512 bytes
    Usb_Speed_Pkt_Size_High = 0x200,
    Usb_Speed_Pkt_Size_Force32 = 0x7FFFFFFF
}Usb_Speed_Pkt_Size;

/**
 * defines usb descriptor types.
 */
typedef enum
{
    DescriptorType_Usb_Dt_Device                = 1,    /*!<specifies a device.*/
    DescriptorType_Usb_Dt_Config                = 2,    /*!<specifies configuration.*/
    DescriptorType_Usb_Dt_String                = 3,    /*!<specifies a string.*/
    DescriptorType_Usb_Dt_Interface            = 4,    /*!<specifies an interface.*/
    DescriptorType_Usb_Dt_Endpoint                = 5,    /*!<specifies an endpoint.*/
    DescriptorType_Usb_Dt_Device_qualifier        = 6,    /*!<specifies a device qualifier.*/
    DescriptorType_Usb_Dt_Other_Speed_Config    = 7,    /*!<specifies other speed configuration.*/
    DescriptorType_Usb_Dt_Interface_Power        = 8,     /*!<specifies interface power.*/
    DescriptorType_Usb_Dt_Hub,                         /*!<specifies the hub.*/
    DescriptorType_Usb_Dt_Language,                    /*!<specifies the language. */
    DescriptorType_Usb_Dt_Product,                     /*!<specifies the product. */
    DescriptorType_Usb_Dt_Manufacturer,                 /*!<specifies the manufacturer. */
    DescriptorType_Usb_Dt_Full_Config,         /*!<specifies a full speed configuration.*/
    DescriptorType_Usb_Dt_High_Config,                  /*!<specifies a hi-speed configuration.*/
    DescriptorType_Usb_Dt_Other_Speed_Full_Config,     /*!<specifies other speed(full-speed) configuration.*/
    DescriptorType_Usb_Dt_Other_Speed_High_Config,     /*!<specifies other speed(hi-speed) configuration.*/
    DescriptorType_Forc32               = 0x7FFFFFFF
}DescriptorType;

/**
 * defines usb sring descriptor index
 * as per usb2.0 specification.
 */
typedef enum
{
    // specifies a language id string descriptor index
    StringDescriptorIndex_Usb_Language_Id = 0,
    // specifies a manufacturer id string descriptor index
    StringDescriptorIndex_Usb_Manf_Id = 1,
    // specifies a product id string descriptor index
    StringDescriptorIndex_Usb_Prod_Id = 2,
    // specifies a serial no string descriptor index
    StringDescriptorIndex_Usb_Serial_Id = 3,

    StringDescriptorIndex_Force32 = 0x7FFFFFFF
} StringDescriptorIndex;

/**
 * @brief determines the type of a device. @sa usbinterfacedescriptor::binterfaceclass.
 *
 *
 */
typedef enum
{
    DeviceClass_Audio               =0x1,   /*!<specifies the audio class.*/
    DeviceClass_Cdc_Control     =0x2,   /*!<specifies the cdc control. */
    DeviceClass_Hid                 =0x3,   /*!<specifies the human interface class.*/
    DeviceClass_Physical            =0x5,   /*!<specifies the physical.*/
    DeviceClass_Image               =0x6,   /*!<specifies the image class.*/
    DeviceClass_Printer             =0x7,   /*!<specifies the printer class.*/
    DeviceClass_MassStorage        =0x8,   /*!<specifies the mass storage class.*/
    DeviceClass__Hub                 =0x9,   /*!<specifies the hub class.*/
    DeviceClass_CdcData            =0xA,   /*!<specifies the cdc data.*/
    DeviceClass_SmartCard  =0xB,   /*!<specifies the chip or smart card class.*/
    DeviceClass_ApplicationSpecific=0xFE,  /*!<specifies applicaiton specific.*/
    DeviceClass_VendorSpecific    =0xFF,   /*!<*specifies vendor specific.*/
    DeviceClass_Force32     = 0x7FFFFFFF
}DeviceClass;

/**
 * @brief determines the type of a device. @sa usbinterfacedescriptor::binterfacesubclass.
 *
 *
 */
typedef enum
{
    DeviceSubClass_Rbc         = 0x1,  /*reduced block commands*/
    DeviceSubClass_Sff_atapi   = 0x2,  /* sff 8020 atapi */
    DeviceSubClass_Qic_157     = 0x3,  /* qic commands */
    DeviceSubClass_Ufi         = 0x4,  /* ufi, floppy*/
    DeviceSubClass_Sff_8070i   = 0x5,  /* sff 8070 atapi */
    DeviceSubClass_Trans  = 0x6,  /* scsi transparent commands set*/
    DeviceSubClass_Force32 = 0x7FFFFFFF
}DeviceSubClass;

/**
 * @brief determines the type of a device. @sa usbinterfacedescriptor::binterfaceprotocol.
 *
 *
 */
typedef enum
{
    DeviceProtocolClass_Cntrl_Bulk_int_cc        = 0x0,  /* control bullk interrupt with command completion interrupt. */
    DeviceProtocolClass_Cntrl_Bulk_Int_Nocc  = 0x1,  /* control bullk interrupt with no command completion interrupt.*/
    DeviceProtocolClass_Bulk_Only_Trnspt    = 0x50, /*bulk only transport protocol*/
    DeviceProtocolClass_Force32         = 0x7FFFFFFF
}DeviceProtocolClass;

/* SCSI Command codes*/
typedef enum
{
    ScsiCommands_Test_Unit_Ready = 0x00,
    ScsiCommands_Request_Sense = 0x03,
    ScsiCommands_Inquiry = 0x12,
    ScsiCommands_Mode_Sense_06 = 0x1A,
    ScsiCommands_Mode_Sense_10 = 0x5A,
    ScsiCommands_Read_Format_Capacities = 0x23,
    ScsiCommands_Read_Capacity_10 = 0x25,
    ScsiCommands_Read_10 = 0x28,
    ScsiCommands_Write_10 = 0x2A,
    ScsiCommands_Verify_10 = 0x2F,
    ScsiCommands_Stop_Start_Unit = 0x1B,
    ScsiCommands_Prevent_Allow_Medium_Removal = 0x1E,
    ScsiCommands_Maintanence_out = 0xA4,
    ScsiCommands_FormatUnit = 0x04,
    ScsiCommands_Force32 = 0x7FFFFFFF
} ScsiCommands;

typedef enum
{
    RequestSenseKey_No_Sense = 0,
    RequestSenseKey_Not_Ready = 2,
    RequestSenseKey_Medium_Error = 3,
    RequestSenseKey_Unit_Attention = 6,
    // Additional sense key codes
    RequestSenseKey_Not_Ready_To_Ready = 0x28,
    RequestSenseKey_Power_On = 0x29,
    RequestSenseKey_No_Medium = 0x3A,
    RequestSenseKey_Ready_In_Progress = 0x04,
    RequestSenseKey_Force32 = 0x7FFFFFFF
}RequestSenseKey;

typedef enum
{
    ScsiCommandLen_Inq_TestUnit     = 6,
    ScsiCommandLen_WrRd10  = 10,
    ScsiCommandLen_ReqSense  = 5,
    ScsiCommandLen_RdFormatCap  = 0xFC,
    ScsiCommandLen_FormatUnit  = 48,
    ScsiCommandLen_Force32          = 0x7FFFFFFF
}ScsiCommandLen;

/**
 * @brief specifies the device request type to be sent across the usb.
 */
typedef enum
{
    DeviceRequestTypes_Get_Status                    = 0,
    DeviceRequestTypes_Clear_Feature                = 1,            /*!< specifies the clear feature request.*/
    DeviceRequestTypes_Endpoint_Clear_Feature    = 1,            /*!<specifies the end point clear feature request.*/
    DeviceRequestTypes_Set_Feature                = 3,             /*!<specifies the set feature request.*/
    DeviceRequestTypes_Set_Address                = 5,            /*!<specifies the set address request.*/
    DeviceRequestTypes_Get_Descriptor                = 6,            /*!<specifies the device descriptor request.*/
    DeviceRequestTypes_Set_Descriptor                = 7,               /*!<specifies the set descriptor request.*/
    DeviceRequestTypes_Get_Configuration            = 8,               /*!<specifies the set configuration request.*/
    DeviceRequestTypes_Set_Configuration            = 9,               /*!<specifies the set configuration request.*/
    DeviceRequestTypes_Get_Interface                = 10,            /*!<specifies the get interface request.*/
    DeviceRequestTypes_Set_Interface                = 11,           /*!<specifies the set interface request.*/
    DeviceRequestTypes_Sync_Frame                    = 12,            /*!<specifies the sync frame request.*/
    DeviceRequestTypes_Port_Reset                    = 13,            /*!<specifies the reset port request -- not available before 5.40. */
    DeviceRequestTypes_Poweron_Vbus                = 14,            /*!<specifies the power on vbus request -- not available before 5.40.*/
    DeviceRequestTypes_Poweroff_Vbus                = 15,             /*!<specifies the power off vbus request -- not available before 5.40.*/
    DeviceRequestTypes_Port_Speed                    = 16,             /*!<specifies the port speed request -- not available before 5.40.*/
    DeviceRequestTypes_Port_Status                = 17,                /*!<specifies the port status request -- not available before 5.40.*/
    DeviceRequestTypes_Force32                  = 0x7FFFFFFF
} DeviceRequestTypes;

// MSD command status values
typedef enum
{
    Msd_Command_Status_Passed = 0,
    Msd_Command_Status_Failed = 1,
    Msd_Command_Status_Phase_Error = 2,
    Msd_Command_Status_Force32 = 0x7FFFFFFF
} Msd_Command_Status;

/**
 * @brief Defines the bulk status values.
 *
 *
 */
typedef enum
{
    BULK_STAT_OK =0x0,                  /*!<Indicates success.*/
    BULK_STAT_COMMAND_FAIL =0x1,        /*!<Indicates failure code*/
    BULK_STAT_PHASE_ERROR =0x2,          /*!<Indicates an error returned by the device specifies that the results of processing further CBWs will be indeterminate until the device is reset.*/
    BulkStatusCSW_Force32   = 0x7FFFFFFF
}BulkStatusCSW;


/*---------------------------------Scsi Commands--------------------------------------*/

NvError
scsiInquiryCommand(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun);

NvError
scsiReadCapacityCommand(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun);

NvError
scsiReadFormatCapacities(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun);

NvError
scsiRead(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun,
    NvU32 lba,
    NvU32 numOfSectors,
    NvU8* pDataBuffer);

NvError
scsiWrite(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun,
    NvU32 lba,
    NvU32 numOfSectors,
    NvU8* pDataBuffer);

NvError
scsiTestUnitReady(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun);

NvError
scsiRequestSenseCommand(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun);

NvError
scsiFormatUnit(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo * pdevInfo,
    NvU32 blkInhdl,
    NvU32 blkOuthdl,
    NvU8 lun,
    NvU8 FmtData,
    NvU8 CmpList,
    NvU8 DftListFmt,
    NvU16 Interleave);

/*-------------------------------End Of Scsi Commands---------------------------------*/

void
SetCommandBlockWrapper(
    NvU32 transferLen,
    DeviceInfo *pdevInfo,
    NvU32 flags,
    NvU8 lun,
    NvU8 commandLen,
    NvU8 *commandBlock);

NvError
GetDescriptor(
    NvDdkUsbhHandle hUsbh,
    NvU32 ephandle,
    DeviceRequestTypes descType,
    NvU8* pBuffer,
    NvU16 Value,
    NvU16 Index,
    NvU32* requestLen);

NvError EnumerateDevice(NvDdkUsbhHandle hUsbh,DeviceInfo *pdevInfo);

NvError GetMediaInfo(
    NvDdkUsbhHandle ghUsbh,
    DeviceInfo *pdevInfo,
    NvU32 bIn,
    NvU32 bout,
    NvU8 lun);

NvError BulkOnlyResetCommand(NvDdkUsbhHandle hUsbh, DeviceInfo *pdevInfo);

#endif


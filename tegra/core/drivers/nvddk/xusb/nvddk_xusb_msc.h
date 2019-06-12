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
 * Internal definitions for using Mass Storage Class protocol
 */

#ifndef INCLUDED_NVDDK_XUSB_MSC_H
#define INCLUDED_NVDDK_XUSB_MSC_H

#include "nvcommon.h"



#if defined(__cplusplus)
extern "C"
{
#endif

extern NvU8   *BufferXusbCmd;    // 32 bytes are good enough for MSC BOT commands
extern NvU8   *BufferXusbStatus; // 32 bytes are good enough for MSC BOT status
extern NvU8   *BufferXusbData;   // 256 bytes are good enough for MSC BOT command
                                 // response and also good enough for control data TRB
                                 // during enumeration

/////////////////////////////////////////////////////////////////////////////////////
//For sense key, additional sense key opcodes and any details regarding commands,
//refer to http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf
//
// Bulk Only Transport Protocol i.e. BOT for USB mass storage device
//http://www.usb.org/developers/devclass_docs/usbmassbulk_10.pdf
//
// CDBs i.e. Command Descriptor Block. Size of this block is fixed to 12 (0x0c)
// Refer to http://www.usb.org/developers/devclass_docs/usb_msc_boot_1.0.pdf
///////////////////////////////////////INQUIRY Command ///////////////////////////
//This requests the information regarding parameters of the device to be sent to the host

#define CDB_SIZE                             16

enum
{
    INQUIRY_CMD_LEN              = 0x6,
    READ10_CMD_LEN               = 0xA,
    REQUESTSENSE_CMD_LEN         = 0x0C,
    TESTUNITREADY_CMD_LEN        = 0x06,
    READ_CAPACITY_CMD_LEN        = 0xA,
    READ_FORMAT_CAPACITY_CMD_LEN = 0xA,
    WRITE10_CMD_LEN              = 0xA,
};
enum
{
    INQUIRY_CMD_OPCODE              = 0x12,
    READ10_CMD_OPCODE               = 0x28,
    REQUESTSENSE_CMD_OPCODE         = 0x03,
    TESTUNITREADY_CMD_OPCODE        = 0x00,
    READ_CAPACITY_CMD_OPCODE        = 0x25,
    READ_FORMAT_CAPACITY_CMD_OPCODE = 0x23,
    WRITE10_CMD_OPCODE              = 0x2A,
    MODE_SENSE10_CMD_OPCODE         = 0x5a
};

typedef struct InquiryCDBRec
{
    NvU8    opcode                  ;  // should be set to 0x12
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    Resvd2                  ;
    NvU8    Resvd3                  ;
    NvU8    AllocationLen           ;  // maximum number of bytes of inquiry data
                                       //  to be returned. Should be set to 0x24
    NvU8    Resvd4                  ;
    NvU8    Resvd5[6]                  ;
}__attribute__((packed)) InquiryCDB;


//Peripheral Device Type field identifies the device currently connected.
//Host shall support PDT codes 0x0, 0x5, 0x7, 0xe. All other are out of scope
typedef enum
{
    PDT_SBC_DIRECT_ACCESS = 0x0,
    PDT_CDROM             = 0x5,
    PDT_OPTICAL_MEMORY    = 0x7,
    PDT_RBC_DIRECT_ACCESS = 0xe
}PDTSupportedType;

typedef struct InquiryResponseRec
{
    NvU8    Resvd1           :3;
    NvU8    PeripheralDevTyp :5;
    NvU8    Rmb              :1; //if 0 then medimum is not removable else it is removable
    NvU8    Resvd2           :7;
    NvU8    Resvd3           ;
    NvU8    Resvd4           ;
    NvU8    AdditionalLen    ;
    NvU8    Resvd6[3]        ;
    NvU8    VendorId[8]      ;   //host shall support ASCII graphic codes (0x20 to 0x7e)
    NvU8    ProductId[16]    ;   //host shall support ASCII graphic codes (0x20 to 0x7e)
    NvU8    ProductRevLvl[4] ;   //host shall support ASCII graphic codes (0x20 to 0x7e)
}__attribute__((packed)) InquiryResponse;

///////////////////////////////////////READ(10) Command ///////////////////////////
//This requests the device to transfer data to the host



typedef struct Read10CDBRec
{
    NvU8    opcode                  ;  // should be set to 0x28
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    LogicalBlkAddrByte4_msb ;  //logical block at which the read operation begins
    NvU8    LogicalBlkAddrByte3     ;
    NvU8    LogicalBlkAddrByte2     ;
    NvU8    LogicalBlkAddrByte1_lsb ;
    NvU8    Resvd2                  ;
    NvU8    TransferLen_msb         ;  //transfer length field specifies the number of
                                       //contiguous logical blocks of data
    NvU8    TransferLen_lsb         ;  //to be transferred. value of 0 indicates
                                       //that no logical blocks are transferred.
    NvU8    Resvd3                  ;
    NvU8    Resvd4[2]                  ;
}__attribute__((packed)) Read10CDB;

///////////////////////////////////////REQUEST_SENSE Command ///////////////////////////
//This requests the device to transfer sense data to the host
//Whenever an error is reported, host should issue this command to receive the
//sense data describing what caused the error condition. If host issues some other
//command then sense data is lost Sense data is available if an error condition
//(CHECK CONDITION) is reported or other info is available in any field.
//If device has no other sense data available to return, it returns a sense key of
//NO SENSE and an additional sense code of NO ADDITIONAL SENSE INFORMATION
//If a device returns CHECK CONDITION status for a REQUEST SENSE command,
//the sense data may be invalid. This could occur if a non-zero resvd bit is detected
//in cmd packet or the device is malfunctioning Devices shall be capable of returning
//at least 18 bytes of data in response to a REQUEST SENSE command.
//If the Allocation Length is 18 or greater, and a Device returns less than 18 bytes
//of data, the Host Computer should assume that the bytes not transferred are zeros.
//Host Computers can determine how much sense data has been returned by examining
//the Allocation Length parameter in the Command Packet and the Additional Sense
//Length in the sense data. Devices do not adjust the Additional Sense Length to
//reflect truncation if the Allocation Length is less than the sense data available.

//For sense key and additional sense key opcodes, refer to
//http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf

typedef struct RequestSenseCDBRec
{
    NvU8    opcode                  ;
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    Resvd2                  ;
    NvU8    AllocationLen           ; //maximum number of bytes of sense data
                                      //the host can receive
    NvU8    Resvd3[8]               ;
}__attribute__((packed)) RequestSenseCDB;

typedef struct RequestSenseResponseRec
{
    NvU8    Valid                   :1; //A Valid bit of zero indicates that the
                                        //Information field is not as defined in this
                                        //Specification. A Valid bit of one indicates
                                        //that the Information field contains valid
                                        //information as defined in this Specification
    NvU8    ErrorCode               :7; //either 0x70 or 0x71
    NvU8    SegNum                  ;
    NvU8    Resvd1                  :2;
    NvU8    Ili                     :1;
    NvU8    Resvd2                  :1;
    NvU8    SenseKey                :4;
    NvU8    Information[4]          ;  //This field is command-specific; it is typically
                                       //used by some commands to return a logical block
                                       //address denoting where an error occurred. If this
                                       //field has a value, the Valid bit shall be set to 1
    NvU8    AdditionalSenseLen      ;  //Device sets the value of this field to ten,
                                       //to indicate that ten more bytes of
                                       //sense data follow this field
    NvU8    CommandInfo[4]          ;
    NvU8    AdditionalSenseCode     ;
    NvU8    AdditionalSenseCodeQual ;
    NvU8    Resvd3[4]               ;
}__attribute__((packed)) RequestSenseResponse;

//Device stores the execution result status of every command block as Sense Data.
//Sense Data is returned to the host by the REQUEST SENSE command.
//Sense Data consists of three levels of error codes of increasing detail.
//The intention is to provide a top-down approach for a host to determine information
//relating to the error and exception conditions. The Sense Key provides generic
//categories in which error and exception conditions can be reported.
//Hosts would typically use sense keys for high-level error recovery procedures.
//Additional Sense Codes provide further detail describing the sense key.
//Additional Sense Code Qualifiers add further detail to the additional sense code.
//The Additional Sense Code and Additional Sense Code Qualifier can be used by hosts
//where sophisticated error recovery procedures require detailed information describing
//the error and exception conditions

typedef enum
{
    NO_SENSE        = 0, //Indicates that there is no specific sense key information
                         //to be reported.This would be the case for a successful command
    RECOVERED_ERROR = 1, //Indicates that the last command completed successfully
                         //with some recovery action performed by the device.
                         //Details may be determinable by examining the additional sense
                         //bytes and the Information field.When multiple recovered errors
                         //occur during one command, the choice of which
                         //error to report is device specific
    NOT_READY,           //Indicates that the device cannot be accessed. Operator
                         //intervention may be required to correct this condition
    MEDIUM_ERROR,        //Indicates that the command terminated with a non-recovered
                         //error condition that was probably caused by a flaw in the
                         //medium or an error in the recorded data. This sense key may
                         //also be returned if the device is unable to distinguish
                         //between a flaw in the medium and a specific hardware failure
    HARDWARE_ERROR,      //Indicates that the device detected a non-recoverable
                         //hardware failure while performing the command or during
                         //a self test
    ILLEGAL_REQUEST,     //Indicates that there was an illegal parameter in the Command
                         //Packet or in the additional parameters supplied as data for
                         //some commands. If the device detects an invalid parameter
                         //in the Command Packet, then it shall terminate the command
                         //without altering the medium. If the device detects an
                         //invalid parameter in the additional parameters supplied
                         //as data, then the device may have already altered the medium
    UNIT_ATTENTION,      //Indicates that the removable medium may have been changed
                         //or the device has been reset
    DATA_PROTECT,        //Indicates that a command that writes the medium was attempted
                         //on a block that is protected from this operation.
                         //The write operation was not performed
    BLANK_CHECK,         //Indicates that a write-once device or a sequential-access device
                         //encountered blank medium or format-defined end-of-data
                         //indication while reading or a write-once device encountered
                         //a non-blank medium while writing
    VENDOR_SPECIFIC,     //This sense key is available for reporting vendor specific
                         //conditions
    RESERVED1,
    ABORTED_COMMAND,     //Indicates that the device has aborted the command.
                         //The host may be able to recover by trying the command again
    RESERVED2,
    VOLUME_OVERFLOW,     //Indicates that a buffered peripheral device has reached the
                         //end-of-partition and data may remain in the buffer that
                         //has not been written to the medium
    MISCOMPARE,          //Indicates that the source data did not match the data
                         //read from the medium
    RESERVED3   = 15
}MscSenseKeyTyp;

//The Additional Sense Code (ASC) field indicates further information related to
//the error or exception condition reported in the Sense Key field. Support of the
//Additional Sense Codes not explicitly required by this Specification is optional.
//If the device does not have further information related to the error or exception
//condition, the Additional Sense Code is set to NO ADDITIONAL SENSE INFORMATION.
//The Additional Sense Code Qualifier (ASCQ) indicates detailed information related
//to the Additional Sense Code. The ASCQ is optional. If the error or exception
//condition is reportable by the UFI device, the value returned shall be as specified
//in the appropriate section. If the UFI device does not have detailed information
//related to the error or exception condition, the ASCQ shall be set to zero.
//The Additional Sense Bytes field may contain command specific data,
//peripheral device specific data, or vendor specific data that further defines
//the nature of the CHECK CONDITION status.


//FOR ASC and ASCQ -
//Refer to Table-52 at http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf


///////////////////////////////////////TEST UNIT READY Command ///////////////////////////
//The TEST UNIT READY command provides a means to check if the Device is ready.
//This is not a request for a self-test. If the Device would accept an appropriate
//medium-access command without returning CHECK CONDITION status, this command
//returns a GOOD status. If the Device cannot become operational or is in a state
//such that a Host Computer action is required to make the Device ready, the
//Device returns CHECK CONDITION status with a sense key of NOT READY.

typedef struct TestUnitReadyCDBRec
{
    NvU8    opcode                  ;
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    Resvd2[10]              ;
}__attribute__((packed)) TestUnitReadyCDB;

//The TEST UNIT READY command is useful in that it allows a Host Computer to poll
//a Device until it is ready without the need to allocate space for returned data.
//It is especially useful to check media status. Devices are expected to respond
//promptly to indicate the current status of the Device. Only status stage and
//no data stage

//////////////////////////////READ CAPACITY Command ///////////////////////////
//The READ CAPACITY command provides a means for the host computer to request
//information regarding the capacity of the installed medium of the device.

typedef struct ReadCapacityCDBRec
{
    NvU8    opcode                  ;
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    Resvd2[10]              ;
}__attribute__((packed)) ReadCapacityCDB;

typedef struct ReadCapacityResponseRec
{
    union
    {
        NvU32   LastLogicalBlkAddr;
        struct
        {
            NvU8    LastLogicalBlkAddrByte4_msb     ;
            NvU8    LastLogicalBlkAddrByte3         ;
            NvU8    LastLogicalBlkAddrByte2         ;
            NvU8    LastLogicalBlkAddrByte1_lsb     ;
        }stLastLogicalBlkAddrByte;
    }LastLogicalBlkAddrUnion;
    union
    {
        NvU32   BlockLenInByte;
        struct
        {
            NvU8    BlockLenInByte_Byte4_msb        ;
            NvU8    BlockLenInByte_Byte3            ;
            NvU8    BlockLenInByte_Byte2            ;
            NvU8    BlockLenInByte_Byte1_lsb        ;
        }stBlockLenInByte;
    }BlockLenInByteUnion;
}ReadCapacityResponse;

///////////////////////READ FORMAT CAPACITY Command ////////////////////////
//The READ FORMAT CAPACITIES command allows the host to request a list of the
//possible capacities that can be formatted on the currently installed medium.
//If no medium is currently installed, the device shall
//return the maximum capacity that can be formatted by the device



typedef struct ReadFormatCapacityCDBRec
{
    NvU8    opcode                  ;
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    Resvd2[5]               ;
    NvU8    AllocationLen_msb       ; //specifies the maximum number of bytes of
                                      //format data the Host can receive. If this is less
                                      //than the size of capacity data, the device
                                      //returns only the number of bytes requested. However,
                                      //the device shall not adjust the Capacity
                                      //List Length in the format data to reflect truncation
    NvU8    AllocationLen_lsb       ;
    NvU8    Resvd3[3]               ;
}__attribute__((packed)) ReadFormatCapacityCDB;

//Upon receipt of this command block, the device returns a Capacity List to the host
//on the Bulk In endpoint. No media in FDU (Floppy disk unit): Capacity List Header +
//Maximum Capacity Header Media in FDU (Floppy disk unit): Capacity List Header +
//Current Capacity Header + Formattable Capacity Descriptors (if any)

//Capacity List Header
typedef struct ReadFmtCapacityRespCapListHdrRec
{
    NvU8    Resvd1[3]                       ;
    NvU8    CapacityListLen                 ;  //specifies the length in bytes of the
                                               //Capacity Descriptors that follow. Each
                                               //Capacity Descriptor is eight bytes in
                                               //length, making the Capacity List Length
                                               //equal to eight times the number of
                                               //descriptors
}__attribute__((packed)) ReadFmtCapacityRespCapListHdr;

//Current/Maximum Capacity Header
//The Current/Maximum Capacity Descriptor describes the current medium capacity
//if media is mounted in the UFI device and the format is known, else the maximum
//capacity that can be formatted by the UFI device if no media is mounted, or if the
//mounted media is unformatted, or if the format of the mounted media is unknown
typedef struct ReadFmtCapacityRespCurCapHdr
{
    union
    {
        NvU32   BlockNumInByte;
        struct
        {
            NvU8    BlockNumInByte_Byte4_msb ; //Number of Blocks field indicates
                                               //the total number of addressable
                                               //blocks for the descriptor’s media type
            NvU8    BlockNumInByte_Byte3     ;
            NvU8    BlockNumInByte_Byte2     ;
            NvU8    BlockNumInByte_Byte1_lsb ;
        }stBlockNumInByte;
    }BlockNumInByteUnion;
    NvU8    Resvd1                   :6;
    NvU8    DescriptorCode           :2; //specifies the type of descriptor
                                         //returned to the Host.
    NvU8    BlockLenInByte_Byte3_msb ;   //specifies the length in bytes of each
                                         //logical block for the given capacity descriptor.
    NvU8    BlockLenInByte_Byte2     ;
    NvU8    BlockLenInByte_Byte1_lsb ;
}ReadFmtCapacityRespCurCapHdr;

typedef struct ReadFmtCapacityResponseRec
{
    ReadFmtCapacityRespCapListHdr   CapListHdr;
    ReadFmtCapacityRespCurCapHdr    CurCapHdr;
}ReadFmtCapacityResponse;

//Descriptor Code   Descriptor Type
//-----------------------------------
//    01b   Unformatted Media     - Maximum formattable capacity for this cartridge
//    10b   Formatted Media       - Current media capacity
//    11b   No Cartridge in Drive - Maximum formattable capacity for any cartridge
typedef enum
{
    UNFORMATTED_MEDIA   = 1,
    FORMATTED_MEDIA     = 2,
    NO_CARTRIDGE        = 3,
}DescriptorCodeReadFmtCapacity;

///////////////////////////////////WRITE(10) Command ///////////////////////
//The WRITE(10) command requests that the Device write the data transferred by the
//Host Computer to the medium. The Logical Block Address field specifies the logical
//block at which the write operation begins. The Transfer Length field specifies
//the number of contiguous logical blocks of data that are transferred. A Transfer
//Length of zero indicates that no logical blocks are transferred. This condition
//is not considered an error and no data is written.
//Any other value indicates the number of logical blocks that are transferred.


typedef struct Write10CDBRec
{
    NvU8    opcode                  ;
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :5;
    NvU8    LogicalBlkAddrByte4_msb ; //logical block at which the write operation begins
    NvU8    LogicalBlkAddrByte3     ;
    NvU8    LogicalBlkAddrByte2     ;
    NvU8    LogicalBlkAddrByte1_lsb ;
    NvU8    Resvd2                  ;
    NvU8    TransferLen_msb         ; //transfer length field specifies the number of
                                      //contiguous logical blocks of data
    NvU8    TransferLen_lsb         ; //to be transferred. value of 0 indicates that
                                      //no logical blocks are transferred.
    NvU8    Resvd3                  ;
    NvU8    Resvd4                  ;
    NvU8    Resvd5                  ;
}Write10CDB;

///////////////////////////MODE SENSE(10) Command ///////////////////////////
//The MODE SENSE command provides a means for a Device to report parameters to
//the Host Computer. It is a complementary command to the MODE SELECT command.

typedef struct ModeSenseCDBRec
{
    NvU8    opcode                  ;
    NvU8    LogicalUnitNum          :3;
    NvU8    Resvd1                  :1;
    NvU8    DBD                     :1;
    NvU8    Resvd2                  :3;
    NvU8    Resvd3                  :2;
    NvU8    PageCode                :6;
    NvU8    Resvd4[4]               ;
    NvU8    ParamListLen_msb        ;
    NvU8    ParamListLen_lsb        ;
    NvU8    Resvd5[3]               ;
}__attribute__((packed)) ModeSenseCDB;
//TODO : MODE sense response structure

////////////////////////////////////////////////////////////////////
//BOT (Bulk Only Transport)
//http://www.usb.org/developers/devclass_docs/usbmassbulk_10.pdf
//This specification does not provide for bi-directional data transfer in a single command.
//This specification addresses Bulk-Only Transport, or in other words,
//transport of command, data, and status occurring solely via Bulk endpoints
//(not via Interrupt or Control endpoints). This specification only uses the
//default pipe to clear a STALL condition on the Bulk endpoints and to issue
//class-specific requests
//Command Block Wrapper (CBW)
//A packet containing a command block and associated information.
//Command Status Wrapper (CSW)
//A packet containing the status of a command block.
//Phase Error
//An error returned by the device indicating that the results of processing further
//CBWs will be indeterminate until the device is reset.

//Bulk-Only Mass Storage Reset (class-specific request)
//This request is used to reset the mass storage device and its associated interface.
//This class-specific request shall ready the device for the next CBW from the host.
//The host shall send this request via the default pipe to the device. The device shall
//preserve the value of its bulk data toggle bits and endpoint STALL conditions
//despite the Bulk-Only Mass Storage Reset. bmRequestType: Class, Interface, host
//to device
// bRequest field set to 255 (FFh)
// wValue field set to 0
// wIndex field set to the interface number
// wLength field set to 0

#define BOT_MASS_STORAGE_RESET  255

//Get Max LUN (class-specific request)
//The device may implement several logical units that share common device characteristics.
//The host uses bCBWLUN field in CBW to designate which logical unit of the device is the
//destination of the CBW. The Get Max LUN device request is used to determine the number
//of logical units supported by the device. Logical Unit Numbers on the device shall be
//numbered contiguously starting from LUN 0 to a maximum LUN of 15 (Fh).
//To issue a Get Max LUN device request, the host shall issue a device request on the
//default pipe of:
// bmRequestType: Class, Interface, device to host
// bRequest field set to 254 (FEh)
// wValue field set to 0
// wIndex field set to the interface number
// wLength field set to 1

#define BOT_GET_MAX_LUN  254

//The device shall return one byte of data that contains the maximum LUN supported
//by the device. For example, if the device supports four LUNs then the LUNs would
//be numbered from 0 to 3 and the return value would be 3. If no LUN is associated
//with the device, the value returned shall be 0. The host shall not send a command
//block wrapper (CBW) to a non-existing LUN. Devices that do not support multiple
//LUNs may STALL this command.

//Host/Device Packet Transfer Order
//The host shall send the CBW before the associated Data-Out, and the device shall
//send Data-In after the associated CBW and before the associated CSW. The host may
//request Data-In or CSW before sending the associated CBW.
//If the dCBWDataTransferLength is zero, the device and the host shall transfer
//no data between the CBW and the associated CSW.

//Ready -> CBW -> Data in or out -> CSW -> go back to ready

//Command Block Wrapper
#define CBW_SIGNATURE   0x43425355

#define CBWFlags_DATA_DIR_OUT  0x00
#define CBWFlags_DATA_DIR_IN   0x80

typedef struct CommandBlockWrapperRec
{
    NvU32   CBWSignature          ;  //Signature that helps identify this data packet
                                     //as a CBW. The field shall contain the value
                                     //43425355h (little endian)
    NvU32   CBWTag                ;  //A Command Block Tag sent by the host. The device
                                     //shall echo the contents of this field back to the
                                     //host in the dCSWTag field of the associated CSW.
                                     //The dCSWTag positively associates a CSW with the
                                     //corresponding CBW
    NvU32   CBWDataTransferLength ;  //The number of bytes of data that the host expects
                                     //to transfer on the Bulk-In or Bulk-Out endpoint (as
                                     //indicated by the Direction bit) during the
                                     //execution of this command. If this field is zero,
                                     //the device and the host shall transfer no data
                                     //between the CBW and the associated CSW, and the
                                     //device shall ignore the value of the Direction
                                     //bit in bmCBWFlags.
    NvU8     CBWFlags             ;  //The bits of this field are defined as follows:
                                     //Bit 7 Direction - the device shall ignore this
                                     //bit if the dCBWDataTransferLength field is
                                     //zero, otherwise:
                                     // 0 = Data-Out from host to the device,
                                     // 1 = Data-In from the device to the host.
                                     // Bit 6 Obsolete. The host shall set this bit to zero.
                                     // Bits 5..0 Reserved - the host shall set these
                                     //bits to zero.
    NvU8    CBWLUN                ;  //least significant 4 bits
                                     //The device Logical Unit Number (LUN) to which
                                     //the command block is being sent. For devices that
                                     //support multiple LUNs, the host shall place into
                                     //this field the LUN to which this command block is
                                     //addressed. Otherwise, the host shall set this
                                     //field to zero.
    NvU8    CBWCBLength           ;  //Least significant 4 bits
                                     //The valid length of the CBWCB in bytes.
                                     //This defines the valid length of the command block.
                                     //The only legal values are 1 through 16 (01h through 10h)
                                     //All other values are reserved.
    NvU8    CBWCB[16]             ;  //The command block to be executed by the device.
                                     //The device shall interpret the first bCBWCBLength
                                     //bytes in this field as a command block as defined by
                                     //the command set identified by bInterfaceSubClass.
                                     //If the command set supported by the device uses
                                     //command blocks of fewer than 16 (10h) bytes in
                                     //length, the significant bytes shall be transferred
                                     //first, beginning with the byte at offset 15 (Fh).
                                     //The device shall ignore the content of the CBWCB
                                     //field past the byte at offset (15 + bCBWCBLength - 1)
} __attribute__((packed)) CommandBlockWrapper;

#define CSW_SIGNATURE   0x53425355

#define CSW_STATUS_CMD_GOOD      0x0         //00h Command Passed ("good status")
#define CSW_STATUS_CMD_FAIL      0x1         //01h Command Failed
#define CSW_STATUS_CMD_PHASE_ERR 0x2         //02h Phase Error

typedef struct CommandBlockStatusRec
{
    NvU32 CSWSignature   ; //Signature that helps identify this data packet
                           //as a CSW. The signature field shall contain the value
                           //53425355h (little endian), indicating CSW.
    NvU32 CSWTag         ; //The device shall set this field to the value received
                           //in the dCBWTag of the associated CBW.
    NvU32 CSWDataResidue ; //For Data-Out the device shall report in the dCSWDataResidue
                           //the difference between the amount of data expected as stated
                           //in the dCBWDataTransferLength, and the actual amount of data
                           //processed by the device. For Data-In the device shall report
                           //in the dCSWDataResidue the difference between the amount of
                           //data expected as stated in the dCBWDataTransferLength and the
                           //actual amount of relevant data sent by the device.
                           //The dCSWDataResidue shall not exceed the value sent in the
                           //dCBWDataTransferLength.
    NvU8 CSWStatus       ; //bCSWStatus indicates the success or failure of the command.
                           //The device shall set this byte to zero if the command completed
                           //successfully. A non-zero value shall indicate a failure during
                           //command execution
}__attribute__((packed)) CommandBlockStatus;


NvError NvBootXusbMscBotProcessRequest(NvddkXusbContext *Context, NvU8 opcode);


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVDDK_XUSB_MSC_H */


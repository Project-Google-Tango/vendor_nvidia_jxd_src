/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVDDK_XUSB_CONTEXT_H
#define INCLUDED_NVDDK_XUSB_CONTEXT_H


#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct EPDw0Rec
{
    NvU32 EPState               :3; /* Endpoint State */
    NvU32 res0                  :5; /* Reserved */
    NvU32 Mult                  :2; /* Maximum number of burst within an interval */
    NvU32 mpStreams             :5; /* Maximum number of Primary Stream IDs */
    NvU32 LSA                   :1; /* Linear Steram Array */
    NvU32 Interval              :8; /* Interval between requests */
    NvU32 res1                  :8; /* Reserved */
}EPDw0;

typedef struct EPDw1Rec
{
    NvU32 ForceEvent            :1;  /* ForceEvent*/
    NvU32 CErr                  :2;  /* USB bus errors count */
    NvU32 EPType                :3;  /* Endpoint Type */
    NvU32 res0                  :1;  /* Reserved */
    NvU32 HID                   :1;  /* Host Initiated Stream  */
    NvU32 MaxBurstSize          :8;  /* maximum number of bursts per scheduling  */
    NvU32 MaxPacketSize         :16; /* maximum packet size */
}EPDw1;

typedef struct EPDw2Rec
{
    NvU32 DCS                   :1;  /* Dequeue Cycle*/
    NvU32 res0                  :3;  /* Reserved */
    NvU32 TRDequeuePtrLo        :28; /* : LSB of transfer ring dequeue pointer  */
}EPDw2;

typedef struct EPDw3Rec
{
    NvU32 TRDequeuePtrHi;            /* : MSB of transfer ring dequeue pointer  */
}EPDw3;

typedef struct EPDw4Rec
{
    NvU32 AverageTRBLength      :16; /* averagelen , bw calculation*/
    NvU32 MaxESITPayload        :16; /* used for ISO scheduling*/
}EPDw4;

typedef struct EPDw5Rec
{
    NvU32 EDTLA                 :24; /* event data transfer length  */
    NvU32 res0                  :2;  /* Reserved */
    NvU32 SplitXState           :1;  /* SplitXState*/
    NvU32 SEQNUM                :5;  /* for SS*/
}EPDw5;

typedef struct EPDw6Rec
{
    NvU32 CProg                 :8;  /* ForceEvent*/
    NvU32 SByte                 :7;  /* accumulate data payload*/
    NvU32 TP                    :2;  /* Endpoint Type */
    NvU32 REC                   :1;  /* Reserved */
    NvU32 CErrCnt               :2;  /* Holds CErr countdown*/
    NvU32 CED                   :1;  /* prediction of the control endpoint  */
    NvU32 HSP                   :1;  /* HS out endpoint PING state */
    NvU32 RTY                   :1;  /* Retry */
    NvU32 STD                   :1;  /* Skip TD */
    NvU32 Status                :8;  /* Current Err Status*/
}EPDw6;

typedef struct EPDw7Rec
{
    NvU32 DataOffset            :17; /* unused portion of the data buffer */
    NvU32 res0                  :4;  /* Reserved */
    NvU32 LPA                   :1;  /* linked list pointer updated*/
    NvU32 NumTRBs               :5;  /* for SS*/
    NvU32 NumP                  :5;  /* for SS*/
}EPDw7;

typedef struct EPDw8Rec
{
    NvU32 ScratchPad            :32; /* Scratchpad */
}EPDw8;

typedef struct EPDw9Rec
{
    NvU32 ScratchPad            :32; /* Scratchpad */
}EPDw9;

typedef struct EPDw10Rec
{
    NvU32 CMask                 :8;  /* CMask/Cping/CurrentSteamId*/
    NvU32 SMask                 :8;  /* SMask/Sping/CurrentSteamId*/
    NvU32 TC                    :2;  /* Traffic class*/
    NvU32 NS                    :1;  /* No Snoop bit*/
    NvU32 RO                    :1;  /* relaxed orfer bit*/
    NvU32 TLM                   :1;  /* Trb location*/
    NvU32 DLM                   :1;  /* Data referenced by TRB's*/
    NvU32 PNG                   :1;  /* Ping data structure*/
    NvU32 FW                    :1;  /* Is a firmware created endpoing data structure*/
    NvU32 SRR                   :8;  /* StopReclaimRequest*/
}EPDw10;

typedef struct EPDw11Rec
{
    NvU32 DevAddr               :8;  /* Device Address */
    NvU32 HubAddr               :8;  /* Hub Addr*/
    NvU32 RootPortNum           :8;  /* RootPort Num*/
    NvU32 SlotId                :8;  /* Slot Id*/
}EPDw11;

typedef struct EPDw12Rec
{
    NvU32 RouteString           :20; /* RouteString*/
    NvU32 Speed                 :4;  /* Speed*/
    NvU32 LPU                   :1;  /* linked list pointer updated*/
    NvU32 MTT                   :1;  /* Multi Transaction Translator */
    NvU32 Hub                   :1;  /* Hub */
    NvU32 DCI                   :5;  /* DeviceContextIndex */
}EPDw12;

typedef struct EPDw13Rec
{
    NvU32 TTHubSlotID           :8;  /*TTHubSlotID */
    NvU32 TTPortNum             :8;  /* TTPortNum*/
    NvU32 TTT                   :8;  /* Transaction-Translator Think Time*/
    NvU32 InterrupterTarget     :8;  /* InterrupterTarget*/
}EPDw13;

typedef struct EPDw14Rec
{
    NvU32 FRZ                   :1;  /* Temporary Freeze*/
    NvU32 END                   :1;  /* End fo the Link list */
    NvU32 ELM                   :1;  /* endpoint referenced location*/
    NvU32 MRK                   :1;  /* transition in a bulk list*/
    NvU32 EndpointLinkLo        :28; /* LSB of a pointer endpoint data structrure */
}EPDw14;

typedef struct EPDw15Rec
{
    NvU32 EndpointLinkHo        :32; /* MSB of a pointer endpoint data structrure */
}EPDw15;

typedef struct EPRec
{
    EPDw0 EpDw0;
    EPDw1 EpDw1;
    EPDw2 EpDw2;
    EPDw3 EpDw3;
    EPDw4 EpDw4;
    EPDw5 EpDw5;
    EPDw6 EpDw6;
    EPDw7 EpDw7;
    /* Below are not part of xHCI spec */
    EPDw8 EpDw8;
    EPDw9 EpDw9;
    EPDw10 EpDw10;
    EPDw11 EpDw11;
    EPDw12 EpDw12;
    EPDw13 EpDw13;
    EPDw14 EpDw14;
    EPDw15 EpDw15;
}EP;

typedef struct TRBRec
{
    NvU32 DataBuffer[4];
}TRB;

// Define Endpoint Type
typedef enum
{
    USB_NOT_VALID= 0,
    USB_ISOCH_OUT,
    USB_BULK_OUT,
    USB_INTERRUPT_OUT,
    USB_CONTROL_BI,
    USB_ISOCH_IN,
    USB_BULK_IN,
    USB_INTERRUPT_IN,
}USB_Endpoint_Type;

// Define Endpoint State
typedef enum
{
    USB_EP_STATE_DISABLED = 0,
    USB_EP_STATE_RUNNING,
    USB_EP_STATE_HALTED,
    USB_EP_STATE_STOPPED,
    USB_EP_STATE_ERROR,
}USB_Endpoint_State;

typedef enum
{
    NvddkXusbStatus_WorkQSubmitted = 1,
    NvddkXusbStatus_CompQPollStart,
    NvddkXusbStatus_CompQPollError,
    NvddkXusbStatus_CompQPollEnd,
    NvddkXusbStatus_EventQPollStart,
    NvddkXusbStatus_EventQPollError,
    NvddkXusbStatus_EventQPollEnd,

}NvddkXusbStatus;
/*
 * NvddkXusbContext - The context structure for the Xusb driver.
 * A pointer to this structure is passed into the driver's Init() routine.
 * This pointer is the only data that can be kept in a global variable for
 * later reference.
 */
 // TODO rearrange once finalized.
typedef struct NvddkXusbContextRec
{
    //For logging useful into in Boot info table's SecondaryDevStatus field
    //NvddkXusbStatus *XusbBitInfo;
    /// Endpoint data structure context
    EP *EndPtContext;
    //EnquePtr
    TRB *TRBRingEnquePtr;
    //Track EnquePtr for Control transfer
    TRB *TRBRingCtrlEnquePtr;
    //DequePtr
    TRB *TRBRingDequePtr;
    // For all commands except read and write, the data buffer used to get data is
    //BufferXusbData space else it is the address of the memory area passed from the
    //top level flow
    NvU8 *BufferData;
    //Required for Read command
    NvU32 LogicalBlkAddr;
    NvU16 TransferLen;
    // No of device blocks per page;
    //Derived from read capacity response
    NvU16 DeviceNumBlocksPerPage;
    //Copied from Endpoint Descriptor
    struct
    {
        NvU16 PacketSize;
        NvU8  EndpointNumber;
    }EPNumAndPacketSize[2]; //OUT is indexed by 0 (direction out) and IN type is
                            //indexed by 1 (direction in).
    struct
    {
        NvU8 bMaxPacketSize0;     //as sent in device descriptor
        NvU8 DevAddr;             //current device address assigned to device by the host
        NvU8 bNumConfigurations ; //as sent in device descriptor
        NvU8 bConfigurationValue;
        NvU8 InterfaceIndx;
        NvU8 LUN;
    }stEnumerationInfo;
    /// Holds the Xusb Read start time.
    NvU64 ReadStartTime;
    /// Current page under access in CSB space
    NvU32 CurrentCSBPage;
    /// Block size in Log2 scale.
    NvU8 BlockSizeLog2;
    /// Page size in Log2 scale.
    NvU8 PageSizeLog2;
    /// Hub Address
    NvU8 HubAddress;
    /// Root Port Number
    NvU8 RootPortNum;
    /// Slod Id Number
    NvU8 SlotId;
    /// RouteString
    NvU8 RouteString;
    /// Maintain cycle bit
    NvBool CycleBit;
    /// SetAddress
    NvBool SetAddress;
    //sequence no., for bulk out
    NvU32 BulkSeqNumOut;
    //sequence no., for bulk In
    NvU32 BulkSeqNumIn;
    NvBootUsb3Status Usb3Status;

    NvRmMemHandle hCtxMemHandle;
    NvU8* pCtxMemAddr;

} NvddkXusbContext;

//**************************Function Prototypes************************

NvError NvddkXusbUpdWorkQAndChkCompQ(NvddkXusbContext *Context);

void NvddkXusbUpdateEpContext(NvddkXusbContext *Context, USB_Endpoint_Type EpType);

NvError NvddkXusbMscBotProcessRequest(NvddkXusbContext *Context, NvU8 opcode);

void XusbClocksSetEnable(NvBootClocksClockId ClockId, NvBool Enable);
void XusbResetSetEnable(const NvBootResetDeviceId DeviceId, const NvBool Enable);


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVDDK_XUSB_CONTEXT_H */


/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"
#include "nvddk_usbf.h"
#include "tio_usb_nvddk_priv.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_pmu.h"
#include "nvodm_query_discovery.h"
#include "nvboot_bit.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define NV_TIO_USB_RC_BUFFER_IDX  0
#define NV_TIO_USB_TX_BUFFER_IDX  1

// Debug of usb code
#define DB_USB 0

#if DB_USB & 1
#define DBU1(x) NvOsDebugPrintf(x)
#else
#define DBU1(x)
#endif
#if DB_USB & 2
#define DBU2(x) NvOsDebugPrintf(x)
#else
#define DBU2(x)
#endif

//#define USB_DBG_PRINT(X) NvOsDebugPrintf(X)
#define USB_DBG_PRINT(X)

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

typedef enum NvTrAp15UsbReadStateEnum
{
    NvTrAp15UsbReadState_idle,
    NvTrAp15UsbReadState_pending,
} NvTrAp15UsbReadState;

typedef struct NvTrAp15UsbStateRec
{
    NvU8  isInit;               // must be first in struct, see initialization!
    NvU8  isCableConnected;     // was the cable connection checked after init?
    NvU8  isOpen;

    NvU8* MemoryBuffAddr;
    NvRmDeviceHandle    RmHandle;
    NvDdkUsbfHandle     hUsbFunc;
    NvOsSemaphoreHandle Semaphore;
    NvDdkUsbfEndpoint  *pEpList;
    NvU32 MaxUsbTxfrBufferSize;

    NvBool EnumerationDone;
    NvBool IsHighSpeedPort;
    NvU8 UsbConfigurationNo;    // Stores the current configuration number
    NvU8 UsbInterfaceNo;        // Stores current interface number

    NvU32 bufferIndex;          // index of USB receive buffer
    NvU32 inReadBuffer;         // data already in receive buffer
    NvU8* inReadBufferStart;    // pointer to start of those data

    NvTrAp15UsbReadState readState;
} NvTioDdkUsbState;

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

/* Read and write must use separate buffers */
NV_ALIGN(4096) NvU8 gs_readBufferMemory[NV_TIO_USB_MAX_SEGMENT_SIZE];
NV_ALIGN(4096) NvU8 gs_writeBufferMemory[NV_TIO_USB_MAX_SEGMENT_SIZE];

/* Pointers to a pair of 4k aligned buffers */
static NvU8* gs_buffer[2] = {
    &gs_readBufferMemory[0],
    &gs_writeBufferMemory[0]
};

static NvTioDdkUsbState gs_UsbState = { 0 };    // gs_UsbState.isInit = 0;

//###########################################################################
//############################### CODE ######################################
//###########################################################################

static NvError NvTioDdkUsbWriteSegment(
                                       const void *ptr,
                                       NvU32 bytes2Send
                                       );

NvError ProcessSetupPacket(NvTioDdkUsbState *pUsbTestCtxt)
{
    NvError ErrStatus = NvSuccess;
    UsbSetupRequestType requestType;
    SetupPacket setUpPacket;
    NvBool transmitData = NV_FALSE;
    NvU8 *pDataBuffer = NULL;
    NvU32 DataSize;
    NvU8 InterfaceStatus[2] = {0, 0}; //GetStatus() Request to an Interface is always 0

    // Read the SetUp Packet data from Endpoint zero OUT Q head
    NvOsMemcpy((void*) &setUpPacket,
        (void*) pUsbTestCtxt->MemoryBuffAddr,
        sizeof(SetupPacket));

    requestType = (UsbSetupRequestType)(setUpPacket.target |
                                       (setUpPacket.codeType << 5) |
                                       (setUpPacket.dirInFlag << 7));
    requestType &= 0xFF;
    switch (requestType)
    {
        case HOST2DEV_DEVICE:
            USB_DBG_PRINT("HOST2DEV_DEVICE\n");
            switch(setUpPacket.request)
            {
                case CLEAR_FEATURE:
                    USB_DBG_PRINT("CLEAR_FEATURE\n");
                    NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc,
                        pUsbTestCtxt->pEpList[EP_CTRL_IN],
                        NV_TRUE);
                    break;
                case SET_FEATURE:
                    USB_DBG_PRINT("SET_FEATURE\n");
                    NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc,
                        pUsbTestCtxt->pEpList[EP_CTRL_IN],
                        NV_TRUE);
                    break;
                case SET_ADDRESS:
                    USB_DBG_PRINT("SET_ADDRESS\n");
                    ErrStatus = NvDdkUsbfAckEndpoint(pUsbTestCtxt->hUsbFunc,
                        pUsbTestCtxt->pEpList[EP_CTRL_IN]);
                    if (ErrStatus != NvSuccess)
                    {
                        NvOsDebugPrintf("FAiled to Ack EP IN\n");
                    }
                    else
                    {
                        ///Set the address of the USB device
                        NvOsDebugPrintf("Device address set to %d\n",
                                        setUpPacket.value);
                        NvDdkUsbfSetAddress( pUsbTestCtxt->hUsbFunc,
                                             setUpPacket.value);
                    }
                    break;
                case SET_CONFIGURATION:
                    USB_DBG_PRINT("SET_CONFIGURATION\n");
                    ErrStatus = NvDdkUsbfAckEndpoint(pUsbTestCtxt->hUsbFunc,
                        pUsbTestCtxt->pEpList[EP_CTRL_IN]);
                    if (ErrStatus != NvSuccess)
                    {
                        NvOsDebugPrintf("FAiled to Ack EP IN\n");
                    }
                    else
                    {
                        pUsbTestCtxt->UsbConfigurationNo = setUpPacket.value;
                        //Configuring control Out and IN endpoints
                        ErrStatus = NvDdkUsbfConfigureEndpoint(
                            pUsbTestCtxt->hUsbFunc,
                            &pUsbTestCtxt->pEpList[EP_BLK_OUT], NV_TRUE);
                        if (ErrStatus != NvSuccess)
                        {
                            NvOsDebugPrintf("FAiled to cONFIGURE BULK OUT\n");
                        }

                        ErrStatus = NvDdkUsbfConfigureEndpoint(
                            pUsbTestCtxt->hUsbFunc,
                            &pUsbTestCtxt->pEpList[EP_BLK_IN], NV_TRUE);
                        if (ErrStatus != NvSuccess)
                        {
                            NvOsDebugPrintf("FAiled to cONFIGURE BULK IN\n");
                        }
                        else
                            pUsbTestCtxt->EnumerationDone = NV_TRUE;
                    }
                    break;
                default:
                    NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc,
                        pUsbTestCtxt->pEpList[EP_CTRL_IN],
                        NV_TRUE);
                    break;
            }
            break;

    case HOST2DEV_INTERFACE:
        USB_DBG_PRINT("HOST2DEV_INTERFACE\n");
        ErrStatus = NvDdkUsbfAckEndpoint(pUsbTestCtxt->hUsbFunc,
            pUsbTestCtxt->pEpList[EP_CTRL_IN]);
        if (ErrStatus == NvSuccess)
        {
            // Set the Interface number sent by the host
            NvOsDebugPrintf("Interface number set to %d\n", setUpPacket.value);
            pUsbTestCtxt->UsbInterfaceNo = setUpPacket.value;
        }
        break;

    case HOST2DEV_ENDPOINT:
        NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc, pUsbTestCtxt->pEpList[EP_CTRL_IN], NV_TRUE);
        break;

    case DEV2HOST_DEVICE:
        USB_DBG_PRINT("DEV2HOST_DEVICE\n");
        switch(setUpPacket.request)
        {
            case GET_STATUS:
                USB_DBG_PRINT("GET_STATUS::DEVICE\n");
                transmitData = NV_TRUE;
                pDataBuffer = &s_UsbDeviceStatus[0];
                DataSize = USB_DEV_STATUS_LENGTH;
                break;
            case GET_CONFIGURATION:
                USB_DBG_PRINT("GET_CONFIGURATION::DEVICE\n");
                transmitData = NV_TRUE;
                pDataBuffer = &pUsbTestCtxt->UsbConfigurationNo;
                DataSize = setUpPacket.length;
                break;
            case GET_DESCRIPTOR:
                // Get Descriptor Request
                switch((setUpPacket.value) >> 8 )
                {
                    case USB_DT_DEVICE:
                        USB_DBG_PRINT("GET_DESCRIPTOR::DEVICE\n");
                        transmitData = NV_TRUE;
                        pDataBuffer = &s_UsbDeviceDescriptor[0];
                        DataSize = sizeof(s_UsbDeviceDescriptor);
                        break;

                    case USB_DT_CONFIG:
                        USB_DBG_PRINT("GET_DESCRIPTOR::CONFIG\n");
                        transmitData = NV_TRUE;
                        if (pUsbTestCtxt->IsHighSpeedPort)
                        {
                            s_UsbConfigDescriptor[22] =
                                (USB_HIGH_SPEED_PKT_SIZE_BYTES & 0xFF);
                            s_UsbConfigDescriptor[23] =
                                ((USB_HIGH_SPEED_PKT_SIZE_BYTES>>8) & 0xFF);
                            s_UsbConfigDescriptor[29] =
                                (USB_HIGH_SPEED_PKT_SIZE_BYTES & 0xFF);
                            s_UsbConfigDescriptor[30] =
                                ((USB_HIGH_SPEED_PKT_SIZE_BYTES>>8) & 0xFF);
                        }
                        else
                        {
                            s_UsbConfigDescriptor[22] =
                                (USB_FULL_SPEED_PKT_SIZE_BYTES & 0xFF);
                            s_UsbConfigDescriptor[23] =
                                ((USB_FULL_SPEED_PKT_SIZE_BYTES >>8) & 0xFF);
                            s_UsbConfigDescriptor[29] =
                                (USB_FULL_SPEED_PKT_SIZE_BYTES & 0xFF);
                            s_UsbConfigDescriptor[30] =
                                ((USB_FULL_SPEED_PKT_SIZE_BYTES >>8) & 0xFF);
                        }
                        pDataBuffer = &s_UsbConfigDescriptor[0];
                        DataSize = sizeof(s_UsbConfigDescriptor);
                        break;

                    case USB_DT_STRING:
                        transmitData = NV_TRUE;
                        USB_DBG_PRINT("GET_DESCRIPTOR::STRING\n");
                        switch (setUpPacket.value & 0xff)
                        {
                            case 1:
                                DataSize = (s_UsbManufacturerID[0] <=
                                                sizeof(s_UsbManufacturerID)) ?
                                                    s_UsbManufacturerID[0] :
                                                    sizeof(s_UsbManufacturerID);
                                pDataBuffer = &s_UsbManufacturerID[0];
                                break;
                            case 2:    // Product ID
                                DataSize = (s_UsbProductID[0] <=
                                    sizeof(s_UsbProductID)) ?
                                    s_UsbProductID[0] :
                                sizeof(s_UsbProductID);
                                pDataBuffer = &s_UsbProductID[0];
                                break;
                            case 3:    // Serial Number
                                pDataBuffer = &s_UsbSerialNumber[0];
                                DataSize = (s_UsbSerialNumber[0] <=
                                    sizeof(s_UsbSerialNumber)) ?
                                    s_UsbSerialNumber[0] :
                                sizeof(s_UsbSerialNumber);
                                break;
                            default:
                                DataSize = sizeof(s_UsbLanguageID);
                                pDataBuffer = &s_UsbLanguageID[0];
                                break;
                        }
                        break;

                    case USB_DT_DEVICE_QUALIFIER:
                        transmitData = NV_TRUE;
                        pDataBuffer = &s_UsbDeviceQualifier[0];
                        DataSize = sizeof(s_UsbDeviceQualifier);
                        break;

                    default:
                        NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc, pUsbTestCtxt->pEpList[EP_CTRL_IN], NV_TRUE);
                        break;
                }
                break;
            default:
                NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc, pUsbTestCtxt->pEpList[EP_CTRL_IN], NV_TRUE);
                break;
        }
        break;

    case DEV2HOST_INTERFACE:
        USB_DBG_PRINT("DEV2HOST_INTERFACE\n");
        switch (setUpPacket.request)
        {
            case GET_STATUS: // Get Status/Interface Request
                transmitData = NV_TRUE;
                pDataBuffer = &InterfaceStatus[0];
                DataSize = setUpPacket.length;
                break;
            case GET_INTERFACE: // Get Interface Request
                transmitData = NV_TRUE;
                pDataBuffer = &pUsbTestCtxt->UsbInterfaceNo;
                DataSize = setUpPacket.length;
                break;
            default:
                // Stall if any Unsupported request comes
                NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc, pUsbTestCtxt->pEpList[EP_CTRL_IN], NV_TRUE);
                break;
        }
        break;

    case DEV2HOST_ENDPOINT:
        USB_DBG_PRINT("DEV2HOST_ENDPOINT\n");
        NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc, pUsbTestCtxt->pEpList[EP_CTRL_IN], NV_TRUE);
        break;

    default:
        NvDdkUsbfEpSetStalledState(pUsbTestCtxt->hUsbFunc, pUsbTestCtxt->pEpList[EP_CTRL_IN], NV_TRUE);
        break;
    }

    if (transmitData)
    {
        NvU32 TransmitSize = 0;

        if (setUpPacket.length >= DataSize)
        {
            TransmitSize = DataSize;
        }
        else
        {
            TransmitSize = setUpPacket.length;
        }
        NvOsMemcpy((void *)pUsbTestCtxt->MemoryBuffAddr,
            (void *) pDataBuffer,
            DataSize);

        ErrStatus = NvDdkUsbfTransmit(
            pUsbTestCtxt->hUsbFunc,
            pUsbTestCtxt->pEpList[EP_CTRL_IN],
            pUsbTestCtxt->MemoryBuffAddr,
            &TransmitSize,
            NV_TRUE,
            NV_WAIT_INFINITE);

        if (ErrStatus == NvSuccess)
        {
            USB_DBG_PRINT("*********Transmit Data Success********\n");
            ErrStatus = NvDdkUsbfAckEndpoint(pUsbTestCtxt->hUsbFunc,
                pUsbTestCtxt->pEpList[EP_CTRL_OUT]);
            if (ErrStatus != NvSuccess)
            {
                NvOsDebugPrintf("FAiled to Ack EP OUT\n");
            }
            else
            {
                USB_DBG_PRINT("Ack EP OUT Success\n");

            }
        }
        else
        {
            NvOsDebugPrintf("+++++++++Failed to Transmit Data++++++++++\n");
        }
    }
    return ErrStatus;
}

/**
* @result NV_TRUE The USB is set up correctly and is ready to
* transmit and receive messages.
* @param pUsbBuffer pointer to a memory buffer into which data to the host
*        will be stored. This buffer must be 4K bytes in IRAM and must be
*        aligned to 4K bytes. Otherwise it will assert.
* @result NV_FALSE The USB failed to set up correctly.
*/
static NvBool NvTioDdkUsbSetup(NvU8 *pUsbBuffer)
{
    NvError err = NvSuccess;
    int timeout;
    const NvDdkUsbfEndpoint * pEndPointList;
    NvU32 EpCount = 0;
    NvU32 Instance = 0;

    //
    // get RM handle 
    //
    err = NvRmOpen(&gs_UsbState.RmHandle, 0);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("NvTioDdkUsbSetup: NvRmOpen Failed\n");
        goto ErrorHandle;
    }
    else
    {
        NvOsDebugPrintf("NvTioDdkUsbSetup: NvRmOpen Success\n");
    }

    if (0)
    {
        volatile int i = 0;
        for (; i == 0; ) ;
    }

    err = NvOsSemaphoreCreate(&gs_UsbState.Semaphore, 0);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("NvTioDdkUsbSetup: NvOsSemaphoreCreate Failed\n");
        goto ErrorHandle;
    }

    err = NvDdkUsbfOpen(
                    gs_UsbState.RmHandle,
                    &gs_UsbState.hUsbFunc,
                    &gs_UsbState.MaxUsbTxfrBufferSize,
                    gs_UsbState.Semaphore,
                    Instance);

    if (err != NvSuccess)
    {
        NvOsDebugPrintf("NvTioDdkUsbSetup: NvDdkUsbfOpen Failed\n");
        return NV_FALSE;
    }
    else
    {
        NvOsDebugPrintf("NvTioDdkUsbSetup: NvDdkUsbfOpen Success\n");
    }

    gs_UsbState.MemoryBuffAddr = gs_buffer[NV_TIO_USB_RC_BUFFER_IDX];

    pEndPointList = NvDdkUsbfGetEndpointList(
        gs_UsbState.hUsbFunc,
        &EpCount);

    if (!EpCount)
        goto ErrorHandle;

    gs_UsbState.pEpList =
        NvOsAlloc(EpCount*sizeof(NvDdkUsbfEndpoint));

    if (!gs_UsbState.pEpList)
    {
        NvOsDebugPrintf("Memory Allocation Failed\n");
        goto ErrorHandle;
    }
    else
    {
        NvOsMemcpy(gs_UsbState.pEpList,
            pEndPointList,
            EpCount*sizeof(NvDdkUsbfEndpoint));
    }

    // Setup the Bulk end points in the list
    gs_UsbState.pEpList[EP_BLK_OUT].isBulk = NV_TRUE;
    gs_UsbState.pEpList[EP_BLK_OUT].isIn = NV_FALSE;
    gs_UsbState.pEpList[EP_BLK_OUT].isIsoc = NV_FALSE;
    gs_UsbState.pEpList[EP_BLK_OUT].isInterrupt = NV_FALSE;

    gs_UsbState.pEpList[EP_BLK_IN].isBulk = NV_TRUE;
    gs_UsbState.pEpList[EP_BLK_IN].isIn = NV_TRUE;
    gs_UsbState.pEpList[EP_BLK_IN].isIsoc = NV_FALSE;
    gs_UsbState.pEpList[EP_BLK_IN].isInterrupt = NV_FALSE;

#if 0
    //Configuring control Out and IN endpoints
    err = NvDdkUsbfConfigureEndpoint(
        gs_UsbState.hUsbFunc,
        gs_UsbState.pEpList[EP_CTRL_OUT], NV_TRUE);

    err = NvDdkUsbfConfigureEndpoint(
        gs_UsbState.hUsbFunc,
        gs_UsbState.pEpList[EP_CTRL_IN], NV_TRUE);
#endif

   gs_UsbState.EnumerationDone = NV_FALSE;
    gs_UsbState.IsHighSpeedPort = NV_TRUE;

    for (timeout=1000; timeout; timeout--)        // 20 is too short time
    {
        NvU64 portEventsEpComplete;
        NvU32 portEvents = 0;

        // Look for cable connection
        portEventsEpComplete = NvDdkUsbfGetEvents(gs_UsbState.hUsbFunc);
        portEvents = (portEventsEpComplete & 
                      ((1ULL<<NvDdkUsbfEvent_EpTxfrCompleteBit)-1));
        
        if (portEvents & NvDdkUsbfEvent_CableConnect)
        {
            gs_UsbState.EnumerationDone = NV_FALSE;

            // Start enumeration
            err = NvDdkUsbfStart(gs_UsbState.hUsbFunc);
            NvOsWaitUS(100000);
            if (err == NvSuccess)
            {
                while(!gs_UsbState.EnumerationDone)
                {
                    DBU1("Enumerate loop\n");

                    err = NvOsSemaphoreWaitTimeout(gs_UsbState.Semaphore, 1000);

                    portEventsEpComplete =
                        NvDdkUsbfGetEvents(gs_UsbState.hUsbFunc);
                    portEvents = (portEventsEpComplete &
                                  ((1ULL<<NvDdkUsbfEvent_EpTxfrCompleteBit)-1));

                    if (!(portEvents & NvDdkUsbfEvent_CableConnect))
                    {
                        DBU1("USB CABLE REMOVED\n");
                        break;
                    }

                    if (portEvents & NvDdkUsbfEvent_PortHighSpeed)
                    {
                        gs_UsbState.IsHighSpeedPort = NV_TRUE;
                    }
                    else
                    {
                        gs_UsbState.IsHighSpeedPort = NV_FALSE;
                    }

                    if (portEvents & NvDdkUsbfEvent_BusSuspend)
                    {
                        DBU1("USB BUS SUSPEND\n");
                    }

                    if (portEvents & NvDdkUsbfEvent_BusReset)
                    {
                        DBU1("USB RESET\n");
                        err = NvDdkUsbfStart(gs_UsbState.hUsbFunc);
                        if (err != NvSuccess)
                        {
                            gs_UsbState.EnumerationDone = NV_FALSE;
                            break;
                        }
                        //NvOsWaitUS(100000);
                        continue;
                    }

                    if (portEvents & NvDdkUsbfEvent_SetupPacket)
                    {
                        NvU32 rcvdBytes = sizeof(SetupPacket);
                        DBU1("USB SETUP PACKET\n");
                        err = NvDdkUsbfReceive(
                                    gs_UsbState.hUsbFunc,
                                    gs_UsbState.pEpList[EP_CTRL_OUT],
                                    gs_UsbState.MemoryBuffAddr,
                                    &rcvdBytes,
                                    NV_FALSE,
                                    NV_WAIT_INFINITE);
                        if (err == NvSuccess)
                        {
                            ProcessSetupPacket(&gs_UsbState);
                        }
                        else
                        {
                            DBU1("+++++++++Failed to Recieve Data+++++++++\n");
                        }
                    }
                    if (gs_UsbState.EnumerationDone)
                    {
                        // Successfully enumerated
                        DBU1("Enumeration completed successfully\n");
                        return NV_TRUE;
                    }
                }
            }
            else
            {
                NvOsDebugPrintf("Usbf start failed\n");
            }
        }
        NvOsWaitUS(1000);
    }

    // Cable not connected
    if (0)
    {
        volatile int i = 0;
        for (; i == 0; ) ;
    }

ErrorHandle:
    NvRmClose(gs_UsbState.RmHandle);
    return NV_FALSE;
}

#if 0
// This is for testing bug 423638
// see also tests/nvtest/usb200 which calls this function.
NvError NvTioTriggerUsb200Bug(void);
NvError NvTioTriggerUsb200Bug(void)
{
    #define BUGGY_USB_SIZE 0x200
    NvError result;
    static NvU8 buf[BUGGY_USB_SIZE];
    NvU32 cnt = sizeof(buf);
    int i;

    NvOsMemset(buf, 'a', sizeof(buf));


    for (i=0; i<2; i++) {
        result = NvDdkUsbfTransmit(gs_UsbState.hUsbFunc,
                                   gs_UsbState.pEpList[EP_BLK_IN],
                                   buf,
                                   &cnt,
                                   (i == 1) ? NV_TRUE : NV_FALSE,
                                   NV_WAIT_INFINITE);

        while (result == NvError_UsbfTxfrActive)
        {
            result = NvDdkUsbfEndpointStatus(gs_UsbState.hUsbFunc,
                                             gs_UsbState.pEpList[EP_BLK_IN]);
        }
    }

    return result;
}
#endif

static NvError NvTioDdkUsbWriteSegment(
                                       const void *ptr,
                                       NvU32 bytes2Send
                                      )
{
    NvError result;
    NvU32 bytesSent = bytes2Send;

    if (bytes2Send>NV_TIO_USB_MAX_SEGMENT_SIZE)
        return DBERR(NvError_InsufficientMemory);

    NvOsMemcpy(gs_buffer[NV_TIO_USB_TX_BUFFER_IDX], ptr, bytes2Send);

    // timeout zero and wait while UsbfTxfrActive emulate infinite timeout
    result = NvDdkUsbfTransmit(gs_UsbState.hUsbFunc,
                               gs_UsbState.pEpList[EP_BLK_IN],
                               gs_buffer[NV_TIO_USB_TX_BUFFER_IDX],
                               &bytesSent,
                               NV_TRUE,
                               0);

    while (result == NvError_UsbfTxfrActive)
    {
        result = NvDdkUsbfEndpointStatus(gs_UsbState.hUsbFunc,
                                         gs_UsbState.pEpList[EP_BLK_IN]);
    }

    if (result != NvError_UsbfTxfrComplete)
        return DBERR(result);

    if (bytesSent != bytes2Send)
        return DBERR(NvError_FileWriteFailed);

    return NvSuccess;
}

static NvError NvTioDdkUsbReadSegment(
                                      NvU32* bytes2Receive,
                                      NvU32  timeout_msec)
{
    NvError result;

    if (gs_UsbState.readState == NvTrAp15UsbReadState_idle)
    {
        DBU2(("NvTioDdkUsbReadSegment: new read"));
        gs_UsbState.readState = NvTrAp15UsbReadState_pending;
        result = NvDdkUsbfReceive(gs_UsbState.hUsbFunc,
                                  gs_UsbState.pEpList[EP_BLK_OUT],
                                  gs_buffer[NV_TIO_USB_RC_BUFFER_IDX],
                                  bytes2Receive,
                                  NV_TRUE,
                                  0);

        if (result != NvError_UsbfTxfrActive)
        {
            gs_UsbState.readState = NvTrAp15UsbReadState_idle;
            return (result == NvSuccess) ? result : DBERR(result);
        }
    }
    else
    {
        DBU2(("NvTioDdkUsbReadSegment: read pending"));
    }

//     result = NvOsSemaphoreWaitTimeout(gs_UsbState.Semaphore, timeout_msec);
//     if (result)
//         return DBERR(result);     // NvError_Timeout
    result = NvDdkUsbfTransferWait(gs_UsbState.hUsbFunc,
                                   gs_UsbState.pEpList[EP_BLK_OUT],
                                   timeout_msec);
    if (result)
        return DBERR(NvError_Timeout);

    result = NvDdkUsbfReceive(gs_UsbState.hUsbFunc,
                              gs_UsbState.pEpList[EP_BLK_OUT],
                              gs_buffer[NV_TIO_USB_RC_BUFFER_IDX],
                              bytes2Receive,
                              NV_TRUE,
                              0);

    gs_UsbState.readState = NvTrAp15UsbReadState_idle;

    DBU2(("NvTioDdkUsbReadSegment: off loop complete"));

    return NvSuccess;
}

//===========================================================================
// NvTioDdkUsbCheckPath() - check filename to see if it is a usb port
//===========================================================================
static NvError NvTioDdkUsbCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a usb port
    //
    if (!NvTioOsStrncmp(path, "usb:", 4))
        return NvSuccess;
    if (!NvTioOsStrncmp(path, "debug:", 6))
        return NvSuccess;
    return DBERR(NvError_BadValue);
}

//===========================================================================
// NvTioDdkUsbClose()
//===========================================================================
static void NvTioDdkUsbClose(NvTioStream *stream)
{
    NvU32 i;
    NvU32 settle_time_us;
    NvU64 guid = NV_VDD_USB_ODM_ID;
    NvOdmPeripheralConnectivity const *pConnectivity;

    // TODO: cancel all pending transfers
    NvDdkUsbfClose(gs_UsbState.hUsbFunc);
    gs_UsbState.isOpen = 0;
    // Ironically, we need to turn on USB power after closing USB.
    // this is because usbf will turn off USB power, so if we reset
    // the board, USB won't be available as bootrom assumes USB is on
    // by default.

    /* get the connectivity info */
    pConnectivity = NvOdmPeripheralGetGuid( guid );
    if( !pConnectivity )
    {
        return;
    }

    /* enable the power rail */

    for( i = 0; i < pConnectivity->NumAddress; i++ )
    {
        if( pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            NvRmPmuVddRailCapabilities cap;

            /* address is the vdd rail id */ 
            NvRmPmuGetCapabilities( gs_UsbState.RmHandle,
                pConnectivity->AddressList[i].Address, &cap );

            /* set the rail volatage to the recommended */
            NvRmPmuSetVoltage( gs_UsbState.RmHandle,
                pConnectivity->AddressList[i].Address, cap.requestMilliVolts,
                &settle_time_us );

            /* wait for the rail to settle */
            NvOdmOsWaitUS( settle_time_us ); 
        }
    }
}

//===========================================================================
// NvTioDdkUsbFopen()
//===========================================================================
static NvError NvTioDdkUsbFopen(
                    const char *path,
                    NvU32 flags,
                    NvTioStream *stream)
{
    if (gs_UsbState.isOpen)
        return NvSuccess;

    gs_UsbState.bufferIndex = NV_TIO_USB_RC_BUFFER_IDX;

    // Check the cable connection for very first time
    if (!gs_UsbState.isCableConnected)
        gs_UsbState.isCableConnected =
            NvTioDdkUsbSetup(gs_buffer[gs_UsbState.bufferIndex]);

    if (!gs_UsbState.isCableConnected)
        return DBERR(NvError_UsbfCableDisConnected);

    gs_UsbState.isOpen = 1;
    gs_UsbState.inReadBuffer = 0;

    gs_UsbState.readState = NvTrAp15UsbReadState_idle;

    DBU2(("NvTioDdkUsbFopen"));

    return NvSuccess;
}

//===========================================================================
// NvTioDdkUsbFwrite()
//===========================================================================
static NvError NvTioDdkUsbFwrite(
                    NvTioStream *stream,
                    const void *buf,
                    size_t size)
{
    NvError result;
    NvU8* src = (NvU8*)buf;

    while (size)
    {
        NvU32 payload = NV_MIN(size, NV_TIO_USB_MAX_SEGMENT_SIZE-1);

        // TODO: FIXME  workaround for bug 423638
        if (payload == 0x200)
            payload = 0x104;    // anything less than 0x200 should work

        DBU2(("NvTioDdkUsbFwrite: call NvTioDdkUsbWriteSegment\n"));
        result = NvTioDdkUsbWriteSegment(src, payload);
        if (result)
            return DBERR(result);

        src += payload;
        size -= payload;
    }

    DBU2(("NvTioDdkUsbFwrite: return success\n"));
    return NvSuccess;
}

//===========================================================================
// NvTioDdkUsbFread()
//===========================================================================
static NvError NvTioDdkUsbFread(
                    NvTioStream *stream,
                    void        *buf,
                    size_t       buf_count,
                    size_t      *recv_count,
                    NvU32        timeout_msec)
{
    NvError err;
    NvU8* dst = (NvU8*)buf;

    *recv_count = 0;

    while (buf_count)
    {
        NvU32 bytes2Receive;

        if (gs_UsbState.inReadBuffer)
        {
            NvU32 bytesToCopy = NV_MIN(buf_count, gs_UsbState.inReadBuffer);
            DBU2(("NvTioDdkUsbFread: copy from segment"));
            NvOsMemcpy(dst, gs_UsbState.inReadBufferStart, bytesToCopy);
            *recv_count += bytesToCopy;
            dst += bytesToCopy;
            buf_count -= bytesToCopy;
            gs_UsbState.inReadBufferStart += bytesToCopy;
            gs_UsbState.inReadBuffer -= bytesToCopy;
        }

        if (!buf_count)
        {
            DBU2(("NvTioDdkUsbFread: copied, returning"));
            break;
        }

        //  Setting timeout to 0 if something was already received
        if (*recv_count)
            timeout_msec = 0;

        bytes2Receive = NV_TIO_USB_MAX_SEGMENT_SIZE;

        DBU2(("NvTioDdkUsbFread: segment read start"));
        err = NvTioDdkUsbReadSegment(&bytes2Receive,
                                     timeout_msec);
        switch (err)
        {
            case NvError_Timeout: DBU2(("NvError_Timeout")); break;
            case NvError_UsbfTxfrFail: DBU2(("NvError_UsbfTxfrFail")); break;
            case NvSuccess: DBU2(("NvSuccess")); break;
            default: DBU2(("UNKNOWN!")); break;
        }
        if (err)
        {
            if (err == NvError_Timeout && *recv_count)
                return NvSuccess;
            else
                return DBERR(err);
        }

        DBU2(("NvTioDdkUsbFread: setup new segment"));
        gs_UsbState.inReadBufferStart = gs_buffer[NV_TIO_USB_RC_BUFFER_IDX];
        gs_UsbState.inReadBuffer = bytes2Receive;
    }

    return NvSuccess;
}

//===========================================================================
// NvTioRegisterUsb()
//===========================================================================
void NvTioRegisterUsb(void)
{
    static NvTioStreamOps ops = {0};

    if (!gs_UsbState.isInit)
    {
        NvOsMemset(&gs_UsbState, 0, sizeof(gs_UsbState));
        gs_UsbState.isInit = 1;
        gs_UsbState.isCableConnected = 0;
        gs_UsbState.isOpen = 0;
    }

    if (!ops.sopCheckPath) {
        ops.sopName        = "Ap15_usb";
        ops.sopCheckPath   = NvTioDdkUsbCheckPath;
        ops.sopFopen       = NvTioDdkUsbFopen;
        ops.sopFread       = NvTioDdkUsbFread;
        ops.sopFwrite      = NvTioDdkUsbFwrite;
        ops.sopClose       = NvTioDdkUsbClose;

        NvTioRegisterOps(&ops);
    }
}


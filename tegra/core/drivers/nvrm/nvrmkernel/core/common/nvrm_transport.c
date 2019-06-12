/*
 * Copyright (c) 2007-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *          Transport API</b>
 *
 * @b Description: This is the implementation of Transport API, which
 * implements a simple means to pass messages across a port name regardless of
 * port exist in what processor (on same processor or other processor).
 */

#ifndef NVRM_TRANSPORT_IN_KERNEL
#define NVRM_TRANSPORT_IN_KERNEL 1
#endif

#include "nvrm_transport.h"
#include "nvrm_xpc.h"
#include "nvrm_interrupt.h"
#include "nvrm_avp_private.h"
#include "common/nvrm_rpc.h"
#include "nvutil.h"
#include "nvassert.h"
#include "nvcommon.h"
#include "avp.h"

#define LOOPBACK_PROFILE 0

// indices where to save data for the loopback test
#define LOOP_CPU_SEND_INDEX 0
#define LOOP_AVP_ISR_INDEX  1
#define LOOP_AVP_RECV_INDEX 2
#define LOOP_AVP_SEND_INDEX 3
#define LOOP_CPU_ISR_INDEX  4
#define LOOP_CPU_RECV_INDEX 5

#define SEMAPHORE_BASED_MUTUAL_EXCLUSION 0

// The amount of time to poll before sleeping during the msg send operation.
#define POLL_TIME_IN_MS 10

enum {MAX_INT_FOR_TRANSPORT = 2};

// Interrupt bit index in the interrupt controller relocation table.
enum {CPU_TRANSPORT_INT_OBE = 1};
enum {CPU_TRANSPORT_INT_IBF = 0};
enum {AVP_TRANSPORT_INT_OBE = 0};
enum {AVP_TRANSPORT_INT_IBF = 1};

// Some constraints parameter to develop the transport APIs.

// Maximum port name length
enum {MAX_PORT_NAME_LENGTH = 16};

// Maximum possible message length between the ports
#define  MAX_COMMAND_SIZE   16

// Message header size MessageCommand + port Name + message Length (24 Bytes)
enum {MESSAGE_HEADER_SIZE = 0x20};

// Maximum receive message queue depth
enum {MAX_MESSAGE_DEPTH = 30};

// Maximum time to wait for the response when open the port.
enum {MAX_OPEN_TIMEOUT_MS = 200};

// Try to resend the message after this time.
enum {MESSAGE_RETRY_AFTER_MS = 500 };

// Connection message transfer and response wait timeout.
enum {MAX_CONNECTION_TIMEOUT_MS = 500 };



// Transport Commands which uses to do the handshaking and message transfer
// between the processor. This commands are send to the remote processor
// when any type if transaction happens.
typedef enum
{
  TransportCmd_None = 0x0,

  // The first transport command from the cpu->avp will inform the
  // avp of size of the buffer.
  TransportCmd_SetBufferInfo,

  // Transport command for staring the connection process.
  TransportCmd_Connect,

  // Transport command for disconnecting the port and deleting the port entry.
  TransportCmd_Disconnect,

  // Transport command which used for normal message transfer to the port.
  TransportCmd_Message,

  // When a command requires a response, the value in the command field will
  // be changed by the called processor here to indicate that the response is ready.
  TransportCmd_Response,

  TransportCmd_Force32 = 0x7FFFFFFF

} TransportCmd;



// Ports (endpoint) state.
typedef enum
{
    // Port is opened only.
    PortState_Open = 0x1,

    // Port is waiting for connection.
    PortState_Waiting,

    // Port is connected.
    PortState_Connected,

    // Port has been disconnected from other side.  You can pop out messages
    // but you can't send anymore
    PortState_Disconnected,

    // Set to destroy when there is someone waiting for a connection, but
    // and a different thread calls to kill close the port.
    PortState_Destroy,

    PortState_Force32 = 0x7FFFFFFF
} PortState;



// Message list which will be queued in the port receive message queue.
typedef struct RmReceiveMessageRec
{
    // Length of message.
    NvU32 MessageLength;

    // Fixed size message buffer where the receiving message will be store.
    NvU8 MessageBuffer[MAX_MESSAGE_LENGTH];
} RmReceiveMessage;


// Combines the information for keeping the received messages to the
// corresponding ports.
typedef struct MessageQueueRec
{
    // Receive message Q details to receive the message.  We make the queue 1 extra bigger than the
    // requested size, and then we can do lockless updates because only the Recv function modifies
    // ReadIndex, and only the ISR modifies the WriteIndex
    RmReceiveMessage *pReceiveMsg;

    volatile NvU16 ReadIndex;
    volatile NvU16 WriteIndex;

    NvU16 QueueSize;

} MessageQueue;



// Combines all required information for the transport port.
// The port information  contains the state, recv message q, message depth and
// message length.
typedef struct NvRmTransportRec
{
    // Name of the port, 1 exra byte for NULL termination
    char PortName[MAX_PORT_NAME_LENGTH+1];

    // The state of port whether this is open or connected or waiting for
    // connection.
    PortState State;

    // Receive message Box which contains the receive messages for this port.
    MessageQueue RecvMessageQueue;

    // Semaphore which is signal after getting the message for that port.
    // This is the client passed semaphore.
    NvOsSemaphoreHandle hOnPushMsgSem;

    // Pointer to the partner port.  If the connect is to a remote partner,
    // then this pointer is NULL
    NvRmTransportHandle hConnectedPort;

    // If this is a remote connection, this holds the remote ports "name"
    NvU32               RemotePort;

    // save a copy of the rm handle.
    NvRmDeviceHandle hRmDevice;

    struct NvRmTransportRec *pNext;

    // unlikely to be used members at the end

    // to be signalled when someone waits for a connector.
    NvOsSemaphoreHandle hOnConnectSem;

#if LOOPBACK_PROFILE
    NvBool              bLoopTest;
#endif

} NvRmTransport;



// Combines the common information for keeping the transport information and
// sending and receiving the messages.
typedef struct NvRmPrivPortsRec
{
    // Device handle.
    NvRmDeviceHandle hDevice;

    // List of port names of the open ports in the system.
    NvRmTransport   *pPortHead;

    // Mutex for transport
    NvOsMutexHandle mutex;

    NvRmMemHandle hMessageMem;
    void          *pTransmitMem;
    void          *pReceiveMem;
    NvU32         MessageMemPhysAddr;

    NvRmPrivXpcMessageHandle hXpc;

    // if a message comes in, but the receiver's queue is full,
    // then we don't clear the inbound message to allow another message
    // and set this flag.  We use 2 variables here, so we don't need a lock.
    volatile NvU8  ReceiveBackPressureOn;
    NvU8           ReceiveBackPressureOff;

#if LOOPBACK_PROFILE
    volatile NvU32 *pTimer;
#endif
} NvRmPrivPorts;


// !!! Fixme, this should be part of the rm handle.
static NvRmPrivPorts s_TransportInfo;

extern NvU32 NvRmAvpPrivGetUncachedAddress(NvU32 addr);

#define MESSAGE_QUEUE_SIZE_IN_BYTES ( sizeof(RmReceiveMessage) * (MAX_MESSAGE_DEPTH+1) )
static NvU32 s_RpcAvpQueue[ (MESSAGE_QUEUE_SIZE_IN_BYTES + 3) / 4 ];
static NvU32 s_RpcCpuQueue[ (MESSAGE_QUEUE_SIZE_IN_BYTES + 3) / 4 ];
static struct NvRmTransportRec s_RpcAvpPortStruct;
static struct NvRmTransportRec s_RpcCpuPortStruct;

static NvOsInterruptHandle s_TransportInterruptHandle = NULL;

static NvRmTransportHandle
FindPort(NvRmDeviceHandle hDevice, char *pPortName);

static NvError
NvRmPrivTransportSendMessage(NvRmDeviceHandle hDevice, NvU32 *message, NvU32 MessageLength);

static void HandleAVPResetMessage(NvRmDeviceHandle hDevice);

// expect caller to handle mutex
static char *NvRmPrivTransportUniqueName(void)
{
    static char UniqueName[] = "aaaaaaaa+";
    NvU32 len = 8;
    NvU32 i;

    // this will roll a new name until we hit zzzz:zzzz
    // it's not unbounded, but it is a lot of names...
    // Unique names end in a '+' which won't be allowed in supplied names, to avoid
    // collision.
    for (i=0; i < len; ++i)
    {
        ++UniqueName[i];
        if (UniqueName[i] != 'z')
        {
            break;
        }
        UniqueName[i] = 'a';

    }

    return UniqueName;
}


/* Returns NV_TRUE if the message was inserted ok
 *  Returns NV_FALSE if message was not inserted because the queue is already full

 */static NvBool
InsertMessage(NvRmTransportHandle hPort, const NvU8 *message, const NvU32 MessageSize)
{
    NvU32 index;
    NvU32 NextIndex;

    index = (NvU32)hPort->RecvMessageQueue.WriteIndex;
    NextIndex = index + 1;
    if (NextIndex == hPort->RecvMessageQueue.QueueSize)
        NextIndex = 0;

    // check for full condition
    if (NextIndex == hPort->RecvMessageQueue.ReadIndex)
        return NV_FALSE;

    // copy in the message
    NvOsMemcpy(hPort->RecvMessageQueue.pReceiveMsg[index].MessageBuffer,
               message,
               MessageSize);
    hPort->RecvMessageQueue.pReceiveMsg[index].MessageLength = MessageSize;

    hPort->RecvMessageQueue.WriteIndex = (NvU16)NextIndex;
    return NV_TRUE;
}


static void
ExtractMessage(NvRmTransportHandle hPort, NvU8 *message, NvU32 *pMessageSize, NvU32 MaxSize)
{
    NvU32 NextIndex;
    NvU32 index = (NvU32)hPort->RecvMessageQueue.ReadIndex;
    NvU32 size  = hPort->RecvMessageQueue.pReceiveMsg[index].MessageLength;

    NextIndex = index + 1;
    if (NextIndex == hPort->RecvMessageQueue.QueueSize)
        NextIndex = 0;

    NV_ASSERT(index != hPort->RecvMessageQueue.WriteIndex); // assert on empty condition
    NV_ASSERT(size <= MaxSize);

    *pMessageSize = size;

    // only do the copy and update if there is sufficient room, otherwise
    // the caller will propogate an error up.
    if (size > MaxSize)
    {
        return;
    }
    NvOsMemcpy(message,
               hPort->RecvMessageQueue.pReceiveMsg[index].MessageBuffer,
               size);

    hPort->RecvMessageQueue.ReadIndex = (NvU16)NextIndex;
}



static void *s_TmpIsrMsgBuffer;

/**
 * Connect message
 *  [ Transport Command ]
 *  [ Remote Handle ]
 *  [ Port Name ]
 *
 * Response:
 *   [ Remote Handle ] <- [ Local Handle ]
 */

static void
HandleConnectMessage(NvRmDeviceHandle hDevice, volatile NvU32 *pMessage)
{
    char PortName[MAX_PORT_NAME_LENGTH+1];
    NvU32 RemotePort;
    NvRmTransportHandle hPort;

    RemotePort = pMessage[1];
    NvOsMemcpy(PortName, (void*)&pMessage[2], MAX_PORT_NAME_LENGTH);
    PortName[MAX_PORT_NAME_LENGTH] = 0;

    // See if there is a local port with that name
    hPort = FindPort(hDevice, PortName);
    if (hPort && hPort->State == PortState_Waiting)
    {
        NvOsAtomicCompareExchange32((NvS32 *)&hPort->State, PortState_Waiting, PortState_Connected);
        if (hPort->State == PortState_Connected)
        {
            hPort->RemotePort = RemotePort;
            NvOsSemaphoreSignal(hPort->hOnConnectSem);
            pMessage[1] = (NvU32)hPort;
        }
        else
        {
            pMessage[1] = 0;
        }
    }
    else
    {
        pMessage[1] = 0;
    }
    pMessage[0] = TransportCmd_Response;
}



/**
 * Disconnect message
 *  [ Transport Command ]
 *  [ Local Handle ]
 *
 * Response:
 *   [ Local Handle ] <- 0
 */
static void
HandleDisconnectMessage(NvRmDeviceHandle hDevice, volatile NvU32 *pMessage)
{
    NvRmTransportHandle hPort;
    hPort = (NvRmTransportHandle)pMessage[1];

    // !!! For sanity we should walk the list of open ports to make sure this is a valid port!
    if (hPort && hPort->State == PortState_Connected)
    {
        hPort->State = PortState_Disconnected;
        hPort->RemotePort = 0;
    }
    pMessage[1] = 0;
    pMessage[0] = TransportCmd_None;
}


/**
 * Disconnect message
 *  [ Transport Command ]
 *  [ Local Handle ]
 *  [ Message Length ]
 *  [ Message ]
 *
 * Response:
 *   [ Message Length ] <- NvSuccess
 *   [ Transport Command ] <- When we can accept a new message
 */

static void
HandlePortMessage(NvRmDeviceHandle hDevice, volatile NvU32 *pMessage)
{
    NvRmTransportHandle hPort;
    NvU32               MessageLength;
    NvBool              bSuccess;

    hPort         = (NvRmTransportHandle)pMessage[1];
    MessageLength = pMessage[2];

#if LOOPBACK_PROFILE
    if (hPort && hPort->bLoopTest)
    {
# if NV_IS_AVP
        pMessage[LOOP_AVP_ISR_INDEX + 3] = *s_TransportInfo.pTimer;
# else
        pMessage[LOOP_CPU_ISR_INDEX + 3] = *s_TransportInfo.pTimer;
# endif
    }
#endif


    // !!! For sanity we should walk the list of open ports to make sure this is a valid port!
    // Queue the message even if in the open state as presumably this should only have happened if
    // due to a race condition with the transport connected messages.
    if (hPort && (hPort->State == PortState_Connected || hPort->State == PortState_Open))
    {
        bSuccess = InsertMessage(hPort, (NvU8*)&pMessage[3], MessageLength);
        if (bSuccess)
        {
            if (hPort->hOnPushMsgSem)
                NvOsSemaphoreSignal(hPort->hOnPushMsgSem);
            pMessage[0] = TransportCmd_None;
        }
        else
        {
            ++s_TransportInfo.ReceiveBackPressureOn;
        }
    }
}

static void
HandleAVPResetMessage(NvRmDeviceHandle hDevice)
{
    NvRmTransportHandle hPort;

    hPort = FindPort(hDevice,(char*)"RPC_CPU_PORT");
    if (hPort && (hPort->State == PortState_Connected || hPort->State == PortState_Open))
    {
        NvU32 message;
        message = NvRmMsg_AVP_Reset;
        InsertMessage(hPort, (NvU8*)&message, sizeof(NvU32));
        if (hPort->hOnPushMsgSem)
            NvOsSemaphoreSignal(hPort->hOnPushMsgSem);
        else
            NV_ASSERT(0);
    }
    else
        NV_ASSERT(0);

}


/**
 * Handle the Inbox full interrupt.
 */
static void
InboxFullIsr(void *args)
{
    NvRmDeviceHandle hDevice = (NvRmDeviceHandle)args;
    NvU32 MessageData;
    NvU32 MessageCommand;
    volatile NvU32 *pMessage;

    MessageData = NvRmPrivXpcGetMessage(s_TransportInfo.hXpc);
    if(MessageData == AVP_WDT_RESET)
    {
         HandleAVPResetMessage(hDevice);
         NvRmInterruptDone(s_TransportInterruptHandle);
         return;
    }
    // if we're on the AVP, the first message we get will configure the message info
    if (s_TransportInfo.MessageMemPhysAddr == 0)
    {
#if NV_IS_AVP
        MessageData = NvRmAvpPrivGetUncachedAddress(MessageData);
#else
        MessageData = MessageData;
#endif
        s_TransportInfo.MessageMemPhysAddr = MessageData;
        s_TransportInfo.pReceiveMem  = (void*)MessageData;
        s_TransportInfo.pTransmitMem = (void *) (MessageData + MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE);
        // ack the message and return.
        *(NvU32*)s_TransportInfo.pReceiveMem = TransportCmd_None;
        return;
    }

    // otherwise decode and dispatch the message.


    if (s_TransportInfo.pReceiveMem == NULL)
    {
        /* QT/EMUTRANS takes this path. */
        NvRmMemRead(s_TransportInfo.hMessageMem, MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE, s_TmpIsrMsgBuffer, MAX_MESSAGE_LENGTH);
        pMessage = s_TmpIsrMsgBuffer;
        NvRmMemWrite(s_TransportInfo.hMessageMem, MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE, s_TmpIsrMsgBuffer, 2*sizeof(NvU32));
    }
    else
    {
        pMessage = (NvU32*)s_TransportInfo.pReceiveMem;
    }

    MessageCommand = pMessage[0];

    switch (MessageCommand)
    {
    case  TransportCmd_Connect:
        HandleConnectMessage(hDevice, pMessage);
        break;

    case  TransportCmd_Disconnect:
        HandleDisconnectMessage(hDevice, pMessage);
        break;

    case TransportCmd_Message:
        HandlePortMessage(hDevice, pMessage);
        break;

    default:
        NV_ASSERT(0);
    }

    NvRmInterruptDone(s_TransportInterruptHandle);
}


/**
 * Handle the outbox empty interrupt.
 */
static void
OutboxEmptyIsr(void *args)
{
    // !!! This is not currently used... ignore for now.  Might be required if we find that we
    //     need to spin for long periods of time waiting for other end of the connection to consume
    //     messages.
    //
    NvRmInterruptDone(s_TransportInterruptHandle);
}

static void
NvRmPrivProcIdGetProcessorInfo(
    NvRmDeviceHandle hDevice,
    NvRmModuleID *pProcModuleId)
{
#if NV_IS_AVP
    *pProcModuleId = NvRmModuleID_Avp;
#else
    *pProcModuleId = NvRmModuleID_Cpu;
#endif
}

/**
 * Register for the transport interrupts.
 */
static NvError
RegisterTransportInterrupt(NvRmDeviceHandle hDevice)
{
    NvOsInterruptHandler DmaIntHandlers[MAX_INT_FOR_TRANSPORT];
    NvU32 IrqList[MAX_INT_FOR_TRANSPORT];
    NvRmModuleID ProcModuleId;

    if (s_TransportInterruptHandle)
    {
        return NvSuccess;
    }
    NvRmPrivProcIdGetProcessorInfo(hDevice, &ProcModuleId);

    if (ProcModuleId == NvRmModuleID_Cpu)
    {
        IrqList[0] = NvRmGetIrqForLogicalInterrupt(
            hDevice, NvRmModuleID_ResourceSema, CPU_TRANSPORT_INT_IBF);
        IrqList[1] = NvRmGetIrqForLogicalInterrupt(
            hDevice, NvRmModuleID_ResourceSema, CPU_TRANSPORT_INT_OBE);
    }
    else
    {
        IrqList[0] = NvRmGetIrqForLogicalInterrupt(
            hDevice, NvRmModuleID_ResourceSema, AVP_TRANSPORT_INT_IBF);
        IrqList[1] = NvRmGetIrqForLogicalInterrupt(
            hDevice, NvRmModuleID_ResourceSema, AVP_TRANSPORT_INT_OBE);
    }

    /* There is no need for registering the OutboxEmptyIsr, so we only register
     * one interrupt i.e. InboxFullIsr  */
    DmaIntHandlers[0] = InboxFullIsr;
    DmaIntHandlers[1] = OutboxEmptyIsr;
    return NvRmInterruptRegister(hDevice, 1, IrqList, DmaIntHandlers,
            hDevice, &s_TransportInterruptHandle, NV_TRUE);
}

// allocate buffers to be used for sending/receiving messages.
static void
NvRmPrivTransportAllocBuffers(NvRmDeviceHandle hRmDevice)
{
#if !NV_IS_AVP
    // These buffers are always allocated on the CPU side.  We'll pass the address over the AVP
    //

    NvError Error = NvSuccess;
    NvRmMemHandle hNewMemHandle = NULL;

    // Create memory handle and allocate memory
    Error = NvRmMemHandleAlloc(hRmDevice, NULL, 0,
                XPC_MESSAGE_ALIGNMENT_SIZE, NvOsMemAttribute_Uncached,
                (MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE)*2,
                NVRM_MEM_TAG_RM_MISC, 1, &hNewMemHandle);
    if (Error)
        goto fail;

    s_TransportInfo.MessageMemPhysAddr = NvRmMemPin(hNewMemHandle);

    // If it is success to create the memory handle.
    // We have to be able to get a mapping to this, because it is used at interrupt time!
    s_TransportInfo.hMessageMem = hNewMemHandle;
    Error = NvRmMemMap(hNewMemHandle, 0,
        (MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE)*2,
        NVOS_MEM_READ_WRITE,
        &s_TransportInfo.pTransmitMem);
    if (Error)
    {
        s_TransportInfo.pTransmitMem = NULL;
        s_TransportInfo.pReceiveMem = NULL;
    }
    else
    {
        s_TransportInfo.pReceiveMem  = (void *) (((NvUPtr)s_TransportInfo.pTransmitMem) +
                                                              MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE);
    }

    s_TransportInfo.hMessageMem = hNewMemHandle;
    NvRmMemWr32(hNewMemHandle, 0, 0xdeadf00d); // set this non-zero to throttle messages to the avp till avp is ready.
    NvRmMemWr32(hNewMemHandle,  MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE, 0);

    NvRmPrivXpcSendMessage(s_TransportInfo.hXpc, s_TransportInfo.MessageMemPhysAddr);
    return;


fail:
    NvRmMemHandleFree(hNewMemHandle);
    s_TransportInfo.hMessageMem = NULL;
    return;
#else
    return;
#endif
}


static void
NvRmPrivTransportFreeBuffers(NvRmDeviceHandle hRmDevice)
{
#if !NV_IS_AVP
    NvRmMemHandleFree(s_TransportInfo.hMessageMem);
#endif
}

/**
 * Initialize the transport structures, this is callled once
 * at NvRmOpen time.
 */
#if NVRM_TRANSPORT_IN_KERNEL
static volatile NvBool s_TransportInited = NV_FALSE;
NvError NvRmTransportInit(NvRmDeviceHandle hRmDevice)
#else
NvError NvRmTransportInit(NvRmDeviceHandle hRmDevice)
{
    NV_ASSERT(!"This API shouldn't be called yet");
    return NvSuccess;
}

NvError NvRmPrivTransportInit(NvRmDeviceHandle hRmDevice)
#endif
{
    NvError err;

    NvOsMemset(&s_TransportInfo, 0, sizeof(s_TransportInfo));
    s_TransportInfo.hDevice = hRmDevice;

    err = NvOsMutexCreate(&s_TransportInfo.mutex);
    if (err)
        goto fail;

#if !NVOS_IS_WINDOWS
    err = NvRmPrivXpcCreate(hRmDevice, &s_TransportInfo.hXpc);
    if (err)
        goto fail;

    NvRmPrivTransportAllocBuffers(hRmDevice);
#endif

    if (1)  // Used in EMUTRANS mode where the buffers cannot be mapped.
    {
        s_TmpIsrMsgBuffer = NvOsAlloc(MAX_MESSAGE_LENGTH);
        if (!s_TmpIsrMsgBuffer)
            goto fail;
    }

#if LOOPBACK_PROFILE
    {
        NvU32             TimerAddr;
        NvU32             TimerSize;

        NvRmModuleGetBaseAddress(hRmDevice, NvRmModuleID_TimerUs, &TimerAddr, &TimerSize);
        // map the us counter
        err = NvRmPhysicalMemMap(TimerAddr, TimerSize, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached, (void*)&s_TransportInfo.pTimer);
        if (err)
            goto fail;
    }

#endif

#if !NVOS_IS_WINDOWS
    err = RegisterTransportInterrupt(hRmDevice);
    if (err)
        goto fail;
#endif
#if NVRM_TRANSPORT_IN_KERNEL
    s_TransportInited = NV_TRUE;
#endif
    return NvSuccess;


fail:
#if !NVOS_IS_WINDOWS
    NvRmPrivXpcDestroy(s_TransportInfo.hXpc);
    NvRmPrivTransportFreeBuffers(hRmDevice);
#endif
    NvOsFree(s_TmpIsrMsgBuffer);
    NvOsMutexDestroy(s_TransportInfo.mutex);
    return err;
}

/**
 * DeInitialize the transport structures.
 */
#if NVRM_TRANSPORT_IN_KERNEL
void NvRmTransportDeInit(NvRmDeviceHandle hRmDevice)
#else
void NvRmTransportDeInit(NvRmDeviceHandle hRmDevice)
{
    NV_ASSERT(!"This API shouldn't be called yet");
}

void NvRmPrivTransportDeInit(NvRmDeviceHandle hRmDevice)
#endif
{
    // Unregister the interrupts.
#if !NVOS_IS_WINDOWS
    NvRmPrivXpcDestroy(s_TransportInfo.hXpc);
    NvRmPrivTransportFreeBuffers(hRmDevice);
    NvRmInterruptUnregister(hRmDevice, s_TransportInterruptHandle);
    s_TransportInterruptHandle = NULL;
#endif
    NvOsFree(s_TmpIsrMsgBuffer);
    NvOsMutexDestroy(s_TransportInfo.mutex);
}


static void
InsertPort(NvRmDeviceHandle hDevice, NvRmTransportHandle hPort)
{
    hPort->pNext = s_TransportInfo.pPortHead;
    s_TransportInfo.pPortHead = hPort;
}


static NvRmTransportHandle
FindPort(NvRmDeviceHandle hDevice, char *pPortName)
{
    NvRmTransportHandle hPort = NULL;
    NvRmTransportHandle hIter = NULL;

    hIter = s_TransportInfo.pPortHead;
    while (hIter)
    {
        if ( NvOsStrcmp(pPortName, hIter->PortName) == 0)
        {
            hPort = hIter;
            break;
        }
        hIter = hIter->pNext;
    }

    return hPort;
}


// Remove the given hPort from the list of ports
static void
DeletePort(NvRmDeviceHandle hRmDevice, const NvRmTransportHandle hPort)
{
    // Pointer to the pointer alleviates all special cases in linked list walking.
    // I wish I was clever enough to have figured this out myself.

    NvRmTransportHandle *hIter;

    hIter = &s_TransportInfo.pPortHead;
    while (*hIter)
    {
        if ( *hIter == hPort )
        {
            *hIter = (*hIter)->pNext;
            break;
        }
        hIter = &(*hIter)->pNext;
    }
}




/**
 * Open the port handle with a given port name. With the same name, only two
 * port can be open.
 * Thread Safety: It is done inside the function.
 */

NvError
NvRmTransportOpen(
    NvRmDeviceHandle hRmDevice,
    char *pPortName,
    NvOsSemaphoreHandle RecvMessageSemaphore,
    NvRmTransportHandle *phTransport)
{
    NvU32               PortNameLen;
    NvRmTransportHandle hPartner = NULL;
    NvRmTransportHandle hPort    = NULL;
    NvError             err      = NvError_InsufficientMemory;
    char                TmpName[MAX_PORT_NAME_LENGTH+1];

#if NVRM_TRANSPORT_IN_KERNEL
    while (!s_TransportInited) {
        // This can happen, if this API is called before avp init.
        NvOsSleepMS(500);
    }
#else
#if !NV_IS_AVP
    err = NvRmPrivInitAvp(hRmDevice);
    if (err)
    {
        return err;
    }
#endif
#endif
    // Look and see if this port exists anywhere.
    if (pPortName == NULL)
    {
        NvOsMutexLock(s_TransportInfo.mutex);

        pPortName = NvRmPrivTransportUniqueName();
        PortNameLen = NvOsStrlen(pPortName);
        NvOsStrncpy(TmpName, pPortName, sizeof(TmpName) );
        pPortName = TmpName;

        NvOsMutexUnlock(s_TransportInfo.mutex);
    }
    else
    {
        PortNameLen = NvOsStrlen(pPortName);
        NV_ASSERT(PortNameLen <= MAX_PORT_NAME_LENGTH);
    }

    NvOsMutexLock(s_TransportInfo.mutex);
    hPartner = FindPort(hRmDevice, pPortName);
    if (hPartner && hPartner->hConnectedPort != NULL)
    {
        NvOsMutexUnlock(s_TransportInfo.mutex);
        return NvError_TransportPortAlreadyExist;
    }

    // check if this is one of the special RPC ports used by the rm
    if ( NvOsStrcmp(pPortName, "RPC_AVP_PORT") == 0)
    {
        //If someone else wants to open this port
        //just return the one already created.
        if (hPartner)
        {
            hPort = hPartner;
            goto success;
        }
        else
        {
            hPort = &s_RpcAvpPortStruct;
            hPort->RecvMessageQueue.pReceiveMsg = (void *)&s_RpcAvpQueue[0];
        }
    }
    else if (NvOsStrcmp(pPortName, "RPC_CPU_PORT") == 0)
    {
        hPort = &s_RpcCpuPortStruct;
        hPort->RecvMessageQueue.pReceiveMsg = (void *)&s_RpcCpuQueue[0];
    }
    else
    {
        // Create a new TransportPort
        hPort = NvOsAlloc( sizeof(*hPort) );
        if (!hPort)
            goto fail;

        NvOsMemset(hPort, 0, sizeof(*hPort) );

        // Allocate the receive queue
        hPort->RecvMessageQueue.pReceiveMsg = NvOsAlloc( sizeof(RmReceiveMessage) * (MAX_MESSAGE_DEPTH+1));
        if (!hPort->RecvMessageQueue.pReceiveMsg)
            goto fail;
    }

    NvOsStrncpy(hPort->PortName, pPortName, PortNameLen);
    hPort->State =  PortState_Open;
    hPort->hConnectedPort = hPartner;

    if (RecvMessageSemaphore)
    {
        err = NvOsSemaphoreClone(RecvMessageSemaphore, &hPort->hOnPushMsgSem);
        if (err)
            goto fail;
    }

    hPort->RecvMessageQueue.QueueSize = MAX_MESSAGE_DEPTH+1;
    hPort->hRmDevice = hRmDevice;

    if (hPort->hConnectedPort != NULL)
    {
        hPort->hConnectedPort->hConnectedPort = hPort;
    }
    InsertPort(hRmDevice, hPort);


    // !!! loopback info
#if LOOPBACK_PROFILE
    if (NvOsStrcmp(hPort->PortName, "LOOPTEST") == 0)
        hPort->bLoopTest = 1;
#endif

success:
    NvOsMutexUnlock(s_TransportInfo.mutex);
    *phTransport = hPort;
    return NvSuccess;

fail:
    if (hPort)
    {
        NvOsFree(hPort->RecvMessageQueue.pReceiveMsg);
        NvOsSemaphoreDestroy(hPort->hOnPushMsgSem);
        NvOsFree(hPort);
        hPort = NULL;
    }
    NvOsMutexUnlock(s_TransportInfo.mutex);

    return err;
}


/**
 * Close the transport handle
 * Thread Safety: It is done inside the function.
 */
void NvRmTransportClose(NvRmTransportHandle hPort)
{
    NvU32 RemoteMessage[4];

    if (!hPort)
        return;

    // Look and see if this port exists anywhere.
    NV_ASSERT(hPort);


    NvOsMutexLock(s_TransportInfo.mutex);
    DeletePort(hPort->hRmDevice, hPort);  // unlink this port

    // Check if there is already a port waiting to connect, and if there is
    // switch the port state to _Destroy, and signal the waiters semaphore.
    // The "State" member is not protected by the mutex because it can be
    // updated by the ISR.
    while (hPort->State == PortState_Waiting)
    {
        NvOsAtomicCompareExchange32((NvS32*)&hPort->State, PortState_Waiting, PortState_Destroy);
        if (hPort->State == PortState_Destroy)
        {
            NvOsSemaphoreSignal(hPort->hOnConnectSem);

            // in this case, we can't complete the destroy, the signalled thread will
            // have to complete.  We just return now
            NvOsMutexUnlock(s_TransportInfo.mutex);
            return;
        }
    }

    if (hPort->hConnectedPort)
    {
        // unlink this port from the other side of the connection.
        hPort->hConnectedPort->hConnectedPort = NULL;
    }

    if (hPort->RemotePort)
    {
        RemoteMessage[0] = TransportCmd_Disconnect;
        RemoteMessage[1] = hPort->RemotePort;
        NvRmPrivTransportSendMessage(hPort->hRmDevice, RemoteMessage, 2*sizeof(NvU32));
    }

    NvOsSemaphoreDestroy(hPort->hOnPushMsgSem);


    if (hPort == &s_RpcAvpPortStruct ||
        hPort == &s_RpcCpuPortStruct)
    {
        // don't free these..
        NvOsMemset(hPort, 0, sizeof(*hPort));
    }
    else
    {
        NvOsFree(hPort->RecvMessageQueue.pReceiveMsg);
        NvOsFree(hPort);
    }

    NvOsMutexUnlock(s_TransportInfo.mutex);
}


/**
 * Wait for the connection to the other end.
 * Thread Safety: It is done inside the function.
 */
NvError
NvRmTransportWaitForConnect(
    NvRmTransportHandle hPort,
    NvU32 TimeoutMS)
{
    NvOsSemaphoreHandle hSem = NULL;
    NvError             err  = NvSuccess;

    NvOsMutexLock(s_TransportInfo.mutex);
    if (hPort->State != PortState_Open)
    {
        NvOsMutexUnlock(s_TransportInfo.mutex);
        err = NvError_TransportPortAlreadyExist;
        goto exit_gracefully;
    }

    err = NvOsSemaphoreCreate(&hSem, 0);
    if (err)
    {
        NvOsMutexUnlock(s_TransportInfo.mutex);
        goto exit_gracefully;
    }

    hPort->hOnConnectSem = hSem;
    hPort->State = PortState_Waiting;
    NvOsMutexUnlock(s_TransportInfo.mutex);

    err = NvOsSemaphoreWaitTimeout(hSem, TimeoutMS);
    if (err)
    {
        // we have to be careful here, the ISR _might_ happen just after the semaphore
        // times out.
        NvOsAtomicCompareExchange32((NvS32 *)&hPort->State, PortState_Waiting, PortState_Open);
        NV_ASSERT(hPort->State == PortState_Open || hPort->State == PortState_Connected);
        if (hPort->State == PortState_Connected)
        {
            err = NvSuccess;
        }
    }

    NvOsMutexLock(s_TransportInfo.mutex);
    hPort->hOnConnectSem = NULL;
    NvOsMutexUnlock(s_TransportInfo.mutex);

    if (hPort->State == PortState_Destroy)
    {
        // finish the destroy process
        NvRmTransportClose(hPort);
        err = NvError_TransportConnectionFailed;
    }

exit_gracefully:
    NvOsSemaphoreDestroy(hSem);
    return err;
}



static NvError
NvRmPrivTransportWaitResponse(NvRmDeviceHandle hDevice, NvU32 *response, NvU32 ResponseLength, NvU32 TimeoutMS)
{
    NvU32   CurrentTime;
    NvU32   StartTime;
    NvU32   Response;
    NvBool  GotResponse = NV_TRUE;
    NvError err         = NvError_Timeout;
    volatile NvU32 *pXpcMessage = (volatile NvU32*)s_TransportInfo.pTransmitMem;

    if (pXpcMessage == NULL)
    {
        if (!NV_IS_AVP)
        {
            Response = NvRmMemRd32(s_TransportInfo.hMessageMem, 0);
        } else
        {
            NV_ASSERT(0);
            return NvSuccess;
        }
    }
    else
    {
        Response = pXpcMessage[0];
    }

    if (Response != TransportCmd_Response)
    {
        GotResponse = NV_FALSE;

        // response is not back yet, so spin till its here.
        StartTime = NvOsGetTimeMS();
        CurrentTime = StartTime;
        while ( (CurrentTime - StartTime) < TimeoutMS )
        {
            if ( pXpcMessage && (pXpcMessage[0] == TransportCmd_Response) )
            {
                GotResponse = NV_TRUE;
                break;
            }
            else if ( !pXpcMessage )
            {
                NV_ASSERT(!"Invalid pXpcMessage pointer is accessed");
            }
            CurrentTime = NvOsGetTimeMS();
        }
    }

    if ( pXpcMessage && GotResponse )
    {
        err = NvSuccess;
        NvOsMemcpy(response, (void *)pXpcMessage, ResponseLength);
    }

    return err;
}


static NvError
NvRmPrivTransportSendMessage(NvRmDeviceHandle hDevice, NvU32 *message, NvU32 MessageLength)
{
    volatile NvU32 *bar;
    NvU32 ReadData;

    if (s_TransportInfo.pTransmitMem == NULL)
    {
        /* QT/EMUTRANS takes this code path */
        if (!NV_IS_AVP)
        {
            ReadData = NvRmMemRd32(s_TransportInfo.hMessageMem, 0);
        } else
        {
            NV_ASSERT(0);
            return NvSuccess;
        }
    }
    else
    {
        ReadData = ((NvU32*)s_TransportInfo.pTransmitMem)[0];
    }

    // Check for clear to send
    if ( ReadData != 0)
        return NvError_TransportMessageBoxFull;  // someone else is sending a message

    bar = (volatile NvU32 *)s_TransportInfo.pTransmitMem;

    if (s_TransportInfo.pTransmitMem == NULL)
    {
        /* QT/EMUTRANS takes this code path */
        NvRmMemWrite(s_TransportInfo.hMessageMem, 0, message, MessageLength);
    }
    else
    {
        NvOsMemcpy(s_TransportInfo.pTransmitMem, message, MessageLength);
        /*
         * WAR: The first and last words of memory need to be read, to make sure
         * that the data is flushed to memory from store buffers, before passing
         * memory pointer to AVP. This is to avoid AVP reading wrong data from memory
         * while correct data is in CPU's store buffers.
         */
        (void)*bar;
        (void)*(bar + ( (MessageLength+sizeof(NvU32)-1) / sizeof(NvU32) )  - 1);
    }
    NvRmPrivXpcSendMessage(s_TransportInfo.hXpc, s_TransportInfo.MessageMemPhysAddr);
    return NvSuccess;
}

NvError
NvRmTransportSendMsgInLP0(NvRmTransportHandle hPort, void *pMessageBuffer, NvU32 MessageSize)
{
    volatile NvU32 *bar;
    NvU32 ReadData;
    NvU32 TotalLength;
    NvU32 Message[3 + ((MAX_MESSAGE_LENGTH) / sizeof(NvU32))];

    NV_ASSERT(pMessageBuffer);

    Message[0] = TransportCmd_Message;
    Message[1] = hPort->RemotePort;
    Message[2] = MessageSize;

    if (MessageSize)
        NvOsMemcpy(&Message[3], pMessageBuffer, MessageSize);

    ReadData = ((NvU32*)s_TransportInfo.pTransmitMem)[0];

    // Check for clear to send
    if ( ReadData != 0)
        return NvError_TransportMessageBoxFull;  // someone else is sending a message

    bar = (volatile NvU32 *)s_TransportInfo.pTransmitMem;

    NvOsMemcpy(s_TransportInfo.pTransmitMem, Message, MessageSize + 3*sizeof(NvU32));
    TotalLength = MessageSize + (3 * sizeof(NvU32));
    /*
     * WAR: The first and last words of memory need to be read, to make sure
     * that the data is flushed to memory from store buffers, before passing
     * memory pointer to AVP. This is to avoid AVP reading wrong data from memory
     * while correct data is in CPU's store buffers.
     */
    (void)*bar;
    (void)*(bar + ( (TotalLength+sizeof(NvU32)-1) / sizeof(NvU32) )  - 1);

    NvRmPrivXpcSendMessage(s_TransportInfo.hXpc, s_TransportInfo.MessageMemPhysAddr);
    return NvSuccess;
}

static void
NvRmPrivTransportClearSend(NvRmDeviceHandle hDevice)
{
    if (s_TransportInfo.pTransmitMem == NULL)
    {
        /* QT/EMUTRANS take this path */
        if (!NV_IS_AVP)
        {
            NvRmMemWr32(s_TransportInfo.hMessageMem, 0, TransportCmd_None);
        } else
        {
            NV_ASSERT(0);
        }
    }
    else
    {
        ((NvU32*)s_TransportInfo.pTransmitMem)[0] = TransportCmd_None;
    }
}

/**
 * Make the connection to the other end.
 * Thread Safety: It is done inside the function.
 */
NvError NvRmTransportConnect(NvRmTransportHandle hPort, NvU32 TimeoutMS)
{
    NvRmTransportHandle hPartnerPort;
    NvU32               StartTime;
    NvU32               CurrentTime;
    NvU32               ConnectMessage[ MAX_PORT_NAME_LENGTH/4 + 3];
    NvError             err;


    // Look and see if there is a local port with the same name that is currently waiting, if there is
    // mark both ports as connected.

    NV_ASSERT(hPort);
    NV_ASSERT(hPort->hRmDevice);
    NV_ASSERT(hPort->State == PortState_Open);


    StartTime = NvOsGetTimeMS();
    for (;;)
    {
        // Someone is waiting for a connection here locally.
        NvOsMutexLock(s_TransportInfo.mutex);

        hPartnerPort = hPort->hConnectedPort;
        if (hPartnerPort)
        {
            // Found a local connection
            if (hPartnerPort->State == PortState_Waiting)
            {

                hPartnerPort->State = PortState_Connected;
                hPartnerPort->hConnectedPort = hPort;

                hPort->State = PortState_Connected;
                NvOsSemaphoreSignal(hPartnerPort->hOnConnectSem);
                break;
            }
        }
        else if (s_TransportInfo.hMessageMem || s_TransportInfo.pReceiveMem)  // if no shared buffer, then we can't create a remote connection.
        {
            ConnectMessage[0] = TransportCmd_Connect;
            ConnectMessage[1] = (NvU32)hPort;
            NvOsMemcpy(&ConnectMessage[2], hPort->PortName, MAX_PORT_NAME_LENGTH);

            err = NvRmPrivTransportSendMessage(hPort->hRmDevice, ConnectMessage, sizeof(ConnectMessage));
            if (!err)
            {
                // should send back 2 words of data.  Give remote side 1000ms to respond, which should be about 100x more
                // than it needs.
                NvU32 WaitTime = NV_MAX(1000, TimeoutMS);
                if (TimeoutMS == NV_WAIT_INFINITE)
                    TimeoutMS = NV_WAIT_INFINITE;

                // !!! Note, we can do this without holding the mutex...
                err = NvRmPrivTransportWaitResponse(hPort->hRmDevice, ConnectMessage, 2*sizeof(NvU32), WaitTime);
                NvRmPrivTransportClearSend(hPort->hRmDevice);
                if (err)
                {
                    // the other side is not responding to messages, doh!
                    NvOsMutexUnlock(s_TransportInfo.mutex);
                    return NvError_TransportConnectionFailed;
                }

                // check the response
                hPort->RemotePort = ConnectMessage[1];
                if (hPort->RemotePort != 0)
                {
                    hPort->State = PortState_Connected;
                    break;
                }
            }
        }
        NvOsMutexUnlock(s_TransportInfo.mutex);
        NV_ASSERT(hPort->State == PortState_Open);  // it better still be open

        // Didn't find a connection, wait a few ms and then try again
        CurrentTime = NvOsGetTimeMS();
        if ( (CurrentTime - StartTime) > TimeoutMS )
            return NvError_Timeout;

        NvOsSleepMS(10);
    }

    NvOsMutexUnlock(s_TransportInfo.mutex);
    return NvSuccess;
}


/**
 * Set the queue depth and message size of the transport handle.
 * Thread Safety: It is done inside the function.
 */
NvError NvRmTransportSetQueueDepth(
    NvRmTransportHandle hPort,
    NvU32 MaxQueueDepth,
    NvU32 MaxMessageSize)
{
    RmReceiveMessage *pNewReceiveMsg = NULL;

    NV_ASSERT(hPort != NULL);
    NV_ASSERT(MaxQueueDepth != 0);
    NV_ASSERT(MaxMessageSize != 0);

    // You cannot change the queue after a connection has been opened
    NV_ASSERT(hPort->State == PortState_Open);

    // !!! FIXME
    // Xpc does not allow changing the base message size, so we can't change the message size here (yet!)
    // Once we have per port message buffers we can set this.
    NV_ASSERT(MaxMessageSize <= MAX_MESSAGE_LENGTH);

    // These are statically allocated ports, they cannot be modified!
    // !!! FIXME: this is just a sanity check.  Remove this and make it so that
    //            cpu/avp rpc doesn't call this function and just knows that the
    //            transport will give it a port with a large enough queue to support
    //            rpc, since rpc ports and queue are statically allocated this has to be true.
    if (hPort == &s_RpcAvpPortStruct ||
        hPort == &s_RpcCpuPortStruct)
    {
        if (MaxMessageSize <= MAX_MESSAGE_LENGTH &&
            MaxQueueDepth <= MAX_MESSAGE_DEPTH)
        {
            return NvSuccess;
        }

        NV_ASSERT(!" Illegal meesage length or queue depth. ");
    }

    // Freeing default allocated message queue.
    NvOsFree(hPort->RecvMessageQueue.pReceiveMsg);
    hPort->RecvMessageQueue.pReceiveMsg = NULL;
    // create a new message queue struct, one longer than requested on purpose.
    pNewReceiveMsg = NvOsAlloc( sizeof(RmReceiveMessage) * (MaxQueueDepth+1));
    if (pNewReceiveMsg == NULL)
        return NvError_InsufficientMemory;

    hPort->RecvMessageQueue.pReceiveMsg = pNewReceiveMsg;
    hPort->RecvMessageQueue.QueueSize = (NvU16)(MaxQueueDepth+1);

    return NvSuccess;
}


static NvError
NvRmPrivTransportSendRemoteMsg(
    NvRmTransportHandle hPort,
    void* pMessageBuffer,
    NvU32 MessageSize,
    NvU32 TimeoutMS)
{
    NvError err;
    NvU32 StartTime;
    NvU32 CurrentTime;
    NvU32 Message[3 + ((MAX_MESSAGE_LENGTH) / sizeof(NvU32))];

    NV_ASSERT((MAX_MESSAGE_LENGTH) >= MessageSize);

    StartTime = NvOsGetTimeMS();

    Message[0] = TransportCmd_Message;
    Message[1] = hPort->RemotePort;
    Message[2] = MessageSize;

    NvOsMemcpy(&Message[3], pMessageBuffer, MessageSize);

    for (;;)
    {
        NvOsMutexLock(s_TransportInfo.mutex);
        err = NvRmPrivTransportSendMessage(hPort->hRmDevice, Message, MessageSize + 3*sizeof(NvU32));
        NvOsMutexUnlock(s_TransportInfo.mutex);
        if (err == NvSuccess)
        {
            return NvSuccess;
        }

        // Sleep and then try again in a few ms to send again
        CurrentTime = NvOsGetTimeMS();
        if ( TimeoutMS != NV_WAIT_INFINITE && (CurrentTime - StartTime) > TimeoutMS )
            return NvError_Timeout;
        /* Sleeping for 1msec may not sleep exactly for 1msec. It depends
         * on OS tick time. If tick time is much bigger,then this 1msec
         * sleep would cause performance issues. At the same time, if complete
         * polling is used, it can potentially block other threads from running.
         * To reduce the impact of sleep in either ways, poll for around
         * POLL_TIME_IN_MS and if operation is not complete then start sleeping.
         * POLL_TIME_IN_MS can be adjusted if it has any adverse effect.
         */
        if ( (CurrentTime - StartTime) > POLL_TIME_IN_MS )
            NvOsSleepMS(1); // try again later...
    }
}



static NvError
NvRmPrivTransportSendLocalMsg(
    NvRmTransportHandle hPort,
    void* pMessageBuffer,
    NvU32 MessageSize,
    NvU32 TimeoutMS)
{
    NvU32  CurrentTime;
    NvU32  StartTime;
    NvError err            = NvSuccess;

    NvRmTransportHandle hRemotePort;

    NvOsMutexLock(s_TransportInfo.mutex);
    hRemotePort = hPort->hConnectedPort;


    StartTime = NvOsGetTimeMS();
    CurrentTime = StartTime;

    for (;;)
    {
        // try to insert into the message into the receivers queue.
        NvBool bSuccess = InsertMessage(hRemotePort, (NvU8*)pMessageBuffer, MessageSize);
        if (bSuccess)
        {
            if (hRemotePort->hOnPushMsgSem)
                NvOsSemaphoreSignal(hRemotePort->hOnPushMsgSem);
            break;
        }

        // The destination port is full.
        if (TimeoutMS == 0)
        {
            err = NvError_TransportMessageBoxFull;
            break;
        }

        // The user wants a timeout, so we just sleep a short time so the
        // other thread can pop a message.  It would be better to use another semaphore
        // to indicate that the box is not full, but that just seems overkill since this
        // should rarely happen anyhow.
        // unlock the mutex, and wait a small amount of time.
        NvOsMutexUnlock(s_TransportInfo.mutex);

        /* Sleeping for 1msec may not sleep exactly for 1msec. It depends
         * on OS tick time. If tick time is much bigger,then this 1msec
         * sleep would cause performance issues. At the same time, if complete
         * polling is used, it can potentially block other threads from running.
         * To reduce the impact of sleep in either ways, poll for around
         * POLL_TIME_IN_MS and if operation is not complete then start sleeping.
         * POLL_TIME_IN_MS can be adjusted if it has any adverse effect.
         */
        if ( (CurrentTime - StartTime) > POLL_TIME_IN_MS )
            NvOsSleepMS(1);
        NvOsMutexLock(s_TransportInfo.mutex);
        if (TimeoutMS != NV_WAIT_INFINITE)
        {
            // check for a timeout condition.
            CurrentTime = NvOsGetTimeMS();
            if ( (CurrentTime - StartTime) >= TimeoutMS)
            {
                err = NvError_Timeout;
                break;
            }
        }
    }
    NvOsMutexUnlock(s_TransportInfo.mutex);

    return err;
}


/**
 * Send the message to the other end port.
 * Thread Safety: It is done inside the function.
 */
NvError
NvRmTransportSendMsg(
    NvRmTransportHandle hPort,
    void* pMessageBuffer,
    NvU32 MessageSize,
    NvU32 TimeoutMS)
{
    NvError err;

    NV_ASSERT(hPort);
    NV_ASSERT(hPort->State == PortState_Connected);
    NV_ASSERT(pMessageBuffer);

#if LOOPBACK_PROFILE
    if (hPort->bLoopTest)
    {
# if NV_IS_AVP
        ((NvU32*)pMessageBuffer)[LOOP_AVP_SEND_INDEX] = *s_TransportInfo.pTimer;
# else
        ((NvU32*)pMessageBuffer)[LOOP_CPU_SEND_INDEX] = *s_TransportInfo.pTimer;
# endif
    }
#endif

    if (hPort->hConnectedPort)
    {
        err = NvRmPrivTransportSendLocalMsg(hPort, pMessageBuffer, MessageSize, TimeoutMS);
    }
    else if (hPort->State == PortState_Connected)
    {
        err = NvRmPrivTransportSendRemoteMsg(hPort, pMessageBuffer, MessageSize, TimeoutMS);
    }
    else
    {
        NV_ASSERT(0);  // someone did something naughty
        err = NvError_TransportNotConnected;
    }

    return err;
}



/**
 * Receive the message from the other end port.
 * Thread Safety: It is done inside the function.
 */
NvError
NvRmTransportRecvMsg(
    NvRmTransportHandle hPort,
    void* pMessageBuffer,
    NvU32 MaxSize,
    NvU32 *pMessageSize)
{
    NvU8 TmpMessage[MAX_MESSAGE_LENGTH];

    NV_ASSERT(hPort);
    NV_ASSERT( (hPort->State == PortState_Connected) || (hPort->State == PortState_Disconnected) );
    NV_ASSERT(pMessageBuffer);
    NV_ASSERT(pMessageSize);


    NvOsMutexLock(s_TransportInfo.mutex);
    if (hPort->RecvMessageQueue.ReadIndex == hPort->RecvMessageQueue.WriteIndex)
    {
        NvOsMutexUnlock(s_TransportInfo.mutex);
        return NvError_TransportMessageBoxEmpty;
    }

    ExtractMessage(hPort, (NvU8*)pMessageBuffer, pMessageSize, MaxSize);
    if (*pMessageSize > MaxSize)
    {
        // not enough room to copy the message
        NvOsMutexUnlock(s_TransportInfo.mutex);
        NV_ASSERT(!" RM Transport: Illegal message size. ");
    }


    // if there was backpressure asserted, try to handle the currently posted message, and re-enable messages
    if (s_TransportInfo.ReceiveBackPressureOn != s_TransportInfo.ReceiveBackPressureOff)
    {
        NV_ASSERT( ((NvU8)s_TransportInfo.ReceiveBackPressureOn) == ((NvU8)(s_TransportInfo.ReceiveBackPressureOff+1)) );
        ++s_TransportInfo.ReceiveBackPressureOff;

        if (s_TransportInfo.pReceiveMem == NULL)
        {
            /* QT/EMUTRANS takes this path. */
            NvRmMemRead(s_TransportInfo.hMessageMem,
                        MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE,
                        TmpMessage,
                        MAX_MESSAGE_LENGTH);
            HandlePortMessage(hPort->hRmDevice, (volatile void *)TmpMessage);
            NvRmMemWrite(s_TransportInfo.hMessageMem,
                         MAX_MESSAGE_LENGTH + MAX_COMMAND_SIZE,
                         TmpMessage,
                         2*sizeof(NvU32) );
        }
        else
        {
            HandlePortMessage(hPort->hRmDevice, (NvU32*)s_TransportInfo.pReceiveMem);
        }
    }

#if LOOPBACK_PROFILE
    if (hPort->bLoopTest)
    {
# if NV_IS_AVP
        ((NvU32*)pMessageBuffer)[LOOP_AVP_RECV_INDEX] = *s_TransportInfo.pTimer;
# else
        ((NvU32*)pMessageBuffer)[LOOP_CPU_RECV_INDEX] = *s_TransportInfo.pTimer;
# endif
    }
#endif

    NvOsMutexUnlock(s_TransportInfo.mutex);

    return NvSuccess;
}

void
NvRmTransportGetPortName(
    NvRmTransportHandle hPort,
    NvU8 *PortName,
    NvU32 PortNameSize )
{
    NvU32 len;

    NV_ASSERT(hPort);
    NV_ASSERT(PortName);

    len = NvOsStrlen(hPort->PortName);
    if (len >= PortNameSize)
    {
        NV_ASSERT(!" RM Transport: Port Name too long. ");
    }

    NvOsStrncpy((char *)PortName, hPort->PortName, PortNameSize);
}


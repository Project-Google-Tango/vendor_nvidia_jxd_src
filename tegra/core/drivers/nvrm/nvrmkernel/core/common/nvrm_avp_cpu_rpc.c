/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
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
 * @b Description: This is the wrapper implementation of Transport API.
 */

#ifndef NVRM_TRANSPORT_IN_KERNEL
#define NVRM_TRANSPORT_IN_KERNEL 0
#endif

#include "nvrm_transport.h"
#include "nvrm_xpc.h"
#include "nvrm_rpc.h"
#include "nvrm_interrupt.h"
#include "nvassert.h"
#if NVRM_TRANSPORT_IN_KERNEL
#include "nvrm_avp_private.h"
#endif

// lobal variable for handles. 
static NvOsThreadHandle s_RecvThreadId_Service;
static NvRmRPCHandle gs_hRPCHandle = NULL;
static volatile int s_ContinueProcessing = 1;

#if !NV_IS_AVP
   #define  PORT_NAME  "RPC_CPU_PORT"
#else
   #define  PORT_NAME  "RPC_AVP_PORT"
#endif
// Receive message port thread.
static void ServiceThread( void *args );
static void ServiceThread( void *args )
{
    NvError Error = NvSuccess;
    static NvU8 ReceiveMessage[MAX_MESSAGE_LENGTH];
    NvU32 MessageLength = 0;

    Error = NvRmPrivRPCWaitForConnect(gs_hRPCHandle);
    if (Error)
    {
        goto exit_gracefully;
    }
    while (s_ContinueProcessing)
    {
        Error = NvRmPrivRPCRecvMsg(gs_hRPCHandle, ReceiveMessage,
            &MessageLength);
        if (Error == NvError_InvalidState)
        {
            break;
        }
        if (!Error)
        {
            ReceiveMessage[MessageLength] = '\0';
        }
        NvRmPrivProcessMessage(gs_hRPCHandle, (char*)ReceiveMessage,
            MessageLength);
    }

    exit_gracefully:        
        return;
}

NvError NvRmPrivRPCInit(NvRmDeviceHandle hDeviceHandle, char* portName,
    NvRmRPCHandle *hRPCHandle )
{
    NvError Error = NvSuccess;
    
    *hRPCHandle = NvOsAlloc(sizeof(NvRmRPC));
    if (!*hRPCHandle)
    {
        Error = NvError_InsufficientMemory;
        return Error;
    }
    (*hRPCHandle)->TransportRecvSemId = NULL;
    (*hRPCHandle)->svcTransportHandle = NULL;
    (*hRPCHandle)->isConnected = NV_FALSE;

    Error = NvOsSemaphoreCreate(&(*hRPCHandle)->TransportRecvSemId, 0);
    if( Error != NvSuccess)
    {
        goto clean_up;
    }
    Error = NvOsMutexCreate(&(*hRPCHandle)->RecvLock );
    if( Error != NvSuccess)
    {
        goto clean_up;
    }

#if (!NV_IS_AVP) && (NVRM_TRANSPORT_IN_KERNEL)
    Error = NvRmPrivInitAvp(hDeviceHandle);
    if (Error)
    {
        return Error;
    }
#endif

    // Create the transport handle with the port name in args.
    (*hRPCHandle)->hRmDevice = hDeviceHandle;
    Error = NvRmTransportOpen((*hRPCHandle)->hRmDevice, portName,
        (*hRPCHandle)->TransportRecvSemId, &(*hRPCHandle)->svcTransportHandle);
    if( Error != NvSuccess)
    {
        goto clean_up;
    }
    
clean_up:
    if( Error != NvSuccess)
    {
        if((*hRPCHandle)->svcTransportHandle != NULL)
            NvRmTransportClose((*hRPCHandle)->svcTransportHandle);
        if((*hRPCHandle)->TransportRecvSemId != NULL)
            NvOsSemaphoreDestroy((*hRPCHandle)->TransportRecvSemId);
        NvOsMutexDestroy((*hRPCHandle)->RecvLock);
        (*hRPCHandle)->svcTransportHandle = NULL;
    }
    return Error;
}

void NvRmPrivRPCDeInit( NvRmRPCHandle hRPCHandle )
{
    if(hRPCHandle != NULL)
    {
        if(hRPCHandle->svcTransportHandle != NULL)
        {
            NvOsSemaphoreDestroy(hRPCHandle->TransportRecvSemId);
            NvOsMutexDestroy(hRPCHandle->RecvLock);
            NvRmTransportClose(hRPCHandle->svcTransportHandle);
            hRPCHandle->svcTransportHandle = NULL;
            hRPCHandle->isConnected = NV_FALSE;
        }
        NvOsFree(hRPCHandle);
    }
}

void NvRmPrivRPCSendMsg(NvRmRPCHandle hRPCHandle,
                           void* pMessageBuffer,
                           NvU32 MessageSize)
{
    NvError Error = NvSuccess;
    NV_ASSERT(hRPCHandle->svcTransportHandle != NULL);
    
    NvOsMutexLock(hRPCHandle->RecvLock);
    Error = NvRmTransportSendMsg(hRPCHandle->svcTransportHandle,
        pMessageBuffer, MessageSize, NV_WAIT_INFINITE);
    NvOsMutexUnlock(hRPCHandle->RecvLock);
    if(Error)
    NV_ASSERT(Error == NvSuccess);
}

void NvRmPrivRPCSendMsgWithResponse( NvRmRPCHandle hRPCHandle,
                                        void* pRecvMessageBuffer,
                                        NvU32 MaxSize,
                                        NvU32 * pMessageSize,
                                        void* pSendMessageBuffer,
                                        NvU32 MessageSize)
{
    NvError Error = NvSuccess;
    NV_ASSERT(hRPCHandle->svcTransportHandle != NULL);

    NvOsMutexLock(hRPCHandle->RecvLock);
    Error = NvRmTransportSendMsg(hRPCHandle->svcTransportHandle,
        pSendMessageBuffer, MessageSize, NV_WAIT_INFINITE);
    if (Error)
    {
        // TODO: Determine cause of error and pass appropriate error to caller.
        goto clean_up;
    }
   
    NvOsSemaphoreWait(hRPCHandle->TransportRecvSemId);
   
    Error = NvRmTransportRecvMsg(hRPCHandle->svcTransportHandle,
        pRecvMessageBuffer, MaxSize, pMessageSize);
    if (Error)
    {
        goto clean_up;
    }

clean_up:
    NV_ASSERT(Error == NvSuccess);
    NvOsMutexUnlock(hRPCHandle->RecvLock);
}

NvError NvRmPrivRPCWaitForConnect( NvRmRPCHandle hRPCHandle )
{
    NvError Error = NvSuccess;

    NV_ASSERT(hRPCHandle != NULL);
    NV_ASSERT(hRPCHandle->svcTransportHandle != NULL);

    if(hRPCHandle->isConnected == NV_FALSE)
    {
        Error = NvRmTransportSetQueueDepth(hRPCHandle->svcTransportHandle,
            MAX_QUEUE_DEPTH, MAX_MESSAGE_LENGTH);
        if (Error)
        {
            goto clean_up;
        }
        Error = NvError_InvalidState;
        // Connect to the other end
        while (s_ContinueProcessing)
        {
            Error = NvRmTransportWaitForConnect(
                hRPCHandle->svcTransportHandle, 100 );
            if (Error == NvSuccess)
            {
                hRPCHandle->isConnected = NV_TRUE;
                break;
            }
            // if there is some other issue than a timeout, then bail out.
            if (Error != NvError_Timeout)
            {
                goto clean_up;
            }
        }
    }
    
clean_up: 
    return Error;
}

NvError NvRmPrivRPCConnect( NvRmRPCHandle hRPCHandle )
{
    NvError Error = NvSuccess;

    NV_ASSERT(hRPCHandle != NULL);
    NV_ASSERT(hRPCHandle->svcTransportHandle != NULL);
   
    NvOsMutexLock(hRPCHandle->RecvLock);
    if(hRPCHandle->isConnected == NV_TRUE)
    {
        goto clean_up;
    }
    Error = NvRmTransportSetQueueDepth(hRPCHandle->svcTransportHandle,
        MAX_QUEUE_DEPTH, MAX_MESSAGE_LENGTH);
    if (Error)
    {
        goto clean_up;
    }
    Error = NvError_InvalidState;

    #define CONNECTION_TIMEOUT (20 * 1000)

    // Connect to the other end with a large timeout
    // Timeout value has been increased to suit slow enviornments like
    // emulation FPGAs
    Error = NvRmTransportConnect(hRPCHandle->svcTransportHandle,
        CONNECTION_TIMEOUT );
    if(Error == NvSuccess)
    {
        hRPCHandle->isConnected = NV_TRUE;
    }

    #undef CONNECTION_TIMEOUT

clean_up:
    NvOsMutexUnlock(hRPCHandle->RecvLock);
    return Error;
}

NvError NvRmPrivRPCRecvMsg( NvRmRPCHandle hRPCHandle, void* pMessageBuffer,
    NvU32 * pMessageSize )
{
    NvError Error = NvSuccess;
    NV_ASSERT(hRPCHandle->svcTransportHandle != NULL);
   
    if (s_ContinueProcessing == 0)
    {
       Error = NvError_InvalidState;
       goto clean_up;
    }
 
    NvOsSemaphoreWait(hRPCHandle->TransportRecvSemId);
    if(s_ContinueProcessing != 0)
    {

        Error = NvRmTransportRecvMsg(hRPCHandle->svcTransportHandle,
            pMessageBuffer, MAX_MESSAGE_LENGTH, pMessageSize);
    }
    else
    {
        Error = NvError_InvalidState;
    }
clean_up:
    return Error;
}

void NvRmPrivRPCClose( NvRmRPCHandle hRPCHandle )
{
    // signal the thread to exit
    s_ContinueProcessing = 0;
    if(hRPCHandle && hRPCHandle->svcTransportHandle != NULL)
    {
        if (hRPCHandle->TransportRecvSemId)
            NvOsSemaphoreSignal(hRPCHandle->TransportRecvSemId);
    }
}

NvError NvRmPrivInitService(NvRmDeviceHandle hDeviceHandle)
{
    NvError Error = NvSuccess;
    Error = NvRmPrivRPCInit(hDeviceHandle, PORT_NAME, &gs_hRPCHandle);
    if( Error != NvSuccess)
    {
        goto exit_gracefully;
    }
    NV_ASSERT(gs_hRPCHandle != NULL);
   
#if !NV_IS_AVP
    Error = NvOsInterruptPriorityThreadCreate(ServiceThread, NULL,
        &s_RecvThreadId_Service);
#else
    Error = NvOsThreadCreate(ServiceThread, NULL, &s_RecvThreadId_Service);
#endif

    exit_gracefully:
    return Error;
}

void NvRmPrivServiceDeInit()
{
    NvRmPrivRPCClose(gs_hRPCHandle);    
    NvOsThreadJoin(s_RecvThreadId_Service);
    NvRmPrivRPCDeInit(gs_hRPCHandle);
}

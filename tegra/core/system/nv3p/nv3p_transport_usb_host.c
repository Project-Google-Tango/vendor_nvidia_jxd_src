/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nv3p.h"
#include "nv3p_bytestream.h"
#include "nv3p_transport.h"
#include "nv3p_transport_usb_host.h"

#define MAX_USB_INSTANCES 0xff
#define MAX_CMD_TIMEOUT (8*60*1000)

typedef struct Nv3pTransportRec
{
    NvUsbDeviceHandle hUsb;
} Nv3pTransport;

Nv3pTransport s_Transport;
static Nv3pTransportUsbThreadArgs s_gDataXferThread;

static void Nv3pTransportTransferData(void* args)
{
    Nv3pTransportUsbThreadArgs* cmdargs = 0;
    do
    {
        cmdargs = (Nv3pTransportUsbThreadArgs*) args;
        NvOsSemaphoreWait(cmdargs->CmdReadySema);
        if(cmdargs->ExitThread)
            break;
        if(cmdargs->IsDataSendCmd)
            cmdargs->CmdStatus = NvUsbDeviceWrite(
                                                cmdargs->hUsb,
                                                cmdargs->data,
                                                cmdargs->length );
        else
            cmdargs->CmdStatus = NvUsbDeviceRead(
                                                cmdargs->hUsb,
                                                cmdargs->data,
                                                cmdargs->length,
                                                cmdargs->received );
        NvOsSemaphoreSignal(cmdargs->CmdDoneSema);
    }while(NV_TRUE);
}

NvError
Nv3pTransportReopenUsb( Nv3pTransportHandle *hTrans, NvU32 instance )
{
    NvError e;
    Nv3pTransport *trans;

    trans = &s_Transport;
    trans->hUsb = 0;

    e = NvUsbDeviceOpen( instance, &trans->hUsb );
    if( e != NvSuccess )
    {
        goto fail;
    }

    if( !trans->hUsb )
    {
        goto fail;
    }

    *hTrans = trans;
    s_gDataXferThread.IsThreadModeEnabled = NV_FALSE;
    if(s_gDataXferThread.IsThreadModeEnabled)
    {
        s_gDataXferThread.IsThreadRunning = NV_TRUE;
        s_gDataXferThread.ExitThread= NV_FALSE;
        s_gDataXferThread.hUsb = trans->hUsb;
        e = NvOsSemaphoreCreate(&s_gDataXferThread.CmdDoneSema, 0);
        if (e != NvSuccess)
            goto fail;
        e = NvOsSemaphoreCreate(&s_gDataXferThread.CmdReadySema, 0);
        if (e != NvSuccess)
            goto fail;
    e = NvOsThreadCreate(
        Nv3pTransportTransferData,
        (void *)&s_gDataXferThread, 
        &s_gDataXferThread.hThread);
    }

    return NvSuccess;

fail:
    // FIXME: error code
    return e;
}

NvError
Nv3pTransportOpenUsb( Nv3pTransportHandle *hTrans, NvU32 instance )
{
    return Nv3pTransportReopenUsb(hTrans, instance);
}

NvError
Nv3pTransportCloseUsb( Nv3pTransportHandle hTrans )
{
    if(s_gDataXferThread.IsThreadModeEnabled)
    {
        //if data xfer thread is alive kill it
        if(s_gDataXferThread.IsThreadRunning)
        {
            s_gDataXferThread.ExitThread = NV_TRUE;
            NvOsSemaphoreSignal(s_gDataXferThread.CmdReadySema);
            NvOsThreadJoin(s_gDataXferThread.hThread);
            NvUsbDeviceClose( hTrans->hUsb );
        }
        s_gDataXferThread.IsThreadModeEnabled = NV_FALSE;
        NvOsSemaphoreDestroy(s_gDataXferThread.CmdDoneSema);
        NvOsSemaphoreDestroy(s_gDataXferThread.CmdReadySema);
    }
    else
    {
        NvUsbDeviceClose( hTrans->hUsb );
    }
    return NvSuccess;
}

NvError
Nv3pTransportSendUsb( 
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags )
{
    NvError e;
    NvU32 TimeElapsed = 0;
    if(s_gDataXferThread.IsThreadModeEnabled)
    {
        s_gDataXferThread.IsDataSendCmd = NV_TRUE;
        s_gDataXferThread.data = data;
        s_gDataXferThread.length = length;
        NvOsSemaphoreSignal(s_gDataXferThread.CmdReadySema);
        do
        {
            e = NvOsSemaphoreWaitTimeout(s_gDataXferThread.CmdDoneSema, 100);
            TimeElapsed += 100;
            if((e != NvSuccess) && (TimeElapsed >= MAX_CMD_TIMEOUT))
            {
                e = NvError_Timeout;
                //data transfer thread is not responding
                s_gDataXferThread.IsThreadRunning = NV_FALSE;
                goto fail;
            }
        } while (e != NvSuccess);

    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            NvUsbDeviceWrite( hTrans->hUsb, data, length )
        );
    }

    return NvSuccess;

fail:
    return e;
}

NvError
Nv3pTransportReceiveUsb(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags )
{
    NvError e;
    NvU32 TimeElapsed = 0;
    if(s_gDataXferThread.IsThreadModeEnabled)
    {
        s_gDataXferThread.IsDataSendCmd = NV_FALSE;
        s_gDataXferThread.data = data;
        s_gDataXferThread.length = length;
        s_gDataXferThread.received = received;
        NvOsSemaphoreSignal(s_gDataXferThread.CmdReadySema);
        do
        {
            e = NvOsSemaphoreWaitTimeout(s_gDataXferThread.CmdDoneSema, 100);
            TimeElapsed += 100;
            if((e != NvSuccess) && (TimeElapsed >= MAX_CMD_TIMEOUT))
            {
                e = NvError_Timeout;
                //data transfer thread is not responding
                s_gDataXferThread.IsThreadRunning = NV_FALSE;
                goto fail;
            }
        } while (e != NvSuccess);
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            NvUsbDeviceRead( hTrans->hUsb, data, length, received )
        );
    }
    return NvSuccess;

fail:
    return e;
}

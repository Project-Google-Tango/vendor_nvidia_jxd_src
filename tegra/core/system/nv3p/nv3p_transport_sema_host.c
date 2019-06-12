/*
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
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
#include "nv3p_transport_sema_host.h"
#include "nvapputil.h"

typedef struct SemaDataPipeRec
{
    NvU8 DataBuffer[NVFLASH_DOWNLOAD_CHUNK];
    NvU32 ValidDataBytes;
    NvU32 ValidDataIndex;
    NvOsSemaphoreHandle CmdReadySema;
    NvOsSemaphoreHandle CmdDoneSema;
} SemaDataPipe;

typedef struct Nv3pTransportRec
{
    SemaDataPipe *TxPipe;
    SemaDataPipe *RxPipe;
    NvU32 Id;
} Nv3pTransport;

static SemaDataPipe g_sTxPipe;
static SemaDataPipe g_sRxPipe;

NvError
Nv3pTransportReopenSema(Nv3pTransportHandle *hTrans, NvU32 instance)
{
    NvError e = NvSuccess;
    Nv3pTransportHandle trans = 0;
    static NvBool Initialized = NV_FALSE;;
    trans = NvOsAlloc(sizeof(g_sTxPipe));
    trans->Id = instance;

    if(!Initialized)
    {
        Initialized = NV_TRUE;
        g_sTxPipe.ValidDataBytes = 0;
        g_sTxPipe.ValidDataIndex = 0;
        g_sRxPipe.ValidDataBytes = 0;
        g_sRxPipe.ValidDataIndex = 0;
        e = NvOsSemaphoreCreate(&g_sTxPipe.CmdDoneSema, 0);
        if (e != NvSuccess)
            goto fail;
        e = NvOsSemaphoreCreate(&g_sTxPipe.CmdReadySema, 0);
        if (e != NvSuccess)
            goto fail;
        e = NvOsSemaphoreCreate(&g_sRxPipe.CmdDoneSema, 0);
        if (e != NvSuccess)
            goto fail;
        e = NvOsSemaphoreCreate(&g_sRxPipe.CmdReadySema, 0);
        if (e != NvSuccess)
            goto fail;

    }

    if(instance == 0)
    {
        trans->TxPipe = &g_sTxPipe;
        trans->RxPipe = &g_sRxPipe;
    }
    else
    {
        trans->TxPipe = &g_sRxPipe;
        trans->RxPipe = &g_sTxPipe;
    }

    *hTrans = trans;
    return NvSuccess;

fail:
    // FIXME: error code
    return e;
}

NvError
Nv3pTransportOpenSema(Nv3pTransportHandle *hTrans, NvU32 instance)
{
    return Nv3pTransportReopenSema(hTrans, instance);
}

NvError
Nv3pTransportCloseSema(Nv3pTransportHandle hTrans)
{

    if(hTrans->TxPipe->CmdDoneSema)
        NvOsSemaphoreDestroy(hTrans->TxPipe->CmdDoneSema);
    if(hTrans->TxPipe->CmdReadySema)
        NvOsSemaphoreDestroy(hTrans->TxPipe->CmdReadySema);

    hTrans->TxPipe->CmdDoneSema = 0;
    hTrans->TxPipe->CmdReadySema = 0;

    return NvSuccess;
}

NvError
Nv3pTransportSendSema(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags)
{
    NvError e = NvSuccess;
    while(hTrans->TxPipe->ValidDataBytes)
    {
        NvOsSleepMS(10);
    }
    NvOsMemcpy(hTrans->TxPipe->DataBuffer, data, length);
    hTrans->TxPipe->ValidDataBytes = length;
    hTrans->TxPipe->ValidDataIndex = 0;
    NvOsSemaphoreSignal(hTrans->TxPipe->CmdReadySema);
    NvOsSemaphoreWait(hTrans->TxPipe->CmdDoneSema);
    return e;
}

NvError
Nv3pTransportReceiveSema(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags)
{
    NvError e = NvSuccess;
    if(hTrans->RxPipe->ValidDataIndex == 0)
    {
        NvOsSemaphoreWait(hTrans->RxPipe->CmdReadySema);
        if(hTrans->RxPipe->ValidDataBytes == 0)
        {
            NV_ASSERT(NV_FALSE);
            return NvError_BadParameter;
        }
    }

    if(length <= hTrans->RxPipe->ValidDataBytes)
    {
        NvOsMemcpy(data,
          &hTrans->RxPipe->DataBuffer[hTrans->RxPipe->ValidDataIndex], length);
        hTrans->RxPipe->ValidDataBytes -= length;
        hTrans->RxPipe->ValidDataIndex += length;
    }
    else
    {
        NvOsMemcpy(data,
            &hTrans->RxPipe->DataBuffer[hTrans->RxPipe->ValidDataIndex],
            hTrans->RxPipe->ValidDataBytes);
        hTrans->RxPipe->ValidDataBytes = 0;
    }

    if(hTrans->RxPipe->ValidDataBytes == 0)
    {
        hTrans->RxPipe->ValidDataIndex = 0;
        NvOsSemaphoreSignal(hTrans->RxPipe->CmdDoneSema);
    }
    return e;
}


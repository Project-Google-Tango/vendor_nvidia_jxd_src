/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_stdio.h"
#include "nvassert.h"

#ifdef TIO_TARGET_SUPPORTED
    #define NvTioDebugf NvTioStdioTargetDebugf
#endif
//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioStdioInitPkt() - Initialize a packet
//===========================================================================
void NvTioStdioInitPkt(NvTioStdioPktHandle pkt,  const void *buffer)
{
     pkt->pPktHeader        = (NvTioStdioPktHeader *) buffer;
     pkt->pPktHeader->fd    = -1;
     pkt->pPktHeader->cmd   = -1;
     pkt->pPktHeader->checksum   = 0;
     pkt->pPktHeader->payload   = 0;
     pkt->pData             =  (char*) buffer + sizeof(NvTioStdioPktHeader);
}

//===========================================================================
// NvTioStdioPktAddChar() - add character to a stio pkt
//===========================================================================
void NvTioStdioPktAddChar(NvTioStdioPktHandle pkt, char c)
{
    *(pkt->pData + pkt->pPktHeader->payload) = c;
    pkt->pPktHeader->payload++;
}

//===========================================================================
// NvTioStdioPktAddData() - add a set of arbitrary data bytes to a pkt
//===========================================================================
void NvTioStdioPktAddData(NvTioStdioPktHandle pkt, const void *data, size_t cnt)
{
    NvTioStdioPktHeader *pHeader = pkt->pPktHeader;
    char    *dst = pkt->pData + pHeader->payload;
    NvOsMemcpy(dst, data, cnt);
    pHeader->payload += cnt;
}

//===========================================================================
// NvTioStdioPktCheckSum() - Compute the checksum of a packet
//===========================================================================
NvU32 NvTioStdioPktChecksum(NvTioStdioPktHandle pkt)
{
    NvU32     count = pkt->pPktHeader->payload;
    NvU8    *data = (NvU8*) pkt->pData;
    NvU32   checksum = 0;
    while(count)
    {
        checksum += *(data++);
        count--;
    }
    return checksum;
}

//===========================================================================
// NvTioStdioGetChars() - get more characters in input buffer
//===========================================================================
NvError NvTioStdioGetChars(NvTioRemote *remote, NvU32 bytes, NvU32 timeout_msec)
{
    NvError err;
    NvU32 len = remote->bufLen - remote->cnt;

    NV_ASSERT(remote->bufLen >= remote->cnt);
    NV_ASSERT(remote->cmdLen == -1);    // should be no pending command
    if (len < bytes)
        return DBERR(NvError_InsufficientMemory);

    len = bytes;
    err = NvTioFreadTimeout(
                    &remote->stream,
                    remote->buf + remote->cnt,
                    len,
                    &len,
                    timeout_msec);
    if (!err)
        remote->cnt += len;
#ifndef TIO_TARGET_SUPPORTED
#if NVTIO_STDIO_HOST_VERBOSE
    if(!err && len)
    {
        NvTioDebugf("host received:\n ");
        NvTioShowBytes((const NvU8*)(remote->buf + remote->cnt - len), len);
    }
#endif
#endif
    return DBERR(err);
}

//===========================================================================
// NvTioStdioEraseCommand() - erase command from buffer
//===========================================================================
void NvTioStdioEraseCommand(NvTioRemote *remote)
{
    NV_ASSERT(remote->dst == -1);
    remote->cmdLen = -1;
    remote->cnt = 0;
}

//===========================================================================
// NvTioStdioPktSend() - send a stdio packet
//===========================================================================
NvError NvTioStdioPktSend(NvTioRemote *remote, NvTioStdioPktHandle pkt)
{
    int retryCnt = 0;           // Number of times to try before failing
    NvError err;
    const void *src = pkt->pPktHeader;
    size_t      len  = sizeof(NvTioStdioPktHeader) + pkt->pPktHeader->payload;
    NV_ASSERT(remote->cmdLen == -1);

    pkt->pPktHeader->checksum = NvTioStdioPktChecksum(pkt);
    do {
        err = NvTioFwrite(&remote->stream, src, len);
        if (err)
            break;
    } while(err && retryCnt--);

    if(err) {
        NvTioDebugf("Error@%d, %s: fail to send packet. Error %#10x (%s)\n",
                        __LINE__, __FILE__, err, NV_TIO_ERROR_STRING(err));
    }
    return DBERR(err);
}

//===========================================================================
// NvTioStdioPktCheck() - Check for a complete command/reply pkt in the buffer
//                      - Only one command will be stored in the reveive buffer
//===========================================================================
NvError NvTioStdioPktCheck(NvTioRemote *remote)
{
    NvTioStdioPktHeader *header;
    size_t len;
    NvTioStdioPkt pkt;
    NvU32 checksum;

    if (remote->cmdLen >= 0)
        return NvSuccess;

    if(remote->cnt < sizeof(NvTioStdioPktHeader))
        return NvError_Timeout;

    header = (NvTioStdioPktHeader*) remote->buf;
    len = header->payload + sizeof(NvTioStdioPktHeader);

    if(remote->cnt < len)
        return NvError_Timeout;

    if(remote->cnt > len) {
        NvTioDebugf("Error @%d, %s: Package corrupted!\n", __LINE__, __FILE__);
        return DBERR(NvError_BadValue);
    }

    pkt.pPktHeader = header;
    pkt.pData = remote->buf + sizeof(NvTioStdioPktHeader);
    checksum = NvTioStdioPktChecksum(&pkt);
    if(checksum != header->checksum) {
        NvTioDebugf("Error @%d, %s: checksum error, expecting %#08x, got %#08x\n",
                    __LINE__, __FILE__, header->checksum, checksum);
        return DBERR(NvError_BadValue);
    }
    remote->cmdLen = len;
    return NvSuccess;
}

//===========================================================================
// NvTioStdioPktWait() - wait until a valid pkt is in the buffer
//===========================================================================
NvError NvTioStdioPktWait(
                NvTioRemote *remote,
                NvU32 timeout_msec)
{
    NvError err;
    NvTioStdioPktHeader *header = (NvTioStdioPktHeader*) remote->buf;
    NvU32 bytes; // max bytes to read

    for (;;) {
        err = NvTioStdioPktCheck(remote);
        if (err != NvError_Timeout)
            break;
        if(remote->cnt < sizeof(NvTioStdioPktHeader))
        {
            bytes = sizeof(NvTioStdioPktHeader) - remote->cnt;
        } else {
           // NV_ASSERT to keep nvhost from looping infinitely
           // when the packet header is corrupted.
            NV_ASSERT(header->payload <=
                NV_TIO_STDIO_MAX_PKT - sizeof(NvTioStdioPktHeader));
            bytes = sizeof(NvTioStdioPktHeader) + header->payload
                    - remote->cnt;
        }
        err = NvTioStdioGetChars(remote, bytes, timeout_msec);
        if (DBERR(err))
            break;
    }
    return DBERR(err);
}

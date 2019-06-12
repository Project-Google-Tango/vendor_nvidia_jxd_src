/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "rtp.h"
#include "rtp_video.h"

int ProcessH263Packet(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream)
{
    int P, V, PLEN;
    int header, copysize = 0, offset = 0;
    char *newbuf;

    header = (buf[0] << 8) | (buf[1]);

    P = (header >> 10) & 0x1;
    V = (header >> 9) & 0x1;
    PLEN = (header >> 3) & 0x3F;
  
    size -= 2;
    buf += 2;

    if (size <= 0)
        return -1;

    if (V) // has a VRC byte at end of header
    {
        size -= 1;
        buf += 1;
    }

    if (size <= 0)
        return -1;

    if (PLEN > 0) // has an extra picture header
    {
        size -= PLEN;
        buf += PLEN;
    }

    if (size <= 0)
        return -1;

    copysize = size;
    if (P) // Two bytes of the starting picture code are removed
    {
        size += 2;
        offset = 2;
    }

    newbuf = NvOsAlloc(size + 1);
    if (!newbuf)
        return -1;

    if (P)
    {
        newbuf[0] = 0;
        newbuf[1] = 0;
    }

    NvOsMemcpy(newbuf + offset, buf, copysize);

    packet->size = size;
    packet->buf = (NvU8 *)newbuf;
    packet->timestamp = GetTimestampDiff(stream, timestamp) *
                            10000000 / stream->nClockRate;

    packet->seqnum = seq;
    packet->flags = (M) ? PACKET_LAST : 0;

//NvOsDebugPrintf("new h263 packet: seq: %d ts: %d pts: %f size: %d M: %d V: %d P: %d\n", seq, timestamp, packet->timestamp / 10000.0, size, M, V, P);

    return 0;
}

int ProcessMP4VPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream)
{
    char *newbuf;

    if (size <= 0)
        return -1;

    newbuf = NvOsAlloc(size + 1);
    if (!newbuf)
        return -1;

    NvOsMemcpy(newbuf, buf, size);

    packet->size = size;
    packet->buf = (NvU8 *)newbuf;
    packet->timestamp = GetTimestampDiff(stream, timestamp) *
                            10000000 / stream->nClockRate;
    packet->seqnum = seq;
    packet->flags = (M) ? PACKET_LAST : 0;

//NvOsDebugPrintf("new mp4v packet: seq: %d ts: %d pts: %f size: %d M: %d\n", seq, timestamp, packet->timestamp / 10000.0, size, M);

    return 0;
}

int ProcessASFPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream)
{
    int retval = 0;
    char *newbuf;
    int L, R, D, I;
    NvU32 lenoffset;
    unsigned char hdr = ((unsigned char*)buf)[0];
    int alloclen;

    if (size < 4)
        return -1;

    L = (hdr >> 6) & 0x1;
    R = (hdr >> 5) & 0x1;
    D = (hdr >> 4) & 0x1;
    I = (hdr >> 3) & 0x1;

    buf[0] = 0;

    lenoffset = NvMMNToHL(*(unsigned int*)buf);

    buf += 4;
    size -= 4;

    if (R)
    {
        buf += 4;
        size -= 4;
    }

    if (D)
    {
        buf += 4;
        size -= 4;
    }

    if (I)
    {
        buf += 4;
        size -= 4;
    }

    if (size < 0)
        return -1;

//    NvOsDebugPrintf("asf: %d %d %d %d %d %d\n", S, L, R, D, I, lenoffset);

    alloclen = size;
    if (alloclen < stream->nMaxPacketLen)
        alloclen = stream->nMaxPacketLen;

    if (!L)
    {
        NvBool bCreateNew = NV_TRUE;
        NvU64 curTS = GetTimestampDiff(stream, timestamp) * 10000;

        retval = -1;

        if (stream->pReconPacket && stream->nReconPacketTS == curTS)
        {
            // part of an existing packet
            newbuf = stream->pReconPacket;
            NvOsMemcpy(newbuf + lenoffset, buf, size);
            bCreateNew = NV_FALSE;
        }
        else if (stream->pReconPacket)
        {
            // old packet is now finished
            packet->size = stream->nReconPacketLen;
            packet->buf = (NvU8 *)stream->pReconPacket;
            stream->pReconPacket = NULL;
            packet->timestamp = stream->nReconPacketTS;
            packet->seqnum = seq;
            packet->flags = (M) ? PACKET_LAST : 0;

            retval = 0;
        }

        if (bCreateNew)
        {
            newbuf = stream->pReconPacket = NvOsAlloc(alloclen + 1);
            if (!newbuf)
                return -1;

            NvOsMemset(newbuf, 0, alloclen + 1);
            stream->nReconPacketLen = alloclen;
            stream->nReconPacketTS = curTS;

            NvOsMemcpy(newbuf + lenoffset, buf, size);
        }
    }
    else
    {
        newbuf = NvOsAlloc(alloclen + 1);
        if (!newbuf)
            return -1;

        NvOsMemset(newbuf, 0, alloclen + 1);

        NvOsMemcpy(newbuf, buf, size);

        packet->size = alloclen;
        packet->buf = (NvU8 *)newbuf;
        packet->timestamp = GetTimestampDiff(stream, timestamp) * 10000; // -> 100ns
        packet->seqnum = seq;
        packet->flags = (M) ? PACKET_LAST : 0;

//NvOsDebugPrintf("new asf packet: seq: %d ts: %d pts: %f size: %d (%d) M: %d -- stream: %d %d\n", seq, timestamp, packet->timestamp / 10000.0, size, alloclen, M, lenoffset, L);

        return 0;
    }

    return retval;
}


int ProcessVC1Packet(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream)
{
    int retval = 0;
    char *newbuf;
    int alloclen;
    NvU32 header, auControlHeader;
    int FRAG;

    if(size <= 0)
        return -1;

/*  Structure of AU header */
/*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
/*  |AU     | RA    |  AUP  | PTS   | DTS   | */
/*  |Control| Count |  Len  | Delta | Delta | */
/*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

    header = NvMMNToHL(*(unsigned int*)buf);
    auControlHeader = (header >> 24);

    buf  += 12;
    size -= 12;

/*  Syntax of AU Control field */ 
/*  0    1    2    3    4    5    6    7 */
/*  +----+----+----+----+----+----+----+----+ */
/*  |  FRAG   | RA | SL | LP | PT | DT | R  | */
/*  +----+----+----+----+----+----+----+----+ */

    FRAG = (auControlHeader >> 6) & 0x3;

    alloclen = size;
    if (alloclen < stream->nMaxPacketLen)
        alloclen = stream->nMaxPacketLen;

    switch (FRAG)
    {
        /* The AU payload contains a fragment of a frame other than the first or last fragment */
        case 0:
    
            if (stream->pReconPacket)
            {
                // part of an existing packet
                newbuf = stream->pReconPacket;
                NvOsMemcpy(newbuf + stream->nReconDataLen, buf, size);
                stream->nReconDataLen += size;
            }
            retval = -1;
            break;

        /* The AU payload contains the first fragment of a frame */
        case 1:
            if (stream->pReconPacket == NULL)
            {
                newbuf = stream->pReconPacket = NvOsAlloc(alloclen + 1);
                if (!newbuf)
                    return -1;
                NvOsMemset(newbuf, 0, alloclen + 1);
                stream->nReconPacketLen = alloclen;
                NvOsMemcpy(newbuf, buf, size);
                stream->nReconDataLen += size;
                retval = -1;
            }
            else
            {
                /* Discard old fragment */
                NvOsFree(stream->pReconPacket);
                newbuf = stream->pReconPacket = NvOsAlloc(alloclen + 1);
                if (!newbuf)
                    return -1;
                NvOsMemset(newbuf, 0, alloclen + 1);
                stream->nReconPacketLen = alloclen;
                NvOsMemcpy(newbuf, buf, size);
                stream->nReconDataLen += size;
                retval = -1;
            }
            break;

        /* The AU payload contains the last fragment of a frame */
        case 2:
            if (stream->pReconPacket)
            {
                newbuf = stream->pReconPacket;
                NvOsMemcpy(newbuf, buf, size);
                packet->size = stream->nReconPacketLen;
                packet->buf = (NvU8 *)newbuf;
                packet->timestamp = GetTimestampDiff(stream, timestamp) * 10000; 
                packet->seqnum = seq;
                packet->flags = (M) ? PACKET_LAST : 0;
                retval = 0;
            }
            break;

            /* Complete frame */
        case 3:
            {
                newbuf = NvOsAlloc(alloclen + 1);
                if(!newbuf)
                    return -1;
                NvOsMemset(newbuf, 0, alloclen + 1);
                NvOsMemcpy(newbuf, buf, size);
                packet->size = alloclen;
                packet->buf = (NvU8 *)newbuf;
                packet->timestamp = GetTimestampDiff(stream, timestamp) * 10000; 
                packet->seqnum = seq;
                packet->flags = (M) ? PACKET_LAST : 0;
                retval = 0;
            }
            break;

        default:
            break;
    }

    return retval;
}


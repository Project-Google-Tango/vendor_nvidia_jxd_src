/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "rtp.h"
#include "rtp_audio.h"
#include "nvmm_logger.h"

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

/* size of packed frame for each mode, excluding TOC byte  for AMR_WB*/
static NvU8 packed_size_amr_wb[16] = {17, 23, 32, 36, 40, 46, 50, 58,
                                        60,  5,  0,  0,  0,  0,  0,  0};
/* size of packed frame for each mode, excluding TOC byte for AMR_NB*/
static NvU8 packed_size_amr_nb[16] = {12, 13, 15, 17, 19, 20, 26, 31,
                                        5, 0, 0, 0, 0, 0, 0, 0};
/* Assuming Maximum AMR frames in one packet is 512, thic can be changed if a packet have more than 512*/
#define MAX_AMR_PACKET_FRAMES 512
/* FT value for AMR_WB SID/DTX frame is 9*/
#define AMR_WB_FT_FOR_SID_FRAME 9
/* FT value for AMR_WB SID/DTX frame is 8*/
#define AMR_NB_FT_FOR_SID_FRAME 8
/* FT value for AMR_WB and AMR_NB NODATA frame is 15*/
#define AMR_FT_FOR_NODATA_FRAME 15

/* RFC 3267, but supporting octet-aligned with no ILL/ILP/CRC junk only */
int ProcessAMRPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                     RTPPacket *packet, RTPStream *stream)
{
    int F = 1, FT;
    char *newbuf, *tmp;
    int numbufs = 0, i=0;
    NvU8 TocFT[MAX_AMR_PACKET_FRAMES], bufsize=0, *tocbyte =0;
    NvS32 headerIndex = 0, payloadIndex = 0, sidframe = 0;

    if (size < 2)
        return -1;

    size -= 1;
    buf += 1;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
        "++ProcessAMRPacket :: size = %d, seq = %d",size,seq));

    tmp = buf;
    while (tmp < buf + size)
    {
        F = (tmp[0] >> 7) & 0x1;
        FT = (tmp[0] & 0x78) >> 3;

        TocFT[numbufs] = FT;
        numbufs++;

        if ((!F) || (numbufs >= MAX_AMR_PACKET_FRAMES))
            break;

        tmp[0] = tmp[0] & 0x7f;  // clear F bit
        tmp++;
    }

    if (numbufs > MAX_AMR_PACKET_FRAMES)
    {
         NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
            "Alert!!!Max Frames :%d in one packet are Crossed the Limit : %d",
             numbufs,MAX_AMR_PACKET_FRAMES));
    }

    newbuf = NvOsAlloc(size);
    if (!newbuf)
        return -1;

    if (stream->eCodecType == NvStreamType_WAMR)
    {
        sidframe = AMR_WB_FT_FOR_SID_FRAME;
        tocbyte = packed_size_amr_wb;
    }
    else if (stream->eCodecType == NvStreamType_NAMR)
    {
        sidframe = AMR_NB_FT_FOR_SID_FRAME;
        tocbyte = packed_size_amr_nb;
    }

    for (i = 0; i < numbufs; i++)
    {
        FT = TocFT[i];
        bufsize = tocbyte[FT];

        //handling valid frames (FT: 0-8 for AMR_WB and FT :0-7 AMR_NB)
        //and SID frame (FT:9 for AMR_WB and FT:8 for AMR_NB)
        if ((FT >=0) && (FT <= sidframe))
        {
            // copy header
            NvOsMemcpy(&(newbuf[headerIndex]), &(buf[i]), 1);
            // copy payload
            NvOsMemcpy(&(newbuf[headerIndex + 1]),
                       &(buf[numbufs + payloadIndex]), bufsize);

            headerIndex += (bufsize + 1);
            payloadIndex += bufsize;

            if (FT == sidframe)
            {
                 NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                  "SID:FT:%d N:%d BS:%d H:%d P:%d PS:%d HIdx:%d PIdx:%d S:%d",
                        FT,numbufs,bufsize,headerIndex-(bufsize + 1),
                        (headerIndex-(bufsize + 1))+1,
                        numbufs + (payloadIndex-bufsize),
                        headerIndex,payloadIndex,(i+1)+payloadIndex));
            }
            else
            {
                 NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                 "Valid:FT:%d N:%d BS:%d H:%d P:%d PS:%d HIdx:%d PIdx:%d S:%d",
                        FT,numbufs,bufsize,headerIndex-(bufsize + 1),
                        (headerIndex-(bufsize + 1))+1,
                        numbufs + (payloadIndex-bufsize),
                        headerIndex,payloadIndex,(i+1)+payloadIndex));
            }
        }
        //NO_DATA Frame detected, no need to send payload and just sending header byte
        else if ((FT > sidframe) && (FT <=AMR_FT_FOR_NODATA_FRAME))
        {
            // copy header
             NvOsMemcpy(&(newbuf[headerIndex]), &(buf[i]), 1);
             headerIndex += 1;
              NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                    "NO_DATA:FT:%d N:%d H:%d HIdx:%d S:%d",
                     FT,numbufs,headerIndex-1,headerIndex,(i+1)+payloadIndex));
        }
    }

    packet->size = size;
    packet->buf = (NvU8 *)newbuf;
    packet->timestamp = GetTimestampDiff(stream, timestamp) *
                        10000000 / stream->nClockRate;
    packet->seqnum = seq;
    packet->flags = PACKET_LAST;

//NvOsDebugPrintf("new amr packet: seq: %d ts: %d pts: %f size: %d\n", seq, timestamp, packet->timestamp / 10000.0, size);

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "--ProcessAMRPacket"));

    return 0;
}


static int simpleGetBits (simpleBitstreamPtr self, const unsigned int numberOfBits)
{
    if (self->validBits <= 16)
    {
        self->validBits += 16 ;
        self->dataWord <<= 16 ;
        self->dataWord |= (unsigned int) *self->buffer++ << 8 ;
        self->dataWord |= (unsigned int) *self->buffer++ ;
    }

    self->readBits += numberOfBits ;
    self->validBits -= numberOfBits ;
    return ((self->dataWord >> self->validBits) & ((1L << numberOfBits) - 1)) ;
}

/* RFC 3640, very minimal support */
int ProcessAACPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                     RTPPacket *packet, RTPStream *stream)
{
    int auheaderlen, hdrlen, i, numheaders;
    char *newbuf, *hdrstart;
    simpleBitstream bs;

    if (size < 2)
        return -1;

    // read AU header len
    auheaderlen = (buf[1] & 0xff) | (buf[0] << 8);
    auheaderlen = (auheaderlen + 7) / 8;

    size -= 2;
    buf += 2;

    hdrlen = (stream->sizelength + stream->indexlength + 7) / 8;
    numheaders = auheaderlen / hdrlen;
    
    hdrstart = buf;

    // skip rest of AU header
    size -= auheaderlen;
    buf += auheaderlen;

    bs.buffer = (NvU8*)hdrstart;
    bs.readBits = bs.dataWord = bs.validBits = 0;

    for (i = 0; i < numheaders; i++)
    {
        int datasize = simpleGetBits(&bs, stream->sizelength);
        int index = simpleGetBits(&bs, stream->indexlength);

        if (datasize < 0)
            return -1;

        if (index != 0)
        {
            NvOsDebugPrintf("interleaved 3640 packets not supported yet\n");
        }

        newbuf = NvOsAlloc(datasize);
        if (!newbuf)
            return -1;

        NvOsMemcpy(newbuf, buf, datasize);

        buf += datasize;
        size -= datasize;

        packet->size = datasize;
        packet->buf = (NvU8 *)newbuf;
        packet->timestamp = GetTimestampDiff(stream, timestamp) *
                            10000000 / stream->nClockRate;
        packet->seqnum = seq;
        packet->flags = (M) ? PACKET_LAST : 0;

//NvOsDebugPrintf("new aac packet: %d seq: %d ts: %d pts: %f size: %d\n", i, seq, timestamp, packet->timestamp / 10000.0, datasize);

        // Allow the RTP calling code to deliver the final packet
        if (i < numheaders - 1)
        {
            // Add the TSOffset if we are inserting before processing the final packet
            // This addition is to handle SEEK cases, where TSOffset is non-zero
            packet->timestamp += stream->TSOffset;
            if (NvSuccess != InsertInOrder(stream->oPacketQueue, packet))
            {
                if (packet->buf)
                    NvOsFree(packet->buf);
                return -1;
            }
        }
    }

    return 0;
}


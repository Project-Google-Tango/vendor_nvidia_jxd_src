/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtp.h"
#include "rtp_video_h264.h"
#include "rtp_video.h"

/* H.264 specific FMTP processing */
NvError ParseH264FMTPLine(RTPStream *stream, char *line)
{
    char *s = line;
    RTSPH264MetaData* pH264MetaData;

    if(!stream->codecContext)
    {
        stream->codecContext = NvOsAlloc(sizeof(RTSPH264MetaData));
        if(!stream->codecContext)
            return NvError_InsufficientMemory;
        stream->nCodecContextLen = sizeof(RTSPH264MetaData);
        NvOsMemset(stream->codecContext, 0, stream->nCodecContextLen);
    }
    pH264MetaData = (RTSPH264MetaData*)stream->codecContext;

    while (*s != '\0')
    {
        if (!NvOsStrncmp("profile-level-id=", s, 17))
        {
            s += 17;
            pH264MetaData->profileLevelId = atoi(s);
        }
        else if (!NvOsStrncmp("max-mbps=", s, 9))
        {
            s += 9;
            pH264MetaData->maxMbps = atoi(s);
        }
        else if (!NvOsStrncmp("max-fs=", s, 7))
        {
            s += 17;
            pH264MetaData->maxFs = atoi(s);
        }
        else if (!NvOsStrncmp("max-cpb=", s, 8))
        {
            s += 8;
            pH264MetaData->maxCpb = atoi(s);
        }
        else if (!NvOsStrncmp("max-dpb=", s, 8))
        {
            s += 8;
            pH264MetaData->maxDpb = atoi(s);
        }
        else if (!NvOsStrncmp("max-br=", s, 7))
        {
            s += 7;
            pH264MetaData->maxBr = atoi(s);
        }
        else if (!NvOsStrncmp("redundant-pic-cap=", s, 18))
        {
            s += 18;
            pH264MetaData->redundantPicCap = atoi(s);
        }
        else if (!NvOsStrncmp("sprop-parameter-sets=", s, 21))
        {
            int seq_len = 0;
            int pic_len = 0;
            int nConfigLen = 0;
            char *seq_param_set;
            int seq_param_set_size;
            char *pic_param_set;
            int pic_param_set_size;
            NvU8 *temp_buf = NULL;

            s += 21;
            if (stream->configData)
            {
            goto skip; 
                NvOsFree(stream->configData);
                stream->configData = NULL;
                stream->nConfigLen = 0;
            }

            while (*s != ';' && *s != '\0')
            {
                if(*s == ',')
                {
                    s ++;
                    break;
                }
                s ++;
                seq_len ++;
            }
            seq_param_set = NvOsAlloc(seq_len + 1);
            if(!seq_param_set)
            {
                return NvError_InsufficientMemory;
            }
            NvOsMemcpy(seq_param_set, (s-(seq_len+1)), seq_len);
            while (*s != ';' && *s != '\0')
            {
                s ++;
                pic_len ++;
            }
            pic_param_set = NvOsAlloc(pic_len + 1);
            if(!pic_param_set)
            {
                NvOsFree(seq_param_set);
                return NvError_InsufficientMemory;
            }

            NvOsMemcpy(pic_param_set, (s-pic_len), pic_len);

            seq_param_set_size = DecodeBase64(seq_param_set, seq_param_set, seq_len);
            pic_param_set_size = DecodeBase64(pic_param_set, pic_param_set, pic_len);
            nConfigLen = seq_param_set_size + pic_param_set_size + 8;
            stream->configData = NvOsAlloc(nConfigLen + 1);
            if(!stream->configData)
            {
                NvOsFree(seq_param_set);
                NvOsFree(pic_param_set);
                return NvError_InsufficientMemory;
            }

            temp_buf = stream->configData;
            stream->nConfigLen = nConfigLen;
            NvOsMemset(temp_buf, 0, nConfigLen + 1);

            temp_buf[0] = (NvU8)((seq_param_set_size>>24)&0xFF);
            temp_buf[1] = (NvU8)((seq_param_set_size>>16)&0xFF);
            temp_buf[2] = (NvU8)((seq_param_set_size>>8)&0xFF);
            temp_buf[3] = (NvU8)((seq_param_set_size)&0xFF);
            temp_buf    += 4;
            NvOsMemcpy(temp_buf, seq_param_set, seq_param_set_size);
            temp_buf    += seq_param_set_size;
            temp_buf[0] = (NvU8)((pic_param_set_size>>24)&0xFF);
            temp_buf[1] = (NvU8)((pic_param_set_size>>16)&0xFF);
            temp_buf[2] = (NvU8)((pic_param_set_size>>8)&0xFF);
            temp_buf[3] = (NvU8)((pic_param_set_size)&0xFF);
            temp_buf    += 4;
            NvOsMemcpy(temp_buf, pic_param_set, pic_param_set_size);

            NvOsFree(seq_param_set);
            NvOsFree(pic_param_set);

        }
        else if (!NvOsStrncmp("parameter-add=", s, 14))
        {
            s += 14;
            pH264MetaData->parameterAdd = atoi(s);
        }
        else if (!NvOsStrncmp("packetization-mode=", s, 19))
        {
            s += 19;
            pH264MetaData->packetizationMode = atoi(s);
        }
        else if (!NvOsStrncmp("sprop-interleaving-depth=", s, 25))
        {
            s += 25;
            pH264MetaData->spropInterleavingDepth = atoi(s);
        }
        else if (!NvOsStrncmp("deint-buf-cap=", s, 14))
        {
            s += 14;
            pH264MetaData->deintBufCap = atoi(s);
        }
        else if (!NvOsStrncmp("sprop-deint-buf-req=", s, 20))
        {
            s += 20;
            pH264MetaData->spropDeintBufReq = atoi(s);
        }
        else if (!NvOsStrncmp("sprop-init-buf-time=", s, 20))
        {
            s += 20;
            pH264MetaData->spropInitBufTime = atoi(s);
        }
        else if (!NvOsStrncmp("sprop-max-don-diff=", s, 19))
        {
            s += 19;
            pH264MetaData->spropMaxDonDiff = atoi(s);
        }
        else if (!NvOsStrncmp("max-rcmd-nalu-size=", s, 19))
        {
            s += 19;
            pH264MetaData->maxRcmdNaluSize = atoi(s);
        }
skip:
        while (*s != ';' && *s != '\0')
            s++;
        if (*s == ';')
            s++;
        while (*s == ' ' && *s != '\0')
            s++;
    }

    return NvSuccess;
}

/* Bubble sort packets inside interleve list based on don distance */
static void SortInterleavePacketList(RTPPacket **pPacketList, NvU32 nNal)
{
    NvU32 i,j;
    RTPPacket *mPacket;
    for(i = 0; i < nNal; i ++)
    {
        for(j = 0; j < nNal-1; j ++)
        {
            if(((RTSPH264InterleaveData *)pPacketList[j]->buf)->dDon < ((RTSPH264InterleaveData *)pPacketList[j+1]->buf)->dDon)
            {
                mPacket = pPacketList[j+1];
                pPacketList[j+1] = pPacketList[j];
                pPacketList[j] = mPacket;
            }
        }
    }
}

/* Set Recon packet fields */
static void SetReconPacket(RTPStream *stream, char *pPacket, int nPacketLen, int nDataLen)
{
    if(stream)
    {
        stream->pReconPacket    = pPacket;
        stream->nReconPacketLen = nPacketLen;
        stream->nReconDataLen   = nDataLen;
    }
}

/* Reset Recon packet fields */
static void ResetReconPacket(RTPStream *stream)
{
    if(stream)
    {
        stream->pReconPacket    = NULL;
        stream->nReconPacketLen = 0;
        stream->nReconDataLen   = 0;
    }
}

/* Calculate don distance */
static NvS32 CalculateDonDistance(RTPStream *stream, NvU32 mDon)
{
    RTSPH264MetaData* pH264MetaData = (RTSPH264MetaData*)stream->codecContext;

    if(mDon > pH264MetaData->pDon)
        return (mDon - pH264MetaData->pDon);
    else
        return (65535 - pH264MetaData->pDon + mDon + 1);
}

/* Calculate Absolute don */
static NvS32 CalculateAbsDon(NvU32 mAbsDon, NvU32 mDon, NvU32 nDon)
{
    if(mDon == nDon) 
        return (NvS32)mAbsDon;

    if(mDon < nDon && (nDon - mDon < 32768))
        return ((NvS32)mAbsDon+ (NvS32)nDon - (NvS32)mDon);

    if(mDon > nDon && (mDon - nDon >= 32768))
        return ((NvS32)mAbsDon + 65536 - mDon + nDon);

    if(mDon < nDon && (nDon - mDon >= 32768))
        return ((NvS32)mAbsDon - ((NvS32)mDon + 65536 - (NvS32)nDon));

    if(mDon > nDon && (mDon - nDon < 32768))
        return ((NvS32)mAbsDon - ((NvS32)mDon - (NvS32)nDon));

    return -1;
}

/* Calculate don difference */
static NvS32 CalculateDonDiff(NvU32 mDon, NvU32 nDon)
{
    if(mDon == nDon) 
        return 0;

    if(mDon < nDon && (nDon - mDon < 32768))
        return ((NvS32)nDon - (NvS32)mDon);

    if(mDon < nDon && (nDon - mDon >= 32768))
        return ((NvS32)nDon - (NvS32)mDon - 65536);

    if(mDon > nDon && (mDon - nDon < 32768))
        return -((NvS32)mDon - (NvS32)nDon);

    if(mDon > nDon && (mDon - nDon >= 32768))
        return (65536 - mDon + nDon);

    return 0;
}

/* Try to dequeue packes from interleave list */
static int GetPacketFromInterleaveList(NvU32 timestamp, 
                                       char *buf, 
                                       int size, 
                                       RTPPacket *packet, 
                                       RTPStream *stream)
{
    int retval                     = 0;
    char *new_buf                  = NULL;
    char *tmp_buf                  = NULL;
    unsigned char start_sequence[] = {0, 0};
    NvBool bDeliverPacket          = NV_FALSE;
    NvU16 nal_size                 = 0; 
    NvU32 vclNalCount,i,k;
    NvU32 mAbsDon, nAbsDon;
    NvU32 gAbsDon = 0;
    NvU32 sAbsDon = 0;
    NvU32 mDon, nDon, dDon, tempdDon, donDiff;
    NvU32 nElement;
    NvU64 time_stamp;
    NvError status                 = NvSuccess;
    RTSPH264InterleaveData *interleaveData;
    RTPPacket deinPacket;
    RTPPacket **pPacketList;
    RTSPH264MetaData* pH264MetaData = (RTSPH264MetaData*)stream->codecContext;

    /* Step 1: Are there N VCL NAL units in the deinterleaving list? */
    /* get number of NAL units in re-order list */
    vclNalCount = GetListSize(pH264MetaData->oH264PacketQueue);

    /* more than N VCL NAL units in the deinterleaving list? */
    if(vclNalCount >= pH264MetaData->spropInterleavingDepth)
        bDeliverPacket = NV_TRUE;

    /* Step 2: If sprop-max-don-diff is present, don_diff(m,n) is > sprop-max-don-diff */
    if(pH264MetaData->bSpropMaxDonDiff == NV_TRUE)
    {
        gAbsDon = 0;
        sAbsDon = 0;

        if(vclNalCount >= 1)
        {
            status = PeekListElement(pH264MetaData->oH264PacketQueue, &deinPacket, 0);
            if(status != NvSuccess)
            {
                retval = -1;
                return retval;
            }
            interleaveData = (RTSPH264InterleaveData*)(deinPacket.buf);

            mAbsDon = interleaveData->nalDon; 
            mDon = interleaveData->nalDon; 

            if(mAbsDon > gAbsDon) gAbsDon = mAbsDon;
            if(mAbsDon < sAbsDon) sAbsDon = mAbsDon;

            interleaveData->absDon = mAbsDon;

            for(i = 1; i < vclNalCount; i++)
            {
                status = PeekListElement(pH264MetaData->oH264PacketQueue, &deinPacket, i);
                if(status != NvSuccess)
                {
                    retval = -1;
                    return retval;
                }
                interleaveData = (RTSPH264InterleaveData*)deinPacket.buf;
                nDon = interleaveData->nalDon; 

                /* calculate ABS DON for m and n NAL units */
                nAbsDon = CalculateAbsDon(mAbsDon, mDon, nDon);

                if(nAbsDon > gAbsDon) gAbsDon = nAbsDon;
                if(nAbsDon < sAbsDon) sAbsDon = nAbsDon;

                mDon    = nDon;
                mAbsDon = nAbsDon;
                interleaveData->absDon = mAbsDon; /* store AbsDon */
            }
            /* calculate DON diff for m and n ABS dons */
            donDiff = CalculateDonDiff(sAbsDon, gAbsDon);
            if(donDiff > pH264MetaData->spropMaxDonDiff)
                bDeliverPacket = NV_TRUE;
        }
    }

    /* Step 3:Initial buffering has lasted for the duration => sprop-init-buf-time */

    /* initial buffering has lasted for the duration =>  sprop-init-buf-time? */
    time_stamp = GetTimestampDiff(stream, timestamp);
    if(pH264MetaData->spropInitBufTime)
    {
        if(time_stamp > pH264MetaData->spropInitBufTime)
            bDeliverPacket = NV_TRUE;
    }

    if(bDeliverPacket == NV_TRUE)
    {
        if(pH264MetaData->bSpropMaxDonDiff == NV_TRUE)
        {
            if(vclNalCount >= 1)
            {
                pPacketList = NvOsAlloc(sizeof(RTPPacket) * vclNalCount);
                if(!pPacketList)
                {
                    retval = -1;
                    return retval;
                }

                k = 0;
                for(i = 0; i < vclNalCount; i++)
                {
                    status = PeekListElement(pH264MetaData->oH264PacketQueue, &deinPacket, i);
                    if(status != NvSuccess)
                        break;
                    interleaveData = (RTSPH264InterleaveData*)deinPacket.buf;
                    nDon = interleaveData->nalDon; 
                    donDiff = CalculateDonDiff(interleaveData->absDon, gAbsDon);
                    if(donDiff > pH264MetaData->spropMaxDonDiff)
                    {
                        dDon = CalculateDonDistance(stream, nDon);
                        interleaveData->dDon = dDon;
                        status = RemoveListElement(pH264MetaData->oH264PacketQueue, pPacketList[k], i);
                        if(status != NvSuccess)
                            break;
                        k ++;
                    }
                }

                /* Sort packets in PDON order */
                if(k >1)
                    SortInterleavePacketList(pPacketList, k);

                nal_size = 0; 
                for(i = 0; i < k; i++)
                {
                    tmp_buf = (char *)pPacketList[i]->buf;
                    tmp_buf += sizeof(RTSPH264InterleaveData); /* skip DON related fileds */
                    nal_size += ((tmp_buf[0] << 8) | tmp_buf[1]); 
                }

                new_buf = NvOsAlloc((k * (sizeof(start_sequence) + 2)) + nal_size);
                if(!new_buf)
                {
                    retval = -1;
                    return retval;
                }
                for(i = 0; i < k; i++)
                {
                    tmp_buf = (char *)pPacketList[k]->buf;
                    tmp_buf += sizeof(RTSPH264InterleaveData); 
                    nal_size = ((tmp_buf[0] << 8) | tmp_buf[1]); 
                    NvOsMemcpy(new_buf, tmp_buf, sizeof(start_sequence) + 2 + nal_size);
                    NvOsFree(pPacketList[k]->buf);
                    new_buf += (sizeof(start_sequence) + 2 + nal_size);
                }
                interleaveData = (RTSPH264InterleaveData *)(pPacketList[k-1]->buf);
                pH264MetaData->pDon = interleaveData->nalDon;
                NvOsFree(pPacketList);
                packet->size = k * (sizeof(start_sequence) + 2) + nal_size;
                packet->buf  = (unsigned char *)new_buf;
            }
        }
        else
        {
            if(vclNalCount >= 1)
            {
                status = PeekListElement(pH264MetaData->oH264PacketQueue, &deinPacket, 0);
                if(status != NvSuccess)
                {
                    retval = -1;
                    return retval;
                }
                interleaveData = (RTSPH264InterleaveData*)(deinPacket.buf);
                nDon = interleaveData->nalDon; 
                dDon = CalculateDonDistance(stream, nDon);
                nElement = 0;
                for(i = 1; i < vclNalCount; i++)
                {
                    status = PeekListElement(pH264MetaData->oH264PacketQueue, &deinPacket, i);
                    if(status != NvSuccess)
                        break;
                    interleaveData = (RTSPH264InterleaveData*)(deinPacket.buf);
                    nDon = interleaveData->nalDon; 
                    tempdDon = CalculateDonDistance(stream, nDon);
                    if(tempdDon < dDon)
                    {
                        dDon     = tempdDon;
                        nElement = i;
                    }
                }
                status = RemoveListElement(pH264MetaData->oH264PacketQueue, &deinPacket, nElement);
                if(status != NvSuccess)
                {
                    retval = -1;
                    return retval;
                }
                tmp_buf = (char *)deinPacket.buf;
                tmp_buf += sizeof(RTSPH264InterleaveData); /* skip DON related fileds */
                nal_size = ((tmp_buf[0] << 8) | tmp_buf[1]); 

                new_buf = NvOsAlloc(sizeof(start_sequence) + 2 + nal_size);
                if(!new_buf)
                {
                    retval = -1;
                    return retval;
                }
                pH264MetaData->pDon = interleaveData->nalDon;
                NvOsMemcpy(new_buf, tmp_buf, sizeof(start_sequence) + 2 + nal_size);
                NvOsFree(deinPacket.buf);

                packet->size = sizeof(start_sequence) + 2 + nal_size;
                packet->buf  = (unsigned char*)new_buf;
            }
        }
    }
    else
        retval = -1;

    return retval;
}

/* Single NAL unit packet */
static int ProcessH264PacketSNALU(char *buf, 
                                  int size, 
                                  RTPPacket *packet, 
                                  RTPStream *stream, 
                                  unsigned char packet_type)
{
    int retval                     = 0;
    NvU32 nal_size                 = 0;
    char *new_buf                  = NULL;
    char *tmp_buf                  = NULL;
    unsigned char *pSize           = NULL;

    nal_size = size;
    pSize    = (unsigned char *)&nal_size;
    new_buf  = NvOsAlloc(size + sizeof(nal_size));
    if(!new_buf)
    {
        retval = -1;
        return retval;
    }
    tmp_buf = new_buf;
    new_buf[0] = pSize[3];
    new_buf += 1;
    new_buf[0] = pSize[2];
    new_buf += 1;
    new_buf[0] = pSize[1];
    new_buf += 1;
    new_buf[0] = pSize[0];
    new_buf += 1;
    NvOsMemcpy(new_buf,  buf, size);
    packet->size = size +  sizeof(nal_size) ;
    packet->buf = (unsigned char *)tmp_buf;

    return retval;
}

/* Single-time aggregation packet */
static int ProcessH264PacketSTAPA(char *buf, 
                                  int size, 
                                  RTPPacket *packet, 
                                  RTPStream *stream, 
                                  unsigned char packet_type)
{
    int retval                     = 0;
    int src_len                    = 0;
    int nal_size                 = 0;
    char *new_buf                  = NULL;
    char *dst                      = NULL;
    const char *src                = NULL;
    int nal_units                    = 0;
    int i =0;
    unsigned char *pSize           = NULL;
    pSize    = (unsigned char *)&nal_size;

    buf ++;     /* Skip NAL unit */
    size --;
    src     = buf;
    src_len = size;

    /* Find total NALs */
    do
    {
        nal_size = ((unsigned char)src[0] << 8) | ((unsigned char)src[1]);
        src     += 2;
        src_len -= 2;
        src     += nal_size;
        src_len -= nal_size;
        nal_units ++;
    } while (src_len > 2);

    new_buf = NvOsAlloc(size + nal_units*2);
    if(!new_buf)
    {
        retval = -1;
        return retval;
    }
    dst     = new_buf;
    src     = buf;
    src_len = size;

    for(i =0; i<nal_units; i++)
    {
        nal_size = ((unsigned char)src[0] << 8) | ((unsigned char)src[1]);
        if (nal_size <= src_len)
        {
            dst[0] = pSize[3];
            dst += 1;
            dst[0] = pSize[2];
            dst += 1;
            dst[0] =pSize[1];
            dst += 1;
            dst[0] =pSize[0];
            dst += 1;
            NvOsMemcpy(dst, src+2, nal_size);
            dst += nal_size;
        }
        src     += (nal_size+2);
        src_len -= (nal_size+2);

    }
    packet->size      =  size + nal_units*2;
    packet->buf       = (unsigned char*)new_buf;

    return retval;
}

/* Single-time aggregation packet STAP-B */
static int ProcessH264PacketSTAPB(int M, 
                                  int seq, 
                                  NvU32 timestamp, 
                                  char *buf, 
                                  int size, 
                                  RTPPacket *packet, 
                                  RTPStream *stream,
                                  unsigned char packet_type)
{
    int retval                     = 0;
    int total_length               = 0;
    int src_len                    = 0;
    NvU16 nal_size                 = 0; 
    NvU16 nal_don                  = 0; 
    char *new_buf                  = NULL;
    char *tmp_buf                  = NULL;
    unsigned char start_sequence[] = {0, 0};
    const char *src                = NULL;
    RTSPH264InterleaveData interleaveData;
    RTPPacket deinPacket;
    RTSPH264MetaData* pH264MetaData = (RTSPH264MetaData*)stream->codecContext;

    NvOsMemset(&interleaveData, 0, sizeof(RTSPH264InterleaveData));
    buf ++;         /* Skip NAL unit */
    size --;
    nal_don = ((unsigned char)buf[0] << 8) | ((unsigned char)buf[1]); 
    buf  += 2;     /* Skip DON */
    size -= 2;
    src     = buf;
    src_len = size;
    do
    {
        NvOsMemset(&deinPacket, 0, sizeof(RTPPacket));
        nal_size = ((unsigned char)src[0] << 8) | ((unsigned char)src[1]); 
        src     += 2;
        src_len -= 2;
        if (nal_size <= src_len) 
            total_length += sizeof(start_sequence) + nal_size;
        total_length += 2;
        src     += nal_size;
        src_len -= nal_size;

        new_buf = NvOsAlloc(sizeof(start_sequence) + sizeof(RTSPH264InterleaveData) + 2 + nal_size);
        if(!new_buf)
        {
            retval = -1;
            return retval;
        }
        tmp_buf    = new_buf;

        /* Copy DON */
        interleaveData.nalDon = nal_don;
        NvOsMemcpy(tmp_buf, &interleaveData, sizeof(RTSPH264InterleaveData));
        tmp_buf += sizeof(RTSPH264InterleaveData);

        /* Copy start sequence */
        NvOsMemcpy(tmp_buf, start_sequence, sizeof(start_sequence));
        tmp_buf += sizeof(start_sequence);

        /*Copy NAL size and NAL data */
        NvOsMemcpy(tmp_buf, buf, 2 + nal_size);

        deinPacket.buf       = (unsigned char*)new_buf;
        deinPacket.timestamp = GetTimestampDiff(stream, timestamp) *
                                10000000 / stream->nClockRate;
        deinPacket.seqnum    = seq;
        deinPacket.flags     = (M) ? PACKET_LAST : 0;

        /* enqueue NAL to reorder list */
        if (NvSuccess != InsertInOrder(pH264MetaData->oH264PacketQueue, &deinPacket))
        {
            if (deinPacket.buf)
            {
                NvOsFree(deinPacket.buf);
                retval = -1;
                return retval;
            }
        }

        buf +=  (2 + nal_size); /* Point to next NAL */
        nal_don ++;             /* increment NAL DON */

    } while (src_len > 2);      

    retval = GetPacketFromInterleaveList(timestamp, buf, size, packet, stream);

    return retval;
}

static int ProcessH264PacketMTAP(int M, 
                                 int seq, 
                                 NvU32 timestamp, 
                                 char *buf, 
                                 int size, 
                                 RTPPacket *packet, 
                                 RTPStream *stream,
                                 unsigned char packet_type)
{
    int retval                     = 0;
    int total_length               = 0;
    int src_len                    = 0;
    NvU16 nal_size                 = 0; 
    NvU16 nal_don                  = 0; 
    NvU16 nal_don_base             = 0; 
    NvU16 nal_don_dif              = 0; 
    NvU32 nal_timeStampOffset      = 0;
    NvU32 nal_timestamp            = 0;
    char *new_buf                  = NULL;
    char *tmp_buf                  = NULL;
    unsigned char start_sequence[] = {0, 0};
    const char *src                = NULL;
    RTPPacket deinPacket;
    RTSPH264InterleaveData interleaveData;
    RTSPH264MetaData* pH264MetaData = (RTSPH264MetaData*)stream->codecContext;

    NvOsMemset(&interleaveData, 0, sizeof(RTSPH264InterleaveData));
    buf ++;         /* Skip NAL unit */
    size --;
    nal_don_base = ((unsigned char)buf[0] << 8) | ((unsigned char)buf[1]); 
    buf  += 2;     /* Skip DON base */
    size -= 2;
    src     = buf;
    src_len = size;
    do
    {
        NvOsMemset(&deinPacket, 0, sizeof(RTPPacket));
        nal_size = ((unsigned char)src[0] << 8) | ((unsigned char)src[1]); 
        src     += 2;
        src_len -= 2;
        if (nal_size <= src_len) 
            total_length += sizeof(start_sequence) + nal_size;
        total_length += 2;
        src     += nal_size;
        src_len -= nal_size;
        nal_don_dif = (unsigned char)src[0]; 
        nal_don     = (nal_don_base + nal_don_dif) % 65536;
        buf  ++;                    /* Skip DON  diff */
        size --;
        if(packet_type == 26)       /*MTAP16 */
        {
            nal_timeStampOffset = ((unsigned char)src[0] << 8) | ((unsigned char)src[1]); 
            src     += 2;           /* Skip TS offset */
            src_len -= 2;
        }
        else if (packet_type == 27) /*MTAP24 */
        {
            nal_timeStampOffset = (((unsigned char)src[0] << 16) | ((unsigned char)src[1] << 8)  | ((unsigned char)src[2])); 
            src     += 3;           /* Skip TS offset */
            src_len -= 3;
        }
        nal_timestamp = timestamp + nal_timeStampOffset;
        new_buf = NvOsAlloc(sizeof(start_sequence) + sizeof(RTSPH264InterleaveData) + 2 + nal_size);
        if(!new_buf)
        {
            retval = -1;
            return retval;
        }
        tmp_buf    = new_buf;

        /* Copy DON */
        interleaveData.nalDon = nal_don;
        NvOsMemcpy(tmp_buf, &interleaveData, sizeof(RTSPH264InterleaveData));
        tmp_buf += sizeof(RTSPH264InterleaveData);

        /* Copy start sequence */
        NvOsMemcpy(tmp_buf, start_sequence, sizeof(start_sequence));
        tmp_buf += sizeof(start_sequence);

        /*Copy NAL size and NAL data */
        NvOsMemcpy(tmp_buf, buf, 2 + nal_size);

        deinPacket.buf       = (unsigned char*)new_buf;
        deinPacket.timestamp = GetTimestampDiff(stream, nal_timestamp) *
                                10000000 / stream->nClockRate;
        deinPacket.seqnum    = seq;
        deinPacket.flags     = (M) ? PACKET_LAST : 0;

        /* enqueue NAL to reorder list */
        if (NvSuccess != InsertInOrder(pH264MetaData->oH264PacketQueue, &deinPacket))
        {
            if (deinPacket.buf)
            {
                NvOsFree(deinPacket.buf);
                retval = -1;
                return retval;
            }
        }

        buf +=  (2 + 2 + 1 + nal_size); /* Point to next NAL */
        nal_don ++;                 /* increment NAL DON */

    } while (src_len > 2);      

    retval = GetPacketFromInterleaveList(timestamp, buf, size, packet, stream);

    return retval;

}

static int ProcessH264PacketFU(int M, 
                               int seq, 
                               NvU32 timestamp, 
                               char *buf, 
                               int size, 
                               RTPPacket *packet, 
                               RTPStream *stream,
                               unsigned char packet_type)
{
    int retval                     = 0;
    NvU16 nal_don                  = 0; 
    int nal_size                 = 0;
    int nReconPacketLen            = 0;
    char *new_buf                  = NULL;
    char *tmp_buf                  = NULL;
    unsigned char packet_type_byte = buf[0];
    unsigned char *pSize;
    unsigned char fu_indicator, fu_header, start_bit, endt_bit, fu_nal_type, recon_nal;
    static int last_seq = 0;

    RTPPacket deinPacket;
    RTSPH264InterleaveData interleaveData;
    RTSPH264MetaData* pH264MetaData = (RTSPH264MetaData*)stream->codecContext;

    buf++;
    size--;                  
    fu_indicator = packet_type_byte;
    fu_header    = *buf;   
    start_bit    = fu_header >> 7;
    endt_bit     = (fu_header & 0x40) >> 6;
    fu_nal_type  = (fu_header & 0x1f);
    recon_nal    = fu_indicator & (0xe0);
    pSize      = (unsigned char *)&nal_size;
    recon_nal |= fu_nal_type;
    buf++;
    size--;

    if(packet_type == 29)
    {
        buf  += 2;
        size -= 2;
    }

    if(start_bit) 
    {
        last_seq = seq;
        if(stream->pReconPacket)
        {
            NvOsFree(stream->pReconPacket);
            ResetReconPacket(stream);
        }
        nReconPacketLen = 64*1024; /* start with 64K buffer */
        nal_size = size + sizeof(recon_nal);
        if(nal_size > nReconPacketLen)
            nReconPacketLen += nal_size;

        new_buf = NvOsAlloc(nReconPacketLen);
        if(!new_buf)
        {
             if(stream->pReconPacket)
                NvOsFree(stream->pReconPacket);
            ResetReconPacket(stream);
            retval = -1;
            return retval;
        }
        else
        {
            tmp_buf    = new_buf;
            new_buf[0] = recon_nal;
            new_buf   += sizeof(recon_nal);
            NvOsMemcpy(new_buf, buf, size);
            SetReconPacket(stream, tmp_buf, nReconPacketLen, nal_size);
        }
        retval = -1;
        return retval;
    } 
    else if((!start_bit)&& (!endt_bit))
    {
         nal_size = stream->nReconDataLen;
         if((!nal_size) || ((last_seq  + 1) != seq))
        {
            if(stream->pReconPacket)
                NvOsFree(stream->pReconPacket);
            ResetReconPacket(stream);
            retval = -1;
            return retval;
        }
        last_seq  = seq;
        nal_size  += size;
        if(nal_size > stream->nReconPacketLen)
        {
            new_buf = NvOsAlloc(nal_size);
            if(!new_buf)
            {
                if(stream->pReconPacket)
                    NvOsFree(stream->pReconPacket);
                ResetReconPacket(stream);
                retval = -1;
                return retval;
            }
            else
            {
                tmp_buf = new_buf;
                NvOsMemcpy(new_buf, stream->pReconPacket, stream->nReconDataLen);
                new_buf += stream->nReconDataLen;
                NvOsMemcpy(new_buf, buf, size);
                if(stream->pReconPacket)
                    NvOsFree(stream->pReconPacket);
                SetReconPacket(stream, tmp_buf, nal_size, nal_size);
            }
        }
        else
        {
            NvOsMemcpy(stream->pReconPacket + stream->nReconDataLen, buf, size);
            stream->nReconDataLen = nal_size;
        }
        retval = -1;
        return retval;
    }
    else if(endt_bit)
    {
        nal_size = stream->nReconDataLen;
        if((!nal_size) || ((last_seq  + 1) != seq))
        {
             if(stream->pReconPacket)
                 NvOsFree(stream->pReconPacket);
            ResetReconPacket(stream);
            retval = -1;
            return retval;
        }
        last_seq  = seq;
        nal_size += size ;
        if(packet_type == 28)
            new_buf = NvOsAlloc(4 + nal_size);
        else if(packet_type == 29)
            new_buf = NvOsAlloc(sizeof(RTSPH264InterleaveData) + 4 + nal_size);
        if(!new_buf)
        {
            if(stream->pReconPacket)
                NvOsFree(stream->pReconPacket);
            ResetReconPacket(stream);
            retval = -1;
            return retval;
        }
        else
        {
            tmp_buf = new_buf;
            if(packet_type == 29)
            {
                /* Copy DON */
                interleaveData.nalDon = nal_don;
                NvOsMemcpy(tmp_buf, &interleaveData, sizeof(RTSPH264InterleaveData));
                tmp_buf += sizeof(RTSPH264InterleaveData);
            }

            tmp_buf[0] = pSize[3];
            tmp_buf += 1;
            tmp_buf[0] = pSize[2];
            tmp_buf += 1;
            tmp_buf[0] = pSize[1];
            tmp_buf += 1;
            tmp_buf[0] = pSize[0];
            tmp_buf += 1;
            NvOsMemcpy(tmp_buf, stream->pReconPacket, stream->nReconDataLen);
            if(stream->pReconPacket)
                NvOsFree(stream->pReconPacket);
            tmp_buf += stream->nReconDataLen;
            ResetReconPacket(stream);
            NvOsMemcpy(tmp_buf, buf, size);

            if(packet_type == 28)
            {
                packet->size      = 4 + nal_size;
                packet->buf       = (unsigned char*)new_buf;
                packet->fuseqnum  = seq;
                return retval;
            }
            else if(packet_type == 29)
            {
                deinPacket.buf       = (unsigned char*)new_buf;
                deinPacket.timestamp = GetTimestampDiff(stream, timestamp) *
                                        10000000 / stream->nClockRate;

                deinPacket.seqnum    = seq;
                deinPacket.fuseqnum  = seq;
                deinPacket.flags     = (M) ? PACKET_LAST : 0;
                /* enqueue NAL to reorder list */
                if (NvSuccess != InsertInOrder(pH264MetaData->oH264PacketQueue, &deinPacket))
                {
                    if (deinPacket.buf)
                    {
                        NvOsFree(deinPacket.buf);
                        retval = -1;
                        return retval;
                    }
                }
            }
        }
    }
    else
    {
        retval = -1;
        return retval;
    }

    if(packet_type == 29)
        retval = GetPacketFromInterleaveList(timestamp, buf, size, packet, stream);

    return retval;
}


/* Process H.264 packet RFC 3984 */
int ProcessH264Packet(int M, 
                      int seq, 
                      NvU32 timestamp, 
                      char *buf, 
                      int size,
                      RTPPacket *packet, 
                      RTPStream *stream)
{
    unsigned char packet_type_byte = buf[0];
    unsigned char packet_type      = (packet_type_byte & 0x1f);
    int retval                     = 0;

    if(packet_type >= 1 && packet_type <= 23)
        packet_type = 1;

    switch (packet_type)
    {
        case 0:     /* Un-defined H264Packet Type */                   
            retval = -1;
            NvOsDebugPrintf(" **** Un-defined H264Packet Type: %d ****\n", packet_type);
            break;
    
        case 1:     /* Single NALU (one packet, one NAL) */
            retval = ProcessH264PacketSNALU(buf, size, packet, stream, packet_type);
            break;
        
        case 24:    /* STAP-A (one packet, multiple nals) */
            retval = ProcessH264PacketSTAPA(buf, size, packet, stream, packet_type);
            break;

        case 25:    /* STAP-B */
            retval = ProcessH264PacketSTAPB(M, seq, timestamp, buf, size, packet, stream, packet_type);
            NvOsDebugPrintf(" **** Un-Tested H264Packet Type: %d ****\n", packet_type);
            break;

        case 26:    /* MTAP-16 */
        case 27:    /* MTAP-24 */
            retval =  ProcessH264PacketMTAP(M, seq, timestamp, buf, size, packet, stream, packet_type);
            NvOsDebugPrintf(" **** Un-Tested H264Packet Type: %d ****\n", packet_type);
            break;

        case 28:    /* FU-A - fragmented nal */
        case 29:    /* FU-B */
            retval = ProcessH264PacketFU(M, seq, timestamp, buf, size, packet, stream, packet_type);
            break;
    
        case 30:    /* Un-defined H264Packet Type */                                    
        case 31:    /* Un-defined H264Packet Type */                                   
        default:
            retval = -1;
            NvOsDebugPrintf(" **** Un-defined H264Packet Type: %d ****\n", packet_type);
            break;
    }

    if(retval == 0)
    {
        packet->timestamp = GetTimestampDiff(stream, timestamp) *
                             10000000 / stream->nClockRate;
        packet->seqnum    = seq;
        packet->flags     = (M) ? PACKET_LAST : 0;
    }

    return retval;
}

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

/*
* As per ISO/IEC  14496:3-2001(E) - Frame Length of CELP Layer 0
*/
static NvU32 CELPFrameLengthTable[64][7]={
    {154,156,23,8,2,156,134},
    {170,172,23,8,2,172,150},
    {186,188,23,8,2,188,166},
    {147,149,23,8,2,149,127},
    {156,158,23,8,2,158,136},
    {165,167,23,8,2,167,145},
    {114,116,23,8,2,116,94},
    {120,122,23,8,2,122,100},
    {126,128,23,8,2,128,106},
    {132,134,23,8,2,134,112},
    {138,140,23,8,2,140,118},
    {142,144,23,8,2,144,122},
    {146,148,23,8,2,148,126},
    {154,156,23,8,2,156,134},
    {166,168,23,8,2,168,146},
    {174,176,23,8,2,176,154},
    {182,184,23,8,2,184,162},
    {190,192,23,8,2,192,170},
    {198,200,23,8,2,200,178},
    {206,208,23,8,2,208,186},
    {210,212,23,8,2,212,190},
    {214,216,23,8,2,216,194},
    {110,112,23,8,2,112,90},
    {114,116,23,8,2,116,94},
    {118,120,23,8,2,120,98},
    {120,122,23,8,2,122,100},
    {122,124,23,8,2,124,102},
    {186,188,23,8,2,188,166},
    {218,220,40,8,2,220,174},
    {230,232,40,8,2,232,186},
    {242,244,40,8,2,244,198},
    {254,256,40,8,2,256,210},
    {266,268,40,8,2,268,222},
    {278,280,40,8,2,280,234},
    {286,288,40,8,2,288,242},
    {294,296,40,8,2,296,250},
    {318,320,40,8,2,320,276},
    {342,344,40,8,2,344,298},
    {358,360,40,8,2,360,314},
    {374,376,40,8,2,376,330},
    {390,392,40,8,2,392,346},
    {406,408,40,8,2,408,362},
    {422,424,40,8,2,424,378},
    {136,138,40,8,2,138,92},
    {142,144,40,8,2,144,98},
    {148,150,40,8,2,150,104},
    {154,156,40,8,2,156,110},
    {160,162,40,8,2,162,116},
    {166,168,40,8,2,168,122},
    {170,172,40,8,2,172,126},
    {174,176,40,8,2,176,130},
    {186,188,40,8,2,188,142},
    {198,200,40,8,2,200,154},
    {206,208,40,8,2,208,162},
    {214,216,40,8,2,216,170},
    {222,224,40,8,2,224,178},
    {230,232,40,8,2,232,186},
    {238,240,40,8,2,240,194},
    {216,218,40,8,2,218,172},
    {160,162,40,8,2,162,116},
    {280,282,40,8,2,282,238},
    {338,340,40,8,2,340,296},
    // 62-63 are reserved and should not be used
    {-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1}
};

/*
* As per ISO/IEC  14496:3-2001(E) - Frame Length of HVXC
*/
static NvU32 HVXCFrameLengthTable[4][4]={
    {40, 40, 40, 40},
    {80, 80, 80, 80},
    {40, 28, 2, 0},
    {80, 40, 25, 3}
};

static NvS32 simpleGetBits (simpleBitstreamPtr self, const NvU32 numberOfBits)
{
    if (self->validBits <= 16)
    {
        self->validBits += 16;
        self->dataWord <<= 16;
        self->dataWord |= (NvU32) *self->buffer++ << 8;
        self->dataWord |= (NvU32) *self->buffer++;
    }

    self->readBits += numberOfBits;
    self->validBits -= numberOfBits;

    return ((self->dataWord >> self->validBits) & ((1L << numberOfBits) - 1)) ;
}

static NvS64 latmGetValue(simpleBitstreamPtr bPtr)
{
    NvU8 bytesForValue = simpleGetBits(bPtr, 2);
    NvS64 value = 0;
    NvS32 i;
    for (i = 0; i <= bytesForValue; i++)
    {
        value <<= 8;
        value |= simpleGetBits(bPtr, 8);
    }

    return value;
}

static void AudioSpecificConfig (simpleBitstreamPtr bPtr, LatmContext *context)
{
    NvS32 dependsOnCoreCoder, extensionFlag, extensionFlag3;
    NvS32 extensionSamplingFreqIndex;
    context->objectType = simpleGetBits(bPtr, 5);

    context->samplingFreqIndex = simpleGetBits(bPtr, 4);

    if (context->samplingFreqIndex == 0xf)
    {
        context->samplingFreq = simpleGetBits(bPtr, 24);
    }

    context->channelConfiguration = simpleGetBits(bPtr, 4);

    /*
     * As per ISO/IEC 14496-3:2001/Amd.1:2003(E) Table 1.8
     */
    if (context->objectType == 5)
    {
        context->sbrPresentFlag = 1;
        extensionSamplingFreqIndex = simpleGetBits(bPtr, 4 ); //extensionSamplingFreqIndex;
        if (extensionSamplingFreqIndex == 0xf)
        {
            simpleGetBits(bPtr, 24 ); //extensionSamplingFreq;
        }
        context->objectType = simpleGetBits(bPtr, 5);
    }

    if (context->objectType == 1 || context->objectType == 2 ||
        context->objectType == 3 || context->objectType == 4 ||
        context->objectType == 6 || context->objectType == 7 ||
        context->objectType == 17 || context->objectType == 19 ||
        context->objectType == 20 || context->objectType == 21 ||
        context->objectType == 22 || context->objectType == 23)
    {
        // GASpecificConfig();
        simpleGetBits(bPtr, 1); //frameLengthFlag;
        dependsOnCoreCoder = simpleGetBits(bPtr, 24);
        if (dependsOnCoreCoder)
        {
            simpleGetBits(bPtr, 14); //coreCoderDelay;
        }

        extensionFlag = simpleGetBits(bPtr, 1);

        if (!context->channelConfiguration)
        {
            //program_config_element (); TODO
        }

        if (context->objectType == 6 || context->objectType == 20)
        {
            simpleGetBits(bPtr, 3); //layerNr; 3
        }

        if (extensionFlag)
        {
            if (context->objectType == 22)
            {
                simpleGetBits(bPtr, 5); //numOfSubFrame;
                simpleGetBits(bPtr, 11); //layer_length;
            }

            if (context->objectType == 17 || context->objectType == 19 ||
                context->objectType == 20 || context->objectType == 23 )
            {
                simpleGetBits(bPtr, 1);//aacSectionDataResilienceFlag;
                simpleGetBits(bPtr, 1);//aacScalefactorDataResilienceFlag;
                simpleGetBits(bPtr, 1);//aacSpectralDataResilienceFlag;
            }

            extensionFlag3 = simpleGetBits(bPtr, 1);
            if (extensionFlag3)
            {
                /* tbd in version 3 */
            }
        }
    }

// Ignore the rest of object types
    /*    if ( context->objectType == 8 )
            CelpSpecificConfig();
        if ( context->objectType == 9 )
            HvxcSpecificConfig();
        if ( context->objectType == 12 )
             TTSSpecificConfig();
        if ( context->objectType == 13 || context->objectType == 14 ||
              context->objectType == 15 || context->objectType==16)
        {
            StructuredAudioSpecificConfig();
        }

        if ( context->objectType == 24)
             ErrorResilientCelpSpecificConfig();
        if ( context->objectType == 25)
             ErrorResilientHvxcSpecificConfig();
        if ( context->objectType == 26 || context->objectType == 27)
             ParametricSpecificConfig();
        if ( context->objectType == 17 || context->objectType == 19 ||
                context->objectType == 20 || context->objectType == 21 ||
                context->objectType == 22 || context->objectType == 23 ||
                context->objectType == 24 || context->objectType == 25 ||
                context->objectType == 26 || context->objectType == 27 )
        {
            epConfig;
            if ( epConfig == 2 || epConfig == 3 )
            {
                  ErrorProtectionSpecificConfig();
            }
            if ( epConfig == 3 )
            {
                directMapping;
                if ( ! directMapping )
                {
                    // tbd
                }
            }
       } */
}


static NvS32 PayloadLengthInfo(simpleBitstreamPtr bPtr, LatmContext * context)
{
    NvS32 tmp, MuxSlotLengthCoded, MuxSlotLengthBytes = 0;
    if (context->allStreamsSameTimeFraming)
    {
        if (context->frameLengthType == 0)
        {
            do
            { /* always one complete access unit */
                tmp = simpleGetBits(bPtr, 8);
                MuxSlotLengthBytes += tmp;
            } while (tmp == 255);
        }
        else if (context->frameLengthType == 3 || // CELP Frame Lengths
                    context->frameLengthType == 4 ||
                    context->frameLengthType == 5)
        {
            MuxSlotLengthCoded = simpleGetBits(bPtr, 2);
            if (context->frameLengthType == 4)
            {
                MuxSlotLengthBytes =
                    CELPFrameLengthTable[
                        context->CELPFrameLengthTableIndex][0];
            }
            else if (context->frameLengthType == 5)
            {
                MuxSlotLengthBytes =
                    CELPFrameLengthTable[
                        context->CELPFrameLengthTableIndex]
                        [1+MuxSlotLengthCoded];
            }
            else
            {
                MuxSlotLengthBytes =
                    CELPFrameLengthTable[
                        context->CELPFrameLengthTableIndex]
                        [5 + (MuxSlotLengthCoded & 1)];
            }
        }
        else if (context->frameLengthType == 6 || // HVXC Frame Lengths
            context->frameLengthType == 7)
        {
                MuxSlotLengthCoded = simpleGetBits(bPtr, 2);
                MuxSlotLengthBytes =
                    HVXCFrameLengthTable[
                        (context->frameLengthType-6)*2 +
                            context->HVXCFrameLengthTableIndex]
                        [MuxSlotLengthCoded];
        }
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,
            "PayloadLengthInfo: FrameLengthType=%d MuxSlotLengthBytes=%d",
            context->frameLengthType, MuxSlotLengthBytes));
    }

   // Not used as per RFC 3016

   /*
    else
    {
        numChunk = simpleGetBits(bPtr, 4);
        for (chunkCnt=0; chunkCnt <= context->numChunk; chunkCnt++)
        {
            streamIndx = simpleGetBits(bPtr, 2);
            prog = progCIndx[chunkCnt] = progSIndx[streamIndx];
            lay = layCIndx[chunkCnt] = laySIndx [streamIndx];
            if( context->frameLengthType == 0 )
            {
                do { // not necessarily a complete access unit
                 tmp = simpleGetBits(bPtr, 8);
                MuxSlotLengthBytes += tmp;
                } while (tmp == 255);
                simpleGetBits(bPtr, 1);//AuEndFlag[streamID[prog][lay]]; 1 bslbf
            }
            else
            {
                if ( context->frameLengthType == 5 ||
                    context->frameLengthType == 7 ||
                    context->frameLengthType == 3 )
                {
                    MuxSlotLengthCoded = simpleGetBits(bPtr, 2);
                }
            }

        }
    }*/

    return MuxSlotLengthBytes;
}

void StreamMuxConfig(simpleBitstreamPtr bPtr, LatmContext * context)
{
    NvS32 otherDataLenBits, otherDataLenEsc, otherDataLenTmp, crcCheckPresent;
    NvS8 useSameConfig;

    context->audioMuxVersion = simpleGetBits(bPtr, 1);
    if (context->audioMuxVersion == 1)
    {
        context->audioMuxVersionA = simpleGetBits(bPtr, 1);
    }

    if (context->audioMuxVersionA == 0)
    {
        if (context->audioMuxVersion == 1)
        {
            context->taraFullness = latmGetValue(bPtr);
        }

        context->streamCnt = 0;
        context->allStreamsSameTimeFraming = 1;
        // simplified revision of StreamMuxConfig
        // From RFC 3016 "Multiplexing multiple objects (programs),
        // - Multiplexing scalable layers : These two features MUST NOT
        //   be used in applications

        simpleGetBits(bPtr, 1);  // allStreamSameTimeFraming = 1
        context->numSubFrames = simpleGetBits(bPtr, 6);
        //NvOsDebugPrintf("numSubFrames = %d\n", context->numSubFrames);

        simpleGetBits(bPtr, 4);  // numPrograms = 0
        simpleGetBits(bPtr, 3); // numLayer = 0

        if (context->audioMuxVersion == 0)
        {
            AudioSpecificConfig(bPtr, context);
        }
        else
        {
            useSameConfig = simpleGetBits(bPtr, 3);
            if (!useSameConfig)
                AudioSpecificConfig(bPtr, context);
        }

        context->frameLengthType =0;
        context->frameLengthType = simpleGetBits(bPtr, 3);// frameLengthType[streamID[prog][ lay]];
        if (context->frameLengthType == 0)
        {
            simpleGetBits(bPtr, 8); //latmBufferFullness[streamID[prog][ lay]];
            if (!context->allStreamsSameTimeFraming)
            {
                // FIXME: how is this ever true?
                if ((context->objectType == 6 || context->objectType == 20) &&
                    (context->objectType == 8 || context->objectType == 24))
                {
                    simpleGetBits(bPtr, 6); //coreFrameOffset;
                }
            }
        }
        else if (context->frameLengthType == 1)
        {
            simpleGetBits(bPtr, 9); //frameLength[streamID[prog][lay]];
        }
        else if (context->frameLengthType == 3 ||
                 context->frameLengthType == 4 ||
                 context->frameLengthType == 5)
        {
                context->CELPFrameLengthTableIndex = simpleGetBits(bPtr, 6); //CELPframeLengthTableIndex[streamID[prog][lay]];
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,
                    "StreamMuxConfig: CELPFrameLengthTableIndex=%d",
                    context->CELPFrameLengthTableIndex));
        }
        else if (context->frameLengthType  == 6 ||
                 context->frameLengthType  == 7)
        {
            context->HVXCFrameLengthTableIndex = simpleGetBits(bPtr, 1); //HVXCframeLengthTableIndex[streamID[prog][ lay]];
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,
                "StreamMuxConfig: HVXCFrameLengthTableIndex=%d",
                context->HVXCFrameLengthTableIndex));
        }

        context->otherDataPresent = simpleGetBits(bPtr, 1);
        if (context->otherDataPresent)
        {
            otherDataLenBits = 0; /* helper variable 32bit */
            do
            {
                otherDataLenBits = otherDataLenBits << 8;
                otherDataLenEsc = simpleGetBits(bPtr, 1);
                otherDataLenTmp = simpleGetBits(bPtr, 8);
                otherDataLenBits = otherDataLenBits + otherDataLenTmp;
            } while (otherDataLenEsc);
        }

        crcCheckPresent = simpleGetBits(bPtr, 1);
        if (crcCheckPresent)
             simpleGetBits(bPtr, 8); //crcCheckSum;
    }
    else
    {
        /* tbd */
    }
}

int ProcessAACLATMPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                         RTPPacket *packet, RTPStream *stream)
{
    NvS8 *hdrstart, *payload = NULL, *ptrPayloadData;
    NvS8 useSameStreamMux;
    NvS32 i, j, payloadSize = 0;
    NvS32 FrameHdrSize = 0, RemainingPacketSize = 0;
    simpleBitstream bs;
    char *tempBuf = NULL;
    int retval  = 0;
    LatmContext *context = (LatmContext *)stream->codecContext;
    NvU64 subframetimestamp = 0;
    if (M)
    {
        // Check for prev packet, last block of AudioMuxElement spread across
        // multiple rtp packets
        if (!context->prevMarkerBit)
        {
            //Check the allocated buffer size  and re-allocate if necessary
            if ((size + stream->nReconDataLen) > stream->nReconPacketLen)
            {
                tempBuf = NvOsAlloc(stream->nReconPacketLen + size);
                if (!tempBuf)
                {
                    retval = -1;
                    goto cleanup;
                }
                NvOsMemcpy(tempBuf, stream->pReconPacket,
                           stream->nReconDataLen);
                stream->nReconPacketLen += size;
                NvOsFree(stream->pReconPacket);
                stream->pReconPacket = tempBuf;
            }
            else
            {
                tempBuf = stream->pReconPacket;
            }

            tempBuf += stream->nReconDataLen;
            NvOsMemcpy(tempBuf, buf, size);
            stream->nReconDataLen += size;
        }
        // else AudioMuxElement fits in a single rtp packet, no changes required
    }
    else
    {
        // NvOsDebugPrintf("\n\nFragmented AudioMuxElement\n\n");
        if (stream->nReconPacketLen == 0) // first block of AudioMuxElement
        {
            // start with 10 times the size
            stream->nReconPacketLen = 10 * size;
            tempBuf = NvOsAlloc(stream->nReconPacketLen);
            if (!tempBuf)
            {
                retval = -1;
                goto cleanup;
            }
            stream->pReconPacket = tempBuf;
        }
        else
        {
            // Check the allocated buffer size and re-allocate if necessary
            if ((size + stream->nReconDataLen) > stream->nReconPacketLen)
            {
                tempBuf = NvOsAlloc(stream->nReconPacketLen + 5 * size);
                if (!tempBuf)
                {
                    retval = -1;
                    goto cleanup;
                }

                NvOsMemcpy(tempBuf, stream->pReconPacket,
                           stream->nReconDataLen);
                stream->nReconPacketLen += 5 * size;
                NvOsFree(stream->pReconPacket);
                stream->pReconPacket = tempBuf;
                tempBuf += stream->nReconDataLen;
            }
        }

        if (tempBuf)
        {
            NvOsMemcpy(tempBuf, buf, size);
            stream->nReconDataLen += size;
        }

        retval = -1; // Don't enqueue
        return retval;
    }

    hdrstart = (NvS8 *)buf;

    if (M && stream->pReconPacket) // Change if reconstructed packet
        hdrstart = (NvS8 *)stream->pReconPacket;

    ptrPayloadData = hdrstart;
    RemainingPacketSize = size;

    // ISO/IEC 14496-3  AudioMuxElement

    if (stream->cpresent)
    {
        useSameStreamMux = simpleGetBits(&bs, 1);
        if (!useSameStreamMux)
            StreamMuxConfig(&bs, context);
    }

    if (context->audioMuxVersion == 0)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,
            "++ProcessAACLATM Size=%d",size));

        packet->timestamp = GetTimestampDiff(stream, timestamp) *
                            10000000 / stream->nClockRate;
        packet->seqnum = seq;
        packet->flags = (M) ? PACKET_LAST : 0;
        subframetimestamp = packet->timestamp;
        for (i = 0; i <= context->numSubFrames; i++)
        {
            bs.buffer = (NvU8 *)ptrPayloadData;
            bs.readBits = bs.dataWord = bs.validBits = 0;

            payloadSize = PayloadLengthInfo(&bs, context);
            FrameHdrSize = bs.readBits / 8;
            // truncate the payloadsize to the max packet size
            if (FrameHdrSize + payloadSize > RemainingPacketSize)
            {
                payloadSize = RemainingPacketSize - FrameHdrSize;
            }

            payload = NvOsAlloc(payloadSize);
            if (!payload)
            {
               retval = -1;
               goto cleanup;
            }

            // Sometimes Payload is not byte aligned
            if ((bs.readBits % 8) == 0)
            {
                NvOsMemcpy(payload, ptrPayloadData + FrameHdrSize, payloadSize);
            }
            else
            {
                for (j=0; j<payloadSize; j++)
                {
                    payload[j] = simpleGetBits(&bs, 8);
                }
            }

            // Insert in Packet Queue
            packet->size = payloadSize;
            packet->buf = (NvU8 *)payload;

            ptrPayloadData += FrameHdrSize + payloadSize;
            RemainingPacketSize -= FrameHdrSize + payloadSize;

            if (RemainingPacketSize == 0)
            {
                packet->timestamp = subframetimestamp;
                break;
            }
            // Allow the RTP calling code to deliver the final packet
            else if (i < context->numSubFrames)
            {
                packet->serverts = packet->timestamp;
                packet->timestamp = subframetimestamp + stream->TSOffset;
                if (NvSuccess != InsertInOrder(stream->oPacketQueue, packet))
                {
                    if (packet->buf)
                        NvOsFree(packet->buf);
                    return -1;
                }
            }
            packet->timestamp = subframetimestamp;
        }

        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,
            "--ProcessAACLATM Size=%d",size-RemainingPacketSize));

        if (context->otherDataPresent)
        {
            for (i = 0; i < context->otherDataLenBits; i++)
            {
                simpleGetBits(&bs, 1);
            }
        }
    }
    else
    {
         /* tbd */
    }

cleanup:
    if ((retval!= 0) || M)
    {
        if (stream->pReconPacket)
        {
            NvOsFree(stream->pReconPacket);
            stream->pReconPacket    = NULL;
        }
        stream->nReconDataLen   = 0;
        stream->nReconPacketLen = 0;
    }

    context->prevMarkerBit = M;
    return retval;
}

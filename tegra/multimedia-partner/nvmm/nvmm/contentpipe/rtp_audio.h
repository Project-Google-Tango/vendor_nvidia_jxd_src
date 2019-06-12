/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef RTP_AUDIO_H_
#define RTP_AUDIO_H_

// codec specific processing
typedef struct simpleBitstream
{   
    const NvU8 *   buffer ;
    NvU32            readBits ;
    NvU32            dataWord ;
    NvU32            validBits ;

} simpleBitstream, *simpleBitstreamPtr ;

int ProcessAMRPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                     RTPPacket *packet, RTPStream *stream);

int ProcessAACPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                     RTPPacket *packet, RTPStream *stream);

int ProcessAACLATMPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                     RTPPacket *packet, RTPStream *stream);

void StreamMuxConfig(simpleBitstreamPtr bPtr, LatmContext * context);


#if 0
NvS32 ProcessMpeg4AVPacket( NvS32 M, NvS32 seq, NvU32 timestamp, 
           char *buf, NvS32 size,  RTPPacket *packet, RTPStream *stream);
#endif

#endif


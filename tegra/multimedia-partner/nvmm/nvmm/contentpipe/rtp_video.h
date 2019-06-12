/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef RTP_VIDEO_H_
#define RTP_VIDEO_H_

// codec/rfc specific FMTP processing
NvError ParseH264FMTPLine(RTPStream *stream, char *line);


// codec specific processing

int ProcessH263Packet(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream);

int ProcessMP4VPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream);

int ProcessASFPacket(int M, int seq, NvU32 timestamp, char *buf, int size,
                     RTPPacket *packet, RTPStream *stream);

int ProcessVC1Packet(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream);

int ProcessH264Packet(int M, int seq, NvU32 timestamp, char *buf, int size,
                      RTPPacket *packet, RTPStream *stream);

#endif


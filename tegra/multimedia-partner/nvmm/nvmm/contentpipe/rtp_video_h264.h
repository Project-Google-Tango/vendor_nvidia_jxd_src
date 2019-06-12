
/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef RTP_VIDEO_H264_H_
#define RTP_VIDEO_H264_H_

#include "rtp_video.h"

typedef struct RTSPH264InterleaveDataRec
{
    NvU32 nalDon;
    NvU32 absDon;
    NvU32 dDon;
} RTSPH264InterleaveData;

typedef struct RTSPH264MetaDataRec
{
    NvU32 profileLevelId;
    NvU32 maxMbps;
    NvU32 maxFs;
    NvU32 maxCpb;
    NvU32 maxDpb;
    NvU32 maxBr;
    NvU32 redundantPicCap;
    NvU32 parameterAdd;
    NvU32 packetizationMode;
    NvU32 spropInterleavingDepth;
    NvU32 spropDeintBufReq;
    NvU32 deintBufCap;
    NvU32 spropInitBufTime;
    NvU32 spropMaxDonDiff;
    NvU32 pDon;
    NvU32 maxRcmdNaluSize;
    void *oH264PacketQueue;
    NvBool bSpropMaxDonDiff;
} RTSPH264MetaData;

#endif


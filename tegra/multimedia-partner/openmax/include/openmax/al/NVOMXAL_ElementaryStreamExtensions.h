/* Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * This is the NVIDIA OpenMAX AL Elementary Stream extensions interface.
 *
 */

#ifndef NVOMXAL_ELEMENTARYSTREAMEXTENSIONS_H_
#define NVOMXAL_ELEMENTARYSTREAMEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#pragma pack(push, 4)

#ifdef __cplusplus
extern "C" {
#endif

#define NV_ES_FIXED_HEADER_SIZE         8
#define NV_ES_VARIABLE_HEADER_MAX_SIZE 16

typedef struct
{
    char mNvIdentifier[4];    // video: 'nesv', audio: 'nesa' or 'npcm'
    XAuint32 mHeaderBodySize;  // data size read to fill NvESHeaderBody
} NvESHeaderFixed;

typedef struct
{
    XAuint32 mFrameSize;
    XAuint64 mTimeStamp;
    XAuint32 nExtraSize;
} NvESHeaderBody;

typedef struct
{
    XAuint32 mFrameSize;
} NvESPCMHeaderBody;

// This should be prepended to the raw PCM data for each frame
// Bits: 31-24 Bits Per Sample (always 16, optional field)
//       23-20 Channels (1,2,4,6 or 8 etc..)
//       19-0  SampleRate
typedef struct
{
    XAuint32 mFormat;
} NvESPCMHeaderData;


#ifdef __cplusplus
} /* extern "C" */
#endif

#pragma pack(pop)

#endif /* NVOMXAL_ELEMENTARYSTREAMEXTENSIONS_H_ */


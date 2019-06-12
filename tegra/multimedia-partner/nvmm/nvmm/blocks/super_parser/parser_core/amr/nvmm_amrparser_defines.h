/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NV_AMRPARSER_DEFINES_H
#define INCLUDED_NV_AMRPARSER_DEFINES_H


#include "nvos.h"

typedef enum {
    NvStreamTypeAMRNB, 
    NvStreamTypeAMRWB
}NvAmrStreamType;

typedef struct 
{
    NvS32 bitRate;
    NvS32 samplingFreq;               //samplerate
    NvU64 total_time;
    NvS32 noOfChannels;               //channels
    NvS32 sampleSize;                  //wordlength
    NvS32 data_offset;    
    NvU32 Dtx;                            /** Dtx */
    NvAmrStreamType streamType;
}NvAmrTrackInfo;


typedef struct
{
    NvAmrTrackInfo *pTrackInfo;
}NvAmrStreamInfo;



#endif //INCLUDED_NV_AMRPARSER_DEFINES_H





























































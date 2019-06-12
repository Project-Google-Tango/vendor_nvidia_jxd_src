/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NV_AACPARSER_DEFINES_H
#define INCLUDED_NV_AACPARSER_DEFINES_H


#include "nvos.h"

typedef struct 
{
    NvS32 bitRate;
    NvS32 samplingFreq;               //samplerate
    NvS64 total_time;
    NvS32 noOfChannels;               //channels
    NvS32 sampleSize;                  //wordlength
    NvS32 data_offset;
    NvU32 objectType;
    NvU32 profile;
    NvU32 samplingFreqIndex;
    NvU32 channelConfiguration;
}NvAacTrackInfo;


typedef struct
{
    NvAacTrackInfo *pTrackInfo;
}NvAacStreamInfo;


#endif //INCLUDED_NV_AACPARSER_DEFINES_H





























































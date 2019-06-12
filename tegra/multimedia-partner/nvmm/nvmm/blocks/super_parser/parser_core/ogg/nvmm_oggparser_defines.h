
/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/
#ifndef INCLUDED_NV_OGGPARSER_DEFINES_H
#define INCLUDED_NV_OGGPARSER_DEFINES_H

#include "nvos.h"
#include "nvmm_ogg.h"

typedef struct 
{
    NvS32 bitRate;
    NvS32 samplingFreq;               //samplerate
    NvS32 total_time;
    NvS32 noOfChannels;               //channels
    NvS32 sampleSize;                  //wordlength
    NvS32 data_offset;
}NvOggTrackInfo;

 typedef struct
 {
    //void *OggParams;                       /*Ogg parser struct pointer*/
    OggVorbis_File *pOggParams;
 } NvOggParams;


typedef struct
{
    NvOggTrackInfo *pTrackInfo;
}NvOggStreamInfo;







































#endif //INCLUDED_NV_OGGPARSER_DEFINES_H















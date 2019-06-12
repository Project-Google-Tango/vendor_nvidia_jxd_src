/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NV_M2VPARSER_DEFINES_H
#define INCLUDED_NV_M2VPARSER_DEFINES_H


#include "nvos.h"

typedef struct 
{

    NvS64 total_time;
    NvS32 data_offset;
    NvU32 objectType;
    NvU32 profile;
    NvU32 Width; // specifies vertical size of pixel aspect ratio
    NvU32 Height;// specifies horizontal size of pixel aspect ratio
    NvU32 Fps; //frame rate
    NvU32 VideoBitRate; //video bit rtae
}NvM2vTrackInfo;


typedef struct
{
    NvM2vTrackInfo *pTrackInfo;
}NvM2vStreamInfo;

/**
* @brief          This structure contains the file parser related data.
*/
typedef struct 
{   
    NvS32           FileSize;
    NvU8            LastFrameFound;
    NvU8            bNoMoreData;      // Set to TRUE when we run out of data
    NvS32           TotalSize;
    NvU32           CurrentFrame;     // current frame about to fetch (within current track)
    NvU32           CurrentOffset;    // Most recently fetched offset
    NvU32           CurrentSize;      // Most recently fetched size (associated with above offset)
    NvU32           error;            // returned error value.
} NvM2vStream;
#endif 


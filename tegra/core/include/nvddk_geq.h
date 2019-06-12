
/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/ 

#ifndef INCLUDED_NVDDK_GEQ_H
#define INCLUDED_NVDDK_GEQ_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define NUM_GEQ_FILTERS                   5
#define EQ_NO_CHANNEL                     2

typedef struct NvMMMixerStream5BandEQProperty_
{
    NvU32   structSize;
    NvS32   dBGain[EQ_NO_CHANNEL][NUM_GEQ_FILTERS];
} NvMMMixerStream5BandEQProperty;

#if defined(__cplusplus)
}
#endif

#endif  //INCLUDED_NVDDK_GEQ_H

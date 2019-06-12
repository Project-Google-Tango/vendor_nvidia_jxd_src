/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief      Contains definition of structure required for ADTS header
 */

#ifndef INCLUDED_NVMM_ADTS_HEADER_H
#define INCLUDED_NVMM_ADTS_HEADER_H

#include "nvos.h"

/**
 * @brief          This structure contains the file parser related data.
 */
typedef struct 
{   
    NvS32           FileSize;
    NvS32           ListenerNumber;

    // MP3 stuff
    NvU8            LastFrameFound;
    NvU8            bNoMoreData;      // Set to TRUE when we run out of data
    NvS32           TotalSize;
    NvU32           CurrentFrame;     // current frame about to fetch (within current track)
    NvU32           CurrentOffset;    // Most recently fetched offset
    NvU32           CurrentSize;      // Most recently fetched size (associated with above offset)
    NvU32           error;            // returned error value.
} NvAacStream;

/**
 * @brief          struct NvADTSHeader
 * @remarks        This structure contains header information of the ADTS file
 */
typedef struct 
{
    // const doesn't change from one frame to the next
    NvS32 syncword;                    // 12 bits
    NvS32 id;                          // 1
    NvS32 layer;                       // 2
    NvS32 protection_abs;              // 1
    NvS32 profile;                     // 2
    NvS32 sampling_freq_idx;           // 4
    NvS32 private_bit;                 // 1
    NvS32 channel_config;              // 3
    NvS32 original_copy;               // 1
    NvS32 home;                        // 1
    // variable header part, these can change from frame to frame
    NvS32 copyright_id_bit;            // 1
    NvS32 copyright_id_start;          // 1
    NvS32 frame_length;                // 13
    NvS32 adts_buffer_fullness;        // 11
    NvS32 num_of_rdb;                  // 2   only 1 data buffer (i.e. 0) is supported in ISDB-T for each header
    // CRC Value
    NvS32 crc_check;
} NvADTSHeader;

enum{
    MAX_AAC_FRAME_SINGLE_CH_SIZE = 768,
    MAX_NUMBER_OF_CH = 6,
    MAX_ADTSAAC_BUFFER_SIZE = (MAX_AAC_FRAME_SINGLE_CH_SIZE * MAX_NUMBER_OF_CH*2),
    ADTSAAC_HEADER_SIZE = 9
};

#endif  // #ifndef INCLUDED_NVMM_ADTS_HEADER_H



























































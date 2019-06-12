/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV_MP4PARSER_DEFINES_H
#define INCLUDED_NV_MP4PARSER_DEFINES_H

#include "nvos.h"
#include "nvmm_common.h"
#include "nvmm_metadata.h"

#define NVLOG_CLIENT NVLOG_MP4_PARSER // Required for logging information

#define READ_BULK_DATA_SIZE (1024*10) //Data read size for preparing framing info
#define MAX_SPS_PARAM_SETS  2
#define MAX_PPS_PARAM_SETS  2

#define TICKS_PER_SECOND 1000000
#define MAX_MP4_SPAREAREA_SIZE (256 * 1024)

#define N_LAST_AUDIOBUFFERS 4

static const NvS32 FreqIndexTable[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, 0 };

static const unsigned char NvLeadingZerosTable[2048] =
{
    // 1 w/ 10 zeros (0)
    11,
    10,
    // 1 w/ 9 zeros (1)
    9,
    9,
    // 2 w/ 8 zeros (2, 3)
    8, 8,
    8, 8,
    // 4 w/ 7 zeros (4-7)
    7, 7, 7, 7,
    7, 7, 7, 7,
    // 8 w/ 6 zeros (8-15)
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    // 16 w/ 5 zeros (16-31)
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    // 32 w/ 4 zeros (32-63)
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    // 64 w/ 3 zeros (64-127)
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    // 128 w/ 2 zeros (128-255)
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    // 255 w/ 1 zero (256-511)
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    // 512 w/ 0 zeros (512-1023)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define MP4_DEFAULT_ATOM_HDR_SIZE 8

/**
 * Current mono/stereo decoder supports 1 front channel element
 *
 */
#define MAX_CHAN_ELMNTS 1
/**
 * Maximum no of tracks allowed in the file
 *
 */
#define MAX_TRACKS          16
/**
 * Number of information elements in each track info
 */
#define TRK_INFO_NELEM      7
/**
 * Maximum size of ESDS bytes
 */
#define MAX_ESDS_BYTES      512
#define MAX_ESDS_ATOM_SIZE  (50*1024)

/**
 * STSS Buffer Size
 */
#define STSS_BUFF_SIZE      (8192*4)

/**
 * MP4 Atoms FOURCC definition
 *
 */
#define MP4_FOURCC(ch0, ch1, ch2, ch3) ((NvU32)(NvU8)(ch3) | ((NvU32)(NvU8)(ch2) << 8) | ((NvU32)(NvU8)(ch1) << 16) | ((NvU32)(NvU8)(ch0) << 24))


#define MP4_MAX_METADATA  256
#define NV_MAX_SYNC_SCAN_FOR_THUMB_NAIL  20

/**
 * Decoder descriptor tags
 */
#define ES_DESC_TAG               0x03
#define DECODER_CONGIF_DESCR_TAG  0x04
#define DECODER_SPECIFIC_INFO_TAG 0x05


/**
 * Different streams
 */
#define OBJECT_DESCRIPTOR_STREAM   0x01
#define CLOCK_REFERENCE_STREAM     0x02
#define SCENE_DESCRIPTION_STREAM   0x03
#define VIDEO_STREAM              0x04
#define AUDIO_STREAM              0x05
#define MPEG7_STREAM              0x06
#define IPMP_STREAM               0x07
#define OBJECT_CONTENT_INFO_STREAM  0x08
#define MPEGJ_STREAM              0x09

/**
 * Video specific tags
 */
#define MPEG4_VIDEO 0x20
#define VISUAL_OBJECT_SEQUENCE_START_CODE   0x000001B0
#define VISUAL_OBJECT_SEQUENCE_END_CODE     0x000001B1
#define VISUAL_OBJECT_START_CODE            0x000001B5
#define VIDEO_OBJECT_START_CODE             0x00000100
#define VIDEO_OBJECT_LAYER_START_CODE       0x00000120
#define SHORT_VIDEO_START_MARKER            0x000020
#define USER_DATA_START_CODE                0x000001B2
#define START_CODE_PREFIX                   0x000001
#define EXTENDED_PAR                        0xF

/**
 * Audio specific tags
 */
#define MPEG4_AUDIO         0x40
#define MPEG2AACLC_AUDIO    0x67
#define MP3_AUDIO           0x6B
#define MPEG4_QCELP         0xE1
#define MPEG4_EVRC         0xD1

#define AUDIO_TRACK 1
#define VIDEO_TRACK 2

#define NUM_FRAMES_IN_BLOCK      5120  /* so that it is integral number of sectors (512 bytes) */

/*
 * Misc definitions
 */
#define MP4_INVALID_NUMBER 0xFFFFFFFF
#define MAX_REF_TRACKS  10
#define MAX_DECSPECINFO_COUNT 10

#define GETBITS(data, num) (data >> (32 - num))

typedef struct
{
    NvU32 size;
    NvU32 shift_reg_bits;
    NvU32 shift_reg;
    NvU8 *rdptr;
    NvBool IsError;
} NvMp4Bitstream;

/* Enum to distinguish between CAVLC and CABAC */
typedef enum
{
    AvcEntropy_Cavlc = 0x1,
    AvcEntropy_Cabac,
    AvcEntropy_Invalid,
    AvcEntropy_Force32 = 0x7FFFFFFF
} NvMp4AvcEntropy;

/**
 * Structure to store the AAC audio track configuration.
 */
typedef struct NvMp4AacConfigurationDataRec
{
    NvU32 ObjectType;
    NvU32 SamplingFreqIndex;
    NvU32 SamplingFreq;
    NvU32 NumOfChannels;
    NvU32 SampleSize;
    NvU32 ChannelConfiguration;
    NvU32 FrameLengthFlag;
    NvU32 DependsOnCoreCoder;
    NvU32 CoreCoderDelay;
    NvU32 ExtensionFlag;
    NvU32 SbrPresentFlag;
    NvU32 ExtensionAudioObjectType;
    NvU32 ExtensionSamplingFreqIndex;
    NvU32 ExtensionSamplingFreq;
    NvU32 EpConfig;
    NvU32 DirectMapping;
} NvMp4AacConfigurationData;

/**
 * FF REW Status  .
 */
typedef enum
{
    NvMp4ParserPlayState_FF,             /**< Forward mode */
    NvMp4ParserPlayState_REW,            /**< Rewind mode */
    NvMp4ParserPlayState_PLAY,            /**< Play mode */
    NvMp4ParserPlayState_Force32 = 0x7FFFFFFF
} NvMp4ParserPlayState;

/**
 * GetNextSyncUnit Error conditions.
 */
typedef enum
{
    NvMp4SyncUnitStatus_NOERROR = 0,                             /**< No error */
    NvMp4SyncUnitStatus_ERROR = -1,                                /**< Encountered a general non recoverable error */
    NvMp4SyncUnitStatus_STSSENTRIES_DONE = -2

} NvMp4SyncUnitStatus;

/**
 * Track Content.
 */
typedef enum
{
    NvMp4TrackTypes_AUDIO = 0,
    NvMp4TrackTypes_AAC,        /**< Mpeg-4 AAC LC/MAIN track */
    NvMp4TrackTypes_AACSBR,        /**< Mpeg-4 AAC SBR track */
    NvMp4TrackTypes_BSAC,        /**< Mpeg-4 ER BSAC track */
    NvMp4TrackTypes_QCELP,        /**< QCELP track */
    NvMp4TrackTypes_NAMR,        /**< Narrow band AMR track */
    NvMp4TrackTypes_WAMR,        /**< Wide band AMR track */
    NvMp4TrackTypes_EVRC,        /**< Wide band AMR track */
    NvMp4TrackTypes_VIDEO = 0x100,
    NvMp4TrackTypes_MPEG4,      /**< MPEG-4 Video Track*/
    NvMp4TrackTypes_AVC,            /**<H.264 track>*/
    NvMp4TrackTypes_S263,            /**<H.263 track>*/
    NvMp4TrackTypes_MJPEGA,
    NvMp4TrackTypes_MJPEGB,
    NvMp4TrackTypes_OTHER,       /**< NvMp4TrackTypes_OTHER not supported tracks*/
    NvMp4TrackTypes_UnsupportedAudio,  /**<for unsupported audio by mp4 parser >**/
    NvMp4TrackTypes_UnsupportedVideo,  /**<for unsupported Video by mp4 parser >**/
    NvMp4TrackTypes_Force32 = 0x7FFFFFFF
} NvMp4TrackTypes;

#define NVMP4_ISTRACKAUDIO(x) ((x) > NvMp4TrackTypes_AUDIO && (x) < NvMp4TrackTypes_VIDEO)
#define NVMP4_ISTRACKVIDEO(x) ((x) > NvMp4TrackTypes_VIDEO && (x) < NvMp4TrackTypes_OTHER)

/**
 * Track Type.
 */
typedef enum
{
    NvMp4StreamType_Audio,             /**< NvMp4StreamType_Audio Stream */
    NvMp4StreamType_Video,             /**< NvMp4StreamType_Video/M-JPEG Stream */
    NvMp4StreamType_Stream,            /**< represents streaming media.*/
    NvMp4StreamType_ObjectDescriptor,   /**< Object Descriptor Stream */
    NvMp4StreamType_ClockReference,        /**< Clock Reference Stream */
    NvMp4StreamType_SceneDescription,  /**< Scene Description Stream */
    NvMp4StreamType_MPEG7,             /**< MPEG7 Stream */
    NvMp4StreamType_ObjectContentInfo, /**< Object Content Info Stream */
    NvMp4StreamType_IPMP,              /**< NvS32ellectual Property Management and Protection Stream */
    NvMp4StreamType_MPEGJ,             /**< MPEG-J Stream */
    NvMp4StreamType_Hint,              /**< Hint Track */
    NvMp4StreamType_OTHER,
    NvMp4StreamType_Force32 = 0x7FFFFFFF

} NvMp4StreamType;


/**
 * Movie data
 */
typedef struct NvMp4MovieDataRec
{
    NvU64 CreationTime;                   /**< Creation time (In seconds since midnight 1.1.1904) */
    NvU64 ModifiedTime;               /**< Modification time (In seconds since midnight 1.1.1904) */
    NvU32 TimeScale;                      /**< Number of time units in a second. */
    NvU64 Duration;                       /**< Duration of the longest track. */
    NvU32 NextTrackID;                    /**< Value to be added to find ID of next track. */
    NvU32 TrackCount;                     /**< Number of tracks in the file. */
    NvU32 AudioTracks;                    /**< Number of NvMp4StreamType_Audio Tracks in the file */
    NvU32 VideoTracks;                    /**< Number of NvMp4StreamType_Video Tracks in the file */

} NvMp4MovieData;

typedef struct NvMp4MediaMetadataRec
{
    NvS8 Title[MP4_MAX_METADATA];
    NvS8 Album[MP4_MAX_METADATA];
    NvS8 Artist[MP4_MAX_METADATA];
    NvS8 AlbumArtist[MP4_MAX_METADATA];
    NvS8 Genre[MP4_MAX_METADATA];
    NvS8 Year[MP4_MAX_METADATA];
    NvS8 Composer[MP4_MAX_METADATA];
    NvS8 Copyright[MP4_MAX_METADATA];
    NvU32 TrackNumber;
    NvU32 TotalTracks;
    NvU64 CoverArtOffset;
    NvU32 CoverArtSize;
    NvU32 TitleSize;
    NvU32 AlbumSize;
    NvU32 ArtistSize;
    NvU32 AlbumArtistSize;
    NvU32 GenreSize;
    NvU32 YearSize;
    NvU32 ComposerSize;
    NvU32 CopyrightSize;
    NvMMMetaDataCharSet TitleEncoding;
    NvMMMetaDataCharSet AlbumEncoding;
    NvMMMetaDataCharSet ArtistEncoding;
    NvMMMetaDataCharSet AlbumArtistEncoding;
    NvMMMetaDataCharSet GenreEncoding;
    NvMMMetaDataCharSet YearEncoding;
    NvMMMetaDataCharSet ComposerEncoding;
    NvMMMetaDataCharSet CopyrightEncoding;
} NvMp4MediaMetadata;

/// data structure to help synch unit searches
typedef struct NvMp4SynchUnitDataRec
{
    /// signals if the structure is ready to use
    NvBool IsReady;
    /// the entry number in the stss atom for the first entry in the buffer
    NvU32 FirstEntryIndex;
    /// number of valid entries in the buffer
    NvU32 ValidCount;
    /// half of the cached values go in this one
    NvS32 ReadBuffer[STSS_BUFF_SIZE];
} NvMp4SynchUnitData;

typedef struct NvElstTableRec
{
    NvU32 EntryCount;
    NvU64 SegmentDuration;
    NvU64 MediaTime;
    NvU16 MediaRate;
} NvElstTable;

typedef struct NvStscEntryRec
{
    NvU32 first_chunk;
    NvU32 samples_per_chunk;
    NvU32 sample_desc_index;
    struct NvStscEntryRec *next; /*next=Null if it's the last entry*/
} NvStscEntry;

/**
   * Stores the frames size and offset.
   */
typedef struct NvMp4FramingInfoRec
{
    NvU32 TotalNoFrames;
    NvU32 TotalNoBlocks; /* totalNoFrames / NFRAMESINBLOCK */
    NvU32 CurrentBlock;
    NvU32 CurrentFrameIndex;
    NvU32 FrameSize;
    NvU32 CurrentChunk;
    NvU64 *OffsetTable;
    NvU32 *SizeTable;
    NvU64 *DTSTable;
    NvU32 *CTSTable;
    NvU32 MaxFramesPerBlock;
    NvBool IsCo64bitOffsetPresent;
    NvU32 SkipFrameCount;
    NvU32 ValidFrameCount;
    NvBool IsCorruptedFile;
    NvU32 BitRate;
    NvU32 FrameCounter;
    NvU32 MaxFrameSize;
    NvStscEntry *StscEntryListHead;
    NvBool IsValidStsc;
} NvMp4FramingInfo;


/**
   * Stores the track Information
   */
typedef struct NvMp4TrackInformationRec
{
    NvMp4TrackTypes TrackType;
    NvMp4StreamType StreamType;
    NvU32 TrackID;                          /**< ID of the track */
    NvU64 TrackOffset;                    /**< Track offset */

    NvU64 ELSTOffset;
    NvU32 ELSTEntries;

    NvU64 STTSOffset;                     /**< stts atom offset from beginning of the file */
    NvU32 STTSEntries;

    NvU64 CTTSOffset;                     /**< ctts atom offset from beginning of the file */
    NvU32 CTTSEntries;

    NvU64 STBLOffset;                     /**< stsc atom offset from beginning of the file */
    NvU64 STBLSize;                     /**< stsc atom offset from beginning of the file */

    NvU64 STSCOffset;                     /**< stsc atom offset from beginning of the file */
    NvU32 STSCEntries;                      /**< Number of stsc entries */
    NvU32 STSCAtomSize;                   /**< Number of stsc entries */

    NvU64 STCOOffset;                     /**< stbl atom offset from beginning of the file */
    NvU32 STCOEntries;                      /**< Number of stco entries */

    NvU64 CO64Offset;
    NvU32 CO64Entries;

    NvU64 STSSOffset;                     /**< stss atom offset from beginning of the file */
    NvU32 STSSEntries;                      /**< Number of stss entries */

    NvU64 STSZOffset;                     /**< stbl atom offset from beginning of the file */
    NvU32 STSZSampleSize;                 /**< Default sample size. If =0 then variable sample size*/
    NvU32 STSZSampleCount;                /**< Number of stsz entries */

    NvU32 HandlerType;                    /**< Handler declaring the nature of the media */
    NvU64 TimeScale;                      /**< Number of time units in a second. */

    NvU64 Duration;                       /**< Duration of the track in it's timescale. */
    NvU32 ObjectType;                     /**< Object type. */
    NvU32 AvgBitRate;
    NvU32 MaxBitRate;
    NvU32 Width;
    NvU32 Height;

    NvU32 AtomType;                    /** Atom Type */
    NvU32 AtomSize;                     /** Size of the atom */
    NvU8 *pDecSpecInfo[MAX_DECSPECINFO_COUNT]; /*decoder configuration for the particular media*/
    NvU32 DecInfoSize[MAX_DECSPECINFO_COUNT]; /*size of decoder specific information*/
    NvU32 nDecSpecInfoCount; /*count of valid decoder specific information*/

    NvU64 *SegmentTable;
    NvS64 *pELSTMediaTime;
    NvU8 *pESDS;
    NvBool IsCo64bitOffsetPresent;
    NvBool IsELSTPresent;
    NvBool EndOfStream;
    NvU32 SyncFrameSize;

    NvU32 IFrameNumbers[NV_MAX_SYNC_SCAN_FOR_THUMB_NAIL];
    NvU64 LargestIFrameTimestamp;
    NvBool IsValidSeekTable;
    NvU32  MaxScanSTSSEntries;

} NvMp4TrackInformation ;

typedef struct NvAVCConfigDataRec
{
    NvU32 ES_ID;
    NvU32 DataRefIndex;
    NvU32 Width;
    NvU32 Height;
    NvU32 HorizontalResolution;
    NvU32 VerticalResolution;
    NvU32 DataSize;
    NvU32 FrameCount;

    NvS32 Field1;
    NvS32 Field2;
    NvS32 Gamma;
    NvS32 Depth;
    NvS32 ColorTableID;

    NvS32 ConfigVersion;
    NvS32 ProfileIndication;
    NvS32 ProfileCompatibility;
    NvS32 LevelIndication;
    NvU32 Length;
    NvU32 NumSequenceParamSets;
    NvU32 SeqParamSetLength[MAX_SPS_PARAM_SETS];

    NvU8 SpsNALUnit[MAX_SPS_PARAM_SETS][128]; // 128 bytes is arbitrary: Need sufficient storage

    NvU32 NumOfPictureParamSets;
    NvU32 PicParamSetLength[MAX_PPS_PARAM_SETS];

    NvU8 PpsNALUnit[MAX_PPS_PARAM_SETS][128]; // 128 bytes is arbitrary: Need sufficient storage
    NvMp4AvcEntropy EntropyType;

} NvAVCConfigData;


#endif //INCLUDED_NV_MP4PARSER_DEFINES_H



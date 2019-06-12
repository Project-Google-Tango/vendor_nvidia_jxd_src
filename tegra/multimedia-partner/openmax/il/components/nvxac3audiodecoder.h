/* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** nvxeac3audiodecoder.h */

#ifndef NVXAC3AUDIODECODER_H_
#define NVXAC3AUDIODECODER_H_

#include "common/NvxComponent.h"

#define NVX_AC3_SYNCWORD       0x0B77
#define NVX_AC3_SYNCWORD_REV   0x770B

#define NVX_AC3_MAX_FRAMES       1        //< Maximum number of frames per time slice
#define NVX_AC3_MAX_PCM_CHANNELS 8        //< Maximum number of PCM channels
#define NVX_AC3_MAX_PCM_OUTPUTS  8        //< Maximum number of PCM outputs

#define NVX_AC3_MAX_BLOCKS        6
#define NVX_AC3_BLOCK_LENGTH      256
#define NVX_AC3_FRAME_SIZE_WORDS  3         //< Number of header bytes to read to obtain header size

#define	NVX_AC3_MAX_DD_FRAME_WORDS   1920   //< Maximum number of words per dolby digital frame
#define	NVX_AC3_MAX_DDP_FRAME_WORDS  2048   //< Maximum number of words per DD+ frame

#define NVX_AC3_CHANNEL_BUFSIZE (NVX_AC3_MAX_BLOCKS * NVX_AC3_BLOCK_LENGTH)
#define NVX_AC3_MAX_PCM_BUFSIZE (NVX_AC3_CHANNEL_BUFSIZE * NVX_AC3_MAX_PCM_CHANNELS)

// Translates dlb_bfd
typedef struct NVX_AC3_BUFFER_DESC
{
    void        **ppBuffer;         //< Pointer to the buffer data
    OMX_U16     nChannels;          //< Current number of samples in the buffer
    OMX_U16     nStride;            //< Offset between PCM samples for interleaved data
} NVX_AC3_BUFFER_DESC;

// Translates decoder error codes
typedef enum NVX_AC3_DEC_ERR
{
    NVX_AC3_DEC_ERR_NONE,          //< No frame error
    NVX_AC3_DEC_ERR_PARTIAL_FRAME, //< Partial frame error
    NVX_AC3_DEC_ERR_FRAME,         //< General frame error
    NVX_AC3_DEC_ERR_UNKNOWN,       //< Unknown error
} NVX_AC3_DEC_ERR;

// Translates to ddpi_udc_pt_op
typedef struct NVX_AC3_TIMESLICE_OUTPUT
{
    NVX_AC3_BUFFER_DESC  pPcmBufferDesc[NVX_AC3_MAX_PCM_OUTPUTS];
        //< Array of PCM buffer descriptors for each channel

    int             nBlocksPCM;                      //< Number of PCM blocks
    int             nSampleRate;                     //< Sample rate of data
    unsigned int    nBlocksPerTimeSlice;             //< Number of blocks per time slice

    OMX_BOOL        bPcmValid[NVX_AC3_MAX_PCM_OUTPUTS]; //< Whether this PCM output is valid
    NVX_AC3_DEC_ERR eError[NVX_AC3_MAX_PCM_OUTPUTS];    //< Stores error codes
} NVX_AC3_TIMESLICE_OUTPUT;

typedef enum NVX_AC3_OUTPUT_MODE
{
    NVX_AC3_OUTPUT_MODE_C       = 1,
    NVX_AC3_OUTPUT_MODE_LR      = 2,
    NVX_AC3_OUTPUT_MODE_CLR     = 3,
    NVX_AC3_OUTPUT_MODE_LRS     = 4,
    NVX_AC3_OUTPUT_MODE_CLRS    = 5,
    NVX_AC3_OUTPUT_MODE_LRLsRs  = 6,
    NVX_AC3_OUTPUT_MODE_CLRLsRs = 7
} NVX_AC3_OUTPUT_MODE;

#endif // NVXEAC3AUDIODECODER_H_


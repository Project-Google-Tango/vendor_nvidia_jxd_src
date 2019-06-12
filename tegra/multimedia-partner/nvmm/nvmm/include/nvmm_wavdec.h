/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @brief Defines the structure for holding the Decoder Side Context specific information.
 */

#ifndef INCLUDED_NVMM_AUDIO_H
#define INCLUDED_NVMM_AUDIO_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"
#include "nvmm_block.h"
#include "nvmm_wave.h"  // Needed for Property only...Context structures(2) should probably be in the C file.

#if defined(__cplusplus)
extern "C"
{
#endif

#define MAX_IN_BUFFER_SIZE     30720 // A hack for AVI parser
#define MIN_IN_BUFFER_SIZE     8192
#define MAX_OUT_BUFFER_SIZE    8192
#define MIN_OUT_BUFFER_SIZE    8192
#define MAX_INPUT_BUFFERS      32
#define MIN_INPUT_BUFFERS      1
#define MAX_OUTPUT_BUFFERS     10
#define MIN_OUTPUT_BUFFERS     5

#define MIN_MIXER_BUFFER_SIZE  6144
#define MAX_MIXER_BUFFER_SIZE  6144
#define MAX_MIXER_BUFFERS      10
#define MIN_MIXER_BUFFERS      4



/** Context for Wave Decoder block    */
typedef struct WavDecContext_ {

    NvMMBlockContext block; //!< NvMMBlock base class, must be first member to allow down-casting
    NvU32 StructSize;
    NvBool bInitCalled;
    NvBool SetAttributeDisable;  // NV_FALSE if SetAttribute is used, NV_TRUE if first input Payload is used
    SendBlockEventFunction sendEvent;
    void *pOutGoingEventContext;
    NvMMWaveStreamProperty  waveProp;
    NvU32                   pos;
    NvBool UpdateTimestamp;
    NvU64                   iTimeStamp;
    // handle to hold the base of IRAM memory
    NvRmMemHandle hMemIRAMHandle;
    // base pointer to hold the total IRAM buffer required address
    NvU32 pIRAMBaseAddress;

    //ADPCM
    NvS16 *samplePtr;       /* Pointer to current sample  */
    NvU32 numSamples;      /* samples/channel reading: starts at total count and decremented  */
    NvU16 blockSamplesRemaining;   /* Samples remaining per channel */
    NvU16 samplesPerBlock;
    NvU8 *packet;        /* Temporary buffer for packets */
    NvU16 nCoefs;          /* ADPCM: number of coef sets */
    NvS16 *samples;         /* interleaved samples buffer */
    NvBool found_cooledit;
    NvU32 remainingBytes;
} WavDecContext;

NvError NvMMWavDecOpen(
    NvMMBlockHandle *phBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle semaphore,
    NvMMDoWorkFunction *pDoWorkFunction);

void NvMMWavDecClose(
    NvMMBlockHandle hBlock,
    NvMMInternalDestructionParameters *pParams);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_AUDIO_H

/**
 * @file nvmm_mps_defines.h
 * @brief <b>NVIDIA Media Parser Package:mps parser/b>
 *
 * @b Description:   This file has definitions common to parser and reader
 */

/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all NvS32ellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVMM_MPS_DEFINES_H
#define INCLUDED_NVMM_MPS_DEFINES_H

#include "nvmm.h"
#include "nvlocalfilecontentpipe.h"

#define NV_MSP_DEBUG_PRINT 0

/* Various search limits used to avoid indefinite parsing and CPU hogging */
#define NV_MPS_MAX_SCRATCH_SIZE (8*1024)
#define NV_MPS_MAX_PACK_SEARCH_SIZE (32*1024)
#define NV_MPS_DURATION_SEARCH_COUNT (10)
#define NV_MPS_MAX_PES_SEARCH_SIZE (5*1024*1024)
#define NV_MPS_MAX_AVINFO_SEARCH_SIZE (1*1024*1024)

/* MPEG standard related defines */
#define NV_MPS_SYNC_MARKER_SIZE    (4)
#define NV_MPS_VSEQ_HEADER_CODE    (0xB3)
#define NV_MPS_PACK_HEADER_CODE    (0xBA)
#define NV_MPS_SYS_HEADER_CODE     (0xBB)
#define NV_MPS_STREAM_ID_PSM       (0xBC)
#define NV_MPS_STREAM_ID_PRIV_1    (0xBD)
#define NV_MPS_STREAM_ID_PAD       (0xBE)
#define NV_MPS_STREAM_ID_PRIV_2    (0xBF)
#define NV_MPS_STREAM_ID_AUDIO     (0xC0)
#define NV_MPS_STREAM_ID_AUDIO_EXT (0xD0)
#define NV_MPS_STREAM_ID_VIDEO     (0xE0)
#define NV_MPS_STREAM_ID_OTHER_MIN (0xF0)
#define NV_MPS_STREAM_ID_OTHER_MAX (0xFA)


typedef enum NvMpsMpegType {NV_MPS_MPEG_1,
                            NV_MPS_MPEG_2
                           } eNvMpsMpegType;

typedef enum NvMpsVideoType {NV_MPS_VIDEO_UNKNOWN,
                             NV_MPS_VIDEO_MPEG_1,
                             NV_MPS_VIDEO_MPEG_2
                            } eNvMpsVideoType;

typedef enum NvMpsAudioType {NV_MPS_AUDIO_UNKNOWN,
                             NV_MPS_AUDIO_MP2,
                             NV_MPS_AUDIO_MP3
                            } eNvMpsAudioType;

typedef enum NvMpsStreamType {NV_MPS_STREAM_ANY,
                              NV_MPS_STREAM_AUDIO,
                              NV_MPS_STREAM_VIDEO
                             } eNvMpsMediaType;

typedef enum NvMpsResult {
                         NV_MPS_RESULT_OK,
                         NV_MPS_RESULT_END_OF_FILE,
                         NV_MPS_RESULT_NEED_MORE_DATA,
                         NV_MPS_RESULT_SYNC_NOT_FOUND,
                         NV_MPS_RESULT_BOGUS_HEADER
                        } eNvMpsResult;

#endif //INCLUDED_NVMM_MPS_DEFINES_H

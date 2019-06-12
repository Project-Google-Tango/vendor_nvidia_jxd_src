/*
 * Copyright (c) 2006 - 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVMM_PARSER_TYPES_H
#define INCLUDED_NVMM_PARSER_TYPES_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* NvMMParserCoreType defines various supported parser cores */

/*
 ************* Do *NOT* add new types to the middle of this enum.
 ** WARNING ** Only add to the end, before the VALIDTOP entry.
 ************* Conversely, do *not* delete types from the middle.
 */
typedef enum
{
    NvMMParserCoreType_UnKnown = 0x00, /* Unknown parser */

    NvMMParserCoreType_VALIDBASE = 0x001,
    NvMMParserCoreType_AsfParser,    /* Asf Parser */
    NvMMParserCoreType_3gppParser,   /* Deprecated in favor of Mp4 */
    NvMMParserCoreType_AviParser,    /* Avi Parser */
    NvMMParserCoreType_Mp3Parser,    /* mp3 Parser */
    NvMMParserCoreType_Mp4Parser,    /* mp4 Parser */
    NvMMParserCoreType_OggParser,    /* ogg Parser */
    NvMMParserCoreType_AacParser,    /* Aac Parser */
    NvMMParserCoreType_AmrParser,    /* Amr Parser */
    NvMMParserCoreType_RtspParser,   /* Deprecated */
    NvMMParserCoreType_M2vParser,    /* M2v Parser */
    NvMMParserCoreType_MtsParser,    /* Mts Parser */
    NvMMParserCoreType_NemParser,    /* NvMM generic simple format parser */
    NvMMParserCoreType_WavParser,    /* Represents Parser core Type for Wav Parser */
    NvMMParserCoreType_Dummy1,       /* dummy Parser */
    NvMMParserCoreType_FlvParser,    /* Flv Parser */
    NvMMParserCoreType_Dummy2,       /* dummy Parser */
    NvMMParserCoreType_MkvParser,    /* Mkv Parser */
    NvMMParserCoreType_MpsParser,    /* Mpeg-PS Parser */
    NvMMParserCoreType_VALIDTOP,
    NvMMParserCoreTypeForce32 = 0x7FFFFFFF
} NvMMParserCoreType;

#define NVMM_ISVALIDPARSER_TYPE(x) ((x) > NvMMParserCoreType_VALIDBASE && (x) < NvMMParserCoreType_VALIDTOP)

#if defined(__cplusplus)
}
#endif

#endif


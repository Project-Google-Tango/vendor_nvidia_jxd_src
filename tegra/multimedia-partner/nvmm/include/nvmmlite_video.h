/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM Video Codec Common APIs</b>
 *
 * @b Description: Declares Interface for NvMM Video codec common APIs.
 */

#ifndef INCLUDED_NVMMLITE_VIDEO_H
#define INCLUDED_NVMMLITE_VIDEO_H

/** @defgroup nvmmlite_videocommon Video Codec Common API
 *
 * Describe the APIs here
 *
 * @ingroup nvmmlite_modules
 * @{
 */

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    NvMMLiteVideo_AVCProfileBaseline = 0x01,   /**< Baseline profile */
    NvMMLiteVideo_AVCProfileMain     = 0x02,   /**< Main profile */
    NvMMLiteVideo_AVCProfileExtended = 0x04,   /**< Extended profile */
    NvMMLiteVideo_AVCProfileHigh     = 0x08,   /**< High profile */
    NvMMLiteVideo_AVCProfileHigh10   = 0x10,   /**< High 10 profile */
    NvMMLiteVideo_AVCProfileHigh422  = 0x20,   /**< High 4:2:2 profile */
    NvMMLiteVideo_AVCProfileHigh444  = 0x40,   /**< High 4:4:4 profile */
    NvMMLiteVideo_AVCProfileUnknown      = 0x7FFFFFFF
} NvMMLiteVideoAVCProfileType;

typedef enum
{
    NvMMLiteVideo_AVCLevel1   = 0x01,     /**< Level 1 */
    NvMMLiteVideo_AVCLevel1b  = 0x02,     /**< Level 1b */
    NvMMLiteVideo_AVCLevel11  = 0x04,     /**< Level 1.1 */
    NvMMLiteVideo_AVCLevel12  = 0x08,     /**< Level 1.2 */
    NvMMLiteVideo_AVCLevel13  = 0x10,     /**< Level 1.3 */
    NvMMLiteVideo_AVCLevel2   = 0x20,     /**< Level 2 */
    NvMMLiteVideo_AVCLevel21  = 0x40,     /**< Level 2.1 */
    NvMMLiteVideo_AVCLevel22  = 0x80,     /**< Level 2.2 */
    NvMMLiteVideo_AVCLevel3   = 0x100,    /**< Level 3 */
    NvMMLiteVideo_AVCLevel31  = 0x200,    /**< Level 3.1 */
    NvMMLiteVideo_AVCLevel32  = 0x400,    /**< Level 3.2 */
    NvMMLiteVideo_AVCLevel4   = 0x800,    /**< Level 4 */
    NvMMLiteVideo_AVCLevel41  = 0x1000,   /**< Level 4.1 */
    NvMMLiteVideo_AVCLevel42  = 0x2000,   /**< Level 4.2 */
    NvMMLiteVideo_AVCLevel5   = 0x4000,   /**< Level 5 */
    NvMMLiteVideo_AVCLevel51  = 0x8000,   /**< Level 5.1 */
    NvMMLiteVideo_AVCLevelUnknown = 0x7FFFFFFF,
}NvMMLiteVideoAVCLevelType;

/**
 * MPEG-4 profile types, each profile indicates support for various
 * performance bounds and different annexes.
 *
 * ENUMS:
 *  - Simple Profile, Levels 1-3
 *  - Simple Scalable Profile, Levels 1-2
 *  - Core Profile, Levels 1-2
 *  - Main Profile, Levels 2-4
 *  - N-bit Profile, Level 2
 *  - Scalable Texture Profile, Level 1
 *  - Simple Face Animation Profile, Levels 1-2
 *  - Simple Face and Body Animation (FBA) Profile, Levels 1-2
 *  - Basic Animated Texture Profile, Levels 1-2
 *  - Hybrid Profile, Levels 1-2
 *  - Advanced Real Time Simple Profiles, Levels 1-4
 *  - Core Scalable Profile, Levels 1-3
 *  - Advanced Coding Efficiency Profile, Levels 1-4
 *  - Advanced Core Profile, Levels 1-2
 *  - Advanced Scalable Texture, Levels 2-3
 */
typedef enum
{
    NvMMLiteVideo_MPEG4ProfileSimple           = 0x01,
    NvMMLiteVideo_MPEG4ProfileSimpleScalable   = 0x02,
    NvMMLiteVideo_MPEG4ProfileCore             = 0x04,
    NvMMLiteVideo_MPEG4ProfileMain             = 0x08,
    NvMMLiteVideo_MPEG4ProfileNbit             = 0x10,
    NvMMLiteVideo_MPEG4ProfileScalableTexture  = 0x20,
    NvMMLiteVideo_MPEG4ProfileSimpleFace       = 0x40,
    NvMMLiteVideo_MPEG4ProfileSimpleFBA        = 0x80,
    NvMMLiteVideo_MPEG4ProfileBasicAnimated    = 0x100,
    NvMMLiteVideo_MPEG4ProfileHybrid           = 0x200,
    NvMMLiteVideo_MPEG4ProfileAdvancedRealTime = 0x400,
    NvMMLiteVideo_MPEG4ProfileCoreScalable     = 0x800,
    NvMMLiteVideo_MPEG4ProfileAdvancedCoding   = 0x1000,
    NvMMLiteVideo_MPEG4ProfileAdvancedCore     = 0x2000,
    NvMMLiteVideo_MPEG4ProfileAdvancedScalable = 0x4000,
    NvMMLiteVideo_MPEG4ProfileAdvancedSimple   = 0x8000,
    NvMMLiteVideo_MPEG4ProfileUnknown          = 0x7FFFFFFF
} NvMMLiteVideoMPEG4ProfileType;

/**
 * MPEG-4 level types, each level indicates support for various frame
 * sizes, bit rates, decoder frame rates.  No need
 */
typedef enum
{
    NvMMLiteVideo_MPEG4Level0  = 0x01,   /**< Level 0 */
    NvMMLiteVideo_MPEG4Level0b = 0x02,   /**< Level 0b */
    NvMMLiteVideo_MPEG4Level1  = 0x04,   /**< Level 1 */
    NvMMLiteVideo_MPEG4Level2  = 0x08,   /**< Level 2 */
    NvMMLiteVideo_MPEG4Level3  = 0x10,   /**< Level 3 */
    NvMMLiteVideo_MPEG4Level4  = 0x20,   /**< Level 4 */
    NvMMLiteVideo_MPEG4Level4a = 0x40,   /**< Level 4a */
    NvMMLiteVideo_MPEG4Level5  = 0x80,   /**< Level 5 */
    NvMMLiteVideo_MPEG4LevelUnknown = 0x7FFFFFFF
} NvMMLiteVideoMPEG4LevelType;

/**
 * H.263 profile types, each profile indicates support for various
 * performance bounds and different annexes.
 *
 * ENUMS:
 *  Baseline           : Baseline Profile: H.263 (V1), no optional modes
 *  H320 Coding        : H.320 Coding Efficiency Backward Compatibility
 *                       Profile: H.263+ (V2), includes annexes I, J, L.4
 *                       and T
 *  BackwardCompatible : Backward Compatibility Profile: H.263 (V1),
 *                       includes annex F
 *  ISWV2              : Interactive Streaming Wireless Profile: H.263+
 *                       (V2), includes annexes I, J, K and T
 *  ISWV3              : Interactive Streaming Wireless Profile: H.263++
 *                       (V3), includes profile 3 and annexes V and W.6.3.8
 *  HighCompression    : Conversational High Compression Profile: H.263++
 *                       (V3), includes profiles 1 & 2 and annexes D and U
 *  Internet           : Conversational Internet Profile: H.263++ (V3),
 *                       includes profile 5 and annex K
 *  Interlace          : Conversational Interlace Profile: H.263++ (V3),
 *                       includes profile 5 and annex W.6.3.11
 *  HighLatency        : High Latency Profile: H.263++ (V3), includes
 *                       profile 6 and annexes O.1 and P.5
 */
typedef enum
{
    NvMMLiteVideo_H263ProfileBaseline            = 0x01,
    NvMMLiteVideo_H263ProfileH320Coding          = 0x02,
    NvMMLiteVideo_H263ProfileBackwardCompatible  = 0x04,
    NvMMLiteVideo_H263ProfileISWV2               = 0x08,
    NvMMLiteVideo_H263ProfileISWV3               = 0x10,
    NvMMLiteVideo_H263ProfileHighCompression     = 0x20,
    NvMMLiteVideo_H263ProfileInternet            = 0x40,
    NvMMLiteVideo_H263ProfileInterlace           = 0x80,
    NvMMLiteVideo_H263ProfileHighLatency         = 0x100,
    NvMMLiteVideo_H263ProfileUnknown             = 0x7FFFFFFF
} NvMMLiteVideoH263ProfileType;


/**
 * H.263 level types, each level indicates support for various frame sizes,
 * bit rates, decoder frame rates.
 */
typedef enum
{
    NvMMLiteVideo_H263Level10  = 0x01,
    NvMMLiteVideo_H263Level20  = 0x02,
    NvMMLiteVideo_H263Level30  = 0x04,
    NvMMLiteVideo_H263Level40  = 0x08,
    NvMMLiteVideo_H263Level45  = 0x10,
    NvMMLiteVideo_H263Level50  = 0x20,
    NvMMLiteVideo_H263Level60  = 0x40,
    NvMMLiteVideo_H263Level70  = 0x80,
    NvMMLiteVideo_H263LevelUnknown = 0x7FFFFFFF
} NvMMLiteVideoH263LevelType;

/**
 * MPEG-2 profile types, each profile indicates support for various
 * performance bounds and different annexes.
 */
typedef enum
{
    NvMMLiteVideo_MPEG2ProfileSimple = 0,  /**< Simple Profile */
    NvMMLiteVideo_MPEG2ProfileMain,        /**< Main Profile */
    NvMMLiteVideo_MPEG2Profile422,         /**< 4:2:2 Profile */
    NvMMLiteVideo_MPEG2ProfileSNR,         /**< SNR Profile */
    NvMMLiteVideo_MPEG2ProfileSpatial,     /**< Spatial Profile */
    NvMMLiteVideo_MPEG2ProfileHigh,        /**< High Profile */
    NvMMLiteVideo_MPEG2ProfileUnknown = 0x7FFFFFFF
} NvMMLiteVideoMPEG2ProfileType;


/**
 * MPEG-2 level types, each level indicates support for various frame
 * sizes, bit rates, decoder frame rates.  No need
 */
typedef enum
{
    NvMMLiteVideo_MPEG2LevelLL = 0,  /**< Low Level */
    NvMMLiteVideo_MPEG2LevelML,      /**< Main Level */
    NvMMLiteVideo_MPEG2LevelH14,     /**< High 1440 */
    NvMMLiteVideo_MPEG2LevelHL,      /**< High Level */
    NvMMLiteVideo_MPEG2LevelUnknown = 0x7FFFFFFF
} NvMMLiteVideoMPEG2LevelType;

/**
* VP8 Profile types
*/
typedef enum
{
    NvMMLiteVideo_VP8ProfileMain    = 0x01,
    NvMMLiteVideo_VP8ProfileUnknown = 0x7FFFFFFF
} NvMMLiteVideoVP8ProfileType;

/**
* VP8 Level types
*/
typedef enum
{
    NvMMLiteVideo_VP8LevelVersion0  = 0x0,
    NvMMLiteVideo_VP8LevelVersion1  = 0x1,
    NvMMLiteVideo_VP8LevelVersion2  = 0x2,
    NvMMLiteVideo_VP8LevelVersion3  = 0x3,
    NvMMLiteVideo_VP8LevelUnknown   = 0x7FFFFFFF
} NvMMLiteVideoVP8LevelType;

/**
 * @brief Defines various enum values for coding type.
 */
typedef enum
{
    NvMMLiteVideo_CodingTypeMPEG2 = 0x1,
    NvMMLiteVideo_CodingTypeH263,
    NvMMLiteVideo_CodingTypeMPEG4,
    NvMMLiteVideo_CodingTypeH264,
    NvMMLiteVideo_CodingTypeMJPEG,
    NvMMLiteVideo_CodingTypeVP8,
    NvMMLiteVideo_CodingTypeH265,
    NvMMLiteVideo_CodingType_Force32 = 0x7FFFFFFF
} NvMMLiteVideoCodingType;

/**
 * Defines the Profile and Level information of a codec
 * Read from codec by a GetAttribute() call with
 * NvMMLiteVideoDecAttribute_ProfileLevelQuery and
 * NvMMLiteVideoEncAttribute_ProfileLevelQuery
 */
typedef struct
{
    NvMMLiteVideoCodingType eType;
                         /* Coding Type */
    NvU32 ProfileIndex;  /* Index of the supported profile@level info */
    NvU32 Profile;       /* Caller needs to typecast it to NvMMLiteVideo_MPEG2ProfileType/
                          * NvMMLiteVideoH263ProfileType/NvMMLiteVideoAVCProfileType/
                          * NvMMLiteVideoMPEG4ProfileType based on the codec type */
    NvU32 Level;         /* Caller needs to typecast it to NvMMVideo_MPEG2LevelType/
                          * NvMMLiteVideoH263LevelType/NvMMLiteVideoAVCLevelType/
                          * NvMMLiteVideoMPEG4LevelType based on the codec type */
} NvMMLiteVideoProfileLevelInfo;

/**
 * Defines the capabilities of a decoder
 * Read from decoder by a GetAttribute() call with
 * NvMMLiteVideoDecAttribute_MaxCapabilities
 */
typedef struct
{
    NvMMLiteVideoCodingType eType;
                         /* Coding Type */
    NvU32 CapIndex;      /* Index of the supported capability (0 to N) */
    NvU32 MaxProfile;    /* Caller needs to typecast it to NvMMLiteVideo_MPEG2ProfileType/
                          * NvMMLiteVideoH263ProfileType/NvMMLiteVideoAVCProfileType/
                          * NvMMLiteVideoMPEG4ProfileType based on the codec type */
    NvU32 MaxLevel;      /* Caller needs to typecast it to NvMMVideo_MPEG2LevelType/
                          * NvMMLiteVideoH263LevelType/NvMMLiteVideoAVCLevelType/
                          * NvMMLiteVideoMPEG4LevelType based on the codec type */
    NvU32 MaxWidth;      /* Maximum frame width supported */
    NvU32 MaxHeight;     /* Maximum frame height supported */
    NvU32 FrameRate;     /* Frame rate supported for max resolution */
    NvU32 Bitrate;       /* Bitrate rate supported for max resolution */
} NvMMLiteVideoMaxCapabilities;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMMLITE_VIDEO_H

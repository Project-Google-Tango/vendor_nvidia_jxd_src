/*
 * Copyright (c) 2006 - 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVMM_WRITER_H
#define INCLUDED_NVMM_WRITER_H

#include "nvcommon.h"

#include "nvmm_parser.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/** Maximum size of  userdata */
#define NVMM_MAX_ATOM_SIZE 5
#define ONE_MB (1048576)
#define FILE_SIZE_LIMIT (2147483647)

#define NVMM_DEBUG_BASEWRITER (0)
#define NVMM_DEBUG_BASEWRITER_ERROR (0)

#define MAX_FILE_EXT 8

#if (NVMM_DEBUG_BASEWRITER)
#define NvmmDebugBaseWriter(x) NvOsDebugPrintf x
#else
#define NvmmDebugBaseWriter(x)
#endif

#if (NVMM_DEBUG_BASEWRITER_ERROR)
#define NvmmDebugBaseWriterError(x) NvOsDebugPrintf x
#else
#define NvmmDebugBaseWriterError(x)
#endif

/**
 * @brief Writer Attribute enumerations.
 * Following enum are used by parsers for setting/getting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() and GetAttribute() APIs.
 * @see SetAttribute
 */
typedef enum
{
 /** AttributeType for Base writer Block
    *     To set NvMMWriterAttribute_StreamConfig to writer  block.
    */
    NvMMWriterAttribute_StreamConfig =
                            (NvMMAttributeWriter_Unknown + 1),

  /** AttributeType for Base writer Block
    *     To set NvMMWriterAttribute_FileName for writer block.
    */
    NvMMWriterAttribute_FileName,

  /** AttributeType for Base writer Block
    *     To set NvMMWriterAttribute_SplitTrack for writer block.
    */
    NvMMWriterAttribute_SplitTrack,

 /** AttributeType for Base writer Block
      *     To set NvMMWriterAttribute_StreamCount for writer block.
      */
   NvMMWriterAttribute_StreamCount,

 /** AttributeType for Base writer Block
      *     To get NvMMWriterAttribute_FrameCount from writer block.
      */
   NvMMWriterAttribute_FrameCount,

 /** AttributeType for Base writer Block
    *     To set NvMMWriterAttribute_FileParams for writer block.
    */
    NvMMWriterAttribute_UserData,

  /** AttributeType for Base writer Block
      *     To set NvMMWriterAttribute_FileSize for writer block.
      */
    NvMMWriterAttribute_FileSize,

    /** AttributeType for Base writer Block
      *     To set NvMMWriterAttribute_TempFilePath for writer block.
      * attribute for setting path for temporary files
      */
    NvMMWriterAttribute_TempFilePath,

    /** AttributeType for Base writer Block
      *     To set NvMMWriterAttribute_FileExt for setting file ext.
      */
    NvMMWriterAttribute_FileExt,

    /** AttributeType for Base writer Block
      *     To set NvMMWriterAttribute_TrakDurationLimit for writer block.
      * attribute for setting Duration in msec
      */
    NvMMWriterAttribute_TrakDurationLimit,

    NvMMWriterAttribute_Force32 = 0x7FFFFFFF

}NvMMWriterAttribute;


/** AMR encode type
   */

typedef enum
{
    NvMM_NBAMR_EncodeType_MMS = 0x0, // MMS format
    NvMM_NBAMR_EncodeType_IF1 ,    //IF1 format
    NvMM_NBAMR_EncodeType_IF2,  //IF2 format


    NvMM_NBAMR_EncodeType_Force32 = 0x7FFFFFFF

}NvMMNBAMREncodeType;

/** NvMMAttribute for writer to set the file name */
typedef struct
{
    NvString szURI; //pFileName
}NvMMWriterAttrib_FileName;

/** NvMMAttribute for writer to set the temporary file path */
typedef struct
{
    NvString szURITempPath; //pFileName
}NvMMWriterAttrib_TempFilePath;

/**
  * NvMMAttribute for writer to specify the max file size
  * maxFileSize has to be given by client or app.
  * If client/app doesn't know about this size they can
  * set the value to 0xFFFFFFFFFFFFFFFF (max of NvU64, 8 bytes)
  */
typedef struct
{
    NvU64 maxFileSize;  // max size for the file
}NvMMWriterAttrib_FileSize;

/** NvMMAttribute for writer to set the file name */
typedef struct
{
    NvString fileName;  //pFileName
    NvBool reInit;
}NvMMWriterAttrib_SplitTrack;

/** NvMMAttribute for writer to get the frame count */
typedef struct
{
    NvU64 audioFrameCount; // Count for encoded audio frames
    NvU64 videoFrameCount; // Count for encoded video frames
}NvMMWriterAttrib_FrameCount;

/** NvMMAttribute for writer to set the number of streams */
typedef struct
{
    NvS32 streamCount; //stream count
}NvMMWriterAttrib_StreamCount;

 // max trak duration it applies to both audio and video trak
typedef struct
{
    NvU64 maxTrakDuration;
}NvMMWriterAttrib_TrakDurationLimit;

typedef struct NvMMMP4WriterVideoConfig
{

    NvU32 width;
    NvU32 height;
    NvU32 frameRate;
    NvU8  level;
    NvU8  profile;

} NvMMMP4WriterVideoConfig;


typedef struct NvMMMP4WriterAmrConfig
{

    NvU32         mode_set;           // uint(16)
    NvU32         mode_change_period; // uint(8)
    NvU32         frames_per_sample;  // uint(8)
    NvMMNBAMREncodeType encodeType;

} NvMMMP4WriterAmrConfig;

typedef struct NvMMMP4WriterAACConfig
{

      NvU8 aacSamplingFreqIndex;
      NvU8 aacObjectType;
      NvU8 aacChannelNumber;

} NvMMMP4WriterAACConfig;

typedef struct NvMMMP4WriterAudioConfig
{

  // Union with Stream Config Audio/Video/Speech.
    union
    {
        NvMMMP4WriterAACConfig AACAudioConfig; //AAC Audio Config
        NvMMMP4WriterAmrConfig AMRConfig;//AMR  Config
    }NvMMWriter_AudConfig;

} NvMMMP4WriterAudioConfig;

/**
* Types of userdata
*/
typedef enum{
    NvMMMP4WriterDataInfo_Album = 0,                      // Album title and track number
    NvMMMP4WriterDataInfo_Author,                          // Media author name
    NvMMMP4WriterDataInfo_Classification,               // Media Classification
    NvMMMP4WriterDataInfo_Copyright,                     // copyright etc.
    NvMMMP4WriterDataInfo_Description,                  // Media Description
    NvMMMP4WriterDataInfo_Genre,                           // Media Genre
    NvMMMP4WriterDataInfo_Keywords,                     // Media Keywords
    NvMMMP4WriterDataInfo_LocationInfo,                // Media Location Info
    NvMMMP4WriterDataInfo_PerfName,                    // Media Performer Name
    NvMMMP4WriterDataInfo_Rating,                          // Media Rating
    NvMMMP4WriterDataInfo_Title,                             // Media Title
    NvMMMP4WriterDataInfo_Year,                             // Year When Media was recorded
    NvMMMP4WriterDataInfo_UserSpecific,                // Users own box type (Xtra etc..)
    NvMMMP4WriterDataInfo_Force32 = 0x7FFFFFFF
}NvMM3GPWriterUserDataTypes;

/**
  * dataType can be any of enum type in "NvMM3GPWriterUserDataTypes"
  *
  * "usrBoxType" is the name of the box type
  *     =======================
  *      Name            | Box Type
  *     =======================
  *      Album           | albm
  *      Author          | auth
  *      Classification | clsf
  *      copyright       | cprt
  *      Description    | dscp
  *      Genre           | gnre
  *      KeyWords     | kywd
  *      Location Info | loci
  *      Performer     | perf
  *      Rating           | rtng
  *      title              | titl
  *      year             | yrrc
  *      user specific | < Any 4 bytes type user needs>
  *     =================================
  * "sizeOfUserData" is size of total data in this box
  * "pUserData" pointer to the buffer holding user data that
  *  needs to be added to udta
  */

typedef struct NvMMWriterUserDataConfig
{
    NvMM3GPWriterUserDataTypes dataType;
    NvU8 usrBoxType[NVMM_MAX_ATOM_SIZE];
    NvU32 sizeOfUserData;
    NvU8 *pUserData;
} NvMMWriteUserDataConfig;



/** NvMMWriterInfo structure */
typedef struct
{
    NvMMStreamType StreamType; //streamType Object specifies the type of compression.
    NvMMBlockType BlockType;
    NvU32 avg_bitrate;
    NvU32 max_bitrate;
    // Used for setting the max number of frames.
    NvBool setFrames;
    //Memory required for audio = 7 * sizeof(NvU32) * numberOfFrames
    //Memory required for video = 9 * sizeof(NvU32) * numberOfFrames
    NvU32 numberOfFrames;
    // Union with Stream Config Audio/Video/Speech.
    union
    {
        NvMMMP4WriterVideoConfig VideoConfig;
        NvMMMP4WriterAudioConfig AudioConfig;
    }NvMMWriter_Config;

} NvMMWriterStreamConfig;

/** NvMMAttribute for writer to set the file ext */
typedef struct
{
    NvString szURI; //pFileExt
}NvMMWriterAttrib_FileExt;

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NVMM_WRITER_H


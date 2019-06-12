/*
* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/
#ifndef INCLUDED_NVMM_STREAMTYPES_H
#define INCLUDED_NVMM_STREAMTYPES_H

#if defined(__cplusplus)
extern "C"
{
#endif

/** WARNING **
 * MUST ADD NEW TYPES TO THE END OF THE APPROPRIATE SECTION
 */
typedef enum
{
    NvMMStreamType_OTHER = 0x0,   // Other, not supported tracks

    /* Audio codecs */
    NvMMStreamType_AUDIOBASE = 0x001,
    NvMMStreamType_MP3,    // MP3 track
    NvMMStreamType_WAV,    // Wave track
    NvMMStreamType_AAC,    // Mpeg-4 AAC
    NvMMStreamType_AACSBR, // Mpeg-4 AAC SBR
    NvMMStreamType_BSAC,    // Mpeg-4 ER BSAC track
    NvMMStreamType_QCELP,   //13K (QCELP) speech track
    NvMMStreamType_WMA,    // WMA track
    NvMMStreamType_WMAPro,  // WMAPro Track
    NvMMStreamType_WMALSL,  //WMA Lossless
    NvMMStreamType_WMAV,    // WMA Voice Stream
    NvMMStreamType_WAMR,   // Wide band AMR track
    NvMMStreamType_NAMR,   // Narrow band AMR track
    NvMMStreamType_OGG,    // OGG Vorbis
    NvMMStreamType_A_LAW,  // G711  ALaw
    NvMMStreamType_U_LAW,  // G711  ULaw
    NvMMStreamType_AC3,    // AC3 Track
    NvMMStreamType_MP2,    // MP3 track
    NvMMStreamType_EVRC,    // Enhanced Variable Rate Coder
    NvMMStreamType_ADPCM,    // ADPCM track
    NvMMStreamType_NELLYMOSER,    // Nellymoser track
    NvMMStreamType_Vorbis,    // Vorbis Audio track
    NvMMStreamType_UnsupportedAudio, //Unsupported Audio Track

    NvMMStreamType_VIDEOBASE = 0x100,
    NvMMStreamType_MPEG4,  // MPEG4 track
    NvMMStreamType_H264,   // H.264 track
    NvMMStreamType_H263,   // H263 track
    NvMMStreamType_WMV,    // WMV9 track
    NvMMStreamType_WMV7,   // WMV 7 track
    NvMMStreamType_WMV8,   // WMV 8 track
    NvMMStreamType_JPG,    // jpeg file
    NvMMStreamType_BMP,    // bmp type
    NvMMStreamType_TIFF,   // TIFF type
    NvMMStreamType_MJPEGA, // M-JPEG Format A
    NvMMStreamType_MJPEGB, // M-JPEG Format B
    NvMMStreamType_MJPEG,  // M-JPEG Format
    NvMMStreamType_MPEG2V,   //MPEG2 track
    NvMMStreamType_Dummy1,
    NvMMStreamType_MS_MPG4,   //"MPG4" is based on early draft of MPEG4 standard.
    NvMMStreamType_H264Ext,   // H.264 track, same effect as NvMMStreamType_H264
    NvMMStreamType_MPEG4Ext,  // MPEG4 track, same effect as NvMMStreamType_MPEG4
    NvMMStreamType_Dummy2,
    NvMMStreamType_Dummy3,
    NvMMStreamType_UnsupportedVideo, //Unsupported Video

    NvMMStreamType_Force32 = 0x7FFFFFFF
} NvMMStreamType;

#define NVMM_ISSTREAMAUDIO(x) ((x) > NvMMStreamType_AUDIOBASE && (x) < NvMMStreamType_VIDEOBASE)
#define NVMM_ISSTREAMVIDEO(x) ((x) > NvMMStreamType_VIDEOBASE)

#if defined(__cplusplus)
}
#endif

#endif


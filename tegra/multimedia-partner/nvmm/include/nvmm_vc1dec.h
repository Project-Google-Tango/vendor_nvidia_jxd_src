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
* @brief <b>NVIDIA Driver Development Kit:
*           NvMMBlock VC1 video decoder specific structure</b>
*
* @b Description: Declares Interface for VC1 video decoder.
*/
#ifndef INCLUDED_NVMM_VIDEODEC_VC1_H
#define INCLUDED_NVMM_VIDEODEC_VC1_H

#include "nvcommon.h"

#ifdef __cplusplus
extern "C" {
#endif
    /**
    * @brief Sequence info
    */
    typedef enum
    {
        // If set, then the stream is encoded at constant bitrate
        NvMMBlockVideoDecVC1_ConstantBitrate = 0x1,
        // If set, then the stream can have frames with different resolution
        // Resolution change is only allowed on I pictures
        NvMMBlockVideoDecVC1_MultiResolution,

        NvMMBlockVideoDecVC1_Force32 = 0x7FFFFFFF
    } NvMMBlockVideoDecVC1_Sequence;

    /**
    * @brief Defines the structure for holding VC1 specific information.
    */
    typedef struct
    {
        // Holds info whether bitrate is fixed or variable
        NvMMBlockVideoDecVC1_Sequence  SequenceInfo;
        // Peak transmission rate in bits per second
        NvU32  HRDRate;
        // Buffer size in milliseconds.
        NvU32   HRDBufferSize;
        // Rounding frame rate of the encoded clip
        NvU32  FrameRate;
    } NvMMBlockVideoDecBitStreamProperty_VC1;

    typedef struct { 
        NvU32 biSize; 
        NvU32 biWidth; 
        NvU32 biHeight; 
        NvU16 biPlanes; 
        NvU16 biBitCount; 
        NvU32 biCompression; 
        NvU32 biSizeImage; 
        NvU32 biXPelsPerMeter; 
        NvU32 biYPelsPerMeter; 
        NvU32 biClrUsed; 
        NvU32 biClrImportant; 
    } NvBITMAPINFOHEADER;


    /* NvMMWMVCodecInfo parameter struct to pass codec info to client*/
    typedef struct {
        NvU32 nBufferSize; // specifies to the client how much size required to allocate the buffer
        NvS8 *pClientBuffer; // to fill the codecinfo
  NvBITMAPINFOHEADER nBitmapInfo;// To fill the bitmapinfo of the vc1
    } NvMMWMVCodecInfo;




#ifdef __cplusplus
}
#endif

#endif // INCLUDED_NVMM_VIDEODEC_VC1_H

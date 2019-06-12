/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _FCAMDNG_H_
#define _FCAMDNG_H_

extern "C" {
#include <cutils/str_parms.h>
}

struct FCamFrameParam
{
    unsigned int structsize;        //!< size of this struct for versioning
    const char*  buffer;            //!< 16-bit Bayer pixel data
    unsigned int stride;            //!< Stride of frame (line stride in bytes)
    unsigned int width;             //!< Width of frame
    unsigned int height;            //!< Height of frame
    unsigned int fourcc;            //!< FOURCC code defining RGGB order
    unsigned int bpp;               //!< Bits per channel (e.g. 10, 12, 16)
    unsigned int timeSeconds;       //!< Capture time in seconds part
    unsigned int timeMicroseconds;  //!< Capture time in microseconds part
    float        gain;              //!< sensor gain
    int          exposure;          //!< microseconds
    int          whiteBalance;      //!< Whitebalance in Kelvin
    float        wbNeutral[3];      //!< Whitebalance in sRGB
    float        rawToRGB3000K[12]; //!< 3x4 affine matrix, 3000K lighting
    float        rawToRGB6500K[12]; //!< 3x4 affine matrix, 6500K lighting
    const char*  manufacturer;      //!< 0-terminated device manufacturer name
    const char*  model;             //!< 0-terminated device model name
};

/** Encode raw Bayer raw buffer to DNG.
    @param param Required parameters
    @param tags Optional tags (char* values), will be passed 1:1 as FCam Frame tags
    @param buffer Returns buffer, needs to be freed with later FCamFreeBuffer()
    @param size Returns size of buffer in bytes
    @return 0 on success, or !=0 on error
*/
int FCamSaveDNG(const FCamFrameParam* param, struct str_parms* tags, char** buffer, unsigned* size);

/** Free buffer */
void FCamFreeBuffer(char* buffer);

#endif


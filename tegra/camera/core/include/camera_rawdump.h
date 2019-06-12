/*
 * Copyright (c) 2009-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>Camera Raw Dump header </b>
 */

#ifndef INCLUDED_CAMERA_RAWDUMP_H
#define INCLUDED_CAMERA_RAWDUMP_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"

/**
 * CameraRawDump gives the header format for the raw bayer dumps.
 * The header contains undocumented fields for debug and future use.
 */
#define CAMERA_RAWDUMP_MAGIC_NUMBER 1
#define CAMERA_RAWDUMP_VERSION      1
#define CAMERA_RAWDUMP_BAYERSTART   0xCAFEFEED

#define BAYER_ORDERING(a,b,c,d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))
#define RAWDUMP_BAYER_ORDERING_RGGB BAYER_ORDERING('R', 'G', 'G', 'B')
#define RAWDUMP_BAYER_ORDERING_BGGR BAYER_ORDERING('B', 'G', 'G', 'R')
#define RAWDUMP_BAYER_ORDERING_GRBG BAYER_ORDERING('G', 'R', 'B', 'G')
#define RAWDUMP_BAYER_ORDERING_GBRG BAYER_ORDERING('G', 'B', 'R', 'G')
typedef struct CameraRawDumpRec
{
    unsigned long MagicNumber; // unique identifier
    unsigned long Version;     // for future compatibility
    NvSize Size;               // image width and height
    unsigned int BayerOrdering; // example: 'R', 'G', 'G', 'B'
    unsigned long long SensorGUID;  // unique id for the sensor for reference
    // Undocumented header data
    unsigned long HeaderData[74659]; // calculated from private header size
    unsigned long BayerStartMark; // CAFEFEED : Marker for start of data
} CameraRawDump;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_CAMERA_RAWDUMP_H

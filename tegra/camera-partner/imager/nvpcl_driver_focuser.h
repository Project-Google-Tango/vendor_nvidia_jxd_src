/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_DRIVER_FOCUSER_H
#define PCL_DRIVER_FOCUSER_H

#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Holds ODM imager modes for focuser device.
 */
typedef struct NvPclFocuserObjectRec
{
    NvU32 Version;
    NvU32 Id;

    // Used for actual/physical position
    // Drivers don't actually care about working (non-deadzone) ranges
    NvS32 PositionMin;
    NvS32 PositionMax;

    // EXIF only
    NvF32 FocalLength;
    NvF32 HyperFocal;
    NvF32 MaxAperture;
    NvF32 FNumber;

    // Importing Static
    NvF32 MinFocusDistance;
} NvPclFocuserObject;

typedef struct NvPclFocuserContextRec
{
    NvS32 DelayedPositionReq;
} NvPclFocuserContext;

typedef struct NvPclFocuserObjectRec *NvPclFocuserObjectHandle;

#if defined(__cplusplus)
}
#endif




#endif  //PCL_DRIVER_FOCUSER_H


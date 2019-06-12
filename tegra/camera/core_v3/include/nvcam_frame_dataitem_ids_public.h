/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVCAM_FRAME_DATA_ITEM_IDS_PUBLIC_H
#define INCLUDED_NVCAM_FRAME_DATA_ITEM_IDS_PUBLIC_H

#include "nvmm.h"

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * List of data item identifiers which are public.
 * Clients can use these identifiers to query data
 * items associated with a frame.
 */
typedef enum
{
    // Android V3 properties + NvCam public control properties.
    NvCamFrameDataItemID_CamProps_Controls = 0x1,
    // Android V3 properties + NvCam public dynamic properties.
    NvCamFrameDataItemID_CamProps_Dynamic,
    // Android V3 properties + NvCam public static properties.
    // Static properties does not need to be stored in a frame
    // data. They can be queried directly via camera interface.
    NvCamFrameDataItemID_CamProps_Static,
    NvCamFrameDataItemID_CaptureRequest,
    NvCamFrameDataItemIDPub_SIZE,
    NvCamFrameDataItemIDPub_Force32 = 0x7FFFFFF
} NvCamFrameDataItemIDPublic;


#ifdef __cplusplus
}
#endif
#endif // INCLUDED_NVCAM_FRAME_DATA_ITEM_IDS_PUBLIC_H


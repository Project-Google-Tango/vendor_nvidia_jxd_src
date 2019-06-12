/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM USB Camera APIs</b>
 *
 * @b Description: Declares Interface for NvMM USB Camera APIs.
 */

#ifndef INCLUDED_NVMM_USBCAMERA_H
#define INCLUDED_NVMM_USBCAMERA_H

/** @defgroup nvmm_usbcamera USB Camera API
 *
 * NvMM USB Camera is the source block of still and video image data.
 * It provides the functionality needed for capturing USB camera provided
 * image buffers in memory for further processing before
 * encoding or previewing. (For instance, one may wish
 * to scale or rotate the buffer before encoding or previewing.)
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_surface.h"
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_camera_types.h"
#include "nvcamera_isp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines stream index that will used in nvmm usb camera block
 */
typedef enum
{
    /* index of output Preview stream */
    NvMMUSBCameraStreamIndex_OutputPreview = 0x0,

    /* index of output stream */
    NvMMUSBCameraStreamIndex_Output,

    NvMMUSBCameraStreamIndex_Force32 = 0x7FFFFFFF

} NvMMUSBCameraStreamIndex;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_USBCAMERA_H

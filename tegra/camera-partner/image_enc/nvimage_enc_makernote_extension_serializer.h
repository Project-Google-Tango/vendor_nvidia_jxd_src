/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVIMAGE_ENC_MAKERNOTE_EXTENSION_SERIALIZER_H
#define __NVIMAGE_ENC_MAKERNOTE_EXTENSION_SERIALIZER_H

#include "nvcamera_makernote_extension.h"
#include "nvimage_enc_jds.h"

#ifdef __cplusplus
extern "C" {
#endif

NvError NvCameraInsertToJDS(NvCameraJDS *db, NvCameraMakerNoteExtension *nvCameraMakerNoteExtension);

#ifdef __cplusplus
}
#endif

#endif

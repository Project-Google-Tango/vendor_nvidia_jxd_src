/* Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 *
 * This is the NVIDIA OpenMAX AL common macros and structures
 * definition.
 */

#ifndef NVOMXAL_COMMON_H
#define NVOMXAL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif


/* New format change event which notifies audio format change event
 * and event is followed by XADataFormat_PCM structure.
 * */
#define NVXA_ANDROID_FORMATCHANGE_ITEMDATA_NVAUDIO ((XAuint32) 0x00000100)

/* New ABQ event which provides timestamp along with data.
 * Below event is followed by 64 integer which represents
 * timestamp.
 * */
#define NVXA_ANDROIDBUFFERQUEUEEVENT_PROCESSED_WITHTS ((XAuint32) 0x00001000)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_COMMON_H */

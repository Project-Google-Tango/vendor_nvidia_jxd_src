/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCAMERA_ERROR_H
#define INCLUDED_NVCAMERA_ERROR_H

#if defined(__cplusplus)
extern "C"
{
#endif

// Check the error. If it's an error, show and return the error.
#define NV_CAM_CHECK_SHOW_ERROR(_expr) NV_CHECK_ERROR(NV_SHOW_ERROR(_expr))

// Check the error. If it's an error, show the error and goto fail.
#define NV_CAM_CHECK_SHOW_ERROR_CLEANUP(_expr)  \
            NV_CHECK_ERROR_CLEANUP(NV_SHOW_ERROR(_expr))

// Allocate memory. If it fails, show the error and goto fail.
#define NV_CAM_ALLOC_AND_ERROR_CLEANUP(_ptr , _size) \
    do \
    { \
        (_ptr) = NvOsAlloc((_size));\
        if (!(_ptr)) \
        { \
            NvOsDebugPrintf("Failed allocating memory of size %d bytes at %s:%d\n", \
                            _size,__FILE__,__LINE__); \
            goto fail; \
        } \
    } while (0)

#if defined(__cplusplus)
}
#endif


#endif // INCLUDED_NVCAMERA_ERROR_H

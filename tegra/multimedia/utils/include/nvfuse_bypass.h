/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_FUSE_BYPASS
#define INCLUDED_FUSE_BYPASS

#if defined(__cplusplus)
extern "C"
{
#endif

NvError
NvVP8FuseSet(
    NvRmDeviceHandle hRmDev,
    NvU32 desired_fv);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_FUSE_BYPASS


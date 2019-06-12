/*
 * Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_HDCP_UP_PRIV_H
#define INCLUDED_HDCP_UP_PRIV_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define HDCP_GLOB_SIZE (((3 * 64) + (1 * 64) + (40 * 56)) / 8)
#define HDCP_KEY_OFFSET (((3 * 64) + (1 * 64)) / 8)

/**
 * Returns a pointer to a decrypted glob for Upstream authentication. The
 * pointer must be de-allocated with HdcpReleaseGlob.
 *
 * If this is called multiple times without first releasing the glob, then
 * this will fail (return NULL).
 *
 * This must decrypt the key glob into secure memory. If the memory cannot be
 * mapped, then this must return NULL.
 *
 * @param size Pointer to the size of the glob
 */
typedef void * (*HdcpGetGlobType)( __u32 *size );

/**
 * De-allocates/clears glob memory.
 */
typedef void (*HdcpReleaseGlobType)( void *glob );

#if defined(__cplusplus)
}
#endif

#endif

/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxMutex.h */

#ifndef NVX_MUTEX_H
#define NVX_MUTEX_H

#include <OMX_Core.h>
#include <common/NvxError.h>

/** Create a mutex, returning the handle to it in phMutexHandle */
OMX_ERRORTYPE NvxMutexCreate( OMX_OUT OMX_HANDLETYPE *phMutexHandle );

/** Lock a mutex */
OMX_ERRORTYPE NvxMutexLock( OMX_IN OMX_HANDLETYPE hMutexHandle);

/** Unlock a mutex */
OMX_ERRORTYPE NvxMutexUnlock( OMX_IN OMX_HANDLETYPE hMutexHandle);

/** Destroy the mutex */
OMX_ERRORTYPE NvxMutexDestroy( OMX_IN OMX_HANDLETYPE hMutexHandle);

#endif /* NVX_MUTEX_H */

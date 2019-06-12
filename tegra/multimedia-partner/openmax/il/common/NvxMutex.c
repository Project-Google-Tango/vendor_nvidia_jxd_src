/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "common/NvxMutex.h"

#include "nvos.h"

static int m_bHaveThreading = -1;

OMX_ERRORTYPE NvxMutexCreate(OMX_OUT OMX_HANDLETYPE *phMutexHandle)
{
    NvOsMutexHandle hMutex;
    NvError eErr;
     
    eErr = NvOsMutexCreate(&hMutex);

    if (eErr != NvSuccess) {
        *phMutexHandle = (void *)0;
        if (m_bHaveThreading == 1)
            return OMX_ErrorInsufficientResources;
        else {
            m_bHaveThreading = 0;
            return NVX_SuccessNoThreading;
        }
    }

    m_bHaveThreading = 1;
    *phMutexHandle = (void *)hMutex;
    return OMX_ErrorNone;
}

/** Lock a mutex */
OMX_ERRORTYPE NvxMutexLock(OMX_IN OMX_HANDLETYPE hMutexHandle)
{
    if (m_bHaveThreading)
    {
        if (hMutexHandle == 0)
            return OMX_ErrorBadParameter;
        NvOsMutexLock((NvOsMutexHandle)hMutexHandle );
    }

    return OMX_ErrorNone;
}

/** Unlock a mutex */
OMX_ERRORTYPE NvxMutexUnlock(OMX_IN OMX_HANDLETYPE hMutexHandle)
{
    if (m_bHaveThreading)
    {
        if (hMutexHandle == 0)
            return OMX_ErrorBadParameter;
        NvOsMutexUnlock((NvOsMutexHandle)hMutexHandle );
    }

    return OMX_ErrorNone;
}

/** Destroy the mutex */
OMX_ERRORTYPE NvxMutexDestroy(OMX_IN OMX_HANDLETYPE hMutexHandle)
{
    if (m_bHaveThreading)
    {
        if (hMutexHandle == 0)
            return OMX_ErrorBadParameter;
        NvOsMutexDestroy((NvOsMutexHandle)hMutexHandle);
    }

    return OMX_ErrorNone;
}

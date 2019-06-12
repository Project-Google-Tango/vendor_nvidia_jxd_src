/* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 *
 * This is the NVIDIA OpenMAX AL BufferQueue extensions
 * interface.
 *
 */

#ifndef NVOMXAL_BUFFERQUEUEEXTENSIONS_H_
#define NVOMXAL_BUFFERQUEUEEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Buffer Queue Interface                                                    */
/*---------------------------------------------------------------------------*/

extern XA_API const XAInterfaceID XA_IID_NVBUFFERQUEUE;

struct XANvBufferQueueItf_;
typedef const struct XANvBufferQueueItf_ * const * XANvBufferQueueItf;

typedef void (XAAPIENTRY *xaNvBufferQueueCallback)(
    XANvBufferQueueItf caller,
    void *pContext
);

/** Buffer queue state **/

typedef struct XANvBufferQueueState_ {
    XAuint32 count;
    XAuint32 playIndex;
} XANvBufferQueueState;


struct XANvBufferQueueItf_ {
    XAresult (*Enqueue) (
        XANvBufferQueueItf self,
        const void *pBuffer,
        XAuint32 size
    );
    XAresult (*Clear) (
        XANvBufferQueueItf self
    );
    XAresult (*GetState) (
        XANvBufferQueueItf self,
        XANvBufferQueueState *pState
    );
    XAresult (*RegisterCallback) (
        XANvBufferQueueItf self,
        xaNvBufferQueueCallback callback,
        void* pContext
    );
};

/*---------------------------------------------------------------------------*/
/* Buffer Queue Data Locator                                                 */
/*---------------------------------------------------------------------------*/

/** Addendum to Data locator macros  */
#define XA_DATALOCATOR_NVBUFFERQUEUE       ((XAuint32) 0x80000FFF)

/** Buffer Queue-based data locator definition,
 *  locatorType must be XA_DATALOCATOR_NVBUFFERQUEUE */
typedef struct XADataLocator_NvBufferQueue_ {
    XAuint32    locatorType;
    XAuint32    numBuffers;
} XADataLocator_NvBufferQueue;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_BUFFERQUEUEEXTENSIONS_H_ */

/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

/**
 * \file nvmedia_eglstream.h
 * \brief The NvMedia EGL Stream API
 *
 * This file contains the \ref eglstream_api "EGL Stream API".
 */

#ifndef _NVMEDIA_EGLSTREAM_H
#define _NVMEDIA_EGLSTREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmedia.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nv.h>

/**
 * \defgroup api_eglstream EGL Stream API
 *
 * The EGL Stream API encompasses all NvMedia EGL Stream related functionality
 *
 * @{
 */

/** \brief Major Version number */
#define NVMEDIA_EGLSTREAM_VERSION_MAJOR   1
/** \brief Minor Version number */
#define NVMEDIA_EGLSTREAM_VERSION_MINOR   0

/**
 * \defgroup eglstream_api EGL Stream API
 *
 * EGL Stream support enables interaction with EGL based surfaces.
 *
 * @{
 */

/**
 * \hideinitializer
 * \brief Infinite time-out for NvMedia EGL functions
 */
#define NVMEDIA_EGL_STREAM_TIMEOUT_INFINITE  0xFFFFFFFF

/**
 * \brief A handle representing an EGL stream producer object.
 */
typedef struct NvMediaEGLStreamProducer {
    /*! Input surface type */
    NvMediaSurfaceType type;
    /*! Input surface width */
    EGLint width;
    /*! Input surface height */
    EGLint height;
} NvMediaEGLStreamProducer;

/**
 * \brief Create an EGL stream producer object.
 * \param[in] device The device's resource manager connection is used for the allocation.
 * \param[in] dpy EGL display handle
 * \param[in] stream EGL stream handle
 * \param[in] type Type of the used surface. The supported types are:
 * \n \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin
 * \param[in] width Width of the EGL stream producer
 * \param[in] height Height of the EGL stream producer
 * \return \ref NvMediaEGLStreamProducer The new EGL stream producer's handle or NULL if unsuccessful.
 */
NvMediaEGLStreamProducer *
NvMediaEglStreamProducerCreate(
    NvMediaDevice *device,
    EGLDisplay dpy,
    EGLStreamKHR stream,
    NvMediaSurfaceType type,
    EGLint width,
    EGLint height
);

/**
 * \brief Destroy an EGL stream producer object.
 * \param[in] producer The EGL stream producer to destroy.
 * \return void
 */
void
NvMediaEglStreamProducerDestroy(
    NvMediaEGLStreamProducer *producer
);

/**
 * \brief Post a surface to be sent to an EGL stream connection
 * \param[in] producer The EGL stream producer object to use
 * \param[in] surface The surface to send
 * \param[in] timeStamp The desired time to duplay the surface or
 *   NULL if immedate displaying is required.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 * \n \ref NVMEDIA_STATUS_ERROR
 */
NvMediaStatus
NvMediaEglStreamProducerPostSurface(
    NvMediaEGLStreamProducer *producer,
    NvMediaVideoSurface *surface,
    NvMediaTime *timeStamp
);

/**
 * \brief Retreive a surface that was sent to an EGL stream using the
 * \ref NvMediaEglStreamProducerPostSurface function and the EGL stream has processed
 * and released it.
 * \param[in] producer The EGL stream producer object to use
 * \param[in] surface The surface pointer to receive
 * \param[in] millisecondTimeout The desired time-out. For infinite time-out
 *  use \ref NVMEDIA_EGL_STREAM_TIMEOUT_INFINITE.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 * \n \ref NVMEDIA_STATUS_TIMED_OUT
 * \n \ref NVMEDIA_STATUS_ERROR
 */
NvMediaStatus
NvMediaEglStreamProducerGetSurface(
    NvMediaEGLStreamProducer *producer,
    NvMediaVideoSurface **surface,
    unsigned int millisecondTimeout
);

/**
 * \brief A handle representing an EGL stream consumer object.
 */
typedef struct NvMediaEGLStreamConsumer {
    /*! Input surface type */
    NvMediaSurfaceType type;
} NvMediaEGLStreamConsumer;

/**
 * \brief Create an EGL stream consumer object.
 * \param[in] device The device's resource manager connection is used for the allocation.
 * \param[in] dpy EGL display handle
 * \param[in] stream EGL stream handle
 * \param[in] type Type of the used surface. The supported types are:
 * \n \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin
 * \return \ref NvMediaEGLStreamConsumer The new EGL stream consumer's handle or NULL if unsuccessful.
 */
NvMediaEGLStreamConsumer *
NvMediaEglStreamConsumerCreate(
    NvMediaDevice *device,
    EGLDisplay dpy,
    EGLStreamKHR stream,
    NvMediaSurfaceType type
);

/**
 * \brief Destroy an EGL stream consumer object.
 * \param[in] consumer The EGL stream consumer to destroy.
 * \return void
 */
void
NvMediaEglStreamConsumerDestroy(
    NvMediaEGLStreamConsumer *consumer
);

/**
 * \brief Acquire a surface that was sent by an EGL stream producer.
 * \param[in] consumer The EGL stream consumer object to use
 * \param[in] surface The surface pointer to receive
 * \param[in] millisecondTimeout The desired time-out. For infinite time-out
 *  use \ref NVMEDIA_EGL_STREAM_TIMEOUT_INFINITE.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 * \n \ref NVMEDIA_STATUS_TIMED_OUT
 * \n \ref NVMEDIA_STATUS_ERROR
 */
NvMediaStatus
NvMediaEglStreamConsumerAcquireSurface(
    NvMediaEGLStreamConsumer *consumer,
    NvMediaVideoSurface **surface,
    unsigned int millisecondTimeout,
    NvMediaTime *timeStamp
);

/**
 * \brief Release a surface that was received by the
 *   \ref NvMediaConsumerSurfaceAcquire function.
 * \param[in] consumer The EGL stream consumer object to use
 * \param[in] surface The surface to release
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 * \n \ref NVMEDIA_STATUS_TIMED_OUT
 * \n \ref NVMEDIA_STATUS_ERROR
 */
NvMediaStatus
NvMediaEglStreamConsumerReleaseSurface(
    NvMediaEGLStreamConsumer *consumer,
    NvMediaVideoSurface* surface
);

/** \brief Deprecated function for legacy applications */
#define NvMediaVideoSurfaceStreamProducerCreate NvMediaEglStreamProducerCreate
/** \brief Deprecated function for legacy applications */
#define NvMediaVideoSurfaceStreamProducerDestroy NvMediaEglStreamProducerDestroy
/** \brief Deprecated function for legacy applications */
#define NvMediaPostSurface NvMediaEglStreamProducerPostSurface
/** \brief Deprecated function for legacy applications */
#define NvMediaGetSurface NvMediaEglStreamProducerGetSurface
/** \brief Deprecated function for legacy applications */
#define NvMediaEglConsumerCreate NvMediaEglStreamConsumerCreate
/** \brief Deprecated function for legacy applications */
#define NvMediaEglConsumerDestroy NvMediaEglStreamConsumerDestroy
/** \brief Deprecated function for legacy applications */
#define NvMediaEglConsumerSurfaceAcquire NvMediaEglStreamConsumerAcquireSurface
/** \brief Deprecated function for legacy applications */
#define NvMediaEglConsumerSurfaceRelease NvMediaEglStreamConsumerReleaseSurface

/*@}*/
/**
 * \defgroup history_eglstream History
 * Provides change history for the NvMedia EGL Stream API.
 *
 * \section history_eglstream Version History
 *
 * <b> Version 1.0 </b> March 18, 2013
 * - Initial release
 *
 */
/*@}*/

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVMEDIA_EGLSTREAM_H */

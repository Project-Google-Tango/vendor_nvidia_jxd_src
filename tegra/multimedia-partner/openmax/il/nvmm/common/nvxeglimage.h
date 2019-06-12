#ifndef __OMX_EGL_IMAGE_H__
#define __OMX_EGL_IMAGE_H__

/*
 * Copyright (c) 2005 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation.  
 * Any use, reproduction, disclosure or distribution of this software and
 * related documentation without an express license agreement from NVIDIA
 * Corporation is strictly prohibited.
 */


#include <OMX_Core.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "eglapiinterface.h"
#include "nvassert.h"
#include "common/NvxHelpers.h"

/*
 * EGL image handles returned by Android's EGL layer are not compatible
 * with OMX directly, as Android wraps other information with the handle.
 * This is an unwrapping function defined in Android's EGL for client APIs.
 */
#if defined(ANDROID)
// TODO [arasmus 2010-03-16]: Remove current context function once
// the vendor string query function is supported in Eclair.
// Problem is that OMX doesn't have an EGL context
typedef EGLImageKHR (*EglGetImageForCurrentContext)(
        EGLImageKHR image);

typedef EGLImageKHR (*EglGetImageForImplementation)(
        const char* vendor,
        EGLImageKHR image);
#endif

/*
 * NvxEglImageSiblingHandle -- opaque pointer to OMX's internal representation
 *      of an EGL image sibling.  Essentially, this is OpenMAX's version of a
 *      reference to an EGL image.
 */
typedef struct NvxEglImageSiblingStruct * NvxEglImageSiblingHandle;

/*
 * NvxEglCreateImageSibling -- create an NvxEglImageSibling from an EGL image.
 */
OMX_ERRORTYPE
NvxEglCreateImageSibling(
        EGLImageKHR eglImage,
        NvxEglImageSiblingHandle* hSibling);

/*
 * NvxEglFreeImageSibling -- free resources associated with an
 *      NvxEglImageSibling.
 */
void
NvxEglFreeImageSibling(NvxEglImageSiblingHandle hOmxEglImgSibling);

/*
 * NvxEglImageGetRmSurface -- get the RM surface associated with an
 *      NvxEglImageSibling.
 */
void 
NvxEglImageGetNvxSurface(
        const NvxEglImageSiblingHandle hOmxEglImgSibling,
        NvxSurface *pOutSurface);

/*
 * NvxEglStretchBlitToImage -- perform blit from an RmSurface to an eglImage.
 *      This will do color-space-conversion & scaling.  If waitForFinish is
 *      TRUE, the function will not return until the operation completes, and
 *      pFence is ignored.  If waitForFinish is FALSE, an RmFence is returned in
 *      pOutFence.
 * 
 *      TODO: Need invert and rotate params?
 */
OMX_ERRORTYPE 
NvxEglStretchBlitToImage(
        NvxEglImageSiblingHandle hDestOmxEglImgSibling,
        NvxSurface *pSrcSurface,
        NvBool waitForFinish,
        NvRmFence *pOutFence,
        OMX_MIRRORTYPE eMirrorType);

/*
 * NvxEglStretchBlitFromImage -- just like NvxEglStretchBlitToImage, except
 *      backwards.
 */
OMX_ERRORTYPE 
NvxEglStretchBlitFromImage(
        NvxEglImageSiblingHandle hSrcOmxEglImgSibling,
        NvxSurface *pDestSurface,
        NvBool waitForFinish,
        NvRmFence *pOutFence);

#endif // __OMX_EGL_IMAGE_H__

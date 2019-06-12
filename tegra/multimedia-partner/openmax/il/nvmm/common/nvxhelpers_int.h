/* Copyright (c) 2010 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVXHELPERS_INT_H_
#define NVXHELPERS_INT_H_

#include "NvxHelpers.h"

NvError NvxAllocateOverlayDM(
    NvU32 *pWidth,
    NvU32 *pHeight,
    NvColorFormat eFormat,
    NvxOverlay *pOverlay,
    ENvxVidRendType eType,
    NvBool bDisplayAtRequestRect,
    NvRect *pDisplayRect,
    NvU32  nLayout);
void NvxReleaseOverlayDM(NvxOverlay *pOverlay);
void NvxRenderSurfaceToOverlayDM(NvxOverlay *pOverlay,
                                 NvxSurface *pSrcSurface,
                                 OMX_BOOL bWait);

OMX_U32 NvxGetRotationDM(void);
NvError NvxUpdateOverlayDM(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                           NvBool bSetSurface);
void NvxShutdownDM(void);

void NvxSmartDimmerEnableDM(NvxOverlay *pOverlay, NvBool enable);
OMX_ERRORTYPE NvxStereoRenderingEnableDM(NvxOverlay *pOverlay);

#ifdef USE_DC_DRIVER
/*
 * DC driver related funtions
 *
 * NvxAllocateOverlayDC:: gets the DC handle, and opens a windows
 * NvxUpdateOverlayDC:: Holds the flipping window dimension
 * NvxRenderSurfaceToOverlayDC:: flips the source sutface on created window
 * NvxReleaseOverlayDC:: close the window, and releases the DC handle
 * */

OMX_ERRORTYPE  NvxAllocateOverlayDC(NvU32 *pWidth,
                                    NvU32 *pHeight,
                                    NvColorFormat eFormat,
                                    NvxOverlay *pOverlay,
                                    ENvxVidRendType eType,
                                    NvBool bDisplayAtRequestRect,
                                    NvRect *pDisplayRect,
                                    NvU32 nLayout);

OMX_ERRORTYPE  NvxUpdateOverlayDC(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                                  NvBool bSetSurface);

OMX_ERRORTYPE  NvxRenderSurfaceToOverlayDC(NvxOverlay *pOverlay,
                                 NvxSurface *pSrcSurface,
                                 OMX_BOOL bWait);

OMX_ERRORTYPE NvxReleaseOverlayDC(NvxOverlay *pOverlay);

#endif

#if USE_ANDROID_NATIVE_WINDOW

#ifdef __cplusplus
extern "C" {
#endif

#include "nvgr.h"

NvError NvxAllocateOverlayANW(
    NvU32 *pWidth,
    NvU32 *pHeight,
    NvColorFormat eFormat,
    NvxOverlay *pOverlay,
    ENvxVidRendType eType,
    NvBool bDisplayAtRequestRect,
    NvRect *pDisplayRect,
    NvU32  nLayout);
void NvxReleaseOverlayANW(NvxOverlay *pOverlay);
void NvxRenderSurfaceToOverlayANW(NvxOverlay *pOverlay,
                                  NvxSurface *pSrcSurface,
                                  OMX_BOOL bWait);

OMX_U32 NvxGetRotationANW(void);
NvError NvxUpdateOverlayANW(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                            NvBool bSetSurface);
void NvxSmartDimmerEnableANW(NvxOverlay *pOverlay, NvBool enable);

void NvxWaitFences(const NvRmFence *fences, NvU32 numFences);
int NvxGetPreFenceANW(buffer_handle_t handle);
void NvxWaitPreFenceANW(buffer_handle_t handle, int usage);
#define OMX_MAX_FENCES NVDDK_2D_MAX_FENCES
void NvxGetPreFencesANW(buffer_handle_t handle,
                        NvRmFence fences[OMX_MAX_FENCES],
                        NvU32 *numFences, int usage);

void NvxPutWriteNvFence(buffer_handle_t handle, const NvRmFence *fence);

#ifdef __cplusplus
}
#endif

#endif

#ifndef USE_ANDROID_CAMERA_PREVIEW
#define USE_ANDROID_CAMERA_PREVIEW 0
#endif

#if USE_ANDROID_CAMERA_PREVIEW

#ifdef __cplusplus
extern "C" {
#endif

NvError NvxAllocateOverlayAndroidCameraPreview(
    NvU32 *pWidth,
    NvU32 *pHeight,
    NvColorFormat eFormat,
    NvxOverlay *pOverlay,
    ENvxVidRendType eType,
    NvBool bDisplayAtRequestRect,
    NvRect *pDisplayRect,
    NvU32  nLayout);
void NvxReleaseOverlayAndroidCameraPreview(NvxOverlay *pOverlay);
void NvxRenderSurfaceToOverlayAndroidCameraPreview(NvxOverlay *pOverlay,
                                  NvxSurface *pSrcSurface,
                                  OMX_BOOL bWait);

OMX_U32 NvxGetRotationAndroidCameraPreview(void);
NvError NvxUpdateOverlayAndroidCameraPreview(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                            NvBool bSetSurface);
void NvxInitAndroidCameraPreview(void);
void NvxShutdownAndroidCameraPreview(void);

void NvxSmartDimmerEnableAndroidCameraPreview(NvxOverlay *pOverlay, NvBool enable);

#ifdef __cplusplus
}
#endif

#endif

#endif


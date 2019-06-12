/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

extern "C"
{
#include "NvxHelpers.h"
#include "nvxhelpers_int.h"
#include "nvassert.h"
}

#include <unistd.h>

#include <hardware/camera.h>

NvError NvxAllocateOverlayAndroidCameraPreview(
    NvU32 *pWidth,
    NvU32 *pHeight,
    NvColorFormat eFormat,
    NvxOverlay *pOverlay,
    ENvxVidRendType eType,
    NvBool bDisplayAtRequestRect,
    NvRect *pDisplayRect,
    NvU32 nLayout)
{
    NvU32 width, height;
    NvError status;
    NvxSurface *pSurface = NULL;
    int minUndequeuedBufs = 0;
    struct preview_stream_ops *anw = (struct preview_stream_ops*)pOverlay->pAndroidCameraPreview;

    width = *pWidth;
    height = *pHeight;

    pOverlay->srcX = pOverlay->srcY = 0;
    pOverlay->srcW = width;
    pOverlay->srcH = height;

    if (NvSuccess != NvxSurfaceAlloc(&pSurface, width, height,
                                     eFormat, nLayout))
    {
        goto fail;
    }

    pOverlay->pSurface = pSurface;
    ClearYUVSurface(pOverlay->pSurface);

    //native_window_set_usage()
    anw->get_min_undequeued_buffer_count(anw, &minUndequeuedBufs);
    anw->set_buffer_count(anw, 1 + minUndequeuedBufs);
    anw->set_buffers_geometry(anw, width, height, HAL_PIXEL_FORMAT_YV12);

    if (NvSuccess != NvxUpdateOverlayAndroidCameraPreview(pDisplayRect, pOverlay, NV_TRUE))
        goto fail;


    return NvSuccess;

fail:
    if (pOverlay->pSurface)
        NvxSurfaceFree(&pOverlay->pSurface);

    pOverlay->pSurface = NULL;

    return NvError_BadParameter;
}

NvError NvxUpdateOverlayAndroidCameraPreview(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                            NvBool bSetSurface)
{
    struct preview_stream_ops *anw = (struct preview_stream_ops*)pOverlay->pAndroidCameraPreview;

//    anw->set_crop(anw, pOverlay->srcX, pOverlay->srcY, pOverlay->srcX + pOverlay->srcW, pOverlay->srcY + pOverlay->srcH);

    if (bSetSurface)
    {
        NvxRenderSurfaceToOverlayAndroidCameraPreview(pOverlay, pOverlay->pSurface, OMX_FALSE);
    }

    return NvSuccess;
}

void NvxReleaseOverlayAndroidCameraPreview(NvxOverlay *pOverlay)
{
    NvxSurfaceFree(&pOverlay->pSurface);
    pOverlay->pSurface = NULL;
}

void NvxRenderSurfaceToOverlayAndroidCameraPreview(NvxOverlay *pOverlay,
                                  NvxSurface *pSrcSurface,
                                  OMX_BOOL bWait)
{
    buffer_handle_t *anbuffer, handle;
    NvError Err;
    NvDdk2dBlitParameters TexParam;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle h2d = NvxGet2d();
    NvRmFence fences[OMX_MAX_FENCES];
    NvU32 numFences;
    const NvRmSurface *Surf;

    struct preview_stream_ops *anw = (struct preview_stream_ops*)pOverlay->pAndroidCameraPreview;

    if (!h2d)
        return;

    NvOsMemset(&TexParam, 0, sizeof(NvDdk2dBlitParameters));

    int stride;
    if (0 != anw->dequeue_buffer(anw, &anbuffer, &stride))
        return;

    handle = (*anbuffer);

    NvxLock2d();
    Err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Y_U_V,
                               &(pSrcSurface->Surfaces[0]), &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    nvgr_get_surfaces(handle, &Surf, NULL);
    Err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Y_U_V,
                               (NvRmSurface *)Surf, &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }

    // Fetches the SurfaceFlinger fences from the gralloc buffer
    NvxGetPreFencesANW(handle, fences, &numFences, GRALLOC_USAGE_HW_RENDER);
    NV_ASSERT(numFences <= NVDDK_2D_MAX_FENCES);

    // Add the fences to the destination surface
    NvDdk2dSurfaceLock(pDstDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_Read,
                       NULL,
                       NULL,
                       NULL);
    NvDdk2dSurfaceUnlock(pDstDdk2dSurface, fences, numFences);

    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    NvOsMemset(&DstRect, 0, sizeof(NvRect));

    SrcRect.right = DstRect.right = pOverlay->srcW;
    SrcRect.bottom = DstRect.bottom = pOverlay->srcH;

    // Set attributes
    NvDdk2dSetBlitFilter(&TexParam, NvDdk2dStretchFilter_Nicest);

    NvDdk2dSetBlitTransform(&TexParam, NvDdk2dTransform_None);

    ConvertRect2Fx(&SrcRect, &SrcRectLocal);
    Err = NvDdk2dBlit(h2d, pDstDdk2dSurface, &DstRect,
                      pSrcDdk2dSurface, &SrcRectLocal, &TexParam);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }

    /* Wait for 2D fences */
    Nvx2dFlush(pDstDdk2dSurface);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    if (NvSuccess != Err) {
        anw->cancel_buffer(anw, anbuffer);
    } else {
        anw->enqueue_buffer(anw, anbuffer);
    }
}

OMX_U32 NvxGetRotationAndroidCameraPreview(void)
{
    return 0;
}

void NvxInitAndroidCameraPreview(void)
{
}

void NvxShutdownAndroidCameraPreview(void)
{
    // There is currently no API to release the module.
}

void NvxSmartDimmerEnableAndroidCameraPreview(NvxOverlay *pOverlay, NvBool enable)
{
}

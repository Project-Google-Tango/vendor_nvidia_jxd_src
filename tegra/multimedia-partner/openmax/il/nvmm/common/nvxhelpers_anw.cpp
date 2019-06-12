/* Copyright (c) 2010-2014 NVIDIA CORPORATION.  All rights reserved.
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
#include <cutils/log.h>
#include <sync/sync.h>

NvError NvxAllocateOverlayANW(
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
    ANativeWindow *anw = (ANativeWindow *)pOverlay->pANW;
    int minUndequeuedBufs = 0;

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


    native_window_api_connect(anw, NATIVE_WINDOW_API_CAMERA);
    native_window_set_scaling_mode(anw,
                    NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);

    //native_window_connect?
    anw->query(anw, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBufs);
    native_window_set_buffer_count(anw, 1 + minUndequeuedBufs);
    native_window_set_buffers_geometry(anw, width, height,
                                       HAL_PIXEL_FORMAT_YV12);

    if (NvSuccess != NvxUpdateOverlayANW(pDisplayRect, pOverlay, NV_TRUE))
        goto fail;


    return NvSuccess;

fail:
    if (pOverlay->pSurface)
        NvxSurfaceFree(&pOverlay->pSurface);

    pOverlay->pSurface = NULL;

    return NvError_BadParameter;
}

NvError NvxUpdateOverlayANW(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                            NvBool bSetSurface)
{
    ANativeWindow *anw = (ANativeWindow *)pOverlay->pANW;

    native_window_set_buffers_geometry(anw, pOverlay->srcW, pOverlay->srcH,
                                       HAL_PIXEL_FORMAT_YV12);

    if (bSetSurface)
    {
        NvxRenderSurfaceToOverlayANW(pOverlay, pOverlay->pSurface, OMX_FALSE);
    }

    return NvSuccess;
}

void NvxReleaseOverlayANW(NvxOverlay *pOverlay)
{
    NvxSurfaceFree(&pOverlay->pSurface);
    pOverlay->pSurface = NULL;
}

void NvxRenderSurfaceToOverlayANW(NvxOverlay *pOverlay,
                                  NvxSurface *pSrcSurface,
                                  OMX_BOOL bWait)
{
    ANativeWindow *anw = (ANativeWindow *)pOverlay->pANW;
    android_native_buffer_t *anbuffer;
    NvError Err;
    NvDdk2dBlitParameters TexParam;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle h2d = NvxGet2d();
    NvRmFence fences[OMX_MAX_FENCES];
    NvU32 numFences = OMX_MAX_FENCES;
    NvDdk2dSurfaceType Type;
    int preFenceFd = -1;
    int postFenceFd = -1;
    const NvRmSurface *Surf;

    if (!h2d)
        return;

    NvOsMemset(&TexParam, 0, sizeof(NvDdk2dBlitParameters));

    if (0 != anw->dequeueBuffer(anw, &anbuffer, &preFenceFd))
        return;

    if (preFenceFd >= 0) {
        Err = NvRmFenceGetFromFile(preFenceFd, fences, &numFences);
        if (Err != NvSuccess) {
            ALOGE("%s: NvRmFenceGetFromFile returned %d", __func__, Err);
            sync_wait(preFenceFd, -1);
            numFences = 0;
        }
    } else {
        numFences = 0;
    }

    nvgr_get_surfaces(anbuffer->handle, &Surf, NULL);

    if ((pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_U8_V8)
       || (pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_V8_U8))
       Type = NvDdk2dSurfaceType_Y_UV;
    else
       Type = NvDdk2dSurfaceType_Y_U_V;

    NvxLock2d();
    Err = NvDdk2dSurfaceCreate(h2d, Type,
                               &(pSrcSurface->Surfaces[0]), &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    Err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Y_U_V,
                               (NvRmSurface *)Surf, &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }

    // Add the fences to the destination surface
    if (numFences) {
        NvDdk2dSurfaceLock(pDstDdk2dSurface,
                           NvDdk2dSurfaceAccessMode_Read,
                           NULL,
                           NULL,
                           NULL);
        NvDdk2dSurfaceUnlock(pDstDdk2dSurface, fences, numFences);
    }

    //
    // set the source and destiantion rectangles for the blit. take into account any cropping
    // rectangle that is in effect on the source. the blit destination is always top-left within
    // the android native window.
    //
    DstRect.left   = 0;
    DstRect.top    = 0;
    DstRect.right  = pOverlay->srcW;
    DstRect.bottom = pOverlay->srcH;

    SrcRect.left   = pOverlay->srcX;
    SrcRect.top    = pOverlay->srcY;
    SrcRect.right  = SrcRect.left + pOverlay->srcW;
    SrcRect.bottom = SrcRect.top  + pOverlay->srcH;

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

    // Fetch the blit fence
    NvDdk2dSurfaceLock(pDstDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_Read,
                       NULL,
                       fences,
                       &numFences);
    NvDdk2dSurfaceUnlock(pDstDdk2dSurface, NULL, 0);

    // This is needed only for traditional OMX path (not IOMX),
    // for example when using omxplayer.
    if (pOverlay->StereoOverlayModeFlag)
    {
        NvU32 StereoInfo = nvgr_get_stereo_info(anbuffer->handle);
        StereoInfo |= pOverlay->StereoOverlayModeFlag;
        nvgr_set_stereo_info(anbuffer->handle, StereoInfo);
    }

    if (numFences) {
        Err = NvRmFencePutToFile(__func__, fences, numFences, &postFenceFd);
        if (Err != NvSuccess) {
            NvRmDeviceHandle hRm = NvxGetRmDevice();

            ALOGE("%s: NvRmFencePutToFile returned %d", __func__, Err);
            while (--numFences) {
                NvRmFenceWait(hRm, &fences[numFences], NV_WAIT_INFINITE);
            }
            // Don't fail to queue because of this error.
            Err = NvSuccess;
        }
    }

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    if (NvSuccess != Err) {
        anw->cancelBuffer(anw, anbuffer, preFenceFd);
    } else {
        anw->queueBuffer(anw, anbuffer, postFenceFd);
        if (preFenceFd >= 0) {
            close(preFenceFd);
        }
    }
}

OMX_U32 NvxGetRotationANW(void)
{
    return 0;
}

void NvxSmartDimmerEnableANW(NvxOverlay *pOverlay, NvBool enable)
{
}

static void NvxFencesFromFd(int fenceFd,
                            NvRmFence fences[OMX_MAX_FENCES],
                            NvU32 *numFences)
{
    if (fenceFd >= 0) {
        *numFences = OMX_MAX_FENCES;
        int err = NvRmFenceGetFromFile(fenceFd, fences, numFences);
        if (err) {
            *numFences = 0;
            sync_wait(fenceFd, -1);
        }
        close(fenceFd);
    } else {
        *numFences = 0;
    }
}

int NvxGetPreFenceANW(buffer_handle_t handle)
{
    int fenceFd;
    int err;

    err = nvgr_get_fence(handle, &fenceFd);
    if (err) {
        fenceFd = -1;
        ALOGW("%s: nvgr_get_fence failed, error %d", __func__, err);
        NV_ASSERT(0);
    }

    return fenceFd;
}

void NvxWaitPreFenceANW(buffer_handle_t handle, int usage)
{
    int fenceFd = NvxGetPreFenceANW(handle);
    if (GRALLOC_USAGE_HW_TEXTURE == usage) {
        nvgr_decompress(handle, fenceFd, &fenceFd);
    }
    if (fenceFd >= 0) {
        sync_wait(fenceFd, -1);
        close(fenceFd);
    }
}

void NvxGetPreFencesANW(buffer_handle_t handle,
                        NvRmFence fences[OMX_MAX_FENCES],
                        NvU32 *numFences, int usage)
{
    int fenceFd = NvxGetPreFenceANW(handle);
    if (GRALLOC_USAGE_HW_TEXTURE == usage) {
        nvgr_decompress(handle, fenceFd, &fenceFd);
    }
    NvxFencesFromFd(fenceFd, fences, numFences);
}

void NvxWaitFences(const NvRmFence *fences, NvU32 numFences)
{
    NvRmDeviceHandle hRm = NvxGetRmDevice();
    NvU32 ii;

    for (ii = 0; ii < numFences; ii++) {
        if (fences[ii].SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
            NvRmFenceWait(hRm, &fences[ii], NV_WAIT_INFINITE);
        }
    }
}

void NvxPutWriteNvFence(buffer_handle_t handle, const NvRmFence *fence)
{
    if (fence->SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
        nvgr_put_write_nvfence(handle, fence);
    }
}

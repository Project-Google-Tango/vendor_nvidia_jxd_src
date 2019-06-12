/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvxhelpers_int.h"

static int DC_HEAD = 0;

OMX_ERRORTYPE NvxAllocateOverlayDC(NvU32 *pWidth,
                                   NvU32 *pHeight,
                                   NvColorFormat eFormat,
                                   NvxOverlay *pOverlay,
                                   ENvxVidRendType eType,
                                   NvBool bDisplayAtRequestRect,
                                   NvRect *pDisplayRect,
                                   NvU32 nLayout)
{
    OMX_S32 nvmapfd;
    nvdcHandle hnvdc;
    NvU32 width, height;
    NvxSurface *pSurface = NULL;
    struct nvdcMode getDCMode = {};
    OMX_ERRORTYPE eReturnVal;
    NvError eNvRet;

    if (eType == Rend_TypeHDMI)
        DC_HEAD = 1;

    //  Get libnvrm's knvmap file descriptor.
    nvmapfd = NvRm_MemmgrGetIoctlFile();

    if (nvmapfd >= 0 &&
         (hnvdc = nvdcOpen(nvmapfd)) != NULL)
    {
        // Check the number of heads.  nvdcOpen may succeed but find no heads if
        // the kernel support isn't there.
        if (nvdcQueryNumHeads(hnvdc) <= 0)
        {
            nvdcClose(hnvdc);
            hnvdc = NULL;
            return OMX_ErrorInsufficientResources;
        }
    }
    else
    {
        return OMX_ErrorInsufficientResources;
    }

    //  Create the overlay
    eReturnVal = nvdcGetWindow(hnvdc, DC_HEAD, pOverlay->nIndex);
    if (eReturnVal)
    {
        return eReturnVal;
    }
    pOverlay->hnvdc = hnvdc;

    eReturnVal = nvdcGetMode(hnvdc, DC_HEAD, &getDCMode);
    if (NvSuccess == eReturnVal)
    {
        pOverlay->screenWidth = getDCMode.hActive;
        pOverlay->screenHeight = getDCMode.vActive;
    }
    else
    {
        pOverlay->screenWidth = 800;
        pOverlay->screenHeight = 480;
    }

    width = *pWidth;
    height = *pHeight;

    pOverlay->srcX = pOverlay->srcY = 0;
    pOverlay->srcW = width;
    pOverlay->srcH = height;

    if (eFormat == NvColorFormat_A8R8G8B8)
    {
        eNvRet = NvxSurfaceAlloc(&pSurface, width, height,
                                 eFormat, nLayout);
    }
    else
    {
        eNvRet = NvxSurfaceAllocContiguous(&pSurface, width, height,
                                           eFormat, nLayout);
    }

    if (NvSuccess != eNvRet)
    {
        goto fail;
    }

    pOverlay->pSurface = pSurface;

    if (eFormat == NvColorFormat_Y8)
    {
        ClearYUVSurface(pOverlay->pSurface);
    }

    if (!bDisplayAtRequestRect)
    {
        pDisplayRect = NULL;
    }

    if (NvSuccess != NvxUpdateOverlayDC(pDisplayRect, pOverlay, NV_TRUE))
        goto fail;

    pOverlay->bUsingDC = OMX_TRUE;
    return NvSuccess;

  fail:
    if (pOverlay->pSurface)
        NvxSurfaceFree(&pOverlay->pSurface);

    pOverlay->pSurface = NULL;

    return NvError_BadParameter;
}

OMX_ERRORTYPE  NvxUpdateOverlayDC(NvRect *pNewDestRect, NvxOverlay *pOverlay,
                                  NvBool bSetSurface)
{
    int requestedX, requestedY;
    struct nvdcFlipWinArgs *pflipWin = &pOverlay->flipWin;

    if (pNewDestRect)
    {
        pOverlay->destX = pNewDestRect->left;
        pOverlay->destY = pNewDestRect->top;
        pOverlay->destW = pNewDestRect->right - pNewDestRect->left;
        pOverlay->destH = pNewDestRect->bottom - pNewDestRect->top;
    }
    else
    {
        pOverlay->destX = 0;
        pOverlay->destY = 0;
        pOverlay->destW = pOverlay->screenWidth;
        pOverlay->destH = pOverlay->screenHeight;
    }

    // Adjust dest rect for aspect ratio,
    requestedX = pOverlay->destX;
    requestedY = pOverlay->destY;

    if (pOverlay->nSetRotation == 90 || pOverlay->nSetRotation == 270)
    {
        pOverlay->destX = 0;
        pOverlay->destY = 0;
        pOverlay->destW ^= pOverlay->destH;
        pOverlay->destH ^= pOverlay->destW;
        pOverlay->destW ^= pOverlay->destH;
    }

    NvxAdjustOverlayAspect(pOverlay);

    if (pOverlay->nSetRotation == 90 || pOverlay->nSetRotation == 270)
    {
        int tmp;
        pOverlay->destW ^= pOverlay->destH;
        pOverlay->destH ^= pOverlay->destW;
        pOverlay->destW ^= pOverlay->destH;

        tmp = pOverlay->destX;
        pOverlay->destX = pOverlay->destY + requestedX;
        pOverlay->destY = tmp + requestedY;
    }

    if (pOverlay->srcX + pOverlay->srcW > pOverlay->pSurface->Surfaces[0].Width)
        pOverlay->srcW = pOverlay->pSurface->Surfaces[0].Width - pOverlay->srcX;

    if (pOverlay->srcY + pOverlay->srcH > pOverlay->pSurface->Surfaces[0].Height)
        pOverlay->srcH = pOverlay->pSurface->Surfaces[0].Height - pOverlay->srcY;

    pflipWin->x = pack_ufixed_20_12(pOverlay->srcX, 0);
    pflipWin->y = pack_ufixed_20_12(pOverlay->srcY, 0);
    pflipWin->w = pack_ufixed_20_12(pOverlay->srcX + pOverlay->srcW, 0);
    pflipWin->h = pack_ufixed_20_12(pOverlay->srcY + pOverlay->srcH, 0);

    if (!pOverlay->destW)
        pflipWin->outW = pOverlay->screenWidth;
    else
        pflipWin->outW = pOverlay->destW;

    if (!pOverlay->destH)
        pflipWin->outH = pOverlay->screenHeight;
    else
        pflipWin->outH = pOverlay->destH;

    if (pOverlay->destX > pOverlay->screenWidth)
        pflipWin->outX = 0;
    else
        pflipWin->outX = pOverlay->destX;

    if (pOverlay->destY > pOverlay->screenHeight)
        pflipWin->outY = 0;
    else
        pflipWin->outY = pOverlay->destY;

    if (pflipWin->outX + pflipWin->outW > pOverlay->screenWidth)
        pflipWin->outW = pOverlay->screenWidth - pflipWin->outX;
    if (pflipWin->outY + pflipWin->outH > pOverlay->screenHeight)
        pflipWin->outH = pOverlay->screenHeight - pflipWin->outY;

    pflipWin->swapInterval = 1;

    if (bSetSurface)
    {
        NvxRenderSurfaceToOverlayDC(pOverlay, pOverlay->pSurface, OMX_FALSE);
    }

    return NvSuccess;
}

OMX_ERRORTYPE NvxRenderSurfaceToOverlayDC(NvxOverlay *pOverlay,
    NvxSurface *pSrcSurface,
    OMX_BOOL bWait)
{
    struct nvdcFlipWinArgs flipWin = { };
    struct nvdcFlipArgs flip = { };

    NvRmSurface *surfs = pSrcSurface->Surfaces;

    flipWin.x = pOverlay->flipWin.x;
    flipWin.y = pOverlay->flipWin.y;
    flipWin.w = pOverlay->flipWin.w;
    flipWin.h = pOverlay->flipWin.h;

    flipWin.outX = pOverlay->flipWin.outX;
    flipWin.outY = pOverlay->flipWin.outY;
    flipWin.outW = pOverlay->flipWin.outW;
    flipWin.outH = pOverlay->flipWin.outH;

    // Flip to the display surface.
    flipWin.index = pOverlay->nIndex;

    flipWin.preSyncptId = NVRM_INVALID_SYNCPOINT_ID;
    flipWin.blend = NVDC_BLEND_NONE;

    if (surfs->ColorFormat == NvColorFormat_A8B8G8R8)
    {
        flipWin.surfaces[NVDC_FLIP_SURF_RGB] = &surfs[0];
        flipWin.format = nvdcR8G8B8A8;
    }
    else if (surfs->ColorFormat == NvColorFormat_A8R8G8B8)
    {
        flipWin.surfaces[NVDC_FLIP_SURF_RGB] = &surfs[0];
        flipWin.format = nvdcB8G8R8A8;
    }
    else
    {
        flipWin.surfaces[NVDC_FLIP_SURF_Y] = &surfs[0];
        flipWin.surfaces[NVDC_FLIP_SURF_U] = &surfs[1];

        if (pSrcSurface->SurfaceCount == 2)
        {
            flipWin.format = nvdcYCbCr420SP;
            flipWin.surfaces[NVDC_FLIP_SURF_V] = NULL;
        }
        else
        {
            flipWin.format = nvdcYCbCr420P;
            flipWin.surfaces[NVDC_FLIP_SURF_V] = &surfs[2];
        }
    }

    flipWin.z = pOverlay->nDepth;

    flipWin.swapInterval = 1;

    flip.win = &flipWin;
    flip.numWindows = 1;
    flip.postSyncptId = NVRM_INVALID_SYNCPOINT_ID;

    OMX_ERRORTYPE eReturnVal = nvdcFlip(pOverlay->hnvdc, DC_HEAD, &flip);
    if (eReturnVal)
    {
        return eReturnVal;
    }

    if (bWait)
    {
        if (flip.postSyncptId != NVRM_INVALID_SYNCPOINT_ID)
        {
            NvRmFence fence;
            fence.SyncPointID = flip.postSyncptId;
            fence.Value = flip.postSyncptValue;
            if (NvSuccess != NvRmFenceWait(pOverlay->hRmDev, &fence, 350))
            {
                NvOsDebugPrintf("vid_rend: syncpoint wait timeout\n'");
            }
        }
    }

    return NvSuccess;
}

OMX_ERRORTYPE NvxReleaseOverlayDC(NvxOverlay *pOverlay)
{
    struct nvdcFlipWinArgs flipWin = { };
    struct nvdcFlipArgs flip = { };

    NvxSurfaceFree(&pOverlay->pSurface);
    pOverlay->pSurface = NULL;

    // Flip to the display surface.
    flipWin.index = pOverlay->nIndex;
    flipWin.preSyncptId = NVRM_INVALID_SYNCPOINT_ID;

    flipWin.surfaces[0] = NULL;

    flip.win = &flipWin;
    flip.numWindows = 1;
    flip.postSyncptId = NVRM_INVALID_SYNCPOINT_ID;

    OMX_ERRORTYPE eReturnVal = nvdcFlip(pOverlay->hnvdc, DC_HEAD, &flip);
    if (eReturnVal)
    {
        return eReturnVal;
    }

    nvdcPutWindow(pOverlay->hnvdc, DC_HEAD, pOverlay->nIndex);
    nvdcClose(pOverlay->hnvdc);
    DC_HEAD = 0;

    return NvSuccess;
}

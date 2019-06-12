/*
 * Copyright (c) 2009-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <cutils/log.h>
#include <sync/sync.h>

#include "nvgralloc.h"
#include "nvrm_chip.h"
#include "nvhwc_util.h"
#include "nvsync.h"
#include "nvblit.h"
#include "nvblit_ext.h"


int
NvGr2dInit(NvGrModule *ctx)
{
    NvError err;

    err = NvBlitOpen(&ctx->nvblit);
    if (err != NvSuccess) {
        ALOGE("%s: NvBlitOpen failed (%d)", __func__, err);
        return -1;
    }

    return 0;
}

void
NvGr2dDeInit(NvGrModule *ctx)
{
    NvBlitClose(ctx->nvblit);
    ctx->nvblit = NULL;
}

static int
NeedFilter(int transform,
           NvRect *src,
           NvRect *dst)
{
    int src_w = src->right - src->left;
    int src_h = src->bottom - src->top;
    int dst_w = dst->right - dst->left;
    int dst_h = dst->bottom - dst->top;

    if (transform & HAL_TRANSFORM_ROT_90) {
        src_w -= dst_h;
        src_h -= dst_w;
    } else {
        src_w -= dst_w;
        src_h -= dst_h;
    }

    return src_w | src_h;
}

int
NvGr2dBlit(NvGrModule *ctx,
           NvNativeHandle *src,
           NvRect *srcRect,
           int srcFd,
           NvNativeHandle *dst,
           NvRect *dstRect,
           int dstFd,
           int transform,
           NvPoint *dstOffset,
           int *outFd)
{
    NvError err;
    NvBlitState state;
    NvBlitResult result;
    NvBlitRect sr;
    NvBlitRect dr;
    int releaseFd;

    NvBlitClearState(&state);

    switch (transform) {
    #define CASE(nvt, ht) \
    case ht: NvBlitSetTransform(&state, NvBlitTransform_##nvt); break
    CASE(Rotate270,      HAL_TRANSFORM_ROT_90);
    CASE(Rotate180,      HAL_TRANSFORM_ROT_180);
    CASE(Rotate90,       HAL_TRANSFORM_ROT_270);
    CASE(FlipHorizontal, HAL_TRANSFORM_FLIP_H);
    CASE(FlipVertical,   HAL_TRANSFORM_FLIP_V);
    CASE(InvTranspose,   HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90);
    CASE(Transpose,      HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90);
    #undef CASE
    default:
        break;
    }

    dr.left   = dstRect->left;
    dr.top    = dstRect->top;
    dr.right  = dstRect->right;
    dr.bottom = dstRect->bottom;

    sr.left   = srcRect->left;
    sr.top    = srcRect->top;
    sr.right  = srcRect->right;
    sr.bottom = srcRect->bottom;

    if (NeedFilter(transform, srcRect, dstRect)) {
        NvBlitSetFilter(&state, NvBlitFilter_Linear);
        /* inset by 1/2 pixel to avoid filtering outside the image */
        sr.left   += 0.5f;
        sr.top    += 0.5f;
        sr.right  -= 0.5f;
        sr.bottom -= 0.5f;
    }

    if (dstOffset != NULL) {
        NvBlitSetFlag(&state, NvBlitFlagExt_ReturnDstCrop);
    }

    NvBlitSetSrcSurface(&state, (buffer_handle_t) src, srcFd);
    NvBlitSetDstSurface(&state, (buffer_handle_t) dst, dstFd);
    NvBlitSetSrcRect(&state, &sr);
    NvBlitSetDstRect(&state, &dr);

    err = NvBlit(ctx->nvblit, &state, &result);
    if (err != NvSuccess) {
        ALOGE("%s: NvBlit failed (err=%d)", __func__, err);
        return -1;
    }

    releaseFd = NvBlitGetSync(&result);

    if (outFd) {
        *outFd = releaseFd;
    } else {
        nvsync_wait(releaseFd, -1);
        nvsync_close(releaseFd);
    }

    if (dstOffset != NULL) {
        NvBlitGetDstOffset(&result, dstOffset);
    }

    return 0;
}

int
NvGr2DClear(NvGrModule *ctx,
            buffer_handle_t handle,
            NvRect *rect,
            NvU32 color,
            int inFence,
            int *outFence)
{
    NvError err;
    NvBlitState state;
    NvBlitResult result;
    NvBlitRect blitRect;
    int releaseFd;

    if (rect) {
        blitRect.left = rect->left;
        blitRect.top = rect->top;
        blitRect.right = rect->right;
        blitRect.bottom = rect->bottom;
    }

    NvBlitClearState(&state);
    NvBlitSetSrcColor(&state, color, NvColorFormat_R8G8B8A8);
    NvBlitSetDstSurface(&state, handle, inFence);
    NvBlitSetDstRect(&state, rect ? &blitRect : NULL);

    err = NvBlit(ctx->nvblit, &state, &result);
    if (err != NvSuccess) {
        ALOGE("%s: NvBlit failed (err=%d)", __func__, err);
        return -1;
    }

    releaseFd = NvBlitGetSync(&result);

    if (outFence) {
        *outFence = releaseFd;
    } else {
        nvsync_wait(releaseFd, -1);
        nvsync_close(releaseFd);
    }

    NvGrIncWriteCount((NvNativeHandle *)handle);

    return 0;
}


int NvGr2DClearImplicit(NvGrModule *ctx, NvNativeHandle *h)
{
    int outFence;
    buffer_handle_t handle = (buffer_handle_t) h;
    int status = NvGr2DClear(ctx, handle, NULL, 0, -1, &outFence);

    if (!status && outFence >= 0) {
        ctx->add_fence(h, GRALLOC_USAGE_HW_RENDER, outFence);
    }

    return status;
}

/* This should be used for operations where the src and dst buffers
 * are already locked or don't need to be locked (e.g. shadow buffer).
 */
int
NvGr2dCopyBuffer(NvGrModule *ctx, NvNativeHandle *src,
                 NvNativeHandle *dst,
                 const NvRmFence *srcWriteFenceIn,
                 const NvRmFence *dstReadFencesIn,
                 NvU32 numDstReadFencesIn,
                 int *fenceOut)
{
    NvError err;
    NvBlitState state;
    NvBlitResult result;
    int srcFd = -1;
    int dstFd = -1;
    int outFd;

    if (fenceOut != NULL) {
        *fenceOut = NVSYNC_INIT_INVALID;
    }

    if (srcWriteFenceIn != NULL &&
        srcWriteFenceIn->SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
        srcFd = nvsync_from_fence(__func__, srcWriteFenceIn, 1);
    }

    if (numDstReadFencesIn > 0) {
        dstFd = nvsync_from_fence(__func__, dstReadFencesIn, numDstReadFencesIn);
    }

    NvBlitClearState(&state);
    NvBlitSetSrcSurface(&state, (buffer_handle_t) src, srcFd);
    NvBlitSetDstSurface(&state, (buffer_handle_t) dst, dstFd);

    err = NvBlit(ctx->nvblit, &state, &result);
    if (err != NvSuccess) {
        ALOGE("%s: NvBlit failed (err=%d)", __func__, err);
        return -1;
    }

    outFd = NvBlitGetSync(&result);
    if (nvsync_is_valid(outFd)) {
        if (fenceOut != NULL) {
            *fenceOut = outFd;
        } else {
            nvsync_wait(outFd, -1);
            nvsync_close(outFd);
        }
    }

    return 0;
}

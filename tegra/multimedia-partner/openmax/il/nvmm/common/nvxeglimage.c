/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation.
 * Any use, reproduction, disclosure or distribution of this software and
 * related documentation without an express license agreement from NVIDIA
 * Corporation is strictly prohibited.
 */

#include <OMX_Core.h>
#include "nvxeglimage.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "NvxHelpers.h"
#include "nvmm/components/common/nvmmtransformbase.h"
#include "nvassert.h"
#include "NvxIndexExtensions.h"
#include "../components/nvxvideorenderer.h"
#include "nvxegl.h"
#include "nvwsi.h"

#include <unistd.h>

typedef struct NvxEglImageSiblingStruct NvxEglImageSibling;

struct NvxEglImageSiblingStruct
{
    NvRmSurface surf;
    NvDdk2dSurface* ddk2dSurf;
    NvEglImageSyncObj* syncObj;
    NvWsiPixmap* pixmap;
    int memFd;
};

#if defined(ANDROID) && defined(PLATFORM_IS_ICECREAMSANDWICH)
extern EglGetImageForImplementation g_EglGetEglImageAndroid;
// TODO [arasmus 2010-03-16]: Remove legacy once the new is supported
// in eclair.
extern EglGetImageForCurrentContext g_EglGetEglImageAndroidLegacy;

static EGLImageKHR
UnwrapEglImage(
        EGLImageKHR androidEglImage)
{
    // On Android, unwrap EGL image handles
    NV_ASSERT((g_EglGetEglImageAndroid || g_EglGetEglImageAndroidLegacy)
              && "Function should be always found on Android");

    if (g_EglGetEglImageAndroid)
        return g_EglGetEglImageAndroid("NVIDIA", androidEglImage);

    if (g_EglGetEglImageAndroidLegacy)
        return g_EglGetEglImageAndroidLegacy(androidEglImage);

    return NULL;
}
#endif

static void UnlockImage(NvxEglImageSiblingHandle sib)
{
    NvRmFence fences[NVWSI_MAX_FENCES];
    NvU32 numFences = 0;

    if (sib->syncObj)
        sib->pixmap->Obj.IssueFence(&sib->pixmap->Obj, fences, &numFences);

    NvDdk2dSurfaceUnlock(sib->ddk2dSurf, fences, numFences);
}

static void
LockImage(
        NvxEglImageSiblingHandle sib,
        NvDdk2dSurfaceAccessMode accessMode)
{
    NvRmFence fences[NVDDK_2D_MAX_FENCES];
    NvU32 numFences;
    NvU32 i;

    if (sib->syncObj)
    {
        NvDdk2dSurfaceLock(sib->ddk2dSurf, accessMode,
                           NULL, fences, &numFences);
        for (i = 0; i < numFences; i++)
            sib->pixmap->Obj.WaitFence(&sib->pixmap->Obj, &fences[i]);
    }
    else
    {
        NvDdk2dSurfaceLock(sib->ddk2dSurf, accessMode,
                           NULL, NULL, 0);
    }
}

OMX_ERRORTYPE
NvxEglCreateImageSibling(
        EGLImageKHR eglImage,
        NvxEglImageSiblingHandle* hSibling)
{
    NvEglApiImageInfo imageInfo;
    NvEglApiNvRmSurfaceInfo surfaceInfo;
    NvxEglImageSibling* sib;
    NvDdk2dHandle h2d = NvxGet2d();
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvU32 i;

    NV_ASSERT(hSibling);
    NV_ASSERT(eglImage);

#if defined(ANDROID) && defined(PLATFORM_IS_ICECREAMSANDWICH)
    eglImage = UnwrapEglImage(eglImage);
#endif

    if (g_EglExports.getEglImageInfo(eglImage, &imageInfo) != NvSuccess)
        return OMX_ErrorUndefined;

    if (g_EglExports.fillNvRmSurfaceFromEglImage(eglImage, &surfaceInfo) != NvSuccess) {
        return OMX_ErrorUndefined;
    }

    // XXX add support for YUV images?
    if (surfaceInfo.bufCount != 1) {
        err = OMX_ErrorBadParameter;
        goto close_fd;
    }

    sib = NvOsAlloc(sizeof(NvxEglImageSibling));
    if (!sib) {
        err = OMX_ErrorInsufficientResources;
        goto close_fd;
    }

    sib->surf       = surfaceInfo.buf[0];
    sib->ddk2dSurf  = NULL;
    sib->syncObj    = NULL;
    sib->pixmap     = NULL;
    sib->memFd      = surfaceInfo.memFd[0];

    if (NvRmMemHandleFromFd(sib->memFd, &sib->surf.hMem) != NvSuccess)
    {
        NvxEglFreeImageSibling(sib);
        return OMX_ErrorUndefined;
    }

    if (NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Single,
                             &sib->surf, &sib->ddk2dSurf) != NvSuccess)
    {
        NvxEglFreeImageSibling(sib);
        return OMX_ErrorUndefined;
    }

    if (imageInfo.syncObj)
    {
        sib->syncObj = imageInfo.syncObj;
        g_EglExports.syncObjRef(imageInfo.syncObj, &sib->pixmap);
        NV_ASSERT(sib->pixmap);
    }

    LockImage(sib, NvDdk2dSurfaceAccessMode_Read);

    *hSibling = sib;
    return OMX_ErrorNone;

close_fd:
    for (i = 0; i < surfaceInfo.bufCount; i++) {
        if (surfaceInfo.memFd[i] >= 0) {
            close(surfaceInfo.memFd[i]);
        }
    }
    return err;
}

void NvxEglFreeImageSibling(NvxEglImageSiblingHandle sib)
{
    if (sib->syncObj)
    {
        g_EglExports.syncObjUnref(sib->syncObj);
    }

    if (sib->ddk2dSurf)
    {
        NvDdk2dSurfaceUnlock(sib->ddk2dSurf, NULL, 0);
        NvDdk2dSurfaceDestroy(sib->ddk2dSurf);
    }

    if (sib->surf.hMem)
        NvRmMemHandleFree(sib->surf.hMem);
    sib->surf.hMem = NULL;

    if (sib->memFd >= 0) {
        close(sib->memFd);
        sib->memFd = -1;
    }

    NvOsFree(sib);
}

void NvxEglImageGetNvxSurface(const NvxEglImageSiblingHandle sib,
                              NvxSurface *out)
{
    /* currently not used anywhere */
    NV_ASSERT(!"Not implemented");
}

static NvDdk2dSurfaceType s_SurfaceCount2Type[] = {
    NvDdk2dSurfaceType_Force32,
    NvDdk2dSurfaceType_Single,
    NvDdk2dSurfaceType_Y_UV,
    NvDdk2dSurfaceType_Y_U_V
};

static NvError
CreateDdk2dSurfFromMmSurf(
        NvxSurface* mmSurf,
        NvDdk2dSurface** ddk2dSurf)
{
    NvDdk2dHandle h2d = NvxGet2d(); 
    NvRmSurface* rmSurfs = mmSurf->Surfaces;
    NvDdk2dSurfaceType type = s_SurfaceCount2Type[mmSurf->SurfaceCount];

    return NvDdk2dSurfaceCreate(h2d, type, rmSurfs, ddk2dSurf);
}

static NvError
Blit(
        NvDdk2dSurface*   pDstSurface,
        NvRect*           pDstRect,
        NvDdk2dSurface*   pSrcSurface,
        NvDdk2dFixedRect* pSrcRect,
        OMX_MIRRORTYPE    eMirrorType)
{
    NvDdk2dHandle h2d = NvxGet2d();
    NvDdk2dBlitParameters BlitParam;
    
    // Temp surface allocation is slow -> keep the temp memory.
    // Can be removed after blit optimizations described in bug 673296
    // are done.
    BlitParam.ValidFields = NvDdk2dBlitParamField_Flags;
    BlitParam.Flags = NvDdk2dBlitFlags_KeepTempMemory;

    if (!h2d)
        return NvError_BadParameter;

    NvDdk2dSetBlitFilter(&BlitParam, NvDdk2dStretchFilter_Nicest);
    // Map the mirroring type to Transforms here.
    // For EGL, by default the output is vertically flipped as GL has a different origin, hence
    // OMX_MirrorNone ==> NvDdk2dTransform_FlipVertical , THIS IS DEFAULT
    // OMX_MirrorVertical ==> no operation
    // OMX_MirrorHorizontal ==> NvDdk2dTransform_FlipHorizontal
    // OMX_MirrorBoth ==> NvDdk2dTransform_InvTranspose
    if (eMirrorType == OMX_MirrorNone)
        NvDdk2dSetBlitTransform(&BlitParam, NvDdk2dTransform_FlipVertical);
    else if (eMirrorType == OMX_MirrorHorizontal)
        NvDdk2dSetBlitTransform(&BlitParam, NvDdk2dTransform_FlipHorizontal);
    else if (eMirrorType == OMX_MirrorBoth)
        NvDdk2dSetBlitTransform(&BlitParam, NvDdk2dTransform_InvTranspose);

    // stretch-blit.  Vertical flip is done because OMX and GL have different
    // ideas of what's up.
    return NvDdk2dBlit(h2d, 
                       pDstSurface, pDstRect,
                       pSrcSurface, pSrcRect,
                       &BlitParam);
}

static void
CalculateRects(
        NvRect* pCropRect,
        NvRmSurface* srcRmSurf,
        NvRmSurface* dstRmSurf,
        NvDdk2dFixedRect* pSrcRect,
        NvRect*           pDstRect)
{
    NvRect r;

    if(!pCropRect)
    {
        r.left = 0;
        r.top = 0;
        r.right = srcRmSurf->Width;
        r.bottom = srcRmSurf->Height;
    }
    else
    {
        r = *pCropRect;
        if((r.left == 0) && (r.top == 0) && 
           (r.right == 0) && (r.bottom == 0))
        {
            //default to complete surface rect. 
            r.left = 0;
            r.top = 0;
            r.right = srcRmSurf->Width;
            r.bottom = srcRmSurf->Height;
        }
    }
    ConvertRect2Fx(&r, pSrcRect);

    pDstRect->left = 0;
    pDstRect->top = 0;
    pDstRect->right = dstRmSurf->Width;
    pDstRect->bottom = dstRmSurf->Height;
}

static void
Synchronize(
        NvDdk2dSurface* ddk2dSurf,
        NvDdk2dSurfaceAccessMode accessMode,
        NvBool cpuWait,
        NvRmFence* pOutFence)
{
    if (cpuWait)
    {
        NvDdk2dSurfaceLock(ddk2dSurf, accessMode,
                           NULL, NULL, NULL);
        NvDdk2dSurfaceUnlock(ddk2dSurf, NULL, 0);
    }
    else
    {
        NvRmFence fences[NVDDK_2D_MAX_FENCES];
        NvU32 numFences;
        NV_ASSERT(pOutFence);

        NvDdk2dSurfaceLock(ddk2dSurf, accessMode,
                           NULL, fences, &numFences);
        NvDdk2dSurfaceUnlock(ddk2dSurf, NULL, 0);

        // If there is more than one fence we are dropping it here.
        NV_ASSERT(numFences <= 1);
        if (numFences > 0)
            *pOutFence = fences[0];
        else
            pOutFence->SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
    }
}
 
OMX_ERRORTYPE 
NvxEglStretchBlitToImage(
        NvxEglImageSiblingHandle dstImage,
        NvxSurface* srcMmSurf,
        NvBool waitForFinish,
        NvRmFence* pOutFence,
        OMX_MIRRORTYPE eMirrorType)
{
    NvDdk2dSurface*  src2dSurf;
    NvDdk2dSurface*  dst2dSurf = dstImage->ddk2dSurf;
    NvDdk2dFixedRect srcRect;
    NvRect           dstRect;
    NvError          err;

    NV_ASSERT(waitForFinish || pOutFence);

    if (CreateDdk2dSurfFromMmSurf(srcMmSurf, &src2dSurf) != NvSuccess)
        return OMX_ErrorUndefined;

    CalculateRects(&srcMmSurf->CropRect,
                   &srcMmSurf->Surfaces[0], &dstImage->surf,
                   &srcRect, &dstRect);

    UnlockImage(dstImage);

    err = Blit(dst2dSurf, &dstRect, src2dSurf, &srcRect, eMirrorType);

    LockImage(dstImage, NvDdk2dSurfaceAccessMode_Read);

    Synchronize(src2dSurf, NvDdk2dSurfaceAccessMode_ReadWrite,
                waitForFinish, pOutFence);

    NvDdk2dSurfaceDestroy(src2dSurf);

    return (err ? OMX_ErrorUndefined : OMX_ErrorNone);
}
           
OMX_ERRORTYPE 
NvxEglStretchBlitFromImage(
        NvxEglImageSiblingHandle srcImage,
        NvxSurface* dstMmSurf,
        NvBool waitForFinish,
        NvRmFence* pOutFence)
{
    NvDdk2dSurface*  src2dSurf = srcImage->ddk2dSurf;
    NvDdk2dSurface*  dst2dSurf;
    NvDdk2dFixedRect srcRect;
    NvRect           dstRect;
    NvError          err;

    NV_ASSERT(waitForFinish || pOutFence);

    if (CreateDdk2dSurfFromMmSurf(dstMmSurf, &dst2dSurf) != NvSuccess)
        return OMX_ErrorUndefined;

    CalculateRects(NULL,
                   &srcImage->surf, &dstMmSurf->Surfaces[0],
                   &srcRect, &dstRect);

    UnlockImage(srcImage);

    err = Blit(dst2dSurf, &dstRect, src2dSurf, &srcRect, OMX_MirrorNone);

    LockImage(srcImage, NvDdk2dSurfaceAccessMode_ReadWrite);

    Synchronize(dst2dSurf, NvDdk2dSurfaceAccessMode_Read,
                waitForFinish, pOutFence);

    NvDdk2dSurfaceDestroy(dst2dSurf);

    return (err ? OMX_ErrorUndefined : OMX_ErrorNone);
}

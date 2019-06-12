/* Copyright (c) 2010-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

extern "C" {
#include "NvxComponent.h"
#include "NvxHelpers.h"
#include "nvxhelpers_int.h"
#include "nvmmlitetransformbase.h"
#include <nvassert.h>
}

#include "nvxliteandroidbuffer.h"

#include <media/hardware/HardwareAPI.h>

using namespace android;

static const unsigned int s_nvx_port_in = 0;
static const unsigned int s_nvx_port_out = 1;

static OMX_U32 U32_AT(const uint8_t *ptr)
{
    return ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];
}

OMX_ERRORTYPE HandleEnableANBLite(NvxComponent *pNvComp, OMX_U32 portallowed,
                              void *ANBParam)
{
    EnableAndroidNativeBuffersParams *eanbp;
    eanbp = (EnableAndroidNativeBuffersParams *)ANBParam;

    if (eanbp->nPortIndex != portallowed)
    {
        return OMX_ErrorBadParameter;
    }

    NvxPort *pPort = &(pNvComp->pPorts[portallowed]);

    if (eanbp->enable)
    {
        //decide default color format based on default layout
        NvRmSurfaceLayout layout = NvRmSurfaceGetDefaultLayout();
        if (layout == NvRmSurfaceLayout_Blocklinear)
        {
            pPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)
                                                    (NVGR_PIXEL_FORMAT_NV12);
        }
        else
        {
            pPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)
                                                    (NVGR_PIXEL_FORMAT_YUV420);
        }
    }
    else
    {
        pPort->oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE HandleUseANBLite(NvxComponent *pNvComp, void *ANBParam)
{
    UseAndroidNativeBufferParams *uanbp;
    uanbp = (UseAndroidNativeBufferParams *)ANBParam;

    return NvxUseBufferInt(pNvComp->hBaseComponent,
                           uanbp->bufferHeader, uanbp->nPortIndex,
                           uanbp->pAppPrivate, 0,
                           (OMX_U8 *)uanbp->nativeBuffer.get(),
                           NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T);
}

OMX_ERRORTYPE HandleUseNBHLite(NvxComponent *pNvComp, void *NBHParam)
{
    NVX_PARAM_USENATIVEBUFFERHANDLE *unbhp;
    unbhp = (NVX_PARAM_USENATIVEBUFFERHANDLE *)NBHParam;

    return NvxUseBufferInt(pNvComp->hBaseComponent,
                           unbhp->bufferHeader, unbhp->nPortIndex,
                           NULL, 0,
                           (OMX_U8 *)unbhp->oNativeBufferHandlePtr,
                           NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T);
}

OMX_ERRORTYPE ImportAllANBsLite(NvxComponent *pNvComp,
                            SNvxNvMMLiteTransformData *pData,
                            OMX_U32 streamIndex,
                            OMX_U32 matchWidth,
                            OMX_U32 matchHeight)
{
    SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    OMX_BUFFERHEADERTYPE *pBuffer;
    NvxList *pOutBufList = pPortOut->pEmptyBuffers;
    NvxListItem *cur = NULL;

    NvxMutexLock(pPortOut->hMutex);

    pBuffer = pPortOut->pCurrentBufferHdr;
    if (pBuffer)
    {
        ImportANBLite(pNvComp, pData, streamIndex, pBuffer,
                  matchWidth, matchHeight);
    }

    NvOsMutexLock(pOutBufList->oLock);
    cur = pOutBufList->pHead;
    while (cur)
    {
        pBuffer = (OMX_BUFFERHEADERTYPE *)cur->pElement;
        ImportANBLite(pNvComp, pData, streamIndex, pBuffer,
                  matchWidth, matchHeight);
        cur = cur->pNext;
    }
    NvOsMutexUnlock(pOutBufList->oLock);

    NvxMutexUnlock(pPortOut->hMutex);

    NvOsDebugPrintf("imported: %d buffers\n", pPort->nBufsTot);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE ImportANBLite(NvxComponent *pNvComp,
                        SNvxNvMMLiteTransformData *pData,
                        OMX_U32 streamIndex,
                        OMX_BUFFERHEADERTYPE *buffer,
                        OMX_U32 matchWidth,
                        OMX_U32 matchHeight)
{
    android_native_buffer_t *anbuffer = NULL;
    NvMMBuffer *nvmmbuf = NULL;
    buffer_handle_t handle;
    OMX_U32 width, height;
    NvRmFence fences[OMX_MAX_FENCES];
    NvU32 i, numFences;
    NvMMSurfaceDescriptor *pSurfaces;
    SNvxNvMMLitePort *pPort;
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    const NvRmSurface *Surf;

    if (pData->oPorts[streamIndex].bUsesNBHs)
    {
        buffer_handle_t *hBuffer = (buffer_handle_t *)(buffer->pBuffer);
        if (!(buffer->pBuffer))
            return OMX_ErrorBadParameter;

        handle = (*hBuffer);
    }
    else if (pData->oPorts[streamIndex].bUsesAndroidMetaDataBuffers)
    {
        OMX_U32 nMetadataBufferFormat = U32_AT(buffer->pBuffer);
        if(nMetadataBufferFormat != kMetadataBufferTypeGrallocSource)
        {
            NvOsDebugPrintf("%s[%d]: Unsupported android meta buffer type: eType = 0x%x\n",
                        __func__, __LINE__, nMetadataBufferFormat);
            return OMX_ErrorBadParameter;
        }
        buffer_handle_t *pEmbeddedGrallocSrc = (buffer_handle_t *)(buffer->pBuffer+ 4);
        handle = *pEmbeddedGrallocSrc;
    }
    else
    {
        anbuffer = (android_native_buffer_t *)(buffer->pBuffer);
        if (!anbuffer)
        {
            return OMX_ErrorBadParameter;
        }

        handle = anbuffer->handle;
    }

    nvgr_get_surfaces(handle, &Surf, NULL);
    width = Surf[0].Width;
    height = Surf[0].Height;

    pPort = &(pData->oPorts[streamIndex]);
    if (pPort->nWidth > width || pPort->nHeight > height)
    {
        NvOsDebugPrintf("ANB's don't match: %d %d %d %d\n", pPort->nWidth, width, pPort->nHeight, height);
        return OMX_ErrorBadParameter;
    }

    if (matchWidth > 0 && matchHeight > 0 &&
        (width != matchWidth || height != matchHeight))
    {
        NvOsDebugPrintf("ANBs don't match 2: %d %d %d 5d\n", matchWidth, width, matchHeight, height);
        return OMX_ErrorBadParameter;
    }

    pPrivateData = (NvxBufferPlatformPrivate *)buffer->pPlatformPrivate;

    if (!pPrivateData->nvmmBuffer)
    {
        NvMMSurfaceFences *fencestruct;

        nvmmbuf = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
        if (!nvmmbuf)
        {
            return OMX_ErrorInsufficientResources;
        }

        fencestruct = (NvMMSurfaceFences *)NvOsAlloc(sizeof(NvMMSurfaceFences));
        if (!fencestruct)
        {
            NvOsFree(nvmmbuf);
            return OMX_ErrorInsufficientResources;
        }

        NvOsMemset(nvmmbuf, 0, sizeof(NvMMBuffer));
        NvOsMemset(fencestruct, 0, sizeof(NvMMSurfaceFences));

        nvmmbuf->StructSize = sizeof(NvMMBuffer);
        nvmmbuf->BufferID = (pData->oPorts[streamIndex].nBufsANB)++;
        nvmmbuf->PayloadType = NvMMPayloadType_SurfaceArray;
        nvmmbuf->Payload.Surfaces.fences = fencestruct;
        NvOsMemset(&nvmmbuf->PayloadInfo, 0, sizeof(nvmmbuf->PayloadInfo));

        if (pData->sVpp.bVppEnable != OMX_TRUE)
        {
            pData->oPorts[streamIndex].pBuffers[nvmmbuf->BufferID] = nvmmbuf;
            pData->oPorts[streamIndex].pOMXANBBufMap[nvmmbuf->BufferID] = buffer;
        }

        pData->oPorts[streamIndex].nBufsTot++;
        pPrivateData->nvmmBuffer = nvmmbuf;
    }

    nvmmbuf = (NvMMBuffer *)(pPrivateData->nvmmBuffer);

    NvxGetPreFencesANW(handle, fences, &numFences, GRALLOC_USAGE_HW_RENDER);

    pSurfaces = &(nvmmbuf->Payload.Surfaces);
    pSurfaces->fences->numFences = numFences;
    pSurfaces->fences->outFence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;

    pSurfaces->fences->useOutFence = NV_TRUE;

    if (pData->bWaitOnFence == OMX_TRUE) {
         pSurfaces->fences->useOutFence = NV_FALSE;
    }

    NvOsMemcpy(&(pSurfaces->fences->inFence[0]), fences, sizeof(NvRmFence) * numFences);

    if ((pSurfaces->Surfaces[0].hMem != Surf[0].hMem) &&
        (OMX_TRUE == pPrivateData->nvmmBufIsPinned))
    {
        if (pSurfaces->Surfaces[0].hMem != 0)
        {
            NvRmMemUnpin(pSurfaces->Surfaces[0].hMem);
        }
        pPrivateData->nvmmBufIsPinned = OMX_FALSE;
    }

    NvOsMemcpy(pSurfaces->Surfaces, Surf, sizeof(NvRmSurface) * 3);

    if (!pPrivateData->nvmmBufIsPinned)
    {
        pPrivateData->nvmmBufIsPinned = OMX_TRUE;
        pSurfaces->PhysicalAddress[0] = NvRmMemPin(pSurfaces->Surfaces[0].hMem);
        pSurfaces->PhysicalAddress[1] = pSurfaces->PhysicalAddress[0] +
                                        pSurfaces->Surfaces[1].Offset;
        pSurfaces->PhysicalAddress[2] = pSurfaces->PhysicalAddress[0] +
                                        pSurfaces->Surfaces[2].Offset;
    }

    pSurfaces->Empty = NV_TRUE;

    if (pData->sVpp.bVppEnable == OMX_TRUE)
    {
         pData->sVpp.Rendstatus[nvmmbuf->BufferID] = OMX_FALSE;
         NvMMQueueEnQ(pData->sVpp.pOutSurfaceQueue, &nvmmbuf, 10);
         if (pData->sVpp.bVppStop == OMX_TRUE)
             pData->sVpp.bVppStop = OMX_FALSE;
         NvOsSemaphoreSignal(pData->sVpp.VppInpOutAvail);
    }
    else
    {
        pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                                 NvMMBufferType_Payload, sizeof(NvMMBuffer),
                                 nvmmbuf);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE ExportANBLite(NvxComponent *pNvComp,
                        SNvxNvMMLiteTransformData *pData,
                        NvMMBuffer *nvmmbuf,
                        OMX_BUFFERHEADERTYPE **pOutBuf,
                        SNvxNvMMLitePort *pPort,
                        OMX_BOOL bFreeBacking)
{
    // find the matching OMX buffer
    NvxPort *pPortOut = pPort->pOMXPort;
    NvxList *pOutBufList = pPortOut->pEmptyBuffers;
    NvxListItem *cur = NULL;
    OMX_BUFFERHEADERTYPE *omxbuf = NULL;
    NvMMBuffer *buf = NULL;
    android_native_buffer_t *anbuffer;
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    buffer_handle_t handle = NULL;

    omxbuf = *pOutBuf;
    pPrivateData = (NvxBufferPlatformPrivate *)omxbuf->pPlatformPrivate;
    buf = (NvMMBuffer *)pPrivateData->nvmmBuffer;

    if (buf->BufferID != nvmmbuf->BufferID)
    {
        NvOsMutexLock(pOutBufList->oLock);
        cur = pOutBufList->pHead;
        while (cur)
        {
            omxbuf = (OMX_BUFFERHEADERTYPE *)cur->pElement;
            pPrivateData = (NvxBufferPlatformPrivate *)omxbuf->pPlatformPrivate;
            buf = (NvMMBuffer *)pPrivateData->nvmmBuffer;
            if (buf && buf->BufferID == nvmmbuf->BufferID)
            {
                break;
            }
            buf = NULL;
            cur = cur->pNext;
        }
        NvOsMutexUnlock(pOutBufList->oLock);

        if (cur)
            NvxListRemoveEntry(pOutBufList, omxbuf);
        else
        {
            NvOsDebugPrintf("couldn't find matching buffer\n");
            return OMX_ErrorBadParameter;
        }
    }

    *pOutBuf = omxbuf;

    if (pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T)
    {
        anbuffer = (android_native_buffer_t *)(omxbuf->pBuffer);
        if (anbuffer)
        {
            handle = anbuffer->handle;
        }
        omxbuf->nFilledLen = sizeof(buffer_handle_t);
    }
    else if (pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T)
    {
        buffer_handle_t *hBuffer = (buffer_handle_t *)(omxbuf->pBuffer);
        handle = *hBuffer;
        omxbuf->nFilledLen = sizeof(buffer_handle_t);
    }
    else if (pPort->bUsesAndroidMetaDataBuffers)
    {
        OMX_U32 nMetadataBufferFormat = U32_AT(omxbuf->pBuffer);

        if(nMetadataBufferFormat != kMetadataBufferTypeGrallocSource)
        {
            NvOsDebugPrintf("%s[%d]: Unsupported android meta buffer type: eType = 0x%x\n",
                        __func__, __LINE__, nMetadataBufferFormat);
            return OMX_ErrorBadParameter;
        }
        buffer_handle_t *hBuffer = (buffer_handle_t *)((omxbuf->pBuffer)+4);
        handle = *hBuffer;
        omxbuf->nFilledLen = 8;
    }
    else
    {
        NvOsDebugPrintf("Unsupported android buffer type: eType = 0x%x\n",
                        pPrivateData->eType);
        return OMX_ErrorBadParameter;
    }

    if (handle)
    {
        NvU32 StereoInfo = nvmmbuf->PayloadInfo.BufferFlags &
            ( NvMMBufferFlag_StereoEnable |
              NvMMBufferFlag_Stereo_SEI_FPType_Mask |
              NvMMBufferFlag_Stereo_SEI_ContentType_Mask );

        if (nvmmbuf->Payload.Surfaces.fences) {
            NvRmFence *outFence = &nvmmbuf->Payload.Surfaces.fences->outFence;

            if (pData->bUsesSyncFDFromBuffer)
            {
                NvxPutWriteNvFence(handle, outFence);
            }
            else
            {
                NvxWaitFences(outFence, 1);
            }
        }

        nvgr_set_stereo_info(handle, StereoInfo);
        nvgr_set_video_timestamp(handle, omxbuf->nTimeStamp);

        /* WAR bug 827707 */
        nvgr_set_crop(handle, &nvmmbuf->Payload.Surfaces.CropRect);
    }

    if (bFreeBacking)
    {
        NvOsFree(buf);
        pPrivateData->nvmmBuffer = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FreeANBLite(NvxComponent *pNvComp,
                      SNvxNvMMLiteTransformData *pData,
                      OMX_U32 streamIndex,
                      OMX_BUFFERHEADERTYPE *buffer)

{
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvMMBuffer *nvmmbuf;

    NvxMutexLock(pPortOut->hMutex);

    if (!pData->oPorts[streamIndex].nBufsANB)
        goto exit;

    pPrivateData = (NvxBufferPlatformPrivate *)buffer->pPlatformPrivate;

    nvmmbuf = (NvMMBuffer *)pPrivateData->nvmmBuffer;
    if ((!nvmmbuf) || (!pData->oPorts[streamIndex].pOMXANBBufMap[nvmmbuf->BufferID])) {
        goto exit;
    }

    // This assumes that we'll be freeing _all_ buffers at once

    pData->oPorts[streamIndex].nBufsANB--;

    if(pData->sVpp.bVppEnable != OMX_TRUE)
    {
        pData->oPorts[streamIndex].nBufsTot--;
        pData->oPorts[streamIndex].pOMXANBBufMap[nvmmbuf->BufferID] = NULL;
        pData->oPorts[streamIndex].pBuffers[nvmmbuf->BufferID] = NULL;
    }
    if (pPrivateData->nvmmBufIsPinned)
        NvRmMemUnpin(nvmmbuf->Payload.Surfaces.Surfaces[0].hMem);
    pPrivateData->nvmmBufIsPinned = OMX_FALSE;

    NvOsFree(nvmmbuf);
    pPrivateData->nvmmBuffer = NULL;

exit:
    NvxMutexUnlock(pPortOut->hMutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE HandleGetANBUsageLite(NvxComponent *pNvComp,
                                void *ANBParam,
                                OMX_U32 SWAccess)
{
    GetAndroidNativeBufferUsageParams *ganbup;
    ganbup = (GetAndroidNativeBufferUsageParams *)ANBParam;

    if(SWAccess)
        ganbup->nUsage = GRALLOC_USAGE_SW_READ_OFTEN;
    else
    {
        if (ganbup->nPortIndex == s_nvx_port_in)
            ganbup->nUsage = 0;
        else
            ganbup->nUsage |= GRALLOC_USAGE_HW_RENDER;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE CopyNvxSurfaceToANBLite(NvxComponent *pNvComp,
                                  NvMMBuffer *nvmmbuf,
                                  OMX_BUFFERHEADERTYPE *buffer)
{
    NvxSurface *pSrcSurface = &nvmmbuf->Payload.Surfaces;
    android_native_buffer_t *anbuffer;
    buffer_handle_t handle;
    NvError Err;
    NvDdk2dBlitParameters TexParam;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle h2d = NvxGet2d();
    NvU32 width, height;
    NvRmFence fences[OMX_MAX_FENCES];
    NvU32 numFences;
    const NvRmSurface *Surf;

    if (!h2d)
        return OMX_ErrorBadParameter;

    NvOsMemset(&TexParam, 0, sizeof(NvDdk2dBlitParameters));

    anbuffer = (android_native_buffer_t *)(buffer->pBuffer);
    if (!anbuffer)
        return OMX_ErrorBadParameter;

    handle = anbuffer->handle;

    if (nvgr_is_valid(handle))
    {
        NvU32 StereoInfo = nvmmbuf->PayloadInfo.BufferFlags &
            ( NvMMBufferFlag_StereoEnable |
              NvMMBufferFlag_Stereo_SEI_FPType_Mask |
              NvMMBufferFlag_Stereo_SEI_ContentType_Mask );

        nvgr_set_stereo_info(handle, StereoInfo);
        nvgr_set_video_timestamp(handle, buffer->nTimeStamp);

        /* WAR bug 827707 */
        nvgr_set_crop(handle, &nvmmbuf->Payload.Surfaces.CropRect);
    }
    else
    {
        // NULL-GraphicBuffer here?
        return OMX_ErrorBadParameter;
    }

    nvgr_get_surfaces(handle, &Surf, NULL);
    width = NV_MIN(pSrcSurface->Surfaces[0].Width, Surf[0].Width);
    height = NV_MIN(pSrcSurface->Surfaces[0].Height, Surf[0].Height);

    NvxLock2d();
    Err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Y_U_V,
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

    // Fetches the SurfaceFlinger fences from the gralloc buffer
    NvxGetPreFencesANW(handle, fences, &numFences, GRALLOC_USAGE_HW_RENDER);

    // Add the fences to the destination surface
    NvDdk2dSurfaceLock(pDstDdk2dSurface, NvDdk2dSurfaceAccessMode_Read,
                       NULL, NULL, NULL);
    NvDdk2dSurfaceUnlock(pDstDdk2dSurface, fences, numFences);

    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    NvOsMemset(&DstRect, 0, sizeof(NvRect));

    SrcRect.right = DstRect.right = width;
    SrcRect.bottom = DstRect.bottom = height;

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

    Nvx2dFlush(pDstDdk2dSurface);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    buffer->nFilledLen = sizeof(buffer_handle_t);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE CopyANBToNvxSurfaceLite(NvxComponent *pNvComp,
                                      buffer_handle_t handle,
                                      NvxSurface *pDestSurface,
                                      OMX_BOOL bSkip2DBlit)
{
    NvError Err;
    NvDdk2dBlitParameters TexParam;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle h2d;
    NvU32 width, height;
    NvDdk2dSurfaceType SurfaceType;
    NvU32 numFences;
    NvU32 c;
    NvRmFence fences[OMX_MAX_FENCES];
    NvRmDeviceHandle hRm = NvxGetRmDevice();
    const NvRmSurface *Surf;
    size_t SurfCount;

    if (!handle)
        return OMX_ErrorBadParameter;

    nvgr_get_surfaces(handle, &Surf, &SurfCount);
    nvgr_get_crop(handle, &pDestSurface->CropRect);

    if (bSkip2DBlit)
    {
        NvxWaitPreFenceANW(handle, GRALLOC_USAGE_HW_TEXTURE);

        pDestSurface->SurfaceCount = SurfCount;
        NvOsMemcpy(pDestSurface->Surfaces, Surf,
                   sizeof(NvRmSurface) * NVMMSURFACEDESCRIPTOR_MAX_SURFACES);
        return OMX_ErrorNone;
    }

    h2d = NvxGet2d();
    if (!h2d)
        return OMX_ErrorBadParameter;

    NvOsMemset(&TexParam, 0, sizeof(NvDdk2dBlitParameters));

    NvxLock2d();
    Err = NvDdk2dSurfaceCreate(h2d, NvDdk2dSurfaceType_Y_U_V,
                               &(pDestSurface->Surfaces[0]), &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    if (SurfCount == 1)
        SurfaceType = NvDdk2dSurfaceType_Single;
    else if (SurfCount == 3)
        SurfaceType = NvDdk2dSurfaceType_Y_U_V;
    else
        goto L_cleanup;
    Err = NvDdk2dSurfaceCreate(h2d, SurfaceType, (NvRmSurface *) Surf,
                               &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }

    // Fetches the SurfaceFlinger fences from the gralloc buffer
    NvxGetPreFencesANW(handle, fences, &numFences, GRALLOC_USAGE_HW_TEXTURE);

    /* transfer gralloc fences into ddk2d:
     * do a CPU wait on any pending nvddk2d fences (there should be
     * none), then unlock with the fences from gralloc.
     *
     * Note that ddk2d access mode is the opposite of the gralloc
     * usage.  We locked the gralloc buffer for read access, so
     * we are giving ddk2d write fences.
     * This step is necessary to avoid 2D reading before any possible
     * HW access (for example 3D) was done rendering.
     */
    NvDdk2dSurfaceLock(pSrcDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_Write,
                       NULL, NULL, NULL);
    NvDdk2dSurfaceUnlock(pSrcDdk2dSurface, fences, numFences);

    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    NvOsMemset(&DstRect, 0, sizeof(NvRect));

    width = NV_MIN(Surf[0].Width, pDestSurface->Surfaces[0].Width);
    height = NV_MIN(Surf[0].Height, pDestSurface->Surfaces[0].Height);

    SrcRect.right = DstRect.right = width;
    SrcRect.bottom = DstRect.bottom = height;

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

    // Now flush the Dest surface, which essentially does a CPU wait.
    // Otherwise the NvxSurface might be read before the blit is completed
    Nvx2dFlush(pDstDdk2dSurface);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    return OMX_ErrorNone;
}


OMX_ERRORTYPE HandleStoreMetaDataInBuffersParamLite(void *pParam,
                                                SNvxNvMMLitePort *pPort,
                                                OMX_U32 PortAllowed)
{
    StoreMetaDataInBuffersParams *pStoreMetaData =
        (StoreMetaDataInBuffersParams*)pParam;

    if (!pStoreMetaData || pStoreMetaData->nPortIndex != PortAllowed)
        return OMX_ErrorBadParameter;

    pPort[PortAllowed].bUsesAndroidMetaDataBuffers =
        pStoreMetaData->bStoreMetaData;

    // Do not allow for decode case
    if (PortAllowed != s_nvx_port_out)
        pPort[PortAllowed].bEmbedNvMMBuffer = pStoreMetaData->bStoreMetaData;

    return OMX_ErrorNone;
}

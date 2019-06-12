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
#include "nvmmtransformbase.h"
#include <nvassert.h>
}

#include "nvxandroidbuffer.h"

#include <media/hardware/HardwareAPI.h>

using namespace android;

OMX_ERRORTYPE HandleEnableANB(NvxComponent *pNvComp, OMX_U32 portallowed,
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
        // TODO: Disable usebuffer/etc, store old format, mark as allowed
        pPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)
                                                    (HAL_PIXEL_FORMAT_YV12);
    }
    else
    {
        pPort->oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE HandleUseANB(NvxComponent *pNvComp, void *ANBParam)
{
    UseAndroidNativeBufferParams *uanbp;
    uanbp = (UseAndroidNativeBufferParams *)ANBParam;

    return NvxUseBufferInt(pNvComp->hBaseComponent,
                           uanbp->bufferHeader, uanbp->nPortIndex,
                           uanbp->pAppPrivate, 0,
                           (OMX_U8 *)uanbp->nativeBuffer.get(),
                           NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T);
}

OMX_ERRORTYPE HandleUseNBH(NvxComponent *pNvComp, void *NBHParam)
{
    NVX_PARAM_USENATIVEBUFFERHANDLE *unbhp;
    unbhp = (NVX_PARAM_USENATIVEBUFFERHANDLE *)NBHParam;

    return NvxUseBufferInt(pNvComp->hBaseComponent,
                           unbhp->bufferHeader, unbhp->nPortIndex,
                           NULL, 0,
                           (OMX_U8 *)unbhp->oNativeBufferHandlePtr,
                           NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T);
}

OMX_ERRORTYPE ImportAllANBs(NvxComponent *pNvComp,
                            SNvxNvMMTransformData *pData,
                            OMX_U32 streamIndex,
                            OMX_U32 matchWidth,
                            OMX_U32 matchHeight)
{
    SNvxNvMMPort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    OMX_BUFFERHEADERTYPE *pBuffer;
    NvxList *pOutBufList = pPortOut->pEmptyBuffers;
    NvxListItem *cur = NULL;

    NvxMutexLock(pPortOut->hMutex);

    pBuffer = pPortOut->pCurrentBufferHdr;
    if (pBuffer)
    {
        ImportANB(pNvComp, pData, streamIndex, pBuffer,
                  matchWidth, matchHeight);
    }

    NvOsMutexLock(pOutBufList->oLock);
    cur = pOutBufList->pHead;
    while (cur)
    {
        pBuffer = (OMX_BUFFERHEADERTYPE *)cur->pElement;
        ImportANB(pNvComp, pData, streamIndex, pBuffer,
                  matchWidth, matchHeight);
        cur = cur->pNext;
    }
    NvOsMutexUnlock(pOutBufList->oLock);

    NvxMutexUnlock(pPortOut->hMutex);

    NvOsDebugPrintf("imported: %d buffers\n", pPort->nBufsTot);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE ImportANB(NvxComponent *pNvComp,
                        SNvxNvMMTransformData *pData,
                        OMX_U32 streamIndex,
                        OMX_BUFFERHEADERTYPE *buffer,
                        OMX_U32 matchWidth,
                        OMX_U32 matchHeight)
{
    android_native_buffer_t *anbuffer;
    NvMMBuffer *nvmmbuf = NULL;
    buffer_handle_t handle;
    OMX_U32 width, height;
    NvRmDeviceHandle hRm = NvxGetRmDevice();
    NvMMSurfaceDescriptor *pSurfaces;
    SNvxNvMMPort *pPort;
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    const NvRmSurface *Surf;
    size_t SurfCount;

    if (pData->oPorts[streamIndex].bUsesNBHs)
    {
        buffer_handle_t *hBuffer = (buffer_handle_t *)(buffer->pBuffer);
        if (!(buffer->pBuffer))
            return OMX_ErrorBadParameter;

        handle = (*hBuffer);
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

    nvgr_get_surfaces(handle, &Surf, &SurfCount);
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
        nvmmbuf = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
        if (!nvmmbuf)
        {
            return OMX_ErrorInsufficientResources;
        }

        NvOsMemset(nvmmbuf, 0, sizeof(NvMMBuffer));

        nvmmbuf->StructSize = sizeof(NvMMBuffer);
        nvmmbuf->BufferID = (pData->oPorts[streamIndex].nBufsANB)++;
        nvmmbuf->PayloadType = NvMMPayloadType_SurfaceArray;
        NvOsMemset(&nvmmbuf->PayloadInfo, 0, sizeof(nvmmbuf->PayloadInfo));

        if (pData->InterlaceStream != OMX_TRUE)
        {
            pData->oPorts[streamIndex].pBuffers[nvmmbuf->BufferID] = nvmmbuf;
            pData->oPorts[streamIndex].nBufsTot++;
        }
        pPrivateData->nvmmBuffer = nvmmbuf;
    }

    nvmmbuf = (NvMMBuffer *)(pPrivateData->nvmmBuffer);

    // Wait on pre-fence stored in the buffer
    NvxWaitPreFenceANW(handle, GRALLOC_USAGE_HW_RENDER);

    pSurfaces = &(nvmmbuf->Payload.Surfaces);

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
    if (OMX_FALSE == pPrivateData->nvmmBufIsPinned)
    {
        pPrivateData->nvmmBufIsPinned = OMX_TRUE;
        pSurfaces->PhysicalAddress[0] = NvRmMemPin(pSurfaces->Surfaces[0].hMem);
        pSurfaces->PhysicalAddress[1] = pSurfaces->PhysicalAddress[0] +
                                        pSurfaces->Surfaces[1].Offset;
        pSurfaces->PhysicalAddress[2] = pSurfaces->PhysicalAddress[0] +
                                        pSurfaces->Surfaces[2].Offset;
    }
    pSurfaces->SurfaceCount = SurfCount;
    pSurfaces->Empty = NV_TRUE;

    if (pData->InterlaceStream == OMX_TRUE)
    {
        pData->vmr.Renderstatus[nvmmbuf->BufferID] = FRAME_VMR_FREE;
        pData->vmr.pVideoSurf[nvmmbuf->BufferID] = nvmmbuf;
        NvOsSemaphoreSignal(pData->InpOutAvail);
    }
    else
    {
        pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                                     NvMMBufferType_Payload, sizeof(NvMMBuffer),
                                     nvmmbuf);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE ExportANB(NvxComponent *pNvComp,
                        NvMMBuffer *nvmmbuf,
                        OMX_BUFFERHEADERTYPE **pOutBuf,
                        NvxPort *pPortOut,
                        OMX_BOOL bFreeBacking)
{
    // find the matching OMX buffer
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

    if (buf->PayloadInfo.BufferMetaDataType ==
            NvMMBufferMetadataType_DigitalZoom)
    {
        if (buf->PayloadInfo.BufferMetaData.DigitalZoomBufferMetadata.KeepFrame)
        {
            omxbuf->nFlags |= OMX_BUFFERFLAG_POSTVIEW;
        }
        else
        {
            omxbuf->nFlags &= ~OMX_BUFFERFLAG_POSTVIEW;
        }
    }

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
        anbuffer = (android_native_buffer_t *)omxbuf->pBuffer;
        if (anbuffer)
        {
            handle = anbuffer->handle;
        }
    }
    else if (pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T)
    {
        buffer_handle_t *hBuffer = (buffer_handle_t *)omxbuf->pBuffer;
        handle = *hBuffer;
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

        nvgr_set_stereo_info(handle, StereoInfo);
        nvgr_set_video_timestamp(handle, omxbuf->nTimeStamp);
    }
/*
    anbuffer = (android_native_buffer_t *)(omxbuf->pBuffer);
    if (!anbuffer)
        return OMX_ErrorBadParameter;

    if (pPrivateData->nvmmBufIsPinned)
        NvRmMemUnpin(buf->Payload.Surfaces.Surfaces[0].hMem);
    pPrivateData->nvmmBufIsPinned = OMX_FALSE;
*/

    if (bFreeBacking)
    {
        NvOsFree(buf);
        pPrivateData->nvmmBuffer = NULL;
    }

    omxbuf->nFilledLen = sizeof(buffer_handle_t);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE FreeANB(NvxComponent *pNvComp,
                      SNvxNvMMTransformData *pData,
                      OMX_U32 streamIndex,
                      OMX_BUFFERHEADERTYPE *buffer)

{
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    NvMMBuffer *nvmmbuf;

    pPrivateData = (NvxBufferPlatformPrivate *)buffer->pPlatformPrivate;
    nvmmbuf = (NvMMBuffer *)pPrivateData->nvmmBuffer;

    if (!nvmmbuf)
        return OMX_ErrorNone;

    // This assumes that we'll be freeing _all_ buffers at once

    pData->oPorts[streamIndex].nBufsANB--;
    if(pData->InterlaceStream != OMX_TRUE)
    {
        pData->oPorts[streamIndex].nBufsTot--;
        pData->oPorts[streamIndex].pBuffers[nvmmbuf->BufferID] = NULL;
    }
    if (pPrivateData->nvmmBufIsPinned)
        NvRmMemUnpin(nvmmbuf->Payload.Surfaces.Surfaces[0].hMem);
    pPrivateData->nvmmBufIsPinned = OMX_FALSE;

    NvOsFree(nvmmbuf);
    pPrivateData->nvmmBuffer = NULL;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE HandleGetANBUsage(NvxComponent *pNvComp,
                                void *ANBParam,
                                OMX_U32 SWAccess)
{
    GetAndroidNativeBufferUsageParams *ganbup;
    ganbup = (GetAndroidNativeBufferUsageParams *)ANBParam;

    if(SWAccess)
        ganbup->nUsage = GRALLOC_USAGE_SW_READ_OFTEN;
    else
        ganbup->nUsage = 0;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE CopyNvxSurfaceToANB(NvxComponent *pNvComp,
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


OMX_ERRORTYPE CopyANBToNvxSurface(NvxComponent *pNvComp,
                                  buffer_handle_t handle,
                                  NvxSurface *pDestSurface)
{
    NvError Err;
    NvDdk2dBlitParameters TexParam;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle h2d = NvxGet2d();
    NvU32 width, height;
    NvDdk2dSurfaceType SurfaceType;
    NvRmFence fences[OMX_MAX_FENCES];
    NvU32 numFences;
    const NvRmSurface *Surf;
    size_t SurfCount;

    if (!h2d)
        return OMX_ErrorBadParameter;

    NvOsMemset(&TexParam, 0, sizeof(NvDdk2dBlitParameters));

    if (!handle)
       return OMX_ErrorBadParameter;

    nvgr_get_surfaces(handle, &Surf, &SurfCount);
    width = NV_MIN(Surf[0].Width, pDestSurface->Surfaces[0].Width);
    height = NV_MIN(Surf[0].Height, pDestSurface->Surfaces[0].Height);

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

    Err = NvDdk2dSurfaceCreate(h2d, SurfaceType, (NvRmSurface *)Surf,
                               &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }

    // Fetches the SurfaceFlinger fences from the gralloc buffer
    NvxGetPreFencesANW(handle, fences, &numFences, GRALLOC_USAGE_HW_TEXTURE);

    // Add the fences to the source surface
    NvDdk2dSurfaceLock(pSrcDdk2dSurface, NvDdk2dSurfaceAccessMode_Write,
                       NULL, NULL, NULL);
    NvDdk2dSurfaceUnlock(pSrcDdk2dSurface, fences, numFences);

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

    // Wait for the blit to complete.
    Nvx2dFlush(pDstDdk2dSurface);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
        NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();

    return OMX_ErrorNone;
}


OMX_ERRORTYPE HandleStoreMetaDataInBuffersParam(void *pParam,
                                                SNvxNvMMPort *pPort,
                                                OMX_U32 PortAllowed)
{
    StoreMetaDataInBuffersParams *pStoreMetaData =
        (StoreMetaDataInBuffersParams*)pParam;

    if (!pStoreMetaData || pStoreMetaData->nPortIndex != PortAllowed)
        return OMX_ErrorBadParameter;

    pPort[PortAllowed].bUsesAndroidMetaDataBuffers =
        pStoreMetaData->bStoreMetaData;


    return OMX_ErrorNone;
}

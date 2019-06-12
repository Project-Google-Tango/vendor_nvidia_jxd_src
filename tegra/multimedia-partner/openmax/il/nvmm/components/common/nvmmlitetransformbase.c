/* Copyright (c) 2010-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <nvassert.h>

#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvrm_surface.h"
#include "nvrm_hardware_access.h"
#include "nvmmlitetransformbase.h"
#include "NvxHelpers.h"
#include "nvxeglimage.h"
#include "NvxTrace.h"

#include "nvmm.h"
#include "nvmmlite_videodec.h"
#include "nvmm_videoenc.h"

#if USE_ANDROID_NATIVE_WINDOW
#include "nvxliteandroidbuffer.h"
#include "nvxhelpers_int.h"
#include <media/hardware/MetadataBufferType.h>
#endif
#include "nvfxmath.h"


/* Local statics */

#define NUM_METABUFS       7
#define NUM_METABUFENTRIES 5

#define JPEG_DEFAULT_SURFACE_WIDTH   1024
#define JPEG_DEFAULT_SURFACE_HEIGHT   768

#define EXTRACT_A_BYTE_AT_BIT_N(Word, N)    (0xFF & ((NvU32)Word >> N))
#define CONVERT_TO_BIG_ENDIAN(Word)         (EXTRACT_A_BYTE_AT_BIT_N(Word, 24) << 0)    | \
                                            (EXTRACT_A_BYTE_AT_BIT_N(Word, 16) << 8)    | \
                                            (EXTRACT_A_BYTE_AT_BIT_N(Word, 8) << 16)    | \
                                            (EXTRACT_A_BYTE_AT_BIT_N(Word, 0) << 24)

static NvError NvxNvMMLiteTransformReturnEmptyInput(void *pContext, NvU32 streamIndex,
                                                NvMMBufferType BufferType, NvU32 BufferSize, void *pBuffer);
NvError NvxNvMMLiteTransformDeliverFullOutput(void *pContext, NvU32 streamIndex,
                                                 NvMMBufferType BufferType, NvU32 BufferSize, void *pBuffer);
static NvError NvxNvMMLiteTransformSendBlockEventFunction(void *pContext, NvU32 eventType,
                                                      NvU32 eventSize, void *eventInfo);
static void NvxVideoDecSurfaceFree(NvMMBuffer *pMMBuffer);
static NvError NvxVideoSurfaceAlloc(NvRmDeviceHandle hRmDev,
                    NvMMBuffer *pMMBuffer, NvU32 BufferID, NvU32 Width,
                    NvU32 Height, NvU32 ColourFormat, NvU32 Layout,
                    NvU32 *pImageSize, NvBool UseAlignment,
                    NvOsMemAttribute Coherency, NvBool UseNV21);
static NvError NvxNvMMLiteTransformAllocateSurfaces(SNvxNvMMLiteTransformData *pData,
                                                NvMMLiteEventNewVideoStreamFormatInfo *pVideoStreamInfo,
                                                OMX_U32 streamIndex);

#ifdef BUILD_GOOGLETV
static NvError NvxNvMMLiteTransformCreatePreviousBuffList(SNvxNvMMLitePort *pPort);
#endif

//FIXME: Goes away once block names don't have to be unique
static int s_nBlockCount = 0;

#define EMPTY_BUFFER_ID 0x7FFFFFFF

static void FlushOutputQueue(SNvxNvMMLiteTransformData *pData, SNvxNvMMLitePort *pPort);

OMX_ERRORTYPE NvxNvMMLiteTransformSetProfile(SNvxNvMMLiteTransformData *pData, NVX_CONFIG_PROFILE *pProf)
{
    pData->bProfile = pProf->bProfile;
    pData->bVerbose = pProf->bVerbose;
    pData->bDiscardOutput = pProf->bStubOutput;
    pData->nForceLocale = pProf->nForceLocale;
    pData->nNvMMLiteProfile = pProf->nNvMMProfile;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetFileName(SNvxNvMMLiteTransformData *pData, NVX_PARAM_FILENAME *pProf)
{
    pData->fileName = pProf->pFilename;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetFileCacheSize(SNvxNvMMLiteTransformData *pData, OMX_U32 nSize)
{
    pData->nFileCacheSize = nSize;
    pData->bSetFileCache = OMX_TRUE;

#if 0
    if (pData->hBlock)
    {
        NvMMLiteAttrib_FileCacheSize oFCSize;

        NvOsMemset(&oFCSize, 0, sizeof(NvMMLiteAttrib_FileCacheSize));

        oFCSize.nFileCacheSize = nSize;

        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMLiteAttribute_FileCacheSize,
            NvMMLiteSetAttrFlag_Notification,
            sizeof(NvMMLiteAttrib_FileCacheSize),
            &oFCSize);

    }

#endif
    return OMX_ErrorNone;
}

typedef struct _BufferMarkData {
    OMX_HANDLETYPE hMarkTargetComponent;
    OMX_PTR pMarkData;
    OMX_TICKS nTimeStamp;
    OMX_U32 nFlags;
    OMX_PTR pMetaDataSource;
} BufferMarkData;

static OMX_ERRORTYPE NvxNvMMLiteSendNextInputBufferBack(SNvxNvMMLiteTransformData *pData, int nNumStream)
{
    NvMMBuffer *pNvxBuffer = NULL;
    SNvxNvMMLitePort *pPort;

    pPort = &pData->oPorts[nNumStream];

    if (NvMMQueueGetNumEntries(pData->pInputQ[nNumStream]))
    {
        NvMMQueueDeQ(pData->pInputQ[nNumStream], &pNvxBuffer);
    }
    else
    {
        NvOsDebugPrintf("ERROR: Input Queue Empty");
        return OMX_ErrorUnderflow;
    }

    if (pNvxBuffer->PayloadType != NvMMPayloadType_SurfaceArray)
    {
        pNvxBuffer->Payload.Ref.startOfValidData = 0;
        pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    }
    pPort->nBufsInUse++;

    pData->TransferBufferToBlock(pData->hBlock, nNumStream,
        NvMMBufferType_Payload,
        sizeof(NvMMBuffer), pNvxBuffer);
    //Trigger Nvmmlite block worker function
    NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));

    return OMX_ErrorNone;
}

#if USE_ANDROID_NATIVE_WINDOW
static OMX_U32 U32_AT(const uint8_t *ptr)
{
    return ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];
}
#endif

static NvU64 normTS(OMX_TICKS inTS)
{
    if (inTS >= 0)
        return inTS * 10;
    return -1;
}

static OMX_ERRORTYPE NvxNvMMLiteProcessInputBuffer(SNvxNvMMLiteTransformData *pData, int nNumStream, OMX_BOOL *pbNextWouldFail)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMBuffer *pNvxBuffer = NULL;
    OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
    SNvxNvMMLitePort *pPort;
    NvU32 nWorkingBuffer = 0;
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    OMX_BOOL bSentData = OMX_FALSE;
    NvU32 Pending_buffers=0;
    OMX_BOOL bProcessSecureBuf = OMX_TRUE;

    // FIXME: This will go away once GTV TVP support is complete
#ifdef BUILD_GOOGLETV
    bProcessSecureBuf = OMX_FALSE;
#endif

    pPort = &pData->oPorts[nNumStream];

    if (pPort->nType != TF_TYPE_INPUT || pPort->nBufsInUse >= pPort->nBufsTot
         || !NvMMQueueGetNumEntries(pData->pInputQ[nNumStream]))
    {
        if (pbNextWouldFail)
            *pbNextWouldFail = OMX_TRUE;
        return OMX_ErrorNotReady;
    }

    pOMXBuf = pPort->pOMXPort->pCurrentBufferHdr;
    if (!pOMXBuf)
        return OMX_ErrorNotReady;

    pPrivateData = (NvxBufferPlatformPrivate *)pOMXBuf->pPlatformPrivate;
    if (!pPrivateData)
        return OMX_ErrorNotReady;

    if (pOMXBuf->nFilledLen > 0 ||
        pPrivateData->eType != NVX_BUFFERTYPE_NORMAL)
    {
        // Fill buffers for consumption
        if (!pPort->bSkipWorker)
            NvxMutexLock(pPort->oMutex);

        if (NvMMQueueGetNumEntries(pData->pInputQ[nNumStream]))
        {
            NvMMQueueDeQ(pData->pInputQ[nNumStream], &pNvxBuffer);
        }
        else
        {
            NvOsDebugPrintf("ERROR: Input Queue Empty");
            return OMX_ErrorUnderflow;
        }
        pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
        pNvxBuffer->PayloadInfo.BufferFlags = 0;
        pPort->pOMXBufMap[pNvxBuffer->BufferID] = pOMXBuf;
        nWorkingBuffer = pNvxBuffer->BufferID;
        pPort->nBufsInUse++;

        if (pbNextWouldFail)
            *pbNextWouldFail = pPort->nBufsInUse >= pPort->nBufsTot || \
                               !NvMMQueueGetNumEntries(pData->pInputQ[nNumStream]);
        if (!pPort->bSkipWorker)
            NvxMutexUnlock(pPort->oMutex);

        if (pPrivateData->eType == NVX_BUFFERTYPE_EGLIMAGE)
        {
            // Stretch Blit EGL source to NvMMLite surface
            NvxEglImageSiblingHandle hEglSib =
                (NvxEglImageSiblingHandle)pOMXBuf->pBuffer;
            NvxEglStretchBlitFromImage(hEglSib,
                                       &pNvxBuffer->Payload.Surfaces,
                                       NV_TRUE, NULL);
        }
#if USE_ANDROID_NATIVE_WINDOW
        else if (pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T ||
                pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T)
        {
            buffer_handle_t handle;
            const NvRmSurface *Surf;
            int bufferIsYUV;

            if (pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T)
            {
                android_native_buffer_t *anbuffer = (android_native_buffer_t *)(pOMXBuf->pBuffer);
                if (!anbuffer)
                {
                    return OMX_ErrorStreamCorrupt;
                }
                handle = anbuffer->handle;
            }
            else
            {
                handle = *(buffer_handle_t *)(pOMXBuf->pBuffer);
            }

            nvgr_get_surfaces(handle, &Surf, NULL);
            bufferIsYUV = nvgr_is_yuv(handle);
            if (!pPort->bEmbedANBSurface &&
                (bufferIsYUV &&
                 (Surf[0].Width == pPort->nWidth) &&
                 (Surf[0].Height == pPort->nHeight))) {
                // Wont require 2D blit, ANB surface can directly be passed
                // Free allocated surfaces
                int i;
                for (i = 0; i < pPort->nBufsTot; i++)
                    NvxVideoDecSurfaceFree(pPort->pBuffers[i]);
                pPort->bEmbedANBSurface = OMX_TRUE;
            } else if (pPort->bEmbedANBSurface &&
                       (!bufferIsYUV ||
                        (Surf[0].Width != pPort->nWidth) ||
                        (Surf[0].Height != pPort->nHeight))) {
                // Resolution changed? Need 2D blit
                NvxNvMMLiteTransformAllocateSurfaces(pData,
                        &pData->VideoStreamInfo,
                        nNumStream);
                pPort->bEmbedANBSurface = OMX_FALSE;
            }

            CopyANBToNvxSurfaceLite(pData->pParentComp,
                                    handle,
                                    &(pNvxBuffer->Payload.Surfaces),
                                    pPort->bEmbedANBSurface);
        }
#endif
        else if (pPort->bUsesAndroidMetaDataBuffers)
        {
#if USE_ANDROID_NATIVE_WINDOW
            OMX_U32 nMetadataBufferFormat;

            if (pOMXBuf->nFilledLen <= 4)
            {
                return OMX_ErrorStreamCorrupt;
            }

            nMetadataBufferFormat = U32_AT(pOMXBuf->pBuffer);

            switch (nMetadataBufferFormat)
            {
                case kMetadataBufferTypeCameraSource:
                {
                    OMX_BUFFERHEADERTYPE *pEmbeddedOMXBuffer = NULL;
                    NvMMBuffer *pEmbeddedNvmmBuffer = NULL;

                    if (pOMXBuf->nFilledLen != 4 + sizeof(OMX_BUFFERHEADERTYPE))
                    {
                        return OMX_ErrorStreamCorrupt;
                    }

                    pEmbeddedOMXBuffer = (OMX_BUFFERHEADERTYPE*)(pOMXBuf->pBuffer + 4);
                    if (!pEmbeddedOMXBuffer)
                        return OMX_ErrorStreamCorrupt;

                    pEmbeddedNvmmBuffer = (NvMMBuffer*)(pEmbeddedOMXBuffer->pBuffer);
                    if (!pEmbeddedNvmmBuffer)
                        return OMX_ErrorStreamCorrupt;

                    if (pPort->bEnc2DCopy)
                    {
                        NvRect DestRect;
                        NvRect SrcRect;
                        NvOsMemset(&SrcRect, 0, sizeof(NvRect));
                        NvOsMemset(&DestRect, 0, sizeof(NvRect));
                        DestRect.right = pNvxBuffer->Payload.Surfaces.Surfaces[0].Width-1;
                        DestRect.bottom = pNvxBuffer->Payload.Surfaces.Surfaces[0].Height-1;
                        SrcRect.right = pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[0].Width-1;
                        SrcRect.bottom = pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[0].Height-1;

                        NV_DEBUG_PRINTF(("%s bEnc2DCopy copying %dx%d -> %dx%d backgrounddonemap:%x",
                            __FUNCTION__,
                            SrcRect.right,
                            SrcRect.bottom,
                            DestRect.right,
                            DestRect.bottom,
                            pPort->nBackgroundDoneMap));

                        NvxMutexLock(pPort->hVPPMutex);
                        NvxCopyMMSurfaceDescriptor2DProc(&pNvxBuffer->Payload.Surfaces, DestRect,
                            &pEmbeddedNvmmBuffer->Payload.Surfaces, SrcRect, &pPort->Video2DProc,
                            (((1 << pPort->nBufsTot) - 1) != pPort->nBackgroundDoneMap));
                        pPort->nBackgroundDoneMap |= (1 << nWorkingBuffer);
                        NvxMutexUnlock(pPort->hVPPMutex);
                    }
                    else
                    {
                        // swap the surface
                        pNvxBuffer->Payload.Surfaces =
                            pEmbeddedNvmmBuffer->Payload.Surfaces;
                        pNvxBuffer->PayloadInfo = pEmbeddedNvmmBuffer->PayloadInfo;
                        pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
                    }
                    break;
                }
                case kMetadataBufferTypeGrallocSource:
                {
                    buffer_handle_t *pEmbeddedGrallocSrc = (buffer_handle_t *)(pOMXBuf->pBuffer+ 4);
                    buffer_handle_t handle = (*pEmbeddedGrallocSrc);
                    const NvRmSurface *Surf;
                    size_t SurfCount;
                    NvBool bSkip2DBlit = NV_FALSE;

                    nvgr_get_surfaces(handle, &Surf, &SurfCount);

                    if (Surf[0].Width == pPort->nWidth &&
                        Surf[0].Height == pPort->nHeight &&
                        SurfCount == 3)
                    {
                        bSkip2DBlit = NV_TRUE;
                    }


                    if (pPort->bEmbedNvMMBuffer == OMX_TRUE && bSkip2DBlit != NV_TRUE)
                    {
                        if ((pData->oBlockType == NvMMLiteBlockType_EncH264) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncJPEG) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncMPEG4) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncH263) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncVP8))
                            {
                                NvU32 i = 0;
                                NvU32 ImageSize = 0;
                                NvU32 err = NvSuccess;
                                NvBool UseAlignment = NV_TRUE;

                                for (i = 0; i < pPort->nBufsTot; i++)
                                {
                                    UseAlignment = NV_FALSE;
                                    err = NvxVideoSurfaceAlloc(pData->hRmDevice,
                                        pPort->pBuffers[i], i,
                                        pPort->nWidth,
                                        pPort->nHeight,
                                        pPort->pOMXPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar ?
                                            NvMMVideoDecColorFormat_YUV420SemiPlanar : NvMMVideoDecColorFormat_YUV420,
                                        Surf[0].Layout,
                                        &ImageSize,
                                        UseAlignment, NvOsMemAttribute_WriteCombined, NV_FALSE);
                                    if (err != NvSuccess)
                                    {
                                        NV_DEBUG_PRINTF(("Unable to allocate the memory"
                                                " for the Input buffer !"));
                                        return OMX_ErrorInsufficientResources;
                                    }
                                }
                                pPort->bEmbedNvMMBuffer = OMX_FALSE;

                            }
                    }

                    CopyANBToNvxSurfaceLite(pData->pParentComp,
                                            *pEmbeddedGrallocSrc,
                                            &(pNvxBuffer->Payload.Surfaces),
                                            pPort->bEmbedNvMMBuffer);

                    pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
                    break;
                }
                case kMetadataBufferTypeEglStreamSource:
                {
                    NV_DEBUG_PRINTF(("Meta Data Buffer Format is Egl Source \n"));
                    OMX_BUFFERHEADERTYPE *pEmbeddedOMXBuffer = NULL;
                    NvMMBuffer *pEmbeddedNvmmBuffer = NULL;

                    pEmbeddedOMXBuffer = (OMX_BUFFERHEADERTYPE*)(pOMXBuf->pBuffer + 4);

                    if ((!pPort->bEnc2DCopy) &&
                        (pPort->bEmbedNvMMBuffer == OMX_FALSE) &&
                       ((pData->oBlockType == NvMMLiteBlockType_EncH264) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncJPEG) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncMPEG4)||
                        (pData->oBlockType == NvMMLiteBlockType_EncH263) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncVP8)))
                    {
                        // Surfaces are provided by eglstream media source free
                        // Omx allocated & set embedded buffer flag.
                        NvU32 i;
                        for (i = 0; i < pPort->nBufsTot; i++)
                            NvxVideoDecSurfaceFree(pPort->pBuffers[i]);
                        pPort->bEmbedNvMMBuffer = OMX_TRUE;
                    }

                    if (!pEmbeddedOMXBuffer)
                        return OMX_ErrorStreamCorrupt;
                    if (pEmbeddedOMXBuffer->nFilledLen != sizeof(NvMMBuffer))
                        return OMX_ErrorStreamCorrupt;

                    // retrieve the embedded Nvmm Buffer
                    pEmbeddedNvmmBuffer = (NvMMBuffer*)(pOMXBuf->pBuffer + 4 +
                        sizeof(OMX_BUFFERHEADERTYPE));

                    if (!pEmbeddedNvmmBuffer)
                        return OMX_ErrorStreamCorrupt;

                    // retreive the hMem which was sent as pPlatFormPrivate data in the OMX Buffer
                    eError= NvRmMemHandleFromFd(pEmbeddedOMXBuffer->pPlatformPrivate,
                        &pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[0].hMem);
                    if (eError != OMX_ErrorNone)
                        return OMX_ErrorStreamCorrupt;

                    pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[2].hMem =
                        pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[0].hMem;
                    pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[1].hMem =
                        pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[0].hMem;

                    // swap the surface
                    pNvxBuffer->Payload.Surfaces =
                        pEmbeddedNvmmBuffer->Payload.Surfaces;
                    pNvxBuffer->PayloadInfo = pEmbeddedNvmmBuffer->PayloadInfo;
                    pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
                    break;
                }
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
                case KMetadataVideoEditorBufferTypeGrallocSource:
                {
                    NvError Err;
                    NvRect SrcRect, DstRect;
                    int32_t sourceWidth, sourceHeight;
                    NvMMSurfaceDescriptor *pSurfaceDescriptor;
                    buffer_handle_t handle;
                    size_t SurfCount;
                    const NvRmSurface *Surf;

                    if (pPort->bEmbedNvMMBuffer == OMX_TRUE)
                    {
                        if ((pData->oBlockType == NvMMLiteBlockType_EncH264) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncJPEG) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncMPEG4) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncH263) ||
                            (pData->oBlockType == NvMMLiteBlockType_EncVP8))
                        {
                            NvU32 i = 0;
                            NvU32 ImageSize = 0;
                            NvU32 err = NvSuccess;
                            NvBool UseAlignment = NV_TRUE;
                            for (i = 0; i < pPort->nBufsTot; i++)
                            {
                                UseAlignment = NV_FALSE;
                                err = NvxVideoSurfaceAlloc(pData->hRmDevice,
                                    pPort->pBuffers[i], i,
                                    pPort->nWidth,
                                    pPort->nHeight,
                                    pPort->pOMXPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar ?
                                        NvMMVideoDecColorFormat_YUV420SemiPlanar : NvMMVideoDecColorFormat_YUV420,
                                    NvRmSurfaceGetDefaultLayout(),
                                    &ImageSize,
                                    UseAlignment, NvOsMemAttribute_WriteCombined, NV_FALSE);
                                if (err != NvSuccess)
                                {
                                    NV_DEBUG_PRINTF(("Unable to allocate the memory"
                                            " for the Input buffer !"));
                                    return OMX_ErrorInsufficientResources;
                                }
                            }
                            pPort->bEmbedNvMMBuffer = OMX_FALSE;
                        }
                    }

                    sourceWidth  = U32_AT(pOMXBuf->pBuffer+4);
                    sourceHeight = U32_AT(pOMXBuf->pBuffer+8);
                    handle = (buffer_handle_t) (pOMXBuf->pBuffer+12);

                    pSurfaceDescriptor = (NvMMSurfaceDescriptor *)malloc(sizeof(NvMMSurfaceDescriptor));
                    NvOsMemset(pSurfaceDescriptor, 0, sizeof(NvMMSurfaceDescriptor));

                    // This custom version of get_surfaces skips the shared memory buffer
                    // validation since the pointer is invalid in this embedded struct.
                    nvgr_get_surfaces_no_buffer_magic(handle, &Surf, &SurfCount);
                    NvOsMemcpy(&pSurfaceDescriptor->Surfaces, Surf,
                               SurfCount * sizeof(NvRmSurface));

                    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
                    NvOsMemset(&DstRect, 0, sizeof(NvRect));

                    SrcRect.right = sourceWidth;
                    SrcRect.bottom = sourceHeight;
                    DstRect.right = pNvxBuffer->Payload.Surfaces.Surfaces[0].Width;
                    DstRect.bottom = pNvxBuffer->Payload.Surfaces.Surfaces[0].Height;
                    pSurfaceDescriptor->SurfaceCount = SurfCount;

                    NvxCopyMMSurfaceDescriptor(&pNvxBuffer->Payload.Surfaces, DstRect,
                            pSurfaceDescriptor, SrcRect);
                    free(pSurfaceDescriptor);

                    pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
                    break;
                }
#endif
                default: return OMX_ErrorStreamCorrupt;
            }
#else
            // we have an OMXBuffer embedded into OMXBuffer.
            if (pOMXBuf->nFilledLen == sizeof(OMX_BUFFERHEADERTYPE))
            {
                OMX_BUFFERHEADERTYPE *pEmbeddedOMXBuffer = NULL;
                NvMMBuffer *pEmbeddedNvmmBuffer = NULL;

                pEmbeddedOMXBuffer = (OMX_BUFFERHEADERTYPE*)(pOMXBuf->pBuffer);
                if (!pEmbeddedOMXBuffer)
                    return OMX_ErrorStreamCorrupt;

                pEmbeddedNvmmBuffer = (NvMMBuffer*)(pEmbeddedOMXBuffer->pBuffer);
                if (!pEmbeddedNvmmBuffer)
                    return OMX_ErrorStreamCorrupt;

                // swap the surface
                pNvxBuffer->Payload.Surfaces =
                    pEmbeddedNvmmBuffer->Payload.Surfaces;
                pNvxBuffer->PayloadInfo = pEmbeddedNvmmBuffer->PayloadInfo;

                pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
            }
            else
            {
                return OMX_ErrorStreamCorrupt;
            }
#endif
        }
        else if ( pPort->bUsesRmSurfaces )
        {
            NvU32 stereoFlags;
            NvMMBuffer *pMMIn = (NvMMBuffer *)pOMXBuf->pBuffer;
            pNvxBuffer->Payload.Surfaces = pMMIn->Payload.Surfaces;
            pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes = pMMIn->Payload.Ref.sizeOfValidDataInBytes;
            pNvxBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
            // Set EXIF info
            if(pMMIn->PayloadInfo.BufferMetaDataType == NvMMBufferMetadataType_EXIFData)
            {
                NvOsMemcpy(&pNvxBuffer->PayloadInfo,
                            &pMMIn->PayloadInfo,
                            sizeof(NvMMPayloadMetadata));
            }
            // StereoInfo field has stereo information stored as NvStereoType
            // Extract stereo flags from there and add them to buffer's flags
            stereoFlags = NvRmStereoTypeToNvMmStereoFlags(pPrivateData->StereoInfo);
            pNvxBuffer->PayloadInfo.BufferFlags |= stereoFlags;
        }
        else if (pNvxBuffer->PayloadType == NvMMPayloadType_SurfaceArray)
        {
            // this input port formats data into surface planes:
            if ((pOMXBuf->nFlags & OMX_BUFFERFLAG_NV_BUFFER) || (pPort->bEmbedNvMMBuffer))
            {
                // we have NvMMLite Buffer embedded into OMXBuffer.
                if(pOMXBuf->nFilledLen == sizeof(NvMMBuffer))
                {
                    NvMMBuffer Embeddedbuf;
                    NvOsMemcpy(&Embeddedbuf, pOMXBuf->pBuffer, sizeof(NvMMBuffer));
                    if (pPort->bEnc2DCopy)
                    {
                        // implement 2d copy code.
                        // Compute SB settings
                        NvRect DestRect;
                        NvRect SrcRect;
                        NvOsMemset(&SrcRect, 0, sizeof(NvRect));
                        NvOsMemset(&DestRect, 0, sizeof(NvRect));
                        DestRect.right = pNvxBuffer->Payload.Surfaces.Surfaces[0].Width;
                        DestRect.bottom = pNvxBuffer->Payload.Surfaces.Surfaces[0].Height;
                        SrcRect.right = Embeddedbuf.Payload.Surfaces.Surfaces[0].Width;
                        SrcRect.bottom = Embeddedbuf.Payload.Surfaces.Surfaces[0].Height;

                        NvxCopyMMSurfaceDescriptor(&pNvxBuffer->Payload.Surfaces, DestRect,
                            &Embeddedbuf.Payload.Surfaces, SrcRect);
                    }
                    else
                    {
                        // swap the surface
                        pNvxBuffer->Payload.Surfaces = Embeddedbuf.Payload.Surfaces;
                        pNvxBuffer->PayloadInfo = Embeddedbuf.PayloadInfo;
                        if (pOMXBuf->nFlags & OMX_BUFFERFLAG_RETAIN_OMX_TS) {
                            pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
                        }
                    }
                }
            }
            else if(pData->bEmbedRmSurface)
            {
                NvOsMemcpy( pNvxBuffer->Payload.Surfaces.Surfaces,
                            pOMXBuf->pBuffer,
                            NVMMSURFACEDESCRIPTOR_MAX_SURFACES* sizeof(NvRmSurface));
                if (pOMXBuf->nFlags & OMX_BUFFERFLAG_RETAIN_OMX_TS) {
                    pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
                }
            }
            else
            {
                NvError err = NvSuccess;
                if (pData->InputSurfaceHook)
                    (pData->InputSurfaceHook)(pOMXBuf, &pNvxBuffer->Payload.Surfaces);
                err = NvxCopyOMXSurfaceToMMBuffer(
                    pOMXBuf,
                    pNvxBuffer,
                    NV_FALSE);
                NV_ASSERT( err == NvSuccess );
                if ( err != NvSuccess )
                    eError = OMX_ErrorUndefined;
            }
            pNvxBuffer->Payload.Surfaces.Empty = NV_FALSE;
        }
        else
        {
            NvU32 size = pOMXBuf->nFilledLen;

            if ((size > pNvxBuffer->Payload.Ref.sizeOfBufferInBytes) && !pPort->bAvoidCopy)
            {
                NvError error;

                if (pPort->pBuffers[nWorkingBuffer])
                {
                    NvMMLiteUtilDeallocateBuffer(pPort->pBuffers[nWorkingBuffer]);
                }

                /* WriteCombined allocs for both Audio and Video input buffers if bInSharedMemory == NV_TRUE */
                if ((pPort->bInSharedMemory == NV_TRUE) && (pData->bVideoTransform || pData->bAudioTransform))
                    pPort->bufReq.memorySpace = NvMMMemoryType_WriteCombined;

                error = NvMMLiteUtilAllocateBuffer(pData->hRmDevice,
                    size + 1024,
                    pPort->bufReq.byteAlignment,
                    pPort->bufReq.memorySpace,
                    pPort->bInSharedMemory,
                    &(pPort->pBuffers[nWorkingBuffer]));
                if (NvSuccess != error)
                {
                    NV_DEBUG_PRINTF(("failed to reallocate %d bytes in RM\n", size+1024));
                    return OMX_ErrorInsufficientResources;
                }
                pPort->pBuffers[nWorkingBuffer]->BufferID = nWorkingBuffer;

                pNvxBuffer = pPort->pBuffers[nWorkingBuffer];
                pNvxBuffer->PayloadInfo.TimeStamp = normTS(pOMXBuf->nTimeStamp);
            }
            pNvxBuffer->Payload.Ref.startOfValidData = 0;
            pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes = size;

            if (pPort->bAvoidCopy)
            {
                pNvxBuffer->Payload.Ref.pMem = pOMXBuf->pBuffer;
            }
            else if (pNvxBuffer->Payload.Ref.pMem)
            {
                NvOsMemcpy(pNvxBuffer->Payload.Ref.pMem, pOMXBuf->pBuffer,
                    size);
                /* CacheMaint if bInSharedMemory == NV_TRUE and MemoryType == WriteCombined */
                if (pPort->bInSharedMemory == NV_TRUE && pNvxBuffer->Payload.Ref.MemoryType == NvMMMemoryType_WriteCombined)
                    NvRmMemCacheMaint(pNvxBuffer->Payload.Ref.hMem, (void *) pNvxBuffer->Payload.Ref.pMem, size, NV_TRUE, NV_FALSE);
            }
            else if (pNvxBuffer->Payload.Ref.hMem)
            {
                NvRmMemWrite(pNvxBuffer->Payload.Ref.hMem,
                    pNvxBuffer->Payload.Ref.Offset,
                    pOMXBuf->pBuffer, size);
            }

        }

        if (pData->oBlockType == NvMMLiteBlockType_DecH264)
        {
            pNvxBuffer->PayloadInfo.BufferMetaDataType = NvMBufferMetadataType_H264;
            if (pData->nNalStreamFlag)
            {
                pNvxBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.BufferFormat = NvH264BufferFormat_RTPStreamFormat;
                pNvxBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.NALUSizeFieldWidthInBytes = pData->nNalSize;
            }
            else
            {
                pNvxBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.BufferFormat = NvH264BufferFormat_ByteStreamFormat;
                pNvxBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.NALUSizeFieldWidthInBytes = 0;
            }
        }

        if (pData->oBlockType == NvMMLiteBlockType_DecVC1 ||
            pData->oBlockType == NvMMLiteBlockType_DecWMA)
        {
            if (pOMXBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
                pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
        }


        if ( (pOMXBuf->nFlags & OMX_BUFFERFLAG_NEED_PTS) && (!pData->bFilterTimestamps) )
        {
            NvError err = NvSuccess;
            NvBool bFilterTimestamps = NV_TRUE;

            pData->bFilterTimestamps = OMX_TRUE;

            pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_PTSCalculationRequired;
            err = pData->hBlock->SetAttribute(pData->hBlock, NvMMLiteVideoDecAttribute_FilterTimestamp,
                                                              0, sizeof(NvBool), &bFilterTimestamps);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF((" SetAttribute() FAILED.\n"));
                return OMX_ErrorBadParameter;
            }
        }

        if (OMX_BUFFERFLAG_SKIP_FRAME & pOMXBuf->nFlags)
            pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_SkipFrame;

        if (OMX_BUFFERFLAG_EXTRADATA & pOMXBuf->nFlags)
        {
            OMX_BUFFERHEADERTYPE* ptmpHeader;
            NvU8* pDataEx;
            OMX_OTHER_EXTRADATATYPE *p_extraDataType;

            DRM_EXTRADATA_ENCRYPTION_METADATA *metadata;
            NvU32 lastEncryptedDataEnd;
            NvU32 i=0, size=0;

            ptmpHeader = (OMX_BUFFERHEADERTYPE* )pOMXBuf;
            pDataEx = ptmpHeader->pBuffer + ptmpHeader->nOffset + ptmpHeader->nFilledLen;
            pDataEx = (OMX_U8*)(((OMX_U32)pData+3)&(~3));
            p_extraDataType = (OMX_OTHER_EXTRADATATYPE *)pDataEx;
            if (p_extraDataType != NULL)
            {
#ifdef BUILD_GOOGLETV
                if (NvMMCryptoIsWidevine(p_extraDataType->eType))
                {
                    if (NvMMLiteBlockType_DecH264 == pData->oBlockType)
                    {
                        NvOsDebugPrintf("Decrypt widevine video");
                        NvMMCryptoProcessAudioBuffer(
                                pOMXBuf->pBuffer,
                                pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes,
                                pNvxBuffer->Payload.Ref.pMem + pNvxBuffer->Payload.Ref.startOfValidData,
                                pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes,
                                p_extraDataType->data);
/*
                        NvMMCryptoProcessVideoBuffer(
                                pOMXBuf->pBuffer,
                                pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes,
                                pNvxBuffer->Payload.Ref.pMem + pNvxBuffer->Payload.Ref.startOfValidData,
                                pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes,
                                p_extraDataType->data);

                        NvMMAesWvMetadata *pNvMMAesWvMetadata;

                        pNvMMAesWvMetadata = (NvMMAesWvMetadata *)((NvU32)pNvxBuffer->Payload.Ref.pMem);

                        pNvxBuffer->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_AesWv;
                        pNvxBuffer->PayloadInfo.BufferMetaData.AesWvMetadata.AlgorithmID =
                            pNvMMAesWvMetadata->AlgorithmID;
                        pNvxBuffer->PayloadInfo.BufferMetaData.AesWvMetadata.NonAlignedOffset = pNvMMAesWvMetadata->NonAlignedOffset;

                        NvOsMemcpy(pNvxBuffer->PayloadInfo.BufferMetaData.AesWvMetadata.Iv, pNvMMAesWvMetadata->Iv, 16);

                        pNvxBuffer->Payload.Ref.startOfValidData =
                            (((NvU32)pNvMMAesWvMetadata + sizeof(NvMMAesWvMetadata) + 3) & (~3)) -
                             (NvU32)pNvMMAesWvMetadata;

                        NvOsDebugPrintf("EXTRADATA is widevine start = %d", pNvxBuffer->Payload.Ref.startOfValidData);
*/
//                        pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes -= pNvxBuffer->Payload.Ref.startOfValidData;
                    }
                    else
                    {
                        NvOsDebugPrintf("Decrypt widevine audio");
                        NvMMCryptoProcessAudioBuffer(
                                pOMXBuf->pBuffer,
                                pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes,
                                pNvxBuffer->Payload.Ref.pMem + pNvxBuffer->Payload.Ref.startOfValidData,
                                pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes,
                                p_extraDataType->data);
                    }
                }
                else
#endif
                if (p_extraDataType->eType != OMX_ExtraDataNone)
                {
                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_FALSE;
                    pNvxBuffer->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_Aes;

                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData = (NvMMAesData *)((NvU32)pNvxBuffer->Payload.Ref.pMem +
                                                                            pNvxBuffer->Payload.Ref.startOfValidData + pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes);
                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData =
                                                            (NvMMAesData*)(((NvU32)pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData + 3) & (~3));

                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.PhysicalAddress_AesData =
                        pNvxBuffer->Payload.Ref.PhyAddress + pNvxBuffer->Payload.Ref.startOfValidData + pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes;
                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.PhysicalAddress_AesData =
                        ((pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.PhysicalAddress_AesData + 3) & (~3));

                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount = 0;

                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AlgorithmID = NvMMMAesAlgorithmID_AES_ALGO_ID_CTR_MODE;
                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->IV_size = 16;

                    lastEncryptedDataEnd = 0;
                    do
                    {
                        p_extraDataType = (OMX_OTHER_EXTRADATATYPE *) pDataEx;
                        if ((p_extraDataType == NULL) || (p_extraDataType->eType == OMX_ExtraDataNone))
                            break;
                        // Should use sizeof(OMX_OTHER_EXTRADATATYPE), but client which fills pDataEx is stale. Need to fix that first
                        pDataEx += sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32) + sizeof(OMX_EXTRADATATYPE) + sizeof(OMX_U32);

                        metadata = (DRM_EXTRADATA_ENCRYPTION_METADATA *) pDataEx;
                        // Should use sizeof(DRM_EXTRADATA_ENCRYPTION_METADATA), but client which fills pDataEx is stale. Need to fix that first
                        pDataEx += sizeof(OMX_U32) + sizeof(OMX_U32) + sizeof(OMX_U64) + sizeof(OMX_U64) + sizeof(OMX_U8);
                        pDataEx = (OMX_U8*)(((OMX_U32)pDataEx+3)&(~3));

                        if ((OMX_EXTRADATATYPE)NVX_IndexExtraDataEncryptionMetadata == p_extraDataType->eType)
                        {
                            /* Copy the AES Metadata to nvmm buffer*/
                            NV_DEBUG_PRINTF(("drm encryptionOffset: %d, encryptionSize: %d", metadata->encryptionOffset, metadata->encryptionSize));
                            pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount++;
                            pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_TRUE;

                            if ((metadata->encryptionOffset == 0) && (i > 0))
                            {
                                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i-1].BytesOfEncryptedData +=
                                    metadata->encryptionSize;
                                lastEncryptedDataEnd = metadata->encryptionOffset + metadata->encryptionSize;
                                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount--;
                                metadata++;

                                NV_DEBUG_PRINTF(("i: %d, BytesOfClearData: %d, BytesOfEncryptedData: %d\n",
                                    i-1, pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i-1].BytesOfClearData,
                                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i-1].BytesOfEncryptedData));
                            }
                            else
                            {
                                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].BytesOfClearData = metadata->encryptionOffset - lastEncryptedDataEnd;
                                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].BytesOfEncryptedData = metadata->encryptionSize;
                                lastEncryptedDataEnd = metadata->encryptionOffset + metadata->encryptionSize;

                                NvOsMemcpy(pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].InitializationVector,
                                    (unsigned char *)&metadata->ivData.qwInitializationVector, 8);     // char array IV

                                // Convert to big endian
                                *(NvU32 *)&pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].InitializationVector[8]  = 0;
                                *(NvU32 *)&pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].InitializationVector[12] =
                                    CONVERT_TO_BIG_ENDIAN(metadata->ivData.qwBlockOffset);
                                metadata++;

                                NV_DEBUG_PRINTF(("i: %d, BytesOfClearData: %d, BytesOfEncryptedData: %d\n",
                                    i, pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].BytesOfClearData,
                                    pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].BytesOfEncryptedData));
#if NV_ENABLE_DEBUG_PRINTS
                                {
                                    NvU8 *pIV = pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AesPacketInfo[i].InitializationVector;
                                    NvOsDebugPrintf("IV: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", pIV[0], pIV[1], pIV[2], pIV[3]);
                                    NvOsDebugPrintf("IV: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", pIV[4], pIV[5], pIV[6], pIV[7]);
                                    NvOsDebugPrintf("IV: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", pIV[8], pIV[9], pIV[10], pIV[11]);
                                    NvOsDebugPrintf("IV: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", pIV[12], pIV[13], pIV[14], pIV[15]);
                                }
#endif
                            }
                        }
                        else
                        {
                            NV_DEBUG_PRINTF(("DRM content :: Unknown extra data type %lx", p_extraDataType->eType));
                        }
                        i++;
                    } while (i < 32);

                    NV_DEBUG_PRINTF(("metadataCount: %d\n", pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount));
                    size = pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount * sizeof(NvMMAesData);
                    /* CacheMaint if bInSharedMemory == NV_TRUE and MemoryType == WriteCombined */
                    if (pPort->bInSharedMemory == NV_TRUE && pNvxBuffer->Payload.Ref.MemoryType == NvMMMemoryType_WriteCombined)
                            NvRmMemCacheMaint(pNvxBuffer->Payload.Ref.hMem, (void *) pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData, size, NV_TRUE, NV_FALSE);
                }
            }
        }

        if (NvMMLiteBlockType_DecH264 == pData->oBlockType && pData->bSecure && bProcessSecureBuf)
        {
            NvU32 i;
            NvU32 BufferSize = 0;
            NvMMAesData *pAesData = NULL;
            OMX_BOOL bWidevine = OMX_FALSE;
            OMX_BOOL bWidevineCtr = OMX_FALSE;
            NvMMAesClientMetadata *pNvMMAesClientMetadata = (NvMMAesClientMetadata *)((NvU32)pNvxBuffer->Payload.Ref.pMem);

            if (!NvOsStrncmp((const char *)"WIDEVINECTR", (const char *)pNvMMAesClientMetadata->BufferMarker, sizeof("WIDEVINECTR")))
            {
                bWidevine = OMX_FALSE;
                bWidevineCtr = OMX_TRUE;
            }
            else if (!NvOsStrncmp((const char *)"WIDEVINE", (const char *)pNvMMAesClientMetadata->BufferMarker, sizeof("WIDEVINE")))
            {
                bWidevine = OMX_TRUE;
                bWidevineCtr = OMX_FALSE;
            }
            else
            {
                NV_DEBUG_PRINTF(("%s:%d Incorrect marker = %s\n", __func__, __LINE__, pNvMMAesClientMetadata->BufferMarker));
            }

            if (bWidevineCtr == OMX_TRUE){
                    // In CTR mode, WV engine does not account for the WV Meta data
                    pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes += ((sizeof(NvMMAesClientMetadata) + 3) & (~3));
                    // not sure if its ok to directly modify pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes
                    // if not, need to put it to a temporary variable
            }

            pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData =
                (NvMMAesData *)((NvU32)pNvxBuffer->Payload.Ref.pMem + pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes);
            pAesData = pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData;
            NvOsMemset(pAesData, 0, sizeof(NvMMAesData));

            pNvxBuffer->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_Aes;
            pAesData->KID = pNvMMAesClientMetadata->KID;
            if (NvMMMAesAlgorithmID_AES_ALGO_ID_CBC_MODE == pNvMMAesClientMetadata->AlgorithmID)
            {
                pAesData->AlgorithmID = NvMMMAesAlgorithmID_AES_ALGO_ID_WIDEVINE_CBC_MODE;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_TRUE;
                pAesData->AesPacketInfo[0].IvValid = NV_TRUE;
            }
            else if (NvMMMAesAlgorithmID_AES_ALGO_ID_CTR_MODE == pNvMMAesClientMetadata->AlgorithmID)
            {
                pAesData->AlgorithmID = NvMMMAesAlgorithmID_AES_ALGO_ID_CTR_MODE;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_TRUE;
            }
            else
            {
                pAesData->AlgorithmID = NvMMMAesAlgorithmID_AES_ALGO_ID_NOT_ENCRYPTED;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_FALSE;
                pAesData->AesPacketInfo[0].IvValid = NV_FALSE;
            }

            if (bWidevine == OMX_TRUE || bWidevineCtr == OMX_TRUE)
            {
                pNvxBuffer->Payload.Ref.startOfValidData =
                    (((NvU32)pNvMMAesClientMetadata + sizeof(NvMMAesClientMetadata) + 3) & (~3)) - (NvU32)pNvMMAesClientMetadata;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->IV_size = 16;
                if (pNvMMAesClientMetadata->MetaDataCount > NUM_PACKETS)
                {
                    NV_DEBUG_PRINTF(("MetaDataCount exceeds available no.of packets - MetaDataCount = %d, NUM_PACKETS = %d\n",
                                     pNvMMAesClientMetadata->MetaDataCount, NUM_PACKETS));
                }
                else if (pNvMMAesClientMetadata->MetaDataCount <= 0)
                {
                    NV_DEBUG_PRINTF(("MetaDataCount = %d is invalid\n", pNvMMAesClientMetadata->MetaDataCount));
                }

                for (i = 0; i < pNvMMAesClientMetadata->MetaDataCount; i++)
                {
                    pAesData->AesPacketInfo[i].BytesOfClearData = pNvMMAesClientMetadata->AesPacket[i].BytesOfClearData;
                    pAesData->AesPacketInfo[i].BytesOfEncryptedData = pNvMMAesClientMetadata->AesPacket[i].BytesOfEncryptedData;
                    pAesData->AesPacketInfo[i].IvValid = pNvMMAesClientMetadata->AesPacket[i].IvValid;

                    if (pAesData->AesPacketInfo[i].IvValid)
                        NvOsMemcpy(&pAesData->AesPacketInfo[i].InitializationVector, &pNvMMAesClientMetadata->Iv[i][0], 16);
                    BufferSize += (pAesData->AesPacketInfo[i].BytesOfClearData + pAesData->AesPacketInfo[i].BytesOfEncryptedData);
                }

                if (bWidevine)
                    pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes -= pNvxBuffer->Payload.Ref.startOfValidData;
                else if (bWidevineCtr)
                    pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes = BufferSize;

                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount = pNvMMAesClientMetadata->MetaDataCount;

                if (BufferSize != pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes)
                {
                    NV_DEBUG_PRINTF(("Incorrect Sample size detected !!! - BufferSize = %d, sizeOfValidDataInBytes = %d\n",
                                    BufferSize, pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes));
                }
            }
        }

        if (pData->bProfile)
        {
            if (pData->nNumTBTBPackets == 0)
                pData->llFirstTBTB = NvOsGetTimeUS();
            pData->nNumTBTBPackets++;
            NVMMLITEVERBOSE(("%s: TBTB: (%p, len %d) at %f\n", pData->sName,
                pOMXBuf, pOMXBuf->nFilledLen,
                NvOsGetTimeUS() / 1000000.0));
        }

        if (pPort->bSetAudioParams)
        {
            NvMMAudioMetadata *audioMeta = &(pNvxBuffer->PayloadInfo.BufferMetaData.AudioMetadata);

            pNvxBuffer->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_Audio;
            audioMeta->nSampleRate = (NvU32)(pPort->nSampleRate);
            audioMeta->nChannels = (NvU32)(pPort->nChannels);
            audioMeta->nBitsPerSample = (NvU32)(pPort->nBitsPerSample);
        }

        pData->TransferBufferToBlock(pData->hBlock, nNumStream,
            NvMMBufferType_Payload,
            sizeof(NvMMBuffer), pNvxBuffer);

        if ((pData->oBlockType == NvMMLiteBlockType_EncH264) ||
            (pData->oBlockType == NvMMLiteBlockType_EncMPEG4) ||
            (pData->oBlockType == NvMMLiteBlockType_EncH263) ||
            (pData->oBlockType == NvMMLiteBlockType_EncVP8))
        {
            Pending_buffers = NvMMQueueGetNumEntries(pPort->pOMXPort->pFullBuffers);
            pData->hBlock->SetAttribute(pData->hBlock, NvMMAttributeVideoEnc_PendingInputBuffers,
                                    0, sizeof(NvU32), &Pending_buffers);
        }

        //Trigger Nvmmlite block worker function
        NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        pPort->bFirstBuffer = OMX_FALSE;

        if (!pPort->bAvoidCopy)
            bSentData = OMX_TRUE;
    }

    if (pOMXBuf->nFlags & OMX_BUFFERFLAG_EOS && !pPort->bSentEOS)
    {
        NvMMLiteEventStreamEndInfo streamEndInfo;

        NVMMLITEVERBOSE(("Sending EOS to block\n"));
        NvDebugOmx(("Sending EOS to block %s", pData->sName));

        NvOsMemset(&streamEndInfo, 0, sizeof(NvMMLiteEventStreamEndInfo));
        streamEndInfo.event = NvMMLiteEvent_StreamEnd;
        streamEndInfo.structSize = sizeof(NvMMLiteEventStreamEndInfo);

        pData->TransferBufferToBlock(pData->hBlock, nNumStream,
            NvMMBufferType_StreamEvent,
            sizeof(NvMMLiteEventStreamEndInfo),
            &streamEndInfo);
        //Trigger Nvmmlite block worker function
        NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        pPort->bSentEOS = OMX_TRUE;

        // Don't return valid length buffer
        if (pOMXBuf->nFilledLen > 0) {
            pPort->pOMXPort->pCurrentBufferHdr = NULL;
            return eError;
         }
    }

    if ((pPort->bAvoidCopy && pPort->bSentEOS) ||
        (!bSentData && !pPort->bAvoidCopy) ||
        (pOMXBuf->nFilledLen <= 0))
    {
        NvxPortReleaseBuffer(pPort->pOMXPort, pOMXBuf);
    }

    pPort->pOMXPort->pCurrentBufferHdr = NULL;

    return eError;
}

OMX_BOOL NvxNvMMLiteTransformIsInputSkippingWorker(SNvxNvMMLiteTransformData *pData,
                                               int nStreamNum)
{
    SNvxNvMMLitePort *pPort;
    pPort = &pData->oPorts[nStreamNum];

    return pPort->bSkipWorker;
}

static OMX_ERRORTYPE NvxNvMMLiteTransformReconfigurePortBuffers(SNvxNvMMLiteTransformData *pData)
{
    NvError status;
    NvU32 NumSurfaces,i,streamIndex;
    SNvxNvMMLitePort *pPort;
    streamIndex = pData->nReconfigurePortNum;
    pPort = &(pData->oPorts[streamIndex]);

    // In case of non-Tunnleled case we donot share our buffers with any other OMX component.
    // Hence we can handle this on our own.
    // In order to reconfigure the new buffers we need to :
    // 1.  Pause the NvMMLite Block.
    // 2.  Call AbortBuffers on the concerned Stream of the NvMMLite Block.
    // 3.  Deallocate all the existing buffers.
    // 4.  Reallocate all the buffers.
    // 5.  TBF new buffers to the Block.
    // 6.  Set the state to running.

    if(!pPort->bUsesANBs)
    {
        pPort->nBufsTot = 0;
        NumSurfaces = pData->VideoStreamInfo.NumOfSurfaces;
        status = NvxNvMMLiteTransformAllocateSurfaces(pData,
            &pData->VideoStreamInfo,
            pData->nReconfigurePortNum);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("Unable to allocate the memory for the Input buffer !"));
            return OMX_ErrorInsufficientResources;
        }

        for (i = 0; i < NumSurfaces; i++)
        {
            pData->TransferBufferToBlock(pData->hBlock,
                    streamIndex,
                    NvMMBufferType_Payload,
                    sizeof(NvMMBuffer),
                    pData->oPorts[pData->nReconfigurePortNum].pBuffers[i]);
            //Trigger Nvmmlite block worker function
            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
            pPort->nBufsTot++;
        }
        if (pPort->pBuffers[0])
        {
            pPort->nWidth = (pPort->pBuffers[0])->Payload.Surfaces.Surfaces[0].Width;
            pPort->nHeight = (pPort->pBuffers[0])->Payload.Surfaces.Surfaces[0].Height;
        }
    }

    pData->bReconfigurePortBuffers = OMX_FALSE;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformWorkerBase(SNvxNvMMLiteTransformData *pData, int nNumStream, OMX_BOOL *pbMoreWork)
{
    SNvxNvMMLitePort *pPort;
    OMX_BOOL bNextWouldFail = OMX_TRUE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pPort = &pData->oPorts[nNumStream];
    pData->hBlock->pDoWork(pData->hBlock,
                           NvMMLiteDoWorkCondition_Critical, (NvBool *)pbMoreWork);

    if (pPort->bCanSkipWorker && !pPort->bSentInitialBuffers)
    {
        int i = 0;

        pPort->bSkipWorker = OMX_TRUE;

        for (i = 0; i < pPort->nBufsTot; i++)
        {
            if (pPort->pBuffers[i]->PayloadType != NvMMPayloadType_SurfaceArray)
            {
                pPort->pBuffers[i]->Payload.Ref.startOfValidData = 0;
                pPort->pBuffers[i]->Payload.Ref.sizeOfValidDataInBytes = 0;
            }
            pData->TransferBufferToBlock(pData->hBlock, nNumStream,
                NvMMBufferType_Payload,
                sizeof(NvMMBuffer),
                pPort->pBuffers[i]);
            //Trigger Nvmmlite block worker function
            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        }

        pPort->bSentInitialBuffers = OMX_TRUE;
        return OMX_ErrorNone;
    }
    {
        NvU32 i = 0;
        // process Output buffers.
        for (i = 0; i < pData->nNumOutputPorts; i++)
        {
            while (NvMMQueueGetNumEntries(pData->pOutputQ[i]))
            {
                NvxLiteOutputBuffer OutBuf;
                NvMMQueuePeek(pData->pOutputQ[i], &OutBuf);
                NV_DEBUG_PRINTF(("%s: %s[%d] before process %d id %d streamIndex %d buf 0x%x \n",
                    pData->sName,
                    __FUNCTION__, __LINE__,
                    NvMMQueueGetNumEntries(pData->pOutputQ[pPort->nOutputQueueIndex]),
                    OutBuf.Buffer.BufferID,
                    OutBuf.streamIndex,
                    (int)&OutBuf.Buffer));

                // check omx buffers.
                eError = NvxNvMMLiteTransformProcessOutputBuffers(pData, OutBuf.streamIndex,
                     &OutBuf.Buffer);
                if (eError != NvError_Busy)
                {
                    NvMMQueueDeQ(pData->pOutputQ[i], &OutBuf);
                }
                else
                {
                    NV_DEBUG_PRINTF(("%s: %s[%d] NvError_Busy %d id %d streamIndex %d buf 0x%x \n",
                        pData->sName,
                        __FUNCTION__, __LINE__,
                        NvMMQueueGetNumEntries(pData->pOutputQ[pPort->nOutputQueueIndex]),
                        OutBuf.Buffer.BufferID,
                        OutBuf.streamIndex,
                        (int)&OutBuf.Buffer));
                    break;
                }
            }
        }
    }

    if (!pPort->bSkipWorker)
    {
        pData->bHasInputRunning = OMX_TRUE;
        eError = NvxNvMMLiteProcessInputBuffer(pData, nNumStream, &bNextWouldFail);
        *pbMoreWork = ((!bNextWouldFail) || *pbMoreWork);
        return eError;
    }

    return OMX_ErrorNone;
}

static void NvxNvMMLiteTransformSetBufferConfig(SNvxNvMMLiteTransformData *pData, int nStream)
{
    NvMMLiteNewBufferRequirementsInfo *req;
    NvU32 nNumBufs = pData->oPorts[nStream].nBufsTot;
    NvU32 nBufSize = pData->oPorts[nStream].nBufSize;

    req = &(pData->oPorts[nStream].bufReq);

    if (nNumBufs < req->minBuffers)
        nNumBufs = req->minBuffers;
    if (nNumBufs > req->maxBuffers)
        nNumBufs = req->maxBuffers;
    if (nBufSize < req->minBufferSize)
        nBufSize = req->minBufferSize;
    if (nBufSize > req->maxBufferSize)
        nBufSize = req->maxBufferSize;

    pData->oPorts[nStream].nBufsTot = nNumBufs;
    pData->oPorts[nStream].nBufSize = nBufSize;
    pData->oPorts[nStream].bInSharedMemory = !!(pData->bInSharedMemory || req->bInSharedMemory);
}

OMX_ERRORTYPE NvxNvMMLiteTransformOpen(SNvxNvMMLiteTransformData *pData, NvMMLiteBlockType oBlockType, const char *sBlockName, int nNumStreams, NvxComponent *pNvComp)
{
    int i = 0;
    NvError status = NvSuccess;
    NvMMLiteCreationParameters oCreate;
    int len;
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    int isVpp = 0;
#endif

    if (nNumStreams > TF_NUM_STREAMS)
        return OMX_ErrorBadParameter;

    pData->nNumStreams = nNumStreams;
    pData->pParentComp = pNvComp;

    if (oBlockType == NvMMLiteBlockType_DecMPEG4 ||
        oBlockType == NvMMLiteBlockType_DecH264 ||
        oBlockType == NvMMLiteBlockType_DecH265 ||
        oBlockType == NvMMLiteBlockType_DecVC1 ||
        oBlockType == NvMMLiteBlockType_DecSuperJPEG ||
        oBlockType == NvMMLiteBlockType_DigitalZoom ||
        oBlockType == NvMMLiteBlockType_DecMPEG2 ||
        oBlockType == NvMMLiteBlockType_DecVP8 ||
        oBlockType == NvMMLiteBlockType_DecMJPEG)
    {
        pData->bVideoTransform = OMX_TRUE;
    }

    pData->bAudioTransform = OMX_FALSE;
    if (oBlockType > NvMMLiteBlockType_DecAudioStart &&
        oBlockType < NvMMLiteBlockType_DecAudioEnd)
    {
        pData->bAudioTransform = OMX_TRUE;
    }

    if (NvSuccess != NvRmOpen(&(pData->hRmDevice), 0))
        return OMX_ErrorBadParameter;

    pData->nNalStreamFlag = 0;
    pData->nNumInputPorts = pData->nNumOutputPorts = 0;
    pData->oBlockType = oBlockType;
    pData->bSequence = OMX_FALSE;
    pData->bUseLowOutBuff = OMX_FALSE;
    pData->bFilterTimestamps = OMX_FALSE;

    for (i = 0; i < TF_NUM_STREAMS; i++)
    {
        OMX_BOOL bEmbedNvMMBuffer = pData->oPorts[i].bEmbedNvMMBuffer;
        OMX_BOOL bUsesANBs = pData->oPorts[i].bUsesANBs;
        OMX_BOOL bWillCopyToANB = pData->oPorts[i].bWillCopyToANB;
        OMX_BOOL bUsesAndroidMetaDataBuffers =
                    pData->oPorts[i].bUsesAndroidMetaDataBuffers;
        OMX_BOOL bEnc2DCopy = pData->oPorts[i].bEnc2DCopy;

        NvOsMemset(&(pData->oPorts[i]), 0, sizeof(SNvxNvMMLitePort));
        pData->oPorts[i].nWidth = 176;
        pData->oPorts[i].nHeight = 144;
        NvOsMemset(&pData->oPorts[i].outRect, 0, sizeof(OMX_CONFIG_RECTTYPE));
        pData->oPorts[i].outRect.nSize = sizeof(OMX_CONFIG_RECTTYPE);
        pData->oPorts[i].bHasSize = OMX_FALSE;
        pData->oPorts[i].bEOS = OMX_FALSE;
        pData->oPorts[i].bEmbedNvMMBuffer = bEmbedNvMMBuffer;
        pData->oPorts[i].bEmbedANBSurface = OMX_FALSE;
        pData->oPorts[i].bUsesAndroidMetaDataBuffers =
            bUsesAndroidMetaDataBuffers;
        pData->oPorts[i].DispWidth = 0;
        pData->oPorts[i].DispHeight = 0;
        pData->oPorts[i].xScaleWidth = NV_SFX_ONE;
        pData->oPorts[i].xScaleHeight = NV_SFX_ONE;
        pData->oPorts[i].bUsesANBs = bUsesANBs;
        pData->oPorts[i].bWillCopyToANB = bWillCopyToANB;
        pData->oPorts[i].bEnc2DCopy = bEnc2DCopy;
        pData->oPorts[i].bAvoidCopy = OMX_FALSE;
        pData->oPorts[i].pMarkQueue = NULL;
#ifdef BUILD_GOOGLETV
        pData->oPorts[i].hPreviousBuffersMutex = NULL;
#endif
        pData->oPorts[i].oMutex = NULL;
    }

    pData->blockCloseDone = OMX_FALSE;
    pData->blockErrorEncountered = OMX_FALSE;

    for (i = 0; i < nNumStreams; i++)
    {
        pData->oPorts[i].bFirstBuffer = OMX_TRUE;
        pData->oPorts[i].bFirstOutBuffer = OMX_TRUE;
    }

    len = NvOsStrlen(sBlockName);
    if (len > 15)
        len = 15;

    NvOsMemset(pData->sName, 0, 32);
    NvOsSnprintf(pData->sName, 31, "%d%s", s_nBlockCount, sBlockName);
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    NV_DEBUG_PRINTF(("Before Opening VPP"));
    /*VPP Datastructure intilization*/
    if((oBlockType > NvMMLiteBlockType_DecVideoStart ) &&
        (oBlockType < NvMMLiteBlockType_DecVideoEnd))
    {
        isVpp = pData->sVpp.bVppEnable;
        if(isVpp && !pData->bThumbnailMode)
        {
            NV_DEBUG_PRINTF(("VPP is enabled!"));
            if (pData->sVpp.nVppType == NV_VPP_TYPE_EGL)
            {
               //Fall back to CPU based VPP is the shader files are not present
               NvOsStatType stat;
               if (NvOsStat("/system/etc/firmware/InvertY.fs", &stat) != NvSuccess)
               {
                    NvOsDebugPrintf("Shader file not present, forcing CPU based VPP");
                    pData->sVpp.nVppType = NV_VPP_TYPE_CPU;
               }
            }
            pData->sVpp.hVppThread = NULL;
            pData->sVpp.bVppThreadtopuase = OMX_FALSE;
            pData->sVpp.VppInpOutAvail = NULL;
            pData->sVpp.VppInitDone = NULL;
            pData->sVpp.pReleaseOutSurfaceQueue = NULL;
            pData->sVpp.pOutSurfaceQueue = NULL;
            pData->sVpp.VppThreadPaused = NULL;
        }
    }
    else
        pData->sVpp.bVppEnable = OMX_FALSE;
#endif
    NvOsMemset(&oCreate, 0, sizeof(oCreate));
    oCreate.Type = oBlockType;
    oCreate.BlockSpecific = (NvU64)(pData->BlockSpecific);
    oCreate.structSize = sizeof(NvMMLiteCreationParameters);

    status = NvMMLiteOpenBlock(&pData->hBlock, &oCreate);

    if (status != NvSuccess)
    {
        // TO DO: check for other error/success codes?
        return OMX_ErrorInsufficientResources;
    }
    s_nBlockCount = (s_nBlockCount + 1) % 1000;

    pData->bInSharedMemory = (oBlockType == NvMMLiteBlockType_DigitalZoom);
    /* Get the TBF of the block */
    pData->TransferBufferToBlock = pData->hBlock->TransferBufferToBlock;
    pData->is_MVC_present_flag = NV_FALSE ;
    /* Set event and transfer functions */
    pData->hBlock->SetSendBlockEventFunction(pData->hBlock,
        pData,
        NvxNvMMLiteTransformSendBlockEventFunction);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetStreamUsesSurfaces(SNvxNvMMLiteTransformData *pData, int nStreamNum, OMX_BOOL bUseSurface)
{
    NV_ASSERT(nStreamNum < TF_NUM_STREAMS);

    pData->oPorts[nStreamNum].bUsesRmSurfaces = bUseSurface;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetupAudioParams(SNvxNvMMLiteTransformData *pData, int nStreamNum, OMX_U32 sampleRate, OMX_U32 bitsPerSample, OMX_U32 nChannels)
{
    NV_ASSERT(nStreamNum < TF_NUM_STREAMS);

    pData->oPorts[nStreamNum].bSetAudioParams = OMX_TRUE;
    pData->oPorts[nStreamNum].nSampleRate = sampleRate;
    pData->oPorts[nStreamNum].nChannels = nChannels;
    pData->oPorts[nStreamNum].nBitsPerSample = bitsPerSample;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetEventCallback(SNvxNvMMLiteTransformData *pData, void (*EventCB)(NvxComponent *pComp, NvU32 eventType, NvU32 eventSize, void *eventInfo), void *pEventData)
{
    pData->EventCallback = EventCB;
    pData->pEventData = pEventData;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetInputSurfaceHook(SNvxNvMMLiteTransformData *pData, LiteInputSurfaceHookFunction ISHook)
{
    pData->InputSurfaceHook = ISHook;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetupCameraInput(SNvxNvMMLiteTransformData *pData,
                                         int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs,
                                         int nInputBufSize, OMX_BOOL bCanSkipWorker)
{
    int i = 0;
    pData->hBlock->SetTransferBufferFunction(pData->hBlock, nStreamNum,
        NvxNvMMLiteTransformReturnEmptyInput,
        pData, nStreamNum);

    pData->nNumInputPorts++;

    pData->oPorts[nStreamNum].pOMXPort = pOMXPort;
    pData->oPorts[nStreamNum].nType = TF_TYPE_INPUT;
    pData->oPorts[nStreamNum].nBufsTot = nNumInputBufs;
    pData->oPorts[nStreamNum].nBufferWritePos = 0;
    pData->oPorts[nStreamNum].nBufsInUse = 0;
    pData->oPorts[nStreamNum].bSentEOS = OMX_FALSE;
    pData->oPorts[nStreamNum].bAborting = OMX_FALSE;
    pData->oPorts[nStreamNum].bError = OMX_FALSE;
    pData->oPorts[nStreamNum].bCanSkipWorker = bCanSkipWorker;
    pData->oPorts[nStreamNum].bSkipWorker = OMX_FALSE;
    pData->oPorts[nStreamNum].bInSharedMemory = pData->bInSharedMemory;
    pData->oPorts[nStreamNum].nBufSize = nInputBufSize;

    if(!pData->oPorts[nStreamNum].pMarkQueue)
    {
        NvMMQueueCreate(&(pData->oPorts[nStreamNum].pMarkQueue), MAX_NVMMLITE_BUFFERS,
            sizeof(BufferMarkData), NV_FALSE);
        NvxMutexCreate(&(pData->oPorts[nStreamNum].oMutex));
    }

    // Camera host input buffers should be allocated by nvmm-camera
//    pData->oPorts[nStreamNum].bNvMMClientAlloc = OMX_FALSE;
    // FIXME: This will not work.

    // Note that the working contents of these NvMMBuffers will be copied from the buffers
    // allocated in nvmm-camera.  (See NvxNvMMTransformReturnEmptyInput().)  We must have our
    // own copies because we can't cache the (thunked?) pointers coming from the nvmm layer.
    for (i = 0; i < pData->oPorts[nStreamNum].nBufsTot; i++)
    {
        pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
        if (!pData->oPorts[nStreamNum].pBuffers[i])
        {
            NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !"));
            return OMX_ErrorInsufficientResources;
        }
        NvOsMemset(pData->oPorts[nStreamNum].pBuffers[i], 0, sizeof(NvMMBuffer));
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetupInput(SNvxNvMMLiteTransformData *pData,
                                         int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs,
                                         int nInputBufSize, OMX_BOOL bCanSkipWorker)
{
    int i = 0;
    NvError err = NvSuccess;
    NvBool UseAlignment = NV_TRUE;

    pData->hBlock->SetTransferBufferFunction(pData->hBlock,
                                               nStreamNum,
                                               NvxNvMMLiteTransformReturnEmptyInput,
                                               pData,
                                               nStreamNum);

    pData->nNumInputPorts++;

    pData->oPorts[nStreamNum].pOMXPort = pOMXPort;
    pData->oPorts[nStreamNum].nType = TF_TYPE_INPUT;
    pData->oPorts[nStreamNum].nBufsTot = nNumInputBufs;
    pData->oPorts[nStreamNum].nBufferWritePos = 0;
    pData->oPorts[nStreamNum].nBufsInUse = 0;
    pData->oPorts[nStreamNum].bSentEOS = OMX_FALSE;
    pData->oPorts[nStreamNum].bAborting = OMX_FALSE;
    pData->oPorts[nStreamNum].bError = OMX_FALSE;
    pData->oPorts[nStreamNum].bCanSkipWorker = bCanSkipWorker;
    pData->oPorts[nStreamNum].bSkipWorker = OMX_FALSE;
    pData->oPorts[nStreamNum].bInSharedMemory = pData->bInSharedMemory;
    pData->oPorts[nStreamNum].nBufSize = nInputBufSize;
    pData->oPorts[nStreamNum].nInputQueueIndex = nStreamNum;

    if(!pData->oPorts[nStreamNum].pMarkQueue)
    {
        NvMMQueueCreate(&(pData->oPorts[nStreamNum].pMarkQueue), MAX_NVMMLITE_BUFFERS, \
                sizeof(BufferMarkData), NV_FALSE);
        NvxMutexCreate(&(pData->oPorts[nStreamNum].oMutex));

        NvxMutexCreate(&(pData->oPorts[nStreamNum].hVPPMutex));

        NvMMQueueCreate(&(pData->pInputQ[pData->nNumInputPorts - 1]), MAX_NVMMLITE_BUFFERS,
                sizeof(NvMMBuffer *), NV_TRUE);
    }

    NvxNvMMLiteTransformSetBufferConfig(pData, nStreamNum);
    // Since we're not NvMMLite tunneled, default to creating our own buffers.
    if (pData->oBlockType == NvMMLiteBlockType_DecSuperJPEG)
    {
        pData->oPorts[nStreamNum].bSentInitialBuffers = OMX_TRUE;
        pData->oPorts[nStreamNum].bufReq.byteAlignment = 1024;
    }

    // For surface input, allocate surfaces as the input buffers
    if (pData->oPorts[nStreamNum].bHasSize &&
        pData->oPorts[nStreamNum].bRequestSurfaceBuffers)
    {
        NvU32 ImageSize = 0;
        OMX_BOOL bUseRmSurfaces = (pOMXPort->bNvidiaTunneling) &&
            (pOMXPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);

        err = NvSuccess;

        if (bUseRmSurfaces)
        {
            NvxNvMMLiteTransformSetStreamUsesSurfaces(pData, nStreamNum, bUseRmSurfaces);
        }

        for (i = 0; i < pData->oPorts[nStreamNum].nBufsTot; ++i)
        {
            if (pData->oPorts[nStreamNum].pBuffers[i] != NULL)
            {
                err = NvMMQueueEnQ(pData->pInputQ[pData->oPorts[nStreamNum].nInputQueueIndex], &(pData->oPorts[nStreamNum].pBuffers[i]), 10);
                if (NvSuccess != err)
                {
                    NvOsDebugPrintf("ERROR: buffer enqueue failed line %d 0x%x \n", __LINE__, err);
                    return err;
                }
                continue;
            }

            pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
            if (!pData->oPorts[nStreamNum].pBuffers[i])
            {
                NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !"));
                return OMX_ErrorInsufficientResources;
            }

            // If we're not Nvidia tunneled and using NvMMLite surfaces, we need to allocate our own.
            // Otherwise, we don't need to waste memory allocating more surfaces
            if (bUseRmSurfaces)
            {
                NvMMBuffer * pNvxBuffer = (NvMMBuffer *)pData->oPorts[nStreamNum].pBuffers[i];
                NvOsMemset(pNvxBuffer, 0, sizeof(NvMMBuffer));
                pNvxBuffer->BufferID = i;
                pNvxBuffer->PayloadInfo.TimeStamp = 0;
            }
            else
            {
                if((pData->oPorts[nStreamNum].bEmbedNvMMBuffer != OMX_TRUE) ||
                    (pData->oPorts[nStreamNum].bEnc2DCopy))
                {
                    if ((pData->oBlockType == NvMMLiteBlockType_EncH264) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncJPEG) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncMPEG4) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncVP8) ||
                        (pData->oBlockType == NvMMLiteBlockType_EncH263))
                    {
                        UseAlignment = NV_FALSE;
                        err = NvxVideoSurfaceAlloc(pData->hRmDevice,
                            pData->oPorts[nStreamNum].pBuffers[i], i,
                            pData->oPorts[nStreamNum].nWidth,
                            pData->oPorts[nStreamNum].nHeight,
                            pOMXPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar ?
                                NvMMVideoDecColorFormat_YUV420SemiPlanar : NvMMVideoDecColorFormat_YUV420,
                            NvRmSurfaceGetDefaultLayout(), // This is done intentionally for better performance
                            &ImageSize,
                            UseAlignment, NvOsMemAttribute_WriteCombined, NV_FALSE);
                        if (err != NvSuccess)
                        {
                            NV_DEBUG_PRINTF(("Unable to allocate the memory"
                                    " for the Input buffer !"));
                            return OMX_ErrorInsufficientResources;
                        }
                    }
                    else
                    {
                        err = NvxVideoSurfaceAlloc(pData->hRmDevice,
                            pData->oPorts[nStreamNum].pBuffers[i], i,
                            pData->oPorts[nStreamNum].nWidth, pData->oPorts[nStreamNum].nHeight,
                            NvMMVideoDecColorFormat_YUV420,
                            NvRmSurfaceGetDefaultLayout(),
                            &ImageSize,
                            UseAlignment, NvOsMemAttribute_Uncached, NV_FALSE);
                        if (err != NvSuccess)
                        {
                            NV_DEBUG_PRINTF(("Unable to allocate the memory "
                                    "for the Input buffer !"));
                            return OMX_ErrorInsufficientResources;
                        }
                    }
                }
                else
                {
                    pData->oPorts[nStreamNum].pBuffers[i]->StructSize = sizeof(NvMMBuffer);
                    pData->oPorts[nStreamNum].pBuffers[i]->BufferID = i;
                    pData->oPorts[nStreamNum].pBuffers[i]->PayloadType = NvMMPayloadType_SurfaceArray;

                    NvOsMemset(&pData->oPorts[nStreamNum].pBuffers[i]->PayloadInfo, 0, sizeof(pData->oPorts[nStreamNum].pBuffers[i]->PayloadInfo));

                }
            }
            err = NvMMQueueEnQ(pData->pInputQ[pData->oPorts[nStreamNum].nInputQueueIndex], &(pData->oPorts[nStreamNum].pBuffers[i]), 10);
            if (NvSuccess != err)
            {
                NvOsDebugPrintf("ERROR: buffer enqueue failed line %d 0x%x \n", __LINE__, err);
                return err;
            }
        }
    }
    else
    {
        if (pData->oPorts[nStreamNum].bInSharedMemory == NV_FALSE &&
            pData->oPorts[nStreamNum].bufReq.memorySpace == NvMMMemoryType_SYSTEM)
        {
            pData->oPorts[nStreamNum].bAvoidCopy = OMX_TRUE;
        }

        /* WriteCombined allocs for both Audio and Video input buffers if bInSharedMemory == NV_TRUE */
        if ((pData->oPorts[nStreamNum].bInSharedMemory == NV_TRUE) && (pData->bVideoTransform || pData->bAudioTransform))
            pData->oPorts[nStreamNum].bufReq.memorySpace = NvMMMemoryType_WriteCombined;

        NVMMLITEVERBOSE(("creating %d input buffers of size: %d, align %d, shared %d\n",
            nNumInputBufs,
            pData->oPorts[nStreamNum].nBufSize,
            pData->oPorts[nStreamNum].bufReq.byteAlignment,
            pData->oPorts[nStreamNum].bInSharedMemory));

        // Allocate normal buffers
        for (i = 0; i < pData->oPorts[nStreamNum].nBufsTot; i++)
        {
            pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
            if (pData->oPorts[nStreamNum].bAvoidCopy == OMX_FALSE)
            {
                err = NvMMLiteUtilAllocateBuffer(pData->hRmDevice,
                    pData->oPorts[nStreamNum].nBufSize,
                    pData->oPorts[nStreamNum].bufReq.byteAlignment,
                    pData->oPorts[nStreamNum].bufReq.memorySpace,
                    pData->oPorts[nStreamNum].bInSharedMemory,
                    &(pData->oPorts[nStreamNum].pBuffers[i]));

                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("Unable to allocate the memory for the Input buffer!"));
                    return OMX_ErrorInsufficientResources;
                }
            }
            else
            {
                 NvOsMemset(pData->oPorts[nStreamNum].pBuffers[i], 0, sizeof(NvMMBuffer));
                 pData->oPorts[nStreamNum].pBuffers[i]->StructSize = sizeof(NvMMBuffer);
                 pData->oPorts[nStreamNum].pBuffers[i]->Payload.Ref.MemoryType = pData->oPorts[nStreamNum].bufReq.memorySpace;
                 pData->oPorts[nStreamNum].pBuffers[i]->Payload.Ref.sizeOfBufferInBytes = pData->oPorts[nStreamNum].nBufSize;
                 pData->oPorts[nStreamNum].pBuffers[i]->PayloadType = NvMMPayloadType_MemPointer;
            }
            pData->oPorts[nStreamNum].pBuffers[i]->BufferID = i;
            err = NvMMQueueEnQ(pData->pInputQ[pData->oPorts[nStreamNum].nInputQueueIndex], &(pData->oPorts[nStreamNum].pBuffers[i]), 10);
            if (NvSuccess != err)
            {
                NvOsDebugPrintf("ERROR: buffer enqueue failed line %d 0x%x \n", __LINE__, err);
                return err;
            }
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformRegisterDeliverBuffer(SNvxNvMMLiteTransformData *pData, int nStreamNum)
{
    pData->hBlock->SetTransferBufferFunction(pData->hBlock,
        nStreamNum,
        NvxNvMMLiteTransformDeliverFullOutput,
        pData,
        nStreamNum);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformSetupOutput(SNvxNvMMLiteTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nInputStreamNum, int nNumOutputBufs, int nOutputBufSize)
{
    int i = 0;

    pData->hBlock->SetTransferBufferFunction(pData->hBlock, nStreamNum,
            NvxNvMMLiteTransformDeliverFullOutput,
            pData, nStreamNum);

    pData->nNumOutputPorts++;

    pData->oPorts[nStreamNum].pOMXPort = pOMXPort;
    pData->oPorts[nStreamNum].nInputPortNum = nInputStreamNum;
    pData->oPorts[nStreamNum].nType = TF_TYPE_OUTPUT;
    pData->oPorts[nStreamNum].nBufsTot = nNumOutputBufs;
    pData->oPorts[nStreamNum].nBufferWritePos = 0;
    pData->oPorts[nStreamNum].nBufsInUse = 0;
    pData->oPorts[nStreamNum].bSentEOS = OMX_FALSE;
    pData->oPorts[nStreamNum].bAborting = OMX_FALSE;
    pData->oPorts[nStreamNum].bError = OMX_FALSE;
    pData->oPorts[nStreamNum].bInSharedMemory = pData->bInSharedMemory;
    pData->oPorts[nStreamNum].nBufSize = nOutputBufSize;
#ifdef BUILD_GOOGLETV
    if (NvSuccess != NvxListCreate(&(pData->oPorts[nStreamNum].pPreviousBuffersList)))
    {
        return OMX_ErrorInsufficientResources;
    }
    if(!pData->oPorts[nStreamNum].hPreviousBuffersMutex)
        NvxMutexCreate(&(pData->oPorts[nStreamNum].hPreviousBuffersMutex));
    pData->oPorts[nStreamNum].nNumPreviousBuffs = 0;
    pData->oPorts[nStreamNum].nNumPortSetting = 0;
    pData->oPorts[nStreamNum].bPortSettingChanged = OMX_FALSE;
#endif

    if(!pData->oPorts[nStreamNum].oMutex)
    {
        NvxMutexCreate(&(pData->oPorts[nStreamNum].oMutex));

        NvMMQueueCreate(&(pData->pOutputQ[pData->nNumOutputPorts - 1]), MAX_NVMMLITE_BUFFERS,
            sizeof(NvxLiteOutputBuffer), NV_TRUE);
    }
    pData->oPorts[nStreamNum].nOutputQueueIndex = pData->nNumOutputPorts - 1;

    if (pData->bVideoTransform)
    {
        // not sure for cases other than SrcCamera and DigitalZoom
        if (pData->oBlockType == NvMMLiteBlockType_SrcCamera ||
            pData->oBlockType == NvMMLiteBlockType_SrcUSBCamera ||
            pData->oBlockType == NvMMLiteBlockType_DigitalZoom)
        {
            for (i = 0; i < nNumOutputBufs; i++)
            {
                pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
                NvOsMemset(pData->oPorts[nStreamNum].pBuffers[i], 0, sizeof(NvMMBuffer));
                pData->oPorts[nStreamNum].pBuffers[i]->BufferID = i;
            }
        }

        return OMX_ErrorNone;
    }

    NvxNvMMLiteTransformSetBufferConfig(pData, nStreamNum);

    nNumOutputBufs = pData->oPorts[nStreamNum].nBufsTot;

    NVMMLITEVERBOSE(("creating %d output buffers of size: %d, align %d, shared %d\n",
        pData->oPorts[nStreamNum].nBufsTot,
        pData->oPorts[nStreamNum].nBufSize,
        pData->oPorts[nStreamNum].bufReq.byteAlignment,
        pData->oPorts[nStreamNum].bInSharedMemory));

    /* WriteCombined allocs for Audio and Jpeg encoder output buffers if bInSharedMemory == NV_TRUE */
    if ((pData->oPorts[nStreamNum].bInSharedMemory == NV_TRUE) && (pData->bAudioTransform || (pData->oBlockType == NvMMBlockType_EncJPEG)))
    {
        pData->oPorts[nStreamNum].bufReq.memorySpace = NvMMMemoryType_WriteCombined;
    }

    if ((NvMMLiteBlockType_DecH264 == pData->oBlockType) && (pData->bSecure))
        pData->bVideoProtect = OMX_TRUE;

    for (i = 0; i < nNumOutputBufs; i++)
    {
        NvError err = NvSuccess;
        pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
        err = NvMMLiteUtilAllocateProtectedBuffer(pData->hRmDevice,
            pData->oPorts[nStreamNum].nBufSize,
            pData->oPorts[nStreamNum].bufReq.byteAlignment,
            pData->oPorts[nStreamNum].bufReq.memorySpace,
            pData->oPorts[nStreamNum].bInSharedMemory,
            pData->bVideoProtect,
            &(pData->oPorts[nStreamNum].pBuffers[i]));

        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("Unable to allocate the memory for the output buffer!"));
            return OMX_ErrorInsufficientResources;
        }

        pData->oPorts[nStreamNum].pBuffers[i]->BufferID = i;
        pData->TransferBufferToBlock(pData->hBlock, nStreamNum,
            NvMMBufferType_Payload,
            sizeof(NvMMBuffer),
            pData->oPorts[nStreamNum].pBuffers[i]);
        //Trigger Nvmmlite block worker function
        NvxWorkerTrigger(&( pData->oPorts[nStreamNum].pOMXPort->pNvComp->oWorkerData));
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformFlush(SNvxNvMMLiteTransformData *pData, OMX_U32 nPort)
{
    OMX_U32 nFirstPort = 0, nLastPort = pData->nNumStreams - 1;
    OMX_U32 i = 0;
    NvMMLiteBlockHandle hNvMMLiteBlock = pData->hBlock;
    NvxComponent *pComp = pData->pParentComp;

    if (nPort == OMX_ALL)
    {
        /* flush all */
    }
    else if (nPort < pData->nNumStreams)
    {
        nFirstPort = nLastPort = nPort;
    }
    else
        return OMX_ErrorNone;

    pData->bFlushing = OMX_TRUE;
    NVMMLITEVERBOSE(("%s: %s[%d] flush/seek started \n", pData->sName, __FUNCTION__, __LINE__));

    for (i = nFirstPort; i <= nLastPort; i++)
    {
        SNvxNvMMLitePort *pPort = &(pData->oPorts[i]);

        pPort->bEOS = OMX_FALSE;
        pPort->bSentEOS = OMX_FALSE;

        if (pPort->bCanSkipWorker == OMX_TRUE)
        {
            if (pData->oBlockType != NvMMLiteBlockType_DecSuperJPEG)
                pPort->bSentInitialBuffers = OMX_FALSE;
            pPort->bSkipWorker = OMX_FALSE;
        }

        pPort->position = (OMX_TICKS)-1;

        if (pPort->nType == TF_TYPE_INPUT)
        {
            OMX_U32 j;

            NvxMutexUnlock(pComp->hWorkerMutex);
            hNvMMLiteBlock->AbortBuffers(hNvMMLiteBlock, i);

            NvxMutexLock(pComp->hWorkerMutex);

            for (j = 0; j < MAX_NVMMLITE_BUFFERS; j++)
            {
                if (pPort->pOMXBufMap[j])
                {
                    NvMMQueueEnQ(pData->pInputQ[pPort->nInputQueueIndex], &(pPort->pBuffers[j]), 10);
                    NvxPortReleaseBuffer(pPort->pOMXPort,
                        pPort->pOMXBufMap[j]);
                    pPort->pOMXBufMap[j] = NULL;
                    pPort->nBufsInUse--;
                }
            }

            if (pPort->pSavedEOSBuf)
            {
                NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pSavedEOSBuf);
                pPort->pSavedEOSBuf = NULL;
            }

        }
        else if (pPort->nType == TF_TYPE_OUTPUT)
        {
            NvxMutexUnlock(pComp->hWorkerMutex);
            hNvMMLiteBlock->AbortBuffers(hNvMMLiteBlock, i);
            NvxMutexLock(pComp->hWorkerMutex);
            if (pPort->bUsesANBs)
            {
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
                 if(pData->sVpp.bVppEnable == OMX_TRUE)
                     NvxVPPFlush(pData);
#endif

                 FlushOutputQueue(pData, pPort);
            }
            else
            {
                while (NvMMQueueGetNumEntries(pData->pOutputQ[pPort->nOutputQueueIndex]))
                {
                    // drop all buffers from the queue.
                    NvxLiteOutputBuffer OutBuf;
                    NvMMBuffer *PayloadBuffer = NULL;
                    NvU32 id = EMPTY_BUFFER_ID;

                    NvMMQueueDeQ(pData->pOutputQ[pPort->nOutputQueueIndex], &OutBuf);
                    NV_DEBUG_PRINTF(("%s: %s[%d] sending back id %d time %.3f \n",
                        pData->sName, __FUNCTION__, __LINE__,
                        OutBuf.Buffer.BufferID,
                        OutBuf.Buffer.PayloadInfo.TimeStamp / 10000000.0));

                    PayloadBuffer = &OutBuf.Buffer;
                    id = (int) PayloadBuffer->BufferID;
                    if (id == EMPTY_BUFFER_ID)
                        continue;
                    if (pPort->pOMXBufMap[id])
                    {
                        pPort->pOMXBufMap[id] = NULL;
                    }
                    pPort->bBufNeedsFlush[id] = OMX_FALSE;
                    if (PayloadBuffer->PayloadType == NvMMPayloadType_SurfaceArray)
                    {
                        PayloadBuffer->Payload.Surfaces.Empty = NV_TRUE;
                    }
                    else
                    {
                        PayloadBuffer->Payload.Ref.startOfValidData = 0;
                        PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes  = 0;
                    }
                    pData->TransferBufferToBlock(pData->hBlock, i,
                        NvMMBufferType_Payload,
                        sizeof(NvMMBuffer),
                        PayloadBuffer);
                    //Trigger Nvmmlite block worker function
                    NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                }

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
                if(!pData->bThumbnailMode && pData->sVpp.bVppEnable == OMX_TRUE)
                {
                    NvxVPPFlush(pData);
                }
#endif
            }
        }
    }

    for (i = nFirstPort; i <= nLastPort; i++)
    {
        SNvxNvMMLitePort *pPort = &(pData->oPorts[i]);

        if (pPort->pMarkQueue)
        {
            BufferMarkData data;
            while (NvSuccess == NvMMQueueDeQ(pPort->pMarkQueue, &data))
            {
            }
        }
        pPort->bEOS = OMX_FALSE;
        pPort->bSentEOS = OMX_FALSE;
        pPort->position = (OMX_TICKS)-1;
    }
    NVMMLITEVERBOSE(("%s: %s[%d] flush/seek ended \n", pData->sName, __FUNCTION__, __LINE__));
    pData->bFlushing = OMX_FALSE;
    pData->blockErrorEncountered = OMX_FALSE;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformFreePortBuffers(SNvxNvMMLiteTransformData *pData, int nStreamNum)
{
    int i;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[nStreamNum]);

    if (pPort->nType == TF_TYPE_NONE)
        return OMX_ErrorNone;

    for (i = 0; i < pPort->nBufsTot; i++)
    {
        if (pPort->pBuffers[i])
        {
            if (pPort->pBuffers[i]->PayloadType == NvMMPayloadType_SurfaceArray)
            {
                if (!pPort->bEmbedNvMMBuffer && !pPort->bUsesANBs && !pPort->bEmbedANBSurface)
                {
                    NvxVideoDecSurfaceFree(pPort->pBuffers[i]);
                }
            }
            else if (pData->oPorts[nStreamNum].bAvoidCopy == OMX_FALSE)
            {
                NvMMLiteUtilDeallocateBuffer(pPort->pBuffers[i]);
            }

            if (!pPort->bUsesANBs)
            {
                NvOsFree(pPort->pBuffers[i]);
            }
            pPort->pBuffers[i] = NULL;
        }
    }

    pPort->nBackgroundDoneMap = 0;
    pPort->nBufsTot = 0;

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    if ((!pPort->bUsesANBs) && (pData->sVpp.bVppEnable == NV_TRUE) &&
        (!pData->bThumbnailMode) && (pPort->nType == TF_TYPE_OUTPUT))
    {
        for (i = 0; i < TOTAL_NUM_SURFACES; i++)
        {
            if (pData->sVpp.pVideoSurf[i])
            {
                NvxVideoDecSurfaceFree(pData->sVpp.pVideoSurf[i]);
                NvOsFree(pData->sVpp.pVideoSurf[i]);
                pData->sVpp.pVideoSurf[i]= NULL;
            }
        }
    }
#endif

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxNvMMLiteTransformDestroyPort(SNvxNvMMLiteTransformData *pData, int nStreamNum)
{
    SNvxNvMMLitePort *pPort = &(pData->oPorts[nStreamNum]);


    if (pPort->nType == TF_TYPE_NONE)
        return OMX_ErrorNone;

    if (pPort->pMarkQueue)
        NvMMQueueDestroy(&pPort->pMarkQueue);
    pPort->pMarkQueue = NULL;

    if (pPort->nType == TF_TYPE_OUTPUT)
        NvMMQueueDestroy(&pData->pOutputQ[pPort->nOutputQueueIndex]);

    if (pPort->nType == TF_TYPE_INPUT && pData->pInputQ)
    {
        NvMMQueueDestroy(&pData->pInputQ[pPort->nInputQueueIndex]);
        pData->pInputQ[pPort->nInputQueueIndex] = NULL;
        pPort->nInputQueueIndex = 0;
    }

#ifdef BUILD_GOOGLETV
    if (pPort->nType == TF_TYPE_OUTPUT)
    {
        if (pPort->pPreviousBuffersList)
        {
            while (NvxListGetNumEntries(pPort->pPreviousBuffersList))
            {
                NvMMBuffer *pNvMMBuffer = NULL;
                NvxListDeQ(pPort->pPreviousBuffersList, &pNvMMBuffer);
                NvxVideoDecSurfaceFree(pNvMMBuffer);
                NvOsFree(pNvMMBuffer);
            }
            NvxListDestroy(&pPort->pPreviousBuffersList);
            pPort->pPreviousBuffersList = NULL;
        }
        if (pPort->hPreviousBuffersMutex)
        {
            NvxMutexDestroy(pPort->hPreviousBuffersMutex);
            pPort->hPreviousBuffersMutex = NULL;
        }
    }
#endif

    if (pPort->oMutex)
        NvxMutexDestroy(pPort->oMutex);
    pPort->oMutex = NULL;

    NvxMutexDestroy(pPort->hVPPMutex);
    pPort->hVPPMutex = NULL;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformPreChangeState(SNvxNvMMLiteTransformData *pData,
                                             OMX_STATETYPE oNewState,
                                             OMX_STATETYPE oOldState)
{
    int start = 0, stop = 0, pause = 0;
    NvMMLiteBlockHandle hNvMMLiteBlock = pData->hBlock;
    NvError status = NvSuccess;
    NvMMLiteState eState = NvMMLiteState_Running;

    if (!hNvMMLiteBlock)
        return OMX_ErrorNone;

    hNvMMLiteBlock->GetState(hNvMMLiteBlock, &eState);

    if (oNewState == OMX_StateExecuting &&
        (oOldState == OMX_StateIdle || oOldState == OMX_StatePause))
        start = 1;
    else if ((oNewState == OMX_StateIdle || oNewState == OMX_StateLoaded ||
        oNewState == OMX_StateInvalid) &&
        (oOldState == OMX_StateExecuting || oOldState == OMX_StatePause))
        stop = 1;
    else if (oNewState == OMX_StatePause && oOldState == OMX_StateExecuting)
        pause = 1;

    if (start && eState != NvMMLiteState_Running)
    {
        status = pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Running);
        if (status != NvSuccess)
            return OMX_ErrorInvalidState;

        return OMX_ErrorNone;
    }

    if (stop && eState != NvMMLiteState_Stopped)
    {
        NvU32 i;

        pData->bStopping = OMX_TRUE;
        hNvMMLiteBlock->SetState(hNvMMLiteBlock, NvMMLiteState_Stopped);
        pData->bStopping = OMX_FALSE;
        pData->bFlushing = OMX_TRUE;

        for (i = 0; i < pData->nNumStreams; i++)
        {
            SNvxNvMMLitePort *pPort = &(pData->oPorts[i]);

            if (pPort->nType != TF_TYPE_NONE)
            {
                hNvMMLiteBlock->AbortBuffers(hNvMMLiteBlock, i);
            }

            if (pPort->nType == TF_TYPE_INPUT)
            {
                if (pPort->pSavedEOSBuf)
                {
                    NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pSavedEOSBuf);
                    pPort->pSavedEOSBuf = NULL;
                }
            }
            else if (pPort->nType == TF_TYPE_OUTPUT)
            {
                FlushOutputQueue(pData, pPort);
            }
        }
        pData->bFlushing = OMX_FALSE;
        return OMX_ErrorNone;
    }
    else if (stop)
    {
        NvU32 i;
        pData->bFlushing = OMX_TRUE;
        for (i = 0; i < pData->nNumStreams; i++)
        {
            SNvxNvMMLitePort *pPort = &(pData->oPorts[i]);

            if (pPort->nType != TF_TYPE_NONE)
            {
                hNvMMLiteBlock->AbortBuffers(hNvMMLiteBlock, i);
            }

            if (pPort->nType == TF_TYPE_INPUT)
            {
                NvU32 j;
                for (j = 0; j < MAX_NVMMLITE_BUFFERS; j++)
                {

                    if (pPort->pOMXBufMap[j])
                    {
                        NvMMQueueEnQ(pData->pInputQ[pPort->nInputQueueIndex], &(pPort->pBuffers[j]), 10);
                        NvxPortReleaseBuffer(pPort->pOMXPort,
                            pPort->pOMXBufMap[j]);
                        pPort->pOMXBufMap[j] = NULL;
                        pPort->nBufsInUse--;
                    }
                }

                if (pPort->pSavedEOSBuf)
                {
                    NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pSavedEOSBuf);
                    pPort->pSavedEOSBuf = NULL;
                }
            }
            else if (pPort->nType == TF_TYPE_OUTPUT)
            {
                FlushOutputQueue(pData, pPort);
            }
        }
        pData->bFlushing = OMX_FALSE;
        return OMX_ErrorNone;
    }

    if (pause && eState != NvMMLiteState_Paused)
    {
        status = pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Paused);
        pData->bPausing = OMX_TRUE;
        status = pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Paused);
        pData->bPausing = OMX_FALSE;
        if (status != NvSuccess)
            return OMX_ErrorInvalidState;

        return OMX_ErrorNone;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformClose(SNvxNvMMLiteTransformData *pData)
{
    NvMMLiteBlockHandle hNvMMLiteBlock = pData->hBlock;
    NvU32 i = 0;

    NVMMLITEVERBOSE(("Closing NvxNvMMLiteTransform %s\n", pData->sName));

    if (hNvMMLiteBlock)
    {
        NvMMLiteState eState = NvMMLiteState_Running;

        hNvMMLiteBlock->GetState(hNvMMLiteBlock, &eState);
        if (eState != NvMMLiteState_Stopped)
        {
            hNvMMLiteBlock->SetState(hNvMMLiteBlock, NvMMLiteState_Stopped);
        }
        // Reset the following as block is closed.
        pData->VideoStreamInfo.Width = pData->VideoStreamInfo.Height = 0;
        pData->VideoStreamInfo.NumOfSurfaces = 0;

        // Free the scratch buffer if allocated
        if (pData->pScratchBuffer != NULL)
        {
            NvxVideoDecSurfaceFree(pData->pScratchBuffer);
            NvOsFree(pData->pScratchBuffer);
            pData->pScratchBuffer = NULL;
        }

        for (i = 0; i < pData->nNumStreams; i++)
        {
            if (pData->oPorts[i].nType != TF_TYPE_NONE)
            {
                pData->oPorts[i].bAborting = OMX_TRUE;
                hNvMMLiteBlock->AbortBuffers(hNvMMLiteBlock, i);
            }
            if (pData->oPorts[i].nType == TF_TYPE_OUTPUT)
            {
                FlushOutputQueue(pData, &pData->oPorts[i]);
            }
        }

        for (i = 0; i < pData->nNumStreams; i++)
            NvxNvMMLiteTransformFreePortBuffers(pData, i);

        for (i = 0; i < pData->nNumStreams; i++) {
            if (pData->oPorts[i].bUsesANBs && pData->oPorts[i].nBufsANB) {
                int j = 0;
                for (; j < MAX_NVMMLITE_BUFFERS; j++) {
                    if (pData->oPorts[i].pOMXANBBufMap[j])
                        NvxNvMMLiteTransformFreeBuffer(pData,pData->oPorts[i].pOMXANBBufMap[j],i);
                }
            }
        }

        NvMMLiteCloseBlock(hNvMMLiteBlock);
        pData->hBlock = NULL;

        for (i = 0; i < pData->nNumStreams; i++)
        {
            NvxNvMMLiteTransformDestroyPort(pData, i);
        }
    }
    else
    {
        for (i = 0; i < pData->nNumStreams; i++)
            NvxNvMMLiteTransformFreePortBuffers(pData, i);

        for (i = 0; i < pData->nNumStreams; i++) {
            if (pData->oPorts[i].bUsesANBs && pData->oPorts[i].nBufsANB) {
                int j = 0;
                for (; j < MAX_NVMMLITE_BUFFERS; j++) {
                    if (pData->oPorts[i].pOMXANBBufMap[j])
                        NvxNvMMLiteTransformFreeBuffer(pData,pData->oPorts[i].pOMXANBBufMap[j],i);
                }
            }
        }

        pData->hBlock = NULL;

        for (i = 0; i < pData->nNumStreams; i++)
        {
            NvxNvMMLiteTransformDestroyPort(pData, i);
        }
    }

    if (pData->bProfile)
    {
        NvOsFileHandle oOut;
        NvError status;

        if (pData->nNumTBTBPackets > 0)
        {
            NvU64 totTime = pData->llLastTBTB - pData->llFirstTBTB;
            NvOsDebugPrintf("Total packets: %d\n", pData->nNumTBTBPackets);
            NvOsDebugPrintf("Total display time (walltime): %f\n",
                totTime / 1000000.0);
            NvOsDebugPrintf("Average FPS (walltime): %f\n",
                1.0 / (totTime / pData->nNumTBTBPackets / 1000000.0));
            totTime = pData->llFinalOutput - pData->llFirstTBTB;
            NvOsDebugPrintf("Total decoding time (walltime): %f\n", totTime / 1000000.0);

            status = NvOsFopen((const char *)pData->sName,
                NVOS_OPEN_CREATE|NVOS_OPEN_WRITE,
                &oOut);
            if (NvSuccess == status)
            {
                totTime = pData->llLastTBTB - pData->llFirstTBTB;
                NvOsFprintf(oOut, "\n\n\n");
                NvOsFprintf(oOut, "Total packets: %d\n", pData->nNumTBTBPackets);
                NvOsFprintf(oOut, "Total display time (walltime): %f\n",
                    totTime / 1000000.0);
                NvOsFprintf(oOut, "Average FPS (walltime): %f\n",
                    1.0 / (totTime / pData->nNumTBTBPackets / 1000000.0));
                totTime = pData->llFinalOutput - pData->llFirstTBTB;
                NvOsFprintf(oOut, "\n\n\n");
                NvOsFprintf(oOut, "Total decoding time (walltime): %f\n", totTime / 1000000.0);
                NvOsFclose(oOut);

            }

        }
    }
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    /*Clear VPP Datastructure */
    if (pData->sVpp.bVppEnable)
    {
        NvxVPPdestroy(pData);
    }
#endif

    NvRmClose(pData->hRmDevice);
    pData->hRmDevice = 0;
    pData->nNumStreams = 0;

    return OMX_ErrorNone;
}

static void SetSkipFrames(OMX_BOOL bSkip, SNvxNvMMLiteTransformData *pBase)
{
    NvU32 val = (NvU32)bSkip;

    if (bSkip == pBase->bSkipBFrame)
        return;

    pBase->bSkipBFrame = bSkip;
    pBase->hBlock->SetAttribute(pBase->hBlock, NvMMLiteVideoDecAttribute_SkipFrames,
                                0, sizeof(NvU32), &val);
}

OMX_TICKS NvxNvMMLiteTransformGetMediaTime(SNvxNvMMLiteTransformData *pData,
                                       int nPort)
{
    NV_ASSERT(nPort < TF_NUM_STREAMS);
    return pData->oPorts[nPort].position;
}

static NvError NvxNvMMLiteTransformAllocateSurfaces(SNvxNvMMLiteTransformData *pData,
                                                NvMMLiteEventNewVideoStreamFormatInfo *pVideoStreamInfo,
                                                OMX_U32 streamIndex)
{
    NvU32 i = 0, ImageSize = 0;
    NvError status = NvError_Force32;
    NvBool UseAlignment = NV_FALSE;

    for (i = 0; i < pVideoStreamInfo->NumOfSurfaces; i++)
    {
        pData->oPorts[streamIndex].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
        if (!pData->oPorts[streamIndex].pBuffers[i])
        {
            NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !"));
            status = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(pData->oPorts[streamIndex].pBuffers[i], 0, sizeof(NvMMBuffer));
        status = NvxVideoSurfaceAlloc(pData->hRmDevice,
            pData->oPorts[streamIndex].pBuffers[i], i,
            pData->oPorts[streamIndex].nWidth, pData->oPorts[streamIndex].nHeight,
            pVideoStreamInfo->ColorFormat,
            /*NvRmSurfaceLayout_Pitch,*/pVideoStreamInfo->Layout,
            &ImageSize, UseAlignment, NvOsMemAttribute_Uncached, NV_FALSE);

        if (status != NvSuccess && NvMMLiteBlockType_DecSuperJPEG == pData->oBlockType)
        {
            pData->oPorts[streamIndex].nHeight = (pVideoStreamInfo->Height + 7) >> 3;
            pData->oPorts[streamIndex].nWidth = (pVideoStreamInfo->Width + 7) >> 3;
            status = NvxVideoSurfaceAlloc(pData->hRmDevice,
                pData->oPorts[streamIndex].pBuffers[i], i,
                pData->oPorts[streamIndex].nWidth, pData->oPorts[streamIndex].nHeight,
                pVideoStreamInfo->ColorFormat, pVideoStreamInfo->Layout,
                &ImageSize, UseAlignment, NvOsMemAttribute_Uncached, NV_FALSE);
        }

        if (status != NvSuccess)
        {
            if (pData->pParentComp)
            {
                OMX_ERRORTYPE eReturnVal = OMX_ErrorInsufficientResources;
                NvxComponent *pNvComp = pData->pParentComp;

                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
            }
            NvOsFree(pData->oPorts[streamIndex].pBuffers[i]);
            goto fail;
        }
    }
    return status;
fail:
    {
        NvU32 j = 0;
        for (j = 0; j < i; j++)
        {
            if (pData->oPorts[streamIndex].pBuffers[j])
            {
                NvxVideoDecSurfaceFree(pData->oPorts[streamIndex].pBuffers[j]);
                NvOsFree(pData->oPorts[streamIndex].pBuffers[j]);
                pData->oPorts[streamIndex].pBuffers[j] = NULL;
            }
        }
       return status;
    }
}

static void HandleNewStreamFormat(SNvxNvMMLiteTransformData *pData,
                                  NvU32 streamIndex, NvU32 eventType,
                                  void *eventInfo)
{
    NvError status = NvSuccess;
    NvU32 i = 0, NumSurfaces;
    NvMMLiteEventNewVideoStreamFormatInfo *NewStreamInfo;
    NvU32 nExtraBuffers = 2;
    SNvxNvMMLitePort *pPort;
    OMX_BOOL bNeedNewFormat = OMX_FALSE, bNeedUnpause = OMX_TRUE;
    OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    NvBool bThumbnailMode = NV_FALSE;
    NvU32 ImageSize = 0;
    NvBool UseNV21 = NV_FALSE;

    NVMMLITEVERBOSE(("New stream format: %d\n", streamIndex));

    if (OMX_TRUE == pData->bUseLowOutBuff)
        nExtraBuffers = 0;

    if (pData->bAudioTransform && eventInfo && (pData->oPorts[streamIndex].nType == TF_TYPE_INPUT))
    {
        if ((pData->oBlockType == NvMMLiteBlockType_DecAAC) || (pData->oBlockType == NvMMLiteBlockType_DecEAACPLUS)) {

            pPort = &(pData->oPorts[streamIndex]);
            NvxSendEvent(pData->pParentComp,
                             OMX_EventPortSettingsChanged,
                             pPort->pOMXPort->oPortDef.nPortIndex,
                             OMX_IndexParamAudioAac,
                             NULL);
        }
        return;
    }

    if (!eventInfo ||
        pData->oPorts[streamIndex].nType != TF_TYPE_OUTPUT ||
        !pData->bVideoTransform)
    {
        return;
    }

    NewStreamInfo = (NvMMLiteEventNewVideoStreamFormatInfo *)eventInfo;
    pPort = &(pData->oPorts[streamIndex]);

    pPort->bHasSize = OMX_TRUE;
    if(NewStreamInfo->IsMVC)
    {
        pData->is_MVC_present_flag = NV_TRUE ;
    }

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    if(!pData->bThumbnailMode && pData->sVpp.bVppEnable == OMX_TRUE)
    {
        pData->sVpp.frameWidth = NewStreamInfo->Width;
        pData->sVpp.frameHeight= NewStreamInfo->Height;
        pData->sVpp.importBufferID = 0;
        status = NvxVPPcreate(pData);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("VPP : unable to create VPP \n"));
            goto fail;
        }
    }
#endif

    if (NvMMLiteBlockType_DecSuperJPEG == pData->oBlockType)
    {
        int ht = NewStreamInfo->Height;
        int wd = NewStreamInfo->Width;
        int shiftbit = 0, pad = 0;
        OMX_S32 xsf = pData->xScaleFactorWidth > pData->xScaleFactorHeight ?
                      pData->xScaleFactorWidth : pData->xScaleFactorHeight;

        if (pData->bThumbnailPreferred)
        {
            if (NewStreamInfo->BitStreamProperty.JPEGBitStreamProperty.isThumbnailMarkerDecoded)
            {
                NvU32 data = 1;
                pData->hBlock->SetAttribute(pData->hBlock,
                    NvMMLiteVideoDecAttribute_Decode_Thumbnail, 0,
                    sizeof (NvU32), &data);
                ht = NewStreamInfo->BitStreamProperty.JPEGBitStreamProperty.ThumbnailImageHeight;
                wd = NewStreamInfo->BitStreamProperty.JPEGBitStreamProperty.ThumbnailImageWidth;
            }

            if (pData->nThumbnailWidth && pData->nThumbnailHeight)
            {
                if (wd >= (int)pData->nThumbnailWidth * 8 &&
                    ht >= (int)pData->nThumbnailHeight * 8)
                {
                    shiftbit = 3;
                    pad = 7;
                }
                else if (wd >= (int)pData->nThumbnailWidth * 4 &&
                         ht >= (int)pData->nThumbnailHeight * 4)
                {
                    shiftbit = 2;
                    pad = 3;
                }
                else if (wd >= (int)pData->nThumbnailWidth * 2 &&
                         ht >= (int)pData->nThumbnailHeight * 2)
                {
                    shiftbit = 1;
                    pad = 1;
                }
            }
        }
        else if (xsf <= NV_SFX_FP_TO_FX((float)0.125))
        {
            shiftbit = 3;
            pad = 7;
        }
        else if (xsf <= NV_SFX_FP_TO_FX((float)0.25))
        {
            shiftbit = 2;
            pad = 3;
        }
        else if (xsf <= NV_SFX_FP_TO_FX((float)0.5))
        {
            shiftbit = 1;
            pad = 1;
        }

        pPort->nHeight = (ht + pad) >> shiftbit;
        pPort->nWidth = (wd + pad) >> shiftbit;

        // this is for display, so decimate the size until it's smaller
        if (pPort->bUsesRmSurfaces || pPort->bWillCopyToANB)
        {
            // round to closest even value
            wd = (pPort->nWidth + 1) & ~1;
            ht = (pPort->nHeight + 1) & ~1;

            if (NewStreamInfo->BitStreamProperty.JPEGBitStreamProperty.scalingFactorsSupported)
            {
                while ((wd > 2000 || ht > 2000) && wd > 64 && ht > 64)
                {
                    wd = wd >> 1;
                    ht = ht >> 1;
                }
            }

            wd = (wd + 1) & ~1;
            ht = (ht + 1) & ~1;

            pPort->nWidth = wd;
            pPort->nHeight = ht;
        }

        NumSurfaces = NewStreamInfo->NumOfSurfaces + pData->nExtraSurfaces;

        // EXIF info
        pData->isEXIFPresent = (OMX_U8)(NewStreamInfo->BitStreamProperty.JPEGBitStreamProperty.isEXIFPresent);
        if (pData->pParentComp)
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvxSendEvent(pNvComp, NVX_EventImage_EXIFInfo, 0,
                         (OMX_U32)(pData->isEXIFPresent), 0);

            pData->nPrimaryImageWidth  = NewStreamInfo->Width;
            pData->nPrimaryImageHeight = NewStreamInfo->Height;
            switch (NewStreamInfo->ColorFormat) {
            case NvMMVideoDecColorFormat_GRAYSCALE:
                pData->eColorFormat = OMX_COLOR_FormatMonochrome;
               break;
            case NvMMVideoDecColorFormat_YUV420:
                pData->eColorFormat = OMX_COLOR_FormatYUV420Planar;
                break;
            case NvMMVideoDecColorFormat_YUV422:
                pData->eColorFormat = OMX_COLOR_FormatYUV422Planar;
                break;
            case NvMMVideoDecColorFormat_YUV422T:
                pData->eColorFormat = NVX_ColorFormatYUV422T;
                break;
            case NvMMVideoDecColorFormat_YUV444:
                pData->eColorFormat = NVX_ColorFormatYUV444;
                break;
            default:
                //FIXME
                break;
            }

            NvxSendEvent(pNvComp, NVX_EventImage_JPEGInfo, 0, 0, 0);
        }
    }
    else
    {
        if (pData->bThumbnailMode == NV_TRUE)
        {
            pData->hBlock->GetAttribute(pData->hBlock,NvMMLiteVideoDecAttribute_ThumbnailDecode, sizeof(OMX_BOOL), &bThumbnailMode);
        }
        pPort->nHeight = NewStreamInfo->Height;
        pPort->nWidth = NewStreamInfo->Width;
        if (bThumbnailMode)
            nExtraBuffers = 0;
        NumSurfaces = NewStreamInfo->NumOfSurfaces + nExtraBuffers;
    }

    if (pPort->nHeight == 0 || pPort->nWidth == 0)
    {
        pPort->nHeight = 160;
        pPort->nWidth = 120;

        if (pData->pParentComp)
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvxSendEvent(pNvComp, OMX_EventError,
                         OMX_ErrorFormatNotDetected, 0, NULL);
        }

        NV_DEBUG_PRINTF(("0 sized decoder image"));
        return;
    }

    if (NumSurfaces > MAX_NVMMLITE_BUFFERS)
        NumSurfaces = MAX_NVMMLITE_BUFFERS;

    if (pData->VideoStreamInfo.NumOfSurfaces && !pData->bAvoidOldFormatChangePath)
    {
        if (pData->VideoStreamInfo.Width == NewStreamInfo->Width &&
            pData->VideoStreamInfo.Height == NewStreamInfo->Height &&
            pData->VideoStreamInfo.NumOfSurfaces == NumSurfaces)
        {
            // If the NewStreamFormat is exactly same as to that of the
            // previous one, then we already have allocated all the buffers.
            // Just do nothing.
            return;
        }

        // If we *do * have preallocated buffers, this just store the
        // NewStreamInfo and trigger the WorkerThread DoWork function.
        // Which will pause the block, perform abort buffers, deallocation and
        // reallcoation of output buffers.
        pData->VideoStreamInfo = *NewStreamInfo;
        pData->VideoStreamInfo.NumOfSurfaces = NumSurfaces;
        pData->bReconfigurePortBuffers = OMX_TRUE;
        pData->nReconfigurePortNum = streamIndex;
    }

    NvOsDebugPrintf("Allocating new output: %dx%d (x %d), ThumbnailMode = %d\n",
                pPort->nWidth, pPort->nHeight, NumSurfaces, bThumbnailMode);

    if (pData->bReconfigurePortBuffers == OMX_FALSE || pPort->bUsesANBs)
        pPort->nBufsTot = 0;
    NewStreamInfo->NumOfSurfaces = NumSurfaces;

    //pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Paused);
    pData->bPausing = OMX_TRUE;

    pPort->outRect.nLeft = 0;
    pPort->outRect.nTop = 0;
    pPort->outRect.nWidth = pPort->nWidth;
    pPort->outRect.nHeight = pPort->nHeight;

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    if (!pData->bThumbnailMode && pData->sVpp.bVppEnable == OMX_TRUE)
    {
        NvU32 ImageSize = 0;
        NvError status = NvSuccess;
        for (i=0 ; i < NumSurfaces; i++)
        {
            pData->sVpp.pVideoSurf[i] = NvOsAlloc(sizeof(NvMMBuffer));
            if (!pData->sVpp.pVideoSurf[i])
            {
                NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !"));
                status = NvError_InsufficientMemory;
                goto fail;
            }
            NvOsMemset(pData->sVpp.pVideoSurf[i], 0, sizeof(NvMMBuffer));
            status = NvxVideoSurfaceAlloc(pData->hRmDevice,
                            pData->sVpp.pVideoSurf[i], i,
                            pData->oPorts[streamIndex].nWidth, pData->oPorts[streamIndex].nHeight,
                            NewStreamInfo->ColorFormat,
                            NewStreamInfo->Layout, &ImageSize, NV_FALSE,NvOsMemAttribute_WriteBack, UseNV21);
            if (status != NvSuccess)
            {
                if (pData->pParentComp)
                {
                    OMX_ERRORTYPE eReturnVal = OMX_ErrorInsufficientResources;
                    NvxComponent *pNvComp = pData->pParentComp;
                    NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
                }
                goto fail;
            }
            pData->sVpp.Renderstatus[i] = FRAME_FREE;
            pData->oPorts[streamIndex].pBuffers[i] =NvOsAlloc(sizeof(NvMMBuffer));
            if (!pData->oPorts[streamIndex].pBuffers[i])
            {
                NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !line=%d",__LINE__));
                status = NvError_InsufficientMemory;
                goto fail;
            }
            pData->oPorts[streamIndex].pBuffers[i] = pData->sVpp.pVideoSurf[i];
        }
    }
#endif

    if (pPort->bUsesANBs)
    {
        NvxPort *pOMXPort = pPort->pOMXPort;
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
        pPortDef = &(pOMXPort->oPortDef);

#if USE_ANDROID_NATIVE_WINDOW
        switch (NewStreamInfo->ColorFormat) {
            case NvMMVideoDecColorFormat_YUV420:
                if(NewStreamInfo->Layout == NvRmSurfaceLayout_Tiled)
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV420;
                }
                else //NvRmSurfaceLayout_Pitch
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YV12;
                }
                break;
            case NvMMVideoDecColorFormat_YUV420SemiPlanar:
                if(NewStreamInfo->Layout == NvRmSurfaceLayout_Blocklinear)
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_NV12;
                }
                break;
            case NvMMVideoDecColorFormat_YUV422:
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV422;
                break;
            case NvMMVideoDecColorFormat_YUV422T:
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV422R;
                break;
            case NvMMVideoDecColorFormat_YUV422SemiPlanar:
                if(NewStreamInfo->Layout == NvRmSurfaceLayout_Blocklinear)
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_NV16;
                }
                break;
            case NvMMVideoDecColorFormat_YUV444:
            case NvMMVideoDecColorFormat_GRAYSCALE:
            default:
                //We dont support YUV444 in graphics, hence by default setting it to YUV420
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV420;
                break;
        }
#endif
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        if (!pData->bThumbnailMode && pData->sVpp.bVppEnable == OMX_TRUE)
        {
            pPortDef->nBufferCountActual = NumSurfaces;
            pPortDef->nBufferCountMin = NumSurfaces;
            pOMXPort->nMinBufferCount = NumSurfaces;
            pOMXPort->nMaxBufferCount = NumSurfaces + 2;
            bNeedNewFormat = OMX_TRUE;
            pPort->bBlockANBImport = OMX_TRUE;
            for (i = 0; i < NumSurfaces; i++)
            {
                pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                                         NvMMBufferType_Payload,
                                         sizeof(NvMMBuffer),
                                         pData->sVpp.pVideoSurf[i]);
                //Trigger Nvmmlite block worker function
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
            }
        }
        else
#endif
        {
            if (NumSurfaces != pPortDef->nBufferCountActual ||
                NumSurfaces != pPortDef->nBufferCountMin)
            {
                pPortDef->nBufferCountActual = NumSurfaces;
                pPortDef->nBufferCountMin = NumSurfaces;
                pOMXPort->nMinBufferCount = NumSurfaces;
                pOMXPort->nMaxBufferCount = NumSurfaces + 2;
                bNeedNewFormat = OMX_TRUE;
                pPort->bBlockANBImport = OMX_TRUE;
            }
            else
            {
#if USE_ANDROID_NATIVE_WINDOW
                if (ImportAllANBsLite(pData->pParentComp, pData, streamIndex,
                                      pPortDef->format.video.nFrameWidth,
                                      pPortDef->format.video.nFrameHeight) != NvSuccess)
               {
                   pPort->bBlockANBImport = OMX_TRUE;
               }
#endif
            }
       }
    }
    else
    {
#ifdef BUILD_GOOGLETV
        if (pData->bVideoTransform && pPort->nNumPortSetting)
        {
            if (NvxListGetNumEntries(pPort->pPreviousBuffersList))
            {
                //Back-to-back port setting change events.
                NV_DEBUG_PRINTF(("Back-to-back port setting changed events received"));
            }
            pPort->bPortSettingChanged = OMX_TRUE;

            //Keep a backup of previous buffers
            if (NvxNvMMLiteTransformCreatePreviousBuffList(pPort) != 0)
            {
                NV_DEBUG_PRINTF(("%s::%d:: Error while backing up previous buffers.\n",__FUNCTION__,__LINE__));
                return;
            }

            pData->bStopping = OMX_TRUE;
            pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Stopped);
            pData->bStopping = OMX_FALSE;

            //Abort any buffers which are held by decoder.
            pPort->bAborting = OMX_TRUE;
            pData->hBlock->AbortBuffers(pData->hBlock, streamIndex);
            pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Running);
            pPort->bAborting = OMX_FALSE;
            pPort->bPortSettingChanged = OMX_FALSE;
        }
#endif

#ifdef NV_DEF_USE_PITCH_MODE
        NewStreamInfo->Layout = NvRmSurfaceLayout_Pitch;
#endif

        if((NumSurfaces + nExtraBuffers) < MAX_NVMMLITE_BUFFERS)
        {
            NumSurfaces += nExtraBuffers;
            pData->VideoStreamInfo.NumOfSurfaces = NewStreamInfo->NumOfSurfaces = NumSurfaces;
        }

        if (NewStreamInfo->ColorFormat == NvMMVideoDecColorFormat_YUV422)
        {
            pPort->pOMXPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV422Planar;
            bNeedNewFormat = OMX_TRUE;
        }
        else if (NewStreamInfo->ColorFormat == NvMMVideoDecColorFormat_YUV420SemiPlanar)
        {
            pPort->pOMXPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420SemiPlanar;
            bNeedNewFormat = OMX_TRUE;
        }
        else if (NewStreamInfo->ColorFormat == NvMMVideoDecColorFormat_YUV422SemiPlanar)
        {
            pPort->pOMXPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV422SemiPlanar;
            bNeedNewFormat = OMX_TRUE;
        }

        if (pData->bReconfigurePortBuffers == OMX_FALSE) {
            status = NvxNvMMLiteTransformAllocateSurfaces(pData, NewStreamInfo,
                                                      streamIndex);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Unable to allocate input buffers"));
                return;
            }
            // Reallocate ScratchBuffer for DRS cases
            if (pData->pScratchBuffer != NULL)
            {
                NvxVideoDecSurfaceFree(pData->pScratchBuffer);
                NvOsFree(pData->pScratchBuffer);
                pData->pScratchBuffer = NULL;
            }
            //Allocate scratch buffer if needed
            if (NewStreamInfo->Layout == NvRmSurfaceLayout_Blocklinear)
            {
                if (pData->bThumbnailMode)
                    UseNV21 = NV_TRUE;

                pData->pScratchBuffer = NvOsAlloc(sizeof(NvMMBuffer));
                if (!pData->pScratchBuffer)
                {
                    NV_DEBUG_PRINTF(("Unable to allocate the memory for the scratch buffer !"));
                    return;
                }
                NvOsMemset(pData->pScratchBuffer, 0, sizeof(NvMMBuffer));
                status = NvxVideoSurfaceAlloc(pData->hRmDevice,
                    pData->pScratchBuffer, 0,
                    NewStreamInfo->Width,
                    NewStreamInfo->Height,
                    NewStreamInfo->ColorFormat,
                    NvRmSurfaceLayout_Pitch,
                    &ImageSize, NV_TRUE, NvOsMemAttribute_Uncached, UseNV21);
                if (status != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("Unable to allocate the surface for scratch buffer !"));
                }
           }
#ifdef BUILD_GOOGLETV
            pPort->nNumPreviousBuffs = NumSurfaces;
            pPort->nNumPortSetting++;
#endif

            for (i = 0; i < NumSurfaces; i++)
            {
                pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                                             NvMMBufferType_Payload,
                                             sizeof(NvMMBuffer),
                                             pPort->pBuffers[i]);
                //Trigger Nvmmlite block worker function
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                pPort->nBufsTot++;
            }

            // assign the surface width/height to port for configuration
            if (pPort->pBuffers[0])
            {
                pPort->nWidth = (pPort->pBuffers[0])->Payload.Surfaces.Surfaces[0].Width;
                pPort->nHeight = (pPort->pBuffers[0])->Payload.Surfaces.Surfaces[0].Height;
            }
        }
    }

    if (pData->nNvMMLiteProfile & 0x10)
    {
        pData->NumBuffersWithBlock = NumSurfaces;
        NvOsDebugPrintf("%s: Number of buffers with Block : %d \n",
                        pData->sName, pData->NumBuffersWithBlock);
    }

    // Cache the NewStreamInfo information with us.
    // This will help us in multiple SPS case.
    pData->VideoStreamInfo = *NewStreamInfo;
    pData->VideoStreamInfo.NumOfSurfaces = NumSurfaces;

    // send event OMX_EventPortSettingsChanged, now that width and height
    // information are available.
    if (bNeedNewFormat ||
        pPort->pOMXPort->oPortDef.format.video.nFrameWidth != pPort->nWidth ||
        pPort->pOMXPort->oPortDef.format.video.nFrameHeight != pPort->nHeight)
    {
        pPort->outRect.nWidth = pPort->nWidth;
        pPort->outRect.nHeight = pPort->nHeight;

        bNeedUnpause = OMX_FALSE;

        NvxSendEvent(pData->pParentComp,
                     OMX_EventPortSettingsChanged,
                     pPort->pOMXPort->oPortDef.nPortIndex,
                     OMX_IndexParamPortDefinition,
                     NULL);
    }

    pOMXBuf = pPort->pOMXPort->pCurrentBufferHdr;
    if (NULL != pOMXBuf)
        pPrivateData = (NvxBufferPlatformPrivate *)pOMXBuf->pPlatformPrivate;

    if (bNeedUnpause || pPort->bUsesRmSurfaces || pData->bEmbedRmSurface ||
        !pData->bVideoTransform || pPort->nType != TF_TYPE_OUTPUT ||
        ((pPrivateData) && (pPrivateData->eType == NVX_BUFFERTYPE_EGLIMAGE)))
    {
        pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Running);
        pData->bPausing = OMX_FALSE;
    }
    return;

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
fail:
    if (!pData->bThumbnailMode && pData->sVpp.bVppEnable == OMX_TRUE)
    {
        NvxVPPdestroy(pData);
        NvU32 j = 0;
        for (j = 0; j <= i; j++)
        {
            if (pData->sVpp.pVideoSurf[j])
            {
                NvxVideoDecSurfaceFree(pData->sVpp.pVideoSurf[j]);
                NvOsFree(pData->sVpp.pVideoSurf[j]);
                pData->sVpp.pVideoSurf[j] = NULL;
            }
        }
    }
    return;
#endif
}

static void HandleStreamEvent(SNvxNvMMLiteTransformData *pData, NvU32 streamIndex,
                              NvU32 eventType, void *eventInfo)
{
    switch (eventType)
    {
    case NvMMLiteEvent_NewBufferRequirements:
        NVMMLITEVERBOSE(("%s: NvMMLiteEvent_NewBufferRequirements stream=%i\n",
            pData->sName, streamIndex));
        if (streamIndex < pData->nNumStreams)
        {
            NvOsMemcpy(&(pData->oPorts[streamIndex].bufReq), eventInfo,
                sizeof(NvMMLiteNewBufferRequirementsInfo));
        }
        break;
    case NvMMLiteEvent_StreamEnd:
        NVMMLITEVERBOSE(("%s: StreamEndEvent\n", pData->sName));
        if ((pData->oPorts[streamIndex].nType == TF_TYPE_OUTPUT) && (!pData->bFlushing))
        {
            pData->oPorts[streamIndex].bEOS = OMX_TRUE;
            NVMMLITEVERBOSE(("End of Output Stream\n"));
        }
        break;

    case NvMMLiteEvent_ProfilingData:
        {
            NvMMLiteEventProfilingData *pprofilingData = (NvMMLiteEventProfilingData *)eventInfo;
            NV_DEBUG_PRINTF(("\nProfiling Data Event"));
            if (pprofilingData)
                NV_DEBUG_PRINTF(("\nInstanceName: %s", pprofilingData->InstanceName));
            NV_DEBUG_PRINTF(("\nTotal Decode Time : %d", pprofilingData->AccumulatedTime));
            NV_DEBUG_PRINTF(("\nNumber of Frames  : %d", pprofilingData->NoOfTimes));
            NV_DEBUG_PRINTF(("\nStarvation Count  : %d", pprofilingData->inputStarvationCount));
        }
        break;

    case NvMMLiteEvent_NewStreamFormat:
        {
            HandleNewStreamFormat(pData, streamIndex, eventType, eventInfo);
        }
        break;
    case NvMMLiteEvent_BlockError:
        {
            NvMMLiteBlockErrorInfo *pBlockErr = (NvMMLiteBlockErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;

            NVMMLITEVERBOSE(("StreamBlockErrorEvent\n"));

            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                eReturnVal = (pBlockErr) ? NvxTranslateErrorCode(pBlockErr->error) : OMX_ErrorUndefined;
                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
            }
        }
        break;

    case NvMMLiteEvent_StreamError:
        {
            NvMMLiteStreamErrorInfo *pStreamErr = (NvMMLiteStreamErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;

            NVMMLITEVERBOSE(("StreamStreamErrorEvent\n"));
            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                eReturnVal = NvxTranslateErrorCode(pStreamErr->error);
                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, 0);
            }
        }
        break;

    case NvMMLiteEvent_Unknown:
    default:
        break;
    }
}

static void CopyMarkDataToQueue(NvMMQueueHandle pMarkQueue, NvxPort *pInPort, OMX_BUFFERHEADERTYPE *pInHdr)
{
    BufferMarkData data;

    data.hMarkTargetComponent = pInHdr->hMarkTargetComponent;
    data.pMarkData = pInHdr->pMarkData;
    data.nTimeStamp = pInHdr->nTimeStamp;
    data.nFlags = pInHdr->nFlags & ~OMX_BUFFERFLAG_EOS;
    data.pMetaDataSource = pInPort->pMetaDataSource;

    pInPort->bSendEvents = OMX_FALSE;

    NvMMQueueEnQ(pMarkQueue, &data, 10);
}

static void CopyMarkDataFromQueue(NvMMQueueHandle pMarkQueue, NvxPort *pOutPort,
                                  OMX_BUFFERHEADERTYPE *pOutHdr)
{

    BufferMarkData data;

    if (NvSuccess != NvMMQueueDeQ(pMarkQueue, &data))
        return;

    pOutHdr->hMarkTargetComponent = data.hMarkTargetComponent;
    pOutHdr->pMarkData = data.pMarkData;
    pOutHdr->nTimeStamp = data.nTimeStamp;
    pOutHdr->nFlags |= data.nFlags;

    pOutPort->bSendEvents = OMX_TRUE;
    pOutPort->uMetaDataCopyCount = 0;
    pOutPort->pMetaDataSource = data.pMetaDataSource; /* this is for marks, so we can send on last output */

    pOutPort->pMetaDataSource->uMetaDataCopyCount++;
}


static NvError
NvxNvMMLiteTransformReturnEmptyInput(
                                 void *pContext,
                                 NvU32 streamIndex,
                                 NvMMBufferType BufferType,
                                 NvU32 BufferSize,
                                 void *pBuffer)
{
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)pContext;
    NvxComponent *pComp = pData->pParentComp;
    NvError eError = NvSuccess;

    if (BufferType == NvMMBufferType_StreamEvent)
    {
        NvMMLiteEventType eventType = ((NvMMLiteStreamEventBase *)pBuffer)->event;
        HandleStreamEvent(pData, streamIndex, eventType, pBuffer);
    }

    if (BufferType == NvMMBufferType_Payload)
    {
        NvMMBuffer *PayloadBuffer = (NvMMBuffer *)pBuffer;
        SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
        int id = PayloadBuffer->BufferID;

        NvxMutexLock(pComp->hWorkerMutex);

        if (pPort->pBuffers[id])
            *(pPort->pBuffers[id]) = *PayloadBuffer;

        eError = NvMMQueueEnQ(pData->pInputQ[pPort->nInputQueueIndex], &(pPort->pBuffers[id]), 10);
        if (NvSuccess != eError)
        {
            NvOsDebugPrintf("ERROR: buffer enqueue failed line %d 0x%x \n", __LINE__, eError);
            return eError;
        }

        if (pPort->pOMXBufMap[id])
        {
            if (pData->bProfile)
            {
                pData->llLastTBTB = NvOsGetTimeUS();
                NVMMLITEVERBOSE(("%s: ReturnEmpty: (%p) at %f\n",
                    pData->sName,
                    pPort->pOMXBufMap[id],
                    pData->llLastTBTB / 1000000.0));
            }

            if (!pData->bFlushing)
                pPort->position = pPort->pOMXBufMap[id]->nTimeStamp;

            if (pData->nNumOutputPorts > 0)
                CopyMarkDataToQueue(pPort->pMarkQueue, pPort->pOMXPort,
                                    pPort->pOMXBufMap[id]);

            if (!pPort->bSkipWorker)
                NvxMutexLock(pPort->oMutex);

            pPort->nBufsInUse--;

            if ((OMX_TRUE == pPort->bEmbedNvMMBuffer) && (!pPort->bEnc2DCopy) && (!pPort->bUsesAndroidMetaDataBuffers))
            {
                NvOsMemcpy(pPort->pOMXBufMap[id]->pBuffer, PayloadBuffer, sizeof(NvMMBuffer));
                pPort->pOMXBufMap[id]->nFlags |= OMX_BUFFERFLAG_NV_BUFFER;
                pPort->pOMXBufMap[id]->nFilledLen = sizeof(NvMMBuffer);
            }
            {
                // if eglstreamsource, release memhandle!
                OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
                NvxBufferPlatformPrivate *pPrivateData = NULL;

                pOMXBuf = pPort->pOMXBufMap[id];
                pPrivateData = (NvxBufferPlatformPrivate *)pOMXBuf->pPlatformPrivate;

                if (!pPrivateData)
                    goto doneAndMoveToNext;

                if ( !(pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T ||
                       pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T ||
                       pPrivateData->eType == NVX_BUFFERTYPE_EGLIMAGE) &&
                       pPort->bUsesAndroidMetaDataBuffers
                    )
                {
#if USE_ANDROID_NATIVE_WINDOW
                    OMX_U32 nMetadataBufferFormat;

                    if (pOMXBuf->nFilledLen <= 4)
                    {
                        goto doneAndMoveToNext;
                    }

                    nMetadataBufferFormat = U32_AT(pOMXBuf->pBuffer);
                    if( nMetadataBufferFormat == kMetadataBufferTypeEglStreamSource )
                    {
                        OMX_BUFFERHEADERTYPE *pEmbeddedOMXBuffer = NULL;
                        NvMMBuffer *pEmbeddedNvmmBuffer = NULL;

                        NV_DEBUG_PRINTF(("Meta Data Buffer Format is Egl Source  -- FREE(dec RefCnt) RMHANDLE NOW++\n"));
                        pEmbeddedOMXBuffer = (OMX_BUFFERHEADERTYPE*)(pOMXBuf->pBuffer + 4);

                        if (!pEmbeddedOMXBuffer)
                            goto doneAndMoveToNext;
                        if (pEmbeddedOMXBuffer->nFilledLen != sizeof(NvMMBuffer))
                            goto doneAndMoveToNext;

                        // retrieve the embedded Nvmm Buffer
                        pEmbeddedNvmmBuffer = (NvMMBuffer*)(pOMXBuf->pBuffer + 4 +
                                                sizeof(OMX_BUFFERHEADERTYPE));

                        if (!pEmbeddedNvmmBuffer)
                            goto doneAndMoveToNext;

                        // release the hMem which was accessed using gethandle api call.
                        NvRmMemHandleFree(pEmbeddedNvmmBuffer->Payload.Surfaces.Surfaces[0].hMem);
                        NV_DEBUG_PRINTF(("Meta Data Buffer Format is Egl Source  -- FREE(dec RefCnt) RMHANDLE NOW--\n"));
                    }
#endif
                }
            }
doneAndMoveToNext:
            NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pOMXBufMap[id]);
            pPort->pOMXBufMap[id] = NULL;
            if (!pPort->bSkipWorker)
                NvxMutexUnlock(pPort->oMutex);
        }

        if (pPort->bSkipWorker && (pData->bFlushing || pData->bPausing))
        {
            NvxNvMMLiteSendNextInputBufferBack(pData, streamIndex);
            NvxMutexUnlock(pComp->hWorkerMutex);
            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
            return NvSuccess;
        }
        else if (pPort->bSkipWorker && !pPort->bSentEOS && !pPort->bAborting)
        {
            int attempts = 0, tot_attempts = 0;
            // WA: Assert if no buffer available
            while (!pPort->pOMXPort->pCurrentBufferHdr)
            {
                NvxPortGetNextHdr(pPort->pOMXPort);

                if (!pPort->pOMXPort->pCurrentBufferHdr)
                {
                    NvxMutexUnlock(pComp->hWorkerMutex);

                    attempts++;
                    if (attempts > 100 || pData->bPausing)
                    {
                        NvMMLiteBlockHandle hNvMMLiteBlock = pData->hBlock;
                        NvMMLiteState eState = NvMMLiteState_Running;

                        if (!pData->bPausing)
                            NV_DEBUG_PRINTF(("comp %s : Input (port %d) taking a very long time\n",
                                            pData->sName, streamIndex));

                        hNvMMLiteBlock->GetState(hNvMMLiteBlock, &eState);
                        if (eState == NvMMLiteState_Stopped)
                        {
                            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                            return NvSuccess;
                        }

                        if (eState == NvMMLiteState_Paused)
                        {
                            // Transfer the buffer back to block
                            NvxNvMMLiteSendNextInputBufferBack(pData, streamIndex);
                            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                            return NvSuccess;
                        }

                        attempts = 0;
                        tot_attempts++;

                        if (tot_attempts > 10)
                            NV_ASSERT(pPort->pOMXPort->pCurrentBufferHdr);
                    }

                    NvOsSleepMS(10);
                    NvxMutexLock(pComp->hWorkerMutex);
                }
            }
            NvxNvMMLiteProcessInputBuffer(pData, streamIndex, NULL);
            if (pPort->bSentEOS)
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        }
        else if (pPort->pOMXPort)
        {
            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        }

      NvxMutexUnlock(pComp->hWorkerMutex);
    }

    return NvSuccess;
}

static OMX_ERRORTYPE NvxPortBufferPtrToRmSurf(NvxPort *pPort, OMX_U8 *pBuf, NvMMSurfaceDescriptor **pSurf)
{
    OMX_U32 i;
    *pSurf = NULL;
    for (i = 0; i < pPort->nReqBufferCount; i++)
    {
        if (pBuf == pPort->ppBufferHdrs[i]->pBuffer)
        {
            *pSurf = (NvMMSurfaceDescriptor *)pPort->ppRmSurf[i];
            return OMX_ErrorNone;
        }
    }
    return OMX_ErrorUndefined;
}

/* Adjust the crop rect to the left eye for monoscopic displays */
static void GetStereoCropLeft(NvU32 flags, NvRect *crop)
{
    NvStereoType stereoType = NvStereoTypeFromBufferFlags(flags);

    switch (stereoType) {
    case NV_STEREO_LEFTRIGHT:
        crop->right = (crop->left + crop->right) >> 1;
        break;
    case NV_STEREO_RIGHTLEFT:
        crop->left  = (crop->left + crop->right) >> 1;
        break;
    case NV_STEREO_TOPBOTTOM:
        crop->bottom = (crop->top + crop->bottom) >> 1;
        break;
    case NV_STEREO_BOTTOMTOP:
        crop->top = (crop->top + crop->bottom) >> 1;
        break;
    default:
        NvOsDebugPrintf("Unsupported stereo type 0x%x", (int)stereoType);
        break;
    }
}

// Note: Worker function should call this function.
NvError NvxNvMMLiteTransformProcessOutputBuffers(SNvxNvMMLiteTransformData *pData,
     NvU32 streamIndex,
     void *pBuffer)
{
    SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvU32 id = EMPTY_BUFFER_ID;
    NvMMBuffer *PayloadBuffer = NULL;
    NvError eError = NvSuccess;

    if (pBuffer)
    {
        PayloadBuffer = (NvMMBuffer *)pBuffer;
        id = (int) PayloadBuffer->BufferID;
    }
    NV_DEBUG_PRINTF(("%s: %s[%d] id %d streamidx %d + \n",
        pData->sName,
     __FUNCTION__, __LINE__, id, streamIndex));
    if (pPort->bEOS && !pPort->bSentEOS && !pPort->bError && (id == EMPTY_BUFFER_ID || !pBuffer))
    {
        // eos event will trigger worker thread.
        NvxPortGetNextHdr(pPortOut);
        if (!pPortOut->pCurrentBufferHdr)
        {
            if (pData->bStopping || pPort->bAborting || pData->bFlushing)
                return NvSuccess;
            else
                return NvError_Busy;
        }
        pPortOut->pCurrentBufferHdr->nFilledLen = 0;
        pPortOut->pCurrentBufferHdr-> nTimeStamp = pData->LastTimeStamp;
        pPortOut->pCurrentBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
        NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
        NVMMLITEVERBOSE(("delivering EOS buffer\n"));
        NvDebugOmx(("Block %s: Delivering EOS buffer", pData->sName));
        pPort->bSentEOS = OMX_TRUE;

        if (pData->bProfile)
        {
            pData->llFinalOutput = NvOsGetTimeUS();
            NVMMLITEVERBOSE(("%s: DeliverFullOutput: (%p) at %f\n",
                pData->sName,
                pPortOut->pCurrentBufferHdr,
                pData->llFinalOutput / 1000000.0));
        }

        if (pData->bSequence)
        {
            // for next frame in the sequence
            pPort->bEOS = OMX_FALSE;
            pPort->bSentEOS = OMX_FALSE;
        }
        return NvSuccess;
    }
    if (id == EMPTY_BUFFER_ID)
        return NvSuccess;

    if (!pPort->bSentEOS && !pPort->bAborting && pBuffer)
    {
        NvMMBuffer *PayloadBuffer = (NvMMBuffer *)pBuffer;
        int id = (int) PayloadBuffer->BufferID;
        OMX_BOOL bNoData = OMX_FALSE;
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        OMX_BOOL bCanReturnBufferToBlock = OMX_TRUE;

        if (!pPort->pBuffers[id])
        {
            goto done;
        }
        if (pData->bFlushing)
        {
            pPort->bBufNeedsFlush[id] = OMX_TRUE;
            NV_DEBUG_PRINTF(("%s: %s[%d] - \n", pData->sName, __FUNCTION__, __LINE__));
            eError = NvError_Busy;
            goto done;
        }
        if (pData->bVideoTransform && pPort->nSeekType == OMX_TIME_SeekModeAccurate)
        {
            NvU64 nCurrentTimestamp = PayloadBuffer->PayloadInfo.TimeStamp;
            if (nCurrentTimestamp < pPort->nTimestamp)
            {
                // start skipping B frame
                SetSkipFrames(OMX_TRUE, pData);

                // Discard the output buffer
                NV_DEBUG_PRINTF(("[%s] - %d Discard the output buffer",pData->sName, streamIndex));
                pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                       NvMMBufferType_Payload,
                       sizeof(NvMMBuffer),
                       pPort->pBuffers[id]);
                //Trigger Nvmmlite block worker function
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                goto done;
            }
            pPort->nSeekType = OMX_TIME_SeekModeFast;

            // stop skipping B frame
            SetSkipFrames(OMX_FALSE, pData);
        }

        NvxPortGetNextHdr(pPortOut);
        // WA: Assert if no buffer available
        if (!pPortOut->pCurrentBufferHdr)
        {
            // Try later, whenever buffers are available.
            return NvError_Busy;
        }
        pBufferHdr = pPortOut->pCurrentBufferHdr;
        if (pData->bProfile)
        {
            NVMMLITEVERBOSE(("%s:%d Got full id %d (len %d) at %f, ts: %f\n",
                pData->sName, streamIndex, id,
                PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes,
                NvOsGetTimeUS() / 1000000.0,
                PayloadBuffer->PayloadInfo.TimeStamp / 10000000.0));
            if (pData->bDiscardOutput)
            {
                pPort->pBuffers[id]->Payload.Ref.startOfValidData = 0;
                pPort->pBuffers[id]->Payload.Ref.sizeOfValidDataInBytes = 0;
                pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                    NvMMBufferType_Payload,
                    sizeof(NvMMBuffer),
                    pPort->pBuffers[id]);
                //Trigger Nvmmlite block worker function
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                goto done;
            }
        }

        if (pData->bVideoTransform)
        {
            NvxBufferPlatformPrivate *pPrivateData =
                (NvxBufferPlatformPrivate *)pBufferHdr->pPlatformPrivate;
            // Extract stereo type from input buffer flags
            NvStereoType stereoType =
                NvStereoTypeFromBufferFlags(PayloadBuffer->PayloadInfo.BufferFlags);
            if (pPort->bUsesRmSurfaces)
            {
                // tunnelling
                NvMMBuffer *pOutBuf = NULL;
                pOutBuf = (NvMMBuffer *)pBufferHdr->pBuffer;
                pOutBuf->Payload.Surfaces = PayloadBuffer->Payload.Surfaces;
                pOutBuf->Payload.Ref.sizeOfValidDataInBytes = PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes;
                pBufferHdr->nFilledLen = sizeof(NvMMBuffer);
                pOutBuf->PayloadInfo.BufferFlags |= PayloadBuffer->PayloadInfo.BufferFlags;
                if (PayloadBuffer->PayloadInfo.BufferMetaDataType ==
                    NvMMBufferMetadataType_DigitalZoom)
                {
                    NvOsMemcpy(&pOutBuf->PayloadInfo,
                               &PayloadBuffer->PayloadInfo,
                               sizeof(NvMMPayloadMetadata));
                }

                // If needed, put stereo type in private data
                if (pPrivateData && NV_STEREO_NONE != stereoType)
                {
                    pPrivateData->StereoInfo = (OMX_U32) stereoType;
                }

                bCanReturnBufferToBlock = OMX_FALSE;
            }
            else
            {
                // not tunneling..
                NvRect *CropRect = &PayloadBuffer->Payload.Surfaces.CropRect;
                NvMMDisplayResolution *DispRes;
                OMX_U32 nCropW, nCropH;
                OMX_S32 xWidth, xHeight;

                // Images have croprect set to values before scaling, correct if needed
                if (NvMMBlockType_DecSuperJPEG == pData->oBlockType
                    && (pPort->bUsesRmSurfaces || pPort->bWillCopyToANB))
                {
                    CropRect->left = 0;
                    CropRect->top = 0;
                    CropRect->right = pPort->nWidth;
                    CropRect->bottom = pPort->nHeight;
                }

                if ((pData->is_MVC_present_flag != NV_TRUE)
                    &&(PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_StereoEnable))
                {
                    GetStereoCropLeft(PayloadBuffer->PayloadInfo.BufferFlags, CropRect);
                }

                nCropW = (OMX_U32)(CropRect->right - CropRect->left);
                nCropH = (OMX_U32)(CropRect->bottom - CropRect->top);

                if ((pPrivateData->croprect.nLeft != CropRect->left) ||
                    (pPrivateData->croprect.nTop != CropRect->top) ||
                    (pPrivateData->croprect.nWidth != nCropW) ||
                    (pPrivateData->croprect.nHeight != nCropH))
                {
                    if (!(nCropW ==0 || nCropH ==0))
                    {
                        pPrivateData->croprect.nLeft = CropRect->left;
                        pPrivateData->croprect.nTop = CropRect->top;
                        pPrivateData->croprect.nWidth = nCropW;
                        pPrivateData->croprect.nHeight = nCropH;
                    }
                }

                DispRes = &PayloadBuffer->Payload.Surfaces.DispRes;

                if ((CropRect->left != pPort->outRect.nLeft)      ||
                    (CropRect->top != pPort->outRect.nTop)        ||
                    (nCropW != pPort->outRect.nWidth) ||
                    (nCropH != pPort->outRect.nHeight))
                {
                    //Sent Event Handler notification to client about crop rectangle changes.
                    if (!(nCropW ==0 || nCropH ==0))
                    {
                        pPort->outRect.nLeft = CropRect->left;
                        pPort->outRect.nTop = CropRect->top;
                        pPort->outRect.nWidth = nCropW;
                        pPort->outRect.nHeight = nCropH;
                        NvxSendEvent(pData->pParentComp,
                          OMX_EventPortSettingsChanged,
                          pPortOut->oPortDef.nPortIndex,
                          OMX_IndexConfigCommonOutputCrop,
                          (void *)&pPort->outRect);
                    }
                }

                // Recalculate scaling multiplier for stereo case.
                if (NV_STEREO_NONE != stereoType) {
                    switch (stereoType) {
                        case NV_STEREO_LEFTRIGHT:
                        case NV_STEREO_RIGHTLEFT:
                            // If disp res was invalid (zero), we must set it to the original size
                            // of the video. That size is outrect width x2.
                            if (DispRes->Width == 0)
                                DispRes->Width = pPort->outRect.nWidth * 2;
                            break;
                        case NV_STEREO_TOPBOTTOM:
                        case NV_STEREO_BOTTOMTOP:
                            // If disp res was invalid (zero), we must set it to the original size
                            // of the video. That size is outrect height x2.
                            if (DispRes->Height == 0)
                                DispRes->Height = pPort->outRect.nHeight * 2;
                            break;
                        default:
                            break;
                    }
                }

                if (DispRes->Width == 0)
                    DispRes->Width = pPort->outRect.nWidth;
                if (DispRes->Height == 0)
                    DispRes->Height = pPort->outRect.nHeight;
                if (pPort->DispWidth == 0)
                    pPort->DispWidth = pPort->outRect.nWidth;
                if (pPort->DispHeight == 0)
                    pPort->DispHeight = pPort->outRect.nHeight;

                xWidth = NV_SFX_FP_TO_FX(DispRes->Width * 1.0 /
                                         pPort->outRect.nWidth);
                xHeight = NV_SFX_FP_TO_FX(DispRes->Height * 1.0 /
                                          pPort->outRect.nHeight);
                if ((xWidth != pPort->xScaleWidth ||
                    xHeight != pPort->xScaleHeight) ||
                    (pPort->DispWidth != DispRes->Width ||
                    pPort->DispHeight != DispRes->Height))
                {
                    pPort->xScaleWidth = xWidth;
                    pPort->xScaleHeight = xHeight;
                    pPort->DispWidth = DispRes->Width;
                    pPort->DispHeight = DispRes->Height;
                    NvxSendEvent(pData->pParentComp,
                        OMX_EventPortSettingsChanged,
                        pPortOut->oPortDef.nPortIndex,
                        OMX_IndexConfigCommonScale,
                        NULL);
                }

                if (pPrivateData->eType == NVX_BUFFERTYPE_EGLIMAGE)
                {
                    // Stretch Blit source to EGL image handle
                    NvxEglImageSiblingHandle hEglSib =
                        (NvxEglImageSiblingHandle)pBufferHdr->pBuffer;

                    NvxEglStretchBlitToImage(hEglSib,
                        &(PayloadBuffer->Payload.Surfaces),
                        NV_TRUE, NULL, pData->mirror_type);
                }
#if USE_ANDROID_NATIVE_WINDOW
                else if (pPort->bUsesAndroidMetaDataBuffers ||
                        pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T ||
                        pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T)
                {
                    // If this flag got set earlier, call older function
                    if (!pPort->bWillCopyToANB)
                    {
                        ExportANBLite(pData->pParentComp,
                                      pData, PayloadBuffer,
                                      &pBufferHdr, pPort, OMX_FALSE);
                    }
                    else
                    {
                        CopyNvxSurfaceToANBLite(pData->pParentComp,
                                        PayloadBuffer,
                                        pBufferHdr);
                    }
                }
#endif
                else if (OMX_TRUE == pPort->bEmbedNvMMBuffer)
                {
                    // Embed NvMMLite Buffer into OMX buffer.
                    NvOsMemcpy(pBufferHdr->pBuffer, PayloadBuffer,
                               sizeof(NvMMBuffer));
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_NV_BUFFER;
                    pBufferHdr->nFilledLen = sizeof(NvMMBuffer);
                }
                else
                {
                    NvMMSurfaceDescriptor *pDestSurf = NULL;
                    NvxPortBufferPtrToRmSurf(pPortOut, pBufferHdr->pBuffer,
                                             &pDestSurf);

                    if (pDestSurf)
                    {
                        // Compute SB settings
                        NvRect DestRect;
                        NvOsMemset(&DestRect, 0, sizeof(NvRect));
                        DestRect.right = pDestSurf->Surfaces[0].Width;
                        DestRect.bottom = pDestSurf->Surfaces[0].Height;

                        NvxCopyMMSurfaceDescriptor(pDestSurf, DestRect,
                                &PayloadBuffer->Payload.Surfaces, *CropRect);
                        pBufferHdr->nFilledLen = sizeof(NvMMSurfaceDescriptor);
                    }
                    else
                    {
                        NvMMSurfaceDescriptor *pDestSurf = NULL;

                        NvxPortBufferPtrToRmSurf(pPortOut,
                            pPortOut->pCurrentBufferHdr->pBuffer, &pDestSurf);

                        if (pDestSurf)
                        {
                            // Compute SB settings
                            NvRect DestRect;
                            NvOsMemset(&DestRect, 0, sizeof(NvRect));
                            DestRect.right = pDestSurf->Surfaces[0].Width;
                            DestRect.bottom = pDestSurf->Surfaces[0].Height;

                            NvxCopyMMSurfaceDescriptor(pDestSurf, DestRect,
                                &PayloadBuffer->Payload.Surfaces, *CropRect);
                            pPortOut->pCurrentBufferHdr->nFilledLen = sizeof(NvMMSurfaceDescriptor);
                        }
                        else
                        {
                            if ((pData->bEmbedRmSurface) ||
                                (pPrivateData->eType == NVX_BUFFERTYPE_NEEDRMSURFACE) ||
                                (pPrivateData->eType == NVX_BUFFERTYPE_HASRMSURFACE))
                            {
                                NvU32 size = PayloadBuffer->Payload.Surfaces.SurfaceCount * sizeof(NvRmSurface);
                                if (size > pPortOut->pCurrentBufferHdr->nAllocLen)
                                {
                                    NvOsDebugPrintf("buffer too small for surface %d \n", pPortOut->pCurrentBufferHdr->nAllocLen);
                                    pPortOut->pCurrentBufferHdr->nFilledLen = 0;
                                    pPrivateData->eType = NVX_BUFFERTYPE_NORMAL;
                                }
                                else
                                {
                                    NvOsMemcpy(pPortOut->pCurrentBufferHdr->pBuffer, PayloadBuffer->Payload.Surfaces.Surfaces, size);
                                    pPortOut->pCurrentBufferHdr->nFilledLen = size;
                                    pPrivateData->eType = NVX_BUFFERTYPE_HASRMSURFACE;
                                    pPrivateData->StereoInfo = (OMX_U32) stereoType;
                                    bCanReturnBufferToBlock = OMX_FALSE;
                                }
                            }
                            else
                            {
                                pPrivateData->eType = NVX_BUFFERTYPE_NORMAL;
                                pPrivateData->StereoInfo = (OMX_U32) stereoType;
                                NvOsMemcpy(&(pData->pScratchBuffer->PayloadInfo), &(PayloadBuffer->PayloadInfo), sizeof(PayloadBuffer->PayloadInfo));
                                NvxCopyMMBufferToOMXBuf(PayloadBuffer, pData->pScratchBuffer != NULL? pData->pScratchBuffer:NULL, pPortOut->pCurrentBufferHdr);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            NvU32 size = PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes;

            if (size > pBufferHdr->nAllocLen)
            {
                NvOsDebugPrintf("ERROR: OMX Buffer too small\n");
                size = pBufferHdr->nAllocLen;
            }

            pBufferHdr->nFilledLen = size;

            if (OMX_TRUE == pPort->bEmbedNvMMBuffer)
            {
                // Embed NvMMLite Buffer into OMX buffer.
                if (PayloadBuffer->Payload.Ref.hMem)
                {
                    NvU32 ucSize = size;
                    NvU32 memFd = NvRmMemGetFd(PayloadBuffer->Payload.Ref.hMem);

                    if(pData->bUseNvBuffer2)
                    {
                        if (0 <= (int)memFd)
                        {
                            // Private data:- memFd is required in case of VPR for mapping
                            // across process boundaries
                            NvOsMemcpy(pBufferHdr->pBuffer , &memFd,
                                       sizeof(NvU32));

                            NvOsMemcpy(pBufferHdr->pBuffer + sizeof(NvU32), PayloadBuffer,
                                       sizeof(NvMMBuffer));

                            // For VPR use case, copy actual size of the data in surface too
                            NvOsMemcpy(pBufferHdr->pBuffer + sizeof(NvMMBuffer) + sizeof(NvU32), &ucSize,
                                       sizeof(NvU32));

                            pBufferHdr->nFlags |= OMX_BUFFERFLAG_NV_BUFFER2;
                        }
                        else
                        {
                            NvOsDebugPrintf("ERROR: NvRmMemGetFd failed to get FD!\n");
                        }
                    }
                    else
                    {
                        NvOsMemcpy(pBufferHdr->pBuffer, PayloadBuffer,
                                   sizeof(NvMMBuffer));
                        // Private data:- memFd is required in case of VPR for mapping
                        // across process boundaries
                        NvOsMemcpy(pBufferHdr->pBuffer + sizeof(NvMMBuffer), &memFd,
                                   sizeof(NvU32));
                        // For VPR use case, copy actual size of the data in surface too
                        NvOsMemcpy(pBufferHdr->pBuffer + sizeof(NvMMBuffer) + sizeof(NvU32), &ucSize,
                                   sizeof(NvU32));
                    }

                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_NV_BUFFER;
                    pBufferHdr->nFilledLen = sizeof(NvMMBuffer);
                }
                else
                    NvOsDebugPrintf("%s: %d: EmbedNvMMBuffer requires hMem to be non-NULL\n",
                                    __FUNCTION__,__LINE__);
            }
            else if (PayloadBuffer->Payload.Ref.pMem)
            {
                /* CacheMaint if bInSharedMemory == NV_TRUE and MemoryType == WriteCombined */
                if (pPort->bInSharedMemory == NV_TRUE &&
                    PayloadBuffer->Payload.Ref.MemoryType == NvMMMemoryType_WriteCombined)
                {
                    NvRmMemCacheMaint(PayloadBuffer->Payload.Ref.hMem,
                                      (void *) PayloadBuffer->Payload.Ref.pMem,
                                      size, NV_FALSE, NV_TRUE);
                }
                NvOsMemcpy(pBufferHdr->pBuffer,
                    (void *)((OMX_U8 *)PayloadBuffer->Payload.Ref.pMem +
                             PayloadBuffer->Payload.Ref.startOfValidData),
                    size);
            }
            else if (PayloadBuffer->Payload.Ref.hMem)
            {
                NvRmMemRead(PayloadBuffer->Payload.Ref.hMem,
                    PayloadBuffer->Payload.Ref.Offset,
                    pBufferHdr->pBuffer, size);
            }

            if(NvMMBlockType_SuperParser == pData->oBlockType &&
                PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_PTSCalculationRequired)
            {
                pBufferHdr->nFlags |= OMX_BUFFERFLAG_NEED_PTS;
            }

            if (size == 0)
                bNoData = OMX_TRUE;
        }

        if (pData->nNumInputPorts > 0 &&
            pPort->nInputPortNum < TF_NUM_STREAMS &&
            pData->oPorts[pPort->nInputPortNum].pMarkQueue &&
            (!(PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag)))
        {
            CopyMarkDataFromQueue(pData->oPorts[pPort->nInputPortNum].pMarkQueue, pPortOut, pBufferHdr);
        }

        if (NvMMLiteBlockType_EncJPEG == pData->oBlockType &&
            PayloadBuffer->PayloadInfo.BufferMetaData.JpegEncMetadata.bFrameEnd)
        {
            pBufferHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
            pBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
        }

        if (PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes > 0 &&
            PayloadBuffer->PayloadInfo.BufferMetaDataType == NvMMBufferMetadataType_Audio)
        {
            NvOsMemcpy(&(pPort->audioMetaData),
                       &(PayloadBuffer->PayloadInfo.BufferMetaData.AudioMetadata),
                       sizeof(NvMMAudioMetadata));
            pPort->bAudioMDValid = OMX_TRUE;
        }

        if (pData->bAudioTransform &&
            PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes > 0)
        {
            OMX_U32 nRate, nChannels, nBits;
            NvMMAudioMetadata *audioMeta = &(PayloadBuffer->PayloadInfo.BufferMetaData.AudioMetadata);
            nRate = audioMeta->nSampleRate;
            nChannels = audioMeta->nChannels;
            nBits = audioMeta->nBitsPerSample;

            if ((pPort->bAudioMDValid &&
                (nRate != pPort->nSampleRate || nChannels != pPort->nChannels ||
                nBits != pPort->nBitsPerSample)) || !pPort->bAudioMDValid)
            {
                OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;

                pBufferHdr->nFlags |= NVX_BUFFERFLAG_CONFIGCHANGED;

                if (pLocalMode)
                {
                    pLocalMode->nSamplingRate = nRate;
                    pLocalMode->nChannels = nChannels;
                    pLocalMode->nBitPerSample = nBits;
                    NvxSendEvent(pData->pParentComp,
                        OMX_EventPortSettingsChanged,
                        pPortOut->oPortDef.nPortIndex,
                        OMX_IndexParamAudioPcm,
                        pLocalMode);
                }
            }
        }

        if (pData->bAudioTransform)
        {
            NvMMAudioMetadata *audioMeta = &(PayloadBuffer->PayloadInfo.BufferMetaData.AudioMetadata);
            pPort->nSampleRate = audioMeta->nSampleRate;
            pPort->nChannels = audioMeta->nChannels;
            pPort->nBitsPerSample = audioMeta->nBitsPerSample;
        }

        if ((NvS64)PayloadBuffer->PayloadInfo.TimeStamp >= 0)
        {
            pBufferHdr->nTimeStamp = ((OMX_TICKS)(PayloadBuffer->PayloadInfo.TimeStamp)/ 10);
            pData->LastTimeStamp = pBufferHdr->nTimeStamp;
        }
        else
        {
            pBufferHdr->nTimeStamp = -1;
        }

        if (pPort->pBuffers[id]->PayloadType == NvMMPayloadType_SurfaceArray)
        {
            pPort->pBuffers[id]->Payload.Surfaces.Empty = NV_TRUE;
        }
        else
        {
            pPort->pBuffers[id]->Payload.Ref.startOfValidData = 0;
            pPort->pBuffers[id]->Payload.Ref.sizeOfValidDataInBytes  = 0;
        }

        if (bNoData && pBufferHdr->nFlags == 0)
        {
            NV_DEBUG_PRINTF(("%s: %s[%d] TransferBufferToBlock id %d \n",
                    pData->sName, __FUNCTION__, __LINE__, id));
            pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                NvMMBufferType_Payload,
                sizeof(NvMMBuffer),
                pPort->pBuffers[id]);
            //Trigger Nvmmlite block worker function
            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        }
        else
        {
            if((NvMMLiteBlockType_EncH264 == pData->oBlockType) ||
                (NvMMLiteBlockType_EncMPEG4 == pData->oBlockType) ||
                (NvMMLiteBlockType_EncH263 == pData->oBlockType) ||
                (NvMMLiteBlockType_EncVP8 == pData->oBlockType) ||
                (NvMMLiteBlockType_EncAAC == pData->oBlockType) ||
                (NvMMLiteBlockType_SuperParser == pData->oBlockType))
            {
                if (PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag)
                {
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
                }
                if (PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_MVC)
                {
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_MVC;
                }
                if (PayloadBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame == NV_TRUE)
                {
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
                }
                if (PayloadBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.EndofFrame == NV_TRUE)
                {
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
                }
            }
            if ((NvMMLiteBlockType_EncH264 == pData->oBlockType) ||
                (NvMMLiteBlockType_EncVP8 == pData->oBlockType))
            {
                pData->LastFrameQP = PayloadBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.AvgQP;
            }
            if (pBufferHdr->pPlatformPrivate)
            {
                NvxBufferPlatformPrivate *pPrivateData = (NvxBufferPlatformPrivate *)pBufferHdr->pPlatformPrivate;
                pPrivateData->pData = (void *)pData;
            }

            pPort->pOMXBufMap[id] = pBufferHdr;
            NvxPortDeliverFullBuffer(pPortOut, pBufferHdr);
            NV_DEBUG_PRINTF(("%s: %s[%d] id %d FINAL - \n", pData->sName, __FUNCTION__, __LINE__, id));
            pPort->bFirstOutBuffer = OMX_FALSE;
            NvxPortGetNextHdr(pPortOut);

            if (pData->bVideoTransform)
            {
                if (pData->nNvMMLiteProfile & 0x10)
                {
                    pData->NumBuffersWithBlock--;
                    NvOsDebugPrintf("%s: Number of buffers with Block : %d \n",
                        pData->sName, pData->NumBuffersWithBlock);
                }
            }
            if ((!pData->bVideoTransform &&
                 !pData->bVideoProtect   &&
                  ((NvMMLiteBlockType_EncH264 == pData->oBlockType)  ||
                   (NvMMLiteBlockType_EncMPEG4 == pData->oBlockType) ||
                   (NvMMLiteBlockType_EncH263 == pData->oBlockType) ||
                   (NvMMLiteBlockType_EncVP8 == pData->oBlockType))) ||
                (!pPort->bEmbedNvMMBuffer && !pPort->bUsesANBs &&
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
                 pData->sVpp.bVppEnable != OMX_TRUE &&
#endif
                 pData->bVideoTransform && bCanReturnBufferToBlock))
            {
                // Returning NvMMLite buffer back to block
                // as it is completly copied to application buffer.
                NvMMBuffer *SendBackBuf = pPort->pBuffers[id];
                pPort->pOMXBufMap[id] = NULL;
                pPort->bBufNeedsFlush[id] = OMX_FALSE;
                NV_DEBUG_PRINTF(("%s: %s[%d] TransferBufferToBlock \n",
                    pData->sName, __FUNCTION__, __LINE__));
                pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                    NvMMBufferType_Payload,
                    sizeof(NvMMBuffer), SendBackBuf);
                //Trigger Nvmmlite block worker function
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                if (pData->nNvMMLiteProfile & 0x10)
                {
                    pData->NumBuffersWithBlock++;
                }
            }
        }
    }
done:
    return eError;
}

NvError
NvxNvMMLiteTransformDeliverFullOutput(
                                  void *pContext,
                                  NvU32 streamIndex,
                                  NvMMBufferType BufferType,
                                  NvU32 BufferSize,
                                  void *pBuffer)
{
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)pContext;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvxLiteOutputBuffer OutBuf;
    NvError eError = NvSuccess;
    if (BufferType == NvMMBufferType_StreamEvent)
    {
        NvMMLiteEventType eventType = ((NvMMLiteStreamEventBase *)pBuffer)->event;
        HandleStreamEvent(pData, streamIndex, eventType, pBuffer);
    }

    // queue is threadsafe.
    if ((pPort->bEOS && !pPort->bSentEOS && !pPort->bError) ||
        ((BufferType == NvMMBufferType_Payload) && (!pPort->bEOS && !pPort->bAborting)))
    {
        OutBuf.streamIndex = streamIndex;
        // can we copy it to some other structure.
        if (pBuffer && (BufferType == NvMMBufferType_Payload))
        {
            OutBuf.Buffer = *((NvMMBuffer *)pBuffer);
            if (pPort->pBuffers[OutBuf.Buffer.BufferID])
                *(pPort->pBuffers[OutBuf.Buffer.BufferID]) = *((NvMMBuffer *)pBuffer);
        }
        else
        {
            NvOsMemset(&OutBuf.Buffer, 0, sizeof(OutBuf.Buffer));
            OutBuf.Buffer.BufferID = EMPTY_BUFFER_ID;
        }
        NV_DEBUG_PRINTF(("%s: %s[%d] NvMMQueueEnQ count %d id %d time %.3f\n",
            pData->sName, __FUNCTION__, __LINE__,
            NvMMQueueGetNumEntries(pData->pOutputQ[pPort->nOutputQueueIndex]),
            OutBuf.Buffer.BufferID,
            OutBuf.Buffer.PayloadInfo.TimeStamp / 10000000.0));

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        if(!pData->bThumbnailMode && pData->sVpp.bVppEnable == OMX_TRUE && (BufferType == NvMMBufferType_Payload))
        {
            NvMMQueueEnQ(pData->sVpp.pReleaseOutSurfaceQueue, &pPort->pBuffers[OutBuf.Buffer.BufferID], 10);
            if (pData->sVpp.bVppStop == OMX_TRUE)
                pData->sVpp.bVppStop = OMX_FALSE;
            NvOsSemaphoreSignal(pData->sVpp.VppInpOutAvail);
        } else
#endif
        {
            eError = NvMMQueueEnQ(pData->pOutputQ[pPort->nOutputQueueIndex], &OutBuf, 10);
            if (NvSuccess != eError)
            {
                NvOsDebugPrintf("ERROR: buffer enqueue failed line %d 0x%x \n", __LINE__, eError);
                return eError;
            }
        }

#ifndef BUILD_GOOGLETV
        //Trigger worker function to process buffer queue.
        NvxWorkerTrigger(&( pPortOut->pNvComp->oWorkerData));
    }
#else
        if (!pPort->bPortSettingChanged)
        {
            //Trigger worker function to process buffer queue.
            NvxWorkerTrigger(&( pPortOut->pNvComp->oWorkerData));
        }
    }
    else if (pPort->bAborting && pPort->bPortSettingChanged)
    {
        NvxMutexLock(pPort->hPreviousBuffersMutex);
        if (NvxListGetNumEntries(pPort->pPreviousBuffersList))
        {
            NvxListItem *cur = pPort->pPreviousBuffersList->pHead;
            while(cur)
            {
                NvMMBuffer *pNvMMBuffer1 = cur->pElement;
                NvMMSurfaceDescriptor *pSurfaces1 = &(pNvMMBuffer1->Payload.Surfaces);
                NvMMBuffer *pNvMMBuffer2 = (NvMMBuffer *)pBuffer;
                NvMMSurfaceDescriptor *pSurfaces2 = &(pNvMMBuffer2->Payload.Surfaces);

                if (((OMX_PTR)pSurfaces1->PhysicalAddress[0]) == ((OMX_PTR)pSurfaces2->PhysicalAddress[0]))
                {
                    NvxListRemoveEntry(pPort->pPreviousBuffersList, pNvMMBuffer1);
                    NvxVideoDecSurfaceFree(pNvMMBuffer1);
                    NvOsFree(pNvMMBuffer1);
                    break;
                }
                cur = cur->pNext;
            }
        }
        else
        {
            NvxMutexUnlock(pPort->hPreviousBuffersMutex);
            return NvError_BadParameter;
        }
#if 0
        while(NvMMQueueGetNumEntries(pData->pOutputQ[pPort->nOutputQueueIndex]))
        {
            // Free all buffers from the queue.
            NvxListItem *cur = pPort->pPreviousBuffersList->pHead;
            NvxLiteOutputBuffer OutBuf;
            NvMMQueueDeQ(pData->pOutputQ[streamIndex], &OutBuf);
            while(cur)
            {
                NvMMBuffer *pNvMMBuffer1 = cur->pElement;
                NvMMSurfaceDescriptor *pSurfaces1 = &(pNvMMBuffer1->Payload.Surfaces);
                NvMMBuffer *pNvMMBuffer2 = &OutBuf.Buffer;
                NvMMSurfaceDescriptor *pSurfaces2 = &(pNvMMBuffer2->Payload.Surfaces);

                if (((OMX_PTR)pSurfaces1->PhysicalAddress[0]) == ((OMX_PTR)pSurfaces2->PhysicalAddress[0]))
                {
                    NvxListRemoveEntry(pPort->pPreviousBuffersList, pNvMMBuffer1);
                    NvxVideoDecSurfaceFree(pNvMMBuffer1);
                    NvOsFree(pNvMMBuffer1);
                }
                cur = cur->pNext;
            }
        }
#endif
        NvxMutexUnlock(pPort->hPreviousBuffersMutex);
    }
#endif

    return NvSuccess;
}

OMX_ERRORTYPE NvxNvMMLiteTransformFreeBuffer(SNvxNvMMLiteTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream)
{
    int i = 0;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[nStream]);

    if (!pBuffer || !pPort->bUsesANBs)
        return OMX_ErrorNone;
#if USE_ANDROID_NATIVE_WINDOW
    if (OMX_ErrorNone != FreeANBLite(pData->pParentComp, pData,
                                     nStream, pBuffer))
    {
        return OMX_ErrorNone;
    }
#endif

    for (; i < MAX_NVMMLITE_BUFFERS; i++)
    {
        if (pPort->pOMXBufMap[i] == pBuffer)
        {
            pPort->pOMXBufMap[i] = NULL;
            pPort->bBufNeedsFlush[i] = OMX_FALSE;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMLiteTransformFillThisBuffer(SNvxNvMMLiteTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream)
{
    OMX_BOOL bFound = OMX_FALSE;
    NvBool bTriggerRequired = NV_FALSE;
    int i = 0;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[nStream]);
    NvxBufferPlatformPrivate *pPrivateData = NULL;

    if (!pBuffer)
        return OMX_ErrorNone;

    pPrivateData = (NvxBufferPlatformPrivate *)pBuffer->pPlatformPrivate;

    bTriggerRequired = (NvMMQueueGetNumEntries(pData->pOutputQ[pPort->nOutputQueueIndex]) > 0);

    if ((!pPrivateData ||
        ((pPrivateData->eType != NVX_BUFFERTYPE_NEEDRMSURFACE) &&
        (pPrivateData->eType != NVX_BUFFERTYPE_HASRMSURFACE))) &&
        (!pPort->bUsesANBs))
    {
        if (pPort->pOMXPort && pData->bVideoTransform &&
            !pPort->bEmbedNvMMBuffer && !pPort->bUsesRmSurfaces)
        {
            bFound = OMX_TRUE;
            NV_DEBUG_PRINTF(("%s: %s[%d] exit \n", pData->sName,
                __FUNCTION__, __LINE__));
            goto Exit;
        }
    }

    if (!pData->bVideoTransform && !pPort->bEmbedNvMMBuffer &&
        ((NvMMLiteBlockType_EncH264 == pData->oBlockType)  ||
         (NvMMLiteBlockType_EncMPEG4 == pData->oBlockType) ||
         (NvMMLiteBlockType_EncH263 == pData->oBlockType) ||
         (NvMMLiteBlockType_EncVP8 == pData->oBlockType)))
    {
        if (pPort->pOMXPort)
        {
            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
        }
        NV_DEBUG_PRINTF(("%s: %s[%d] exit \n", pData->sName,
                __FUNCTION__, __LINE__));
        return OMX_ErrorNone;
    }

    if (pPort->bUsesANBs && pData->bVideoTransform)
    {
        if (!pData->VideoStreamInfo.NumOfSurfaces || pPort->bBlockANBImport)
        {
            bFound = OMX_TRUE;
            NV_DEBUG_PRINTF(("%s: %s[%d] exit \n", pData->sName,
                __FUNCTION__, __LINE__));
            goto Exit;
        }

#if USE_ANDROID_NATIVE_WINDOW
        if (OMX_ErrorNone != ImportANBLite(pData->pParentComp, pData,
                                           nStream, pBuffer, 0, 0))
        {
            NV_DEBUG_PRINTF(("%s: %s[%d] exit \n", pData->sName,
                __FUNCTION__, __LINE__));
            goto Exit;
        }
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        if((!pData->bThumbnailMode) && (pData->sVpp.bVppEnable) && (pData->sVpp.bVppThreadActive))
        {
            NvMMBuffer *nvmmbuf = NULL;
            nvmmbuf = GetFreeNvMMSurface(&(pData->sVpp));
            if(nvmmbuf)
            {
                pData->TransferBufferToBlock(pData->hBlock, nStream,
                                     NvMMBufferType_Payload, sizeof(NvMMBuffer),
                                     nvmmbuf);
            }
        }
#endif
#endif

        for (; i < MAX_NVMMLITE_BUFFERS; i++)
        {
            if (pPort->pOMXBufMap[i] == pBuffer)
            {
                pPort->pOMXBufMap[i] = NULL;
                pPort->bBufNeedsFlush[i] = OMX_FALSE;
            }
        }
        bFound = OMX_TRUE;
        NV_DEBUG_PRINTF(("%s: %s[%d] exit \n", pData->sName,
            __FUNCTION__, __LINE__));
        goto Exit;
    }

#ifdef BUILD_GOOGLETV
    //Free the buffers allocated before port setting change.
    if (pData->bVideoTransform && (!pPort->bUsesANBs) && pPort->bUsesRmSurfaces)
    {
        NvxMutexLock(pPort->hPreviousBuffersMutex);
        if (NvxListGetNumEntries(pPort->pPreviousBuffersList))
        {
            NvxListItem *cur = pPort->pPreviousBuffersList->pHead;
            while(cur)
            {
                //Cannot compare on a buffer ID, those are of the new buffers.
                NvMMBuffer *pNvMMBuffer1 = cur->pElement;
                NvMMSurfaceDescriptor *pSurfaces1 = &(pNvMMBuffer1->Payload.Surfaces);
                NvMMBuffer *pNvMMBuffer2 = pBuffer->pBuffer;
                NvMMSurfaceDescriptor *pSurfaces2 = &(pNvMMBuffer2->Payload.Surfaces);

                if (((OMX_PTR)pSurfaces1->PhysicalAddress[0]) == ((OMX_PTR)pSurfaces2->PhysicalAddress[0]))
                {
                    NvxListRemoveEntry(pPort->pPreviousBuffersList, pNvMMBuffer1);
                    NvxVideoDecSurfaceFree(pNvMMBuffer1);
                    NvOsFree(pNvMMBuffer1);
                    NvxMutexUnlock(pPort->hPreviousBuffersMutex);
                    return OMX_ErrorNone;
                }
                cur = cur->pNext;
            }
        }
        NvxMutexUnlock(pPort->hPreviousBuffersMutex);
    }
#endif

    for (; i < MAX_NVMMLITE_BUFFERS; i++)
    {
        if (pPort->pOMXBufMap[i] == pBuffer || pPort->bBufNeedsFlush[i])
        {
            NvMMBuffer *SendBackBuf = pPort->pBuffers[i];
            if (SendBackBuf == NULL)
            {
                NvOsDebugPrintf("Got NULL NvMMLite buffer in NvxNvMMLiteTransformFillThisBuffer\n");
                bFound = OMX_TRUE;
                goto Exit;
            }
            if(pBuffer->nFlags & OMX_BUFFERFLAG_NV_BUFFER)
            {
                // we have an NvMMLite Buffer embedded into OMXBuffer.
                if (pBuffer->nFilledLen == sizeof(NvMMBuffer))
                {
                    NvMMBuffer Embedbuf;
                    NvOsMemcpy(&Embedbuf, pBuffer->pBuffer, sizeof(NvMMBuffer));

                    // swap the payload
                    if (SendBackBuf->PayloadType == NvMMPayloadType_SurfaceArray)
                        SendBackBuf->Payload.Surfaces = Embedbuf.Payload.Surfaces;
                    else
                        SendBackBuf->Payload.Ref = Embedbuf.Payload.Ref;
                    SendBackBuf->PayloadInfo = Embedbuf.PayloadInfo;
                }
            }
            pPort->pOMXBufMap[i] = NULL;
            pPort->bBufNeedsFlush[i] = OMX_FALSE;
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
            if ((!pPort->bUsesANBs) && (pData->sVpp.bVppEnable == OMX_TRUE) && (!pData->bThumbnailMode))
            {
                pData->sVpp.Renderstatus[i] = FRAME_FREE;
                NvOsSemaphoreSignal(pData->sVpp.VppInpOutAvail);
            }
            else
#endif
                pData->TransferBufferToBlock(pData->hBlock, nStream,
                    NvMMBufferType_Payload,
                    sizeof(NvMMBuffer), SendBackBuf);
            //Trigger Nvmmlite block worker function
            if (pPort->pOMXPort)
                NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));

            bFound = OMX_TRUE;
            if (pData->bVideoTransform)
            {
                if (pData->nNvMMLiteProfile & 0x10)
                {
                    pData->NumBuffersWithBlock++;
                }
            }
            NV_DEBUG_PRINTF(("%s: %s[%d] exit \n", pData->sName,
                __FUNCTION__, __LINE__));
            break;
        }
    }

Exit:
    if (((bFound && (pData->bHasInputRunning || !pPort->bCanSkipWorker)) ||
        (bTriggerRequired)) && (pPort->pOMXPort))
    {
        NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
    }

    return (bFound) ? OMX_ErrorNone : OMX_ErrorBadParameter;
}

void NvxNvMMLiteTransformPortEventHandler(SNvxNvMMLiteTransformData *pData,
                                      int nPort, OMX_U32 uEventType)
{
    NvU32 i;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[nPort]);
    NvxComponent *pComp = pData->pParentComp;

    pPort->bBlockANBImport = OMX_FALSE;

    if(pData->bReconfigurePortBuffers == OMX_TRUE && uEventType == 1) //(uEventType == 1)-> Port Enable
    {
        NvxNvMMLiteTransformReconfigurePortBuffers(pData);
    }

    if(pData->bReconfigurePortBuffers == OMX_TRUE && uEventType == 0) //(uEventType == 0)-> Port Disable
    {
        pData->bStopping = OMX_TRUE;
        NvxMutexUnlock(pComp->hWorkerMutex);
        pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Stopped);
        NvxMutexLock(pComp->hWorkerMutex);
        pData->bStopping = OMX_FALSE;

        pPort->bAborting = OMX_TRUE;
        NvxMutexUnlock(pComp->hWorkerMutex);
        pData->hBlock->AbortBuffers(pData->hBlock, nPort);
        NvxMutexLock(pComp->hWorkerMutex);
        if (pPort->nType == TF_TYPE_OUTPUT)
        {
            FlushOutputQueue(pData, pPort);
        }

        pPort->bAborting = OMX_FALSE;
        if (pData->bVideoTransform)
        {
            NvU32 data = 1;

            NvxMutexUnlock(pComp->hWorkerMutex);
            pData->hBlock->SetAttribute(pData->hBlock,
                NvMMLiteVideoDecAttribute_SearchForHeaderData,
                NvMMLiteSetAttrFlag_Notification,
                sizeof (NvU32),
                &data);
            NvxMutexLock(pComp->hWorkerMutex);
        }

        if(!pPort->bUsesANBs)
            NvxNvMMLiteTransformFreePortBuffers(pData, nPort);

        for (i = 0; i < MAX_NVMMLITE_BUFFERS; i++)
        {
            if (pPort->pOMXBufMap[i])
            {
                pPort->pOMXBufMap[i] = NULL;
            }
        }

        NvxMutexUnlock(pComp->hWorkerMutex);
        pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Running);
        NvxMutexLock(pComp->hWorkerMutex);
        return;
    }

    if (uEventType && pData->bVideoTransform && pPort->nType == TF_TYPE_OUTPUT)
    {
        if (pData->bPausing)
            pData->hBlock->SetState(pData->hBlock, NvMMLiteState_Running);
        pData->bPausing = OMX_FALSE;
        return;
    }

    if (uEventType || !pData->bVideoTransform || pPort->nType != TF_TYPE_OUTPUT)
        return;

    // if we're not tunneling, and this is the first buffer, skip all this
    // flushing/disabling/etc
    if (!pPort->bUsesRmSurfaces && pPort->bFirstOutBuffer)
    {
        return;
    }

}

static NvError
NvxNvMMLiteTransformSendBlockEventFunction(
                                       void *pContext,
                                       NvU32 eventType,
                                       NvU32 eventSize,
                                       void *eventInfo)
{
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)pContext;

    if (pData->EventCallback)
        pData->EventCallback(pData->pEventData, eventType,
        eventSize, eventInfo);

    switch (eventType)
    {
    case NvMMLiteEvent_Unknown:
        break;

    case NvMMLiteEvent_BlockClose:
        NVMMLITEVERBOSE(("%s: BlockCloseDone\n", pData->sName));
        pData->blockCloseDone = OMX_TRUE;
        break;

    case NvMMLiteEvent_BlockWarning:
        NVMMLITEVERBOSE(("%s: NvMMLiteEventCamera_\n", pData->sName));
        if (pData->pParentComp)
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMLiteBlockWarningInfo *pBlockWarningInfo = (NvMMLiteBlockWarningInfo*)eventInfo;
            NvxSendEvent(pNvComp, NVX_EventBlockWarning,
                         pBlockWarningInfo->warning, 0,
                         (OMX_PTR)pBlockWarningInfo->additionalInfo);
        }
        break;
    case NvMMLiteEvent_ForBuffering:
        {
            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                NvMMLiteEventForBuffering *pBlockBufferingInfo = (NvMMLiteEventForBuffering*)eventInfo;
                NvxSendEvent(pNvComp, NVX_EventForBuffering,
                             (NvU32)pBlockBufferingInfo->ChangeStateToPause,
                             pBlockBufferingInfo->BufferingPercent,
                             NULL);
            }
        }
        break;
    case NvMMLiteEvent_SetAttributeError:
        {
            NvMMLiteSetAttributeErrorInfo *pBlockErr = (NvMMLiteSetAttributeErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;

            NVMMLITEVERBOSE(("SetAttributeErrorEvent\n"));

            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                eReturnVal = (pBlockErr) ? NvxTranslateErrorCode(pBlockErr->error) : OMX_ErrorUndefined;
                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
            }
        }
        break;
    case NvMMLiteEvent_BlockError:
        {
            NvMMLiteBlockErrorInfo *pBlockErr = (NvMMLiteBlockErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;
            int streamId = 0;

            NVMMLITEVERBOSE(("StreamBlockErrorEvent\n"));

            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                if(pBlockErr)
                {
                    eReturnVal = NvxTranslateErrorCode(pBlockErr->error);
                    NvOsDebugPrintf("Event_BlockError from %s : Error code - %x\n", pData->sName, pBlockErr->error);
                }
                else
                {
                    eReturnVal = OMX_ErrorUndefined;
                    NvOsDebugPrintf("Event_BlockError from %s : Error information not available\n", pData->sName);
                }

                if(pData->blockErrorEncountered == OMX_FALSE)
                {
                    pData->blockErrorEncountered = OMX_TRUE;
                    NvOsDebugPrintf("Sending error event from %s",pData->sName);
                    NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);

                    for (streamId = 0; streamId < (int)pData->nNumStreams;
                        streamId++)
                    {
                        pData->oPorts[streamId].bError = OMX_TRUE;
                    }
                }
                else
                {
                    NvOsDebugPrintf("Blocking error event from %s",pData->sName);
                }
            }
        }
        break;

    case NvMMLiteEvent_DoMoreWork:
        {
            NVMMLITEVERBOSE(("NvMMLiteEvent_DoMoreWork \n"));
            if (pData->pParentComp)
                NvxWorkerTrigger(&(pData->pParentComp->oWorkerData));
            break;
        }

    default:
        break;
    }
    return NvSuccess;
}

static NvError NvxVideoSurfaceAlloc(NvRmDeviceHandle hRmDev,
                    NvMMBuffer *pMMBuffer, NvU32 BufferID, NvU32 Width,
                    NvU32 Height, NvU32 ColourFormat, NvU32 Layout,
                    NvU32 *pImageSize, NvBool UseAlignment,
                    NvOsMemAttribute Coherency, NvBool UseNV21)
{
    NvError err;

    NvxSurface *pSurface = (NvxSurface *)(&pMMBuffer->Payload.Surfaces);

    if (ColourFormat == NvMMVideoDecColorFormat_YUV420)
    {
        err = NvxSurfaceAllocContiguousYuv(
            pSurface,
            Width,
            Height,
            NvMMVideoDecColorFormat_YUV420,
            Layout,
            pImageSize,
            UseAlignment, Coherency,
            NV_TRUE);
    }
    else
    {
        err = NvxSurfaceAllocYuv(
            pSurface,
            Width,
            Height,
            ColourFormat,
            Layout,
            pImageSize,
            UseAlignment, Coherency, UseNV21);
    }

    pMMBuffer->StructSize = sizeof(NvMMBuffer);
    pMMBuffer->BufferID = BufferID;
    pMMBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
    pMMBuffer->Payload.Surfaces.fences = NULL;

    NvOsMemset(&pMMBuffer->PayloadInfo, 0, sizeof(pMMBuffer->PayloadInfo));

    return err;
}

static void NvxVideoDecSurfaceFree(NvMMBuffer *pMMBuffer)
{
    NvxSurface *pSurface = (NvxSurface *)(&pMMBuffer->Payload.Surfaces);
    if (pSurface)
    {
        int i=0;
        for (i=0;i<pSurface->SurfaceCount;i++)
        {
            if ((3 == pSurface->SurfaceCount) &&
                (pSurface->Surfaces[0].hMem == pSurface->Surfaces[1].hMem))
            {
                //single plane allocation
                NvxMemoryFree(&pSurface->Surfaces[0].hMem);
                pSurface->Surfaces[0].hMem = NULL;
                pSurface->Surfaces[1].hMem = NULL;
                pSurface->Surfaces[2].hMem = NULL;
                break;
            }
            if (pSurface->Surfaces[i].hMem != NULL)
            {
                NvxMemoryFree(&pSurface->Surfaces[i].hMem);
                pSurface->Surfaces[i].hMem = NULL;
            }
        }
    }
    return;
}

static void FlushOutputQueue(SNvxNvMMLiteTransformData *pData, SNvxNvMMLitePort *pPort)
{
    NvU32 streamIndex = pPort->nOutputQueueIndex;
    while (NvMMQueueGetNumEntries(pData->pOutputQ[streamIndex]))
    {
        // drop all buffers from the queue.
        NvxLiteOutputBuffer OutBuf;
        NvMMQueueDeQ(pData->pOutputQ[streamIndex], &OutBuf);
        if (pData->bFlushing)
            pPort->bBufNeedsFlush[OutBuf.Buffer.BufferID] = OMX_TRUE;
        NV_DEBUG_PRINTF(("%s: %s[%d] dropping id %d time %.3f \n",
            pData->sName, __FUNCTION__, __LINE__,
            OutBuf.Buffer.BufferID,
            OutBuf.Buffer.PayloadInfo.TimeStamp / 10000000.0));
    }
}

#ifdef BUILD_GOOGLETV
static NvError NvxNvMMLiteTransformCreatePreviousBuffList(SNvxNvMMLitePort *pPort)
{
    NvU32 i = 0;

    NvxMutexLock(pPort->hPreviousBuffersMutex);

    for (i = 0; i < pPort->nNumPreviousBuffs; i++)
    {
        NvxListEnQ(pPort->pPreviousBuffersList, pPort->pBuffers[i], 10);
    }

    NvxMutexUnlock(pPort->hPreviousBuffersMutex);

    return NvSuccess;
}
#endif

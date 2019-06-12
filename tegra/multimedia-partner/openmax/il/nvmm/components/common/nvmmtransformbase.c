/*
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <nvassert.h>

#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvrm_surface.h"
#include "nvrm_hardware_access.h"
#include "nvmmtransformbase.h"
#include "nvmm_videodec.h"
#include "nvmm_videoenc.h"
#include "NvxHelpers.h"
#include "nvmm_camera.h"
#include "nvmm_digitalzoom.h"
#include "nvmm_parser.h"
#include "nvxeglimage.h"
#include "NvxTrace.h"
#include "nvmm/components/nvxcamera.h"
#if USE_ANDROID_NATIVE_WINDOW
#include "nvxandroidbuffer.h"
#include "nvgr.h"
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

static NvError NvxNvMMTransformReturnEmptyInput(void *pContext, NvU32 streamIndex,
                                                NvMMBufferType BufferType, NvU32 BufferSize, void *pBuffer);
NvError NvxNvMMTransformDeliverFullOutput(void *pContext, NvU32 streamIndex,
                                                 NvMMBufferType BufferType, NvU32 BufferSize, void *pBuffer);
static NvError NvxNvMMTransformDeliverFullOutputInternal(SNvxNvMMTransformData *pData,
                                                NvMMBuffer* output,
                                                NvU32 streamIndex);
static NvError NvxNvMMTransformSendBlockEventFunction(void *pContext, NvU32 eventType,
                                                      NvU32 eventSize, void *eventInfo);
static void NvxVideoDecSurfaceFree(NvMMBuffer *pMMBuffer);
static NvError NvxVideoSurfaceAlloc(NvRmDeviceHandle hRmDev,
                    NvMMBuffer *pMMBuffer, NvU32 BufferID, NvU32 Width,
                    NvU32 Height, NvU32 ColourFormat, NvU32 Layout,
                    NvU32 *pImageSize, NvBool UseAlignment,
                    NvOsMemAttribute Coherency,
                    NvBool ClearYUVsurface);

/*deinterlace related functions */
static void NvxDeinterlaceThread(void* arg);
static NvError TVMRInit(SNvxNvMMTransformData *pData);
static void TVMRClose(SNvxNvMMTransformData *pData);
static TVMRVideoSurface * GetTVMRVideoSurface(TVMRCtx *pTVMR, const NvMMBuffer* nvmmbuf);
static NvMMBuffer* GetFreeNvMMSurface(TVMRCtx *pTVMR);
static NvU32 CheckFreeBuffers(TVMRCtx *pTVMR);
static NvError Deinterlace(SNvxNvMMTransformData *pData, NvU64 delta);
/*deinterlace related functions */

//FIXME: Goes away once block names don't have to be unique
static int s_nBlockCount = 0;

OMX_ERRORTYPE NvxNvMMTransformSetProfile(SNvxNvMMTransformData *pData, NVX_CONFIG_PROFILE *pProf)
{
    pData->bProfile = pProf->bProfile;
    pData->bVerbose = pProf->bVerbose;
    pData->bDiscardOutput = pProf->bStubOutput;
    pData->nForceLocale = pProf->nForceLocale;
    pData->nNvMMProfile = pProf->nNvMMProfile;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetFileName(SNvxNvMMTransformData *pData, NVX_PARAM_FILENAME *pProf)
{
    pData->fileName = pProf->pFilename;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetUlpMode(SNvxNvMMTransformData *pData, NVX_CONFIG_ULPMODE *pProf)
{
    pData->enableUlpMode = pProf->enableUlpMode;
    pData->ulpkpiMode = pProf->kpiMode;

#if 0
    /*FIXME reenable once nvmap errors with metabuffers are resolved */
    if (pData->hBlock)
    {
        NvMMAttrib_EnableUlpMode oUlpInfo;

        NvOsMemset(&oUlpInfo, 0, sizeof(NvMMAttrib_EnableUlpMode));

        oUlpInfo.bEnableUlpMode = pData->enableUlpMode;
        oUlpInfo.ulpKpiMode = pData->ulpkpiMode;

        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMAttribute_UlpMode,
            NvMMSetAttrFlag_Notification,
            sizeof(NvMMAttrib_EnableUlpMode),
            &oUlpInfo);

        //Wait for the file attribute set acknowledgement
        NvOsSemaphoreWait (pData->SetAttrDoneSema);
    }
#endif

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetFileCacheSize(SNvxNvMMTransformData *pData, OMX_U32 nSize)
{
    pData->nFileCacheSize = nSize;
    pData->bSetFileCache = OMX_TRUE;

    if (pData->hBlock)
    {
        NvMMAttrib_FileCacheSize oFCSize;

        NvOsMemset(&oFCSize, 0, sizeof(NvMMAttrib_FileCacheSize));

        oFCSize.nFileCacheSize = nSize;

        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMAttribute_FileCacheSize,
            NvMMSetAttrFlag_Notification,
            sizeof(NvMMAttrib_FileCacheSize),
            &oFCSize);

        //Wait for the file attribute set acknowledgement
        NvOsSemaphoreWait(pData->SetAttrDoneSema);
    }

    return OMX_ErrorNone;
}

typedef struct _BufferMarkData {
    OMX_HANDLETYPE hMarkTargetComponent;
    OMX_PTR pMarkData;
    OMX_TICKS nTimeStamp;
    OMX_U32 nFlags;
    OMX_PTR pMetaDataSource;
} BufferMarkData;

static OMX_ERRORTYPE NvxNvMMSendNextInputBufferBack(SNvxNvMMTransformData *pData, int nNumStream)
{
    NvMMBuffer *pNvxBuffer = NULL;
    SNvxNvMMPort *pPort;

    pPort = &pData->oPorts[nNumStream];

    pNvxBuffer = pPort->pBuffers[pPort->nCurrentBuffer];
    pNvxBuffer->BufferID = pPort->nCurrentBuffer;

    if (pNvxBuffer->PayloadType != NvMMPayloadType_SurfaceArray)
    {
        pNvxBuffer->Payload.Ref.startOfValidData = 0;
        pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    }

    pPort->nCurrentBuffer++;
    pPort->nCurrentBuffer = pPort->nCurrentBuffer % pPort->nBufsTot;
    pPort->nBufsInUse++;

    pData->TransferBufferToBlock(pData->hBlock, nNumStream,
        NvMMBufferType_Payload,
        sizeof(NvMMBuffer), pNvxBuffer);

    return OMX_ErrorNone;
}

#if USE_ANDROID_NATIVE_WINDOW
  static OMX_U32 U32_AT(const uint8_t *ptr) {
       return ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];
}
#endif

static OMX_ERRORTYPE NvxNvMMProcessInputBuffer(SNvxNvMMTransformData *pData, int nNumStream, OMX_BOOL *pbNextWouldFail)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMBuffer *pNvxBuffer = NULL;
    OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
    SNvxNvMMPort *pPort;
    NvU32 nWorkingBuffer = 0;
    NvxBufferPlatformPrivate *pPrivateData = NULL;
    OMX_BOOL bSentData = OMX_FALSE;

    pPort = &pData->oPorts[nNumStream];

    if (pPort->nType != TF_TYPE_INPUT || pPort->nBufsInUse >= pPort->nBufsTot)
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
        pNvxBuffer = pPort->pBuffers[pPort->nCurrentBuffer];

        if (pPort->pNvMMBufMap[pPort->nCurrentBuffer])
        {
             pPort->pNvMMBufMap[pPort->nCurrentBuffer] = NULL;
        }

        pNvxBuffer->BufferID = pPort->nCurrentBuffer;
        if (pOMXBuf->nTimeStamp >= 0)
        {
            pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
        }
        else
        {
            pNvxBuffer->PayloadInfo.TimeStamp = -1;
        }
        pNvxBuffer->PayloadInfo.BufferFlags = 0;
        pPort->pOMXBufMap[pPort->nCurrentBuffer] = pOMXBuf;
        nWorkingBuffer = pPort->nCurrentBuffer;
        pPort->nCurrentBuffer++;
        pPort->nCurrentBuffer = pPort->nCurrentBuffer % pPort->nBufsTot;
        pPort->nBufsInUse++;
        if (pbNextWouldFail)
            *pbNextWouldFail = (pPort->nBufsInUse >= pPort->nBufsTot);
        if (!pPort->bSkipWorker)
            NvxMutexUnlock(pPort->oMutex);

        if (pPrivateData->eType == NVX_BUFFERTYPE_EGLIMAGE)
        {
            // Stretch Blit EGL source to NvMM surface
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

            CopyANBToNvxSurface(pData->pParentComp,
                                handle,
                                &(pNvxBuffer->Payload.Surfaces));
        }
#endif
        else if (pPort->bUsesAndroidMetaDataBuffers)
        {

#if USE_ANDROID_NATIVE_WINDOW
            if (pOMXBuf->nFilledLen <= 4)
            {
                return OMX_ErrorStreamCorrupt;
            }

            OMX_U32 nMetadataBufferFormat = U32_AT(pOMXBuf->pBuffer);
            switch (nMetadataBufferFormat) {
                case kMetadataBufferTypeCameraSource: {
                    if (pOMXBuf->nFilledLen != 4 + sizeof(OMX_BUFFERHEADERTYPE))
                    {
                        return OMX_ErrorStreamCorrupt;
                    }

                    if (pPort->bEmbedNvMMBuffer == OMX_FALSE &&
                        pData->oBlockType == NvMMBlockType_EncJPEG)
                    {
                        // Surfaces are provided by camera source free
                        // Omx allocated & set embedded buffer flag.
                         NvU32 i;
                        for (i = 0; i < pPort->nBufsTot; i++)
                            NvxVideoDecSurfaceFree(pPort->pBuffers[i]);
                        pPort->bEmbedNvMMBuffer = OMX_TRUE;
                    }

                    OMX_BUFFERHEADERTYPE *pEmbeddedOMXBuffer = NULL;
                    NvMMBuffer *pEmbeddedNvmmBuffer = NULL;

                    pEmbeddedOMXBuffer = (OMX_BUFFERHEADERTYPE*)(pOMXBuf->pBuffer + 4);
                    if (!pEmbeddedOMXBuffer)
                        return OMX_ErrorStreamCorrupt;

                    pEmbeddedNvmmBuffer = (NvMMBuffer*)(pEmbeddedOMXBuffer->pBuffer);
                    if (!pEmbeddedNvmmBuffer)
                        return OMX_ErrorStreamCorrupt;

                    // swap the surface
                    pNvxBuffer->Payload.Surfaces =
                        pEmbeddedNvmmBuffer->Payload.Surfaces;
                    pNvxBuffer->PayloadInfo = pEmbeddedNvmmBuffer->PayloadInfo;

                    pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
                } break;
                case kMetadataBufferTypeGrallocSource: {
                    buffer_handle_t *pEmbeddedGrallocSrc = (buffer_handle_t *)(pOMXBuf->pBuffer+ 4);

                    CopyANBToNvxSurface(pData->pParentComp,
                    *pEmbeddedGrallocSrc,
                    &(pNvxBuffer->Payload.Surfaces));

                    pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
                } break;
                default:
                    return OMX_ErrorStreamCorrupt;
            }
#else
            // we have an OMXBuffer embedded into OMXBuffer.
            if(pOMXBuf->nFilledLen == sizeof(OMX_BUFFERHEADERTYPE))
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
                if (pOMXBuf->nTimeStamp >= 0)
                {
                    pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
                }
                else
                {
                    pNvxBuffer->PayloadInfo.TimeStamp = -1;
                }
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
                // we have NvMM Buffer embedded into OMXBuffer.
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

                        NvxCopyMMSurfaceDescriptor(&pNvxBuffer->Payload.Surfaces,
                            DestRect,
                            &Embeddedbuf.Payload.Surfaces, SrcRect);
                    }
                    else
                    {
                        // swap the surface
                        pNvxBuffer->Payload.Surfaces = Embeddedbuf.Payload.Surfaces;
                        pNvxBuffer->PayloadInfo = Embeddedbuf.PayloadInfo;
                        if (pOMXBuf->nFlags & OMX_BUFFERFLAG_RETAIN_OMX_TS) {
                            pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
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
                    pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
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

            if ( size > pNvxBuffer->Payload.Ref.sizeOfBufferInBytes )
            {
                NvError error;

                if (pPort->pBuffers[nWorkingBuffer])
                {
                    if (pPort->bNvMMClientAlloc)
                        NvMMUtilDeallocateBuffer(pPort->pBuffers[nWorkingBuffer]);
                }

                /* WriteCombined allocs for both Audio and Video input buffers if bInSharedMemory == NV_TRUE */
                if ((pPort->bInSharedMemory == NV_TRUE) && (pData->bVideoTransform || pData->bAudioTransform))
                    pPort->bufReq.memorySpace = NvMMMemoryType_WriteCombined;

                error = NvMMUtilAllocateBuffer(pData->hRmDevice,
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
                if (pOMXBuf->nTimeStamp >= 0)
                {
                    pNvxBuffer->PayloadInfo.TimeStamp = pOMXBuf->nTimeStamp * 10;
                }
                else
                {
                    pNvxBuffer->PayloadInfo.TimeStamp = -1;
                }
            }
            pNvxBuffer->Payload.Ref.startOfValidData = 0;
            pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes = size;

            if (pData->oBlockType ==  NvMMBlockType_3gppWriter)
            {
                // Pass Sync frame & code config info to nvmm writer.
                if (pOMXBuf->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
                    pNvxBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame = NV_TRUE;
                else
                    pNvxBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame = NV_FALSE;

                if (pOMXBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
                    pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
            }

            if (pNvxBuffer->Payload.Ref.pMem)
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

        if (pData->oBlockType == NvMMBlockType_DecH264)
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

        if (pData->oBlockType == NvMMBlockType_DecVC1 ||
            pData->oBlockType == NvMMBlockType_DecWMA)
        {
            if (pOMXBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
                pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
        }

        if (pOMXBuf->nFlags & OMX_BUFFERFLAG_NEED_PTS)
        {
            pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_PTSCalculationRequired;
        }

        if (pOMXBuf->nFlags & OMX_BUFFERFLAG_EOT)
        {
            pNvxBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_EndOfTrack;
        }

        if (OMX_BUFFERFLAG_EXTRADATA & pOMXBuf->nFlags)
        {
            OMX_BUFFERHEADERTYPE* ptmpHeader;
            NvU8* pData;
            OMX_OTHER_EXTRADATATYPE *p_extraDataType;

            ptmpHeader = (OMX_BUFFERHEADERTYPE* )pOMXBuf;
            pData = ptmpHeader->pBuffer + ptmpHeader->nOffset + ptmpHeader->nFilledLen;
            pData = (OMX_U8*)(((OMX_U32)pData+3)&(~3));
            p_extraDataType = (OMX_OTHER_EXTRADATATYPE *)pData;

            if (((OMX_EXTRADATATYPE)NVX_IndexExtraDataEncryptionMetadata == p_extraDataType->eType) &&
                (pOMXBuf->pAppPrivate != NULL))
            {
                NvxAesMetadata *pAesMetadata = (NvxAesMetadata *) pOMXBuf->pAppPrivate;
                NvU32 i=0, size=0;
                NvU8* ptmpHeader = NULL;
                DRM_EXTRADATA_ENCRYPTION_METADATA *metadata;
                NvU32 lastEncryptedDataEnd;

                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_TRUE;
                pNvxBuffer->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_Aes;

                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData = (NvMMAesData *)((NvU32)pNvxBuffer->Payload.Ref.pMem +
                    pNvxBuffer->Payload.Ref.startOfValidData +
                    pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes);
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData =
                    (NvMMAesData*)(((NvU32)pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData + 3) & (~3));

                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.PhysicalAddress_AesData = pNvxBuffer->Payload.Ref.PhyAddress +
                    pNvxBuffer->Payload.Ref.startOfValidData +
                    pNvxBuffer->Payload.Ref.sizeOfValidDataInBytes;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.PhysicalAddress_AesData =
                    ((pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.PhysicalAddress_AesData + 3) & (~3));

                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount = pAesMetadata->metadataCount;

                /* Fill up NvMMAesData */

                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.bEncrypted = OMX_TRUE;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->AlgorithmID = NvMMMAesAlgorithmID_AES_ALGO_ID_CTR_MODE;
                pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData->IV_size = 16;

                ptmpHeader = pNvxBuffer->Payload.Ref.pMem;
                metadata = pAesMetadata->metadataList;
                lastEncryptedDataEnd = 0;

                NV_DEBUG_PRINTF(("metadataCount: %d\n", pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount));

                for (i=0; i<pAesMetadata->metadataCount; i++)
                {
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
                size = pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.metadataCount * sizeof(NvMMAesData);
                /* CacheMaint if bInSharedMemory == NV_TRUE and MemoryType == WriteCombined */
                if (pPort->bInSharedMemory == NV_TRUE && pNvxBuffer->Payload.Ref.MemoryType == NvMMMemoryType_WriteCombined)
                    NvRmMemCacheMaint(pNvxBuffer->Payload.Ref.hMem, (void *) pNvxBuffer->PayloadInfo.BufferMetaData.AesMetadata.pAesData, size, NV_TRUE, NV_FALSE);
            }
        }

        if (pData->bProfile)
        {
            if (pData->nNumTBTBPackets == 0)
                pData->llFirstTBTB = NvOsGetTimeUS();
            pData->nNumTBTBPackets++;
            NVMMVERBOSE(("%s: TBTB: (%p, len %d) at %f\n", pData->sName,
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

        pPort->bFirstBuffer = OMX_FALSE;
        bSentData = OMX_TRUE;
    }

    if (pOMXBuf->nFlags & OMX_BUFFERFLAG_EOS && !pPort->bSentEOS)
    {
        NvMMEventStreamEndInfo streamEndInfo;

        NVMMVERBOSE(("Sending EOS to block\n"));
        NvDebugOmx(("Sending EOS to block %s", pData->sName));

        NvOsMemset(&streamEndInfo, 0, sizeof(NvMMEventStreamEndInfo));
        streamEndInfo.event = NvMMEvent_StreamEnd;
        streamEndInfo.structSize = sizeof(NvMMEventStreamEndInfo);

        pData->TransferBufferToBlock(pData->hBlock, nNumStream,
            NvMMBufferType_StreamEvent,
            sizeof(NvMMEventStreamEndInfo),
            &streamEndInfo);
        pPort->bSentEOS = OMX_TRUE;
    }

    if (pData->oBlockType == NvMMBlockType_AudioMixer &&
        pOMXBuf->nFlags & OMX_BUFFERFLAG_EOS && !bSentData)
    {
        pPort->pSavedEOSBuf = pOMXBuf;
        bSentData = OMX_TRUE;
    }

    if (!bSentData)
    {
        NvxPortReleaseBuffer(pPort->pOMXPort, pOMXBuf);
    }

    pPort->pOMXPort->pCurrentBufferHdr = NULL;

    return eError;
}

OMX_BOOL NvxNvMMTransformIsInputSkippingWorker(SNvxNvMMTransformData *pData,
                                               int nStreamNum)
{
    SNvxNvMMPort *pPort;
    pPort = &pData->oPorts[nStreamNum];

    return pPort->bSkipWorker;
}

// forward declaration of functions.
static OMX_ERRORTYPE NvxNvMMTransformFreePortBuffers(SNvxNvMMTransformData *pData,
                                                     int nStreamNum);
static NvError NvxNvMMTransformAllocateSurfaces(SNvxNvMMTransformData *pData,
                                                NvMMEventNewVideoStreamFormatInfo *pVideoStreamInfo,
                                                OMX_U32 streamIndex);

static OMX_ERRORTYPE NvxNvMMTransformReconfigurePortBuffers(SNvxNvMMTransformData *pData)
{
    NvError status;
    NvU32 NumSurfaces,i,streamIndex;
    SNvxNvMMPort *pPort;
    streamIndex = pData->nReconfigurePortNum;
    pPort = &(pData->oPorts[streamIndex]);

    if (OMX_FALSE == pPort->bTunneling)
    {
        /**** NON Tunneled case ****/
        // In case of non-Tunnleled case we donot share our buffers with any other OMX component.
        // Hence we can handle this on our own.
        // In order to reconfigure the new buffers we need to :
        // 1.  Pause the NvMM Block.
        // 2.  Call AbortBuffers on the concerned Stream of the NvMM Block.
        // 3.  Deallocate all the existing buffers.
        // 4.  Reallocate all the buffers.
        // 5.  TBF new buffers to the Block.
        // 6.  Set the state to running.

        if (!pPort->bUsesANBs)
        {
            pPort->nBufsTot = 0;
            NumSurfaces = pData->VideoStreamInfo.NumOfSurfaces;
            status = NvxNvMMTransformAllocateSurfaces(pData,
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
                pPort->nBufsTot++;
            }
        }
    }
    else
    {
        // TBD.
        // In Tunneled mode we are sharing OMX_Buffers with other OMX component (renderer) and hence we might need
        // app intervention.
        // We need to do following things in order to realize this.
        // 1.  Pause current OMX component and corresponding NvMM block.
        // 2.  Pause the tunneled OMX component.
        // 3.  Flush the tunneld component so that we can have all the buffers back.
        // 4.  Flush the Decoder NvMM block in order to get back buffers from NvMM side.
        // 5.  Deallocate all the Port Buffers and corresponding NvMM buffers.
        // 6.  Reallocate new Port buffers and corresponding NvMM Buffers.
        // 7.  TBF all these buffers to the decoder
        // 8.  set the sate to Executing / Running for tunneled component
        NV_ASSERT(0);
    }

    pData->bReconfigurePortBuffers = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformWorkerBase(SNvxNvMMTransformData *pData, int nNumStream, OMX_BOOL *pbNextWouldFail)
{
    SNvxNvMMPort *pPort;

    pPort = &pData->oPorts[nNumStream];

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
        }

        pPort->bSentInitialBuffers = OMX_TRUE;
        *pbNextWouldFail = OMX_TRUE;
        return OMX_ErrorNone;
    }

    if (!pPort->bSkipWorker)
    {
        pData->bHasInputRunning = OMX_TRUE;
        return NvxNvMMProcessInputBuffer(pData, nNumStream, pbNextWouldFail);
    }

    if (pbNextWouldFail)
        *pbNextWouldFail = OMX_TRUE;
    return OMX_ErrorNone;
}

static void NvxNvMMTransformSetBufferConfig(SNvxNvMMTransformData *pData, int nStream)
{
    NvMMNewBufferConfigurationInfo bufConfig;
    NvMMNewBufferRequirementsInfo *req;
    NvU32 nNumBufs = pData->oPorts[nStream].nBufsTot;
    NvU32 nBufSize = pData->oPorts[nStream].nBufSize;

    NvOsSemaphoreWait(pData->oPorts[nStream].buffReqSem);

    req = &(pData->oPorts[nStream].bufReq);

    if (nNumBufs < req->minBuffers)
        nNumBufs = req->minBuffers;
    if (nNumBufs > req->maxBuffers)
        nNumBufs = req->maxBuffers;
    if (nBufSize < req->minBufferSize)
        nBufSize = req->minBufferSize;
    if (nBufSize > req->maxBufferSize)
        nBufSize = req->maxBufferSize;

    if (pData->oBlockType == NvMMBlockType_SuperParser)
        pData->pParentComp->pPorts[nStream].oPortDef.nBufferSize = nBufSize;

    NvOsMemset(&bufConfig, 0, sizeof(bufConfig));
    bufConfig.numBuffers = nNumBufs;
    pData->oPorts[nStream].nBufsTot = nNumBufs;
    bufConfig.bufferSize = nBufSize;
    pData->oPorts[nStream].nBufSize = nBufSize;

    bufConfig.memorySpace = req->memorySpace;
    bufConfig.byteAlignment = req->byteAlignment;
    bufConfig.endianness = req->endianness;
    bufConfig.bPhysicalContiguousMemory = req->bPhysicalContiguousMemory;
    bufConfig.bInSharedMemory = !!(pData->bInSharedMemory | req->bInSharedMemory);
    pData->oPorts[nStream].bInSharedMemory = bufConfig.bInSharedMemory;


    bufConfig.structSize = sizeof(NvMMNewBufferConfigurationInfo);
    bufConfig.event = NvMMEvent_NewBufferConfiguration;

    pData->TransferBufferToBlock(pData->hBlock, nStream,
        NvMMBufferType_StreamEvent,
        sizeof(NvMMNewBufferConfigurationInfo),
        &bufConfig);

    NvOsSemaphoreWait(pData->oPorts[nStream].buffConfigSem);
}

OMX_ERRORTYPE NvxNvMMTransformOpen(SNvxNvMMTransformData *pData, NvMMBlockType oBlockType, const char *sBlockName, int nNumStreams, NvxComponent *pNvComp)
{
    NvMMLocale locale = 0;
    int i = 0;
    NvError status = NvSuccess;
    NvMMCreationParameters oCreate;
    int len;
    NvMMAttrib_EnableUlpMode oUlpInfo;

    NvOsMemset(&oUlpInfo, 0, sizeof(NvMMAttrib_EnableUlpMode));
    if (nNumStreams > TF_NUM_STREAMS)
        return OMX_ErrorBadParameter;

    pData->nNumStreams = nNumStreams;
    pData->pParentComp = pNvComp;

    pData->InterlaceThreadActive = OMX_FALSE;  //Interlace thread is not yet created

    if (pData->nForceLocale > 0)
    {
        locale = (pData->nForceLocale == 1) ? NvMMLocale_CPU : NvMMLocale_AVP;
    }

    if (oBlockType == NvMMBlockType_DecMPEG4 ||
        oBlockType == NvMMBlockType_DecH264 ||
        oBlockType == NvMMBlockType_DecVC1 ||
        oBlockType == NvMMBlockType_DecSuperJPEG ||
        oBlockType == NvMMBlockType_DigitalZoom ||
        oBlockType == NvMMBlockType_DecMPEG2VideoRecon ||
        oBlockType == NvMMBlockType_DecVP8 ||
        oBlockType == NvMMBlockType_DecMPEG2)
    {
        pData->bVideoTransform = OMX_TRUE;
    }

    pData->bAudioTransform = OMX_FALSE;
    if (oBlockType > NvMMBlockType_DecAudioStart &&
        oBlockType < NvMMBlockType_DecAudioEnd)
    {
        pData->bAudioTransform = OMX_TRUE;
    }

    if (NvSuccess != NvRmOpen(&(pData->hRmDevice), 0))
        return OMX_ErrorBadParameter;

    pData->locale = locale;
    pData->nNalStreamFlag = 0;
    pData->nNumInputPorts = pData->nNumOutputPorts = 0;
    pData->oBlockType = oBlockType;
    pData->bSequence = OMX_FALSE;
    for (i = 0; i < TF_NUM_STREAMS; i++)
    {
        OMX_BOOL bEmbedNvMMBuffer = pData->oPorts[i].bEmbedNvMMBuffer;
        OMX_BOOL bUsesANBs = pData->oPorts[i].bUsesANBs;
        OMX_BOOL bWillCopyToANB = pData->oPorts[i].bWillCopyToANB;
        OMX_BOOL bUsesAndroidMetaDataBuffers =
                    pData->oPorts[i].bUsesAndroidMetaDataBuffers;
        OMX_BOOL bEnc2DCopy = pData->oPorts[i].bEnc2DCopy;

        NvOsMemset(&(pData->oPorts[i]), 0, sizeof(SNvxNvMMPort));
        pData->oPorts[i].nWidth = 176;
        pData->oPorts[i].nHeight = 144;
        NvOsMemset(&pData->oPorts[i].outRect, 0, sizeof(OMX_CONFIG_RECTTYPE));
        pData->oPorts[i].outRect.nSize = sizeof(OMX_CONFIG_RECTTYPE);
        pData->oPorts[i].bHasSize = OMX_FALSE;
        pData->oPorts[i].bEOS = OMX_FALSE;
        pData->oPorts[i].bEmbedNvMMBuffer = bEmbedNvMMBuffer;
        pData->oPorts[i].bUsesAndroidMetaDataBuffers =
            bUsesAndroidMetaDataBuffers;
        pData->oPorts[i].xScaleWidth = NV_SFX_ONE;
        pData->oPorts[i].xScaleHeight = NV_SFX_ONE;
        pData->oPorts[i].bUsesANBs = bUsesANBs;
        pData->oPorts[i].bWillCopyToANB = bWillCopyToANB;
        pData->oPorts[i].bEnc2DCopy = bEnc2DCopy;
    }

    if (NvSuccess != NvOsSemaphoreCreate(&(pData->stateChangeDone), 0))
        return OMX_ErrorInsufficientResources;
    if (NvSuccess != NvOsSemaphoreCreate(&(pData->SetAttrDoneSema), 0))
        return OMX_ErrorInsufficientResources;

    pData->blockCloseDone = OMX_FALSE;
    pData->blockErrorEncountered = OMX_FALSE;

    for (i = 0; i < nNumStreams; i++)
    {
        pData->oPorts[i].bFirstBuffer = OMX_TRUE;
        pData->oPorts[i].bFirstOutBuffer = OMX_TRUE;
        if (NvSuccess != NvOsSemaphoreCreate(&(pData->oPorts[i].buffConfigSem), 0))
            return OMX_ErrorInsufficientResources;
        if (NvSuccess != NvOsSemaphoreCreate(&(pData->oPorts[i].buffReqSem), 0))
            return OMX_ErrorInsufficientResources;
        if (NvSuccess != NvOsSemaphoreCreate(&(pData->oPorts[i].blockAbortDone), 0))
            return OMX_ErrorInsufficientResources;
    }

    len = NvOsStrlen(sBlockName);
    if (len > 15)
        len = 15;

    NvOsMemset(pData->sName, 0, 32);
    NvOsSnprintf(pData->sName, 31, "%d%s", s_nBlockCount, sBlockName);

    NvOsMemset(&oCreate, 0, sizeof(oCreate));
    oCreate.Locale = locale;
    oCreate.Type = oBlockType;
    oCreate.SetULPMode = pData->enableUlpMode;
    oCreate.BlockSpecific = (NvU64)(pData->BlockSpecific);
    oCreate.structSize = sizeof(NvMMCreationParameters);

    status = NvMMOpenBlock(&pData->hBlock, &oCreate);

    if (status != NvSuccess)
    {
        // TO DO: check for other error/success codes?
        return OMX_ErrorInsufficientResources;
    }
    s_nBlockCount = (s_nBlockCount + 1) % 1000;

    pData->bInSharedMemory = (locale == NvMMLocale_AVP || pData->bVideoTransform);
    if(pData->bInSharedMemory == 1)
    {
        pData->bInSharedMemory = !(oBlockType == NvMMBlockType_DecH264 && (locale != NvMMLocale_AVP));
        NVMMVERBOSE(("pData->bInSharedMemory %d Locale check : %d locale : %d  AVP : %d", pData->bInSharedMemory, (locale != NvMMLocale_AVP), locale, NvMMLocale_AVP));
    }

    // Strictly rely on buffer requirement from h264 decoder
    if (oBlockType == NvMMBlockType_DecH264)
        pData->bInSharedMemory = OMX_FALSE;

    /* Get the TBF of the block */
    pData->TransferBufferToBlock = pData->hBlock->GetTransferBufferFunction(pData->hBlock, 0, pData);

    /* Set event and transfer functions */
    pData->hBlock->SetSendBlockEventFunction(pData->hBlock,
        pData,
        NvxNvMMTransformSendBlockEventFunction);

    if (pData->nNvMMProfile & 0x0F)
    {
        NvMMAttrib_ProfileInfo oProfileInfo;
        NvOsMemset(&oProfileInfo, 0, sizeof(NvMMAttrib_ProfileInfo));
        oProfileInfo.ProfileType = pData->nNvMMProfile;

        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMVideoDecAttribute_EnableProfiling,
            0,
            sizeof(NvMMAttrib_ProfileInfo),
            &oProfileInfo);
    }

    if (pData->nNvMMProfile & 0x20)
    {
        NvMMAttrib_ProfileInfo oProfileInfo;
        NvOsMemset(&oProfileInfo, 0, sizeof(NvMMAttrib_ProfileInfo));
        oProfileInfo.ProfileType = pData->nNvMMProfile;

        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMVideoDecAttribute_Enable_VDE_Cache_Profiling,
            0,
            sizeof(NvMMAttrib_ProfileInfo),
            &oProfileInfo);
    }

    if (pData->enableUlpMode == OMX_TRUE)
    {
        oUlpInfo.bEnableUlpMode = pData->enableUlpMode;
        oUlpInfo.ulpKpiMode = pData->ulpkpiMode;

        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMAttribute_UlpMode,
            NvMMSetAttrFlag_Notification,
            sizeof(NvMMAttrib_EnableUlpMode),
            &oUlpInfo);

        //Wait for the file attribute set acknowledgement
        NvOsSemaphoreWait (pData->SetAttrDoneSema);
    }

    if (pData->bSetFileCache)
    {
        NvMMAttrib_FileCacheSize oFCSize;
        NvOsMemset(&oFCSize, 0, sizeof(NvMMAttrib_FileCacheSize));
        oFCSize.nFileCacheSize = pData->nFileCacheSize;
        pData->hBlock->SetAttribute(pData->hBlock,
            NvMMAttribute_FileCacheSize,
            NvMMSetAttrFlag_Notification,
            sizeof(NvMMAttrib_FileCacheSize),
            &oFCSize);

        //Wait for the file attribute set acknowledgement
        NvOsSemaphoreWait(pData->SetAttrDoneSema);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetStreamUsesSurfaces(SNvxNvMMTransformData *pData, int nStreamNum, OMX_BOOL bUseSurface)
{
    NV_ASSERT(nStreamNum < TF_NUM_STREAMS);

    pData->oPorts[nStreamNum].bUsesRmSurfaces = bUseSurface;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetupAudioParams(SNvxNvMMTransformData *pData, int nStreamNum, OMX_U32 sampleRate, OMX_U32 bitsPerSample, OMX_U32 nChannels)
{
    NV_ASSERT(nStreamNum < TF_NUM_STREAMS);

    pData->oPorts[nStreamNum].bSetAudioParams = OMX_TRUE;
    pData->oPorts[nStreamNum].nSampleRate = sampleRate;
    pData->oPorts[nStreamNum].nChannels = nChannels;
    pData->oPorts[nStreamNum].nBitsPerSample = bitsPerSample;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetEventCallback(SNvxNvMMTransformData *pData, void (*EventCB)(NvxComponent *pComp, NvU32 eventType, NvU32 eventSize, void *eventInfo), void *pEventData)
{
    pData->EventCallback = EventCB;
    pData->pEventData = pEventData;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetInputSurfaceHook(SNvxNvMMTransformData *pData, InputSurfaceHookFunction ISHook)
{
    pData->InputSurfaceHook = ISHook;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetupCameraInput(SNvxNvMMTransformData *pData,
                                         int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs,
                                         int nInputBufSize, OMX_BOOL bCanSkipWorker)
{
    int i = 0;
    NvBool bTunnelMode = NV_FALSE;

    bTunnelMode = (pOMXPort->bNvidiaTunneling) &&
        (pOMXPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

    if (bTunnelMode)
    {
        NV_DEBUG_PRINTF(("Tunneling is not supported on camera input port"));
        return OMX_ErrorNotImplemented;
    }


    pData->hBlock->SetTransferBufferFunction(pData->hBlock, nStreamNum,
        NvxNvMMTransformReturnEmptyInput,
        pData, nStreamNum);

    pData->nNumInputPorts++;

    pData->oPorts[nStreamNum].pOMXPort = pOMXPort;
    pData->oPorts[nStreamNum].nType = TF_TYPE_INPUT;
    pData->oPorts[nStreamNum].nBufsTot = nNumInputBufs;
    pData->oPorts[nStreamNum].nCurrentBuffer = 0;
    pData->oPorts[nStreamNum].nBufferWritePos = 0;
    pData->oPorts[nStreamNum].nBufsInUse = 0;
    pData->oPorts[nStreamNum].bSentEOS = OMX_FALSE;
    pData->oPorts[nStreamNum].bAborting = OMX_FALSE;
    pData->oPorts[nStreamNum].bError = OMX_FALSE;
    pData->oPorts[nStreamNum].bCanSkipWorker = bCanSkipWorker;
    pData->oPorts[nStreamNum].bSkipWorker = OMX_FALSE;
    pData->oPorts[nStreamNum].bInSharedMemory = pData->bInSharedMemory;
    pData->oPorts[nStreamNum].nBufSize = nInputBufSize;

    NvMMQueueCreate(&(pData->oPorts[nStreamNum].pMarkQueue), MAX_NVMM_BUFFERS, \
        sizeof(BufferMarkData), NV_FALSE);
    NvxMutexCreate(&(pData->oPorts[nStreamNum].oMutex));

    // Camera host input buffers should be allocated by nvmm-camera
    pData->oPorts[nStreamNum].bNvMMClientAlloc = OMX_FALSE;

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

// In some usecases, nvmm_camera allocates buffers inside itself. Openmax camera component needs
// to return the buffers to nvmm_camera.

void NvxNvMMTransformReturnNvMMBuffers(void *pContext, int nNumStream)
{
    int i;
    SNvxNvMMPort *pPort;
    SNvxNvMMTransformData *pData = (SNvxNvMMTransformData *)pContext;

    pPort = &pData->oPorts[nNumStream];

    for (i = 0; i < pPort->nBufsTot; i++)
    {
        if (pPort->pNvMMBufMap[i])
        {
            pData->TransferBufferToBlock(pData->hBlock, nNumStream,
                NvMMBufferType_Payload,
                sizeof(NvMMBuffer),
                pPort->pNvMMBufMap[i]);
            pPort->pNvMMBufMap[i] = NULL;
        }
    }
}

OMX_ERRORTYPE NvxNvMMTransformSetupInput(SNvxNvMMTransformData *pData,
                                         int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs,
                                         int nInputBufSize, OMX_BOOL bCanSkipWorker)
{
    int i = 0;
    NvBool bTunnelMode = NV_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError err = NvSuccess;
    NvBool UseAlignment = NV_TRUE;
    bTunnelMode = (pOMXPort->bNvidiaTunneling) &&
        (pOMXPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

    if (bTunnelMode)
    {
        NVX_CONFIG_GETNVMMBLOCK oGetBlock;

        oGetBlock.nSize = sizeof(NVX_CONFIG_GETNVMMBLOCK);
        oGetBlock.nVersion = NvxComponentSpecVersion;
        oGetBlock.nPortIndex = pOMXPort->nTunnelPort;

        eError = OMX_GetConfig(pOMXPort->hTunnelComponent,
            NVX_IndexConfigGetNVMMBlock,
            &oGetBlock);

        if (OMX_ErrorNone == eError)
        {
            eError = NvxNvMMTransformTunnelBlocks(
                oGetBlock.pNvMMTransform, oGetBlock.nNvMMPort,
                pData, nStreamNum);
            if (eError != OMX_ErrorNone)
            {
                NV_DEBUG_PRINTF(("ERROR: Unable to NvMM tunnel!\n"));
                return eError;
            }
        }

        // Transform should override GetConfig to support nvmm tunneling:
        // OMX_ErrorNotReady is OK. BadParam or Unsupported is not.
        NV_ASSERT( eError != OMX_ErrorBadParameter );
        pData->oPorts[nStreamNum].nType = TF_TYPE_INPUT_TUNNELED;
        return OMX_ErrorNone;
    }


    pData->hBlock->SetTransferBufferFunction(pData->hBlock, nStreamNum,
        NvxNvMMTransformReturnEmptyInput,
        pData, nStreamNum);

    pData->nNumInputPorts++;

    pData->oPorts[nStreamNum].pOMXPort = pOMXPort;
    pData->oPorts[nStreamNum].nType = TF_TYPE_INPUT;
    pData->oPorts[nStreamNum].nBufsTot = nNumInputBufs;
    pData->oPorts[nStreamNum].nCurrentBuffer = 0;
    pData->oPorts[nStreamNum].nBufferWritePos = 0;
    pData->oPorts[nStreamNum].nBufsInUse = 0;
    pData->oPorts[nStreamNum].bSentEOS = OMX_FALSE;
    pData->oPorts[nStreamNum].bAborting = OMX_FALSE;
    pData->oPorts[nStreamNum].bError = OMX_FALSE;
    pData->oPorts[nStreamNum].bCanSkipWorker = bCanSkipWorker;
    pData->oPorts[nStreamNum].bSkipWorker = OMX_FALSE;
    pData->oPorts[nStreamNum].bInSharedMemory = pData->bInSharedMemory;
    pData->oPorts[nStreamNum].nBufSize = nInputBufSize;

    NvMMQueueCreate(&(pData->oPorts[nStreamNum].pMarkQueue), MAX_NVMM_BUFFERS, \
        sizeof(BufferMarkData), NV_FALSE);
    NvxMutexCreate(&(pData->oPorts[nStreamNum].oMutex));

    NvxNvMMTransformSetBufferConfig(pData, nStreamNum);

    // Since we're not NvMM tunneled, default to creating our own buffers.
    pData->oPorts[nStreamNum].bNvMMClientAlloc = OMX_TRUE;
    if (pData->oBlockType == NvMMBlockType_DecSuperJPEG)
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
            NvxNvMMTransformSetStreamUsesSurfaces(pData, nStreamNum, bUseRmSurfaces);
        }

        for (i = 0; i < pData->oPorts[nStreamNum].nBufsTot; i++)
        {
            pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
            if (!pData->oPorts[nStreamNum].pBuffers[i])
            {
                NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !"));
                return OMX_ErrorInsufficientResources;
            }

            // If we're not Nvidia tunneled and using NvMM surfaces, we need to allocate our own.
            // Otherwise, we don't need to waste memory allocating more surfaces
            if (bUseRmSurfaces)
            {
                NvMMBuffer * pNvxBuffer = (NvMMBuffer *)pData->oPorts[nStreamNum].pBuffers[i];
                NvOsMemset(pNvxBuffer, 0, sizeof(NvMMBuffer));
                pNvxBuffer->BufferID = i;
                pNvxBuffer->PayloadInfo.TimeStamp = 0;

                pData->oPorts[nStreamNum].bNvMMClientAlloc = OMX_FALSE;
            }
            else
            {
                if ((pData->oPorts[nStreamNum].bEmbedNvMMBuffer != OMX_TRUE) ||
                    (pData->oPorts[nStreamNum].bEnc2DCopy))
                {
                    if (pData->oBlockType == NvMMBlockType_EncJPEG)
                    {
                        UseAlignment = NV_FALSE;
                        err = NvxVideoSurfaceAlloc(pData->hRmDevice,
                            pData->oPorts[nStreamNum].pBuffers[i], i,
                            pData->oPorts[nStreamNum].nWidth,
                            pData->oPorts[nStreamNum].nHeight,
                            pOMXPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar ?
                                NvMMVideoDecColorFormat_YUV420SemiPlanar : NvMMVideoDecColorFormat_YUV420,
                            NvRmSurfaceLayout_Pitch,
                            &ImageSize,
                            UseAlignment,NvOsMemAttribute_WriteCombined, NV_TRUE);
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
                            NvRmSurfaceLayout_Tiled,
                            &ImageSize,
                            UseAlignment, NvOsMemAttribute_Uncached, NV_TRUE);
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
        }
    }
    else
    {
        /* WriteCombined allocs for both Audio and Video input buffers if bInSharedMemory == NV_TRUE */
        if ((pData->oPorts[nStreamNum].bInSharedMemory == NV_TRUE) && (pData->bVideoTransform || pData->bAudioTransform))
            pData->oPorts[nStreamNum].bufReq.memorySpace = NvMMMemoryType_WriteCombined;

        NVMMVERBOSE(("creating %d input buffers of size: %d, align %d, shared %d\n",
            pData->oPorts[nStreamNum].nBufsTot,
            pData->oPorts[nStreamNum].nBufSize,
            pData->oPorts[nStreamNum].bufReq.byteAlignment,
            pData->oPorts[nStreamNum].bInSharedMemory));

        // Allocate normal buffers
        for (i = 0; i < pData->oPorts[nStreamNum].nBufsTot; i++)
        {
            pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
            err = NvMMUtilAllocateBuffer(pData->hRmDevice,
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

            pData->oPorts[nStreamNum].pBuffers[i]->BufferID = i;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformRegisterDeliverBuffer(SNvxNvMMTransformData *pData, int nStreamNum)
{
    pData->hBlock->SetTransferBufferFunction(pData->hBlock,
        nStreamNum,
        NvxNvMMTransformDeliverFullOutput,
        pData,
        nStreamNum);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformSetupOutput(SNvxNvMMTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nInputStreamNum, int nNumOutputBufs, int nOutputBufSize)
{
    int i = 0;
    NvBool bTunnelMode = NV_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    bTunnelMode = (pOMXPort->bNvidiaTunneling) &&
        (pOMXPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

    if (bTunnelMode)
    {
        NVX_CONFIG_GETNVMMBLOCK oGetBlock;

        oGetBlock.nSize = sizeof(NVX_CONFIG_GETNVMMBLOCK);
        oGetBlock.nVersion = NvxComponentSpecVersion;
        oGetBlock.nPortIndex = pOMXPort->nTunnelPort;

        eError = OMX_GetConfig(pOMXPort->hTunnelComponent,
            NVX_IndexConfigGetNVMMBlock,
            &oGetBlock);

        if (OMX_ErrorNone == eError)
        {
            eError = NvxNvMMTransformTunnelBlocks(
                pData, nStreamNum,
                oGetBlock.pNvMMTransform, oGetBlock.nNvMMPort);
            if (eError != OMX_ErrorNone)
            {
                NV_DEBUG_PRINTF(("ERROR: Unable to NvMM tunnel!\n"));
                return eError;
            }
        }

        // Transform should override GetConfig to support nvmm tunneling:
        // OMX_ErrorNotReady is OK. BadParam or Unsupported is not.
        NV_ASSERT( eError != OMX_ErrorBadParameter );

        pData->oPorts[nStreamNum].nType = TF_TYPE_OUTPUT_TUNNELED;
        return OMX_ErrorNone;
    }


    pData->hBlock->SetTransferBufferFunction(pData->hBlock, nStreamNum,
        NvxNvMMTransformDeliverFullOutput,
        pData, nStreamNum);

    pData->nNumOutputPorts++;

    pData->oPorts[nStreamNum].pOMXPort = pOMXPort;
    pData->oPorts[nStreamNum].nInputPortNum = nInputStreamNum;
    pData->oPorts[nStreamNum].nType = TF_TYPE_OUTPUT;
    pData->oPorts[nStreamNum].nBufsTot = nNumOutputBufs;
    pData->oPorts[nStreamNum].nCurrentBuffer = 0;
    pData->oPorts[nStreamNum].nBufferWritePos = 0;
    pData->oPorts[nStreamNum].nBufsInUse = 0;
    pData->oPorts[nStreamNum].bSentEOS = OMX_FALSE;
    pData->oPorts[nStreamNum].bAborting = OMX_FALSE;
    pData->oPorts[nStreamNum].bError = OMX_FALSE;
    pData->oPorts[nStreamNum].bInSharedMemory = pData->bInSharedMemory;
    pData->oPorts[nStreamNum].nBufSize = nOutputBufSize;

    NvxMutexCreate(&(pData->oPorts[nStreamNum].oMutex));

    if (pData->bVideoTransform)
    {
        // not sure for cases other than SrcCamera and DigitalZoom
        if (pData->oBlockType == NvMMBlockType_SrcCamera ||
            pData->oBlockType == NvMMBlockType_SrcUSBCamera ||
            pData->oBlockType == NvMMBlockType_DigitalZoom)
        {
            for (i = 0; i < nNumOutputBufs; i++)
            {
                if ((!pData->oPorts[nStreamNum].pBuffers[i]) &&
                    (!pData->oPorts[nStreamNum].bUsesANBs))
                {
                    pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
                    NvOsMemset(pData->oPorts[nStreamNum].pBuffers[i], 0, sizeof(NvMMBuffer));
                    pData->oPorts[nStreamNum].pBuffers[i]->BufferID = i;
                }
            }
        }

        return OMX_ErrorNone;
    }

    NvxNvMMTransformSetBufferConfig(pData, nStreamNum);

    nNumOutputBufs = pData->oPorts[nStreamNum].nBufsTot;

    NVMMVERBOSE(("creating %d output buffers of size: %d, align %d, shared %d\n",
        pData->oPorts[nStreamNum].nBufsTot,
        pData->oPorts[nStreamNum].nBufSize,
        pData->oPorts[nStreamNum].bufReq.byteAlignment,
        pData->oPorts[nStreamNum].bInSharedMemory));

    /* WriteCombined allocs for Audio and Jpeg encoder output buffers if bInSharedMemory == NV_TRUE */
    if ((pData->oPorts[nStreamNum].bInSharedMemory == NV_TRUE) && (pData->bAudioTransform || (pData->oBlockType == NvMMBlockType_EncJPEG)))
    {
        pData->oPorts[nStreamNum].bufReq.memorySpace = NvMMMemoryType_WriteCombined;
    }

    for (i = 0; i < nNumOutputBufs; i++)
    {
        NvError err = NvSuccess;
        pData->oPorts[nStreamNum].pBuffers[i] = NvOsAlloc(sizeof(NvMMBuffer));
        err = NvMMUtilAllocateBuffer(pData->hRmDevice,
            pData->oPorts[nStreamNum].nBufSize,
            pData->oPorts[nStreamNum].bufReq.byteAlignment,
            pData->oPorts[nStreamNum].bufReq.memorySpace,
            pData->oPorts[nStreamNum].bInSharedMemory,
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
    }
    pData->oPorts[nStreamNum].bNvMMClientAlloc = OMX_TRUE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformTunnelBlocks(SNvxNvMMTransformData *pSource, int nSourcePort, SNvxNvMMTransformData *pDest, int nDestPort)
{
    pSource->hBlock->SetTransferBufferFunction(pSource->hBlock,
        nSourcePort, pDest->TransferBufferToBlock,
        (void *)pDest->hBlock, nDestPort);
    pDest->hBlock->SetTransferBufferFunction(pDest->hBlock,
        nDestPort, pSource->TransferBufferToBlock,
        (void *)pSource->hBlock, nSourcePort);

    if (pDest->bWantTunneledAlloc)
    {
        pDest->hBlock->SetBufferAllocator(pDest->hBlock, nDestPort, NV_TRUE);
        pDest->oPorts[nDestPort].bTunneledAllocator = OMX_TRUE;
        pSource->oPorts[nSourcePort].bTunneledAllocator = OMX_FALSE;
    }
    else
    {
        pSource->hBlock->SetBufferAllocator(pSource->hBlock, nSourcePort,
            NV_TRUE);
        pDest->oPorts[nDestPort].bTunneledAllocator = OMX_FALSE;
        pSource->oPorts[nSourcePort].bTunneledAllocator = OMX_TRUE;
    }

    pSource->oPorts[nSourcePort].bTunneling = OMX_TRUE;
    pSource->oPorts[nSourcePort].pTunnelBlock = pDest;
    pSource->oPorts[nSourcePort].nTunnelPort = nDestPort;
    pDest->oPorts[nDestPort].bTunneling = OMX_TRUE;
    pDest->oPorts[nDestPort].pTunnelBlock = pSource;
    pDest->oPorts[nDestPort].nTunnelPort = nSourcePort;

    return OMX_ErrorNone;
}

static void NvxNvMMTransformUntunnelBlocks(SNvxNvMMTransformData *pData, OMX_U32 nPort)
{
    SNvxNvMMPort *pPort = &(pData->oPorts[nPort]);
    SNvxNvMMTransformData *pTunnelData;
    NvU32 nTunnelPort;

    if (!pPort->bTunneling || !pPort->pTunnelBlock)
        return;

    pTunnelData = (SNvxNvMMTransformData *)(pPort->pTunnelBlock);
    nTunnelPort = pPort->nTunnelPort;

    if (!pTunnelData->hBlock)
    {
        return;
    }

    if (pTunnelData->oPorts[nTunnelPort].nType == TF_TYPE_OUTPUT_TUNNELED)
    {
        pTunnelData->hBlock->SetTransferBufferFunction(pTunnelData->hBlock,
            nTunnelPort, NvxNvMMTransformDeliverFullOutput,
            pTunnelData, nTunnelPort);
    }
    else if (pTunnelData->oPorts[nTunnelPort].nType == TF_TYPE_INPUT_TUNNELED)
    {
        pTunnelData->hBlock->SetTransferBufferFunction(pTunnelData->hBlock,
            nTunnelPort, NvxNvMMTransformReturnEmptyInput,
            pTunnelData, nTunnelPort);
    }

    pTunnelData->oPorts[nTunnelPort].pTunnelBlock = NULL;
}

static void NvxNvMMFlushTunnelPort(SNvxNvMMTransformData *pData, OMX_U32 nPort)
{
    SNvxNvMMPort *pPort = &(pData->oPorts[nPort]);
    NvMMBlockHandle hNvMMBlock = pData->hBlock;
    //NvMMAttrib_StreamPosition temp;
    SNvxNvMMTransformData *pTunnelData;

    pTunnelData = (SNvxNvMMTransformData *)(pPort->pTunnelBlock);

    if (!pTunnelData)
        return;

    hNvMMBlock->AbortBuffers(hNvMMBlock, nPort);
    NvOsSemaphoreWait(pPort->blockAbortDone);
}

OMX_ERRORTYPE NvxNvMMTransformFlush(SNvxNvMMTransformData *pData, OMX_U32 nPort)
{
    OMX_U32 nFirstPort = 0, nLastPort = pData->nNumStreams - 1;
    OMX_U32 i = 0;
    NvMMBlockHandle hNvMMBlock = pData->hBlock;
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

    for (i = nFirstPort; i <= nLastPort; i++)
    {
        SNvxNvMMPort *pPort = &(pData->oPorts[i]);

        pPort->bEOS = OMX_FALSE;
        pPort->bSentEOS = OMX_FALSE;

        if (pPort->bCanSkipWorker == OMX_TRUE)
        {
            if (pData->oBlockType != NvMMBlockType_DecSuperJPEG)
                pPort->bSentInitialBuffers = OMX_FALSE;
            pPort->bSkipWorker = OMX_FALSE;
        }

        pPort->position = (OMX_TICKS)-1;

        if (pPort->nType == TF_TYPE_INPUT)
        {
            OMX_U32 j;

            NvxMutexUnlock(pComp->hWorkerMutex);
            hNvMMBlock->AbortBuffers(hNvMMBlock, i);
            NvOsSemaphoreWait(pPort->blockAbortDone);
            NvxMutexLock(pComp->hWorkerMutex);

            for (j = 0; j < MAX_NVMM_BUFFERS; j++)
            {
                if (pPort->pOMXBufMap[j])
                {
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
            int j;

            NvxMutexUnlock(pComp->hWorkerMutex);
            hNvMMBlock->AbortBuffers(hNvMMBlock, i);
            NvOsSemaphoreWait(pPort->blockAbortDone);
            if (pData->InterlaceStream == OMX_TRUE)
            {
                pData->Deinterlacethreadtopuase = OMX_TRUE;
                NvOsSemaphoreSignal(pData->InpOutAvail);
                NvOsSemaphoreWait(pData->DeintThreadPaused);
            }
            NvxMutexLock(pComp->hWorkerMutex);

            if (pData->InterlaceStream == OMX_TRUE)
            {
                NvU32 i;
                {
                    NvMMBuffer *OutSurface;
                    NvU32 QueueEntries = NvMMQueueGetNumEntries(pData->pReleaseOutSurfaceQueue);
                    for (i = 0; i < QueueEntries; i++)
                        NvMMQueueDeQ(pData->pReleaseOutSurfaceQueue, &OutSurface);
                }

                pData->vmr.VideoSurfaceIdx = 0;
                pData->vmr.pPrevVideoSurface = pData->vmr.pPrev2VideoSurface = NULL;
                pData->vmr.pCurrVideoSurface = pData->vmr.pNextVideoSurface = NULL;
                {
                    NvMMBuffer *PayloadBuffer;
                    int id;
                    PayloadBuffer = (NvMMBuffer *)pData->vmr.pPrev2Surface;
                    id = (int) PayloadBuffer->BufferID;
                    pPort->bBufNeedsFlush[id] = OMX_TRUE;
                    PayloadBuffer = (NvMMBuffer *)pData->vmr.pPrevSurface;
                    id = (int) PayloadBuffer->BufferID;
                    pPort->bBufNeedsFlush[id] = OMX_TRUE;
                    PayloadBuffer = (NvMMBuffer *)pData->vmr.pCurrSurface;
                    id = (int) PayloadBuffer->BufferID;
                    pPort->bBufNeedsFlush[id] = OMX_TRUE;
                    PayloadBuffer = (NvMMBuffer *)pData->vmr.pNextSurface;
                    id = (int) PayloadBuffer->BufferID;
                    pPort->bBufNeedsFlush[id] = OMX_TRUE;
                }
                pData->vmr.DeltaCount = pData->vmr.DeltaTime = 0;
            }

            for (j = 0; j < MAX_NVMM_BUFFERS; j++)
            {
                if (!pPort->bUsesANBs && pPort->bBufNeedsFlush[j])
                {
                    if (pPort->pOMXBufMap[j] && (pData->InterlaceStream == OMX_FALSE))
                    {
                        pPort->pOMXBufMap[j] = NULL;
                    }
                    pPort->bBufNeedsFlush[j] = OMX_FALSE;
                    if (pPort->pBuffers[j]->PayloadType == NvMMPayloadType_SurfaceArray)
                    {
                        pPort->pBuffers[j]->Payload.Surfaces.Empty = NV_TRUE;
                    }
                    else
                    {
                        pPort->pBuffers[j]->Payload.Ref.startOfValidData = 0;
                        pPort->pBuffers[j]->Payload.Ref.sizeOfValidDataInBytes  = 0;
                    }
                    pData->TransferBufferToBlock(pData->hBlock, i,
                        NvMMBufferType_Payload,
                        sizeof(NvMMBuffer),
                        pPort->pBuffers[j]);
                }
                else if (pPort->bUsesANBs && pPort->bBufNeedsFlush[j] && (pData->InterlaceStream == OMX_TRUE))
                {
                    pPort->bBufNeedsFlush[j] = OMX_FALSE;
                    if (pPort->pBuffers[j]->PayloadType == NvMMPayloadType_SurfaceArray)
                    {
                        pPort->pBuffers[j]->Payload.Surfaces.Empty = NV_TRUE;
                    }
                    else
                    {
                        pPort->pBuffers[j]->Payload.Ref.startOfValidData = 0;
                        pPort->pBuffers[j]->Payload.Ref.sizeOfValidDataInBytes  = 0;
                    }
                    pData->TransferBufferToBlock(pData->hBlock, i,
                        NvMMBufferType_Payload,
                        sizeof(NvMMBuffer),
                        pPort->pBuffers[j]);
                }
            }
        }
        else if (pPort->nType == TF_TYPE_INPUT_TUNNELED)
        {
            NvxMutexUnlock(pComp->hWorkerMutex);
            NvxNvMMFlushTunnelPort(pData, i);
            NvxMutexLock(pComp->hWorkerMutex);
        }
    }

    for (i = nFirstPort; i <= nLastPort; i++)
    {
        SNvxNvMMPort *pPort = &(pData->oPorts[i]);

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
    pData->bFlushing = OMX_FALSE;
    pData->blockErrorEncountered = OMX_FALSE;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxNvMMTransformFreePortBuffers(SNvxNvMMTransformData *pData, int nStreamNum)
{
    int i;
    SNvxNvMMPort *pPort = &(pData->oPorts[nStreamNum]);

    if (pPort->nType == TF_TYPE_NONE)
        return OMX_ErrorNone;

    for (i = 0; i < pPort->nBufsTot; i++)
    {
        if (pPort->pBuffers[i])
        {
            if (pPort->bNvMMClientAlloc)
            {
                if (pPort->pBuffers[i]->PayloadType == NvMMPayloadType_SurfaceArray)
                {
                    if ((!pPort->bEmbedNvMMBuffer && !pPort->bUsesANBs) ||
                        (pData->oPorts[nStreamNum].nType != TF_TYPE_INPUT))
                    {
                        NvxVideoDecSurfaceFree(pPort->pBuffers[i]);
                    }
                }
                else
                {
                    NvMMUtilDeallocateBuffer(pPort->pBuffers[i]);
                }
            }
            if (!pPort->bUsesANBs)
            {
                NvOsFree(pPort->pBuffers[i]);
            }
            pPort->pBuffers[i] = NULL;
        }
    }

    pPort->nBufsTot = 0;

    if ((!pPort->bUsesANBs) && (pData->InterlaceStream == NV_TRUE) && (pPort->nType == TF_TYPE_OUTPUT))
    {
        for (i = 0; i < TOTAL_NUM_SURFACES; i++)
        {
            if (pData->vmr.pVideoSurf[i])
            {
                NvxVideoDecSurfaceFree(pData->vmr.pVideoSurf[i]);
                NvOsFree(pData->vmr.pVideoSurf[i]);
                pData->vmr.pVideoSurf[i]= NULL;
            }
        }
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxNvMMTransformDestroyPort(SNvxNvMMTransformData *pData, int nStreamNum)
{
    SNvxNvMMPort *pPort = &(pData->oPorts[nStreamNum]);

    NvOsSemaphoreDestroy(pPort->buffConfigSem);
    pPort->buffConfigSem = NULL;
    NvOsSemaphoreDestroy(pPort->buffReqSem);
    pPort->buffReqSem = NULL;
    NvOsSemaphoreDestroy(pPort->blockAbortDone);
    pPort->blockAbortDone = NULL;

    if (pPort->nType == TF_TYPE_NONE)
        return OMX_ErrorNone;

    if (pPort->pMarkQueue)
        NvMMQueueDestroy(&pPort->pMarkQueue);
    pPort->pMarkQueue = NULL;

    NvxMutexDestroy(pPort->oMutex);
    pPort->oMutex = NULL;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformPreChangeState(SNvxNvMMTransformData *pData,
                                             OMX_STATETYPE oNewState,
                                             OMX_STATETYPE oOldState)
{
    int start = 0, stop = 0, pause = 0;
    NvMMBlockHandle hNvMMBlock = pData->hBlock;
    NvError status = NvSuccess;
    NvMMState eState = NvMMState_Running;

    if (!hNvMMBlock)
        return OMX_ErrorNone;

    hNvMMBlock->GetState(hNvMMBlock, &eState);

    if (oNewState == OMX_StateExecuting &&
        (oOldState == OMX_StateIdle || oOldState == OMX_StatePause))
        start = 1;
    else if ((oNewState == OMX_StateIdle || oNewState == OMX_StateLoaded ||
        oNewState == OMX_StateInvalid) &&
        (oOldState == OMX_StateExecuting || oOldState == OMX_StatePause))
        stop = 1;
    else if (oNewState == OMX_StatePause && oOldState == OMX_StateExecuting)
        pause = 1;

    if (start && eState != NvMMState_Running)
    {
        status = pData->hBlock->SetState(pData->hBlock, NvMMState_Running);
        NvOsSemaphoreWait(pData->stateChangeDone);
        if (status != NvSuccess)
            return OMX_ErrorInvalidState;

        return OMX_ErrorNone;
    }

    if (stop && eState != NvMMState_Stopped)
    {
        NvU32 i;

        pData->bStopping = OMX_TRUE;
        hNvMMBlock->SetState(hNvMMBlock, NvMMState_Stopped);
        NvOsSemaphoreWait(pData->stateChangeDone);
        pData->bStopping = OMX_FALSE;
        pData->bFlushing = OMX_TRUE;

        for (i = 0; i < pData->nNumStreams; i++)
        {
            SNvxNvMMPort *pPort = &(pData->oPorts[i]);

            if (pPort->nType != TF_TYPE_NONE && pPort->bNvMMClientAlloc)
            {
                hNvMMBlock->AbortBuffers(hNvMMBlock, i);
                NvOsSemaphoreWait(pPort->blockAbortDone);
            }
            else if ((pPort->nType == TF_TYPE_INPUT_TUNNELED ||
                pPort->nType == TF_TYPE_OUTPUT_TUNNELED) &&
                !pPort->bTunneledAllocator)
            {
                NvxNvMMFlushTunnelPort(pData, i);
            }

            if (pPort->nType == TF_TYPE_INPUT)
            {
                if (pPort->pSavedEOSBuf)
                {
                    NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pSavedEOSBuf);
                    pPort->pSavedEOSBuf = NULL;
                }
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
            SNvxNvMMPort *pPort = &(pData->oPorts[i]);

            if (pPort->nType != TF_TYPE_NONE && pPort->bNvMMClientAlloc)
            {
                hNvMMBlock->AbortBuffers(hNvMMBlock, i);
                NvOsSemaphoreWait(pPort->blockAbortDone);
            }
            else if ((pData->oPorts[i].nType == TF_TYPE_INPUT_TUNNELED ||
                pData->oPorts[i].nType == TF_TYPE_OUTPUT_TUNNELED) &&
                !pData->oPorts[i].bTunneledAllocator)
            {
                NvxNvMMFlushTunnelPort(pData, i);
            }

            if (pPort->nType == TF_TYPE_INPUT)
            {
                NvU32 j;
                for (j = 0; j < MAX_NVMM_BUFFERS; j++)
                {

                    if (pPort->pOMXBufMap[j])
                    {
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
        }
        pData->bFlushing = OMX_FALSE;
        return OMX_ErrorNone;
    }

    if (pause && eState != NvMMState_Paused)
    {
        status = pData->hBlock->SetState(pData->hBlock, NvMMState_Paused);
        pData->bPausing = OMX_TRUE;
        NvOsSemaphoreWait(pData->stateChangeDone);
        pData->bPausing = OMX_FALSE;
        if (status != NvSuccess)
            return OMX_ErrorInvalidState;

        return OMX_ErrorNone;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformClose(SNvxNvMMTransformData *pData)
{
    NvMMBlockHandle hNvMMBlock = pData->hBlock;
    NvU32 i = 0;

    NVMMVERBOSE(("Closing NvxNvMMTransform %s\n", pData->sName));

    if (hNvMMBlock)
    {
        NvMMState eState = NvMMState_Running;

        hNvMMBlock->GetState(hNvMMBlock, &eState);
        if (eState != NvMMState_Stopped)
        {
            hNvMMBlock->SetState(hNvMMBlock, NvMMState_Stopped);
            NvOsSemaphoreWait(pData->stateChangeDone);
        }

        for (i = 0; i < pData->nNumStreams; i++)
        {
            if (pData->oPorts[i].nType == TF_TYPE_INPUT_TUNNELED ||
                pData->oPorts[i].nType == TF_TYPE_OUTPUT_TUNNELED)
            {
                SNvxNvMMPort *pPort = &(pData->oPorts[i]);
                SNvxNvMMTransformData *pTunnelData;

                if (!pPort->bTunneling || !pPort->pTunnelBlock)
                    break;

                pTunnelData = (SNvxNvMMTransformData *)(pPort->pTunnelBlock);

                if (pTunnelData->hBlock)
                {
                    pTunnelData->hBlock->GetState(pTunnelData->hBlock, &eState);
                    if (eState != NvMMState_Stopped)
                        return OMX_ErrorNotReady;
                }
            }
        }

        for (i = 0; i < pData->nNumStreams; i++)
        {
            if (pData->oPorts[i].nType != TF_TYPE_NONE && pData->oPorts[i].bNvMMClientAlloc)
            {
                pData->oPorts[i].bAborting = OMX_TRUE;
                hNvMMBlock->AbortBuffers(hNvMMBlock, i);
                NvOsSemaphoreWait(pData->oPorts[i].blockAbortDone);
            }
        }

        for (i = 0; i < pData->nNumStreams; i++)
            NvxNvMMTransformFreePortBuffers(pData, i);

        NvMMCloseBlock(hNvMMBlock);
        pData->hBlock = NULL;

        for (i = 0; i < pData->nNumStreams; i++)
        {
            NvxNvMMTransformDestroyPort(pData, i);
            if (pData->oPorts[i].nType == TF_TYPE_INPUT_TUNNELED ||
                pData->oPorts[i].nType == TF_TYPE_OUTPUT_TUNNELED)
            {
                NvxNvMMTransformUntunnelBlocks(pData, i);
            }
        }
    }
    else
    {
        for (i = 0; i < pData->nNumStreams; i++)
            NvxNvMMTransformFreePortBuffers(pData, i);

        pData->hBlock = NULL;

        for (i = 0; i < pData->nNumStreams; i++)
        {
            NvxNvMMTransformDestroyPort(pData, i);
            if (pData->oPorts[i].nType == TF_TYPE_INPUT_TUNNELED ||
                pData->oPorts[i].nType == TF_TYPE_OUTPUT_TUNNELED)
            {
                NvxNvMMTransformUntunnelBlocks(pData, i);
            }
        }
    }
    for (i = 0; i < pData->nNumStreams; i++)
        pData->oPorts[i].bBufferNegotiationDone = OMX_FALSE;

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

    //deinterlacing related
    if (pData->hDeinterlaceThread)
    {
        if (pData->DeinterlaceThreadClose == OMX_FALSE)
            pData->DeinterlaceThreadClose = OMX_TRUE;

        TVMRClose(pData);
        NvOsSemaphoreSignal(pData->InpOutAvail);
        NvOsThreadJoin(pData->hDeinterlaceThread);
        pData->hDeinterlaceThread = NULL;
    }

    if (pData->TvmrInitDone)
    {
        NvOsSemaphoreDestroy(pData->TvmrInitDone);
        pData->TvmrInitDone = NULL;
    }
    if (pData->pReleaseOutSurfaceQueue)
    {
        NvMMQueueDestroy(&pData->pReleaseOutSurfaceQueue);
        pData->pReleaseOutSurfaceQueue = NULL;
    }

    if (pData->InpOutAvail)
    {
        NvOsSemaphoreDestroy(pData->InpOutAvail);
        pData->InpOutAvail = NULL;
    }
    if (pData->DeintThreadPaused)
    {
        NvOsSemaphoreDestroy(pData->DeintThreadPaused);
        pData->DeintThreadPaused = NULL;
    }
    NvOsSemaphoreDestroy(pData->stateChangeDone);
    pData->stateChangeDone = NULL;

    NvOsSemaphoreDestroy(pData->SetAttrDoneSema);
    pData->SetAttrDoneSema = NULL;

    NvRmClose(pData->hRmDevice);
    pData->hRmDevice = 0;

    return OMX_ErrorNone;
}

OMX_TICKS NvxNvMMTransformGetMediaTime(SNvxNvMMTransformData *pData,
                                       int nPort)
{
    NV_ASSERT(nPort < TF_NUM_STREAMS);
    return pData->oPorts[nPort].position;
}

static NvError NvxNvMMTransformAllocateSurfaces(SNvxNvMMTransformData *pData,
                                                NvMMEventNewVideoStreamFormatInfo *pVideoStreamInfo,
                                                OMX_U32 streamIndex)
{
    NvU32 i = 0, ImageSize = 0;
    NvError status = NvError_Force32;
    NvBool UseAlignment = NV_TRUE;
    NvBool clearYUVsurface = NV_TRUE;

    if (pVideoStreamInfo->InterlaceStream == OMX_TRUE)
        clearYUVsurface = NV_FALSE;

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
            &ImageSize, UseAlignment,NvOsMemAttribute_Uncached, clearYUVsurface);

        if (status != NvSuccess && NvMMBlockType_DecSuperJPEG == pData->oBlockType)
        {
            pData->oPorts[streamIndex].nHeight = (pVideoStreamInfo->Height + 7) >> 3;
            pData->oPorts[streamIndex].nWidth = (pVideoStreamInfo->Width + 7) >> 3;
            status = NvxVideoSurfaceAlloc(pData->hRmDevice,
                pData->oPorts[streamIndex].pBuffers[i], i,
                pData->oPorts[streamIndex].nWidth, pData->oPorts[streamIndex].nHeight,
                pVideoStreamInfo->ColorFormat, pVideoStreamInfo->Layout,
                &ImageSize, UseAlignment,NvOsMemAttribute_Uncached, NV_TRUE);
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
        {
            NvMMEventBufferAllocationFailedInfo info;
            NV_DEBUG_PRINTF(("Sending NvMMEvent_BufferAllocationFailed\n!"));
            NvOsMemset(&info, 0, sizeof(NvMMEventBufferAllocationFailedInfo));
            info.structSize = sizeof(NvMMEventBufferAllocationFailedInfo);
            info.event = NvMMEvent_BufferAllocationFailed;

            pData->TransferBufferToBlock(pData->hBlock,
                streamIndex,
                NvMMBufferType_StreamEvent,
                info.structSize, &info);
        }
       return status;
    }
}

static void HandleNewStreamFormat(SNvxNvMMTransformData *pData,
                                  NvU32 streamIndex, NvU32 eventType,
                                  void *eventInfo)
{
    NvError status = NvSuccess;
    NvU32 i=0, NumSurfaces;
    NvMMEventNewVideoStreamFormatInfo *NewStreamInfo;
    NvU32 nExtraBuffers = 2;
    SNvxNvMMPort *pPort;
    OMX_BOOL bNeedNewFormat = OMX_FALSE, bNeedUnpause = OMX_TRUE;
    OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
    NvxBufferPlatformPrivate *pPrivateData = NULL;

    NVMMVERBOSE(("New stream format: %d\n", streamIndex));

    if (pData->bAudioTransform && eventInfo && (pData->oPorts[streamIndex].nType == TF_TYPE_INPUT))
    {
        if ((pData->oBlockType == NvMMBlockType_DecAAC) || (pData->oBlockType == NvMMBlockType_DecEAACPLUS)) {

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

    NewStreamInfo = (NvMMEventNewVideoStreamFormatInfo *)eventInfo;
    pData->nColorFormat = NewStreamInfo->ColorFormat;
    pPort = &(pData->oPorts[streamIndex]);

    pPort->bNvMMClientAlloc = OMX_TRUE;
    pPort->bHasSize = OMX_TRUE;

    if (NewStreamInfo->InterlaceStream == NV_TRUE)
    {
        if (((NvMMBlockType_DecH264 == pData->oBlockType) || (NvMMBlockType_DecMPEG2 == pData->oBlockType)) && (pData->InterlaceThreadActive == OMX_FALSE))
        {
            NvError status = NvSuccess;
            NewStreamInfo->Width = (NewStreamInfo->Width + 127) &(~127); //Width should be 128 aligned for de interlacing
            NewStreamInfo->Layout = NvRmSurfaceLayout_Pitch;
            NewStreamInfo->NumOfSurfaces += 4;
            pData->vmr.frameWidth = NewStreamInfo->Width;
            pData->vmr.frameHeight= NewStreamInfo->Height;
            pData->InterlaceStream = OMX_TRUE;
            pData->InterlaceThreadActive = OMX_TRUE;
            status = NvMMQueueCreate(&pData->pReleaseOutSurfaceQueue, MAX_NVMM_BUFFERS, sizeof(NvMMBuffer*), NV_FALSE);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Deinterlace : unable to create the queue for output buffers from decoder \n"));
                goto fail;
            }
            status = NvOsSemaphoreCreate(&(pData->TvmrInitDone), 0);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Deinterlace : unable to create the semaphore for TvmrInitDone \n"));
                goto fail;
            }
            status = NvOsSemaphoreCreate(&(pData->InpOutAvail), 0);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Deinterlace : unable to create the semaphore for InpOutAvail \n"));
                goto fail;
            }
            status = NvOsSemaphoreCreate(&(pData->DeintThreadPaused), 0);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Deinterlace : unable to create the semaphore for DeintThreadPaused \n"));
                goto fail;
            }
            status = NvOsThreadCreate(NvxDeinterlaceThread, pData, &pData->hDeinterlaceThread);
            if (status != NvSuccess)
            {
                pData->InterlaceThreadActive = OMX_FALSE;
                NV_DEBUG_PRINTF(("Deinterlace : unable to create the deinterlace thread \n"));
                goto fail;
            }
            status = NvOsSemaphoreWaitTimeout(pData->TvmrInitDone, 2000);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Deinterlace :  TVMRInit is not proper \n"));
                goto fail;
            }
        }
        else if ((NvMMBlockType_DecH264 == pData->oBlockType) || (NvMMBlockType_DecMPEG2 == pData->oBlockType))
        {
            NvOsDebugPrintf("DRC is not Handled for Interlaced stream\n");
            NvxSendEvent(pData->pParentComp, OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            return;
        }
    }

    if ((NvMMBlockType_DecSuperJPEG == pData->oBlockType) &&
        (!NvOsStrcmp(pData->pParentComp->sComponentRoles[0], "image_decoder.jpeg")))
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
                    NvMMVideoDecAttribute_Decode_Thumbnail, 0,
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
        pPort->nHeight = NewStreamInfo->Height;
        pPort->nWidth = NewStreamInfo->Width;
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

    if (NumSurfaces > MAX_NVMM_BUFFERS)
        NumSurfaces = MAX_NVMM_BUFFERS;

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

    NvOsDebugPrintf("Allocating new output: %dx%d (x %d)\n",
                pPort->nWidth, pPort->nHeight, NumSurfaces);
    pPort->nBufsTot = 0;
    NewStreamInfo->NumOfSurfaces = NumSurfaces;


    pData->hBlock->SetState(pData->hBlock, NvMMState_Paused);
    pData->bPausing = OMX_TRUE;

    pPort->outRect.nLeft = 0;
    pPort->outRect.nTop = 0;
    pPort->outRect.nWidth = pPort->nWidth;
    pPort->outRect.nHeight = pPort->nHeight;

    if (pData->InterlaceStream == OMX_TRUE || !pPort->bUsesANBs)
    {
#ifdef NV_DEF_USE_PITCH_MODE
        NewStreamInfo->Layout = NvRmSurfaceLayout_Pitch;
#endif

        status = NvxNvMMTransformAllocateSurfaces(pData, NewStreamInfo,
                                                  streamIndex);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("Unable to allocate input buffers"));
            goto fail;
        }

        for (i = 0; i < NumSurfaces; i++)
        {
            pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                                         NvMMBufferType_Payload,
                                         sizeof(NvMMBuffer),
                                         pPort->pBuffers[i]);
            pPort->nBufsTot++;
        }
    }

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
            case NvMMVideoDecColorFormat_YUV422:
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV422;
                break;
            case NvMMVideoDecColorFormat_YUV422T:
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV422R;
                break;
            case NvMMVideoDecColorFormat_YUV444:
            case NvMMVideoDecColorFormat_GRAYSCALE:
            default:
                //We dont support YUV444 in graphics, hence by default setting it to YUV420
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)NVGR_PIXEL_FORMAT_YUV420;
                break;
        }
#endif

        if (pData->InterlaceStream == NV_TRUE)
        {
            pPortDef->nBufferCountActual = NUM_SURFACES;
            pPortDef->nBufferCountMin = NUM_SURFACES;
            pOMXPort->nMinBufferCount = NUM_SURFACES;
            pOMXPort->nMaxBufferCount = NUM_SURFACES + 2;
            bNeedNewFormat = OMX_TRUE;
            pPort->bBlockANBImport = OMX_TRUE;
        }
        else
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
                if (ImportAllANBs(pData->pParentComp, pData, streamIndex,
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
        if (NewStreamInfo->ColorFormat == NvMMVideoDecColorFormat_YUV422)
        {
            pPort->pOMXPort->oPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV422Planar;
            bNeedNewFormat = OMX_TRUE;
        }
        if (pData->InterlaceStream == NV_TRUE)
        {// Surfaces need for TVMR for deinterlacing incase of non-ANBs
            NvU32 ImageSize = 0;
            NvBool UseAlignment = NV_TRUE;
            NvError status = NvSuccess;
            for (i=0 ; i < TOTAL_NUM_SURFACES; i++)
            {
                pData->vmr.pVideoSurf[i] = NvOsAlloc(sizeof(NvMMBuffer));
                if (!pData->vmr.pVideoSurf[i])
                {
                    NV_DEBUG_PRINTF(("Unable to allocate the memory for the Output buffer structure !"));
                    status = NvError_InsufficientMemory;
                    goto fail;
                }
                NvOsMemset(pData->vmr.pVideoSurf[i], 0, sizeof(NvMMBuffer));
                status = NvxVideoSurfaceAlloc(pData->hRmDevice,
                pData->vmr.pVideoSurf[i], i,
                pData->oPorts[streamIndex].nWidth, pData->oPorts[streamIndex].nHeight,
                NvMMVideoDecColorFormat_YUV420,
                NewStreamInfo->Layout, &ImageSize, UseAlignment,NvOsMemAttribute_Uncached, NV_TRUE);
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
                pData->vmr.Renderstatus[i] = FRAME_VMR_FREE;
            }
        }
        // assign the surface width/height to port for configuration
        if (pPort->pBuffers[0])
        {
            pPort->nWidth = (pPort->pBuffers[0])->Payload.Surfaces.Surfaces[0].Width;
            pPort->nHeight = (pPort->pBuffers[0])->Payload.Surfaces.Surfaces[0].Height;
        }
    }

    if (pData->nNvMMProfile & 0x10)
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
        pData->hBlock->SetState(pData->hBlock, NvMMState_Running);
        pData->bPausing = OMX_FALSE;
    }
    return ;

fail:
    {
        NvU32 j = 0;
        for (j = 0; j <= i; j++)
        {
            if (pData->vmr.pVideoSurf[j])
            {
                NvxVideoDecSurfaceFree(pData->vmr.pVideoSurf[j]);
                NvOsFree(pData->vmr.pVideoSurf[j]);
                pData->vmr.pVideoSurf[j] = NULL;
            }
        }
        // related for deinterlacing
        if (pData->InterlaceStream == OMX_TRUE)
        {
            if (pData->DeintThreadPaused)
            {
                NvOsSemaphoreDestroy(pData->DeintThreadPaused);
                pData->DeintThreadPaused = NULL;
            }
            if (pData->InpOutAvail)
            {
                NvOsSemaphoreDestroy(pData->InpOutAvail);
                pData->InpOutAvail = NULL;
            }
            if (pData->TvmrInitDone)
            {
                NvOsSemaphoreDestroy(pData->TvmrInitDone);
                pData->TvmrInitDone = NULL;
            }
            if (pData->pReleaseOutSurfaceQueue)
            {
                NvMMQueueDestroy(&pData->pReleaseOutSurfaceQueue);
                pData->pReleaseOutSurfaceQueue = NULL;
            }
            if (pData->hDeinterlaceThread)
            {
                if (pData->DeinterlaceThreadClose == OMX_FALSE)
                    pData->DeinterlaceThreadClose = OMX_TRUE;
                NvOsThreadJoin(pData->hDeinterlaceThread);
                pData->hDeinterlaceThread = NULL;
            }
            if (pData->pParentComp)
            {
                    OMX_ERRORTYPE eReturnVal = OMX_ErrorInsufficientResources;
                    NvxComponent *pNvComp = pData->pParentComp;
                    NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
            }
        }
        {
            NvMMEventBufferAllocationFailedInfo info;
            NV_DEBUG_PRINTF(("Sending NvMMEvent_BufferAllocationFailed\n!"));
            info.structSize = sizeof(NvMMEventBufferAllocationFailedInfo);
            info.event = NvMMEvent_BufferAllocationFailed;

            pData->TransferBufferToBlock(pData->hBlock,
                streamIndex,
                NvMMBufferType_StreamEvent,
                info.structSize, &info);
        }
       return ;
    }
}

static void HandleStreamEvent(SNvxNvMMTransformData *pData, NvU32 streamIndex,
                              NvU32 eventType, void *eventInfo)
{
    switch (eventType)
    {
    case NvMMEvent_NewBufferConfiguration:
        NVMMVERBOSE(("NewBufferConfiguration stream=%i\n", streamIndex));
        if (streamIndex < pData->nNumStreams)
        {
            if (NvMMBlockType_DigitalZoom == pData->oBlockType)
            {
                NvMMNewBufferConfigurationInfo *pNewBCInfo = (NvMMNewBufferConfigurationInfo*)eventInfo;
                pData->oPorts[streamIndex].nWidth = pNewBCInfo->format.videoFormat.SurfaceDescription[0].Width;
                pData->oPorts[streamIndex].nHeight = pNewBCInfo->format.videoFormat.SurfaceDescription[0].Height;
            }
            else if (NvMMBlockType_SrcCamera == pData->oBlockType)
            {
                pData->oPorts[streamIndex].nBufsTot = ((NvMMNewBufferConfigurationInfo *)eventInfo)->numBuffers;
            }
            NvOsSemaphoreSignal(pData->oPorts[streamIndex].buffConfigSem);
        }
        break;
    case NvMMEvent_NewBufferConfigurationReply:
        {
            //pContext->bBufConfigReply[streamIndex] =
            //    ((NvMMNewBufferConfigurationReplyInfo *)eventInfo)->bAccepted;
            NVMMVERBOSE(("NewBufferConfigurationReply stream=%i reply=%c\n",
                streamIndex,
                ((NvMMNewBufferConfigurationReplyInfo *)eventInfo)->bAccepted ?'Y':'N'));
            if (streamIndex < pData->nNumStreams)
                NvOsSemaphoreSignal(pData->oPorts[streamIndex].buffConfigSem);
            break;
        }
    case NvMMEvent_NewBufferRequirements:
        NVMMVERBOSE(("NvMMEvent_NewBufferRequirements stream=%i\n",
            streamIndex));
        if (streamIndex < pData->nNumStreams)
        {
            NvOsMemcpy(&(pData->oPorts[streamIndex].bufReq), eventInfo,
                sizeof(NvMMNewBufferRequirementsInfo));
            NvOsSemaphoreSignal(pData->oPorts[streamIndex].buffReqSem);
        }
        break;
    case NvMMEvent_StreamShutdown:
        //pContext->bStreamShutting[streamIndex] = NV_TRUE;
        NVMMVERBOSE(("StreamShutdown\n"));
        break;
    case NvMMEvent_StreamEnd:
        NVMMVERBOSE(("StreamEndEvent\n"));
        if ((pData->oPorts[streamIndex].nType == TF_TYPE_OUTPUT) && (!pData->bFlushing))
        {
            pData->oPorts[streamIndex].bEOS = OMX_TRUE;
            NVMMVERBOSE(("End of Output Stream\n"));
        }
        break;

    case NvMMEvent_ProfilingData:
        {
            NvMMEventProfilingData *pprofilingData = (NvMMEventProfilingData *)eventInfo;
            NV_DEBUG_PRINTF(("\nProfiling Data Event"));
            if (pprofilingData)
                NV_DEBUG_PRINTF(("\nInstanceName: %s", pprofilingData->InstanceName));
            NV_DEBUG_PRINTF(("\nTotal Decode Time : %d", pprofilingData->AccumulatedTime));
            NV_DEBUG_PRINTF(("\nNumber of Frames  : %d", pprofilingData->NoOfTimes));
            NV_DEBUG_PRINTF(("\nStarvation Count  : %d", pprofilingData->inputStarvationCount));
        }
        break;

    case NvMMEvent_NewStreamFormat:
        {
            HandleNewStreamFormat(pData, streamIndex, eventType, eventInfo);
        }
        break;
    case NvMMEvent_BlockError:
        {
            NvMMBlockErrorInfo *pBlockErr = (NvMMBlockErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;

            NVMMVERBOSE(("StreamBlockErrorEvent\n"));

            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                eReturnVal = (pBlockErr) ? NvxTranslateErrorCode(pBlockErr->error) : OMX_ErrorUndefined;
                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
            }
        }
        break;

    case NvMMEvent_StreamError:
        {
            NvMMStreamErrorInfo *pStreamErr = (NvMMStreamErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;

            NVMMVERBOSE(("StreamStreamErrorEvent\n"));
            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                eReturnVal = NvxTranslateErrorCode(pStreamErr->error);
                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, 0);
            }
        }
        break;

    case NvMMEvent_Unknown:
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
    pOutHdr->nFlags = data.nFlags;

    pOutPort->bSendEvents = OMX_TRUE;
    pOutPort->uMetaDataCopyCount = 0;
    pOutPort->pMetaDataSource = data.pMetaDataSource; /* this is for marks, so we can send on last output */

    pOutPort->pMetaDataSource->uMetaDataCopyCount++;
}

static NvError
NvxNvMMTransformReturnEmptyInput(
                                 void *pContext,
                                 NvU32 streamIndex,
                                 NvMMBufferType BufferType,
                                 NvU32 BufferSize,
                                 void *pBuffer)
{
    SNvxNvMMTransformData *pData = (SNvxNvMMTransformData *)pContext;
    NvxComponent *pComp = pData->pParentComp;

    if (BufferType == NvMMBufferType_StreamEvent)
    {
        NvMMEventType eventType = ((NvMMStreamEventBase *)pBuffer)->event;
        HandleStreamEvent(pData, streamIndex, eventType, pBuffer);
    }

    if (BufferType == NvMMBufferType_Payload)
    {
        NvMMBuffer *PayloadBuffer = (NvMMBuffer *)pBuffer;
        SNvxNvMMPort *pPort = &(pData->oPorts[streamIndex]);
        int id = PayloadBuffer->BufferID;

        NvxMutexLock(pComp->hWorkerMutex);

        if (pPort->pBuffers[id])
            *(pPort->pBuffers[id]) = *PayloadBuffer;

        if (pPort->pOMXBufMap[id])
        {
            if (pData->bProfile)
            {
                pData->llLastTBTB = NvOsGetTimeUS();
                NVMMVERBOSE(("%s: ReturnEmpty: (%p) at %f\n",
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

            if (OMX_TRUE == pPort->bEmbedNvMMBuffer && !pPort->bEnc2DCopy && !pPort->bUsesAndroidMetaDataBuffers)
            {
                NvOsMemcpy(pPort->pOMXBufMap[id]->pBuffer, PayloadBuffer, sizeof(NvMMBuffer));
                pPort->pOMXBufMap[id]->nFlags |= OMX_BUFFERFLAG_NV_BUFFER;
                pPort->pOMXBufMap[id]->nFilledLen = sizeof(NvMMBuffer);
            }

            NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pOMXBufMap[id]);
            pPort->pOMXBufMap[id] = NULL;
            if (!pPort->bSkipWorker)
                NvxMutexUnlock(pPort->oMutex);
        }
        // if this buffer is not registered in pOMXBufMap[], it does not come from client but from inside.
        else
        {
            pPort->pNvMMBufMap[id] = PayloadBuffer;
        }

        if (pPort->bSkipWorker && (pData->bFlushing || pData->bPausing))
        {
            NvxNvMMSendNextInputBufferBack(pData, streamIndex);
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
                        NvMMBlockHandle hNvMMBlock = pData->hBlock;
                        NvMMState eState = NvMMState_Running;

                        if (!pData->bPausing)
                            NV_DEBUG_PRINTF(("comp %s : Input (port %d) taking a very long time\n",
                                            pData->sName, streamIndex));

                        hNvMMBlock->GetState(hNvMMBlock, &eState);
                        if (eState == NvMMState_Stopped)
                        {
                            NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
                            return NvSuccess;
                        }

                        if (eState == NvMMState_Paused)
                        {
                            // Transfer the buffer back to block
                            NvxNvMMSendNextInputBufferBack(pData, streamIndex);
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
            NvxNvMMProcessInputBuffer(pData, streamIndex, NULL);
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

NvError
NvxNvMMTransformDeliverFullOutput(
                                  void *pContext,
                                  NvU32 streamIndex,
                                  NvMMBufferType BufferType,
                                  NvU32 BufferSize,
                                  void *pBuffer)
{
    SNvxNvMMTransformData *pData = (SNvxNvMMTransformData *)pContext;
    SNvxNvMMPort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvxComponent *pComp = pData->pParentComp;

    if (BufferType == NvMMBufferType_StreamEvent)
    {
        NvMMEventType eventType = ((NvMMStreamEventBase *)pBuffer)->event;
        HandleStreamEvent(pData, streamIndex, eventType, pBuffer);
    }

    NvxMutexLock(pComp->hWorkerMutex);

    if (pPort->bEOS && !pPort->bSentEOS && !pPort->bError && !pData->InterlaceStream)
    {
        int attempts = 0;

        while (!pPortOut->pCurrentBufferHdr)
        {
            NvxPortGetNextHdr(pPortOut);

            if (!pPortOut->pCurrentBufferHdr)
            {
                NvxMutexUnlock(pComp->hWorkerMutex);

                attempts++;
                if (attempts > 20 && !pData->bPausing)
                {
                    if (OMX_StateIdle == pData->pParentComp->eState)
                    {
                        return NvSuccess;
                    }
                }

                if (pData->bStopping || pPort->bAborting || pData->bFlushing)
                {
                    return NvSuccess;
                }

                NvOsSleepMS(10);

                NvxMutexLock(pComp->hWorkerMutex);
            }
        }

        pPortOut->pCurrentBufferHdr->nFilledLen = 0;
        pPortOut->pCurrentBufferHdr-> nTimeStamp = pData->LastTimeStamp;
        pPortOut->pCurrentBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
        NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
        NVMMVERBOSE(("delivering EOS buffer\n"));
        NvDebugOmx(("Block %s: Delivering EOS buffer", pData->sName));
        pPort->bSentEOS = OMX_TRUE;

        if (pData->bProfile)
        {
            pData->llFinalOutput = NvOsGetTimeUS();
            NVMMVERBOSE(("%s: DeliverFullOutput: (%p) at %f\n",
                pData->sName,
                pPortOut->pCurrentBufferHdr,
                pData->llFinalOutput / 1000000.0));
        }

        if (pData->bSequence)
        {
            // for next frame in the sequence
            pPort->bEOS = OMX_FALSE;
            pPort->bSentEOS = OMX_FALSE;
            goto done;
        }
    }

    if (BufferType == NvMMBufferType_Payload)
    {
        NvMMBuffer *PayloadBuffer = (NvMMBuffer *)pBuffer;
        int id = (int) PayloadBuffer->BufferID;
        if (pPort->pBuffers[id])
            *(pPort->pBuffers[id]) = *PayloadBuffer;

        if (pData->bFlushing)
        {
            pPort->bBufNeedsFlush[id] = OMX_TRUE;
            goto done;
        }
    }
    if (BufferType == NvMMBufferType_Payload && !pPort->bEOS && !pPort->bAborting)
    {
        if (pData->InterlaceStream == OMX_TRUE)
        {
            NvMMBuffer *PayloadBuffer = (NvMMBuffer *)pBuffer;
            if (!pData->bFlushing)
            {
                NvMMQueueEnQ(pData->pReleaseOutSurfaceQueue, &pPort->pBuffers[PayloadBuffer->BufferID], 2000);
                if (pData->StopDeinterlace == OMX_TRUE)
                    pData->StopDeinterlace = OMX_FALSE;

                NvOsSemaphoreSignal(pData->InpOutAvail);
            }
            else
            {
                pPort->bBufNeedsFlush[PayloadBuffer->BufferID] = OMX_TRUE;
            }
        }
        else
        {
            if (!pPort->bEOS && !pPort->bAborting)
                NvxNvMMTransformDeliverFullOutputInternal(pData, pBuffer, streamIndex);
        }
    }
    else
    {
        if ((pData->StopDeinterlace == OMX_TRUE) && (pData->InterlaceStream == OMX_TRUE) && (pPort->bEOS))
        {
            pData->StopDeinterlace = OMX_FALSE;
            NvOsSemaphoreSignal(pData->InpOutAvail);
        }
    }

done:
    NvxMutexUnlock(pComp->hWorkerMutex);
    return NvSuccess;
}

static NvError NvxNvMMTransformDeliverFullOutputInternal(SNvxNvMMTransformData *pData, NvMMBuffer* output, NvU32 streamIndex)
{
    SNvxNvMMPort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvxComponent *pComp = pData->pParentComp;

    {
        NvMMBuffer *PayloadBuffer = (NvMMBuffer *)output;
        int id = (int) PayloadBuffer->BufferID;
        OMX_BOOL bNoData = OMX_FALSE;
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        OMX_BOOL bCanReturnBufferToBlock = OMX_TRUE;

        if (!pPort->pBuffers[id])
        {
            goto done;
        }

        // WA: Assert if no buffer available
        while (!pPortOut->pCurrentBufferHdr)
        {
            NvxPortGetNextHdr(pPortOut);

            if (!pPortOut->pCurrentBufferHdr)
            {
                NvxMutexUnlock(pComp->hWorkerMutex);
                if (pData->bStopping || pPort->bAborting || pData->bFlushing)
                 {
                     if (pData->bFlushing)
                         pPort->bBufNeedsFlush[id] = OMX_TRUE;
                     NvxMutexLock(pComp->hWorkerMutex);
                     return NvSuccess;
                 }

                 NvOsSleepMS(10);
                 NvxMutexLock(pComp->hWorkerMutex);
            }
        }

        pBufferHdr = pPortOut->pCurrentBufferHdr;

        if (pData->bProfile)
        {
            NVMMVERBOSE(("%s:%d Got full (len %d) at %f, ts: %f\n",
                pData->sName, streamIndex,
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
                if ((PayloadBuffer->PayloadInfo.BufferMetaDataType ==
                    NvMMBufferMetadataType_DigitalZoom) ||
                    (PayloadBuffer->PayloadInfo.BufferMetaDataType ==
                    NvMMBufferMetadataType_EXIFData))
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

                if (PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_StereoEnable)
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

                xWidth = NV_SFX_FP_TO_FX(DispRes->Width * 1.0 /
                                         pPort->outRect.nWidth);
                xHeight = NV_SFX_FP_TO_FX(DispRes->Height * 1.0 /
                                          pPort->outRect.nHeight);

                if (xWidth != pPort->xScaleWidth ||
                    xHeight != pPort->xScaleHeight)
                {
                    pPort->xScaleWidth = xWidth;
                    pPort->xScaleHeight = xHeight;
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
                        NV_TRUE, NULL, 0);
                }
#if USE_ANDROID_NATIVE_WINDOW
                else if (pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T ||
                        pPrivateData->eType == NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T)
                {
                    // If this flag got set earlier, call older function
                    if (!pPort->bWillCopyToANB)
                    {
                        ExportANB(pData->pParentComp, PayloadBuffer,
                              &pBufferHdr, pPortOut, OMX_FALSE);
                    }
                    else
                    {
                        CopyNvxSurfaceToANB(pData->pParentComp,
                                            PayloadBuffer,
                                            pBufferHdr);
                    }
                }
#endif
                else if (OMX_TRUE == pPort->bEmbedNvMMBuffer)
                {
                    // Embed NvMM Buffer into OMX buffer.
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
                                NvxCopyMMBufferToOMXBuf(PayloadBuffer, NULL, pPortOut->pCurrentBufferHdr);
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

            if (PayloadBuffer->Payload.Ref.pMem)
            {
                /* CacheMaint if bInSharedMemory == NV_TRUE and MemoryType == WriteCombined */
                if (pPort->bInSharedMemory == NV_TRUE && PayloadBuffer->Payload.Ref.MemoryType == NvMMMemoryType_WriteCombined)
                    NvRmMemCacheMaint(PayloadBuffer->Payload.Ref.hMem, (void *) PayloadBuffer->Payload.Ref.pMem, size, NV_FALSE, NV_TRUE);

                NvOsMemcpy(pBufferHdr->pBuffer,
                    (void *)((OMX_U8 *)PayloadBuffer->Payload.Ref.pMem + PayloadBuffer->Payload.Ref.startOfValidData),
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

        if (NvMMBlockType_EncJPEG == pData->oBlockType &&
            PayloadBuffer->PayloadInfo.BufferMetaData.JpegEncMetadata.bFrameEnd)
        {
            pBufferHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
            pBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
        }

        if (PayloadBuffer->Payload.Ref.sizeOfValidDataInBytes > 0 &&
            PayloadBuffer->PayloadInfo.BufferMetaDataType == NvMMBufferMetadataType_Audio)
        {
            NvOsMemcpy(&(pPort->audioMetaData), &(PayloadBuffer->PayloadInfo.BufferMetaData.AudioMetadata), sizeof(NvMMAudioMetadata));
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
            pBufferHdr->nTimeStamp =  -1;
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
            pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                NvMMBufferType_Payload,
                sizeof(NvMMBuffer),
                pPort->pBuffers[id]);
        }
        else
        {
            if (NvMMBlockType_EncAAC == pData->oBlockType ||
                NvMMBlockType_SuperParser == pData->oBlockType)
            {
                if (PayloadBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag)
                {
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
                    if (NvMMBlockType_SuperParser == pData->oBlockType)
                    {
                        NvOsDebugPrintf("NVX_StreamChangeEvent: %s[%d]",
                            __FUNCTION__, __LINE__);
                        NvxSendEvent(pData->pParentComp,
                           NVX_StreamChangeEvent,
                           0,
                           0,
                           NULL);
                    }

                }
                if (PayloadBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame == NV_TRUE)
                {
                    pBufferHdr->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
                }
            }

            if (pBufferHdr->pPlatformPrivate)
            {
                NvxBufferPlatformPrivate *pPrivateData = (NvxBufferPlatformPrivate *)pBufferHdr->pPlatformPrivate;
                pPrivateData->pData = (void *)pData;
            }

            pPort->pOMXBufMap[id] = pBufferHdr;
            NvxPortDeliverFullBuffer(pPortOut, pBufferHdr);
            pPort->bFirstOutBuffer = OMX_FALSE;
            NvxPortGetNextHdr(pPortOut);
            if (pData->bVideoTransform)
            {
                if (pData->nNvMMProfile & 0x10)
                {
                    pData->NumBuffersWithBlock--;
                    NvOsDebugPrintf("%s: Number of buffers with Block : %d \n",
                        pData->sName, pData->NumBuffersWithBlock);
                }
            }

            if (!pPortOut->hTunnelComponent &&
                !pPort->bEmbedNvMMBuffer && !pPort->bUsesANBs &&
                pData->bVideoTransform && bCanReturnBufferToBlock)
            {
                // Returning NvMM buffer back to block
                // as it is completly copied to application buffer.
                NvMMBuffer *SendBackBuf = pPort->pBuffers[id];
                pPort->pOMXBufMap[id] = NULL;
                pPort->bBufNeedsFlush[id] = OMX_FALSE;
                if (pData->InterlaceStream == OMX_TRUE)
                {
                    pData->vmr.Renderstatus[id] = FRAME_VMR_FREE;
                    NvOsSemaphoreSignal(pData->InpOutAvail);
                }
                else
                    pData->TransferBufferToBlock(pData->hBlock, streamIndex,
                        NvMMBufferType_Payload,
                        sizeof(NvMMBuffer), SendBackBuf);

                if (pData->nNvMMProfile & 0x10)
                {
                    pData->NumBuffersWithBlock++;
                }
            }
        }
    }

done:
    return NvSuccess;
}

OMX_ERRORTYPE NvxNvMMTransformFreeBuffer(SNvxNvMMTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream)
{
    int i = 0;
    SNvxNvMMPort *pPort = &(pData->oPorts[nStream]);

    if (!pBuffer || !pPort->bUsesANBs)
        return OMX_ErrorNone;

#if USE_ANDROID_NATIVE_WINDOW
    if (OMX_ErrorNone != FreeANB(pData->pParentComp, pData,
                                 nStream, pBuffer))
    {
        return OMX_ErrorNone;
    }
#endif

    for (; i < MAX_NVMM_BUFFERS; i++)
    {
        if (pPort->pOMXBufMap[i] == pBuffer)
        {
            pPort->pOMXBufMap[i] = NULL;
            pPort->bBufNeedsFlush[i] = OMX_FALSE;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxNvMMTransformFillThisBuffer(SNvxNvMMTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream)
{
    OMX_BOOL bFound = OMX_FALSE;
    int i = 0;
    SNvxNvMMPort *pPort = &(pData->oPorts[nStream]);
    NvxBufferPlatformPrivate *pPrivateData = (NvxBufferPlatformPrivate *)pBuffer->pPlatformPrivate;

    if (!pBuffer)
        return OMX_ErrorNone;

    if ((!pPrivateData || ((pPrivateData->eType != NVX_BUFFERTYPE_NEEDRMSURFACE) &&
        (pPrivateData->eType != NVX_BUFFERTYPE_HASRMSURFACE))) && (!pPort->bUsesANBs))
    {
        if (pPort->pOMXPort && !pPort->pOMXPort->hTunnelComponent && pData->bVideoTransform
            && pPort->bEmbedNvMMBuffer == OMX_FALSE)
        {
            bFound = OMX_TRUE;
            goto Exit;
        }
    }

    if (pPort->bUsesANBs && pData->bVideoTransform)
    {
        if (!pData->VideoStreamInfo.NumOfSurfaces || pPort->bBlockANBImport)
        {
            bFound = OMX_TRUE;
            goto Exit;
        }
#if USE_ANDROID_NATIVE_WINDOW
        if (OMX_ErrorNone != ImportANB(pData->pParentComp, pData,
                                       nStream, pBuffer, 0, 0))
        {
            goto Exit;
        }
#endif
        for (; i < MAX_NVMM_BUFFERS; i++)
        {
            if (pPort->pOMXBufMap[i] == pBuffer)
            {
                pPort->pOMXBufMap[i] = NULL;
                pPort->bBufNeedsFlush[i] = OMX_FALSE;
            }
        }
        bFound = OMX_TRUE;
        goto Exit;
    }

    for (; i < MAX_NVMM_BUFFERS; i++)
    {
        if (pPort->pOMXBufMap[i] == pBuffer || pPort->bBufNeedsFlush[i])
        {
            NvMMBuffer *SendBackBuf = pPort->pBuffers[i];
            if (SendBackBuf == NULL)
            {
                NvOsDebugPrintf("Got NULL NvMM buffer in NvxNvMMTransformFillThisBuffer\n");
                return OMX_ErrorNone;
            }
            if(pBuffer->nFlags & OMX_BUFFERFLAG_NV_BUFFER)
            {
                // we have an NvMM Buffer embedded into OMXBuffer.
                if (pBuffer->nFilledLen == sizeof(NvMMBuffer))
                {
                    NvMMBuffer Embedbuf;
                    NvOsMemcpy(&Embedbuf, pBuffer->pBuffer, sizeof(NvMMBuffer));
                    // swap the surface
                    SendBackBuf->Payload.Surfaces = Embedbuf.Payload.Surfaces;
                    SendBackBuf->PayloadInfo = Embedbuf.PayloadInfo;
                }
            }
            pPort->pOMXBufMap[i] = NULL;
            pPort->bBufNeedsFlush[i] = OMX_FALSE;

            if ((!pPort->bUsesANBs) && (pData->InterlaceStream == OMX_TRUE))
            {
                pData->vmr.Renderstatus[i] = FRAME_VMR_FREE;
                NvOsSemaphoreSignal(pData->InpOutAvail);
            }
            else
                pData->TransferBufferToBlock(pData->hBlock, nStream,
                            NvMMBufferType_Payload,
                            sizeof(NvMMBuffer), SendBackBuf);

            bFound = OMX_TRUE;
            if (pData->bVideoTransform)
            {
                if (pData->nNvMMProfile & 0x10)
                {
                    pData->NumBuffersWithBlock++;
                }
            }
            break;
        }
    }

Exit:
    if (bFound && (pData->bHasInputRunning || !pPort->bCanSkipWorker))
    {
        NvxWorkerTrigger(&(pPort->pOMXPort->pNvComp->oWorkerData));
    }

    return (bFound) ? OMX_ErrorNone : OMX_ErrorBadParameter;
}

void NvxNvMMTransformPortEventHandler(SNvxNvMMTransformData *pData,
                                      int nPort, OMX_U32 uEventType)
{
    NvU32 i;
    SNvxNvMMPort *pPort = &(pData->oPorts[nPort]);
    NvxComponent *pComp = pData->pParentComp;

    pPort->bBlockANBImport = OMX_FALSE;
    if(pData->bReconfigurePortBuffers == OMX_TRUE && uEventType == 1) //(uEventType == 1)-> Port Enable
    {
        NvxNvMMTransformReconfigurePortBuffers(pData);
    }

    if(pData->bReconfigurePortBuffers == OMX_TRUE && uEventType == 0) //(uEventType == 0)-> Port Disable
   {
        pData->bStopping = OMX_TRUE;
        NvxMutexUnlock(pComp->hWorkerMutex);
        pData->hBlock->SetState(pData->hBlock, NvMMState_Stopped);
        NvOsSemaphoreWait(pData->stateChangeDone);
        NvxMutexLock(pComp->hWorkerMutex);
        pData->bStopping = OMX_FALSE;

        pPort->bAborting = OMX_TRUE;
        NvxMutexUnlock(pComp->hWorkerMutex);
        pData->hBlock->AbortBuffers(pData->hBlock, nPort);
        NvOsSemaphoreWait(pData->oPorts[nPort].blockAbortDone);
        NvxMutexLock(pComp->hWorkerMutex);

        pPort->bAborting = OMX_FALSE;

        if(!pPort->bUsesANBs)
            NvxNvMMTransformFreePortBuffers(pData, nPort);

        for (i = 0; i < MAX_NVMM_BUFFERS; i++)
        {
            if (pPort->pOMXBufMap[i])
            {
                pPort->pOMXBufMap[i] = NULL;
            }
        }

        NvxMutexUnlock(pComp->hWorkerMutex);
        pData->hBlock->SetState(pData->hBlock, NvMMState_Running);
        NvOsSemaphoreWait(pData->stateChangeDone);
        NvxMutexLock(pComp->hWorkerMutex);
        return;
    }
    if (uEventType && pData->bVideoTransform && pPort->nType == TF_TYPE_OUTPUT)
    {
        if (pData->bPausing)
            pData->hBlock->SetState(pData->hBlock, NvMMState_Running);
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

struct NvxEventExtensionEntry {
    NvU32 eNvMMEventType;
    NVX_EVENTTYPE eNvxEventType;
};

struct NvxEventExtensionEntry NvxEventExtensionTable[] = {
    { NvMMEventCamera_AlgorithmsLocked,                    NVX_EventCamera_AlgorithmsLocked },
    { NvMMEventCamera_AutoFocusAchieved,                   NVX_EventCamera_AutoFocusAchieved },
    { NvMMEventCamera_AutoExposureAchieved,                NVX_EventCamera_AutoExposureAchieved },
    { NvMMEventCamera_AutoWhiteBalanceAchieved,            NVX_EventCamera_AutoWhiteBalanceAchieved },
    { NvMMEventCamera_AutoFocusTimedOut,                   NVX_EventCamera_AutoFocusTimedOut },
    { NvMMEventCamera_AutoExposureTimedOut,                NVX_EventCamera_AutoExposureTimedOut },
    { NvMMEventCamera_AutoWhiteBalanceTimedOut,            NVX_EventCamera_AutoWhiteBalanceTimedOut },
    { NvMMEventCamera_CaptureAborted,                      NVX_EventCamera_CaptureAborted },
    { NvMMEventCamera_CaptureStarted,                      NVX_EventCamera_CaptureStarted },

    { NvMMEventCamera_StillCaptureReady,                   NVX_EventCamera_StillCaptureReady },
    { NvMMEventCamera_StillCaptureProcessing,              NVX_EventCamera_StillCaptureProcessing },
    { NvMMEventCamera_PreviewPausedAfterStillCapture,      NVX_EventCamera_PreviewPausedAfterStillCapture },
    { NvMMEventCamera_SensorModeChanged,                   NVX_EventCamera_SensorModeChanged },
    { NvMMEventCamera_EnterLowLight,                       NVX_EventCamera_EnterLowLight },
    { NvMMEventCamera_ExitLowLight,                        NVX_EventCamera_ExitLowLight },
    { NvMMEventCamera_EnterMacroMode,                      NVX_EventCamera_EnterMacroMode },
    { NvMMEventCamera_ExitMacroMode,                       NVX_EventCamera_ExitMacroMode },
    { NvMMEventCamera_PowerOnComplete,                     NVX_EventCamera_PowerOnComplete },
    { NvMMEventCamera_FocusStartMoving,                    NVX_EventCamera_FocusStartMoving },
    { NvMMEventCamera_FocusStopped,                        NVX_EventCamera_FocusStopped },

    { NvMMEvent_Force32,                                   NVX_EventMax},
};

static OMX_ERRORTYPE NvxGetEventExtension(NvU32 eNvMMEventType, NVX_EVENTTYPE* pNvxEventType)
{
    struct NvxEventExtensionEntry* pEntry;
    for (pEntry = NvxEventExtensionTable; NvMMEvent_Force32 != pEntry->eNvMMEventType; pEntry++)
    {
        if (pEntry->eNvMMEventType == eNvMMEventType)
        {
            *pNvxEventType = pEntry->eNvxEventType;
            return OMX_ErrorNone;
        }
    }
    return OMX_ErrorNotImplemented;
}

static NvError
NvxNvMMTransformSendBlockEventFunction(
                                       void *pContext,
                                       NvU32 eventType,
                                       NvU32 eventSize,
                                       void *eventInfo)
{
    SNvxNvMMTransformData *pData = (SNvxNvMMTransformData *)pContext;

    if (pData->EventCallback)
        pData->EventCallback(pData->pEventData, eventType,
        eventSize, eventInfo);

    switch (eventType)
    {
    case NvMMEvent_SetStateComplete:
        NVMMVERBOSE(("%s: SetStateComplete\n", pData->sName));
        NvOsSemaphoreSignal(pData->stateChangeDone);
        break;
    case NvMMEvent_StreamShutdown:
        break;

    case NvMMEvent_Unknown:
        break;

    case NvMMEvent_BlockClose:
        NVMMVERBOSE(("%s: BlockCloseDone\n", pData->sName));
        pData->blockCloseDone = OMX_TRUE;
        break;

    case NvMMEvent_SetAttributeComplete:
        NVMMVERBOSE(("%s: NvMMEvent_SetAttributeComplete\n", pData->sName));
        NvOsSemaphoreSignal(pData->SetAttrDoneSema);
        break;

    case NvMMEvent_BlockAbortDone:
        {
            NvMMEventBlockAbortDoneInfo *pBlockAortDoneInfo = (NvMMEventBlockAbortDoneInfo*)eventInfo;
            NvU32 streamIndex = pBlockAortDoneInfo->streamIndex;

            NVMMVERBOSE(("%s: BlockAbortDone, port %d\n", pData->sName, streamIndex));
            if (streamIndex < pData->nNumStreams)
                NvOsSemaphoreSignal(pData->oPorts[streamIndex].blockAbortDone);
            break;
        }

    case NvMMEventCamera_AutoFocusAchieved:
    case NvMMEventCamera_AutoExposureAchieved:
    case NvMMEventCamera_AutoWhiteBalanceAchieved:
    case NvMMEventCamera_AutoFocusTimedOut:
    case NvMMEventCamera_AutoExposureTimedOut:
    case NvMMEventCamera_AutoWhiteBalanceTimedOut:
    case NvMMEventCamera_StillCaptureReady:
    case NvMMEventCamera_StillCaptureProcessing:
    case NvMMEventCamera_PreviewPausedAfterStillCapture:
    case NvMMEventCamera_SensorModeChanged:
    case NvMMEventCamera_EnterLowLight:
    case NvMMEventCamera_ExitLowLight:
    case NvMMEventCamera_EnterMacroMode:
    case NvMMEventCamera_ExitMacroMode:
    case NvMMEventCamera_FocusStartMoving:
    case NvMMEventCamera_FocusStopped:
    case NvMMEventCamera_PowerOnComplete:
        NVMMVERBOSE(("%s: NvMMEventCamera_\n", pData->sName));
        if (pData->pParentComp)
        {
            NVX_EVENTTYPE eNvxEventType;
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMCameraEventInfo *pCameraEvent = (NvMMCameraEventInfo*)eventInfo;
            OMX_U64 TimeStamp = pCameraEvent ? pCameraEvent->TimeStamp / 10 : 0;
            if (OMX_ErrorNone == NvxGetEventExtension(eventType, &eNvxEventType))
            {
                // BKC: Why are these sent as "DynamicResourcesAvailable" with
                // the actual event type in the Data1 field, rather than
                // simply sending them with the correct event type in the
                // event type field?
                NvxSendEvent(pNvComp, OMX_EventDynamicResourcesAvailable,
                             eNvxEventType, 0, (OMX_PTR)&TimeStamp);
            }
        }
        break;

    case NvMMEventCamera_RawFrameCopy:
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMFrameCopy *frameCopy = (NvMMFrameCopy*)eventInfo;
            NvxSendEvent(pNvComp, NVX_EventCamera_RawFrameCopy,
                frameCopy->bufferSize, 0,
                frameCopy->userBuffer);
            NvOsFree(frameCopy->userBuffer);
        }
        break;

    case NvMMDigitalZoomEvents_PreviewFrameCopy:
    case NvMMDigitalZoomEvents_StillConfirmationFrameCopy:
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMFrameCopy *frameCopy = (NvMMFrameCopy*)eventInfo;
            NvxSendEvent(pNvComp,
                (eventType == NvMMDigitalZoomEvents_PreviewFrameCopy) ?
                NVX_EventCamera_PreviewFrameCopy :
                NVX_EventCamera_StillConfirmationFrameCopy,
                frameCopy->bufferSize, 0,
                frameCopy->userBuffer);
            NvOsFree(frameCopy->userBuffer);
        }
        break;

    case NvMMDigitalZoomEvents_FaceInfo:
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMFaceInfo *faceInfo = (NvMMFaceInfo *)eventInfo;
            NvxSendEvent(pNvComp,
                NVX_EventCamera_FaceInfo,
                faceInfo->numOfFaces,           /* number of detected faces */
                faceInfo->bufSize,              /* size of face list        */
                faceInfo->faces);               /* pointer to a face list   */
            NvOsFree(faceInfo->faces);
        }
        break;

    case NvMMDigitalZoomEvents_StillFrameCopy:
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMFrameCopy *frameCopy = (NvMMFrameCopy*)eventInfo;
            NvxSendEvent(pNvComp,
                NVX_EventCamera_StillYUVFrameCopy,
                frameCopy->bufferSize, 0,
                frameCopy->userBuffer);
            NvOsFree(frameCopy->userBuffer);
        }
        break;

    case NvMMDigitalZoomEvents_SmoothZoomFactor:
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMEventSmoothZoomFactorInfo *pSmoothZoomFactor =
                (NvMMEventSmoothZoomFactorInfo*)eventInfo;

            // Check for the smooth zoom event and send it to the
            // upper layers iff its smooth zoom
            if (pSmoothZoomFactor->IsSmoothZoom)
            {
                NvxSendEvent(pNvComp, NVX_EventCamera_SmoothZoomFactor,
                    pSmoothZoomFactor->ZoomFactor,
                    pSmoothZoomFactor->SmoothZoomAchieved, NULL);
            }

            if(pSmoothZoomFactor->SmoothZoomAchieved)
            {
                NvError Err = NvSuccess;
                Err = NvxCameraUpdateExposureRegions((void *)pNvComp->pComponentData);
                if (Err != NvSuccess)
                {
                    return Err;
                }
            }
        }

        break;

    case NvMMEvent_BlockWarning:
        NVMMVERBOSE(("%s: NvMMEventCamera_\n", pData->sName));
        if (pData->pParentComp)
        {
            NvxComponent *pNvComp = pData->pParentComp;
            NvMMBlockWarningInfo *pBlockWarningInfo = (NvMMBlockWarningInfo*)eventInfo;
            NvxSendEvent(pNvComp, NVX_EventBlockWarning,
                         pBlockWarningInfo->warning, 0,
                         (OMX_PTR)pBlockWarningInfo->additionalInfo);
        }
        break;
        case NvMMEvent_ForBuffering:
        {
            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                NvMMEventForBuffering *pBlockBufferingInfo = (NvMMEventForBuffering*)eventInfo;
                NvxSendEvent(pNvComp, NVX_EventForBuffering,
                             (NvU32)pBlockBufferingInfo->ChangeStateToPause,
                             pBlockBufferingInfo->BufferingPercent,
                             NULL);
            }
        }
        break;
    case NvMMEvent_SetAttributeError:
        {
            NvMMSetAttributeErrorInfo *pBlockErr = (NvMMSetAttributeErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;

            NVMMVERBOSE(("SetAttributeErrorEvent\n"));

            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                eReturnVal = (pBlockErr) ? NvxTranslateErrorCode(pBlockErr->error) : OMX_ErrorUndefined;
                NvxSendEvent(pNvComp, OMX_EventError, eReturnVal, 0, NULL);
            }
        }
        break;
    case NvMMEvent_BlockError:
        {
            NvMMBlockErrorInfo *pBlockErr = (NvMMBlockErrorInfo *)eventInfo;
            OMX_ERRORTYPE eReturnVal = OMX_ErrorUndefined;
            int streamId = 0;

            NVMMVERBOSE(("StreamBlockErrorEvent\n"));

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

        case NvMMEvent_TracklistError:
        {
            if (pData->pParentComp)
            {
                NvxComponent *pNvComp = pData->pParentComp;
                NvMMEventTracklistErrorInfo *trackListError = (NvMMEventTracklistErrorInfo*)eventInfo;
                /*Handle in case of DRM error and that too if it is NvError_DrmLicenseNotFound*/
                if (trackListError->domainName == NvMMErrorDomain_DRM &&
                    trackListError->error == NvError_DrmLicenseNotFound)
                {
                    NvxSendEvent(pNvComp, NVX_EventDRM_DirectLicenseAcquisition,
                                 0, 0, (OMX_PTR)trackListError);
                }
            }
        }
        break;
    default:
        break;
    }
    return NvSuccess;
}

static NvError NvxVideoSurfaceAlloc(NvRmDeviceHandle hRmDev,
                    NvMMBuffer *pMMBuffer, NvU32 BufferID, NvU32 Width,
                    NvU32 Height, NvU32 ColourFormat, NvU32 Layout,
                    NvU32 *pImageSize, NvBool UseAlignment,
                    NvOsMemAttribute Coherency,
                    NvBool ClearYUVsurface)
{
    NvError err;

    NvxSurface *pSurface = &pMMBuffer->Payload.Surfaces;
    if(ColourFormat == NvMMVideoDecColorFormat_YUV420)
    {
            err = NvxSurfaceAllocContiguousYuv(
            pSurface,
            Width,
            Height,
            NvMMVideoDecColorFormat_YUV420,
            Layout,
            pImageSize,
            UseAlignment,Coherency,
            ClearYUVsurface);
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
            UseAlignment,
            Coherency,
            NV_FALSE);
    }

    pMMBuffer->StructSize = sizeof(NvMMBuffer);
    pMMBuffer->BufferID = BufferID;
    pMMBuffer->PayloadType = NvMMPayloadType_SurfaceArray;

    NvOsMemset(&pMMBuffer->PayloadInfo, 0, sizeof(pMMBuffer->PayloadInfo));

    return err;
}

static void NvxVideoDecSurfaceFree(NvMMBuffer *pMMBuffer)
{
    NvxSurface *pSurface = &pMMBuffer->Payload.Surfaces;
    if (pSurface)
    {
        int i=0;
        for (i=0;i<pSurface->SurfaceCount;i++)
        {
            if ((3 == pSurface->SurfaceCount) &&
                (pSurface->Surfaces[0].hMem == pSurface->Surfaces[1].hMem))
            {
                NvxMemoryFree(&pSurface->Surfaces[0].hMem);
                pSurface->Surfaces[0].hMem = NULL;
                pSurface->Surfaces[1].hMem = NULL;
                pSurface->Surfaces[2].hMem = NULL;
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

static void NvxDeinterlaceThread(void* arg)
{
    SNvxNvMMTransformData *pData = (SNvxNvMMTransformData *)arg;
    NvMMBuffer *OutSurface;
    NvU32 QueueEntries = 0;
    NvU64  delta = 0;
    TVMRCtx *pTVMR = &pData->vmr;
    NvError status = NvSuccess;
    NvU32 availInpOutCount = 0;
    NvU32 streamIndex = 1;
    SNvxNvMMPort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvxComponent *pComp = pData->pParentComp;

    status = TVMRInit(pData);
    if (status == NvSuccess)
        NvOsSemaphoreSignal(pData->TvmrInitDone);

    if (pTVMR->DeinterlacingRate == DEINTERLACING_FIELD_RATE)
        availInpOutCount = 1; //for field rate we should need atleast 2 free output buffers and 2 decoder surfaces

    while(pData->DeinterlaceThreadClose == OMX_FALSE)
    {

        status = NvOsSemaphoreWaitTimeout(pData->InpOutAvail, 2000);
        if (status != NvSuccess)
            continue;

        NvxMutexLock(pComp->hWorkerMutex);
        if (pData->Deinterlacethreadtopuase == OMX_TRUE)
        {
            NvOsSemaphoreSignal(pData->DeintThreadPaused);
            pData->StopDeinterlace = OMX_TRUE;
            pData->Deinterlacethreadtopuase = OMX_FALSE;
            NvxMutexUnlock(pComp->hWorkerMutex);
            continue;
        }
        if ((pData->StopDeinterlace == OMX_TRUE) || (pComp->ePendingState == OMX_StatePause))
        {
            if (!pPort->bEOS)
            {
                NvxMutexUnlock(pComp->hWorkerMutex);
                continue;
            }
        }
        QueueEntries = NvMMQueueGetNumEntries(pData->pReleaseOutSurfaceQueue);
        if ((CheckFreeBuffers(pTVMR) > availInpOutCount) && (QueueEntries > availInpOutCount) && (!pData->bFlushing))
        {
            if (pData->DeinterlaceThreadClose == OMX_TRUE)
            {
                NvxMutexUnlock(pComp->hWorkerMutex);
                return ;
            }
            NvMMQueueDeQ(pData->pReleaseOutSurfaceQueue, &OutSurface);
            pTVMR->pNextVideoSurface = GetTVMRVideoSurface(pTVMR, OutSurface);
            pTVMR->pNextSurface = OutSurface;
            if (pTVMR->prevTimeStamp != 0) {
                delta = (OutSurface->PayloadInfo.TimeStamp - pTVMR->prevTimeStamp) >> 1;
                delta = ((pTVMR->DeltaTime * pTVMR->DeltaCount) + delta)/(pTVMR->DeltaCount + 1);
                pTVMR->DeltaCount++;
                pTVMR->DeltaTime = delta;
            }
            pTVMR->prevTimeStamp = OutSurface->PayloadInfo.TimeStamp;
            if (pTVMR->pCurrVideoSurface)
            {
                Deinterlace(pData, delta);
            }
            pTVMR->pPrev2VideoSurface = pTVMR->pPrevVideoSurface;
            pTVMR->pPrevVideoSurface = pTVMR->pCurrVideoSurface;
            pTVMR->pCurrVideoSurface = pTVMR->pNextVideoSurface;
            pTVMR->pPrev2Surface = pTVMR->pPrevSurface;
            pTVMR->pPrevSurface  = pTVMR->pCurrSurface;
            pTVMR->pCurrSurface  = pTVMR->pNextSurface;
        }
        else
        {
            if (pPort->bEOS && !pPort->bSentEOS && !pPort->bError && !QueueEntries)
            {
                int attempts = 0;

                while (!pPortOut->pCurrentBufferHdr)
                {
                    NvxPortGetNextHdr(pPortOut);

                    if (!pPortOut->pCurrentBufferHdr)
                    {
                        NvxMutexUnlock(pComp->hWorkerMutex);

                        attempts++;
                        if (attempts > 20 && !pData->bPausing)
                        {
                            if (OMX_StateIdle == pData->pParentComp->eState)
                            {
                                continue;
                            }
                        }

                        if (pData->bStopping || pPort->bAborting || pData->bFlushing)
                        {
                            continue;
                        }

                        NvOsSleepMS(10);

                        NvxMutexLock(pComp->hWorkerMutex);
                    }
                }

                pPortOut->pCurrentBufferHdr->nFilledLen = 0;
                pPortOut->pCurrentBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
                NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
                NVMMVERBOSE(("delivering EOS buffer\n"));
                NvDebugOmx(("Block %s: Delivering EOS buffer", pData->sName));
                pPort->bSentEOS = OMX_TRUE;

                if (pData->bSequence)
                {
                    // for next frame in the sequence
                    pPort->bEOS = OMX_FALSE;
                    pPort->bSentEOS = OMX_FALSE;
                    NvxMutexUnlock(pComp->hWorkerMutex);
                    continue;
                }
            }
        }
        NvxMutexUnlock(pComp->hWorkerMutex);
    }
}

static NvError Deinterlace(SNvxNvMMTransformData *pData, NvU64 delta)
{
    NvU32 streamIndex = 1; //for output
    TVMRCtx *pTVMR = &pData->vmr;
    TVMRVideoSurface *pOutSurf = NULL;    /* from outstream */
    NvMMBuffer* output = NULL;
    NvError status = NvSuccess;
    NvBool TopFieldFirst;
    TVMRPictureStructure PicStruct1stField, PicStruct2ndField;
    NvMMBufferType BufferType = NvMMBufferType_Payload;

    if (pTVMR->pPrev2VideoSurface != pTVMR->pPrevVideoSurface)
    {
        if (pTVMR->pPrev2VideoSurface)
        {
            pData->TransferBufferToBlock(pData->hBlock, 1,
                    BufferType,
                    sizeof(NvMMBuffer), pTVMR->pPrev2Surface);
       }
    }
    TopFieldFirst = pTVMR->pCurrSurface->PayloadInfo.BufferMetaData.DeinterlaceMetadata.bTopFieldFirst;
    if (TopFieldFirst == NV_TRUE){
        PicStruct1stField = TVMR_PICTURE_STRUCTURE_TOP_FIELD;
        PicStruct2ndField = TVMR_PICTURE_STRUCTURE_BOTTOM_FIELD;
    }
    else {
        PicStruct1stField = TVMR_PICTURE_STRUCTURE_BOTTOM_FIELD;
        PicStruct2ndField = TVMR_PICTURE_STRUCTURE_TOP_FIELD;
    }

    output = GetFreeNvMMSurface(pTVMR);
    pOutSurf = GetTVMRVideoSurface(pTVMR, output);
    status = TVMRVideoMixerRenderYUV(
              pTVMR->pMixer,
              pOutSurf,
              PicStruct1stField,
              pTVMR->pCurrVideoSurface,
              pTVMR->pCurrVideoSurface,
              pTVMR->pPrevVideoSurface,
              pTVMR->pPrevVideoSurface,
              NULL,
              NULL,
              pTVMR->fenceDone);
    if (status != TVMR_STATUS_OK)
    {
        NV_DEBUG_PRINTF(("Deinterlace :  TVMRVideoMixerRenderYUV TVMR error \n"));
        status = NvError_BadValue;
        goto fail;
    }
    TVMRFenceBlock(pTVMR->pDevice, pTVMR->fenceDone);
    output->PayloadInfo = pTVMR->pCurrSurface->PayloadInfo;
    output->PayloadInfo.TimeStamp = pTVMR->pCurrSurface->PayloadInfo.TimeStamp;
    output->Payload.Surfaces.CropRect = pTVMR->pCurrSurface->Payload.Surfaces.CropRect;
    output->Payload.Surfaces.DispRes = pTVMR->pCurrSurface->Payload.Surfaces.DispRes;
    output->Payload.Surfaces.SurfaceCount = pTVMR->pCurrSurface->Payload.Surfaces.SurfaceCount;
    output->Payload.Surfaces.Empty = pTVMR->pCurrSurface->Payload.Surfaces.Empty;

    status = NvxNvMMTransformDeliverFullOutputInternal(pData, output, streamIndex);

    if (pTVMR->DeinterlacingRate == DEINTERLACING_FIELD_RATE)
    {
        /*Deinterlacing the bottom field */
        output = GetFreeNvMMSurface(pTVMR);
        if (output == NULL)
        {
            NV_DEBUG_PRINTF(("Deinterlace : there is no empty buffer \n"));
            status = NvError_BadValue;
            goto fail;
        }
        pOutSurf = GetTVMRVideoSurface(pTVMR, output);

        status = TVMRVideoMixerRenderYUV(
                  pTVMR->pMixer,
                  pOutSurf,
                  PicStruct2ndField,
                  pTVMR->pNextVideoSurface,
                  pTVMR->pCurrVideoSurface,
                  pTVMR->pCurrVideoSurface,
                  pTVMR->pPrevVideoSurface,
                  NULL,
                  NULL,
                  pTVMR->fenceDone);
        if (status != TVMR_STATUS_OK)
        {
            NV_DEBUG_PRINTF((" Deinterlace : TVMRVideoMixerRenderYUV TVMR error \n"));
            status = NvError_BadValue;
            goto fail;
        }

        TVMRFenceBlock(pTVMR->pDevice, pTVMR->fenceDone);
        output->PayloadInfo = pTVMR->pCurrSurface->PayloadInfo;
        output->PayloadInfo.TimeStamp = pTVMR->pCurrSurface->PayloadInfo.TimeStamp + delta;
        output->Payload.Surfaces.CropRect = pTVMR->pCurrSurface->Payload.Surfaces.CropRect;
        output->Payload.Surfaces.DispRes = pTVMR->pCurrSurface->Payload.Surfaces.DispRes;
        output->Payload.Surfaces.SurfaceCount = pTVMR->pCurrSurface->Payload.Surfaces.SurfaceCount;
        output->Payload.Surfaces.Empty = pTVMR->pCurrSurface->Payload.Surfaces.Empty;

        status = NvxNvMMTransformDeliverFullOutputInternal(pData, output, streamIndex);
    }

fail:
    return status;
}

static NvError TVMRInit(SNvxNvMMTransformData *pData)
{
    NvU32 i = 0, j = 0;
    TVMRVideoMixerAttributes VMRattrib;
    NvError status = NvSuccess;
    TVMRCtx *pTVMR = &pData->vmr;

    /* Initialize the 3D engine pipeline */
    if (!pTVMR->pDevice) {
        pTVMR->pDevice = TVMRDeviceCreate(NULL);
    }
    if (!pTVMR->pDevice)
    {
        NV_DEBUG_PRINTF(("Deinterlace : not able to create TVMRDeviceCreate \n"));
        status = NvError_ResourceError;
        goto fail;
    }

    if (pTVMR->pMixer) {
        TVMRVideoMixerDestroy(pTVMR->pMixer);
    }

    /* Create mixer for CSC and De-interlacing */
    pTVMR->pMixer =
        TVMRVideoMixerCreate(pTVMR->pDevice, TVMRSurfaceType_YV12,
                             pTVMR->frameWidth, pTVMR->frameHeight,
                             TVMR_VIDEO_MIXER_FEATURE_ADVANCE1_DEINTERLACING);
    if (!pTVMR->pMixer)
    {
        NV_DEBUG_PRINTF(("Deinterlace : not able to create TVMRVideoMixerCreate \n"));
        status = NvError_InsufficientMemory;
        goto fail;
    }

    /*eanble here for which alogrithm to test */
    pTVMR->DeinterlacingRate = DEINTERLACING_FRAME_RATE;
    //pTVMR->DeinterlacingRate = DEINTERLACING_FIELD_RATE;
    VMRattrib.yInvert = NV_TRUE;
    //Enabiling BOB for res > 720p and temporal for <720p
    if (pTVMR->frameWidth <= 1280 &&
        pTVMR->frameHeight <= 720)
        VMRattrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_ADVANCE1;
    else
        VMRattrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_BOB;
    NV_DEBUG_PRINTF(("Deinterlace : %s@%d: deinterlaceType = %d Rate = %d Width = %d Height = %d\n", __FUNCTION__, __LINE__, VMRattrib.deinterlaceType, pTVMR->DeinterlacingRate,
    pTVMR->frameWidth, pTVMR->frameHeight));

    TVMRVideoMixerSetAttributes(pTVMR->pMixer,
                                TVMR_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE,
                                &VMRattrib);

    pTVMR->fenceDone = TVMRFenceCreate();
    for (i = 0; i <TVMR_SURFACES; i++)
    {
        pTVMR->pVideoSurfaceList[i] =  (TVMRVideoSurface*)NvOsAlloc(sizeof(TVMRVideoSurface));
        if (!pTVMR->pVideoSurfaceList[i])
        {
            NV_DEBUG_PRINTF(("Deinterlace : Unable to allocate the structure TVMRVideoSurface %d \n", i));
            status = NvError_InsufficientMemory;
            goto fail;
        }
        pTVMR->pVideoSurfaceList[i]->type   = TVMRSurfaceType_YV12;
        pTVMR->pVideoSurfaceList[i]->width  = pTVMR->frameWidth;
        pTVMR->pVideoSurfaceList[i]->height = pTVMR->frameHeight;

        for (j = 0; j< 3; j++)
        {
            pTVMR->pVideoSurfaceList[i]->surfaces[j] = (TVMRSurface*)NvOsAlloc(
                                                        sizeof(TVMRSurface));
            if (!pTVMR->pVideoSurfaceList[i]->surfaces[j])
            {
                NV_DEBUG_PRINTF(("Deinterlace : Unable to allocate the TVMR VideoSurfaces %d \n", i));
                status = NvError_InsufficientMemory;
                goto fail;
            }
        }
    }

    pTVMR->prevTimeStamp = 0;
    pTVMR->DeltaCount = 0;
    pTVMR->DeltaTime = 0;
    return status;

fail:
    {
        NvU32 k, j;
        for (k = 0; k < i; k++)
        {
            if (pTVMR->pVideoSurfaceList[k])
            {
                for (j = 0; j< 3; j++)
                {
                    NvOsFree(pTVMR->pVideoSurfaceList[j]->surfaces[j]);
                    pTVMR->pVideoSurfaceList[j]->surfaces[j] = NULL;
                }
                NvOsFree(pTVMR->pVideoSurfaceList[k]);
                pTVMR->pVideoSurfaceList[k] = NULL;
            }
        }
        return status;
    }
}

static TVMRVideoSurface * GetTVMRVideoSurface(TVMRCtx *pTVMR, const NvMMBuffer* nvmmbuf)
{
   TVMRVideoSurface *pVideoSurface = pTVMR->pVideoSurfaceList[pTVMR->VideoSurfaceIdx++];
    if (pTVMR->VideoSurfaceIdx == TVMR_SURFACES)
        pTVMR->VideoSurfaceIdx = 0;

    pVideoSurface->surfaces[0]->priv = (void*)&nvmmbuf->Payload.Surfaces.Surfaces[0];
    pVideoSurface->surfaces[1]->priv = (void*)&nvmmbuf->Payload.Surfaces.Surfaces[1];
    pVideoSurface->surfaces[2]->priv = (void*)&nvmmbuf->Payload.Surfaces.Surfaces[2];
    return pVideoSurface;
}

static NvMMBuffer* GetFreeNvMMSurface(TVMRCtx *pTVMR)
{
    int i;
    for(i=0; i<TOTAL_NUM_SURFACES; i++)
    {
        if(pTVMR->Renderstatus[i] == FRAME_VMR_FREE)
        {
            pTVMR->Renderstatus[i] = FRAME_VMR_SENT_FOR_DISPLAY;
            return pTVMR->pVideoSurf[i];
        }
    }
    return NULL;
}

void TVMRClose(SNvxNvMMTransformData *pData)
{
    TVMRCtx *pTVMR = &pData->vmr;
    NvU32 i, j;

    pTVMR->pNextVideoSurface = NULL;
    pTVMR->pCurrVideoSurface = NULL;
    pTVMR->pPrevVideoSurface = NULL;
    pTVMR->pPrevSurface = NULL;
    pTVMR->pCurrSurface = NULL;
    pTVMR->pNextSurface = NULL;

    for (i = 0; i <TVMR_SURFACES; i++)
    {
        if (pTVMR->pVideoSurfaceList[i])
        {
            for (j = 0; j< 3; j++)
            {
                NvOsFree(pTVMR->pVideoSurfaceList[i]->surfaces[j]);
                pTVMR->pVideoSurfaceList[i]->surfaces[j] = NULL;
            }
            NvOsFree(pTVMR->pVideoSurfaceList[i]);
            pTVMR->pVideoSurfaceList[i] = NULL;
        }
    }
    if (pTVMR->fenceDone) {
        TVMRFenceDestroy(pTVMR->fenceDone);
        pTVMR->fenceDone = NULL;
    }
    if (pTVMR->pMixer) {
        TVMRVideoMixerDestroy(pTVMR->pMixer);
        pTVMR->pMixer = NULL;
    }
    if (pTVMR->pDevice) {
        TVMRDeviceDestroy(pTVMR->pDevice);
        pTVMR->pDevice = NULL;
    }
}

NvU32 CheckFreeBuffers(TVMRCtx *pTVMR)
{
    NvU32 count = 0, i;
    for (i = 0; i <TOTAL_NUM_SURFACES; i++)
    {
        if(pTVMR->Renderstatus[i] == FRAME_VMR_FREE)
        {
            if (++count > 1)
                break;
        }
    }
    return count;
}


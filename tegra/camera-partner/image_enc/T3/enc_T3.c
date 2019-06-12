/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvimage_enc.h"
#include "nvimage_enc_pvt.h"
#include "enc_T3.h"
#include "enc_sw.h"
#include "tvmr.h"
#include "nvmm_queue.h"
#include "nvimage_enc_makernote_extension_serializer.h"
#include "nvimage_enc_jds.h"
#include "nvmakernote_enc.h"

#define STUB 0

#define SW_THUMBNAIL_ENCODING   0
#define FEED_IMAGE_AT_START     1

typedef struct Tegra3HwEncPvtCtxRec {
    TVMRDevice          *device;

    /* input and output queues */
    NvMMQueueHandle     OutputQ;
    NvMMQueueHandle     InputSwThumbnailQ;

    NvMMQueueHandle     InputBufferQ;
    NvMMQueueHandle     FeedInputBufferQ;

    NvOsThreadHandle    hDeliveryThread;
    NvOsThreadHandle    hFeedImageThread;
    NvOsThreadHandle    hDeliveryThumbnailThread;
    TVMRVideoSurface    *pYuvFramePri;
    TVMRVideoSurface    *pYuvFrameThumb;
    TVMRJPEGEncoder     *jpegEncoder;
    TVMRFence           fence;

    /* Synchronising semaphore */
    NvOsSemaphoreHandle hSemInputBufAvail;
    NvOsSemaphoreHandle hSemFeedImageInputBufAvail;
    NvOsSemaphoreHandle hSemThumbnailInputBufAvail;
    NvOsSemaphoreHandle hSemThumbnailEncodeDone;
    NvBool              CloseEncoder;
    NvBool              bSet_CustomQuantTables_Luma;
    NvBool              bSet_CustomQuantTables_Chroma;
    NvBool              bEnableSWEncodingForThumbnail;
    NvImageEncHandle    SwImageEncoder;
    NvOsSemaphoreHandle hSemThumbnailSwOutputBufAvail;
    NvU32               HeaderLength;
    NvU8                *pTempThumbnail;

} Tegra3HwEncPvtCtx;

typedef struct InputBufferRec {
    NvMMBuffer *pInputBuffer;
    NvBool IsThumbnail;
    NvRmMemHandle hExifInfo;
} InputBuffer;


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////// Function Prototype //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


static void OutputThread(void *arg);
static NvError Encoder_Open(NvImageEncHandle hImageEnc);
static void Encoder_Close(NvImageEncHandle hImageEnc);
static NvError Encoder_GetParameters(NvImageEncHandle hImageEnc,
                    NvJpegEncParameters *params);
static NvError Encoder_SetParameters(NvImageEncHandle hImageEnc,
                    NvJpegEncParameters *params);
static NvError Encoder_Start(NvImageEncHandle hImageEnc,
                    NvMMBuffer *InputBuffer,
                    NvMMBuffer *OutPutBuffer,
                    NvBool IsThumbnail);
static void InitializeThumbnailQuantTables(NvJpegEncParameters *ThumbParams);
static NvU32 calculateOutPutBuffSize(NvJpegEncParameters *ImageParams);
static TVMRStatus TVMREncode(NvImageEnc *pContext,
    NvMMBuffer *pInBuffer, NvBool ThumbnailActive);
static TVMRStatus TVMRGetEncodedData(NvImageEnc *pContext,
    NvMMBuffer *pOutBuffer, NvBool ThumbnailActive,
    NvU32 *app5BytesWritten, NvU32 *EncState);
static NvError AddMakerNoteToApp5(NvRmMemHandle hMakernoteExtension,
                                  NvU32 *bytesWritten, NvU8 **buffer);
static void JpegEncoderDeliverFullOutput(
    void *ClientContext,
    NvU32 StreamIndex,
    NvU32 BufferSize,
    void *pBuffer,
    NvError EncStatus);
static NvU32 MNoteTrimSizeToValue(NvU32 input, NvU32 maxMakernoteSize);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////// Function Defination /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void JpegEncoderDeliverFullOutput(
    void *ClientContext,
    NvU32 StreamIndex,
    NvU32 BufferSize,
    void *pBuffer,
    NvError EncStatus)
{
    NvImageEnc *pContext = (NvImageEnc *)ClientContext;
    Tegra3HwEncPvtCtx *pEncCtx =
        (Tegra3HwEncPvtCtx *)pContext->pEncoder->pPrivateContext;

    if (StreamIndex == NvImageEncoderStream_THUMBNAIL_INPUT)
    {
        NvOsSemaphoreSignal(pEncCtx->hSemThumbnailSwOutputBufAvail);
        pContext->ClientCB(pContext->pClientContext,
            NvImageEncoderStream_THUMBNAIL_INPUT, sizeof(NvMMBuffer), pBuffer, EncStatus);
    }
}

static TVMRStatus TVMREncode(NvImageEnc *pContext,
    NvMMBuffer *pInBuffer, NvBool ThumbnailActive)
{
    Tegra3HwEncPvtCtx *pEncCtx =
        (Tegra3HwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    NvU32 quality = 0;
    TVMRVideoSurface *pYuvFrame = NULL;
    NvU8 LumaQuantTable[64];
    NvU8 ChromaQuantTable[64];
    TVMRStatus TVMRstatus = TVMR_STATUS_OK;
    NvU32 uiTargetSize = 0xFFFFFFFF;
    NvU32 HdrLen;

    if (ThumbnailActive == NV_TRUE)
    {
        quality = pEncPvt->ThumbParams.Quality;
        pYuvFrame = (TVMRVideoSurface *)pEncCtx->pYuvFrameThumb;

        NvOsMemcpy(LumaQuantTable,
            pEncPvt->ThumbParams.CustomQuantTables.LumaQuantTable, 64);
        NvOsMemcpy(ChromaQuantTable,
            pEncPvt->ThumbParams.CustomQuantTables.ChromaQuantTable, 64);
    }
    else
    {
        quality = pEncPvt->PriParams.Quality;
        pYuvFrame = (TVMRVideoSurface *)pEncCtx->pYuvFramePri;
    }

    switch (pYuvFrame->type)
    {
        case TVMRSurfaceType_NV12:
            NV_ASSERT(pInBuffer->Payload.Surfaces.SurfaceCount == 2);
            if (pInBuffer->Payload.Surfaces.SurfaceCount != 2)
            {
                NvOsDebugPrintf("Wrong Surface Count\n");
                return TVMR_STATUS_UNSUPPORTED;
            }
            pYuvFrame->surfaces[0]->pitch = pInBuffer->Payload.Surfaces.Surfaces[0].Pitch;
            pYuvFrame->surfaces[0]->priv = (void*)&pInBuffer->Payload.Surfaces.Surfaces[0];
            pYuvFrame->surfaces[1]->pitch = pInBuffer->Payload.Surfaces.Surfaces[1].Pitch;
            pYuvFrame->surfaces[1]->priv = (void*)&pInBuffer->Payload.Surfaces.Surfaces[1];
            break;
        case TVMRSurfaceType_YV12:
            NV_ASSERT(pInBuffer->Payload.Surfaces.SurfaceCount == 3);
            if (pInBuffer->Payload.Surfaces.SurfaceCount != 3)
            {
                NvOsDebugPrintf("Wrong Surface Count\n");
                return TVMR_STATUS_UNSUPPORTED;
            }
            pYuvFrame->surfaces[0]->pitch = pInBuffer->Payload.Surfaces.Surfaces[0].Pitch;
            pYuvFrame->surfaces[0]->priv = (void*)&pInBuffer->Payload.Surfaces.Surfaces[0];
            pYuvFrame->surfaces[1]->pitch = pInBuffer->Payload.Surfaces.Surfaces[2].Pitch;
            pYuvFrame->surfaces[1]->priv = (void*)&pInBuffer->Payload.Surfaces.Surfaces[2];
            pYuvFrame->surfaces[2]->pitch = pInBuffer->Payload.Surfaces.Surfaces[1].Pitch;
            pYuvFrame->surfaces[2]->priv = (void*)&pInBuffer->Payload.Surfaces.Surfaces[1];
            break;
        default:
            NV_ASSERT(!"Unsupported Surface Type\n");
            NvOsDebugPrintf("Unsupported Surface Type\n");
            return TVMR_STATUS_UNSUPPORTED;
    }

    while (1)
    {
        if (ThumbnailActive == NV_FALSE)
        {
#if CAPTURE_PROFILING
            NvOsDebugPrintf("Primary TVMR-Feed TimeIn %d \n", NvOsGetTimeMS());
#endif
            if (NV_FALSE == pEncCtx->bSet_CustomQuantTables_Luma)
            {
                TVMRstatus = TVMRJPEGEncoderFeedFrame(
                                pEncCtx->jpegEncoder,
                                pYuvFrame,
                                NULL,
                                pEncCtx->fence,
                                quality);
            }
            else
            {
                TVMRstatus = TVMRJPEGEncoderFeedFrameRateControl(
                                pEncCtx->jpegEncoder,
                                pYuvFrame,
                                NULL,
                                pEncCtx->fence,
                                (NvU8 *)LumaQuantTable,
                                (NvU8 *)ChromaQuantTable,
                                uiTargetSize);
            }
        }
        else
        {
#if CAPTURE_PROFILING
            NvOsDebugPrintf("Thumbnail TVMR-Feed TimeIn %d \n", NvOsGetTimeMS());
#endif
            TVMRstatus = TVMRJPEGEncoderFeedFrameRateControl(
                            pEncCtx->jpegEncoder,
                            pYuvFrame,
                            NULL,
                            pEncCtx->fence,
                            (NvU8 *)LumaQuantTable,
                            (NvU8 *)ChromaQuantTable,
                            (JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET - JPEG_FRAME_ENC_MAX_HEADER_LENGTH));
        }
        if (TVMRstatus != TVMR_STATUS_OK)
        {
            if (TVMRstatus == TVMR_STATUS_INSUFFICIENT_BUFFERING)
            {
                NvOsThreadYield();
                continue;
            }
            else
            {
                return TVMRstatus;
            }
        }
        else
            break;
    }

    return TVMRstatus;
}

static TVMRStatus TVMRGetEncodedData(NvImageEnc *pContext,
    NvMMBuffer *pOutBuffer, NvBool ThumbnailActive,
    NvU32 *app5BytesWritten, NvU32 *EncState)
{

    Tegra3HwEncPvtCtx *pEncCtx =
        (Tegra3HwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    TVMRStatus TVMRstatus = TVMR_STATUS_OK;
    NvU32 TvmrJpegEncFlag = TVMR_JPEG_ENC_FLAG_NONE;
    NvU32 ImgOffset = 0;
    NvU32 numBytesAvailable = 0;
    NvU32 numBytes = 0;
    NvU32 totalSize = 0;
    NvU8* ptr;
    NvU32 HdrLen = pEncCtx->HeaderLength;

    TVMRFenceBlock(pEncCtx->device, pEncCtx->fence);

    TVMRstatus = TVMRJPEGEncoderBitsAvailable(
                    pEncCtx->jpegEncoder,
                    &numBytesAvailable,
                    TVMR_BLOCKING_TYPE_IF_PENDING,
                    TVMR_TIMEOUT_INFINITE);
    if (TVMRstatus == TVMR_STATUS_OK)
    {
        //ToDo
        /* Add check for only primary too ... */
        if ((pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) &&
            (NV_FALSE == ThumbnailActive))
        {
            // SKIP SOI. Let the Header do it later
            TvmrJpegEncFlag = TVMR_JPEG_ENC_FLAG_SKIP_SOI;
            ImgOffset = JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET;
            *EncState |= PRIMARY_ENC_DONE;
        }
        else if((pContext->pEncoder->SupportLevel == PRIMARY_ENC) &&
                (NV_FALSE == ThumbnailActive))
        {
            // SKIP SOI. Let the Header do it later
            TvmrJpegEncFlag = TVMR_JPEG_ENC_FLAG_SKIP_SOI;
            ImgOffset = HdrLen;
            *EncState |= PRIMARY_ENC_DONE;
        }
        else
        {
            if (numBytesAvailable > JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET)
            {
                TVMRstatus = TVMR_STATUS_BAD_ALLOC;
                NvOsDebugPrintf("Image-Enc Error: Insufficient o/p buffer for"
                    "Thumbnail \n");
                goto fail;
            }

            ImgOffset = JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET - numBytesAvailable;
            *EncState |= THUMBNAIL_ENC_DONE;
        }

        if (ThumbnailActive == NV_FALSE)
        {
            /*
             * Initally we allocate o/p buffer only upto size of
             * JPEG_FRAME_ENC_MAX_THUMBNAIL_SIZE + JPEG_FRAME_ENC_MAX_APP_MARKER_SIZE,
             * as returned by a call to calculateOutPutBuffSize().
             * When we encode primary, TVMR first gives an estimate of the
             * required output size. We use TVMR's estimate to realloc the
             * original allocation, assuming we will be able to accomodate the
             * thumbnail data and the header information in
             * JPEG_FRAME_ENC_MAX_THUMBNAIL_SIZE +
             * JPEG_FRAME_ENC_MAX_APP_MARKER_SIZE size.
             */
            pOutBuffer->Payload.Ref.pMem =
                NvOsRealloc(pOutBuffer->Payload.Ref.pMem,
                (pEncPvt->PriParams.EncodedSize + numBytesAvailable));
            if (pOutBuffer->Payload.Ref.pMem == NULL)
            {
                // initial allocation will be anyways free'd by the client.
                TVMRstatus = TVMR_STATUS_BAD_ALLOC;
                NvOsDebugPrintf("Image-Enc Error: o/p buffer re-alloc failed\n");
                goto fail;
            }

            /* reset the o/p buffer size to the new realloc'd size */
            pOutBuffer->Payload.Ref.sizeOfBufferInBytes =
                pEncPvt->PriParams.EncodedSize + numBytesAvailable;

            ptr = (NvU8 *)((NvU32)pOutBuffer->Payload.Ref.pMem + ImgOffset);

            if (pEncCtx->pTempThumbnail)
            {
                NvOsMemcpy((NvU8 *)((NvU32)pOutBuffer->Payload.Ref.pMem +
                    JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET -
                    pEncPvt->HeaderParams.ThumbNailSize),
                   pEncCtx->pTempThumbnail,
                   pEncPvt->HeaderParams.ThumbNailSize);
            }
        }
        else if ((pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) &&
            (*EncState == THUMBNAIL_ENC_DONE) && ThumbnailActive)
        {
            /*
             * Special case :
             * We do not yet have the primary image size, so we cannot determine
             * the size of the entire JPEG output. So we do a temporary allocation
             * of Thumbnail data, and when we have the primary data, we will append
             * the thumbnail at an offset.
             */
            pEncCtx->pTempThumbnail = NvOsAlloc(numBytesAvailable);
            if (pEncCtx->pTempThumbnail == NULL)
            {
                TVMRstatus = TVMR_STATUS_BAD_ALLOC;
                NvOsDebugPrintf("Image-Enc Error: temp thumbnail buffer"
                    "alloction failed\n");
                goto fail;
            }

            ptr = (NvU8*)pEncCtx->pTempThumbnail;
        }
        else
        {
            // Assume this the only other case, where
            // SupportLevel == JPEG_ENC_COMPLETE == PRIMARY_ENC_DONE
            ptr = (NvU8 *)((NvU32)pOutBuffer->Payload.Ref.pMem + ImgOffset);
        }

         if (!ThumbnailActive)
         {
             AddMakerNoteToApp5(pEncPvt->HeaderParams.hMakernoteExtension,
                app5BytesWritten, &ptr);
         }

        TVMRstatus = TVMRJPEGEncoderGetBits(pEncCtx->jpegEncoder,
                        &numBytes,
                        ptr,
                        TvmrJpegEncFlag);

        if ((TVMRstatus != TVMR_STATUS_OK) &&
            (TVMRstatus != TVMR_STATUS_NONE_PENDING))
        {
            NvOsDebugPrintf("Error returned1 0x%x\n",TVMRstatus);
            goto fail;
        }

        if (NV_TRUE == ThumbnailActive)
        {
            pEncPvt->HeaderParams.ThumbNailSize = numBytes;
        }
        else
        {
            pEncPvt->PrimarySize = numBytes;
        }
    }
    NvOsDebugPrintf("Error returned 0x%x\n",TVMRstatus);

#if CAPTURE_PROFILING
  if(ThumbnailActive)
    {
        NvOsDebugPrintf("Thumbnail TVMR TimeOut %d\n", NvOsGetTimeMS());
    }
    else
    {
        NvOsDebugPrintf("Primary TVMR TimeOut %d\n", NvOsGetTimeMS());
    }
#endif

fail:
    if (pEncCtx->pTempThumbnail)
    {
        NvOsFree(pEncCtx->pTempThumbnail);
        pEncCtx->pTempThumbnail = NULL;
    }
    return TVMRstatus;
}


static void FeedInputThread(void *arg)
{
    NvError status = NvError_NotInitialized;
    NvImageEnc *pContext = (NvImageEnc *)arg;
    Tegra3HwEncPvtCtx *pEncCtx =
        (Tegra3HwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    NvMMBuffer *pInputBuffer = NULL;
    TVMRStatus TVMRstatus = TVMR_STATUS_OK;

    while (!pEncCtx->CloseEncoder)
    {
        NvBool ThumbnailActive = NV_FALSE;
        InputBuffer InputBuff;

        // Wait until data is available
        NvOsSemaphoreWait(pEncCtx->hSemFeedImageInputBufAvail);

        if (pEncCtx->CloseEncoder)
        {
            continue;
        }

        status = NvMMQueueDeQ(pEncCtx->FeedInputBufferQ, &InputBuff);
        if (status != NvSuccess)
        {
            return;
        }

        pInputBuffer = InputBuff.pInputBuffer;
        ThumbnailActive = InputBuff.IsThumbnail;

        TVMRstatus = TVMREncode(pContext, pInputBuffer, ThumbnailActive);
        if (TVMRstatus != TVMR_STATUS_OK)
        {
            NvOsDebugPrintf("Feed Image Error 0x%x\n", TVMRstatus);
            return;
        }
        NvOsSemaphoreSignal(pEncCtx->hSemInputBufAvail);
     }
}


static void ThumbnailEncodeThread(void *arg)
{
    NvError status = NvError_NotInitialized;
    NvImageEnc *pContext = (NvImageEnc *)arg;
    Tegra3HwEncPvtCtx *pEncCtx =
        (Tegra3HwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    NvMMBuffer *pOutputBufferThumbnail = NULL;
    NvMMBuffer *pInputBuffer = NULL;

    NvOsDebugPrintf("ThumbnailEncodeThread ++\n");
    while (!pEncCtx->CloseEncoder)
    {
        // Wait until data is available
        NvOsSemaphoreWait(pEncCtx->hSemThumbnailInputBufAvail);

        if (pEncCtx->CloseEncoder)
        {
            continue;
        }

        NvOsSemaphoreWait(pEncCtx->hSemThumbnailEncodeDone);

        status = NvMMQueueDeQ(pEncCtx->InputSwThumbnailQ, &pInputBuffer);
        if (status != NvSuccess)
        {
            return;
        }
        status = NvMMQueuePeek(pEncCtx->OutputQ, &pOutputBufferThumbnail);
        if (status != NvSuccess)
        {
            return;
        }
        NvImageEnc_Start(pEncCtx->SwImageEncoder, pInputBuffer, pOutputBufferThumbnail, NV_TRUE);
     }
     NvOsDebugPrintf("ThumbnailEncodeThread --\n");
}

#define MAX_APP5_LEN (65536)

static NvError AddMakerNoteToApp5(NvRmMemHandle hMakernote,
                                  NvU32 *bytesWritten, NvU8 **buffer)
{
    NvError status = NvSuccess;
    char MakerNote[MAX_EXIF_MAKER_NOTE_SIZE_IN_BYTES];
    NvCameraJDS db;
    NvS32 app5len = 0;
    NvU8 *bufferlenloc = NULL;
    NvU32 makerNoteEncodeSize = 0;
    NvU32 len = 0;

    *bytesWritten = 0;
    NvRmMemRead(hMakernote, 0, MakerNote, MAX_EXIF_MAKER_NOTE_SIZE_IN_BYTES);

    *((*buffer)++) = M_MARKER;
    *((*buffer)++) = M_APP5;
    app5len += 2;

    bufferlenloc = (*buffer); // fill later
    (*buffer) += 2;
    app5len += 2;

    // signature
    *((*buffer)++) = 'N';
    *((*buffer)++) = 'V';
    *((*buffer)++) = 'D';
    *((*buffer)++) = 'A';
    *((*buffer)++) = '0';
    *((*buffer)++) = '0';
    app5len += 6;

    //encode the APP5 makernote
    len = MNoteTrimSizeToValue(MAX_EXIF_MAKER_NOTE_SIZE_IN_BYTES, MAX_APP5_LEN - app5len - 2);
    encodeMakerNote((*buffer), (NvU8*) MakerNote, len);

    makerNoteEncodeSize = MNoteEncodeSize(len);
    (*buffer) += makerNoteEncodeSize;
    app5len += makerNoteEncodeSize;

    *((*buffer)++) = '\0';
    *((*buffer)++) = '\0';
    app5len += 2;

    *bufferlenloc = (app5len - 2) >> 8; //MSB minus 2 bytes of app marker
    *(bufferlenloc + 1) = (app5len - 2) & 0xFF; //LSB

    *bytesWritten = app5len;

    return status;
}

static NvU32
MNoteTrimSizeToValue(NvU32 input, NvU32 maxMakernoteSize)
{
    if (MNoteEncodeSize(input) > maxMakernoteSize)
        return maxMakernoteSize - NV_MNENC_BLOCK_SIZE;

    return input;
}


static void OutputThread(void *arg)
{
    NvImageEnc *pContext = (NvImageEnc *)arg;
    Tegra3HwEncPvtCtx *pEncCtx =
        (Tegra3HwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    NvMMBuffer *pWorkBuffer = NULL;
    NvMMBuffer *pOutBuffer = NULL;
    NvU32 app5BytesWritten = 0;
    NvError status = NvError_NotInitialized;
    NvU32 EncState = JPEG_ENC_START;
    NvBool EnableSWEncoding = pEncCtx->bEnableSWEncodingForThumbnail;
    NvBool PullOutputBuffer = NV_TRUE;

    TVMRStatus TVMRstatus = TVMR_STATUS_OK;
    NvError SWStatus = NvSuccess;

    while (!pEncCtx->CloseEncoder)
    {
        NvBool ThumbnailActive = NV_FALSE;
        InputBuffer InputBuff;

        // Wait until data is available
        NvOsSemaphoreWait(pEncCtx->hSemInputBufAvail);

        /* Condition for exiting thread */
        if (pEncCtx->CloseEncoder)
        {
            continue;
        }
        /*
         * What we should be checking :
         * 1. Do we have to do thumbnail.
         * 2. We have a buffer, but do not know if it is Primary or Thumbnail
         * 3. If Primary encoding has been done and thumbnail has not been done,
         *    wait for thumbnail data before processing any other data and vice-versa.
         */
        status = NvMMQueueDeQ(pEncCtx->InputBufferQ, &InputBuff);
        if (status != NvSuccess)
        {
            goto Exit;
        }

        if (PullOutputBuffer)
        {
            /* We are ready for encoding .. we should check if we have any o/p
             * buffer available. We will onlyhave 1 output buffer per a complete
             * set of JPEG data going out */
            status = NvMMQueuePeek(pEncCtx->OutputQ, &pOutBuffer);
            if (status != NvSuccess)
            {
                goto Exit;
            }

            PullOutputBuffer = NV_FALSE;
        }

        pWorkBuffer = InputBuff.pInputBuffer;
        ThumbnailActive = InputBuff.IsThumbnail;

        if(!pEncPvt->HeaderParams.hExifInfo)
        {
            /* Take the exif info.
             * We need it for the Header Leangth calculation */
            pEncPvt->HeaderParams.hExifInfo =
                pWorkBuffer->PayloadInfo.BufferMetaData.EXIFBufferMetadata.EXIFInfoMemHandle;
        }

        if (!pEncPvt->HeaderParams.hMakernoteExtension)
        {
            pEncPvt->HeaderParams.hMakernoteExtension =
            pWorkBuffer->PayloadInfo.BufferMetaData.EXIFBufferMetadata.MakerNoteExtensionHandle;
        }

        GetHeaderLen(pContext, pEncPvt->HeaderParams.hExifInfo, &pEncCtx->HeaderLength);


#if !FEED_IMAGE_AT_START
        TVMRstatus = TVMREncode(pContext, pWorkBuffer, ThumbnailActive);
        if (TVMRstatus != TVMR_STATUS_OK)
        {
            goto Exit;
        }
#endif

        TVMRstatus = TVMRGetEncodedData(pContext,
                        pOutBuffer, ThumbnailActive,
                        &app5BytesWritten, &EncState);
        if (TVMRstatus != TVMR_STATUS_OK)
        {
            NvOsDebugPrintf("Error Get encoded data\n");
            goto Exit;
        }

        if (ThumbnailActive == NV_FALSE)
        {
            pContext->ClientCB(pContext->pClientContext,
                NvImageEncoderStream_INPUT, sizeof(NvMMBuffer), pWorkBuffer, status);
        }
        /* return I/p thumbnail buffer */
        else
        {
            pContext->ClientCB(pContext->pClientContext,
                NvImageEncoderStream_THUMBNAIL_INPUT, sizeof(NvMMBuffer), pWorkBuffer, status);
        }

        /* Do we have a complete buffer yet .. we may need to wait for the other set of data */
        NvOsDebugPrintf("[Image-Enc] Client requested %s. %s done\n",
            (pContext->pEncoder->SupportLevel == PRIMARY_ENC_DONE) ? "Primary Only" :
                (pContext->pEncoder->SupportLevel == THUMBNAIL_ENC_DONE) ? "Thumbnail Only" :
                    (pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) ? "Thumbnail and Primary" : "Nothing",
            (EncState == PRIMARY_ENC_DONE) ? "Primary" :
                (EncState == THUMBNAIL_ENC_DONE) ? "Thumbnail" :
                    (EncState == JPEG_ENC_COMPLETE) ? "Thumbnail and Primary" : "Nothing");

        if (EnableSWEncoding == NV_FALSE)
        {
            if (EncState != pContext->pEncoder->SupportLevel){
                NvOsDebugPrintf("[Image-Enc] Continue .. Not ready to deliver \n");
                continue;
            }
        }
Exit:

        /* initialize the output buffer pointers */
        pOutBuffer->Payload.Ref.startOfValidData = 0;
        pOutBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
        pEncPvt->HeaderParams.pbufHeader = (NvU8 *)(pOutBuffer->Payload.Ref.pMem);

        if (pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE)
        {
            if (EnableSWEncoding)
            {
                NvJpegEncParameters ImageParams;

                NvOsSemaphoreWait(pEncCtx->hSemThumbnailSwOutputBufAvail);
                ImageParams.EnableThumbnail = NV_TRUE;
                SWStatus = NvImageEnc_GetParam(pEncCtx->SwImageEncoder, &ImageParams);
                if (SWStatus != NvSuccess)
                {
                    return;
                }
                pEncPvt->HeaderParams.ThumbNailSize = ImageParams.EncodedSize;
            }

            pOutBuffer->Payload.Ref.startOfValidData =
                JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET -
                pEncPvt->HeaderParams.ThumbNailSize - pEncCtx->HeaderLength;
        }

        pEncPvt->HeaderParams.pbufHeader += pOutBuffer->Payload.Ref.startOfValidData;

        /* Compete the JPEG header before sending it outside */
        JPEGEncWriteHeader(pContext, pEncPvt->HeaderParams.pbufHeader);

        pOutBuffer->Payload.Ref.sizeOfValidDataInBytes =
            pEncCtx->HeaderLength + pEncPvt->HeaderParams.ThumbNailSize +
            pEncPvt->PrimarySize + app5BytesWritten;

        NV_ASSERT(pOutBuffer->Payload.Ref.startOfValidData +
            pOutBuffer->Payload.Ref.sizeOfValidDataInBytes <=
            pOutBuffer->Payload.Ref.sizeOfBufferInBytes);

        /* return O/p buffer */
        if (NvMMQueueGetNumEntries(pEncCtx->OutputQ))
        {
            status = NvMMQueueDeQ(pEncCtx->OutputQ, &pOutBuffer);

            NvOsDebugPrintf("size of buffer %d %d %d %d %d\n",
                pOutBuffer->Payload.Ref.sizeOfValidDataInBytes,
                pOutBuffer->Payload.Ref.startOfValidData,
                pEncPvt->PrimarySize,
                pEncCtx->HeaderLength, pEncPvt->HeaderParams.ThumbNailSize);

#if CAPTURE_PROFILING
            NvOsDebugPrintf("Capture Request Completes Time Out %d \n", NvOsGetTimeMS());
#endif

            pContext->ClientCB(pContext->pClientContext,
                NvImageEncoderStream_OUTPUT, sizeof(NvMMBuffer), pOutBuffer, status);
        }

        if ((pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) &&
            (EnableSWEncoding))
        {
            NvOsSemaphoreSignal(pEncCtx->hSemThumbnailEncodeDone);
        }
        /* It is safe to pull the next output buffer */
        PullOutputBuffer = NV_TRUE;
        /* Reset the encoder to start */
        pEncPvt->HeaderParams.hExifInfo = NULL;
        pEncPvt->HeaderParams.hMakernoteExtension = NULL;
        EncState = JPEG_ENC_START;
    }
}

static NvU32
calculateOutPutBuffSize(NvJpegEncParameters *ImageParams)
{
    NvU32 bufferSize = 0;
/*
    switch (ImageParams->InputFormat)
    {
        case NV_IMG_JPEG_COLOR_FORMAT_420:  // 4:2:0
            bufferSize = (ImageParams->Resolution.width * ImageParams->Resolution.height * 3)/2;
            break;

        case NV_IMG_JPEG_COLOR_FORMAT_422:  // 4:2:2
        case NV_IMG_JPEG_COLOR_FORMAT_422T: // 4:2:2T
            bufferSize = (ImageParams->Resolution.width * ImageParams->Resolution.height * 2);
            break;

        // max size
        case NV_IMG_JPEG_COLOR_FORMAT_444:  // 4:4:4
        default:
            bufferSize = (ImageParams->Resolution.width * ImageParams->Resolution.height * 3);
            break;
    }
*/

    /*
     * Initally we allocate o/p buffer only upto size of
     * JPEG_FRAME_ENC_MAX_THUMBNAIL_SIZE + JPEG_FRAME_ENC_MAX_APP_MARKER_SIZE
     * When we encode primary, TVMR first gives an estimate of the
     * required output size. We use TVMR's estimate to realloc the
     * original allocation.
     */
    bufferSize += JPEG_FRAME_ENC_MAX_THUMBNAIL_SIZE +
                    JPEG_FRAME_ENC_MAX_APP_MARKER_SIZE;

    return bufferSize;
}

static void
InitializeThumbnailQuantTables(NvJpegEncParameters *ThumbParams)
{
    /* Default quantization table for luma and chroma for thumbnail encoding
    * to make sure that for 320x240 thumbnail image, the encoded data
    * size is ~16k.
    */
    {
        NvU8 ThumbLumaQt[64] = { 3,  2,  2,  3,  5,  8, 10, 12,
                                 2,  2,  3,  4,  5, 12, 12, 11,
                                 3,  3,  3,  5,  8, 11, 14, 11,
                                 3,  3,  4,  6, 10, 17, 16, 12,
                                 4,  4,  7, 11, 14, 22, 21, 15,
                                 5,  7, 11, 13, 16, 21, 23, 18,
                                10, 13, 16, 17, 21, 24, 24, 20,
                                14, 18, 19, 20, 22, 20, 21, 20 };
        NvOsMemcpy(ThumbParams->CustomQuantTables.LumaQuantTable,
                   ThumbLumaQt, 64);
    }
    {
        NvU8 ThumbChromaQt[64] = { 3,   4,  5,  9, 20, 20, 20, 20,
                                   4,   4,  5, 13, 20, 20, 20, 20,
                                   5,   5, 11, 20, 20, 20, 20, 20,
                                   9,  13, 20, 20, 20, 20, 20, 20,
                                  20,  20, 20, 20, 20, 20, 20, 20,
                                  20,  20, 20, 20, 20, 20, 20, 20,
                                  20,  20, 20, 20, 20, 20, 20, 20,
                                  20,  20, 20, 20, 20, 20, 20, 20 };
        NvOsMemcpy(&ThumbParams->CustomQuantTables.ChromaQuantTable,
                   ThumbChromaQt, 64);
    }
}

static NvError
Encoder_Open(
    NvImageEncHandle hImageEnc)
{
    NvError status = NvError_NotInitialized;
    Tegra3HwEncPvtCtx *pvtCtx = NULL;

    NvOsDebugPrintf("Encoder_Open ++\n");

    pvtCtx = NvOsAlloc(sizeof(Tegra3HwEncPvtCtx));
    if (!pvtCtx)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    NvOsMemset(pvtCtx, 0, sizeof(Tegra3HwEncPvtCtx));

#if SW_THUMBNAIL_ENCODING
    pvtCtx->bEnableSWEncodingForThumbnail = NV_TRUE;
#endif

    status = NvMMQueueCreate(&pvtCtx->InputBufferQ, MAX_QUEUE_SIZE * 2,
                sizeof(InputBuffer), NV_TRUE);
    if (status != NvSuccess) {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    status = NvMMQueueCreate(&pvtCtx->FeedInputBufferQ, MAX_QUEUE_SIZE * 2,
                sizeof(InputBuffer), NV_TRUE);
    if (status != NvSuccess) {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    status = NvMMQueueCreate(&pvtCtx->OutputQ, MAX_QUEUE_SIZE,
                sizeof(NvMMBuffer*), NV_TRUE);
    if (status != NvSuccess) {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    /* Create various semaphores for synchronization */
    status = NvOsSemaphoreCreate(&pvtCtx->hSemInputBufAvail, 0);
    if (status != NvSuccess) {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    /* Create various semaphores for synchronization */
    status = NvOsSemaphoreCreate(&pvtCtx->hSemFeedImageInputBufAvail, 0);
    if (status != NvSuccess) {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    if (pvtCtx->bEnableSWEncodingForThumbnail)
    {
        status = NvMMQueueCreate(&pvtCtx->InputSwThumbnailQ, MAX_QUEUE_SIZE,
                    sizeof(NvMMBuffer*), NV_TRUE);
        if (status != NvSuccess) {
            status = NvError_InsufficientMemory;
            goto cleanup;
        }

        /* Create various semaphores for synchronization */
        status = NvOsSemaphoreCreate(&pvtCtx->hSemThumbnailInputBufAvail, 0);
        if (status != NvSuccess) {
            status = NvError_InsufficientMemory;
            goto cleanup;
        }

        /* Create various semaphores for synchronization */
        status = NvOsSemaphoreCreate(&pvtCtx->hSemThumbnailEncodeDone, 0);
        if (status != NvSuccess) {
            status = NvError_InsufficientMemory;
            goto cleanup;
        }

        /* Create semaphores to indicate thumbnail encoding is done by SW in case
           sw is used for thumbnail Encoding*/
        status = NvOsSemaphoreCreate(&pvtCtx->hSemThumbnailSwOutputBufAvail, 0);
        if (status != NvSuccess) {
            status = NvError_InsufficientMemory;
            goto cleanup;
        }
    }

    /* create the TVMR device */
    pvtCtx->device = TVMRDeviceCreate(NULL);
    if (!pvtCtx->device) {
        status = NvError_JPEGEncHWError;
        goto cleanup;
    }

    if (!(pvtCtx->fence = TVMRFenceCreate()))
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    pvtCtx->jpegEncoder = TVMRJPEGEncoderCreate(
                            MAX_OUTPUT_BUFFERING,
                            MAX_BITSTREAM_SIZE);
    if (!pvtCtx->jpegEncoder)
    {
        status = NvError_JPEGEncHWError;
        goto cleanup;
    }

    hImageEnc->pEncoder->pPrivateContext = pvtCtx;
    hImageEnc->EncStartState = JPEG_ENC_START;

    /* Delivery thread for async operation */
    status = NvOsThreadCreate(OutputThread,
                    hImageEnc, &pvtCtx->hDeliveryThread);
    if (status != NvSuccess)
        goto cleanup;

    status = NvOsThreadCreate(FeedInputThread,
                    hImageEnc, &pvtCtx->hFeedImageThread);
    if (status != NvSuccess)
        goto cleanup;

    if (pvtCtx->bEnableSWEncodingForThumbnail)
    {
        /* Delivery thread for async operation */
        status = NvOsThreadCreate(ThumbnailEncodeThread,
                        hImageEnc, &pvtCtx->hDeliveryThumbnailThread);
        if (status != NvSuccess)
            goto cleanup;

        {
            NvImageEncOpenParameters OpenParams;
            OpenParams.pContext = hImageEnc;
            OpenParams.Type = NvImageEncoderType_SwEncoder;
            OpenParams.ClientIOBufferCB = JpegEncoderDeliverFullOutput;
            OpenParams.LevelOfSupport = THUMBNAIL_ENC;

            status = NvImageEnc_Open(&pvtCtx->SwImageEncoder, &OpenParams);
            if (status != NvSuccess)
                goto cleanup;


            NvOsSemaphoreSignal(pvtCtx->hSemThumbnailEncodeDone);
        }
    }

    NvOsDebugPrintf("Encoder_Open --\n");
    return NvSuccess;

cleanup:
    /* call the clean up */
    Encoder_Close(hImageEnc);
    return status;
};

static void
Encoder_Close(
    NvImageEncHandle hImageEnc)
{
    Tegra3HwEncPvtCtx *pEncCtx = NULL;

    NvOsDebugPrintf("Encoder_Close ++\n");

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return;
    }

    pEncCtx = (Tegra3HwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    /* Close the encoder and clean-up */
    pEncCtx->CloseEncoder = NV_TRUE;

    // ToDo
    /* Add condition for wait if there is any pending work */

    if (pEncCtx->SwImageEncoder)
    {
        NvImageEnc_Close(pEncCtx->SwImageEncoder);
        pEncCtx->SwImageEncoder= NULL;
    }

    /* Signal semaphore and wait for thread exit */
    if (pEncCtx->hSemInputBufAvail)
        NvOsSemaphoreSignal(pEncCtx->hSemInputBufAvail);
    if (pEncCtx->hDeliveryThread)
    {
        NvOsThreadJoin(pEncCtx->hDeliveryThread);
        pEncCtx->hDeliveryThread = NULL;
    }

    if (pEncCtx->hSemFeedImageInputBufAvail)
        NvOsSemaphoreSignal(pEncCtx->hSemFeedImageInputBufAvail);
    if (pEncCtx->hFeedImageThread)
    {
        NvOsThreadJoin(pEncCtx->hFeedImageThread);
        pEncCtx->hFeedImageThread = NULL;
    }

    if (pEncCtx->hSemThumbnailInputBufAvail)
        NvOsSemaphoreSignal(pEncCtx->hSemThumbnailInputBufAvail);
    if (pEncCtx->hDeliveryThumbnailThread)
    {
        NvOsThreadJoin(pEncCtx->hDeliveryThumbnailThread);
        pEncCtx->hDeliveryThumbnailThread = NULL;
    }

    if (pEncCtx->device)
    {
        TVMRDeviceDestroy(pEncCtx->device);
        pEncCtx->device = NULL;
    }

    if (pEncCtx->jpegEncoder)
    {
        TVMRJPEGEncoderDestroy(pEncCtx->jpegEncoder);
        pEncCtx->jpegEncoder = NULL;
    }

    if (pEncCtx->fence)
    {
        TVMRFenceDestroy(pEncCtx->fence);
        pEncCtx->fence = 0;
    }

    if (pEncCtx->device)
    {
        TVMRDeviceDestroy(pEncCtx->device);
        pEncCtx->device = NULL;
    }

    if (pEncCtx->hSemInputBufAvail)
    {
        NvOsSemaphoreDestroy(pEncCtx->hSemInputBufAvail);
        pEncCtx->hSemInputBufAvail = NULL;
    }

    if (pEncCtx->hSemFeedImageInputBufAvail)
    {
        NvOsSemaphoreDestroy(pEncCtx->hSemFeedImageInputBufAvail);
        pEncCtx->hSemFeedImageInputBufAvail = NULL;
    }

    if (pEncCtx->OutputQ)
    {
        NvMMQueueDestroy(&pEncCtx->OutputQ);
        pEncCtx->OutputQ = NULL;
    }

    if (pEncCtx->InputBufferQ)
    {
        NvMMQueueDestroy(&pEncCtx->InputBufferQ);
        pEncCtx->InputBufferQ = NULL;
    }

    if (pEncCtx->FeedInputBufferQ)
    {
        NvMMQueueDestroy(&pEncCtx->FeedInputBufferQ);
        pEncCtx->FeedInputBufferQ = NULL;
    }

    if(pEncCtx->pYuvFramePri)
    {
        int i;
        for (i = 0; i < 3; i++)
            NvOsFree (pEncCtx->pYuvFramePri->surfaces[i]);
        NvOsFree(pEncCtx->pYuvFramePri);
        pEncCtx->pYuvFramePri = NULL;
    }

    if(pEncCtx->pYuvFrameThumb)
    {
        int i;
        for (i = 0; i < 3; i++)
            NvOsFree (pEncCtx->pYuvFrameThumb->surfaces[i]);
        NvOsFree(pEncCtx->pYuvFrameThumb);
        pEncCtx->pYuvFrameThumb = NULL;
    }

    NvOsFree(pEncCtx);
    pEncCtx = NULL;
    NvOsDebugPrintf("Encoder_Close --\n");
};

static NvError
Encoder_GetParameters(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    Tegra3HwEncPvtCtx *pEncCtx = NULL;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return NvError_BadParameter;
    }

    pEncCtx = (Tegra3HwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    /* We do not need to query core JPEG,
     * just return the saved parameters from local context */
    if (params->EnableThumbnail == NV_TRUE)
        NvOsMemcpy(params, &hImageEnc->pEncoder->ThumbParams, sizeof(NvJpegEncParameters));
    else
        NvOsMemcpy(params, &hImageEnc->pEncoder->PriParams, sizeof(NvJpegEncParameters));

    return NvSuccess;
};

static NvError
Encoder_SetParameters(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    /* Gather all data and program the HW */
    /* For T20, run for-loop for the number of entries in the struct
     * and do setAttribute for individual params */
    NvError status = NvSuccess;
    Tegra3HwEncPvtCtx *pEncCtx = NULL;
    TVMRVideoSurface *pYuvFrame = NULL;
    NvJpegEncParameters *ImageParams = NULL;
    TVMRSurfaceType surfaceType;
    NvU32 NumSurfaces;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return NvError_BadParameter;
    }

    pEncCtx = (Tegra3HwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    if (params->EnableThumbnail == NV_TRUE)
    {
        NvOsMemcpy(&hImageEnc->pEncoder->ThumbParams, params, sizeof(NvJpegEncParameters));

        if (params->CustomQuantTables.Enable == NV_FALSE)
            InitializeThumbnailQuantTables(&hImageEnc->pEncoder->ThumbParams);

        ImageParams = params;
        params->EncodedSize = calculateOutPutBuffSize(ImageParams);
        // save encoded size so GetParam returns the right value
        hImageEnc->pEncoder->ThumbParams.EncodedSize = params->EncodedSize;
    }
    else
    {
        NvOsMemcpy(&hImageEnc->pEncoder->PriParams, params, sizeof(NvJpegEncParameters));
        ImageParams = params;
        params->EncodedSize = calculateOutPutBuffSize(ImageParams);
        // save encoded size so GetParam returns the right value
        hImageEnc->pEncoder->PriParams.EncodedSize = params->EncodedSize;
    }

    if (ImageParams->EnableThumbnail == NV_TRUE)
        pYuvFrame = pEncCtx->pYuvFrameThumb;
    else
        pYuvFrame = pEncCtx->pYuvFramePri ;

    if (ImageParams->EncSurfaceType == NV_IMG_JPEG_SURFACE_TYPE_NV12)
    {
        surfaceType = TVMRSurfaceType_NV12;
        NumSurfaces = 2;
    }
    else
    {
        surfaceType = TVMRSurfaceType_YV12;
        NumSurfaces = 3;
    }

    // Allocate once, otherwise just change the resolution
    if(!pYuvFrame)
    {
        NvU32 i;
        if ((ImageParams->Resolution.width < MAX_WIDTH) ||
            (ImageParams->Resolution.height < MAX_HEIGHT))
        {
            pYuvFrame = (TVMRVideoSurface*)NvOsAlloc(sizeof(TVMRVideoSurface));
            NvOsMemset(pYuvFrame, 0, sizeof(TVMRVideoSurface));
            if (!pYuvFrame)
            {
                NvOsDebugPrintf("%s : TVMRVideoSurfaceCreate failed\n", __func__);
                goto fail;
            }
            for (i = 0; i < NumSurfaces; i++)
            {
                pYuvFrame->surfaces[i] = (TVMRSurface*)NvOsAlloc(sizeof(TVMRSurface));
                if (!pYuvFrame->surfaces[i])
                {
                    NvOsDebugPrintf("%s : TVMRSurfaceCreate failed\n", __func__);
                    goto fail;
                }
            }

        }
        else
        {
            status = NvError_BadParameter;
            goto fail;
        }
     }

    pYuvFrame->type   = surfaceType;
    pYuvFrame->width  = ImageParams->Resolution.width;
    pYuvFrame->height = ImageParams->Resolution.height;

    if (ImageParams->EnableThumbnail == NV_TRUE)
        pEncCtx->pYuvFrameThumb = pYuvFrame;
    else
        pEncCtx->pYuvFramePri = pYuvFrame;

    if (ImageParams->CustomQuantTables.QTableType ==
            NV_IMG_JPEG_ENC_QuantizationTable_Luma)
    {
        pEncCtx->bSet_CustomQuantTables_Luma = NV_TRUE;
    }
    else if (ImageParams->CustomQuantTables.QTableType ==
                NV_IMG_JPEG_ENC_QuantizationTable_Chroma)
    {
        pEncCtx->bSet_CustomQuantTables_Chroma = NV_TRUE;
    }
    NvOsMemcpy(ImageParams->CustomQuantTables.LumaQuantTable,
            params->CustomQuantTables.LumaQuantTable, 64);
    NvOsMemcpy(ImageParams->CustomQuantTables.ChromaQuantTable,
            params->CustomQuantTables.ChromaQuantTable, 64);

    if (pEncCtx->bEnableSWEncodingForThumbnail == NV_TRUE)
    {
        status = NvImageEnc_SetParam(pEncCtx->SwImageEncoder, ImageParams);
    }

fail:
     return status;
};

static NvError
Encoder_Start(
    NvImageEncHandle hImageEnc,
    NvMMBuffer *pInputBuffer,
    NvMMBuffer *pOutputBuffer,
    NvBool IsThumbnail)
{

    /* Get all the buffers and enqueue it to the queue.
     * When we have all the buffers, we either block this
     * call, if we do not have a delivery thread, or signal
     * the worker thread to continue with the encoding and
     * filling preparing the final JPEG image */
    NvError status = NvError_NotInitialized;
    Tegra3HwEncPvtCtx *pEncCtx = NULL;
    NvU32 EncState = JPEG_ENC_START;
    NvBool AcceptOutput = NV_FALSE;
    NvMMBuffer *pOutputBufferThumbnail;
    InputBuffer InputBuff;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return NvError_BadParameter;
    }

    pEncCtx = (Tegra3HwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;
    EncState = hImageEnc->EncStartState;

#if CAPTURE_PROFILING
   if(EncState == JPEG_ENC_START)
    {
        NvOsDebugPrintf("log: Capture Request Starts Time In %d \n", NvOsGetTimeMS());
    }
#endif

    if (IsThumbnail)
    {
        EncState |= THUMBNAIL_ENC_DONE;
        if (pEncCtx->bEnableSWEncodingForThumbnail == NV_FALSE)
        {
            InputBuff.pInputBuffer = pInputBuffer;
            InputBuff.IsThumbnail = IsThumbnail;
#if FEED_IMAGE_AT_START
            status = NvMMQueueEnQ(pEncCtx->FeedInputBufferQ, &InputBuff, 0);
            if (status != NvSuccess)
                return status;
#endif
            status = NvMMQueueEnQ(pEncCtx->InputBufferQ, &InputBuff, 0);
        }
        else
        {
            status = NvMMQueueEnQ(pEncCtx->InputSwThumbnailQ, &pInputBuffer, 0);
        }
    }
    else
    {
        EncState |= PRIMARY_ENC_DONE;
        InputBuff.pInputBuffer = pInputBuffer;
        InputBuff.IsThumbnail = IsThumbnail;

#if FEED_IMAGE_AT_START
        status = NvMMQueueEnQ(pEncCtx->FeedInputBufferQ, &InputBuff, 0);
        if (status != NvSuccess)
            return status;
#endif
        status = NvMMQueueEnQ(pEncCtx->InputBufferQ, &InputBuff, 0);
    }
    if (status != NvSuccess)
        return status;

    if (((hImageEnc->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) && ((EncState == THUMBNAIL_ENC_DONE) || (EncState == PRIMARY_ENC_DONE))) ||
        ((hImageEnc->pEncoder->SupportLevel == PRIMARY_ENC) && (EncState == PRIMARY_ENC_DONE)))
        AcceptOutput = NV_TRUE;
    else
        AcceptOutput = NV_FALSE;

    if (AcceptOutput == NV_TRUE)
    {
        NvMMQueueEnQ(pEncCtx->OutputQ, &pOutputBuffer, 0);
    }

    if (pEncCtx->bEnableSWEncodingForThumbnail == NV_FALSE)
    {
#if FEED_IMAGE_AT_START
        NvOsSemaphoreSignal(pEncCtx->hSemFeedImageInputBufAvail);
#else
        NvOsSemaphoreSignal(pEncCtx->hSemInputBufAvail);
#endif
    }
    else
    {
        if (!IsThumbnail)
        {
#if FEED_IMAGE_AT_START
            NvOsSemaphoreSignal(pEncCtx->hSemFeedImageInputBufAvail);
#else
            NvOsSemaphoreSignal(pEncCtx->hSemInputBufAvail);
#endif
        }
        else
        {
            NvOsSemaphoreSignal(pEncCtx->hSemThumbnailInputBufAvail);
        }
    }

    if (((hImageEnc->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) && (EncState == (THUMBNAIL_ENC_DONE | PRIMARY_ENC_DONE))) ||
        ((hImageEnc->pEncoder->SupportLevel == PRIMARY_ENC) && (EncState == PRIMARY_ENC_DONE)))
    {
        hImageEnc->EncStartState = JPEG_ENC_START;
    }
    else
        hImageEnc->EncStartState = EncState;

    return status;
};


/**
 * return the functions for this particular encoder
 */
void
ImgEnc_Tegra3(NvImageEncHandle pContext)
{
    pContext->pEncoder->pfnOpen = Encoder_Open;
    pContext->pEncoder->pfnClose = Encoder_Close;
    pContext->pEncoder->pfnGetParam = Encoder_GetParameters;
    pContext->pEncoder->pfnSetParam = Encoder_SetParameters;
    pContext->pEncoder->pfnStartEncoding = Encoder_Start;
}

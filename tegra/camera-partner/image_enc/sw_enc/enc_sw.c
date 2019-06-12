/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "enc_sw.h"
#include "nvmm_queue.h"

#define MAX_BUFFER_SIZE   40 * 1024

typedef struct nvOmxDestMgrRec {
    struct jpeg_destination_mgr jMgr;
    NvU8 *data;
    NvU32 dataSize;
    NvU32 dataLen;
    NvBool isThumbnail;
} nvDestMgr;


typedef struct SwEncPvtCtxRec {

    /* input and output queues */
    NvMMQueueHandle     InputPrimaryQ;
    NvMMQueueHandle     OutputQ;
    NvMMQueueHandle     InputThumbnailQ;

    NvOsThreadHandle    hDeliveryThread;

    /* Synchronising semaphore */
    NvOsSemaphoreHandle hSemInputBufAvail;
    NvBool              CloseEncoder;
    NvBool              bSet_CustomQuantTables_Luma;
    NvBool              bSet_CustomQuantTables_Chroma;
    NvU8                *pOutBuffer;
    NvU32               DataLength;
    NvU32               BufferSize;
    NvU8                *LocalBuffer;
    NvBool              PullOutputBuffer;

} SwEncPvtCtx;


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

static NvError
Encoder_MapSurface(NvRmSurface *pSurf, NvU8 **ptr, const char *mark)
{
    NvError err = NvSuccess;
    NvU32 sSize = 0;

    sSize = NvRmSurfaceComputeSize(pSurf);

    err = NvRmMemMap(pSurf->hMem,
            pSurf->Offset, sSize, NVOS_MEM_READ_WRITE, (void **)ptr);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("%s: failed to map surface - %d\n",__FUNCTION__,err);
        return err;
    }

    NvRmMemCacheMaint(pSurf->hMem, *ptr, sSize, NV_FALSE, NV_TRUE);
    return err;
}

static void
Encoder_UnMapSurface(NvRmSurface *pSurf, NvU8 *ptr)
{
    NvU32 sSize = 0;

    sSize = NvRmSurfaceComputeSize(pSurf);
    NvRmMemCacheMaint(pSurf->hMem, ptr, sSize, NV_TRUE, NV_TRUE);
    NvRmMemUnmap(pSurf->hMem, ptr, sSize);
}

static void x_init_destination(j_compress_ptr cinfo)
{
    nvDestMgr *pMgr = (nvDestMgr *)cinfo->dest;
    pMgr->jMgr.next_output_byte = pMgr->data;
    pMgr->jMgr.free_in_buffer = pMgr->dataSize;
    pMgr->dataLen = 0;
}

static boolean x_empty_output_buffer(j_compress_ptr cinfo)
{
    nvDestMgr *pMgr = (nvDestMgr *)cinfo->dest;
    NvU8 *newBuffer = NULL;
    NvU32 newSize;

    pMgr->dataLen = pMgr->dataSize - pMgr->jMgr.free_in_buffer;
    if (pMgr->isThumbnail)
    {
        // thumbnails are small, increase by 16k
        newSize = pMgr->dataSize + 16*1024;
    }
    else
    {
        // fullsize jpegs can be big, increase by 1M
        newSize = pMgr->dataSize + 1*1024*1024;
    }
    newBuffer = (NvU8 *)NvOsRealloc(pMgr->data, newSize);
    if (newBuffer == NULL)
    {
        NvOsDebugPrintf("%s: CANNOT EXTEND BUFFER FOR JPEG DATA [%d => %d] bytes\n", __FUNCTION__,
                                (int)pMgr->dataSize, (int)newSize);
        return NV_FALSE;
    }
    pMgr->data = newBuffer;
    pMgr->dataSize = newSize;
    pMgr->jMgr.next_output_byte = pMgr->data + pMgr->dataLen;
    pMgr->jMgr.free_in_buffer = pMgr->dataSize - pMgr->dataLen;
    return NV_TRUE;
}

static void x_term_destination(j_compress_ptr cinfo)
{
    nvDestMgr *pMgr = (nvDestMgr *)cinfo->dest;

    pMgr->dataLen = pMgr->dataSize - pMgr->jMgr.free_in_buffer;
    pMgr->jMgr.next_output_byte = 0;
    pMgr->jMgr.free_in_buffer = 0;
}

static NvError SWEncode(NvImageEnc *pContext,
    NvMMBuffer *pInBuffer, NvMMBuffer *pOutBuffer,
    NvBool ThumbnailActive, NvU32 HdrLen)
{

    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    SwEncPvtCtx *pEncCtx =
        (SwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvU32 quality = 0;

    NvU32 width = pInBuffer->Payload.Surfaces.Surfaces[0].Width;
    NvU32 height = pInBuffer->Payload.Surfaces.Surfaces[0].Height;
    NvU32 pitchY = pInBuffer->Payload.Surfaces.Surfaces[0].Pitch;
    NvU32 pitchU = pInBuffer->Payload.Surfaces.Surfaces[1].Pitch;
    NvU32 pitchV = pInBuffer->Payload.Surfaces.Surfaces[2].Pitch;
    NvU8 *dataY = NULL, *dataU = NULL, *dataV = NULL;
    NvU8 *pY = NULL, *pU = NULL, *pV = NULL;
    NvU32 sSize = 0;
    NvError err = NvSuccess;
    NvU8 *buffer = NULL;
    NvU32 i = 0, halfWidth = width / 2;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    nvDestMgr myDestMgr;
    JSAMPROW row_pointer[2];  /* pointer to JSAMPLE row[s] */

    NvOsMemset(&myDestMgr, 0, sizeof(nvDestMgr));


#if CAPTURE_PROFILING
    NvOsDebugPrintf("Thumbnail SW Feed Time In %d \n", NvOsGetTimeMS());
#endif

    // Allocate scanlines buffer:
    buffer = (NvU8 *)NvOsAlloc(width * 3 * 2);
    if (buffer == NULL)
    {
        return NvError_InsufficientMemory;
    }

    if (ThumbnailActive == NV_TRUE)
    {
        quality = pEncPvt->ThumbParams.Quality;
    }
    else
    {
        quality = pEncPvt->PriParams.Quality;
    }

    // If NVRM mem is allocated, then map it!
    if (pInBuffer->Payload.Surfaces.Surfaces[0].hMem)
    {
        err = Encoder_MapSurface(&pInBuffer->Payload.Surfaces.Surfaces[0],&dataY,(const char *)"Y");
        if (err != NvSuccess)
        {
            goto finish;
        }
        err = Encoder_MapSurface(&pInBuffer->Payload.Surfaces.Surfaces[1],&dataU,(const char *)"U");
        if (err != NvSuccess)
        {
            goto finish;
        }
        err = Encoder_MapSurface(&pInBuffer->Payload.Surfaces.Surfaces[2],&dataV,(const char *)"V");
        if (err != NvSuccess)
        {
            goto finish;
        }
    } else
    {
        dataY = (NvU8 *)(pInBuffer->Payload.Surfaces.Surfaces[0].pBase);
        dataU = (NvU8 *)(pInBuffer->Payload.Surfaces.Surfaces[1].pBase);
        dataV = (NvU8 *)(pInBuffer->Payload.Surfaces.Surfaces[2].pBase);
    }

    row_pointer[0] = (JSAMPROW)buffer;
    row_pointer[1] = (JSAMPROW)(buffer + width*3);
    pY = dataY;
    pU = dataU;
    pV = dataV;

    myDestMgr.isThumbnail = ThumbnailActive;
    myDestMgr.data = pEncCtx->LocalBuffer;
    NvOsDebugPrintf("Local Buffer pointer 0x%x %d\n",pEncCtx->LocalBuffer, pEncCtx->BufferSize);
    myDestMgr.dataSize = pEncCtx->BufferSize;
    myDestMgr.dataLen = 0;
    myDestMgr.jMgr.init_destination = &x_init_destination;
    myDestMgr.jMgr.empty_output_buffer = &x_empty_output_buffer;
    myDestMgr.jMgr.term_destination = &x_term_destination;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    cinfo.dest = (struct jpeg_destination_mgr *)&myDestMgr;
    cinfo.image_width = width;        /* image width and height, in pixels */
    cinfo.image_height = height;
    cinfo.input_components = 3;       /* # of color components per pixel */
    cinfo.in_color_space = JCS_YCbCr; /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    // APP0 segment will break insertExifThumbnail, way too many
    // hardcoded values there.  fixing that func would be a good idea, but
    // requires significant effort, especially since none of the hardcoded
    // values are commented, so it's not obvious what many of them even mean.
    // it doesn't seem like we're using APP0 for most normal jpegs for now
    // anyways, so let's just disable it for these special SW encoder cases.
    cinfo.write_JFIF_header = FALSE;

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height)
    {
        JSAMPLE *row0 = row_pointer[0];
        JSAMPLE *row1 = row_pointer[1];
        JSAMPLE u,v;
        NvU8 *pY0 = pY;
        NvU8 *pY1 = pY + pitchY;
        for (i = 0; i < halfWidth; i++)
        {
            u = pU[i];
            v = pV[i];
            row0[0] = *pY0++;
            row0[3] = *pY0++;
            row1[0] = *pY1++;
            row1[3] = *pY1++;
            row0[1] = row0[4] = row1[1] = row1[4] = u;
            row0[2] = row0[5] = row1[2] = row1[5] = v;
            row0 += 6;
            row1 += 6;

        }
        pU += pitchU;
        pV += pitchV;
        pY += (pitchY * 2);

       (void) jpeg_write_scanlines(&cinfo, row_pointer, 2);
    }

finish:

    jpeg_finish_compress(&cinfo);
    pEncCtx->pOutBuffer = myDestMgr.data;
    myDestMgr.data = NULL;
    pEncCtx->BufferSize = myDestMgr.dataSize;
    pEncCtx->DataLength = myDestMgr.dataLen;

    jpeg_destroy_compress(&cinfo);

    NvOsFree(buffer);
    buffer = NULL;

    if (pInBuffer->Payload.Surfaces.Surfaces[0].hMem)
    {
        Encoder_UnMapSurface(&pInBuffer->Payload.Surfaces.Surfaces[0],dataY);
        Encoder_UnMapSurface(&pInBuffer->Payload.Surfaces.Surfaces[1],dataU);
        Encoder_UnMapSurface(&pInBuffer->Payload.Surfaces.Surfaces[2],dataV);
    }
    return err;
}

static NvError SWGetEncodedData(NvImageEnc *pContext,
    NvMMBuffer *pOutBuffer, NvBool ThumbnailActive, NvU32 HdrLen)
{
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    SwEncPvtCtx *pEncCtx =
        (SwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvU32 ImgOffset = 0;
    NvU32 numBytesAvailable = pEncCtx->DataLength;
    NvU32 totalSize = 0;

    HdrLen = GetHeaderLen(pContext, pEncPvt->HeaderParams.hExifInfo, &HdrLen);

    if ((pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) &&
        (NV_FALSE == ThumbnailActive))
    {
        ImgOffset = JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET;
#if 1
        pOutBuffer->Payload.Ref.startOfValidData =
            JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET -
        pEncPvt->HeaderParams.ThumbNailSize - HdrLen;
#endif
    }
    else if((pContext->pEncoder->SupportLevel == PRIMARY_ENC) &&
            (NV_FALSE == ThumbnailActive))
    {

        ImgOffset = HdrLen;
    }
    else
    {
        ImgOffset = JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET - numBytesAvailable;
        pOutBuffer->Payload.Ref.startOfValidData = ImgOffset - HdrLen;
    }

#if CAPTURE_PROFILING
    NvOsDebugPrintf("Thumbnail SW Memcpy Time In %d \n", NvOsGetTimeMS());
#endif

    NvOsMemcpy((NvU8 *)((NvU32)pOutBuffer->Payload.Ref.pMem + ImgOffset),
        pEncCtx->pOutBuffer, pEncCtx->DataLength);
    pEncCtx->DataLength = numBytesAvailable;

#if CAPTURE_PROFILING
    NvOsDebugPrintf("Thumbnail SW Memcpy Time Out %d \n", NvOsGetTimeMS());
#endif

    if (NV_TRUE == ThumbnailActive)
    {
        pEncPvt->HeaderParams.ThumbNailSize = numBytesAvailable;
        pEncPvt->ThumbParams.EncodedSize = numBytesAvailable;
    }

    pOutBuffer->Payload.Ref.sizeOfValidDataInBytes += numBytesAvailable;

#if CAPTURE_PROFILING
    NvOsDebugPrintf("Thumbnail SW Completes Time Out %d \n", NvOsGetTimeMS());
#endif
    return NvSuccess;

}


static void OutputThread(void *arg)
{
    NvImageEnc *pContext = (NvImageEnc *)arg;
    SwEncPvtCtx *pEncCtx =
        (SwEncPvtCtx *)pContext->pEncoder->pPrivateContext;
    NvEncoderPrivate *pEncPvt =
        (NvEncoderPrivate *)pContext->pEncoder;
    NvMMBuffer *pWorkBuffer = NULL;
    NvMMBuffer *pOutBuffer = NULL;
    NvError status = NvError_NotInitialized;
    NvU32 EncState = JPEG_ENC_START;
    NvU32 HdrLen = 0;  // Header length
    NvError SWStatus = NvSuccess;

    while (!pEncCtx->CloseEncoder)
    {
        NvBool ThumbnailActive = NV_FALSE;
        NvU32 PrimaryBufferCount = 0;
        NvU32 ThumbnailBufferCount = 0;

        // Wait until data is available
        NvOsSemaphoreWait(pEncCtx->hSemInputBufAvail);

        ThumbnailBufferCount = NvMMQueueGetNumEntries(pEncCtx->InputThumbnailQ);
        PrimaryBufferCount = NvMMQueueGetNumEntries(pEncCtx->InputPrimaryQ);

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

        if ((ThumbnailBufferCount > 0) &&
            (pEncPvt->ThumbParams.EnableThumbnail == NV_TRUE) &&
            ((EncState == JPEG_ENC_START) || (EncState == PRIMARY_ENC_DONE)))
        {
            /* Peek a Thumbnail buffer and load all Thumbnail image info */
            status = NvMMQueuePeek(pEncCtx->InputThumbnailQ, &pWorkBuffer);
            if (status != NvSuccess)
            {
                goto Exit;
            }

            ThumbnailActive = NV_TRUE;

            /* Should we do it here .. or at the end after it is actually complete ?? */
            EncState |= THUMBNAIL_ENC_DONE;

        }
        else if (((EncState == JPEG_ENC_START) ||
                  (EncState == THUMBNAIL_ENC_DONE)) &&
                 (NvMMQueueGetNumEntries(pEncCtx->InputPrimaryQ)))
        {
            /* Peek a Primary buffer and load all Primary image info */

            status = NvMMQueuePeek(pEncCtx->InputPrimaryQ, &pWorkBuffer);
            if (status != NvSuccess)
            {
                goto Exit;
            }

            /*NvOsMemcpy(LumaQuantTable,
                pEncCtx->PriParams.CustomQuantTables.LumaQuantTable, 64);
            NvOsMemcpy(ChromaQuantTable,
                pEncCtx->PriParams.CustomQuantTables.ChromaQuantTable, 64);*/

            ThumbnailActive = NV_FALSE;

            /* Should we do it here .. or at the end after it is actually complete ?? */
            EncState |= PRIMARY_ENC_DONE;
        }
        else
        {
            NvOsDebugPrintf("[Image-Enc] Continue .. Invalid Encoder state \n");
            continue;
        }

        if (pEncCtx->PullOutputBuffer)
        {
            /* We are ready for encoding .. we should check if we have any o/p
             * buffer available. We will onlyhave 1 output buffer per a complete
             * set of JPEG data going out */
            status = NvMMQueuePeek(pEncCtx->OutputQ, &pOutBuffer);
            if ((status != NvSuccess) || (pOutBuffer == NULL))
            {
                goto Exit;
            }
            // Reset sizeOfValidDataInBytes to ensure every output buffer
            // size does not contain accumulated sum of previous requests
            // output buffer sizes.
            pOutBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
            pOutBuffer->Payload.Ref.startOfValidData = 0;

            pEncPvt->HeaderParams.pbufHeader = (NvU8 *)(pOutBuffer->Payload.Ref.pMem);

            pEncCtx->PullOutputBuffer = NV_FALSE;
        }

        if(!pEncPvt->HeaderParams.hExifInfo)
        {
            /* Take the exif info.
             * We need it for the Header Leangth calculation */
            pEncPvt->HeaderParams.hExifInfo =
                pWorkBuffer->PayloadInfo.BufferMetaData.EXIFBufferMetadata.EXIFInfoMemHandle;
        }

        GetHeaderLen(pContext, pEncPvt->HeaderParams.hExifInfo, &HdrLen);

        SWStatus = SWEncode(pContext, pWorkBuffer, pOutBuffer, ThumbnailActive, HdrLen);
        if (SWStatus != NvSuccess)
        {
            goto Exit;
        }
        SWStatus = SWGetEncodedData(pContext, pOutBuffer, ThumbnailActive, HdrLen);
        if (SWStatus != NvSuccess)
        {
            goto Exit;
        }

        /* Do we have a complete buffer yet .. we may need to wait for the other set of data */
        NvOsDebugPrintf("[Image-Enc] Client requested %s. %s done\n",
            (pContext->pEncoder->SupportLevel == PRIMARY_ENC_DONE) ? "Primary Only" :
                (pContext->pEncoder->SupportLevel == THUMBNAIL_ENC_DONE) ? "Thumbnail Only" :
                    (pContext->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) ? "Thumbnail and Primary" : "Nothing",
            (EncState == PRIMARY_ENC_DONE) ? "Primary" :
                (EncState == THUMBNAIL_ENC_DONE) ? "Thumbnail" :
                    (EncState == JPEG_ENC_COMPLETE) ? "Thumbnail and Primary" : "Nothing");

        if (EncState != pContext->pEncoder->SupportLevel) {
            NvOsDebugPrintf("[Image-Enc] Continue .. Not ready to deliver \n");
            continue;
        }

Exit:
        if (PrimaryBufferCount > 0)
        {
            status = NvMMQueueDeQ(pEncCtx->InputPrimaryQ, &pWorkBuffer);
            pContext->ClientCB(pContext->pClientContext,
                NvImageEncoderStream_INPUT, sizeof(NvMMBuffer), pWorkBuffer, status);
        }
#if 0

        pEncPvt->HeaderParams.pbufHeader += pOutBuffer->Payload.Ref.startOfValidData;

        /* Compete the JPEG header before sending it outside */
        JPEGEncWriteHeader(pContext, pEncPvt->HeaderParams.pbufHeader);

        pOutBuffer->Payload.Ref.sizeOfValidDataInBytes += HdrLen;

#endif
        /* return O/p buffer */
        if (NvMMQueueGetNumEntries(pEncCtx->OutputQ))
        {
            status = NvMMQueueDeQ(pEncCtx->OutputQ, &pWorkBuffer);
            pContext->ClientCB(pContext->pClientContext,
                NvImageEncoderStream_OUTPUT, sizeof(NvMMBuffer), pOutBuffer, status);
        }
        /* It is safe to pull the next output buffer */
        pEncCtx->PullOutputBuffer = NV_TRUE;

        /* return I/p thumbnail buffer */
        if (ThumbnailBufferCount > 0)
        {
            status = NvMMQueueDeQ(pEncCtx->InputThumbnailQ, &pWorkBuffer);
            pContext->ClientCB(pContext->pClientContext,
                NvImageEncoderStream_THUMBNAIL_INPUT, sizeof(NvMMBuffer), pWorkBuffer, status);
        }

        /* Reset the encoder to start */
        pEncPvt->HeaderParams.hExifInfo = NULL;

        EncState = JPEG_ENC_START;

    }
}

static NvError Encoder_Open(
    NvImageEncHandle hImageEnc)
{
    NvError status = NvError_NotInitialized;
    SwEncPvtCtx *pvtCtx = NULL;

    pvtCtx = NvOsAlloc(sizeof(SwEncPvtCtx));
    if (!pvtCtx)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    NvOsMemset(pvtCtx, 0, sizeof(SwEncPvtCtx));

    pvtCtx->PullOutputBuffer = NV_TRUE;

    pvtCtx->LocalBuffer = NvOsAlloc(MAX_BUFFER_SIZE);
    if (!pvtCtx->LocalBuffer)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    pvtCtx->BufferSize = MAX_BUFFER_SIZE;

    /* Create input - output and thumbnail queue */
    status = NvMMQueueCreate(&pvtCtx->InputPrimaryQ, MAX_QUEUE_SIZE,
                sizeof(NvMMBuffer*), NV_TRUE);
    if (status != NvSuccess) {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    status = NvMMQueueCreate(&pvtCtx->InputThumbnailQ, MAX_QUEUE_SIZE,
                sizeof(NvMMBuffer*), NV_TRUE);
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

    /* Delivery thread for async operation */
    status = NvOsThreadCreate(OutputThread,
                    hImageEnc, &pvtCtx->hDeliveryThread);
    if (status != NvSuccess)
        goto cleanup;

    hImageEnc->pEncoder->pPrivateContext = pvtCtx;
    hImageEnc->EncStartState = JPEG_ENC_START;
    return NvSuccess;

cleanup:
    /* We cannot reach cleanup, if will fail thread creation */
    if (pvtCtx)
    {
        if (pvtCtx->hSemInputBufAvail)
        {
            NvOsSemaphoreDestroy(pvtCtx->hSemInputBufAvail);
            pvtCtx->hSemInputBufAvail = NULL;
        }

        if (pvtCtx->InputPrimaryQ)
        {
            NvMMQueueDestroy(&pvtCtx->InputPrimaryQ);
            pvtCtx->InputPrimaryQ = NULL;
        }

        if (pvtCtx->InputThumbnailQ)
        {
            NvMMQueueDestroy(&pvtCtx->InputThumbnailQ);
            pvtCtx->InputThumbnailQ = NULL;
        }

        if (pvtCtx->OutputQ)
        {
            NvMMQueueDestroy(&pvtCtx->OutputQ);
            pvtCtx->OutputQ = NULL;
        }

        NvOsFree(pvtCtx);
        pvtCtx = NULL;
    }
    return NvSuccess;
};

static void Encoder_Close(
    NvImageEncHandle hImageEnc)
{
    SwEncPvtCtx *pEncCtx = NULL;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return;
    }

    pEncCtx = (SwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    // ToDo
    /* Add condition for wait if there is any pending work */

    /* Close the encoder and clean-up */
    pEncCtx->CloseEncoder = NV_TRUE;

    /* Signal semaphore and wait for thread exit */
    NvOsSemaphoreSignal(pEncCtx->hSemInputBufAvail);
    if (pEncCtx->hDeliveryThread)
    {
        NvOsThreadJoin(pEncCtx->hDeliveryThread);
        pEncCtx->hDeliveryThread = NULL;
    }

    if (pEncCtx->hSemInputBufAvail)
    {
        NvOsSemaphoreDestroy(pEncCtx->hSemInputBufAvail);
        pEncCtx->hSemInputBufAvail = NULL;
    }

    if (pEncCtx->OutputQ)
    {
        NvMMQueueDestroy(&pEncCtx->OutputQ);
        pEncCtx->OutputQ = NULL;
    }

    if (pEncCtx->InputPrimaryQ)
    {
        NvMMQueueDestroy(&pEncCtx->InputPrimaryQ);
        pEncCtx->InputPrimaryQ = NULL;
    }

    if (pEncCtx->InputThumbnailQ)
    {
        NvMMQueueDestroy(&pEncCtx->InputThumbnailQ);
        pEncCtx->InputThumbnailQ = NULL;
    }

    if (pEncCtx->LocalBuffer)
    {
        NvOsFree(pEncCtx->LocalBuffer);
        pEncCtx->LocalBuffer = NULL;
    }

    NvOsFree(pEncCtx);
    pEncCtx = NULL;

};

static NvError Encoder_GetParameters(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    SwEncPvtCtx *pEncCtx = NULL;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return NvError_BadParameter;
    }

    pEncCtx = (SwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    /* We do not need to query core JPEG,
     * just return the saved parameters from local context */
    if (params->EnableThumbnail == NV_TRUE)
        NvOsMemcpy(params, &hImageEnc->pEncoder->ThumbParams, sizeof(NvJpegEncParameters));
    else
        NvOsMemcpy(params, &hImageEnc->pEncoder->PriParams, sizeof(NvJpegEncParameters));

    return NvSuccess;
};

static NvError Encoder_SetParameters(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    /* Gather all data and program the HW */
    /* For T20, run for-loop for the number of entries in the struct
     * and do setAttribute for individual params */
    NvError status = NvSuccess;
    SwEncPvtCtx *pEncCtx = NULL;
    NvJpegEncParameters *ImageParams = NULL;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return NvError_BadParameter;
    }

    pEncCtx = (SwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    if (params->EnableThumbnail == NV_TRUE)
    {
        NvOsMemcpy(&hImageEnc->pEncoder->ThumbParams, params, sizeof(NvJpegEncParameters));

        //ImageParams = params;
        //params->EncodedSize = calculateOutPutBuffSize(ImageParams);
        // save encoded size so GetParam returns the right value
        //hImageEnc->pEncoder->ThumbParams.EncodedSize = params->EncodedSize;
    }
    else
    {
        NvOsMemcpy(&hImageEnc->pEncoder->PriParams, params, sizeof(NvJpegEncParameters));
        //ImageParams = params;
        //params->EncodedSize = calculateOutPutBuffSize(ImageParams);
        // save encoded size so GetParam returns the right value
        //hImageEnc->pEncoder->PriParams.EncodedSize = params->EncodedSize;
    }

fail:
     return status;
};

static NvError Encoder_Start(
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
    SwEncPvtCtx *pEncCtx = NULL;
    NvU32 EncState = JPEG_ENC_START;
    NvBool AcceptOutput = NV_FALSE;

    if ((hImageEnc == NULL) ||
        (hImageEnc->pEncoder->pPrivateContext == NULL))
    {
        return NvError_BadParameter;
    }
    pEncCtx = (SwEncPvtCtx *)hImageEnc->pEncoder->pPrivateContext;

    EncState = hImageEnc->EncStartState;
    if (IsThumbnail)
    {
        EncState |= THUMBNAIL_ENC_DONE;
        status = NvMMQueueEnQ(pEncCtx->InputThumbnailQ, &pInputBuffer, 0);
    }
    else
    {
        EncState |= PRIMARY_ENC_DONE;
        status = NvMMQueueEnQ(pEncCtx->InputPrimaryQ, &pInputBuffer, 0);
    }

    if (((hImageEnc->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) && ((EncState == THUMBNAIL_ENC_DONE) || (EncState == PRIMARY_ENC_DONE))) ||
        ((hImageEnc->pEncoder->SupportLevel == PRIMARY_ENC) && (EncState == PRIMARY_ENC_DONE)) ||
        ((hImageEnc->pEncoder->SupportLevel == THUMBNAIL_ENC) && (EncState == THUMBNAIL_ENC_DONE)))
        AcceptOutput = NV_TRUE;
    else
        AcceptOutput = NV_FALSE;

    if (AcceptOutput == NV_TRUE)
    {
        NvMMQueueEnQ(pEncCtx->OutputQ, &pOutputBuffer, 0);
    }

    NvOsSemaphoreSignal(pEncCtx->hSemInputBufAvail);

    if (((hImageEnc->pEncoder->SupportLevel == JPEG_ENC_COMPLETE) && (EncState == (THUMBNAIL_ENC_DONE | PRIMARY_ENC_DONE))) ||
        ((hImageEnc->pEncoder->SupportLevel == PRIMARY_ENC) && (EncState == PRIMARY_ENC_DONE)))
    {
        hImageEnc->EncStartState = JPEG_ENC_START;
    }
    else
        hImageEnc->EncStartState = EncState;

    return NvSuccess;
};


/**
 * return the functions for this particular encoder
 */
void ImgEnc_swEnc(NvImageEncHandle pContext)
{
    pContext->pEncoder->pfnOpen = Encoder_Open;
    pContext->pEncoder->pfnClose = Encoder_Close;
    pContext->pEncoder->pfnGetParam = Encoder_GetParameters;
    pContext->pEncoder->pfnSetParam  = Encoder_SetParameters;
    pContext->pEncoder->pfnStartEncoding = Encoder_Start;
}

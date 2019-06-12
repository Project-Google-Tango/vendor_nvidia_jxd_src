/*
 * Copyright (c) 2006-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxVideoPostProcess.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include <nvassert.h>
#include <nvos.h>
#include <NvxTrace.h>
#include <NvxComponent.h>
#include <NvxPort.h>
#include <NvxCompReg.h>
#include <nvmmlitetransformbase.h>
#include "nvxlitevideopostprocess.h"

#include "NvxHelpers.h"
#include "nvxliteandroidbuffer.h"

#if defined(__ARM_NEON__)
#include <arm_neon.h>
#endif // __ARM_NEON__

//#undef NV_DEBUG_PRINTF
//#define NV_DEBUG_PRINTF(x) NvOsDebugPrintf x

//#define APPLY_PREPROCESSBLIT

#ifdef APPLY_PREPROCESSBLIT
static void ConvertBLtoPLBuffer(NvMMSurfaceDescriptor* pSrcSurface, NvMMSurfaceDescriptor* pOutSurf, NvRect* CropRect)
{
    NvError Err;
    NvDdk2dBlitParameters TexParam;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dSurface *pDstDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle h2d = NvxGet2d();
    NvDdk2dSurfaceType Type1;
    NvDdk2dSurfaceType Type2;

    if (!h2d)
        return;

    if ((pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_U8_V8)
       || (pSrcSurface->Surfaces[1].ColorFormat == NvColorFormat_V8_U8))
       Type1 = NvDdk2dSurfaceType_Y_UV;
    else
       Type1 = NvDdk2dSurfaceType_Y_U_V;

    if ((pOutSurf->Surfaces[1].ColorFormat == NvColorFormat_U8_V8)
       || (pOutSurf->Surfaces[1].ColorFormat == NvColorFormat_V8_U8))
       Type2 = NvDdk2dSurfaceType_Y_UV;
    else
       Type2 = NvDdk2dSurfaceType_Y_U_V;

    NvxLock2d();
    Err = NvDdk2dSurfaceCreate(h2d, Type1,
                               &(pSrcSurface->Surfaces[0]), &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    Err = NvDdk2dSurfaceCreate(h2d, Type2,
                               &(pOutSurf->Surfaces[0]), &pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }


    Err = NvDdk2dBlit(h2d, pDstDdk2dSurface, NULL,
                      pSrcDdk2dSurface, NULL, NULL);
    if (NvSuccess != Err)
    {
        goto L_cleanup;
    }
    NvDdk2dSurfaceLock(pDstDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_ReadWrite,
                       NULL,
                       NULL,
                       NULL);
    NvDdk2dSurfaceUnlock(pDstDdk2dSurface, NULL, 0);

L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);
    if (pDstDdk2dSurface)
         NvDdk2dSurfaceDestroy(pDstDdk2dSurface);

    NvxUnlock2d();
}
#endif

static NvError VPPInit(SNvxNvMMLiteVPP *pVpp)
{
    NvU32 i = 0, j = 0;
    NvError status = NvSuccess;
    NvxMutexCreate(&(pVpp->oVPPMutex));

    pVpp->importBufferID = 0;
    for (i = 0; i < NUM_VPP_SURFACES; i++)
    {
        pVpp->pVideoSurface[i] =  (VPPSurface*)NvOsAlloc(sizeof(VPPSurface));
        if (!pVpp->pVideoSurface[i])
        {
            NV_DEBUG_PRINTF(("VPPInit : Unable to allocate the structure VPP VideoSurface %d \n", i));
            status = NvError_InsufficientMemory;
            goto fail;
        }
        pVpp->pVideoSurface[i]->width  = pVpp->frameWidth;
        pVpp->pVideoSurface[i]->height = pVpp->frameHeight;
    }

    return status;

fail:
    {
        NvU32 k;
        for (k = 0; k < NUM_VPP_SURFACES; k++)
        {
            if (pVpp->pVideoSurface[k])
            {
                NvOsFree(pVpp->pVideoSurface[k]);
                pVpp->pVideoSurface[k] = NULL;
            }
        }
        return status;
      }
  return status;
}


NvError NvxVPPcreate(void *arg)
{
    NvError status = NvSuccess;
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)arg;

        pData->sVpp.bVppThreadActive = OMX_TRUE;
        pData->sVpp.bVppThreadClose = OMX_FALSE;
        pData->sVpp.bVppStop  = OMX_TRUE;
        pData->sVpp.colorformat = OMX_COLOR_FormatYUV420SemiPlanar;//Hardcodec now for T124

        status = NvMMQueueCreate(&pData->sVpp.pReleaseOutSurfaceQueue, MAX_NVMM_BUFFERS, sizeof(NvMMBuffer*), NV_FALSE);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("VPP : unable to create the queue for output buffers from decoder \n"));
            goto fail;
        }
        status = NvMMQueueCreate(&pData->sVpp.pOutSurfaceQueue, MAX_NVMM_BUFFERS, sizeof(NvMMBuffer*), NV_FALSE);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("VPP : unable to create the queue for output buffers from decoder \n"));
            goto fail;
        }

        status = NvOsSemaphoreCreate(&(pData->sVpp.VppInpOutAvail), 0);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("VPP : unable to create the semaphore for VppInpOutAvail \n"));
            goto fail;
        }
        status = NvOsSemaphoreCreate(&(pData->sVpp.VppInitDone), 0);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("VPP : unable to create the semaphore for VppInitDone \n"));
            goto fail;
        }
        status = NvOsSemaphoreCreate(&(pData->sVpp.VppThreadPaused), 0);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("VPP : unable to create the semaphore for VppThreadPaused \n"));
            goto fail;
        }
        status = NvOsThreadCreate(NvxVPPThread, pData, &pData->sVpp.hVppThread);
        if (status != NvSuccess)
        {
            pData->sVpp.bVppThreadActive = OMX_FALSE;
            NV_DEBUG_PRINTF(("Vpp : unable to create the VPP thread \n"));
            goto fail;
        }
        status = NvOsSemaphoreWaitTimeout(pData->sVpp.VppInitDone, 2000);
        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("Vpp :  VPPInit is not proper \n"));
            goto fail;
        }
        return status;
fail:
      NvxVPPdestroy(pData);
      return status;
}

void NvxVPPdestroy(void *arg)
{
    NvError status = NvSuccess;
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)arg;
    NvU32 j;

        pData->sVpp.bVppThreadClose = OMX_TRUE;
        pData->sVpp.bVppThreadActive = OMX_FALSE;
        pData->sVpp.bVppStop  = OMX_TRUE;
        if (pData->sVpp.hVppThread)
        {
            NvOsThreadJoin(pData->sVpp.hVppThread);
            pData->sVpp.hVppThread = NULL;
        }
        // related for VPP
        if (pData->sVpp.VppInpOutAvail)
        {
            NvOsSemaphoreDestroy(pData->sVpp.VppInpOutAvail);
            pData->sVpp.VppInpOutAvail = NULL;
        }
        if (pData->sVpp.VppInitDone)
        {
            NvOsSemaphoreDestroy(pData->sVpp.VppInitDone);
            pData->sVpp.VppInitDone = NULL;
        }
        if (pData->sVpp.VppThreadPaused)
        {
            NvOsSemaphoreDestroy(pData->sVpp.VppThreadPaused);
            pData->sVpp.VppThreadPaused = NULL;
        }
        if (pData->sVpp.pReleaseOutSurfaceQueue)
        {
            NvMMQueueDestroy(&pData->sVpp.pReleaseOutSurfaceQueue);
            pData->sVpp.pReleaseOutSurfaceQueue = NULL;
        }
        if (pData->sVpp.pOutSurfaceQueue)
        {
            NvMMQueueDestroy(&pData->sVpp.pOutSurfaceQueue);
            pData->sVpp.pOutSurfaceQueue = NULL;
        }
        VPPClose(&(pData->sVpp));
        NV_DEBUG_PRINTF(("NvxVPPdestroy Done"));
}

void NvxVPPFlush(void* arg)
{
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)arg;
    NvMMBuffer *OutSurface;
    OMX_U32 j;
    pData->sVpp.bVppThreadtopuase = OMX_TRUE;
    NvOsSemaphoreSignal(pData->sVpp.VppInpOutAvail);
    NvOsSemaphoreWaitTimeout(pData->sVpp.VppThreadPaused,10);
    NvU32 QueueEntries = NvMMQueueGetNumEntries(pData->sVpp.pOutSurfaceQueue);
    for (j = 0; j < QueueEntries; j++)
    {
         NvMMQueueDeQ(pData->sVpp.pOutSurfaceQueue, &OutSurface);
    }
    QueueEntries = NvMMQueueGetNumEntries(pData->sVpp.pReleaseOutSurfaceQueue);
    for (j = 0; j < QueueEntries; j++)
    {
        NvMMQueueDeQ(pData->sVpp.pReleaseOutSurfaceQueue, &OutSurface);
        pData->sVpp.Renderstatus[OutSurface->BufferID] = FRAME_FREE;
    }
}

void NvxVPPThread(void* arg)
{
    NvError status = NvSuccess;
    SNvxNvMMLiteTransformData *pData = (SNvxNvMMLiteTransformData *)arg;
    NvxComponent *pComp = pData->pParentComp;
    NvU32 streamIndex = 1;
    NvU32 RendBufId;
    SNvxNvMMLitePort *pPort = &(pData->oPorts[streamIndex]);
    NvxPort *pPortOut = pPort->pOMXPort;
    NvU32 InEntries = 0;
    NvU32 OutEntries = 0;
    NvU32 availInpOutCount = 0;
    NvMMBuffer *InSurface;
    NvMMBuffer *OutSurface;
    NvU32 size;
#ifdef APPLY_PREPROCESSBLIT
    NvMMBuffer *pnvbuf;
    NvxSurface *pSurface;
    NvU16 Kind = NvRmMemKind_Generic_16Bx2;
    NvU16 BlockHeightLog2 = 4;
    NvU32 ImageSize = 0;
#endif
    status = VPPInit(&(pData->sVpp));
    status = VppHALInit(pData);
    if (status == NvSuccess)
        NvOsSemaphoreSignal(pData->sVpp.VppInitDone);
    while(pData->sVpp.bVppThreadClose == OMX_FALSE)
    {
        status = NvOsSemaphoreWaitTimeout(pData->sVpp.VppInpOutAvail, 100);
        if (status == NvError_Timeout)
            continue;

        NvxMutexLock(pComp->hWorkerMutex);
        if (pData->sVpp.bVppThreadtopuase == OMX_TRUE)
        {
            NvOsSemaphoreSignal(pData->sVpp.VppThreadPaused);
            pData->sVpp.bVppStop = OMX_TRUE;
            pData->sVpp.bVppThreadtopuase = OMX_FALSE;
            NvxMutexUnlock(pComp->hWorkerMutex);
            continue;
        }
        if ((pData->sVpp.bVppStop == OMX_TRUE) || (pComp->ePendingState == OMX_StatePause))
        {
            if (!pPort->bEOS)
            {
                NvxMutexUnlock(pComp->hWorkerMutex);
                continue;
            }
        }
        InEntries = NvMMQueueGetNumEntries(pData->sVpp.pReleaseOutSurfaceQueue);
        OutEntries = NvMMQueueGetNumEntries(pData->sVpp.pOutSurfaceQueue);
        NV_DEBUG_PRINTF(("Entries: In %d Out %d",InEntries,OutEntries));
        if ((OutEntries > availInpOutCount) && (InEntries > availInpOutCount) && (!pData->bFlushing))
        {
            if (pData->sVpp.bVppThreadClose == OMX_TRUE)
            {
                NV_DEBUG_PRINTF(("%s[%d]", __func__, __LINE__));
                NvxMutexUnlock(pComp->hWorkerMutex);
                return ;
            }
            NvMMQueueDeQ(pData->sVpp.pReleaseOutSurfaceQueue, &InSurface);
            if(pData->sVpp.Renderstatus[InSurface->BufferID] == FRAME_FREE)
            {
                NV_DEBUG_PRINTF(("%s[%d]", __func__, __LINE__));
                NvxMutexUnlock(pComp->hWorkerMutex);
                continue;
            }
            NvMMQueueDeQ(pData->sVpp.pOutSurfaceQueue, &OutSurface);

           NV_DEBUG_PRINTF(("%s[%d]", __func__, __LINE__));
           pData->sVpp.pCurrSurface = InSurface;
           size = sizeof(InSurface->PayloadInfo);
           NvOsMemcpy(&(InSurface->PayloadInfo),&(pData->sVpp.pCurrSurface->PayloadInfo),size);
#ifdef APPLY_PREPROCESSBLIT


    pnvbuf =  NvOsAlloc(sizeof(NvMMBuffer));
    pSurface = (NvxSurface *)(&pnvbuf->Payload.Surfaces);
    status = NvxSurfaceAllocYuv(
            pSurface,
            InSurface->Payload.Surfaces.Surfaces[0].Width,
            InSurface->Payload.Surfaces.Surfaces[0].Height,
            NvMMVideoDecColorFormat_YUV420SemiPlanar,
            NvRmSurfaceLayout_Pitch,
            &ImageSize,
            NV_TRUE,
            Kind,
            BlockHeightLog2,
            NV_FALSE);

    pnvbuf->StructSize = sizeof(NvMMBuffer);
    pnvbuf->BufferID = 0;
    pnvbuf->PayloadType = NvMMPayloadType_SurfaceArray;
    pnvbuf->Payload.Surfaces.fences = NULL;

    NvOsMemset(&pnvbuf->PayloadInfo, 0, sizeof(pnvbuf->PayloadInfo));
    NvOsMemcpy(&(pnvbuf->PayloadInfo),&(pData->sVpp.pCurrSurface->PayloadInfo),size);
    NV_DEBUG_PRINTF(("PL Surface created"));
/*
    if(InSurface->Payload.Surfaces.fences->outFence.SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
        NvOsDebugPrintf("Waiting on outfence");
        NvRmFenceWait(pData->hRmDevice, &InSurface->Payload.Surfaces.fences->outFence, NV_WAIT_INFINITE);
    }
    NvOsDebugPrintf("Done Waiting on outfence");
*/

    ConvertBLtoPLBuffer(&InSurface->Payload.Surfaces, &pnvbuf->Payload.Surfaces, &InSurface->Payload.Surfaces.CropRect);
    NV_DEBUG_PRINTF(("2D blit done"));
            status =  VppHALProcess(pData,pnvbuf,OutSurface);
#else
            status =  VppHALProcess(pData,InSurface,OutSurface);
#endif
#ifdef APPLY_PREPROCESSBLIT
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
      NvOsFree(pnvbuf);
      pnvbuf = NULL;

     NV_DEBUG_PRINTF("InSurface addr is %p and BufferId is %d", InSurface, InSurface->BufferID);
     pData->sVpp.Rendstatus[InSurface->BufferID] = FRAME_SENT_FOR_DISPLAY;
#endif
            NV_DEBUG_PRINTF(("%s[%d]", __func__, __LINE__));
            //Trigger worker function to process buffer queue.
            NvxWorkerTrigger(&( pPortOut->pNvComp->oWorkerData));

            status |= ReturnUsedNvMMSurface(&(pData->sVpp),pData->sVpp.pCurrSurface);
            if(status != NvSuccess)
            {
                NvxMutexUnlock(pComp->hWorkerMutex);
                continue ;
            }
        }
        NvxMutexUnlock(pComp->hWorkerMutex);
    }
    NV_DEBUG_PRINTF(("Calling VPPHALDestroy"));
    VppHALDestroy(pData);
}


void VPPClose(SNvxNvMMLiteVPP *pVpp)
{
    NvU32 i, j;
    pVpp->pCurrSurface = NULL;
    NvxMutexDestroy(pVpp->oVPPMutex);
    pVpp->oVPPMutex = NULL;

    for(i = 0; i < NUM_VPP_SURFACES; i++)
    {
        if (pVpp->pVideoSurface[i])
        {
            NvOsFree(pVpp->pVideoSurface[i]);
            pVpp->pVideoSurface[i] = NULL;
        }
    }
}

NvMMBuffer* GetFreeNvMMSurface(SNvxNvMMLiteVPP *pVpp)
{
     int i;
     int Id = 0;

     NvxMutexLock(pVpp->oVPPMutex);
     if(pVpp->pCurrSurface)
        Id = pVpp->pCurrSurface->BufferID;

     for(i=Id; i<TOTAL_NUM_SURFACES; i++)
     {
         if((pVpp->Renderstatus[i] == FRAME_FREE) &&
             (pVpp->pVideoSurf[i]))
         {
            pVpp->Renderstatus[i] = FRAME_SENT_FOR_DISPLAY;
            NvxMutexUnlock(pVpp->oVPPMutex);
            return pVpp->pVideoSurf[i];
         }
     }
     for(i=0; i<Id; i++)
     {
         if((pVpp->Renderstatus[i] == FRAME_FREE) &&
             (pVpp->pVideoSurf[i]))
         {
             pVpp->Renderstatus[i] = FRAME_SENT_FOR_DISPLAY;
             NvxMutexUnlock(pVpp->oVPPMutex);
             return pVpp->pVideoSurf[i];
         }
     }

    NvxMutexUnlock(pVpp->oVPPMutex);
    return NULL;
}

NvError ReturnUsedNvMMSurface(SNvxNvMMLiteVPP *pVpp, NvMMBuffer* buffer)
{
     int i;
     NvError status = NvSuccess;

     for(i=0; i<TOTAL_NUM_SURFACES; i++)
     {
         if((buffer) && (pVpp->Renderstatus[i] == FRAME_SENT_FOR_DISPLAY) &&
             (pVpp->pVideoSurf[i] == buffer))
         {
             pVpp->Renderstatus[i] = FRAME_FREE;
             return status;
         }
     }
     return NvError_BadValue;
}


/* Copyright (c) 2006-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxVideoRender.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "common/NvxComponent.h"
#include "common/NvxTrace.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvmm.h"
#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "NvxHelpers.h"
#include "nvxeglimage.h"
#include <math.h>

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

/* State information */
typedef struct SNvxVideoScheduleData
{
    OMX_BOOL bSizeSet;

    OMX_BOOL bSentTimeRequest;
    OMX_U32  nNumDropped;

    OMX_S32 nAVSyncOffset;
    OMX_U32 nFramesSinceLastDrop;

    OMX_BOOL bNoAVSync;
    OMX_U32  nFrameDropFreq;
    OMX_U32  nTotFrameDrops;
} SNvxVideoScheduleData;


#define IN_PORT      0
#define OUT_PORT     1
#define CLOCK_PORT   2
#define MAX_BUFFERS 15
#define MIN_BUFSIZE 1024
#define MAX_BUFSIZE (640*480*3)/2

OMX_ERRORTYPE NvxVideoSchedulerInit(OMX_IN  OMX_HANDLETYPE hComponent);

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */


static OMX_ERRORTYPE NvxVSFlush(NvxComponent *pNvComp, OMX_U32 nPort)
{
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVSFlush\n"));
    if (nPort == -1 || nPort == IN_PORT)
    {
        SNvxVideoScheduleData *pNvxData;
        pNvxData = (SNvxVideoScheduleData *)pNvComp->pComponentData;

        pNvxData->bSentTimeRequest = OMX_FALSE;
    }

    return OMX_ErrorNone;
}
        
static OMX_ERRORTYPE NvxVideoSchedulerDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;    
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoSchedulerDeInit\n"));
    NvOsFree(pNvComp->pComponentData); 
    pNvComp->pComponentData = 0; 
    return eError;
}

static OMX_ERRORTYPE NvxVSSetParameter(OMX_IN NvxComponent *pNvComp,
                                       OMX_IN OMX_INDEXTYPE nIndex,
                                       OMX_IN OMX_PTR pComponentParameterStructure)
{
    NV_ASSERT(pNvComp);

    switch (nIndex)
    {
        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)pComponentParameterStructure;

            if(OUT_PORT == pPortDef->nPortIndex)
            {
                pNvComp->pPorts[OUT_PORT].nMinBufferCount = pPortDef->nBufferCountMin;
            }
            break;
        }
        default:
            break;
    }

    return NvxComponentBaseSetParameter(pNvComp, nIndex, pComponentParameterStructure);
}

static OMX_ERRORTYPE NvxVSSetConfig(NvxComponent *pNvComp, OMX_INDEXTYPE nIndex,
                                    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVideoScheduleData *pNvxData = 0;

    NV_ASSERT(pNvComp);

    pNvxData = (SNvxVideoScheduleData *)pNvComp->pComponentData;

    switch (nIndex)
    {
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            pNvxData->bNoAVSync = pProf->bNoAVSync;
            pNvxData->nAVSyncOffset = pProf->nAVSyncOffset;
            if (pProf->nFrameDrop > 0)
                pNvxData->nFrameDropFreq = pProf->nFrameDrop;
            break;
        }
        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }

    return eError;
}

static void NvxCopyInToOut(NvxPort *pIn, NvxPort *pOut)
{
    OMX_BUFFERHEADERTYPE *pInBuffer = pIn->pCurrentBufferHdr;
    OMX_BUFFERHEADERTYPE *pOutBuffer = pOut->pCurrentBufferHdr;

    OMX_U32 uBytes = (OMX_U32)(pInBuffer->nFilledLen);

    NVXTRACE((NVXT_CALLGRAPH, NVXT_VIDEO_SCHEDULER, "+NvxCopyInToOut\n"));
    NvxPortCopyMetadata(pIn, pOut);
    pOutBuffer->nFilledLen = uBytes;

    if (!pOut->hTunnelComponent)
    {
        NvMMBuffer *PayloadBuffer = (NvMMBuffer *)pInBuffer->pBuffer;
        NvxBufferPlatformPrivate *pPrivateData = 
            (NvxBufferPlatformPrivate *)pOutBuffer->pPlatformPrivate;

        if (pPrivateData->eType == NVX_BUFFERTYPE_EGLIMAGE)
        {
            // Stretch Blit source to EGL image handle
            NvxEglImageSiblingHandle hEglSib = 
                 (NvxEglImageSiblingHandle)pOutBuffer->pBuffer;

            NvxEglStretchBlitToImage(hEglSib,
                &(PayloadBuffer->Payload.Surfaces),
                NV_TRUE, NULL, 0);
        }
        else
        {
            // When not tunnelling
            NvxCopyMMBufferToOMXBuf(PayloadBuffer, NULL,  pOutBuffer);
        }
    }
    else if (pInBuffer->pBuffer != pOutBuffer->pBuffer)
    {
        NvOsMemcpy(pOutBuffer->pBuffer, pInBuffer->pBuffer, uBytes);
    }
}

static OMX_ERRORTYPE NvxVideoSchedulerWorkerFunction(OMX_IN NvxComponent *hComponent, OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, 
    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVideoScheduleData *pNvxData = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;    
    OMX_BUFFERHEADERTYPE *pInBuffer, *pClockBuffer;
    NvxPort* pClockPort = &hComponent->pPorts[CLOCK_PORT];
    NvxPort* pVideoInPort = &hComponent->pPorts[IN_PORT];
    NvxPort* pVideoOutPort = &hComponent->pPorts[OUT_PORT];
    OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE oClockRequest;
    OMX_BOOL bDeliver = OMX_FALSE;
    
    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxVideoSchedulerWorkerFunction\n"));
    pNvxData = (SNvxVideoScheduleData *)hComponent->pComponentData;

    NV_ASSERT(pNvxData);

    pInBuffer = pVideoInPort->pCurrentBufferHdr;

    if (!pInBuffer && !(pClockPort && pClockPort->pCurrentBufferHdr))
        return eError;

    if (pInBuffer && !(pVideoOutPort && pVideoOutPort->pCurrentBufferHdr))
        return eError;

    if (pInBuffer && pInBuffer->nFlags & OMX_BUFFERFLAG_EOS)
        bDeliver = OMX_TRUE;

    if (pInBuffer && !pNvxData->bSizeSet && pVideoInPort->hTunnelComponent)
    {
        OMX_PARAM_PORTDEFINITIONTYPE oPortDefinition;
        oPortDefinition.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        oPortDefinition.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
        oPortDefinition.nPortIndex = pVideoInPort->nTunnelPort;
        eError = OMX_GetParameter(pVideoInPort->hTunnelComponent,
                                  OMX_IndexParamPortDefinition,
                                  &oPortDefinition);

        NvOsMemcpy(&(pVideoInPort->oPortDef.format.video),
                   &(oPortDefinition.format.video),
                   sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
        NvOsMemcpy(&(pVideoOutPort->oPortDef.format.video),
                   &(oPortDefinition.format.video),
                   sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
        pNvxData->bSizeSet = OMX_TRUE;
    }

    if (pClockPort && pClockPort->pCurrentBufferHdr)
    {
        OMX_TIME_MEDIATIMETYPE *pUpdate;
        pClockBuffer = pClockPort->pCurrentBufferHdr;

        if (pClockBuffer->nFilledLen != sizeof(OMX_TIME_MEDIATIMETYPE))
        {
            NvxPortReleaseBuffer(pClockPort, pClockBuffer);
            return OMX_ErrorNone;
        }

        /* did we get a response to our clock request? */
        pUpdate = (OMX_TIME_MEDIATIMETYPE *)(void *)(pClockBuffer->pBuffer);
        if (pUpdate->eUpdateType == OMX_TIME_UpdateRequestFulfillment)
        {
            bDeliver = OMX_TRUE;
        }
        else 
            NvxPortReleaseBuffer(pClockPort, pClockPort->pCurrentBufferHdr);    
    }

    if (!pNvxData->bNoAVSync && !bDeliver && pInBuffer && pClockPort && 
        pClockPort->hTunnelComponent && pClockPort->oPortDef.bEnabled && 
        pClockPort->bNvidiaTunneling)
    {
        OMX_BOOL bDrop = OMX_FALSE;
        OMX_BOOL bVidLate = OMX_FALSE;
        OMX_TICKS ts = pInBuffer->nTimeStamp + pNvxData->nAVSyncOffset * 1000;
        if (ts < 0)
            ts = 0;

        NvxCCWaitUntilTimestamp(pClockPort->hTunnelComponent, ts, &bDrop,
                                &bVidLate);

        if (bDrop &&
            pNvxData->nFramesSinceLastDrop > pNvxData->nFrameDropFreq &&
            pNvxData->nFrameDropFreq != (OMX_U32)-1)
        {
            if (pInBuffer->pBuffer == pVideoOutPort->pCurrentBufferHdr->pBuffer)
            {
                pVideoOutPort->pCurrentBufferHdr->nFilledLen = 0;
                NvxPortDeliverFullBuffer(pVideoOutPort, 
                                         pVideoOutPort->pCurrentBufferHdr);
            }
            NvxPortReleaseBuffer(pVideoInPort, pInBuffer);
            pVideoInPort->pCurrentBufferHdr = pInBuffer = NULL;
            bDeliver = OMX_FALSE;
            pNvxData->nFramesSinceLastDrop = 0;
            if (ts > OMX_TICKS_PER_SECOND)
                pNvxData->nTotFrameDrops++;
        }
        else
            bDeliver = OMX_TRUE;
    }

    if (bDeliver && pInBuffer)
    {
        OMX_BOOL bSent = OMX_FALSE;

        if (pVideoOutPort->pCurrentBufferHdr && pInBuffer)
        {
            NvxCopyInToOut(pVideoInPort, pVideoOutPort);

            /* send the video buffer on. */
            eError = NvxPortDeliverFullBuffer(pVideoOutPort, 
                                              pVideoOutPort->pCurrentBufferHdr);

            if (NvxIsSuccess(eError))
            {
                NvxPortReleaseBuffer(pVideoInPort, pInBuffer);
                pVideoInPort->pCurrentBufferHdr = NULL;
                if (pClockPort && pClockPort->pCurrentBufferHdr)
                    NvxPortReleaseBuffer(pClockPort, pClockPort->pCurrentBufferHdr);
                pNvxData->bSentTimeRequest = OMX_FALSE;
                bSent = OMX_TRUE;
                pNvxData->nFramesSinceLastDrop++;
            }
        }

        if (!bSent)
        {
            /* try again */
            *puMaxMsecToNextCall = 5;
            eError = OMX_ErrorNone;
            return eError;
        }
    }

    if (pClockPort && pClockPort->hTunnelComponent && pVideoInPort->pCurrentBufferHdr)
    {
        /* Send out a clock request if we've got a buffer and haven't sent one
           for this buffer yet */
        if (pNvxData->bSentTimeRequest == OMX_FALSE)
        {
            pInBuffer = pVideoInPort->pCurrentBufferHdr;

            oClockRequest.nSize = sizeof(oClockRequest);
            oClockRequest.nVersion = hComponent->oSpecVersion;
            oClockRequest.nPortIndex = pClockPort->nTunnelPort;
            oClockRequest.nMediaTimestamp = pInBuffer->nTimeStamp;
            oClockRequest.pClientPrivate = 0;
            oClockRequest.nOffset = 0;

            eError = OMX_SetConfig(pClockPort->hTunnelComponent, OMX_IndexConfigTimeMediaTimeRequest, &oClockRequest);
            if (eError != OMX_ErrorNone)
            {
                *puMaxMsecToNextCall = 1;
                *pbMoreWork = OMX_FALSE;
                eError = OMX_ErrorNone;
            }
            else {
                pNvxData->bSentTimeRequest = OMX_TRUE;
                *pbMoreWork = OMX_FALSE;
            }
        }
    }

    NvxPortGetNextHdr(pVideoInPort);
    NvxPortGetNextHdr(pVideoOutPort);
    if (pVideoInPort->pCurrentBufferHdr && pVideoOutPort->pCurrentBufferHdr)
        *pbMoreWork = OMX_TRUE;

    return eError;
}

OMX_ERRORTYPE NvxVideoSchedulerInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxVideoScheduleData *pNvxData;

    eError = NvxComponentCreate(hComponent, 3, &pNvComp);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxVideoSchedulerInit:"
            ":eError = %x :[%s(%d)]\n",eError, __FILE__, __LINE__));
        return eError;
    }
    pNvComp->eObjectType        = NVXT_VIDEO_SCHEDULER;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoSchedulerInit\n"));   

    pNvComp->DeInit = NvxVideoSchedulerDeInit;        
    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVideoScheduleData)); 
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    pNvxData = (SNvxVideoScheduleData *)pNvComp->pComponentData;

    NvOsMemset(pNvxData, 0, sizeof(SNvxVideoScheduleData));

    pNvxData->nFrameDropFreq = 5;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.video.scheduler";
    pNvComp->sComponentRoles[0] = "video_scheduler.binary";
    pNvComp->nComponentRoles = 1;
    pNvComp->WorkerFunction = NvxVideoSchedulerWorkerFunction;  
    pNvComp->Flush = NvxVSFlush;
    pNvComp->SetConfig = NvxVSSetConfig;
    pNvComp->SetParameter = NvxVSSetParameter;

    NvxPortInitVideo(&pNvComp->pPorts[IN_PORT], OMX_DirInput, MAX_BUFFERS, MIN_BUFSIZE, OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[IN_PORT].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[IN_PORT], MAX_BUFSIZE);

    NvxPortInitVideo(&pNvComp->pPorts[OUT_PORT], OMX_DirOutput, MAX_BUFFERS, MIN_BUFSIZE, OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[OUT_PORT].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[OUT_PORT], MAX_BUFSIZE);

    /* setup buffer sharing */
    pNvComp->pPorts[IN_PORT].nSharingCandidates = 1;
    pNvComp->pPorts[IN_PORT].pSharingCandidates[0] = &pNvComp->pPorts[OUT_PORT];
    pNvComp->pPorts[OUT_PORT].nSharingCandidates = 1;
    pNvComp->pPorts[OUT_PORT].pSharingCandidates[0] = &pNvComp->pPorts[IN_PORT];

    /* If NVIDIA tunnel & video renderer is supplier, we can support GFSDK surface passing */
    pNvComp->pPorts[IN_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;
    pNvComp->pPorts[OUT_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    /* init clock port */
    NvxPortInitOther(&pNvComp->pPorts[CLOCK_PORT], OMX_DirInput, 4, sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
 
    return eError;
}


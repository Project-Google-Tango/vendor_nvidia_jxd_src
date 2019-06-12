/* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxVirtualComponent.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "common/NvxComponent.h"
#include "common/NvxTrace.h"
#include "nvos.h"
#include "nvassert.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

/* State information */
typedef struct SNvxVirtualComponentData
{
    OMX_U32    ostate;

} SNvxVirtualComponentData;


#define IN_PORT      0
#define OUT_PORT     1
#define REQ_BUFFERS  2
#define MAX_BUFFERS 20
#define MIN_BUFSIZE 1024
#define MAX_BUFSIZE (640*480*3)/2
#define OMX_TIMEOUT_MS 5
#define NVX_SCHEDULER_CLOCK_PORT 2

OMX_ERRORTYPE NvxVidSchedulerInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoPostProcessorInit(OMX_IN  OMX_HANDLETYPE hComponent);

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static OMX_ERRORTYPE NvxVirtualComponentDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVirtualComponentDeInit\n"));
    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxCopyInToOut(OMX_BUFFERHEADERTYPE *pBufferHdr,  NvxPort *pIn, NvxPort *pOut)
{
    OMX_BUFFERHEADERTYPE *pOutBuffer;
    OMX_U32 uBytes = (OMX_U32)(pBufferHdr->nFilledLen);
    NvxPortGetNextHdr(pOut);
    pOutBuffer = pOut->pCurrentBufferHdr;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_VIDEO_POSTPROCESSOR, "+NvxCopyInToOut\n"));

    if (pOutBuffer)
    {
        /* Copy marks, timestamps, flags, etc between current buffers */
        pOutBuffer->hMarkTargetComponent = pBufferHdr->hMarkTargetComponent;
        pOutBuffer->pMarkData = pBufferHdr->pMarkData;
        pOutBuffer->nTimeStamp = pBufferHdr->nTimeStamp;
        pOutBuffer->nFlags = pBufferHdr->nFlags;
        pIn->bSendEvents = OMX_FALSE;
        pOut->bSendEvents = OMX_TRUE;
        pOut->uMetaDataCopyCount = 0;
        pIn->pMetaDataSource = pIn;
        pOut->pMetaDataSource = pIn->pMetaDataSource; /* this is for marks, so we can send on last output */
        pOut->pMetaDataSource->uMetaDataCopyCount++;
        pOutBuffer->nFilledLen = uBytes;

        NvOsMemcpy(pOutBuffer->pBuffer, pBufferHdr->pBuffer, uBytes);

        return OMX_ErrorNone;
    }

    return OMX_ErrorBadParameter;
}

static OMX_ERRORTYPE NvxVirtualComponentFillThisBuffer(NvxComponent *hComponent, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE status = OMX_ErrorNone;
    SNvxVirtualComponentData *pNvxData = 0;
    OMX_BUFFERHEADERTYPE *pBuffer;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvxPort* pVideoInPort = &hComponent->pPorts[IN_PORT];
    pNvxData = (SNvxVirtualComponentData *)hComponent->pComponentData;
    NV_ASSERT(pNvxData);

    if (NvMMQueueGetNumEntries(pVideoInPort->pFullBuffers) > 0)
    {
        status = NvMMQueueDeQ(pVideoInPort->pFullBuffers, (void *) &pBuffer);
        status = NvxPortReleaseBuffer(pVideoInPort, pBuffer);
    }
    return status;
}

static OMX_ERRORTYPE NvxVirtualComponentGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxVirtualComponentData *pNvxData;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    pNvxData = (SNvxVirtualComponentData *)pComponent->pComponentData;
    NV_ASSERT(pNvxData);
    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
        {
            pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
            NvxPort* pVideoInPort = &pComponent->pPorts[IN_PORT];
            if((NvU32)OUT_PORT == pPortDef->nPortIndex && pVideoInPort && pVideoInPort->hTunnelComponent)
            {
                OMX_PARAM_PORTDEFINITIONTYPE oPortDefinition;
                //NvxPort* pVideoInPort = &pComponent->pPorts[IN_PORT];
                oPortDefinition.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
                oPortDefinition.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
                oPortDefinition.nPortIndex = pVideoInPort->nTunnelPort;
                eError = OMX_GetParameter(pVideoInPort->hTunnelComponent,
                                  OMX_IndexParamPortDefinition,
                                  &oPortDefinition);
                NvOsMemcpy(&(pVideoInPort->oPortDef.format.video),
                     &(oPortDefinition.format.video),
                    sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
                NvOsMemcpy(&(pPortDef->format.video), &(oPortDefinition.format.video),sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
                pVideoInPort->oPortDef.nBufferCountMin= pPortDef->nBufferCountMin = oPortDefinition.nBufferCountMin;
                pVideoInPort->oPortDef.nBufferCountActual= pPortDef->nBufferCountActual = oPortDefinition.nBufferCountActual;
                pVideoInPort->oPortDef.nBufferSize= pPortDef->nBufferSize = oPortDefinition.nBufferSize;

            }
            else
                return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            break;
        }
        default:
            return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVirtualComponentEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxVirtualComponentData *pNvxVirtualComponent = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort* pVideoInPort = &pNvComp->pPorts[IN_PORT];
    NvxPort* pVideoOutPort = &pNvComp->pPorts[OUT_PORT];

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVirtualComponentEmptyThisBuffer\n"));

    *bHandled = OMX_FALSE;

    pNvxVirtualComponent = (SNvxVirtualComponentData *)pNvComp->pComponentData;
    if((pNvxVirtualComponent->ostate == OMX_StateExecuting) &&
       (pBufferHdr->nInputPortIndex != NVX_SCHEDULER_CLOCK_PORT))
    {
        eError = NvxCopyInToOut(pBufferHdr, pVideoInPort, pVideoOutPort);
        if (NvxIsSuccess(eError))
        {
            /* send the video buffer to tunneled port of next component. */
            eError = NvxPortDeliverFullBuffer(pVideoOutPort,
                                              pVideoOutPort->pCurrentBufferHdr);
            if (NvxIsSuccess(eError))
            {
                pVideoOutPort->pCurrentBufferHdr = NULL;
                eError = NvxPortEmptyThisBuffer(pVideoInPort, pBufferHdr);
                *bHandled = (eError == OMX_ErrorNone);
            }
        }
        eError = OMX_ErrorNone;
    }

    return eError;
}

static OMX_ERRORTYPE NvxVirtualComponentPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxVirtualComponentData *pNvxData = 0;

    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVirtualComponentPreChangeState\n"));
    pNvxData = (SNvxVirtualComponentData *)pNvComp->pComponentData;
    pNvxData->ostate = oNewState;

    return eError;
}

static OMX_ERRORTYPE NvxVirtualComponentCommonInit(OMX_IN  OMX_HANDLETYPE hComponent, OMX_U32 nPorts)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxVirtualComponentData *pNvxData;

    eError = NvxComponentCreate(hComponent, nPorts, &pNvComp);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxVirtualComponentInit:"
            ":eError = %x :[%s(%d)]\n",eError, __FILE__, __LINE__));
        return eError;
    }
    pNvComp->eObjectType        = NVXT_VIDEO_POSTPROCESSOR;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVirtualComponentInit\n"));

    pNvComp->DeInit = NvxVirtualComponentDeInit;
    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVirtualComponentData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    pNvxData = (SNvxVirtualComponentData *)pNvComp->pComponentData;

    NvOsMemset(pNvxData, 0, sizeof(SNvxVirtualComponentData));

    /* Register function pointers */
    pNvComp->FillThisBufferCB   = NvxVirtualComponentFillThisBuffer;
    pNvComp->GetParameter       = NvxVirtualComponentGetParameter;
    pNvComp->EmptyThisBuffer    = NvxVirtualComponentEmptyThisBuffer;
    pNvComp->PreChangeState     = NvxVirtualComponentPreChangeState;

    pNvxData->ostate = OMX_StateLoaded;
    NvxPortInitVideo(&pNvComp->pPorts[IN_PORT], OMX_DirInput, REQ_BUFFERS, MIN_BUFSIZE, OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[IN_PORT].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[IN_PORT], MAX_BUFSIZE);

    NvxPortInitVideo(&pNvComp->pPorts[OUT_PORT], OMX_DirOutput, REQ_BUFFERS, MIN_BUFSIZE, OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[OUT_PORT].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[OUT_PORT], MAX_BUFSIZE);

    /* If NVIDIA tunnel & video renderer is supplier, we can support GFSDK surface passing */
    pNvComp->pPorts[IN_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;
    pNvComp->pPorts[OUT_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    return eError;
}

OMX_ERRORTYPE NvxVideoPostProcessorInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    NvxComponent *pNvComp;
    eError = NvxVirtualComponentCommonInit(hComponent, 2);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxVideoPostProcessorInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }
    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    pNvComp->pComponentName = "OMX.Nvidia.video.PostProcessor";
    pNvComp->sComponentRoles[0] = "video_PostProcessor.binary";
    pNvComp->sComponentRoles[1] = "iv_processor.yuv";
    pNvComp->nComponentRoles = 2;
    return eError;
}

OMX_ERRORTYPE NvxVidSchedulerInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    NvxComponent *pNvComp;
    eError = NvxVirtualComponentCommonInit(hComponent, 3);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxVidSchedulerInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }
    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    NvxPortInitOther(&pNvComp->pPorts[NVX_SCHEDULER_CLOCK_PORT], OMX_DirInput, 4,
                     sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pNvComp->pPorts[NVX_SCHEDULER_CLOCK_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;
    pNvComp->pPorts[NVX_SCHEDULER_CLOCK_PORT].oPortDef.bEnabled = OMX_TRUE;

    pNvComp->pComponentName = "OMX.Nvidia.vid.Scheduler";
    pNvComp->sComponentRoles[0] = "video_scheduler.binary";
    pNvComp->nComponentRoles = 1;
    return eError;
}


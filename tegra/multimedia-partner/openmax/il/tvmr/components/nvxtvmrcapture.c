/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** nvxtvmrcapture.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "nvassert.h"
#include "nvmm.h"
#include "nvmm_videodec.h"
#include "NvxHelpers.h"
#include "nvfxmath.h"
#include "nvxeglimage.h"
#include "tvmr.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */
typedef struct NvxEglImageSiblingStruct NvxEglImageSibling;

struct NvxEglImageSiblingStruct
{
    NvRmSurface surf;
    NvDdk2dSurface* ddk2dSurf;
    NvEglImageSyncObj* syncObj;
    NvWsiPixmap* pixmap;
};

typedef enum {
    BUFFER_TYPE_EGLIMAGE,
    BUFFER_TYPE_NVMM,
    BUFFER_TYPE_TVMR,
    BUFFER_TYPE_RAW
} EBufferType;

#define MIN_INPUT_BUFFERS    4
#define MIN_OUTPUT_BUFFERS   10
#define MIN_INPUT_BUFFERSIZE  (16 * 16 * 3) / 2
#define MIN_OUTPUT_BUFFERSIZE (16 * 16 * 3) / 2
#define MAX_OUTPUT_SURFACES  4


#define NUM_CAPTURE_FIELDS  (12)
/* Number of internal frame buffers (video memory) */
#define MAX_PORTS 1
static const int s_nvx_num_ports            = 1;
static const int s_nvx_port_output          = 0;

typedef struct VideoSurfaceList VideoSurfaceList;

#define MAX_CAPTURE_TIMEOUT 1000 // 1 second timeout

struct VideoSurfaceList
{
    TVMRVideoSurface *Surf;
    VideoSurfaceList *Next;
};

/* decoder state information */
typedef struct SNvxVideoCapture
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;

    OMX_U32  uDestinationWidth;
    OMX_U32  uDestinationHeight;
    NvU32    eDestinationColorFormat;

    NvxSurface *pDestinationSurface;

    //store locally to free them when release resources is done.
    NvRect               oCropRect;
    OMX_BOOL             bDestRgb;
    OMX_BOOL             bUseEglImage[MAX_PORTS];
    TVMRDevice*          hDevice;
    TVMRVideoMixer*      hMixer;
    TVMRCapture*         pCapture;
    TVMRSurfaceType      captureSurfaceType;
    TVMRFence            fenceDone;
    TVMRVideoSurface     *pPrev2VideoSurface;
    TVMRVideoSurface     *pPrevVideoSurface;
    TVMRVideoSurface     *pCurrVideoSurface;
    TVMRVideoSurface     *pNextVideoSurface;
    TVMRVideoSurface     *pReleaseVideoSurface;
    OMX_BOOL              bCaptureStarted;
    OMX_BOOL              bSecondField;
    TVMRPictureStructure  ePictureStructure;
    OMX_DeinterlaceMethod eDeinterlaceType;
    TVMRCaptureFormat     eCaptureFormat;
    OMX_BOOL              bUseNvBuffer;
    OMX_BOOL              bEmbedRmSurface;
    OMX_HANDLETYPE        hMutex;
    VideoSurfaceList      *pVideoSurfList;
    OMX_U32               uFreeCaptureBuffers;

    OMX_BOOL              bOutputYuvOverlay;
    NvMMQueueHandle       pOutputSurfaceQueue;
    NvxSurface           **ppSurfaceBuffers;
    EBufferType           eBufferType[MAX_PORTS];

} SNvxVideoCaptureData;


static NvError
NvxStretchBlitImage (SNvxVideoCaptureData *pNvxVideoCapture, NvxSurface *pDestinationSurface);

static void
NvxAllocateScratchSurfaceYuv(SNvxVideoCaptureData *pNvxVideoCapture, NvxSurface **ppSurface);

static
OMX_ERRORTYPE NvxVideoCaptureDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoCaptureInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

static
OMX_ERRORTYPE NvxVideoCaptureGetParameter(OMX_IN NvxComponent *pComponent,
                                          OMX_IN OMX_INDEXTYPE nParamIndex,
                                          OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxVideoCaptureData *pNvxVideoCapture;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pNvxVideoCapture = (SNvxVideoCaptureData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoCapture);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoCaptureGetParameter\n"));

    switch (nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        {
            eError = NvxComponentBaseGetParameter(pComponent,
                                                  nParamIndex,
                                                  pComponentParameterStructure);
        }
        break;
    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return eError;
}

static
OMX_ERRORTYPE NvxVideoCaptureSetParameter(OMX_IN NvxComponent *pComponent,
                                          OMX_IN OMX_INDEXTYPE nIndex,
                                          OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxVideoCaptureData *pNvxVideoCapture;
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoCaptureSetParameter\n"));

    pNvxVideoCapture = (SNvxVideoCaptureData *)pComponent->pComponentData;
    switch(nIndex)
    {
    case NVX_IndexParamDeinterlaceMethod:
    {
         NVX_PARAM_DEINTERLACE *deinterlaceparam;
         deinterlaceparam = (NVX_PARAM_DEINTERLACE*) pComponentParameterStructure;
         pNvxVideoCapture->eDeinterlaceType = deinterlaceparam->DeinterlaceMethod;
    }
    break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;

        if (pPortDef->format.video.nFrameHeight == 480)
        {
            pNvxVideoCapture->eCaptureFormat =  TVMR_CAPTURE_FORMAT_NTSC;
        }
        else
        {
            pNvxVideoCapture->eCaptureFormat = TVMR_CAPTURE_FORMAT_PAL;
        }

        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }

    case NVX_IndexConfigUseNvBuffer:
    {
        NVX_PARAM_USENVBUFFER* pParam = (NVX_PARAM_USENVBUFFER*)pComponentParameterStructure;

        if (pParam->bUseNvBuffer)
        {
            if (pParam->nPortIndex == (unsigned)s_nvx_port_output)
            {
                pNvxVideoCapture->bUseNvBuffer = OMX_TRUE;
            }
            else
            {
                return OMX_ErrorBadParameter;
            }
        }
    }
    break;

    case NVX_IndexParamEmbedRmSurface:
    {
        OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
        if (param)
        {
            pNvxVideoCapture->bEmbedRmSurface = param->bEnabled;
        }

    }
       break;

    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoCaptureGetConfig(OMX_IN NvxComponent* pNvComp,
                                       OMX_IN OMX_INDEXTYPE nIndex,
                                       OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxVideoCaptureData *pNvxVideoCapture;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoCaptureGetConfig\n"));

    pNvxVideoCapture = (SNvxVideoCaptureData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoCaptureSetConfig(OMX_IN NvxComponent* pNvComp,
                                       OMX_IN OMX_INDEXTYPE nIndex,
                                       OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxVideoCaptureData *pNvxVideoCapture;
    OMX_CONFIG_RECTTYPE *pCropRect;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoCaptureSetConfig\n"));

    pNvxVideoCapture = (SNvxVideoCaptureData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pNvxVideoCapture->bOverrideTimestamp = pTimestamp->bOverrideTimestamp;
            break;
        }
        case OMX_IndexConfigCommonInputCrop:
        {
            pCropRect = (OMX_CONFIG_RECTTYPE*) pComponentConfigStructure;
            pNvxVideoCapture->oCropRect.left  = pCropRect->nLeft;
            pNvxVideoCapture->oCropRect.top   = pCropRect->nTop;
            pNvxVideoCapture->oCropRect.right = pCropRect->nWidth;
            pNvxVideoCapture->oCropRect.bottom= pCropRect->nHeight;
            break;
        }
        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static
NvError NvxStretchBlitImage (SNvxVideoCaptureData *pNvxVideoCapture,
                             NvxSurface *pDestinationSurface)
{
    TVMRStatus status;

    if (pNvxVideoCapture->bOutputYuvOverlay)
    {
        TVMRVideoSurface oVideoSurface;
        TVMRSurface otvmrSurface[3];
        oVideoSurface.type   = TVMRSurfaceType_YV12;
        oVideoSurface.width  = pDestinationSurface->Surfaces[0].Width;
        oVideoSurface.height = pDestinationSurface->Surfaces[0].Height;
        oVideoSurface.surfaces[0] = &otvmrSurface[0];
        oVideoSurface.surfaces[1] = &otvmrSurface[1];
        oVideoSurface.surfaces[2] = &otvmrSurface[2];
        oVideoSurface.surfaces[0]->pitch = pDestinationSurface->Surfaces[0].Pitch;
        oVideoSurface.surfaces[1]->pitch = pDestinationSurface->Surfaces[2].Pitch;
        oVideoSurface.surfaces[2]->pitch = pDestinationSurface->Surfaces[1].Pitch;

        /* Interchange U/V, Its needed for Gstreamer Video Overlay Sink which uses
           tvmrFlipQueues for display , Both this mixer and decoder can feed surfaces
           to overlay component. So, its better to keep these surfaces in one common order
           before sending these surfaces to overlay component */
        oVideoSurface.surfaces[0]->priv = (void*)&pDestinationSurface->Surfaces[0];
        oVideoSurface.surfaces[1]->priv = (void*)&pDestinationSurface->Surfaces[2];
        oVideoSurface.surfaces[2]->priv = (void*)&pDestinationSurface->Surfaces[1];

        status =
            TVMRVideoMixerRenderYUV(
                pNvxVideoCapture->hMixer,              // *vmr,
                &oVideoSurface,                        // *outputSurface,
                pNvxVideoCapture->ePictureStructure,   // TVMR_PICTURE_STRUCTURE_FRAME,
                pNvxVideoCapture->pNextVideoSurface,   // *next,
                pNvxVideoCapture->pCurrVideoSurface,   // *curr,
                pNvxVideoCapture->pPrevVideoSurface,   // *prev,
                pNvxVideoCapture->pPrev2VideoSurface,  // *prev2,
                NULL,                                  // *vidSrcRect,
                NULL,                                  //m_pTVMRFenceMixerReadList,
                pNvxVideoCapture->fenceDone            // *fencesWait,  /* NULL terminated list */
            );
    }
    else
    {
        TVMROutputSurface oOutputSurface;
        TVMRSurface oTvmrOutputSurface;

        oOutputSurface.type = TVMRSurfaceType_R8G8B8A8;
        oOutputSurface.width  = pDestinationSurface->Surfaces[0].Width;
        oOutputSurface.height = pDestinationSurface->Surfaces[0].Height;
        oOutputSurface.surface = &oTvmrOutputSurface;
        oOutputSurface.surface->pitch = pDestinationSurface->Surfaces[0].Pitch;
        oOutputSurface.surface->priv = (void*)&pDestinationSurface->Surfaces[0];


        status =
            TVMRVideoMixerRender(
                pNvxVideoCapture->hMixer,               // *vmr,
                &oOutputSurface,                        // *outputSurface,
                NULL,                                   // *outputRect,
                pNvxVideoCapture->ePictureStructure,    // TVMR_PICTURE_STRUCTURE_FRAME,
                pNvxVideoCapture->pNextVideoSurface,    // *next,
                pNvxVideoCapture->pCurrVideoSurface,    // *curr,
                pNvxVideoCapture->pPrevVideoSurface,    // *prev,
                pNvxVideoCapture->pPrev2VideoSurface,   // *prev2,
                NULL,                                   // *vidSrcRect,
                NULL,                                   // *vidDstRect,
                NULL,                                   // *subpic,
                NULL,                                   // *subSrcRect,
                NULL,                                   // *subDstRect,
                NULL,                                   //m_pTVMRFenceMixerReadList,
                pNvxVideoCapture->fenceDone             // *fencesWait,  /* NULL terminated list */
            );
    }

    if (status != TVMR_STATUS_OK)
    {
        return OMX_ErrorUndefined;
    }

    TVMRFenceBlock(pNvxVideoCapture->hDevice, pNvxVideoCapture->fenceDone);

    return OMX_ErrorNone;
}

static
void Enlist(SNvxVideoCaptureData *pNvxVideoCapture,
            TVMRVideoSurface *pVideoSurface)
{
    VideoSurfaceList *tempNode = malloc(sizeof(VideoSurfaceList));
    tempNode->Surf = pVideoSurface;
    tempNode->Next = pNvxVideoCapture->pVideoSurfList;
    pNvxVideoCapture->pVideoSurfList = tempNode;
}

static
void Delist(SNvxVideoCaptureData *pNvxVideoCapture,
            TVMRVideoSurface *pVideoSurface)
{
    OMX_BOOL bSurfaceFound = OMX_TRUE;
    VideoSurfaceList *tempNode = pNvxVideoCapture->pVideoSurfList;
    VideoSurfaceList *lastNode = NULL;

    //Add it to Head
    while (tempNode != NULL)
    {
        if (tempNode->Surf == pVideoSurface)
        {
            if (lastNode) lastNode->Next = tempNode->Next;
            if (tempNode == pNvxVideoCapture->pVideoSurfList) pNvxVideoCapture->pVideoSurfList = NULL;
            free (tempNode);
            bSurfaceFound = OMX_TRUE;
            break;
        }
        lastNode = tempNode;
        tempNode = tempNode->Next;
    }

    if (!bSurfaceFound) printf("Surface Missing \n");
}

static
OMX_ERRORTYPE NvxVideoCaptureWorkerFunction(OMX_IN NvxComponent *pComponent,
                                            OMX_IN OMX_BOOL bAllPortsReady,
                                            OMX_OUT OMX_BOOL *pbMoreWork,
                                            OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVideoCaptureData *pNvxVideoCapture = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    NvxSurface *pDestinationSurface = NULL;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxVideoCaptureWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;
    if (!bAllPortsReady)
        return OMX_ErrorNone;

    pNvxVideoCapture = (SNvxVideoCaptureData *)pComponent->pComponentData;

    if (!pPortOut->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    if (!pNvxVideoCapture->bCaptureStarted)
    {
        //start the capture
        TVMRCaptureStart(pNvxVideoCapture->pCapture);
        pNvxVideoCapture->bCaptureStarted = OMX_TRUE;
    }

    if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_TVMR)
    {
        NvxMutexLock(pNvxVideoCapture->hMutex);
        if (pNvxVideoCapture->uFreeCaptureBuffers > 0)
        {
            pNvxVideoCapture->pCurrVideoSurface  = TVMRCaptureGetFrame(pNvxVideoCapture->pCapture,
                                                                       MAX_CAPTURE_TIMEOUT);
            pNvxVideoCapture->uFreeCaptureBuffers --;
        }
        NvxMutexUnlock(pNvxVideoCapture->hMutex);

        if (pNvxVideoCapture->pCurrVideoSurface)
        {
            Enlist(pNvxVideoCapture, pNvxVideoCapture->pCurrVideoSurface);
        }
        else
            goto leave;

        *((NvU32*)pPortOut->pCurrentBufferHdr->pBuffer) = (unsigned int)pNvxVideoCapture->pCurrVideoSurface;
        pPortOut->pCurrentBufferHdr->nOffset = 0;
        pPortOut->pCurrentBufferHdr->nFilledLen = sizeof(TVMRVideoSurface *);
        pNvxVideoCapture->pCurrVideoSurface = NULL;
    }
    else
    {
        if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
        {
            NvxEglImageSiblingHandle hEglSib = (NvxEglImageSiblingHandle)pPortOut->pCurrentBufferHdr->pBuffer;
            pDestinationSurface = pNvxVideoCapture->pDestinationSurface;
            NvOsMemcpy(&pDestinationSurface->Surfaces[0], &hEglSib->surf, sizeof(NvRmSurface));
            pDestinationSurface->SurfaceCount = 1;
        }
        else if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_NVMM)
        {
            NvMMBuffer *pMMBuffer = NULL;
            NvxSurface *pOutputSurface = NULL;
            NvMMQueueDeQ(pNvxVideoCapture->pOutputSurfaceQueue, &pOutputSurface);
            if (!pOutputSurface)
            {
                //If There is not Video Surface.
                goto leave;
            }
            if(pNvxVideoCapture->bEmbedRmSurface)
            {
                NvOsMemcpy(pPortOut->pCurrentBufferHdr->pBuffer, pOutputSurface->Surfaces,
                        NVMMSURFACEDESCRIPTOR_MAX_SURFACES* sizeof(NvRmSurface));

                pPortOut->pCurrentBufferHdr->nFilledLen = NVMMSURFACEDESCRIPTOR_MAX_SURFACES* sizeof(NvRmSurface);
                pPortOut->pCurrentBufferHdr->pInputPortPrivate = pOutputSurface;
            }
            else
            {
                pMMBuffer = (NvMMBuffer *)pPortOut->pCurrentBufferHdr->pBuffer;
                NvOsMemcpy(&(pMMBuffer->Payload.Surfaces), pOutputSurface, sizeof (NvxSurface));
                pMMBuffer->pCore = pOutputSurface;
                pMMBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
                pPortOut->pCurrentBufferHdr->nFilledLen = sizeof(NvMMBuffer);
            }
            pPortOut->pCurrentBufferHdr->nOffset = 0;
            pDestinationSurface = (NvxSurface*) pOutputSurface;
        }
        else if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_RAW)
        {
            pDestinationSurface = pNvxVideoCapture->pDestinationSurface;
        }

        if (pNvxVideoCapture->eDeinterlaceType == OMX_DeintMethod_NoDeinterlacing)
        {
            pNvxVideoCapture->pPrev2VideoSurface = NULL;
            pNvxVideoCapture->pPrevVideoSurface  = NULL;
            pNvxVideoCapture->pCurrVideoSurface  = TVMRCaptureGetFrame(pNvxVideoCapture->pCapture,
                                                                       MAX_CAPTURE_TIMEOUT);
            if (pNvxVideoCapture->pCurrVideoSurface)
            {
                Enlist(pNvxVideoCapture, pNvxVideoCapture->pCurrVideoSurface);
            }
            pNvxVideoCapture->pNextVideoSurface  = NULL;
            pNvxVideoCapture->pReleaseVideoSurface = pNvxVideoCapture->pCurrVideoSurface;
            pNvxVideoCapture->ePictureStructure = TVMR_PICTURE_STRUCTURE_FRAME;
        }
        else
        {
            if (pNvxVideoCapture->bSecondField)
            {
                pNvxVideoCapture->pPrev2VideoSurface = pNvxVideoCapture->pPrevVideoSurface;
                pNvxVideoCapture->pPrevVideoSurface  = pNvxVideoCapture->pCurrVideoSurface;
                pNvxVideoCapture->pNextVideoSurface = TVMRCaptureGetFrame(pNvxVideoCapture->pCapture,
                                                                          MAX_CAPTURE_TIMEOUT);
                if (pNvxVideoCapture->pNextVideoSurface)
                {
                    Enlist(pNvxVideoCapture, pNvxVideoCapture->pNextVideoSurface);
                }
                pNvxVideoCapture->pReleaseVideoSurface = pNvxVideoCapture->pPrev2VideoSurface;
                pNvxVideoCapture->bSecondField = OMX_FALSE;
                pNvxVideoCapture->ePictureStructure    = TVMR_PICTURE_STRUCTURE_BOTTOM_FIELD;
            }
            else
            {
                if (!pNvxVideoCapture->pCurrVideoSurface)
                {
                    pNvxVideoCapture->pPrev2VideoSurface = NULL;
                    pNvxVideoCapture->pPrevVideoSurface  = NULL;
                    pNvxVideoCapture->pCurrVideoSurface  = TVMRCaptureGetFrame(pNvxVideoCapture->pCapture,
                                                                               MAX_CAPTURE_TIMEOUT);
                    if (pNvxVideoCapture->pCurrVideoSurface)
                    {
                        Enlist(pNvxVideoCapture, pNvxVideoCapture->pCurrVideoSurface);
                    }
                    pNvxVideoCapture->pNextVideoSurface  = pNvxVideoCapture->pCurrVideoSurface;
                }
                else
                {
                    pNvxVideoCapture->pPrev2VideoSurface = pNvxVideoCapture->pPrevVideoSurface;
                    pNvxVideoCapture->pPrevVideoSurface  = pNvxVideoCapture->pCurrVideoSurface;
                    pNvxVideoCapture->pCurrVideoSurface  = pNvxVideoCapture->pNextVideoSurface;
                }

                pNvxVideoCapture->bSecondField = OMX_TRUE;
                pNvxVideoCapture->ePictureStructure    = TVMR_PICTURE_STRUCTURE_TOP_FIELD;
            }
        }

        if (!pNvxVideoCapture->pCurrVideoSurface)
        {
            if (pComponent->pCallbacks && pComponent->pCallbacks->EventHandler)
                pComponent->pCallbacks->EventHandler(pComponent->hBaseComponent,
                                                 pComponent->pCallbackAppData,
                                                 OMX_EventError,
                                                 OMX_ErrorUndefined,
                                                 0,
                                                 0);
            eError = OMX_ErrorNone;
        }

        eError = NvxStretchBlitImage(pNvxVideoCapture, pDestinationSurface);

        if (eError != OMX_ErrorNone)
        {
            if (pComponent->pCallbacks && pComponent->pCallbacks->EventHandler)
                pComponent->pCallbacks->EventHandler(pComponent->hBaseComponent,
                                                 pComponent->pCallbackAppData,
                                                 OMX_EventError,
                                                 OMX_ErrorUndefined,
                                                 0,
                                                 0);

            eError = OMX_ErrorNone;
        }

        if (pNvxVideoCapture->pReleaseVideoSurface)
        {
            TVMRCaptureReturnFrame(pNvxVideoCapture->pCapture,
                                   pNvxVideoCapture->pReleaseVideoSurface,
                                   pNvxVideoCapture->fenceDone);

            Delist(pNvxVideoCapture, pNvxVideoCapture->pReleaseVideoSurface);
            pNvxVideoCapture->pReleaseVideoSurface = NULL;
        }

        if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_RAW) {
            if (!pDestinationSurface->CropRect.left && !pDestinationSurface->CropRect.right &&
                !pDestinationSurface->CropRect.top  && !pDestinationSurface->CropRect.bottom)
            {
                pDestinationSurface->CropRect.left = 0;
                pDestinationSurface->CropRect.top  = 0;
                pDestinationSurface->CropRect.right  = pNvxVideoCapture->uDestinationWidth;
                pDestinationSurface->CropRect.bottom = pNvxVideoCapture->uDestinationHeight;
            }
            if (NvxCopyNvxSurfaceToOMXBuf(pDestinationSurface, pPortOut->pCurrentBufferHdr) !=  NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
        }
    }

    NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);

    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);

    *pbMoreWork = (pPortOut->pCurrentBufferHdr != NULL);
    return eError;
leave:
    *puMaxMsecToNextCall = 10;
    *pbMoreWork = OMX_FALSE;
    return eError;
}

static
void NvxAllocateScratchSurfaceYuv(SNvxVideoCaptureData *pNvxVideoCapture,
                                  NvxSurface **ppSurface)
{
    NvU32 uImageSize;
    //Hardcoded values for NvRmSurfaceLayout_Blocklinear
    NvU16 Kind = NvRmMemKind_Generic_16Bx2;
    NvU16 BlockHeightLog2 = 4;


    /* Create the Scratch Surface */
    *ppSurface = NvOsAlloc(sizeof(NvxSurface));

    if (!(*ppSurface))
    {
        *ppSurface = NULL;
        return;
    }

    NvOsMemset(*ppSurface, 0, sizeof(NvxSurface));

    if (NvxSurfaceAllocYuv(*ppSurface, pNvxVideoCapture->uDestinationWidth, pNvxVideoCapture->uDestinationHeight, NvMMVideoDecColorFormat_YUV420, NvRmSurfaceLayout_Pitch, &uImageSize, NV_FALSE, Kind, BlockHeightLog2, NV_FALSE) != NvSuccess)
    {
        NvOsFree(*ppSurface);
        *ppSurface = NULL;
        return;
    }
}

static
OMX_ERRORTYPE NvxVideoCaptureAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoCaptureData *pNvxVideoCapture = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pOutPort = &pComponent->pPorts[s_nvx_port_output];


    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoCaptureAcquireResources\n"));

    /* Get VideoCapture instance */
    pNvxVideoCapture = (SNvxVideoCaptureData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoCapture);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxVideoCaptureAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }


    pNvxVideoCapture->uDestinationWidth  = pOutPort->oPortDef.format.video.nFrameWidth;
    pNvxVideoCapture->uDestinationHeight = pOutPort->oPortDef.format.video.nFrameHeight;

    /* In Tunneling Mode, Output port type is decided based on Tunnel Transaction */
    /* In Non-Tunneled  Mode, Output Port type is decided based on NvBuffer Index Extension */
    if (pOutPort->bNvidiaTunneling)
    {
        NvxPort *pTunnelOutPort  = NULL;
        NvxComponent *pTunnelComponent = NULL;

        pTunnelComponent = NvxComponentFromHandle(pOutPort->hTunnelComponent);

        if (!pTunnelComponent)
        {
            return OMX_ErrorBadParameter;
        }

        pTunnelOutPort = &pTunnelComponent->pPorts[pOutPort->nTunnelPort];

        /* Check both the tunnel ports operate in same domain */
        if (pTunnelOutPort->oPortDef.eDomain != pOutPort->oPortDef.eDomain)
        {
            return OMX_ErrorBadParameter;
        }

        /* Check if port domain is Video Domain */
        if (pOutPort->oPortDef.eDomain != OMX_PortDomainVideo)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure color formats match */
        if (pOutPort->oPortDef.format.video.eColorFormat !=
                            pTunnelOutPort->oPortDef.format.video.eColorFormat)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure The color formats are supported by this component */
        if ((pOutPort->oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar)  &&
            (pOutPort->oPortDef.format.video.eColorFormat != OMX_COLOR_Format32bitARGB8888) &&
            (pOutPort->oPortDef.format.video.eColorFormat != NVX_ColorFormatYV16x2))
        {
            return OMX_ErrorBadParameter;
        }

        /* Initially Validate Incoming Data Type */
        /*check the tunnel transaction type */
        if (pOutPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES)
        {
            if (pOutPort->oPortDef.format.video.eColorFormat == NVX_ColorFormatYV16x2)
            {
                pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_TVMR;
            }
            else
            {
                pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_NVMM;
            }
        }
        else if (pOutPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_DEFAULT)
        {
            pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;

            if (pOutPort->oPortDef.format.video.eColorFormat == NVX_ColorFormatYV16x2)
            {
                return OMX_ErrorBadParameter;
            }
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        if (pOutPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar)
        {
            pNvxVideoCapture->bOutputYuvOverlay = OMX_TRUE;
            pNvxVideoCapture->eDestinationColorFormat = TVMRSurfaceType_YV12;
        }
        else if (pOutPort->oPortDef.format.video.eColorFormat == OMX_COLOR_Format32bitARGB8888)
        {
            pNvxVideoCapture->eDestinationColorFormat = TVMRSurfaceType_R8G8B8A8;
        }
        else if (pOutPort->oPortDef.format.video.eColorFormat == NVX_ColorFormatYV16x2)
        {
            pNvxVideoCapture->eDestinationColorFormat = TVMRSurfaceType_YV16x2;
        }
    }
    else
    {
        switch (pOutPort->oPortDef.format.video.eColorFormat) {
        case OMX_COLOR_Format32bitARGB8888:
             pNvxVideoCapture->eDestinationColorFormat = TVMRSurfaceType_R8G8B8A8;

             if (pNvxVideoCapture->bUseEglImage[s_nvx_port_output])
             {
                 if (pNvxVideoCapture->bUseNvBuffer)
                 {
                     return OMX_ErrorBadParameter;
                 }
                 pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_EGLIMAGE;
             }
             else
             {
                 if (pNvxVideoCapture->bUseNvBuffer)
                 {
                    pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_NVMM;
                 }
                 else
                 {
                    pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
                 }
             }
             break;
        case OMX_COLOR_FormatYUV420Planar:

             if (pNvxVideoCapture->bUseEglImage[s_nvx_port_output])
             {
                 return OMX_ErrorBadParameter;
             }

             pNvxVideoCapture->eDestinationColorFormat =  TVMRSurfaceType_YV12;

             if (pNvxVideoCapture->bUseNvBuffer)
             {
                 pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_NVMM;
             }
             else
             {
                pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
             }

             pNvxVideoCapture->bOutputYuvOverlay = OMX_TRUE;

             break;
        case NVX_ColorFormatYV16x2:
             pNvxVideoCapture->eDestinationColorFormat = TVMRSurfaceType_YV16x2;
             if (pNvxVideoCapture->bUseNvBuffer)
             {
                 pNvxVideoCapture->eBufferType[s_nvx_port_output] = BUFFER_TYPE_TVMR;
             }
             else
             {
                 return OMX_ErrorBadParameter;
             }
             break;
        default:
             return OMX_ErrorBadParameter;
        }
    }

    if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
    {
        pNvxVideoCapture->pDestinationSurface = NvOsAlloc(sizeof(NvxSurface));
        NvOsMemset(pNvxVideoCapture->pDestinationSurface, 0, sizeof(NvxSurface));
    }
    else if (pNvxVideoCapture->eBufferType[s_nvx_port_output] != BUFFER_TYPE_TVMR)
    {
       if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_RAW)
       {
           /* Allocate the scratch surface */
           if (pOutPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar)
           {
                NvxAllocateScratchSurfaceYuv(pNvxVideoCapture, &pNvxVideoCapture->pDestinationSurface);
           }
           else
           {
               /* eFormat should take nvcolorformat type */
                if (NvxSurfaceAlloc(&pNvxVideoCapture->pDestinationSurface,
                                    pNvxVideoCapture->uDestinationWidth,
                                    pNvxVideoCapture->uDestinationHeight,
                                    NvColorFormat_A8B8G8R8,
                                    NvRmSurfaceLayout_Pitch) != NvSuccess)
                {
                    return OMX_ErrorInsufficientResources;
                }
           }
       }
       else
       {
            NvMMQueueCreate(&pNvxVideoCapture->pOutputSurfaceQueue,
                            MAX_OUTPUT_SURFACES,
                            sizeof(NvU32*),
                            NV_FALSE);

            if (!pNvxVideoCapture->pOutputSurfaceQueue)
            {
                return OMX_ErrorInsufficientResources;
            }

            pNvxVideoCapture->ppSurfaceBuffers = (NvxSurface**) NvOsAlloc(MAX_OUTPUT_SURFACES * sizeof(NvxSurface*));

            if (!pNvxVideoCapture->ppSurfaceBuffers)
            {
                return OMX_ErrorInsufficientResources;
            }

            if (pNvxVideoCapture->bOutputYuvOverlay)
            {
                int i;
                /* Allocate Output Surfaces */
                for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
                {
                    NvxAllocateScratchSurfaceYuv(pNvxVideoCapture, &pNvxVideoCapture->ppSurfaceBuffers[i]);

                    if (!pNvxVideoCapture->ppSurfaceBuffers[i])
                    {
                        return OMX_ErrorInsufficientResources;
                    }

                    NvMMQueueEnQ(pNvxVideoCapture->pOutputSurfaceQueue, &pNvxVideoCapture->ppSurfaceBuffers[i], 2000);
                }
            }
            else
            {
                int i = 0;
                /* Allocate Output Surfaces */
                for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
                {
                   /* eFormat should take nvcolorformat type */
                    if (NvxSurfaceAlloc(&pNvxVideoCapture->ppSurfaceBuffers[i],
                                        pNvxVideoCapture->uDestinationWidth,
                                        pNvxVideoCapture->uDestinationHeight,
                                        NvColorFormat_A8B8G8R8,
                                        NvRmSurfaceLayout_Pitch) != NvSuccess)
                    {
                        return OMX_ErrorInsufficientResources;
                    }

                    if (!pNvxVideoCapture->ppSurfaceBuffers[i])
                    {
                        return OMX_ErrorInsufficientResources;
                    }
                    NvMMQueueEnQ(pNvxVideoCapture->pOutputSurfaceQueue, &pNvxVideoCapture->ppSurfaceBuffers[i], 2000);
                }
            }
       }
    }

    pNvxVideoCapture->hDevice = TVMRDeviceCreate(NULL);

    if(!pNvxVideoCapture->hDevice) {
        return OMX_ErrorInsufficientResources;
    }

    pNvxVideoCapture->captureSurfaceType = TVMRSurfaceType_YV16x2;
    pNvxVideoCapture->pCapture =  TVMRCaptureCreate(pNvxVideoCapture->eCaptureFormat,
                                                    pNvxVideoCapture->captureSurfaceType,
                                                    NUM_CAPTURE_FIELDS, NV_TRUE);

    if (!pNvxVideoCapture->pCapture)
    {
        return OMX_ErrorInsufficientResources;
    }

    pNvxVideoCapture->uFreeCaptureBuffers = (NUM_CAPTURE_FIELDS >> 1);

    if (pNvxVideoCapture->eBufferType[s_nvx_port_output] != BUFFER_TYPE_TVMR)
    {
        TVMRVideoMixerAttributes oVmrAttrib;

        pNvxVideoCapture->hMixer = TVMRVideoMixerCreate(pNvxVideoCapture->hDevice,
                                                        pNvxVideoCapture->captureSurfaceType,
                                                        pNvxVideoCapture->pCapture->width,
                                                        pNvxVideoCapture->pCapture->height,
                                                        TVMR_VIDEO_MIXER_FEATURE_ADVANCE1_DEINTERLACING);

        if(!pNvxVideoCapture->hMixer) {
            return OMX_ErrorInsufficientResources;
        }

        if (pNvxVideoCapture->eDeinterlaceType != OMX_DeintMethod_NoDeinterlacing)
        {
            if (pNvxVideoCapture->eDeinterlaceType == OMX_DeintMethod_BobAtFieldRate)
            {
                oVmrAttrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_BOB;
            }
            else if (pNvxVideoCapture->eDeinterlaceType == OMX_DeintMethod_Advanced1AtFieldRate)
            {
                oVmrAttrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_ADVANCE1;
            }
            TVMRVideoMixerSetAttributes(pNvxVideoCapture->hMixer,
                                        TVMR_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE,
                                        &oVmrAttrib);
        }
    }

    pNvxVideoCapture->fenceDone = TVMRFenceCreate();

    pNvxVideoCapture->pCurrVideoSurface  = NULL;
    pNvxVideoCapture->pPrev2VideoSurface = NULL;
    pNvxVideoCapture->pPrevVideoSurface  = NULL;
    pNvxVideoCapture->pNextVideoSurface  = NULL;
    pNvxVideoCapture->pReleaseVideoSurface  = NULL;
    pNvxVideoCapture->ePictureStructure = TVMR_PICTURE_STRUCTURE_FRAME;
    pNvxVideoCapture->bSecondField      = OMX_FALSE;
    pNvxVideoCapture->bInitialized = OMX_TRUE;
    pNvxVideoCapture->bCaptureStarted = OMX_FALSE;

    NvxMutexCreate(&pNvxVideoCapture->hMutex);
    return eError;
}

static
OMX_ERRORTYPE NvxVideoCaptureReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoCaptureData *pNvxVideoCapture = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoCaptureReleaseResources\n"));

    /* get videocapture instance */
    pNvxVideoCapture = (SNvxVideoCaptureData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoCapture);

    if (!pNvxVideoCapture->bInitialized)
        return OMX_ErrorNone;

    if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
    {
        if (pNvxVideoCapture->pDestinationSurface)
        {
            NvOsFree(pNvxVideoCapture->pDestinationSurface);
        }
    }
    else
    {
        if (pNvxVideoCapture->pDestinationSurface)
        {
            NvxSurfaceFree(&pNvxVideoCapture->pDestinationSurface);
        }
    }

    pNvxVideoCapture->pDestinationSurface = NULL;

    pNvxVideoCapture->bInitialized = OMX_FALSE;

    if (pNvxVideoCapture->bCaptureStarted)
    {
        TVMRCaptureStop(pNvxVideoCapture->pCapture);
    }

    if (pNvxVideoCapture->eDeinterlaceType != OMX_DeintMethod_NoDeinterlacing &&
                                              pNvxVideoCapture->bUseEglImage[s_nvx_port_output])
    {
        TVMRCaptureReturnFrame(pNvxVideoCapture->pCapture,
                               pNvxVideoCapture->pCurrVideoSurface,
                               pNvxVideoCapture->fenceDone);

        if (pNvxVideoCapture->bSecondField)
        {
            if (pNvxVideoCapture->pPrevVideoSurface && pNvxVideoCapture->pPrevVideoSurface !=
                                                          pNvxVideoCapture->pCurrVideoSurface)
            {
                TVMRCaptureReturnFrame(pNvxVideoCapture->pCapture,
                                       pNvxVideoCapture->pPrevVideoSurface,
                                       pNvxVideoCapture->fenceDone);
            }
        }
        else
        {
            if (pNvxVideoCapture->pNextVideoSurface &&  pNvxVideoCapture->pCurrVideoSurface != pNvxVideoCapture->pNextVideoSurface)
            {
                TVMRCaptureReturnFrame(pNvxVideoCapture->pCapture,
                                       pNvxVideoCapture->pNextVideoSurface,
                                       pNvxVideoCapture->fenceDone);
            }
        }
    }

    TVMRFenceDestroy(pNvxVideoCapture->fenceDone);

    if (pNvxVideoCapture->pCapture)
    {
        TVMRCaptureDestroy(pNvxVideoCapture->pCapture);
        pNvxVideoCapture->pCapture = NULL;
    }

    if (pNvxVideoCapture->hMixer)
    {
        TVMRVideoMixerDestroy(pNvxVideoCapture->hMixer);
        pNvxVideoCapture->hMixer = NULL;
    }

    if (pNvxVideoCapture->hDevice)
    {
        TVMRDeviceDestroy(pNvxVideoCapture->hDevice);
        pNvxVideoCapture->hDevice = NULL;
    }

    if (pNvxVideoCapture->pOutputSurfaceQueue)
    {
        int i = 0;
        for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
        {
            NvxSurfaceFree(&pNvxVideoCapture->ppSurfaceBuffers[i]);
            pNvxVideoCapture->ppSurfaceBuffers[i] = NULL;
        }

        NvOsFree(pNvxVideoCapture->ppSurfaceBuffers);

        NvMMQueueDestroy(&pNvxVideoCapture->pOutputSurfaceQueue);
        pNvxVideoCapture->pOutputSurfaceQueue = NULL;
    }

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));

    return eError;
}

static
OMX_ERRORTYPE NvxVideoCaptureReturnBuffers(NvxComponent *pNvComp,
                                           OMX_U32 nPortIndex,
                                           OMX_BOOL bWorkerLocked)
{
    SNvxVideoCaptureData *pNvxVideoCapture;
    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoCaptureReturnBuffers\n"));

    pNvxVideoCapture = (SNvxVideoCaptureData *)pNvComp->pComponentData;

    while (pNvxVideoCapture->pVideoSurfList)
    {
         VideoSurfaceList *tempNode = pNvxVideoCapture->pVideoSurfList;
         TVMRCaptureReturnFrame(pNvxVideoCapture->pCapture, pNvxVideoCapture->pVideoSurfList->Surf, NULL);
         pNvxVideoCapture->pVideoSurfList = pNvxVideoCapture->pVideoSurfList->Next;
         free(tempNode);
    }

    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoCaptureFillThisBuffer(NvxComponent *pNvComp,
                                            OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxVideoCaptureData *pNvxVideoCapture;
    TVMRVideoSurface *pVideoSurface = NULL;

    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoCaptureFillThisBuffer\n"));

    pNvxVideoCapture = (SNvxVideoCaptureData *)pNvComp->pComponentData;

    if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_TVMR)
    {
        pVideoSurface = (TVMRVideoSurface*) *((NvU32*)pBufferHdr->pBuffer);
        if (pVideoSurface)
        {
            NvxMutexLock(pNvxVideoCapture->hMutex);
            TVMRCaptureReturnFrame(pNvxVideoCapture->pCapture, pVideoSurface, NULL);
            Delist(pNvxVideoCapture, pVideoSurface);
            pNvxVideoCapture->uFreeCaptureBuffers ++;
            NvxMutexUnlock(pNvxVideoCapture->hMutex);
        }
    }
    else if (pNvxVideoCapture->eBufferType[s_nvx_port_output] == BUFFER_TYPE_NVMM)
    {
        OMX_BOOL bSurfaceFound = OMX_FALSE;
        NvxSurface *pOutputSurface = NULL;

        if(pNvxVideoCapture->bEmbedRmSurface)
        {
            pOutputSurface = (NvxSurface*) pBufferHdr->pInputPortPrivate;
        }
        else
        {
            NvMMBuffer *pMMBuffer  = (NvMMBuffer*) pBufferHdr->pBuffer;
            pOutputSurface = pMMBuffer->pCore;
        }

        if (pOutputSurface)
        {
            int i = 0;
            //match with any of available output surfaces.
            for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
            {
                if (pOutputSurface == pNvxVideoCapture->ppSurfaceBuffers[i])
                {
                    bSurfaceFound = OMX_TRUE;
                    break;
                }
            }

            if (bSurfaceFound)
            {
                NvMMQueueEnQ(pNvxVideoCapture->pOutputSurfaceQueue, &pNvxVideoCapture->ppSurfaceBuffers[i], 2000);
            }
            else
            {
                return OMX_ErrorResourcesLost;
            }
        }
    }

    NvxWorkerTrigger(&(pNvComp->oWorkerData));

    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoCaptureCanUseEglImage(OMX_IN NvxComponent *pNvComp,
                                            OMX_IN OMX_U32 nPortIndex,
                                            OMX_IN void* eglImage)
{
    SNvxVideoCaptureData *pNvxVideoCapture = 0;
    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoCaptureFillThisBuffer\n"));
    pNvxVideoCapture = (SNvxVideoCaptureData *)pNvComp->pComponentData;
    pNvxVideoCapture->bUseEglImage[nPortIndex] = OMX_TRUE;
    return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE NvxVideoCaptureInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxVideoCaptureData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_VIDEO_CAPTURE,"ERROR:NvxVideoCaptureInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_VIDEO_CAPTURE;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVideoCaptureData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxVideoCaptureData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxVideoCaptureData));

    pNvComp->DeInit             = NvxVideoCaptureDeInit;
    pNvComp->GetParameter       = NvxVideoCaptureGetParameter;
    pNvComp->SetParameter       = NvxVideoCaptureSetParameter;
    pNvComp->ReturnBuffers      = NvxVideoCaptureReturnBuffers;
    pNvComp->FillThisBufferCB   = NvxVideoCaptureFillThisBuffer;
    pNvComp->GetConfig          = NvxVideoCaptureGetConfig;
    pNvComp->SetConfig          = NvxVideoCaptureSetConfig;
    pNvComp->WorkerFunction     = NvxVideoCaptureWorkerFunction;
    pNvComp->AcquireResources   = NvxVideoCaptureAcquireResources;
    pNvComp->ReleaseResources   = NvxVideoCaptureReleaseResources;
    pNvComp->CanUseEGLImage     = NvxVideoCaptureCanUseEglImage;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MIN_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingAutoDetect);
 //   pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = NVX_ColorFormatYV16x2;
    pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    // TO DO: Fix with Other Tunnel Modes
    //pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_DEFAULT;
    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    pNvComp->pComponentName = "OMX.Nvidia.video.capture";
    pNvComp->sComponentRoles[0] = "video_capture.generic";
    pNvComp->nComponentRoles    = 1;

    data->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
    data->eDeinterlaceType = OMX_DeintMethod_NoDeinterlacing;

    return eError;
}

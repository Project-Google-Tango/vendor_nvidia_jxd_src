/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** nvxtvmrrender.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "nvassert.h"
#include "NvxHelpers.h"
#include "nvfxmath.h"
#include "tvmr.h"
#include "nvmm.h"
#include "nvmm_videodec.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */
#define MIN_INPUT_BUFFERS    4
#define MIN_INPUT_BUFFERSIZE  512000
#define INVALID_TIMESTAMP 0xFFFFFFFF

#define MAX_TVMR_OUTPUTS 7

/* Number of internal frame buffers (video memory) */
#define MAX_PORTS 1
static const int s_nvx_num_ports            = 1;
static const int s_nvx_port_input           = 0;

/* decoder state information */
typedef struct SNvxVideoTvmrRenderer
{
    OMX_BOOL bInitialized;

    TVMROutput      *pOutput;
    TVMROutputInfo  tvmrOutputInfo[MAX_TVMR_OUTPUTS];
    TVMRFlipQueue   *pFlipQueue;
    OMX_BOOL        bYuvOverlay;
    OMX_BOOL        bUseNvBuffer;
    OMX_BOOL        bUseNvMMBuffer;

    OMX_U32         uDisplayId;
    TVMRDevice      *pDevice;
    TVMRFence       fenceDone;

    TVMRRect        srcRect;
    TVMRRect        dstRect;
    TVMROutputType  outputType;

    OMX_U32         uDisplayWidth;
    OMX_U32         uDisplayHeight;

    OMX_BUFFERHEADERTYPE *pPrevBufferHeader;
    NvxSurface      *pDestinationSurface;
    OMX_U32          uDestinationWidth;
    OMX_U32          uDestinationHeight;

    OMX_BOOL         bKeepAspect;
    OMX_U32          uAspectRatioNumerator;
    OMX_U32          uAspectRatioDenominator;

} SNvxVideoTvmrRendererData;

static
void NvxAllocateScratchSurfaceYuv(SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer,
                                  NvxSurface **ppSurface );

static
OMX_ERRORTYPE NvxVideoTvmrRendererDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoTvmrRendererInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererGetParameter(OMX_IN NvxComponent *pComponent,
                                               OMX_IN OMX_INDEXTYPE nParamIndex,
                                               OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer;

    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoTvmrRenderer);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoTvmrRendererGetParameter\n"));

    switch (nParamIndex)
    {
    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererSetParameter(OMX_IN NvxComponent *pComponent,
                                               OMX_IN OMX_INDEXTYPE nIndex,
                                               OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer;
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoTvmrRendererSetParameter\n"));

    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;
    switch(nIndex)
    {
        case NVX_IndexConfigUseNvBuffer:
        {
            NVX_PARAM_USENVBUFFER* pParam = (NVX_PARAM_USENVBUFFER*)pComponentParameterStructure;
            if (pParam->nPortIndex != s_nvx_port_input)
            {
                return OMX_ErrorBadParameter;
            }
            pNvxVideoTvmrRenderer->bUseNvBuffer = pParam->bUseNvBuffer;
        }
        break;
    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererGetConfig(OMX_IN NvxComponent* pNvComp,
                                            OMX_IN OMX_INDEXTYPE nIndex,
                                            OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoTvmrRendererGetConfig\n"));

    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererSetConfig(OMX_IN NvxComponent* pNvComp,
                                            OMX_IN OMX_INDEXTYPE nIndex,
                                            OMX_IN OMX_PTR pComponentConfigStructure)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoTvmrRendererSetConfig\n"));

    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case OMX_IndexConfigCommonBrightness:
        {
            TVMRFlipQueueAttributes oFlipQueueAttrib;
            OMX_CONFIG_BRIGHTNESSTYPE *pBrightness = (OMX_CONFIG_BRIGHTNESSTYPE*) pComponentConfigStructure;
            oFlipQueueAttrib.brightness = ((float) ((float)(pBrightness->nBrightness) / 100)) - 0.5;
            TVMRFlipQueueSetAttributes(pNvxVideoTvmrRenderer->pFlipQueue,
                                       TVMR_FLIP_QUEUE_ATTRIBUTE_BRIGHTNESS,
                                       &oFlipQueueAttrib);
        }
        break;
        case OMX_IndexConfigCommonContrast:
        {
            TVMRFlipQueueAttributes oFlipQueueAttrib;
            OMX_CONFIG_CONTRASTTYPE *pContrast = (OMX_CONFIG_CONTRASTTYPE*) pComponentConfigStructure;
            oFlipQueueAttrib.contrast = (float) (((float)(pContrast->nContrast + 100) * 1.9 ) / 200) + 0.1;
            TVMRFlipQueueSetAttributes(pNvxVideoTvmrRenderer->pFlipQueue,
                                       TVMR_FLIP_QUEUE_ATTRIBUTE_CONTRAST,
                                       &oFlipQueueAttrib);
        }
        break;
        case OMX_IndexConfigCommonSaturation:
        {
            TVMRFlipQueueAttributes oFlipQueueAttrib;
            OMX_CONFIG_SATURATIONTYPE *pSaturation = (OMX_CONFIG_SATURATIONTYPE*) pComponentConfigStructure;
            oFlipQueueAttrib.saturation = (float) ((float)((pSaturation->nSaturation + 100) * 2) / 200);
            TVMRFlipQueueSetAttributes(pNvxVideoTvmrRenderer->pFlipQueue,
                                       TVMR_FLIP_QUEUE_ATTRIBUTE_SATURATION,
                                       &oFlipQueueAttrib);
        }
        break;
        case OMX_IndexConfigCommonInputCrop:
        {
            OMX_CONFIG_RECTTYPE *pInputRect = (OMX_CONFIG_RECTTYPE*) pComponentConfigStructure;

            pNvxVideoTvmrRenderer->srcRect.x0 = pInputRect->nLeft;
            pNvxVideoTvmrRenderer->srcRect.y0 = pInputRect->nTop;
            pNvxVideoTvmrRenderer->srcRect.x1 = pInputRect->nWidth + pInputRect->nLeft;
            pNvxVideoTvmrRenderer->srcRect.y1 = pInputRect->nHeight + pInputRect->nTop;
        }
        break;
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pOutputRect = (OMX_CONFIG_RECTTYPE*) pComponentConfigStructure;

            pNvxVideoTvmrRenderer->dstRect.x0 = pOutputRect->nLeft;
            pNvxVideoTvmrRenderer->dstRect.y0 = pOutputRect->nTop;
            pNvxVideoTvmrRenderer->dstRect.x1 = pOutputRect->nWidth + pOutputRect->nLeft;
            pNvxVideoTvmrRenderer->dstRect.y1 = pOutputRect->nHeight + pOutputRect->nTop;
        }
        break;
        case NVX_IndexConfigForceAspect:
        {
            NVX_CONFIG_FORCEASPECT *pAspectRatioParams = (NVX_CONFIG_FORCEASPECT*) pComponentConfigStructure;
            pNvxVideoTvmrRenderer->uAspectRatioNumerator   = pAspectRatioParams->nNumerator;
            pNvxVideoTvmrRenderer->uAspectRatioDenominator = pAspectRatioParams->nDenominator;
        }
        break;
        case NVX_IndexConfigKeepAspect:
        {
            NVX_CONFIG_KEEPASPECT *pAspectRatioParams = (NVX_CONFIG_KEEPASPECT*) pComponentConfigStructure;
            pNvxVideoTvmrRenderer->bKeepAspect = pAspectRatioParams->bKeepAspect;
        }
        break;
        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererWorkerFunction(OMX_IN NvxComponent *pComponent,
                                                 OMX_IN OMX_BOOL bAllPortsReady,
                                                 OMX_OUT OMX_BOOL *pbMoreWork,
                                                 OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];


    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxVideoTvmrRendererWorkerFunction\n"));


    *pbMoreWork = bAllPortsReady;
    if (!bAllPortsReady)
        return OMX_ErrorNone;

    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;

    if (!pPortIn->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
    {
        NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
        pPortIn->pCurrentBufferHdr = NULL;

        goto leave;
    }

    if (pPortIn->pCurrentBufferHdr->nFilledLen > 0)
    {
        NvxSurface *pSourceSurface = NULL;
        NvMMBuffer *pInBuf = (NvMMBuffer*) (pPortIn->pCurrentBufferHdr->pBuffer);
        TVMRRect    destRect;
        OMX_U32 uDisplayWidth = 0, uDisplayHeight = 0;
        float fDecodeAspectRatio, fDisplayAspectRatio;

        if (pNvxVideoTvmrRenderer->bUseNvMMBuffer)
        {
            pSourceSurface = (NvxSurface *)&pInBuf->Payload.Surfaces;
        }
        else
        {
            pSourceSurface = (NvxSurface*) pNvxVideoTvmrRenderer->pDestinationSurface;
            if (NvxCopyOMXSurfaceToNvxSurface(pPortIn->pCurrentBufferHdr, pSourceSurface, NV_FALSE) != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
        }

        if ((!pNvxVideoTvmrRenderer->srcRect.x0) && (!pNvxVideoTvmrRenderer->srcRect.x1) &&
            (!pNvxVideoTvmrRenderer->srcRect.y0) && (!pNvxVideoTvmrRenderer->srcRect.y1))
        {
            pNvxVideoTvmrRenderer->srcRect.x0 = 0;
            pNvxVideoTvmrRenderer->srcRect.y0 = 0;
            pNvxVideoTvmrRenderer->srcRect.x1 = pSourceSurface->Surfaces[0].Width;
            pNvxVideoTvmrRenderer->srcRect.y1 = pSourceSurface->Surfaces[0].Height;
        }

        destRect = pNvxVideoTvmrRenderer->dstRect;

        if ((!destRect.x0) && (!destRect.x1) &&
            (!destRect.y0) && (!destRect.y1))
        {
            uDisplayWidth  = pNvxVideoTvmrRenderer->uDisplayWidth;
            uDisplayHeight = pNvxVideoTvmrRenderer->uDisplayHeight;
            /* If aspect ratio is ignored */
            if (!pNvxVideoTvmrRenderer->bKeepAspect)
            {
                destRect.x0 = 0;
                destRect.x1 = uDisplayWidth;
                destRect.y0 = 0;
                destRect.y1 = uDisplayHeight;
            }
        }
        else
        {
            uDisplayWidth  = (destRect.x1 - destRect.x0);
            uDisplayHeight = (destRect.y1 - destRect.y0);
        }

        if (pNvxVideoTvmrRenderer->bKeepAspect)
        {
            fDisplayAspectRatio = (float) uDisplayWidth / uDisplayHeight;
            fDecodeAspectRatio = (float) ((float)pNvxVideoTvmrRenderer->uAspectRatioNumerator / pNvxVideoTvmrRenderer->uAspectRatioDenominator *
                                          (float) pSourceSurface->Surfaces[0].Width / pSourceSurface->Surfaces[0].Height);

            if(fDecodeAspectRatio < fDisplayAspectRatio ) {
                float fFrameWidth = fDecodeAspectRatio * (float) uDisplayHeight;
                destRect.x0 = ((float)uDisplayWidth - fFrameWidth) / 2.0f + destRect.x0;
                destRect.x1 = destRect.x0 + fFrameWidth;
                destRect.y1 = uDisplayHeight + destRect.y0;
            } else {
                float fFrameHeight = (float)uDisplayWidth / fDecodeAspectRatio;
                destRect.x1 = uDisplayWidth + destRect.x0;
                destRect.y0 = ((float)uDisplayHeight - fFrameHeight) / 2.0f + destRect.y0;
                destRect.y1 = destRect.y0 + fFrameHeight;
            }
        }

        if (pNvxVideoTvmrRenderer->bYuvOverlay)
        {
            TVMRVideoSurface oVideoSurface;
            TVMRSurface tvmrSurface[3];

            oVideoSurface.type   = TVMRSurfaceType_YV12;
            oVideoSurface.width  = pSourceSurface->Surfaces[0].Width;
            oVideoSurface.height = pSourceSurface->Surfaces[0].Height;
            oVideoSurface.surfaces[0] = &tvmrSurface[0];
            oVideoSurface.surfaces[1] = &tvmrSurface[1];
            oVideoSurface.surfaces[2] = &tvmrSurface[2];
            oVideoSurface.surfaces[0]->pitch = pSourceSurface->Surfaces[0].Pitch;
            oVideoSurface.surfaces[1]->pitch = pSourceSurface->Surfaces[1].Pitch;
            oVideoSurface.surfaces[2]->pitch = pSourceSurface->Surfaces[2].Pitch;

            /* Assumes that Whoever sends the surfaces to this component, would send only in this order */
            oVideoSurface.surfaces[0]->priv = (void*)&pSourceSurface->Surfaces[0];
            oVideoSurface.surfaces[1]->priv = (void*)&pSourceSurface->Surfaces[2];
            oVideoSurface.surfaces[2]->priv = (void*)&pSourceSurface->Surfaces[1];

            TVMRFlipQueueDisplayYUV(pNvxVideoTvmrRenderer->pFlipQueue,
                                    TVMR_PICTURE_STRUCTURE_FRAME,
                                    &oVideoSurface,
                                    &pNvxVideoTvmrRenderer->srcRect,
                                    &destRect,
                                    NULL,
                                    pNvxVideoTvmrRenderer->fenceDone,
                                    NULL
                                   );

            TVMRFenceBlock(pNvxVideoTvmrRenderer->pDevice, pNvxVideoTvmrRenderer->fenceDone);

        }
        else
        {
            TVMROutputSurface oOutputSurface;
            TVMRSurface oTvmrOutputSurface;

            oOutputSurface.type = TVMRSurfaceType_R8G8B8A8;
            oOutputSurface.width  = pSourceSurface->Surfaces[0].Width;
            oOutputSurface.height = pSourceSurface->Surfaces[0].Height;
            oOutputSurface.surface = &oTvmrOutputSurface;
            oOutputSurface.surface->pitch = pSourceSurface->Surfaces[0].Pitch;
            oOutputSurface.surface->priv = (void*)&pSourceSurface->Surfaces[0];

           TVMRFlipQueueDisplayRGB(pNvxVideoTvmrRenderer->pFlipQueue,
                                   &oOutputSurface,
                                   NULL,
                                   0,
                                   0,
                                   NULL,
                                   NULL,
                                   TVMR_FLIP_QUEUE_COMPOSITE_TYPE_OPAQUE,
                                   NULL
                                  );
        }

        if (pNvxVideoTvmrRenderer->pPrevBufferHeader)
        {
            NvxPortReleaseBuffer(pPortIn, pNvxVideoTvmrRenderer->pPrevBufferHeader);
        }

        pNvxVideoTvmrRenderer->pPrevBufferHeader = pPortIn->pCurrentBufferHdr;

        pPortIn->pCurrentBufferHdr = NULL;
    }

leave:
    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);

    *pbMoreWork = (pPortIn->pCurrentBufferHdr != NULL);

    return eError;
}

static
void NvxAllocateScratchSurfaceYuv(SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer,
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

    if (NvxSurfaceAllocYuv(*ppSurface,
                           pNvxVideoTvmrRenderer->uDestinationWidth,
                           pNvxVideoTvmrRenderer->uDestinationHeight,
                           NvMMVideoDecColorFormat_YUV420,
                           NvRmSurfaceLayout_Pitch,
                           &uImageSize, NV_FALSE, Kind, BlockHeightLog2,
                           NV_FALSE) != NvSuccess)
    {
        NvOsFree(*ppSurface);
        *ppSurface = NULL;
        return;
    }
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    TVMROutputInfo *tvmrOutputInfo;
    int numOutputs = 0, i = 0;
    TVMRSurfaceType surfaceType;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoTvmrRendererAcquireResources\n"));

    /* Get TvmrRenderer instance */
    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoTvmrRenderer);

    /* In Tunneling Mode, Output port type is decided based on Tunnel Transaction */
    /* In Non-Tunneled  Mode, Output Port type is decided based on NvBuffer Index Extension */
    if (pPortIn->bNvidiaTunneling)
    {
        NvxPort *pTunnelInPort  = NULL;
        NvxComponent *pTunnelComponent = NULL;

        pTunnelComponent = NvxComponentFromHandle(pPortIn->hTunnelComponent);

        pTunnelInPort = &pTunnelComponent->pPorts[pPortIn->nTunnelPort];

        /* Check both the tunnel ports operate in same domain */
        if (pTunnelInPort->oPortDef.eDomain != pPortIn->oPortDef.eDomain)
        {
            return OMX_ErrorBadParameter;
        }

        /* Check if port domain is Video Domain */
        if (pPortIn->oPortDef.eDomain != OMX_PortDomainVideo)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure color formats match */
        if (pPortIn->oPortDef.format.video.eColorFormat != pTunnelInPort->oPortDef.format.video.eColorFormat)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure The color formats are supported by this component */
        if ((pPortIn->oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar) &&
            (pPortIn->oPortDef.format.video.eColorFormat != OMX_COLOR_Format32bitARGB8888))
        {
            return OMX_ErrorBadParameter;
        }

        /*check the tunnel transaction type */
        if (pPortIn->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES)
        {
            pNvxVideoTvmrRenderer->bUseNvMMBuffer = OMX_TRUE;
        }
        else if (pPortIn->eNvidiaTunnelTransaction == ENVX_TRANSACTION_DEFAULT)
        {
            pNvxVideoTvmrRenderer->bUseNvMMBuffer = OMX_FALSE;
        }
        else
        {
            return OMX_ErrorBadParameter;
        }
    }
    else
    {
       pNvxVideoTvmrRenderer->bUseNvMMBuffer = pNvxVideoTvmrRenderer->bUseNvBuffer;
    }

    if (pPortIn->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar)
    {
        pNvxVideoTvmrRenderer->bYuvOverlay = OMX_TRUE;
    }
    else
    {
        pNvxVideoTvmrRenderer->bYuvOverlay = OMX_FALSE;
    }

    pNvxVideoTvmrRenderer->uDestinationWidth  = pPortIn->oPortDef.format.video.nFrameWidth;
    pNvxVideoTvmrRenderer->uDestinationHeight = pPortIn->oPortDef.format.video.nFrameHeight;

    if (!pNvxVideoTvmrRenderer->bUseNvMMBuffer)
    {
        /* Allocate the scratch surface */
        if (pNvxVideoTvmrRenderer->bYuvOverlay)
        {
            NvxAllocateScratchSurfaceYuv(pNvxVideoTvmrRenderer, &pNvxVideoTvmrRenderer->pDestinationSurface);

            if (!pNvxVideoTvmrRenderer->pDestinationSurface)
            {
                return OMX_ErrorInsufficientResources;
            }
        }
        else
        {
            /* eFormat should take nvcolorformat type */
            if (NvxSurfaceAlloc(&pNvxVideoTvmrRenderer->pDestinationSurface,
                                pNvxVideoTvmrRenderer->uDestinationWidth,
                                pNvxVideoTvmrRenderer->uDestinationHeight,
                                NvColorFormat_A8B8G8R8,
                                NvRmSurfaceLayout_Pitch) != NvSuccess)
            {
                return OMX_ErrorInsufficientResources;
            }
        }
    }


    tvmrOutputInfo = TVMROutputQuery(&numOutputs);

    if (numOutputs)
    {
        OMX_BOOL bRequestedOutputFound = OMX_FALSE;
        NvOsMemcpy(pNvxVideoTvmrRenderer->tvmrOutputInfo, tvmrOutputInfo, sizeof (TVMROutputInfo) * numOutputs);

        if (pNvxVideoTvmrRenderer->outputType == TMVR_OUTPUT_TYPE_UNKNOWN)
        {
            //switch to 0th display as default display
            pNvxVideoTvmrRenderer->outputType = tvmrOutputInfo[0].type;
        }

        /* get the outputinfo for the requested display id */
        for (i = 0; i < numOutputs; i++)
        {
            if (pNvxVideoTvmrRenderer->outputType == tvmrOutputInfo[i].type)
            {
                bRequestedOutputFound = OMX_TRUE;
                pNvxVideoTvmrRenderer->uDisplayId = i;
                break;
            }
        }

        if (bRequestedOutputFound)
        {
            pNvxVideoTvmrRenderer->pOutput = TVMROutputCreate(pNvxVideoTvmrRenderer->uDisplayId, NULL); // &tvmrOutputInfo[i].mode);

            if (!pNvxVideoTvmrRenderer->pOutput)
            {
                return OMX_ErrorInsufficientResources;
            }
        }
        else
        {
            return OMX_ErrorInsufficientResources;
        }
    }

   pNvxVideoTvmrRenderer->uDisplayWidth  =  pNvxVideoTvmrRenderer->pOutput->outputInfo.mode.width;
   pNvxVideoTvmrRenderer->uDisplayHeight =  pNvxVideoTvmrRenderer->pOutput->outputInfo.mode.height;

    if (pNvxVideoTvmrRenderer->bYuvOverlay)
    {
        surfaceType = TVMRSurfaceType_YV12;
    }
    else
    {
        surfaceType = TVMRSurfaceType_R8G8B8A8;
    }

    pNvxVideoTvmrRenderer->pFlipQueue = TVMRFlipQueueCreate(pNvxVideoTvmrRenderer->uDisplayId, surfaceType);

    if (!pNvxVideoTvmrRenderer->pFlipQueue)
    {
        return OMX_ErrorInsufficientResources;
    }

    pNvxVideoTvmrRenderer->pDevice = TVMRDeviceCreate(NULL);

    if (!pNvxVideoTvmrRenderer->pDevice)
    {
        return OMX_ErrorInsufficientResources;
    }

    pNvxVideoTvmrRenderer->fenceDone = TVMRFenceCreate();

    pNvxVideoTvmrRenderer->bInitialized = OMX_TRUE;

    return eError;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererReturnBuffers(OMX_IN NvxComponent *pComponent,
                                                OMX_U32 nPortIndex,
                                                OMX_BOOL bWorkerLocked)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer = 0;
    NvxPort *pPortIn  = &pComponent->pPorts[s_nvx_port_input];

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoMixerReturnBuffers\n"));

    /* get videotvmrrenderer instance */
    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoTvmrRenderer);

    if (pNvxVideoTvmrRenderer->pPrevBufferHeader)
    {
        NvxPortReleaseBuffer(pPortIn, pNvxVideoTvmrRenderer->pPrevBufferHeader);
        pNvxVideoTvmrRenderer->pPrevBufferHeader = NULL;
    }

    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererFlush(OMX_IN NvxComponent *pComponent,
                                        OMX_U32 nPortIndex)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer = 0;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoTvmrRendererFlush\n"));

    /* get TvmrRenderer instance */
    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoTvmrRenderer);

    if (!pNvxVideoTvmrRenderer->bInitialized)
        return OMX_ErrorNone;

    /* Make sure We release previous buffer sent to disp manager by passing NULL buffer*/
    /* FIXME Does it give black screen when NULL Buffer is sent ?
    nvdispmgr vaguely indicates that prev surface is displayed if no surface is sent */
    if (pNvxVideoTvmrRenderer->bYuvOverlay)
    {
        TVMRFlipQueueDisplayYUV(pNvxVideoTvmrRenderer->pFlipQueue,
                                TVMR_PICTURE_STRUCTURE_FRAME,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL
                               );
    }
    else
    {
        TVMRFlipQueueDisplayRGB(pNvxVideoTvmrRenderer->pFlipQueue,
                                NULL,
                                NULL,
                                0,
                                0,
                                NULL,
                                NULL,
                                TVMR_FLIP_QUEUE_COMPOSITE_TYPE_OPAQUE,
                                NULL
                               );
    }

    if (pNvxVideoTvmrRenderer->pPrevBufferHeader)
    {
        NvxPortReleaseBuffer(pPortIn, pNvxVideoTvmrRenderer->pPrevBufferHeader);
        pNvxVideoTvmrRenderer->pPrevBufferHeader = NULL;
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoTvmrRendererReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoTvmrRendererReleaseResources\n"));

    /* get TvmrRenderer instance */
    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoTvmrRenderer);

    if (!pNvxVideoTvmrRenderer->bInitialized)
        return OMX_ErrorNone;

    if (pNvxVideoTvmrRenderer->pFlipQueue)
    {
        TVMRFlipQueueDestroy(pNvxVideoTvmrRenderer->pFlipQueue);
    }

    if (pNvxVideoTvmrRenderer->pOutput)
    {
        TVMROutputDestroy(pNvxVideoTvmrRenderer->pOutput);
    }

    if (pNvxVideoTvmrRenderer->fenceDone)
    {
        TVMRFenceDestroy(pNvxVideoTvmrRenderer->fenceDone);
    }

    if (pNvxVideoTvmrRenderer->pDevice)
    {
        TVMRDeviceDestroy(pNvxVideoTvmrRenderer->pDevice);
    }

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));

    pNvxVideoTvmrRenderer->bInitialized = OMX_FALSE;

    return eError;
}

/*****************************************************************************/
static
OMX_ERRORTYPE NvxVideoTvmrRendererBaseInit(OMX_IN OMX_HANDLETYPE hComponent,
                                           TVMROutputType oType)
{
    SNvxVideoTvmrRendererData *pNvxVideoTvmrRenderer = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_VIDEO_TVMRRENDERER,"ERROR:NvxVideoTvmrRendererInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_VIDEO_TVMRRENDERER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVideoTvmrRendererData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    pNvxVideoTvmrRenderer = (SNvxVideoTvmrRendererData *)pNvComp->pComponentData;
    NvOsMemset(pNvxVideoTvmrRenderer, 0, sizeof(SNvxVideoTvmrRendererData));

    pNvComp->DeInit             = NvxVideoTvmrRendererDeInit;
    pNvComp->GetParameter       = NvxVideoTvmrRendererGetParameter;
    pNvComp->SetParameter       = NvxVideoTvmrRendererSetParameter;
    pNvComp->ReturnBuffers      = NvxVideoTvmrRendererReturnBuffers;
    pNvComp->Flush              = NvxVideoTvmrRendererFlush;
    pNvComp->FillThisBufferCB   = NULL;
    pNvComp->GetConfig          = NvxVideoTvmrRendererGetConfig;
    pNvComp->SetConfig          = NvxVideoTvmrRendererSetConfig;
    pNvComp->WorkerFunction     = NvxVideoTvmrRendererWorkerFunction;
    pNvComp->AcquireResources   = NvxVideoTvmrRendererAcquireResources;
    pNvComp->ReleaseResources   = NvxVideoTvmrRendererReleaseResources;
    pNvComp->CanUseEGLImage     = NULL;

    switch (oType)
    {
        case TVMR_OUTPUT_TYPE_HDMI:
            pNvComp->pComponentName = "OMX.Nvidia.video.hdmi.sink";
            break;
        case TVMR_OUTPUT_TYPE_CRT:
            pNvComp->pComponentName = "OMX.Nvidia.video.crt.sink";
            break;
        case TVMR_OUTPUT_TYPE_TFTLCD:
            pNvComp->pComponentName = "OMX.Nvidia.video.lvds.sink";
            break;
        default:
            pNvComp->pComponentName = "OMX.Nvidia.video.sink";
    }

    pNvComp->sComponentRoles[0] = "video_TvmrRenderer.generic";
    pNvComp->nComponentRoles    = 1;
    pNvxVideoTvmrRenderer->outputType = oType;

    pNvComp->pPorts[s_nvx_port_input].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MIN_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingAutoDetect);

    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    return eError;
}

OMX_ERRORTYPE NvxVideoTvmrRendererInit(OMX_IN OMX_HANDLETYPE hComponent)
{
   return NvxVideoTvmrRendererBaseInit(hComponent, TMVR_OUTPUT_TYPE_UNKNOWN);
}

OMX_ERRORTYPE NvxVideoTvmrRendererHDMIInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    return NvxVideoTvmrRendererBaseInit(hComponent, TVMR_OUTPUT_TYPE_HDMI);
}

OMX_ERRORTYPE NvxVideoTvmrRendererCRTInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    return NvxVideoTvmrRendererBaseInit(hComponent, TVMR_OUTPUT_TYPE_CRT);
}

OMX_ERRORTYPE NvxVideoTvmrRendererLVDSInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    return NvxVideoTvmrRendererBaseInit(hComponent, TVMR_OUTPUT_TYPE_TFTLCD);
}


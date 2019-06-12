/*
* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/* =========================================================================
*                     I N C L U D E S
* ========================================================================= */

#include "common/NvxComponent.h"
#include <math.h>
#include "common/NvxTrace.h"
#include "common/NvxHelpers.h"
#include "common/NvxWindowManager.h"
#include "common/nvxcrccheck.h"
#include "common/NvxCompReg.h"

#include "nvassert.h"
#include "nvos.h"
#include "nvcommon.h"
#include "nvrm_surface.h"
#include "nvddk_2d_v2.h"
#include "nvmm.h"
#include "nvapputil.h"

/* =========================================================================
*                     D E F I N E S
* ========================================================================= */

typedef struct SNvxVidExtractData
{
    NvxPort         *pInPort;
    NvxSurface      *pSrcSurface;
    
    NvBool           bSizeSet;
    OMX_BOOL         bSentFirstFrameEvent;
    
    OMX_U32          nCaptureBufSize;
    OMX_BOOL         bCopiedFrame;
    
    OMX_BOOL         bSourceFormatTiled;
    OMX_BOOL         bInternalSurfAlloced;
    
    NvRmFence        oFence;
    NvDdk2dSurface *        pDstDdk2dSurface;
    NvRmDeviceHandle hRmDevice;
    NvDdk2dHandle    h2dHandle;
    NvDdk2dBlitParameters TexParam;
    NvxSurface *pTmpSurface;
} SNvxVidExtractData;

#define NVX_VIDEO_PORT 0

    
static void Nvx2dExtractorFlush(SNvxVidExtractData* pNvxVidExtract)
{
    NvRmFence oFence;
    NvU32 NumFencesOut = 1;

    if (!pNvxVidExtract)
        return;
    
    // Commit blt and wait for completion
    NvDdk2dSurfaceLock(pNvxVidExtract->pDstDdk2dSurface,
                       NvDdk2dSurfaceAccessMode_ReadWrite,
                       NULL,
                       &oFence,
                       &NumFencesOut);
    
    NvDdk2dSurfaceUnlock(pNvxVidExtract->pDstDdk2dSurface,
                         &oFence, 1);
    
    NvRmFenceWait(pNvxVidExtract->hRmDevice, &oFence, NV_WAIT_INFINITE);
    
    return;
}


static OMX_ERRORTYPE NvxCopyFrame(SNvxVidExtractData* pNvxVidExtract, NvxSurface *pSrc, NvxSurface *pDest,
                                  OMX_U32 nWidth, OMX_U32 nHeight)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err = NvSuccess;
    NvRect SrcRect, DstRect;
    NvDdk2dHandle    h2d = pNvxVidExtract->h2dHandle;
    NvDdk2dSurface *pSrcDdk2dSurface = NULL;
    NvDdk2dFixedRect SrcRectLocal;

    if (!h2d)
        return OMX_ErrorBadParameter;
    
    // Setup surfaces
    Err = NvDdk2dSurfaceCreate(h2d,
                               NvDdk2dSurfaceType_Y_U_V,
                               &(pSrc->Surfaces[0]),
                               &pSrcDdk2dSurface);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }
    
    if (pNvxVidExtract->pDstDdk2dSurface)
    {
        NvDdk2dSurfaceDestroy(pNvxVidExtract->pDstDdk2dSurface);
        pNvxVidExtract->pDstDdk2dSurface = NULL;
    }
    Err = NvDdk2dSurfaceCreate(h2d,
                               NvDdk2dSurfaceType_Single,
                               &(pDest->Surfaces[0]),
                               &pNvxVidExtract->pDstDdk2dSurface);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }
    
    // Set attributes
    NvDdk2dSetBlitFilter(&pNvxVidExtract->TexParam, NvDdk2dStretchFilter_Nicest);
    NvDdk2dSetBlitTransform(&pNvxVidExtract->TexParam, NvDdk2dTransform_None);

    // Compute SB settings
    NvOsMemset(&SrcRect, 0, sizeof(NvRect));
    NvOsMemset(&DstRect, 0, sizeof(NvRect));
    SrcRect.right = DstRect.right = nWidth;
    SrcRect.bottom = DstRect.bottom = nHeight;
    
    ConvertRect2Fx(&SrcRect, &SrcRectLocal);
    
    // Stretch!
    Err = NvDdk2dBlit(h2d, pNvxVidExtract->pDstDdk2dSurface, &DstRect,
                      pSrcDdk2dSurface, &SrcRectLocal,
                      &pNvxVidExtract->TexParam);
    if (NvSuccess != Err)
    {
        eError = OMX_ErrorUndefined;
        goto L_cleanup;
    }
    
    Nvx2dExtractorFlush(pNvxVidExtract);
    
L_cleanup:
    if (pSrcDdk2dSurface)
        NvDdk2dSurfaceDestroy(pSrcDdk2dSurface);

    return eError;
}

static OMX_ERRORTYPE NvxCaptureRawFrame(SNvxVidExtractData* pNvxVidExtract, 
                                        NvxPort* pVideoPort)
{
    NvxSurface *pSurface;
    NvError err;
    
    OMX_U32 nWidth = pVideoPort->oPortDef.format.video.nFrameWidth;
    OMX_U32 nHeight = pVideoPort->oPortDef.format.video.nFrameHeight;
    
    pNvxVidExtract->nCaptureBufSize = 0;
    
    pNvxVidExtract->nCaptureBufSize = nWidth * nHeight * 2;
    
    pSurface = pNvxVidExtract->pSrcSurface;
    
    if (!pSurface)
        return OMX_ErrorNotReady;
    
    if((pSurface->Surfaces[0].hMem == NULL) ||  \
        (pSurface->Surfaces[0].Width <= 0) ||    \
        (pSurface->Surfaces[0].Height <= 0))
    {
        return OMX_ErrorNotReady;
    }

    if (pNvxVidExtract->pTmpSurface)
        NvxSurfaceFree(&(pNvxVidExtract->pTmpSurface));

    err = NvxSurfaceAlloc(&pNvxVidExtract->pTmpSurface, nWidth, nHeight, NvColorFormat_R5G6B5, 
        NvRmSurfaceLayout_Pitch);
    if (NvSuccess != err)
    {
        return OMX_ErrorNotReady;
    }
    
    NvxCopyFrame(pNvxVidExtract, pSurface, pNvxVidExtract->pTmpSurface, nWidth, nHeight);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVidExtractGetConfig(NvxComponent* pNvComp,
                                            OMX_INDEXTYPE nIndex,
                                            OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVidExtractData *pNvxVidExtract = NULL;
    
    NV_ASSERT(pNvComp);
    
    pNvxVidExtract = (SNvxVidExtractData *)pNvComp->pComponentData;
    
    switch (nIndex)
    {
    case NVX_IndexConfigCaptureRawFrame:
        {
            NVX_CONFIG_CAPTURERAWFRAME *pRawFrame = (NVX_CONFIG_CAPTURERAWFRAME *)pComponentConfigStructure;
            
            if (pRawFrame->pBuffer == NULL || 
                pRawFrame->nBufferSize < pNvxVidExtract->nCaptureBufSize)
            {
                eError = OMX_ErrorBadParameter;
                pRawFrame->nBufferSize = pNvxVidExtract->nCaptureBufSize;
                break;
            }
            
            if (!pNvxVidExtract->pTmpSurface)
                eError = OMX_ErrorNotReady;
            else
            {
                NvRmSurfaceRead(&(pNvxVidExtract->pTmpSurface->Surfaces[0]), 0, 0, pNvxVidExtract->pTmpSurface->Surfaces[0].Width, 
                                  pNvxVidExtract->pTmpSurface->Surfaces[0].Height, pRawFrame->pBuffer);
                
                eError = OMX_ErrorNone;
            }
            break;
        }
    case NVX_IndexConfigCustomColorFormt:
        {
            NVX_CONFIG_CUSTOMCOLORFORMAT *pFormat = (NVX_CONFIG_CUSTOMCOLORFORMAT *)pComponentConfigStructure;
            pFormat->bTiled = (OMX_BOOL)pNvxVidExtract->bSourceFormatTiled;
            break;
        }
    default:
        return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxVidExtractSetConfig(NvxComponent* pNvComp,
                                            OMX_INDEXTYPE nIndex,
                                            OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVidExtractData *pNvxVidExtract = 0;
    
    NV_ASSERT(pNvComp);
    
    pNvxVidExtract = (SNvxVidExtractData *)pNvComp->pComponentData;
    
    switch (nIndex)
    {
    case NVX_IndexConfigCustomColorFormt:
        {
            NVX_CONFIG_CUSTOMCOLORFORMAT *pFormat = (NVX_CONFIG_CUSTOMCOLORFORMAT *)pComponentConfigStructure;
            pNvxVidExtract->bSourceFormatTiled = (NvBool)pFormat->bTiled;
            break;
        }
    default:
        return NvxComponentBaseSetConfig(pNvComp, nIndex, pComponentConfigStructure);
    }
    
    return eError;
}

static OMX_ERRORTYPE NvxVidExtractAcquireResources(NvxComponent *hComponent)
{
    NvError nverr;
    SNvxVidExtractData *pNvxVidExtract = 0;
    NV_ASSERT(hComponent);
    pNvxVidExtract = (SNvxVidExtractData *)hComponent->pComponentData;
    nverr = NvRmOpen(&pNvxVidExtract->hRmDevice, 0);
    if(NvSuccess != nverr)
    {
        return OMX_ErrorUndefined;
    }
    
    nverr = NvDdk2dOpen(pNvxVidExtract->hRmDevice, NULL, 
        &(pNvxVidExtract->h2dHandle));
    if (NvSuccess != nverr)
        return OMX_ErrorUndefined;
    
    return NvxComponentBaseAcquireResources(hComponent);
}

static OMX_ERRORTYPE NvxVidExtractReleaseResources(NvxComponent *hComponent)
{
    SNvxVidExtractData *pNvxVidExtract = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;
    
    NV_ASSERT(hComponent);
    pNvxVidExtract = (SNvxVidExtractData *)hComponent->pComponentData;

    pNvxVidExtract->bSizeSet = OMX_FALSE;

    if (pNvxVidExtract->bInternalSurfAlloced)
    {
        NvxSurfaceFree(&pNvxVidExtract->pSrcSurface);
        pNvxVidExtract->bInternalSurfAlloced = NV_FALSE;
    }

    if (pNvxVidExtract->pTmpSurface)
        NvxSurfaceFree(&(pNvxVidExtract->pTmpSurface));
    if (pNvxVidExtract->pDstDdk2dSurface)
    {
        NvDdk2dSurfaceDestroy(pNvxVidExtract->pDstDdk2dSurface);
        pNvxVidExtract->pDstDdk2dSurface = NULL;
    }
    
    if (pNvxVidExtract->h2dHandle)
    {
        NvDdk2dClose(pNvxVidExtract->h2dHandle);
        pNvxVidExtract->h2dHandle = NULL;
    }

    if (pNvxVidExtract->hRmDevice)
    {
        NvRmClose(pNvxVidExtract->hRmDevice);
        pNvxVidExtract->hRmDevice = NULL;
    }
    
    NvxCheckError(eError, NvxComponentBaseReleaseResources(hComponent));
    
    return eError;
}

static OMX_ERRORTYPE NvxVidExtractFlush(NvxComponent *hComponent, 
                                        OMX_U32 nPort)
{
    SNvxVidExtractData * pNvxVidExtract = hComponent->pComponentData;
    pNvxVidExtract->bSentFirstFrameEvent = OMX_FALSE;
    pNvxVidExtract->bCopiedFrame = OMX_FALSE;
    
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVidExtractPreChangeState(NvxComponent *pNvComp,
                                                 OMX_STATETYPE oNewState)
{
    SNvxVidExtractData * pNvxVidExtract = pNvComp->pComponentData;
    switch (oNewState)
    {
    case OMX_StateIdle:
        pNvxVidExtract->bSentFirstFrameEvent = OMX_FALSE;
        pNvxVidExtract->bCopiedFrame = OMX_FALSE;
        break;
    default: break;
    }
    
    return OMX_ErrorNone;
}

static void NvxWriteOMXFrameDataToNvSurf(SNvxVidExtractData *pNvxVidExtract, 
                                         OMX_BUFFERHEADERTYPE *pInBuffer)
{
    NvBool bSourceTiled = pNvxVidExtract->bSourceFormatTiled;
    
    NvxCopyOMXSurfaceToNvxSurface(pInBuffer,
        pNvxVidExtract->pSrcSurface,
        bSourceTiled);
}

static OMX_ERRORTYPE NvxVidExtractPrepareSource(NvxComponent *hComponent,
                                                SNvxVidExtractData *pNvxVidExtract)
{
    NvxPort *pInPort = pNvxVidExtract->pInPort;
    OMX_U32 nWidth = pInPort->oPortDef.format.video.nFrameWidth;
    OMX_U32 nHeight = pInPort->oPortDef.format.video.nFrameHeight;
    NvMMBuffer *pInBuf = NULL;
    NvError err;
    NvBool bNvidiaTunneled = NV_FALSE;
    
    if (!pInPort->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }
    
    // check whether we're tunneled w/ NVIDIA components:
    bNvidiaTunneled =
        (hComponent->pPorts[NVX_VIDEO_PORT].bNvidiaTunneling) &&
        (hComponent->pPorts[NVX_VIDEO_PORT].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);
    
    if (bNvidiaTunneled)
    {
        pInBuf = (NvMMBuffer *)pInPort->pCurrentBufferHdr->pBuffer;
        pNvxVidExtract->pSrcSurface = (NvxSurface *)&pInBuf->Payload.Surfaces;
    }
    else if (!pNvxVidExtract->pSrcSurface)
    {
        // Only need a surface if the client is passing in its own OMX buffers. Otherwise, we're
        // tunneled, and can handle the color conversions in the 2D DDK
        
        // Default to tiled layout
        NvU32 Layout = NvRmSurfaceLayout_Pitch;
        if (pNvxVidExtract->bSourceFormatTiled)
            Layout = NvRmSurfaceLayout_Tiled;
        
        err = NvxSurfaceAlloc(
            &pNvxVidExtract->pSrcSurface,
            nWidth,
            nHeight,
            NvColorFormat_Y8,
            Layout);
        if (NvSuccess != err)
        {
            NV_DEBUG_PRINTF(("Error: Unable to allocate source surface\n"));
            return OMX_ErrorInsufficientResources;
        }
        pNvxVidExtract->bInternalSurfAlloced = NV_TRUE;
    }
    
    if (!bNvidiaTunneled)
    {
        // Not tunneled: Internal surface should exist
        NV_ASSERT(pNvxVidExtract->pSrcSurface != NULL);
        NvxWriteOMXFrameDataToNvSurf(pNvxVidExtract, pInPort->pCurrentBufferHdr);
    }
    
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVidExtractWorker(NvxComponent *hComponent,
                                         OMX_BOOL bAllPortsReady, 
                                         OMX_BOOL *pbMoreWork,
                                         NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVidExtractData *pNvxVidExtract = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort* pVideoPort = &hComponent->pPorts[NVX_VIDEO_PORT];
    OMX_BUFFERHEADERTYPE *pInBuffer;
    
    *pbMoreWork = bAllPortsReady;
    pNvxVidExtract = (SNvxVidExtractData *)hComponent->pComponentData;
    if (!pNvxVidExtract)
        return OMX_ErrorBadParameter;
    
    if (pNvxVidExtract->bCopiedFrame)
    {
        *pbMoreWork = OMX_FALSE;
        return OMX_ErrorNone;
    }
    
    pNvxVidExtract->pInPort = pVideoPort;
    
    if (!pVideoPort->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pVideoPort);
        
        if (!pVideoPort->pCurrentBufferHdr)
        {
            //*puMaxMsecToNextCall = 5;
            if (!hComponent->bNeedRunWorker)
                return OMX_ErrorNone;
        }
    }
    // If port properties have not been set, acquire default settings:
    if (pVideoPort->hTunnelComponent && !pNvxVidExtract->bSizeSet &&
        pVideoPort->pCurrentBufferHdr)
    {
        OMX_PARAM_PORTDEFINITIONTYPE oPortDefinition;
        oPortDefinition.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        oPortDefinition.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
        oPortDefinition.nPortIndex = pVideoPort->nTunnelPort;
        eError = OMX_GetParameter(pVideoPort->hTunnelComponent,
            OMX_IndexParamPortDefinition,
            &oPortDefinition);
        NvOsMemcpy(&(pVideoPort->oPortDef.format.video),
            &(oPortDefinition.format.video),
            sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
        // Keep current native renderer settings
        pNvxVidExtract->bSizeSet = OMX_TRUE;
    }
    
    pInBuffer = pVideoPort->pCurrentBufferHdr;
    
    if(!(pVideoPort->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS))
    {
        eError = NvxVidExtractPrepareSource(hComponent, pNvxVidExtract);
        if(NvxIsError(eError))
        {
            return eError;
        }
        if (!pNvxVidExtract->pSrcSurface)
        {
            return OMX_ErrorBadParameter;
        }
        // copy frame
        NvxCaptureRawFrame(pNvxVidExtract, pVideoPort);
        pNvxVidExtract->bCopiedFrame = OMX_TRUE;
        if (!pNvxVidExtract->bSentFirstFrameEvent)
        {
            NvxSendEvent(hComponent, NVX_EventFirstFrameDisplayed,
                pVideoPort->oPortDef.nPortIndex, 0, 0);
            pNvxVidExtract->bSentFirstFrameEvent = OMX_TRUE;
        }
    }
    
    eError = NvxPortReleaseBuffer(pVideoPort, pInBuffer);
    pVideoPort->pCurrentBufferHdr = NULL;
    
    if (!pVideoPort->pCurrentBufferHdr)
        NvxPortGetNextHdr(pVideoPort);
    
    if (pVideoPort->pCurrentBufferHdr)
        *pbMoreWork = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxVidExtractDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    if (pNvComp->pComponentData)
    {
        NvOsFree(pNvComp->pComponentData);
        pNvComp->pComponentData = NULL;
    }
    
    return eError;
}

OMX_ERRORTYPE NvxVidExtractInit(OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxVidExtractData * pNvxVidExtract = NULL;
    
    eError = NvxComponentCreate(hComponent, 1, &pNvComp);
    if (OMX_ErrorNone != eError)
        return eError;
    
    pNvComp->DeInit             = NvxVidExtractDeInit;
    pNvComp->PreChangeState     = NvxVidExtractPreChangeState;
    pNvComp->Flush              = NvxVidExtractFlush;
    pNvComp->GetConfig          = NvxVidExtractGetConfig;
    pNvComp->SetConfig          = NvxVidExtractSetConfig;
    pNvComp->WorkerFunction     = NvxVidExtractWorker;
    pNvComp->ReleaseResources   = NvxVidExtractReleaseResources;
    pNvComp->AcquireResources   = NvxVidExtractAcquireResources;
    
    pNvComp->nComponentRoles = 0;
    pNvComp->pComponentName = "OMX.Nvidia.video.extractor";
    
    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVidExtractData));
    if (!pNvComp->pComponentData)
    {
        return OMX_ErrorInsufficientResources;
    }
    
    pNvxVidExtract = (SNvxVidExtractData *)pNvComp->pComponentData;
    NvOsMemset(pNvComp->pComponentData, 0, sizeof(SNvxVidExtractData));
    
    NvxPortInitVideo( &pNvComp->pPorts[NVX_VIDEO_PORT], OMX_DirInput,  2, 1024, OMX_VIDEO_CodingAutoDetect);
    pNvComp->pPorts[NVX_VIDEO_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;
    
    return eError;
}


/*
 * Copyright (c) 2006 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxImageDecoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "nvmm/components/common/nvmmtransformbase.h"
#include "nvassert.h"
#include "nvmm_videodec.h"
#include "nvfxmath.h"
#include "nvmm_exif.h"

#if USE_ANDROID_NATIVE_WINDOW
#include "NvxHelpers.h"
#include "nvxandroidbuffer.h"
#endif

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

#define MAX_INPUT_BUFFERS    4
#define MAX_OUTPUT_BUFFERS   4
#define MIN_INPUT_BUFFERSIZE 50*1024
#define MAX_INPUT_BUFFERSIZE 500*1024
#define MIN_OUTPUT_BUFFERSIZE 1024
#define MAX_OUTPUT_BUFFERSIZE (640*480*3)/2

/* Number of internal frame buffers (video memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_output          = 1;

/* decoder state information */
typedef struct SNvxImageDecoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;

    SNvxNvMMTransformData oBase;

#define TYPE_JPEG 0
    int oType;

    NvMMBlockType oBlockType;
    const char *sBlockName;
} SNvxImageDecoderData;


static OMX_ERRORTYPE NvxImageDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

static OMX_ERRORTYPE NvxImageDecoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxImageDecoderData *pNvxImageDecoder;
    OMX_U32 ColorFormat;

    pNvxImageDecoder = (SNvxImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    ColorFormat = pNvxImageDecoder->oBase.nColorFormat;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageDecoderGetParameter\n"));

    switch (nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        {
            OMX_ERRORTYPE eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            if((NvU32)s_nvx_port_output == pPortDef->nPortIndex)
            {
                if (pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].bHasSize)
                {
                    pPortDef->format.image.nFrameWidth = pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    pPortDef->format.image.nFrameHeight = pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].nHeight;
                    pPortDef->format.image.nStride = pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].nWidth;
                }

                switch(ColorFormat)
                {
                    case NvMMVideoDecColorFormat_YUV420:
                        pPortDef->nBufferSize = (pPortDef->format.image.nFrameWidth * pPortDef->format.image.nFrameHeight * 3)/2;
                        break;

                    case NvMMVideoDecColorFormat_YUV422:
                    case NvMMVideoDecColorFormat_YUV422T:
                        pPortDef->nBufferSize = pPortDef->format.image.nFrameWidth * pPortDef->format.image.nFrameHeight * 2;
                        break;

                    case NvMMVideoDecColorFormat_YUV444:
                    case NvMMVideoDecColorFormat_GRAYSCALE:
                        pPortDef->nBufferSize = pPortDef->format.image.nFrameWidth * pPortDef->format.image.nFrameHeight * 3;
                        break;

                    default:
                        NV_DEBUG_PRINTF(("Unsupported ColorFormat!"));
                        break;
                }
            }
            else
            {
                return eError;
            }
        }
        break;
    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxImageDecoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxImageDecoderData *pNvxImageDecoder;

    pNvxImageDecoder = (SNvxImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageDecoderSetParameter\n"));

    switch(nIndex)
    {
#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamEnableAndroidBuffers:
        {
            OMX_ERRORTYPE result;
            result = HandleEnableANB(pComponent, s_nvx_port_output, pComponentParameterStructure);
            pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].bWillCopyToANB = OMX_TRUE;
            return result;
        }
    case NVX_IndexParamUseAndroidNativeBuffer:
        {
            OMX_ERRORTYPE result;
            result =  HandleUseANB(pComponent, pComponentParameterStructure);
            return result;
        }
#endif
    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
}

static OMX_ERRORTYPE NvxImageDecoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxImageDecoderData *pNvxImageDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderGetConfig\n"));

    pNvxImageDecoder = (SNvxImageDecoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pTimestamp->bOverrideTimestamp = pNvxImageDecoder->bOverrideTimestamp;
        }
        break;
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pBlockReq = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!pNvxImageDecoder || !pNvxImageDecoder->bInitialized)
                return OMX_ErrorNotReady;

            pBlockReq->pNvMMTransform = &pNvxImageDecoder->oBase;
            pBlockReq->nNvMMPort = pBlockReq->nPortIndex; // nvmm and OMX port indexes match
        }
        break;
        case NVX_IndexConfigThumbnail:
        {
            NVX_CONFIG_THUMBNAIL *pTN = (NVX_CONFIG_THUMBNAIL *)pComponentConfigStructure;
            pTN->bEnabled = pNvxImageDecoder->oBase.bThumbnailPreferred;
            pTN->nWidth = pNvxImageDecoder->oBase.nThumbnailWidth;
            pTN->nHeight = pNvxImageDecoder->oBase.nThumbnailHeight;
            break;
        }
        case NVX_IndexConfigJpegInfo:
        {
            NVX_CONFIG_JPEGINFO *pJpegInfo = (NVX_CONFIG_JPEGINFO *)pComponentConfigStructure;

            if (!pNvxImageDecoder || !pNvxImageDecoder->bInitialized)
                return OMX_ErrorNotReady;

            pJpegInfo->PrimaryImageWidth  = pNvxImageDecoder->oBase.nPrimaryImageWidth;
            pJpegInfo->PrimaryImageHeight = pNvxImageDecoder->oBase.nPrimaryImageHeight;
            pJpegInfo->ColorFormat        = pNvxImageDecoder->oBase.eColorFormat;
            break;
        }
        case NVX_IndexConfigExifInfo:
        {
            NVX_CONFIG_EXIFINFO *pExifInfo = (NVX_CONFIG_EXIFINFO *)pComponentConfigStructure;
            pExifInfo->isEXIFPresent = 0;

            if (!pNvxImageDecoder || !pNvxImageDecoder->bInitialized)
                return OMX_ErrorNotReady;

            if (pNvxImageDecoder->oBase.isEXIFPresent)
            {
                NvMM_DECODED_EXIFInfo oExifInfo;
                pNvxImageDecoder->oBase.hBlock->GetAttribute(pNvxImageDecoder->oBase.hBlock,
                                                             NvMMAttribute_GetExifInfo,
                                                             sizeof(oExifInfo),
                                                             &oExifInfo);
	                pExifInfo->isEXIFPresent = 1;

                NvOsMemcpy(pExifInfo->Make, oExifInfo.Make, sizeof(pExifInfo->Make));
                NvOsMemcpy(pExifInfo->Model, oExifInfo.Model, sizeof(pExifInfo->Model));
                pExifInfo->ThumbnailCompression = (OMX_U32)oExifInfo.ThumbnailCompression;
                pExifInfo->ThumbnailOffset      = (OMX_U32)oExifInfo.ThumbnailOffset;
                pExifInfo->ThumbnailLength      = (OMX_U32)oExifInfo.ThumbnailLength;
                pExifInfo->ThumbnailImageWidth  = (OMX_U32)oExifInfo.ThumbnailImageWidth;
                pExifInfo->ThumbnailImageHeight = (OMX_U32)oExifInfo.ThumbnailImageHeight;
                pExifInfo->PrimaryImageWidth    = (OMX_U32)oExifInfo.PrimaryImageWidth;
                pExifInfo->PrimaryImageHeight   = (OMX_U32)oExifInfo.PrimaryImageHeight;
                pExifInfo->ResolutionUnit       = (OMX_U8 )oExifInfo.ResolutionUnit;
                pExifInfo->XResolution          = (OMX_U64)oExifInfo.XResolution;
                pExifInfo->YResolution          = (OMX_U64)oExifInfo.YResolution;
                pExifInfo->bpp                  = (OMX_U8 )oExifInfo.bpp;
                NvOsMemcpy(pExifInfo->ImageDescription, oExifInfo.ImageDescription,
                           sizeof(pExifInfo->ImageDescription));
            }
        }
        break;
        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxImageDecoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxImageDecoderData *pNvxImageDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderSetConfig\n"));

    pNvxImageDecoder = (SNvxImageDecoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case OMX_IndexConfigCommonScale:
        {
            OMX_CONFIG_SCALEFACTORTYPE *pScaleFactor = (OMX_CONFIG_SCALEFACTORTYPE *)pComponentConfigStructure;
            pNvxImageDecoder->oBase.xScaleFactorWidth = pScaleFactor->xWidth;
            pNvxImageDecoder->oBase.xScaleFactorHeight = pScaleFactor->xHeight;
            break;
        }
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pNvxImageDecoder->bOverrideTimestamp = pTimestamp->bOverrideTimestamp;
            break;
        }
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMTransformSetProfile(&pNvxImageDecoder->oBase, pProf);
            break;
        }
        case NVX_IndexConfigThumbnail:
        {
            NVX_CONFIG_THUMBNAIL *pTN = (NVX_CONFIG_THUMBNAIL *)pComponentConfigStructure;
            pNvxImageDecoder->oBase.bThumbnailPreferred = pTN->bEnabled;
            pNvxImageDecoder->oBase.nThumbnailWidth = pTN->nWidth;
            pNvxImageDecoder->oBase.nThumbnailHeight = pTN->nHeight;
            break;
        }
        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxImageDecoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    OMX_BOOL bWouldFail = OMX_TRUE;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxImageDecoderWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;
    if (!bAllPortsReady)
        return OMX_ErrorNone;

    pNvxImageDecoder = (SNvxImageDecoderData *)pComponent->pComponentData;

    if (!pPortIn->pCurrentBufferHdr || !pPortOut->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    // Process data
    eError = NvxNvMMTransformWorkerBase(&pNvxImageDecoder->oBase,
                                        s_nvx_port_input, &bWouldFail);

    if (eError == OMX_ErrorMbErrorsInFrame)
    {
        if (pComponent->pCallbacks && pComponent->pCallbacks->EventHandler)
            pComponent->pCallbacks->EventHandler(pComponent->hBaseComponent,
                                             pComponent->pCallbackAppData,
                                             OMX_EventError,
                                             OMX_ErrorMbErrorsInFrame,
                                             0,
                                             0);

        eError = OMX_ErrorNone;
    }

    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);

    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);

    *pbMoreWork = (pPortIn->pCurrentBufferHdr && pPortOut->pCurrentBufferHdr &&
                   !bWouldFail);
    return eError;
}

static OMX_ERRORTYPE NvxImageDecoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageDecoderAcquireResources\n"));

    /* Get image decoder instance */
    pNvxImageDecoder = (SNvxImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxImageDecoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    eError = NvxNvMMTransformOpen(&pNvxImageDecoder->oBase, pNvxImageDecoder->oBlockType, pNvxImageDecoder->sBlockName, s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMTransformSetupInput(&pNvxImageDecoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_TRUE);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMTransformSetupOutput(&pNvxImageDecoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone)
        return eError;

    {
        OMX_BOOL bNvidiaTunneled =
        (pComponent->pPorts[s_nvx_port_output].bNvidiaTunneling) &&
        (pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES ||
         pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        NvxNvMMTransformSetStreamUsesSurfaces(&pNvxImageDecoder->oBase, s_nvx_port_output, bNvidiaTunneled);
    }

    pNvxImageDecoder->bInitialized = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxImageDecoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageDecoderReleaseResources\n"));

    /* get image decoder instance */
    pNvxImageDecoder = (SNvxImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    eError = NvxNvMMTransformClose(&pNvxImageDecoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxImageDecoder->bInitialized = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxImageDecoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageDecoderFlush\n"));

    /* get image decoder instance */
    pNvxImageDecoder = (SNvxImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    return NvxNvMMTransformFlush(&pNvxImageDecoder->oBase, nPort);
}

static OMX_ERRORTYPE NvxImageDecoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderFillThisBuffer\n"));

    pNvxImageDecoder = (SNvxImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMTransformFillThisBuffer(&pNvxImageDecoder->oBase, pBufferHdr, s_nvx_port_output);
}

static OMX_ERRORTYPE NvxImageDecoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderPreChangeState\n"));

    pNvxImageDecoder = (SNvxImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMTransformPreChangeState(&pNvxImageDecoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxImageDecoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderEmptyThisBuffer\n"));

    pNvxImageDecoder = (SNvxImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMTransformIsInputSkippingWorker(&pNvxImageDecoder->oBase,
                                              s_nvx_port_input))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
        *bHandled = OMX_FALSE;

    return eError;
}


static OMX_ERRORTYPE NvxImageDecoderPortEvent(NvxComponent *pNvComp,
                                              OMX_U32 nPort, OMX_U32 uEventType)
{
    SNvxImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderEmptyThisBuffer\n"));

    pNvxImageDecoder = (SNvxImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return eError;

    NvxNvMMTransformPortEventHandler(&pNvxImageDecoder->oBase, nPort,
                                     uEventType);

    return eError;
}

/*****************************************************************************/

static OMX_ERRORTYPE NvxCommonImageDecoderInit(OMX_HANDLETYPE hComponent, NvMMBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxImageDecoderData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_IMAGE_DECODER,"ERROR:NvxCommonImageDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_IMAGE_DECODER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxImageDecoderData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxImageDecoderData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxImageDecoderData));

    // TO DO: Fill in with image codecs
    switch (oBlockType)
    {
        case NvMMBlockType_DecSuperJPEG: data->oType = TYPE_JPEG; break;
        default: return OMX_ErrorBadParameter;
    }

    data->oBlockType = oBlockType;
    data->sBlockName = sBlockName;
    data->oBase.bThumbnailPreferred = OMX_FALSE;
    data->oBase.nThumbnailWidth = 0;
    data->oBase.nThumbnailHeight = 0;
    data->oBase.xScaleFactorWidth = NV_SFX_WHOLE_TO_FX(1);
    data->oBase.xScaleFactorHeight = NV_SFX_WHOLE_TO_FX(1);
    data->oBase.nExtraSurfaces = 0;

    pNvComp->DeInit             = NvxImageDecoderDeInit;
    pNvComp->GetParameter       = NvxImageDecoderGetParameter;
    pNvComp->SetParameter       = NvxImageDecoderSetParameter;
    pNvComp->GetConfig          = NvxImageDecoderGetConfig;
    pNvComp->SetConfig          = NvxImageDecoderSetConfig;
    pNvComp->WorkerFunction     = NvxImageDecoderWorkerFunction;
    pNvComp->AcquireResources   = NvxImageDecoderAcquireResources;
    pNvComp->ReleaseResources   = NvxImageDecoderReleaseResources;
    pNvComp->FillThisBufferCB   = NvxImageDecoderFillThisBuffer;
    pNvComp->PreChangeState     = NvxImageDecoderPreChangeState;
    pNvComp->EmptyThisBuffer    = NvxImageDecoderEmptyThisBuffer;
    pNvComp->Flush              = NvxImageDecoderFlush;
    pNvComp->PortEventHandler   = NvxImageDecoderPortEvent;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_output], MAX_OUTPUT_BUFFERSIZE);

    // TO DO: determine if we set bVideoTransform so that we can pass NvMM surfaces

    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxJpegDecoderInit\n"));

    eError = NvxCommonImageDecoderInit(hComponent, NvMMBlockType_DecSuperJPEG, "BlockSuperJpgDec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxJpegDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pNvComp->pComponentName = "OMX.Nvidia.jpeg.decoder";
    pNvComp->sComponentRoles[0] = "image_decoder.jpeg";
    pNvComp->nComponentRoles    = 1;

    NvxPortInitImage(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_IMAGE_CodingJPEG);

    return eError;
}

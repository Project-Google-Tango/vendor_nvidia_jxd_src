/*
 * Copyright (c) 2006-2013 NVIDIA Corporation.  All Rights Reserved.
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
#include "nvassert.h"
#include "nvos.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "nvmm/components/common/nvmmlitetransformbase.h"

#include "nvfxmath.h"
#include "nvmm_exif.h"
#include "nvmmlite_video.h"
#include "nvmmlite_videodec.h"

#if USE_ANDROID_NATIVE_WINDOW
#include "NvxHelpers.h"
#include "nvxliteandroidbuffer.h"
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
typedef struct SNvxLiteImageDecoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;

    SNvxNvMMLiteTransformData oBase;

#define TYPE_JPEG 0
    int oType;

    NvMMLiteBlockType oBlockType;
    const char *sBlockName;
} SNvxLiteImageDecoderData;


static OMX_ERRORTYPE NvxLiteImageDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageDecoderInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

static OMX_ERRORTYPE NvxLiteImageDecoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxLiteImageDecoderData *pNvxImageDecoder;
    OMX_U32 ColorFormat;

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    ColorFormat = pNvxImageDecoder->oBase.eColorFormat;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteImageDecoderGetParameter\n"));

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

static OMX_ERRORTYPE NvxLiteImageDecoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder;

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteImageDecoderSetParameter\n"));

    switch(nIndex)
    {
#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamEnableAndroidBuffers:
        {
            OMX_ERRORTYPE result;
            result = HandleEnableANBLite(pComponent, s_nvx_port_output, pComponentParameterStructure);
            pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].bWillCopyToANB = OMX_TRUE;
            pNvxImageDecoder->oBase.oPorts[s_nvx_port_output].bUsesANBs = (pComponent->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar);
            return result;
        }
    case NVX_IndexParamUseAndroidNativeBuffer:
        {
            OMX_ERRORTYPE result;
            result =  HandleUseANBLite(pComponent, pComponentParameterStructure);
            return result;
        }
#endif
    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
}

static OMX_ERRORTYPE NvxLiteImageDecoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxLiteImageDecoderData *pNvxImageDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteImageDecoderGetConfig\n"));

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
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
                                                             NvMMLiteAttribute_GetExifInfo,
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

static OMX_ERRORTYPE NvxLiteImageDecoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxLiteImageDecoderData *pNvxImageDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteImageDecoderSetConfig\n"));

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
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
            NvxNvMMLiteTransformSetProfile(&pNvxImageDecoder->oBase, pProf);
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

static OMX_ERRORTYPE NvxLiteImageDecoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    OMX_BOOL bWouldFail = OMX_TRUE;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxLiteImageDecoderWorkerFunction\n"));
    *pbMoreWork = bAllPortsReady;

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pComponent->pComponentData;

    if (!pPortIn->pCurrentBufferHdr && !pPortOut->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    // Process data
    eError = NvxNvMMLiteTransformWorkerBase(&pNvxImageDecoder->oBase,
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

    return eError;
}

static OMX_ERRORTYPE NvxLiteImageDecoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteImageDecoderAcquireResources\n"));

    /* Get image decoder instance */
    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxLiteImageDecoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    eError = NvxNvMMLiteTransformOpen(&pNvxImageDecoder->oBase, pNvxImageDecoder->oBlockType, pNvxImageDecoder->sBlockName, s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMLiteTransformSetupInput(&pNvxImageDecoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_TRUE);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMLiteTransformSetupOutput(&pNvxImageDecoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone)
        return eError;
    {
        OMX_BOOL bNvidiaTunneled =
        (pComponent->pPorts[s_nvx_port_output].bNvidiaTunneling) &&
        (pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES ||
         pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        NvxNvMMLiteTransformSetStreamUsesSurfaces(&pNvxImageDecoder->oBase, s_nvx_port_output, bNvidiaTunneled);
    }

    pNvxImageDecoder->bInitialized = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxLiteImageDecoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteImageDecoderReleaseResources\n"));

    /* get image decoder instance */
    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    eError = NvxNvMMLiteTransformClose(&pNvxImageDecoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxImageDecoder->bInitialized = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxLiteImageDecoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteImageDecoderFlush\n"));

    /* get image decoder instance */
    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageDecoder);

    return NvxNvMMLiteTransformFlush(&pNvxImageDecoder->oBase, nPort);
}

static OMX_ERRORTYPE NvxLiteImageDecoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteImageDecoderFillThisBuffer\n"));

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformFillThisBuffer(&pNvxImageDecoder->oBase, pBufferHdr, s_nvx_port_output);
}

static OMX_ERRORTYPE NvxLiteImageDecoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteImageDecoderPreChangeState\n"));

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformPreChangeState(&pNvxImageDecoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxLiteImageDecoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteImageDecoderEmptyThisBuffer\n"));

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMLiteTransformIsInputSkippingWorker(&pNvxImageDecoder->oBase,
                                              s_nvx_port_input))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
        *bHandled = OMX_FALSE;

    return eError;
}


static OMX_ERRORTYPE NvxLiteImageDecoderPortEvent(NvxComponent *pNvComp,
                                              OMX_U32 nPort, OMX_U32 uEventType)
{
    SNvxLiteImageDecoderData *pNvxImageDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteImageDecoderPortEvent\n"));

    pNvxImageDecoder = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
    if (!pNvxImageDecoder->bInitialized)
        return eError;

    NvxNvMMLiteTransformPortEventHandler(&pNvxImageDecoder->oBase, nPort,
                                     uEventType);

    return eError;
}

/*****************************************************************************/

static OMX_ERRORTYPE NvxLiteCommonImageDecoderInit(OMX_HANDLETYPE hComponent, NvMMLiteBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxLiteImageDecoderData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_IMAGE_DECODER,"ERROR:NvxCommonImageDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_IMAGE_DECODER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxLiteImageDecoderData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxLiteImageDecoderData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxLiteImageDecoderData));

    // TO DO: Fill in with image codecs
    switch (oBlockType)
    {
        case NvMMLiteBlockType_DecSuperJPEG: data->oType = TYPE_JPEG; break;
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

    pNvComp->DeInit             = NvxLiteImageDecoderDeInit;
    pNvComp->GetParameter       = NvxLiteImageDecoderGetParameter;
    pNvComp->SetParameter       = NvxLiteImageDecoderSetParameter;
    pNvComp->GetConfig          = NvxLiteImageDecoderGetConfig;
    pNvComp->SetConfig          = NvxLiteImageDecoderSetConfig;
    pNvComp->WorkerFunction     = NvxLiteImageDecoderWorkerFunction;
    pNvComp->AcquireResources   = NvxLiteImageDecoderAcquireResources;
    pNvComp->ReleaseResources   = NvxLiteImageDecoderReleaseResources;
    pNvComp->FillThisBufferCB   = NvxLiteImageDecoderFillThisBuffer;
    pNvComp->PreChangeState     = NvxLiteImageDecoderPreChangeState;
    pNvComp->EmptyThisBuffer    = NvxLiteImageDecoderEmptyThisBuffer;
    pNvComp->Flush              = NvxLiteImageDecoderFlush;
    pNvComp->PortEventHandler   = NvxLiteImageDecoderPortEvent;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_output], MAX_OUTPUT_BUFFERSIZE);

    // TO DO: determine if we set bVideoTransform so that we can pass NvMMLite surfaces

    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxLiteJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteJpegDecoderInit\n"));

    eError = NvxLiteCommonImageDecoderInit(hComponent, NvMMLiteBlockType_DecSuperJPEG, "BlockSuperJpgDec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteJpegDecoderInit:"
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



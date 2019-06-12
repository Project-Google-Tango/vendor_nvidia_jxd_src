/*
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** nvxtvmrmixer.c */

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
#include "tvmr.h"
#include "nvxeglimage.h"

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

#define MIN_INPUT_BUFFERS    20
#define MIN_OUTPUT_BUFFERS   4
#define MIN_INPUT_BUFFERSIZE  4096
#define MIN_OUTPUT_BUFFERSIZE 4096
#define NUM_TVMR_DECODE_BUFFERS 4

#define MAX_DECODED_SURFACES 10

#define MAX_OUTPUT_SURFACES 4
#define INVALID_TIMESTAMP   -1

typedef enum {
    BUFFER_TYPE_TVMR,
    BUFFER_TYPE_NVMM,
    BUFFER_TYPE_EGLIMAGE,
    BUFFER_TYPE_RAW,
    BUFFER_TYPE_NONE
} EBufferType;

typedef enum {
    INPUT_PORT_DATA_TYPE_MPEG2VLD,
    INPUT_PORT_DATA_TYPE_OTHER
} EInputPortDataType;

/* Number of internal frame buffers (video memory) */
#define MAX_PORTS 2
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_output          = 1;

/* decoder state information */
typedef struct SNvxVideoMixer
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;
    OMX_TICKS sDisplaytimeStamp;


    OMX_U32  uSourceWidth;
    OMX_U32  uSourceHeight;
    TVMRSurfaceType    eSourceColorFormat;
    NvxSurface         *pCurrentSourceSurface;

    /* Created if Deinterlacing is used in Raw mode */
    NvxSurface         *pPrevSourceSurface;
    NvxSurface         *pNextSourceSurface;

    OMX_U32  uDestinationWidth;
    OMX_U32  uDestinationHeight;
    TVMRSurfaceType    eDestinationColorFormat;
    NvxSurface *pDestinationSurface;

    //store locally to free them when release resources is done.
    NvRect               oCropRect;
    TVMRDevice*          hDevice;
    TVMRVideoMixer*      hMixer;
    TVMRFence             fenceDone;
    TVMRPictureStructure  ePictureStructure;

    //Video Surfaces
    TVMRVideoSurface     *pPrev2VideoSurface;
    TVMRVideoSurface     *pPrevVideoSurface;
    TVMRVideoSurface     *pCurrVideoSurface;
    TVMRVideoSurface     *pNextVideoSurface;
    TVMRVideoSurface     *pReleaseVideoSurface;

    //These are needed when input buffer type is not MPEG2VLD
    OMX_BUFFERHEADERTYPE *pPrev2BufferHdr;
    OMX_BUFFERHEADERTYPE *pPrevBufferHdr;
    OMX_BUFFERHEADERTYPE *pCurrBufferHdr;
    OMX_BUFFERHEADERTYPE *pNextBufferHdr;
    OMX_BUFFERHEADERTYPE *pReleaseBufferHdr;

    OMX_DeinterlaceMethod eDeinterlaceType;
    OMX_BOOL              bSecondField;
    EBufferType           eBufferType[MAX_PORTS];
    EInputPortDataType      eInputPortDataType;

    NvMMQueueHandle       pOutputSurfaceQueue;

    //Output surface contains  TVMROutputsurface for RGB out and TVMRVideoSurface for YUV out
    OMX_BOOL              bOutputYuvOverlay;

    float                 fFrameRate;
    NvxSurface            **ppSurfaceBuffers;
    OMX_BOOL              bUseNvBuffer[MAX_PORTS];
    OMX_BOOL              bUseEglImage[MAX_PORTS];
    OMX_BOOL              bEmbedRmSurface;
    NvxSurface            *pCurrentOutputSurface;

} SNvxVideoMixerData;


static NvError
NvxStretchBlitImageRGB (SNvxVideoMixerData *pNvxVideoMixer, NvxSurface *pDestinationSurface);

static NvError
NvxStretchBlitImageYUV (SNvxVideoMixerData *pNvxVideoMixer, NvxSurface *pDestinationSurface);

static void
NvxAllocateScratchSurfaceYuv(SNvxVideoMixerData *pNvxVideoMixer, NvxSurface **ppSurface, OMX_BOOL bSource);


static
OMX_ERRORTYPE NvxVideoMixerDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoMixerInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

static
OMX_ERRORTYPE NvxVideoMixerGetParameter(OMX_IN NvxComponent *pComponent,
                                        OMX_IN OMX_INDEXTYPE nParamIndex,
                                        OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxVideoMixerData *pNvxVideoMixer;

    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoMixer);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoMixerGetParameter\n"));

    switch (nParamIndex)
    {
    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoMixerSetParameter(OMX_IN NvxComponent *pComponent,
                                        OMX_IN OMX_INDEXTYPE nIndex,
                                        OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxVideoMixerData *pNvxVideoMixer;
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoMixerSetParameter\n"));

    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;
    switch(nIndex)
    {
        case NVX_IndexConfigUseNvBuffer:
        {
            NVX_PARAM_USENVBUFFER* pParam = (NVX_PARAM_USENVBUFFER*)pComponentParameterStructure;

            if (pParam->bUseNvBuffer)
            {
                if (pParam->nPortIndex == (unsigned)s_nvx_port_input)
                {
                    pNvxVideoMixer->bUseNvBuffer[s_nvx_port_input] = OMX_TRUE;
                }
                else if (pParam->nPortIndex == (unsigned)s_nvx_port_output)
                {
                    pNvxVideoMixer->bUseNvBuffer[s_nvx_port_output] = OMX_TRUE;
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
                pNvxVideoMixer->bEmbedRmSurface = param->bEnabled;
            }
        }
       break;
    case NVX_IndexParamDeinterlaceMethod:
    {
         NVX_PARAM_DEINTERLACE *deinterlaceparam;
         deinterlaceparam = (NVX_PARAM_DEINTERLACE*) pComponentParameterStructure;
         pNvxVideoMixer->eDeinterlaceType = deinterlaceparam->DeinterlaceMethod;
    }
    break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoParamPortFormat;

        pVideoParamPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*) pComponentParameterStructure;

        if (pVideoParamPortFormat->eCompressionFormat == OMX_VIDEO_CodingMPEG2)
        {
            pNvxVideoMixer->eInputPortDataType = INPUT_PORT_DATA_TYPE_MPEG2VLD;
        }
    }
    break;
    case OMX_IndexParamVideoMpeg2:
    {
        pNvxVideoMixer->eInputPortDataType = INPUT_PORT_DATA_TYPE_MPEG2VLD;
    }
    break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;

        if (pPortDef->nPortIndex == (unsigned)s_nvx_port_output)
        {
            pNvxVideoMixer->uDestinationWidth  = pPortDef->format.video.nFrameWidth;
            pNvxVideoMixer->uDestinationHeight = pPortDef->format.video.nFrameHeight;
        }
        else if (pPortDef->nPortIndex == (unsigned)s_nvx_port_input)
        {
            pNvxVideoMixer->uSourceWidth  = pPortDef->format.video.nFrameWidth;
            pNvxVideoMixer->uSourceHeight = pPortDef->format.video.nFrameHeight;
        }
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoMixerGetConfig(OMX_IN NvxComponent* pNvComp,
                                     OMX_IN OMX_INDEXTYPE nIndex,
                                     OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxVideoMixerData *pNvxVideoMixer;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoMixerGetConfig\n"));

    pNvxVideoMixer = (SNvxVideoMixerData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        // at minimum, acknowledge configs to caller with OMX_ErrorNone
        case NVX_IndexConfigTimestampOverride:
        case OMX_IndexConfigCommonInputCrop:
        case OMX_IndexConfigCommonBrightness:
        case OMX_IndexConfigCommonContrast:
        case OMX_IndexConfigCommonSaturation:
            break;
        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoMixerSetConfig(OMX_IN NvxComponent* pNvComp,
                                     OMX_IN OMX_INDEXTYPE nIndex,
                                     OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxVideoMixerData *pNvxVideoMixer;
    OMX_CONFIG_RECTTYPE *pCropRect;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoMixerSetConfig\n"));

    pNvxVideoMixer = (SNvxVideoMixerData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pNvxVideoMixer->bOverrideTimestamp = pTimestamp->bOverrideTimestamp;
            break;
        }
        case OMX_IndexConfigCommonInputCrop:
        {
            pCropRect = (OMX_CONFIG_RECTTYPE*) pComponentConfigStructure;
            pNvxVideoMixer->oCropRect.left  = pCropRect->nLeft;
            pNvxVideoMixer->oCropRect.top   = pCropRect->nTop;
            pNvxVideoMixer->oCropRect.right = pCropRect->nWidth;
            pNvxVideoMixer->oCropRect.bottom= pCropRect->nHeight;
            break;
        }
        case OMX_IndexConfigCommonBrightness:
        {
            TVMRVideoMixerAttributes oVmrAttrib;
            OMX_CONFIG_BRIGHTNESSTYPE *pBrightness = (OMX_CONFIG_BRIGHTNESSTYPE*) pComponentConfigStructure;
            oVmrAttrib.brightness = ((float) ((float)(pBrightness->nBrightness * 2 ) / 100)) - 1;
            TVMRVideoMixerSetAttributes(pNvxVideoMixer->hMixer, TVMR_VIDEO_MIXER_ATTRIBUTE_BRIGHTNESS, &oVmrAttrib);
        }
        break;
        case OMX_IndexConfigCommonContrast:
        {
            TVMRVideoMixerAttributes oVmrAttrib;
            OMX_CONFIG_CONTRASTTYPE *pContrast = (OMX_CONFIG_CONTRASTTYPE*) pComponentConfigStructure;
            oVmrAttrib.contrast = (float) (((float)(pContrast->nContrast + 100) * 10) / 200);
            TVMRVideoMixerSetAttributes(pNvxVideoMixer->hMixer, TVMR_VIDEO_MIXER_ATTRIBUTE_CONTRAST, &oVmrAttrib);
        }
        break;
        case OMX_IndexConfigCommonSaturation:
        {
            TVMRVideoMixerAttributes oVmrAttrib;
            OMX_CONFIG_SATURATIONTYPE *pSaturation = (OMX_CONFIG_SATURATIONTYPE*) pComponentConfigStructure;
            oVmrAttrib.saturation = (float) ((float)((pSaturation->nSaturation + 100) * 10) / 200);
            TVMRVideoMixerSetAttributes(pNvxVideoMixer->hMixer, TVMR_VIDEO_MIXER_ATTRIBUTE_SATURATION, &oVmrAttrib);
        }
        break;
        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static
NvError NvxStretchBlitImageYUV (SNvxVideoMixerData *pNvxVideoMixer,
                                NvxSurface *pDestinationSurface)
{
    TVMRVideoSurface oVideoSurface;
    TVMRSurface otvmrSurface[3];
    TVMRRect oTvmrVideoSrcRect = { 0, 0, 0, 0 };
    TVMRStatus status;
    NvU32 Area;

    Area = (pNvxVideoMixer->oCropRect.right - pNvxVideoMixer->oCropRect.left) *
            (pNvxVideoMixer->oCropRect.bottom - pNvxVideoMixer->oCropRect.top);

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

    /*If the crop rectangle exists Assign the crop parameters to the source rectangle  */
    if(Area)
    {
        oTvmrVideoSrcRect.x0 = pNvxVideoMixer->oCropRect.left;
        oTvmrVideoSrcRect.y0 = pNvxVideoMixer->oCropRect.top;
        oTvmrVideoSrcRect.x1 = pNvxVideoMixer->oCropRect.right;
        oTvmrVideoSrcRect.y1 = pNvxVideoMixer->oCropRect.bottom;
    }

    status =
        TVMRVideoMixerRenderYUV(
            pNvxVideoMixer->hMixer,                 // *vmr,
            &oVideoSurface,                         // *outputSurface,
            pNvxVideoMixer->ePictureStructure,      // TVMR_PICTURE_STRUCTURE_FRAME
            pNvxVideoMixer->pNextVideoSurface,      // *next,
            pNvxVideoMixer->pCurrVideoSurface,      // *curr,
            pNvxVideoMixer->pPrevVideoSurface,      // *prev,
            pNvxVideoMixer->pPrev2VideoSurface,     // *prev2,
            Area ? &oTvmrVideoSrcRect : NULL,       // *vidSrcRect,
            NULL,                                   //m_pTVMRFenceMixerReadList,
            pNvxVideoMixer->fenceDone               // *fencesWait,  /* NULL terminated list */
        );


    if (status != TVMR_STATUS_OK)
    {
        return OMX_ErrorUndefined;
    }

    TVMRFenceBlock(pNvxVideoMixer->hDevice, pNvxVideoMixer->fenceDone);

    return OMX_ErrorNone;
}

static
NvError NvxStretchBlitImageRGB (SNvxVideoMixerData *pNvxVideoMixer,
                                NvxSurface *pDestinationSurface)
{
    TVMROutputSurface oOutputSurface;
    TVMRSurface oTvmrOutputSurface;
    TVMRRect oTvmrVideoSrcRect = { 0, 0, 0, 0 };
    TVMRStatus status;
    NvU32 Area;

    Area = (pNvxVideoMixer->oCropRect.right - pNvxVideoMixer->oCropRect.left) *
            (pNvxVideoMixer->oCropRect.bottom - pNvxVideoMixer->oCropRect.top);

    oOutputSurface.type = TVMRSurfaceType_R8G8B8A8;
    oOutputSurface.width  = pDestinationSurface->Surfaces[0].Width;
    oOutputSurface.height = pDestinationSurface->Surfaces[0].Height;
    oOutputSurface.surface = &oTvmrOutputSurface;
    oOutputSurface.surface->pitch = pDestinationSurface->Surfaces[0].Pitch;
    oOutputSurface.surface->priv = (void*)&pDestinationSurface->Surfaces[0];

    /*If the crop rectangle exists Assign the crop parameters to the source rectangle  */
    if(Area)
    {
        oTvmrVideoSrcRect.x0 = pNvxVideoMixer->oCropRect.left;
        oTvmrVideoSrcRect.y0 = pNvxVideoMixer->oCropRect.top;
        oTvmrVideoSrcRect.x1 = pNvxVideoMixer->oCropRect.right;
        oTvmrVideoSrcRect.y1 = pNvxVideoMixer->oCropRect.bottom;
    }

    status =
        TVMRVideoMixerRender(
            pNvxVideoMixer->hMixer,                 // *vmr,
            &oOutputSurface,                        // *outputSurface,
            NULL,                                   // *outputRect,
            pNvxVideoMixer->ePictureStructure,      // TVMR_PICTURE_STRUCTURE_FRAME
            pNvxVideoMixer->pNextVideoSurface,      // *next,
            pNvxVideoMixer->pCurrVideoSurface,      // *curr,
            pNvxVideoMixer->pPrevVideoSurface,      // *prev,
            pNvxVideoMixer->pPrev2VideoSurface,     // *prev2,
            Area ? &oTvmrVideoSrcRect : NULL,       // *vidSrcRect,
            NULL,                                   // *vidDstRect,
            NULL,                                   // *subpic,
            NULL,                                   // *subSrcRect,
            NULL,                                   // *subDstRect,
            NULL,                                   //m_pTVMRFenceMixerReadList,
            pNvxVideoMixer->fenceDone               // *fencesWait,  /* NULL terminated list */
        );

    if (status != TVMR_STATUS_OK)
    {
        return OMX_ErrorUndefined;
    }

    TVMRFenceBlock(pNvxVideoMixer->hDevice, pNvxVideoMixer->fenceDone);
    return OMX_ErrorNone;
}

static
TVMRVideoSurface *GetTVMRVideoSurface(NvxSurface *pSourceSurface)
{
    int i;
    TVMRVideoSurface *pVideoSurface = NULL;

    pVideoSurface =  (TVMRVideoSurface*) malloc(sizeof(TVMRVideoSurface));

    pVideoSurface->type   = TVMRSurfaceType_YV12;
    pVideoSurface->width  = pSourceSurface->Surfaces[0].Width;
    pVideoSurface->height = pSourceSurface->Surfaces[0].Height;

    for (i = 0; i< 3; i++)
    {
        pVideoSurface->surfaces[i] = (TVMRSurface*) malloc(sizeof(TVMRSurface));
    }
    pVideoSurface->surfaces[0]->priv = &pSourceSurface->Surfaces[0];
    pVideoSurface->surfaces[1]->priv = &pSourceSurface->Surfaces[2];
    pVideoSurface->surfaces[2]->priv = &pSourceSurface->Surfaces[1];

    pVideoSurface->surfaces[0]->pitch = pSourceSurface->Surfaces[0].Pitch;
    pVideoSurface->surfaces[1]->pitch = pSourceSurface->Surfaces[2].Pitch;
    pVideoSurface->surfaces[2]->pitch = pSourceSurface->Surfaces[1].Pitch;

    return pVideoSurface;
}

static
void FreeTVMRVideoSurface(TVMRVideoSurface *pVideoSurface)
{
    int i;
    for (i = 0; i< 3; i++)
    {
        free(pVideoSurface->surfaces[i]);
    }

    free(pVideoSurface);
}

static
OMX_ERRORTYPE CheckMixerSettings(SNvxVideoMixerData *pNvxVideoMixer,
                                 NvxPort *pPortIn)
{
    NvxSurface *pSourceSurface = NULL;
    TVMRVideoSurface *pVideoSurface;
    NvMMBuffer *pInBuf;
    int uSourceWidth, uSourceHeight;
    OMX_BUFFERHEADERTYPE *pBufferHdr = pPortIn->pCurrentBufferHdr;


    switch (pPortIn->oPortDef.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
          pNvxVideoMixer->eSourceColorFormat =  TVMRSurfaceType_YV12;
          break;
    case NVX_ColorFormatYV16x2:
          pNvxVideoMixer->eSourceColorFormat = TVMRSurfaceType_YV16x2;
          break;
    default:
          break;
    }

    if (pNvxVideoMixer->hMixer)
    {
        if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_TVMR)
        {
            pVideoSurface = (TVMRVideoSurface *) *((NvU32*)pBufferHdr->pBuffer);
            uSourceWidth  = pVideoSurface->width;
            uSourceHeight = pVideoSurface->height;
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_NVMM)
        {
            pInBuf = (NvMMBuffer *)pBufferHdr->pBuffer;
            pSourceSurface = (NvxSurface *)&pInBuf->Payload.Surfaces;
            uSourceWidth  = pSourceSurface->Surfaces[0].Width;
            uSourceHeight = pSourceSurface->Surfaces[0].Height;
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_RAW)
        {
           uSourceWidth  =  pPortIn->oPortDef.format.video.nFrameWidth;
           uSourceHeight =  pPortIn->oPortDef.format.video.nFrameHeight;
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        if (pNvxVideoMixer->hMixer && (pNvxVideoMixer->hMixer->srcWidth != uSourceWidth ||
            pNvxVideoMixer->hMixer->srcHeight != uSourceHeight ||
            pNvxVideoMixer->eSourceColorFormat != pNvxVideoMixer->hMixer->type))
        {
            TVMRVideoMixerDestroy(pNvxVideoMixer->hMixer);
            pNvxVideoMixer->uSourceWidth  = uSourceWidth;
            pNvxVideoMixer->uSourceHeight = uSourceHeight;

            if (pNvxVideoMixer->eSourceColorFormat == TVMRSurfaceType_YV16x2)
            {
                pNvxVideoMixer->hMixer = TVMRVideoMixerCreate(pNvxVideoMixer->hDevice, TVMRSurfaceType_YV16x2,
                                                              pNvxVideoMixer->uSourceWidth, pNvxVideoMixer->uSourceHeight,
                                                              TVMR_VIDEO_MIXER_FEATURE_ADVANCE1_DEINTERLACING);
            }
            else if (pNvxVideoMixer->eSourceColorFormat == TVMRSurfaceType_YV12)
            {
                pNvxVideoMixer->hMixer = TVMRVideoMixerCreate(pNvxVideoMixer->hDevice,
                                                              TVMRSurfaceType_YV12,
                                                              pNvxVideoMixer->uSourceWidth,
                                                              pNvxVideoMixer->uSourceHeight,
                                                              TVMR_VIDEO_MIXER_FEATURE_ADVANCE1_DEINTERLACING);
            }
            else
            {
                pNvxVideoMixer->hMixer = TVMRVideoMixerCreate(pNvxVideoMixer->hDevice,
                                                              TVMRSurfaceType_NV12,
                                                              pNvxVideoMixer->uSourceWidth,
                                                              pNvxVideoMixer->uSourceHeight,
                                                              TVMR_VIDEO_MIXER_FEATURE_ADVANCE1_DEINTERLACING);
            }

            if(!pNvxVideoMixer->hMixer) {
                return OMX_ErrorInsufficientResources;
            }

            TVMRVideoMixerAttributes oVmrAttrib;
            OMX_U32 attri_mask = 0;

            if (pNvxVideoMixer->eDeinterlaceType != OMX_DeintMethod_NoDeinterlacing)
            {
                if ((pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_BobAtFieldRate) ||
                    (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_BobAtFrameRate))
                {
                    oVmrAttrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_BOB;
                }
                else if ((pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_Advanced1AtFieldRate) ||
                        (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_Advanced1AtFrameRate))
                {
                    oVmrAttrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_ADVANCE1;
                }
                attri_mask = TVMR_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE;
            }

            if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
            {
                oVmrAttrib.yInvert = OMX_TRUE;
                attri_mask |= TVMR_VIDEO_MIXER_ATTRIBUTE_Y_INVERT;
            }
            else
            {
                oVmrAttrib.yInvert = OMX_FALSE;
            }

            TVMRVideoMixerSetAttributes(pNvxVideoMixer->hMixer, attri_mask,  &oVmrAttrib);
        }
    }

    return OMX_ErrorNone;
}


static
TVMRVideoSurface * GetDisplaySurface(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;

    if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_TVMR)
    {
        pNvxVideoMixer->sDisplaytimeStamp =  pPortIn->pCurrentBufferHdr->nTimeStamp;
        return (TVMRVideoSurface *) *((NvU32*)pPortIn->pCurrentBufferHdr->pBuffer);
    }
    else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_NVMM)
    {
        NvMMBuffer * pInBuf = NULL;
        NvxSurface *pSourceSurface;
        pInBuf = (NvMMBuffer *)pPortIn->pCurrentBufferHdr->pBuffer;
        pSourceSurface = (NvxSurface *)&pInBuf->Payload.Surfaces;
        pNvxVideoMixer->sDisplaytimeStamp =  pPortIn->pCurrentBufferHdr->nTimeStamp;
        return GetTVMRVideoSurface(pSourceSurface);
    }
    else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_RAW)
    {
        if (NvxCopyOMXSurfaceToNvxSurface(pPortIn->pCurrentBufferHdr, pNvxVideoMixer->pCurrentSourceSurface, NV_FALSE) != NvSuccess)
        {
            return NULL;
        }

        pNvxVideoMixer->sDisplaytimeStamp = pPortIn->pCurrentBufferHdr->nTimeStamp;
        return GetTVMRVideoSurface(pNvxVideoMixer->pCurrentSourceSurface);
    }
    else {
        return NULL;
    }
}

static
OMX_ERRORTYPE NvxVideoMixerWorkerFunction( OMX_IN NvxComponent *pComponent,
                                           OMX_IN OMX_BOOL bAllPortsReady,
                                           OMX_OUT OMX_BOOL *pbMoreWork,
                                           OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    NvMMBuffer * pInBuf = NULL;
    NvMMBuffer * pOutBuf = NULL;
    NvxSurface *pSourceSurface = NULL;
    NvxSurface *pDestinationSurface = NULL;
    OMX_BOOL    bDisplayFrame = OMX_TRUE;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxVideoMixerWorkerFunction\n"));

    *puMaxMsecToNextCall = 10;

    *pbMoreWork = bAllPortsReady;
    if (!bAllPortsReady)
    {
        return OMX_ErrorNone;
    }

    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;

    if (!pPortIn->pCurrentBufferHdr || !pPortOut->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) {
        pPortOut->pCurrentBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    //Pass the timestamp to renderer
    if (pPortIn->pCurrentBufferHdr->nFilledLen > 0)
    {
        eError = CheckMixerSettings(pNvxVideoMixer, pPortIn);

        if (eError != OMX_ErrorNone)
        {
            return eError;
        }

        if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_EGLIMAGE)
        {
            NV_DEBUG_PRINTF(("EglImage at Input mixer is not supported for Video Mixer Component \n"));
            return OMX_ErrorBadParameter;
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_TVMR)
        {
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_NVMM)
        {
            pInBuf = (NvMMBuffer *)pPortIn->pCurrentBufferHdr->pBuffer;
            pSourceSurface = (NvxSurface *)&pInBuf->Payload.Surfaces;
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_RAW)
        {
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_NVMM) {

            NV_ASSERT(pPortOut->pCurrentBufferHdr->nAllocLen > sizeof(NvMMBuffer));

            if (pNvxVideoMixer->pCurrentOutputSurface == NULL)
            {
                NvMMQueueDeQ(pNvxVideoMixer->pOutputSurfaceQueue, &pNvxVideoMixer->pCurrentOutputSurface);

                if (!pNvxVideoMixer->pCurrentOutputSurface)
                {
                    //If There is not Output Surface.
                    goto leave;
                }
            }

            if(pNvxVideoMixer->bEmbedRmSurface)
            {
                NvOsMemcpy(pPortOut->pCurrentBufferHdr->pBuffer, pNvxVideoMixer->pCurrentOutputSurface->Surfaces,
                        NVMMSURFACEDESCRIPTOR_MAX_SURFACES* sizeof(NvRmSurface));

                pPortOut->pCurrentBufferHdr->nFilledLen = NVMMSURFACEDESCRIPTOR_MAX_SURFACES* sizeof(NvRmSurface);
                pPortOut->pCurrentBufferHdr->pInputPortPrivate = pNvxVideoMixer->pCurrentOutputSurface;

            }
            else
            {
                pOutBuf = (NvMMBuffer *)pPortOut->pCurrentBufferHdr->pBuffer;
                NvOsMemcpy(&(pOutBuf->Payload.Surfaces), pNvxVideoMixer->pCurrentOutputSurface, sizeof (NvxSurface));
                pOutBuf->pCore = pNvxVideoMixer->pCurrentOutputSurface;
                pOutBuf->PayloadType = NvMMPayloadType_SurfaceArray;
                pPortOut->pCurrentBufferHdr->nFilledLen = sizeof(NvMMBuffer);

            }

            pPortOut->pCurrentBufferHdr->nOffset = 0;
            pDestinationSurface = (NvxSurface*) pNvxVideoMixer->pCurrentOutputSurface;
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
        {
            pDestinationSurface = pNvxVideoMixer->pDestinationSurface;
            NvxEglImageSiblingHandle hEglSib = (NvxEglImageSiblingHandle)pPortOut->pCurrentBufferHdr->pBuffer;
            NvOsMemcpy(&pDestinationSurface->Surfaces[0], &hEglSib->surf, sizeof(NvRmSurface));
            pDestinationSurface->SurfaceCount = 1;
        }
        else if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_RAW)
        {
            pDestinationSurface = pNvxVideoMixer->pDestinationSurface;
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        if (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_NoDeinterlacing)
        {
            pNvxVideoMixer->pPrev2VideoSurface = NULL;
            pNvxVideoMixer->pPrevVideoSurface  = NULL;
            pNvxVideoMixer->pCurrVideoSurface = GetDisplaySurface(pComponent);

            pNvxVideoMixer->pNextVideoSurface  = NULL;
            pNvxVideoMixer->pReleaseVideoSurface = pNvxVideoMixer->pCurrVideoSurface;

            pNvxVideoMixer->pPrev2BufferHdr = NULL;
            pNvxVideoMixer->pPrevBufferHdr = NULL;
            pNvxVideoMixer->pCurrBufferHdr = pPortIn->pCurrentBufferHdr;
            pNvxVideoMixer->pNextBufferHdr = NULL;
            pNvxVideoMixer->pReleaseBufferHdr = pPortIn->pCurrentBufferHdr;

            pPortIn->pCurrentBufferHdr = NULL;
            pNvxVideoMixer->ePictureStructure = TVMR_PICTURE_STRUCTURE_FRAME;
            bDisplayFrame = OMX_TRUE;
        }
        else
        {
            if (pNvxVideoMixer->bSecondField)
            {
                if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_RAW)
                {
                    NvxSurface *pVideoSurface = pNvxVideoMixer->pCurrentSourceSurface;
                    pNvxVideoMixer->pCurrentSourceSurface = pNvxVideoMixer->pNextSourceSurface;
                    pNvxVideoMixer->pNextSourceSurface    = pNvxVideoMixer->pPrevSourceSurface;
                    pNvxVideoMixer->pPrevSourceSurface    = pVideoSurface;
                }

                pNvxVideoMixer->pPrev2VideoSurface = pNvxVideoMixer->pPrevVideoSurface;
                pNvxVideoMixer->pPrevVideoSurface  = pNvxVideoMixer->pCurrVideoSurface;
                pNvxVideoMixer->pNextVideoSurface  = GetDisplaySurface(pComponent);

                if (!pNvxVideoMixer->pNextVideoSurface)
                {
                    goto leave;
                }

                pNvxVideoMixer->pReleaseVideoSurface = pNvxVideoMixer->pPrev2VideoSurface;

                pNvxVideoMixer->pPrev2BufferHdr = pNvxVideoMixer->pPrevBufferHdr;
                pNvxVideoMixer->pPrevBufferHdr  = pNvxVideoMixer->pCurrBufferHdr;
                pNvxVideoMixer->pNextBufferHdr  = pPortIn->pCurrentBufferHdr;
                pNvxVideoMixer->pReleaseBufferHdr = pNvxVideoMixer->pPrev2BufferHdr;

                pNvxVideoMixer->bSecondField = OMX_FALSE;
                pNvxVideoMixer->ePictureStructure    = TVMR_PICTURE_STRUCTURE_BOTTOM_FIELD;
                if ((pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_BobAtFrameRate)  ||
                    (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_Advanced1AtFrameRate))
                {
                    bDisplayFrame = OMX_FALSE;
                }
                else
                {
                    bDisplayFrame = OMX_TRUE;
                }
            }
            else
            {
                if (!pNvxVideoMixer->pCurrVideoSurface)
                {
                    pNvxVideoMixer->pPrev2VideoSurface = NULL;
                    pNvxVideoMixer->pPrevVideoSurface  = NULL;
                    pNvxVideoMixer->pCurrVideoSurface = GetDisplaySurface(pComponent);
                    pNvxVideoMixer->pNextVideoSurface  = pNvxVideoMixer->pCurrVideoSurface;

                    pNvxVideoMixer->pPrev2BufferHdr = NULL;
                    pNvxVideoMixer->pPrevBufferHdr  = NULL;
                    pNvxVideoMixer->pCurrBufferHdr  = pPortIn->pCurrentBufferHdr;
                    pNvxVideoMixer->pNextBufferHdr  = pNvxVideoMixer->pCurrBufferHdr;
                }
                else
                {
                    pNvxVideoMixer->pPrev2VideoSurface = pNvxVideoMixer->pPrevVideoSurface;
                    pNvxVideoMixer->pPrevVideoSurface  = pNvxVideoMixer->pCurrVideoSurface;
                    pNvxVideoMixer->pCurrVideoSurface  = pNvxVideoMixer->pNextVideoSurface;

                    pNvxVideoMixer->pPrev2BufferHdr = pNvxVideoMixer->pPrevBufferHdr;
                    pNvxVideoMixer->pPrevBufferHdr  = pNvxVideoMixer->pCurrBufferHdr;
                    pNvxVideoMixer->pCurrBufferHdr  = pNvxVideoMixer->pNextBufferHdr;
                }

                pPortIn->pCurrentBufferHdr = NULL;

                bDisplayFrame = OMX_TRUE;
                pNvxVideoMixer->bSecondField = OMX_TRUE;
                pNvxVideoMixer->ePictureStructure    = TVMR_PICTURE_STRUCTURE_TOP_FIELD;
            }
        }

        if (!pNvxVideoMixer->pCurrVideoSurface)
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

        if (bDisplayFrame)
        {
            if (pNvxVideoMixer->bOutputYuvOverlay)
            {
                eError = NvxStretchBlitImageYUV(pNvxVideoMixer, pDestinationSurface);
            }
            else
            {
                eError = NvxStretchBlitImageRGB(pNvxVideoMixer, pDestinationSurface);
            }

            if (eError != TVMR_STATUS_OK)
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
        }

        if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_RAW && bDisplayFrame) {

            if (!pDestinationSurface->CropRect.left && !pDestinationSurface->CropRect.right &&
                !pDestinationSurface->CropRect.top  && !pDestinationSurface->CropRect.bottom)
            {
                pDestinationSurface->CropRect.left = 0;
                pDestinationSurface->CropRect.top  = 0;
                pDestinationSurface->CropRect.right  = pNvxVideoMixer->uDestinationWidth;
                pDestinationSurface->CropRect.bottom = pNvxVideoMixer->uDestinationHeight;
            }


            if (NvxCopyNvxSurfaceToOMXBuf(pDestinationSurface, pPortOut->pCurrentBufferHdr) != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
        }
    }

    {
        if (pNvxVideoMixer->pReleaseBufferHdr)
        {
            NvxPortReleaseBuffer(pPortIn, pNvxVideoMixer->pReleaseBufferHdr);
            pNvxVideoMixer->pReleaseBufferHdr = NULL;

            if ((pNvxVideoMixer->eBufferType[s_nvx_port_input] != BUFFER_TYPE_TVMR) && pNvxVideoMixer->pReleaseVideoSurface)
            {
                FreeTVMRVideoSurface(pNvxVideoMixer->pReleaseVideoSurface);
            }
            pNvxVideoMixer->pReleaseVideoSurface = NULL;
        }
        else if (pPortIn->pCurrentBufferHdr)
        {
            if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
            {
                NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
            }
        }
    }

    pPortOut->pCurrentBufferHdr->nTimeStamp = pNvxVideoMixer->sDisplaytimeStamp;

    if (bDisplayFrame)
    {
        NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
        pNvxVideoMixer->pCurrentOutputSurface = NULL;
    }

    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);

    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);

    *pbMoreWork = (pPortIn->pCurrentBufferHdr && pPortOut->pCurrentBufferHdr);
    return eError;

leave:
    *puMaxMsecToNextCall = 10;
    *pbMoreWork = OMX_FALSE;

    return eError;
}

static
void NvxAllocateScratchSurfaceYuv(SNvxVideoMixerData *pNvxVideoMixer,
                                  NvxSurface **ppSurface,
                                  OMX_BOOL bSource)
{
    NvU32 uImageSize;
    //Hardcoded values for NvRmSurfaceLayout_Blocklinear
    NvU16 Kind = NvRmMemKind_Generic_16Bx2;
    NvU16 BlockHeightLog2 = 4;

    /* Create the Scratch Surface */
    *ppSurface = NvOsAlloc(sizeof(NvxSurface));

    if (!(*ppSurface))
    {
        NvOsFree(*ppSurface);
        *ppSurface = NULL;
        return;
    }

    NvOsMemset(*ppSurface, 0, sizeof(NvxSurface));

    /* Source Surface ? */
    if (bSource)
    {
        if (NvxSurfaceAllocYuv(*ppSurface,
                               pNvxVideoMixer->uSourceWidth,
                               pNvxVideoMixer->uSourceHeight,
                               NvMMVideoDecColorFormat_YUV420,
                               NvRmSurfaceLayout_Pitch,
                               &uImageSize, NV_FALSE, Kind,BlockHeightLog2,
                               NV_FALSE) != NvSuccess)
        {
            NvOsFree(*ppSurface);
            *ppSurface = NULL;
            return;
        }
    }
    else
    {
        if (NvxSurfaceAllocYuv(*ppSurface,
                               pNvxVideoMixer->uDestinationWidth,
                               pNvxVideoMixer->uDestinationHeight,
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
}

static
OMX_ERRORTYPE NvxVideoMixerAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pOutPort = &pComponent->pPorts[s_nvx_port_output];
    NvxPort *pInPort  = &pComponent->pPorts[s_nvx_port_input];
    OMX_U32 i;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoMixerAcquireResources\n"));

    /* Get VideoMixer instance */
    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoMixer);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxVideoMixerAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    if (pNvxVideoMixer->bUseEglImage[s_nvx_port_input])
    {
        NV_DEBUG_PRINTF(("Egl image Not Supported at Input Port for video mixer component \n"));
        return OMX_ErrorBadParameter;
    }

    /* In Tunneling Mode, Output port type is decided based on Tunnel Transaction */
    /* In Non-Tunneled  Mode, Output Port type is decided based on NvBuffer Index Extension */
    if (pInPort->bNvidiaTunneling)
    {
        NvxPort *pTunnelInPort  = NULL;
        NvxComponent *pTunnelComponent = NULL;

        pTunnelComponent = NvxComponentFromHandle(pInPort->hTunnelComponent);

        pTunnelInPort = &pTunnelComponent->pPorts[pInPort->nTunnelPort];

        /* Check both the tunnel ports operate in same domain */
        if (pTunnelInPort->oPortDef.eDomain != pInPort->oPortDef.eDomain)
        {
            return OMX_ErrorBadParameter;
        }

        /* Check if port domain is Video Domain */
        if (pInPort->oPortDef.eDomain != OMX_PortDomainVideo)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure color formats match */
        if (pInPort->oPortDef.format.video.eColorFormat != pTunnelInPort->oPortDef.format.video.eColorFormat)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure The color formats are supported by this component */
        if ((pInPort->oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar) &&
            (pInPort->oPortDef.format.video.eColorFormat != NVX_ColorFormatYV16x2))
        {
            return OMX_ErrorBadParameter;
        }

        /* Initially Validate Incoming Data Type */
        /*check the tunnel transaction type */
        if (pInPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES)
        {
            if (pNvxVideoMixer->eInputPortDataType == INPUT_PORT_DATA_TYPE_OTHER)
            {
                if (pInPort->oPortDef.format.video.eColorFormat == NVX_ColorFormatYV16x2)
                {
                    pNvxVideoMixer->eBufferType[s_nvx_port_input] = BUFFER_TYPE_TVMR;
                    pNvxVideoMixer->eSourceColorFormat =  TVMRSurfaceType_YV16x2;
                }
                else
                {
                    pNvxVideoMixer->eBufferType[s_nvx_port_input] = BUFFER_TYPE_NVMM;
                    pNvxVideoMixer->eSourceColorFormat =  TVMRSurfaceType_YV12;
                }
            }
        }
        else if (pInPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_DEFAULT)
        {
            /* Raw Buffer Input Data from MPEG2VLD component is not supported */
            if (pNvxVideoMixer->eInputPortDataType == INPUT_PORT_DATA_TYPE_MPEG2VLD)
            {
                return OMX_ErrorBadParameter;
            }
            else if (pNvxVideoMixer->eInputPortDataType == INPUT_PORT_DATA_TYPE_OTHER)
            {
                pNvxVideoMixer->eBufferType[s_nvx_port_input] = BUFFER_TYPE_RAW;
                pNvxVideoMixer->eSourceColorFormat =  TVMRSurfaceType_YV12;
            }
        }
        else
        {
            return OMX_ErrorBadParameter;
        }
    }
    else
    {
        switch (pInPort->oPortDef.format.video.eColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:

             if (pNvxVideoMixer->eInputPortDataType == INPUT_PORT_DATA_TYPE_OTHER)
             {
                 if (pNvxVideoMixer->bUseNvBuffer[s_nvx_port_input])
                 {
                    pNvxVideoMixer->eBufferType[s_nvx_port_input] = BUFFER_TYPE_NVMM;
                 }
                 else
                 {
                    pNvxVideoMixer->eBufferType[s_nvx_port_input] = BUFFER_TYPE_RAW;
                 }
                 pNvxVideoMixer->eSourceColorFormat =  TVMRSurfaceType_YV12;
             }
             else
             {
                 return OMX_ErrorBadParameter;
             }
             break;
        case NVX_ColorFormatYV16x2:
             if (!pNvxVideoMixer->bUseNvBuffer[s_nvx_port_input])
             {
                 return OMX_ErrorBadParameter;
             }

             if (pNvxVideoMixer->eInputPortDataType != INPUT_PORT_DATA_TYPE_OTHER)
             {
                 return OMX_ErrorBadParameter;
             }

             pNvxVideoMixer->eBufferType[s_nvx_port_input]  = BUFFER_TYPE_TVMR;
             pNvxVideoMixer->eSourceColorFormat = TVMRSurfaceType_YV16x2;
             break;
        default:
             return OMX_ErrorBadParameter;
        }
    }

    pNvxVideoMixer->uSourceWidth   = pInPort->oPortDef.format.video.nFrameWidth;
    pNvxVideoMixer->uSourceHeight  = pInPort->oPortDef.format.video.nFrameHeight;

    if (pNvxVideoMixer->eBufferType[s_nvx_port_input] == BUFFER_TYPE_RAW)
    {
        NvxAllocateScratchSurfaceYuv(pNvxVideoMixer, &pNvxVideoMixer->pCurrentSourceSurface, OMX_TRUE);

        if (!pNvxVideoMixer->pCurrentSourceSurface)
        {
            return OMX_ErrorInsufficientResources;
        }

        if (pNvxVideoMixer->eDeinterlaceType != OMX_DeintMethod_NoDeinterlacing)
        {
            NvxAllocateScratchSurfaceYuv(pNvxVideoMixer, &pNvxVideoMixer->pPrevSourceSurface, OMX_TRUE);

            if (!pNvxVideoMixer->pPrevSourceSurface)
            {
                return OMX_ErrorInsufficientResources;
            }

            NvxAllocateScratchSurfaceYuv(pNvxVideoMixer, &pNvxVideoMixer->pNextSourceSurface, OMX_TRUE);

            if (!pNvxVideoMixer->pNextSourceSurface)
            {
                return OMX_ErrorInsufficientResources;
            }
        }
    }

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
        if (pOutPort->oPortDef.format.video.eColorFormat != pTunnelOutPort->oPortDef.format.video.eColorFormat)
        {
            return OMX_ErrorBadParameter;
        }

        /* Make sure The color formats are supported by this component */
        if ((pOutPort->oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar) &&
            (pOutPort->oPortDef.format.video.eColorFormat != OMX_COLOR_Format32bitARGB8888))
        {
            return OMX_ErrorBadParameter;
        }

        /* Initially Validate Incoming Data Type */
        /*check the tunnel transaction type */
        if (pOutPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES)
        {
            pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_NVMM;
        }
        else if (pOutPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_DEFAULT)
        {
            pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        if (pOutPort->oPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar)
        {
            pNvxVideoMixer->eDestinationColorFormat = TVMRSurfaceType_YV12;
            pNvxVideoMixer->bOutputYuvOverlay = OMX_TRUE;
        }
        else
        {
            pNvxVideoMixer->eDestinationColorFormat = TVMRSurfaceType_R8G8B8A8;
        }
    }
    else
    {
        switch (pOutPort->oPortDef.format.video.eColorFormat) {
        case OMX_COLOR_Format32bitARGB8888:
            if (pNvxVideoMixer->bUseEglImage[s_nvx_port_output])
            {
                if (pNvxVideoMixer->bUseNvBuffer[s_nvx_port_output])
                {
                    return OMX_ErrorBadParameter;
                }
                pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_EGLIMAGE;
            }
            else
            {
                if (pNvxVideoMixer->bUseNvBuffer[s_nvx_port_output])
                {
                    pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_NVMM;
                }
                else
                {
                    pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
                }
            }
            pNvxVideoMixer->eDestinationColorFormat = TVMRSurfaceType_R8G8B8A8;
            break;
        case OMX_COLOR_FormatYUV420Planar:
            if (pNvxVideoMixer->bUseEglImage[s_nvx_port_output])
            {
                return OMX_ErrorBadParameter;
            }

            if (pNvxVideoMixer->bUseNvBuffer[s_nvx_port_output])
            {
                pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_NVMM;
            }
            else
            {
                pNvxVideoMixer->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
            }

            pNvxVideoMixer->eDestinationColorFormat = TVMRSurfaceType_YV12;
            pNvxVideoMixer->bOutputYuvOverlay =  OMX_TRUE;

            break;
        default:
            return OMX_ErrorBadParameter;
        }
    }

    pNvxVideoMixer->uDestinationWidth   = pOutPort->oPortDef.format.video.nFrameWidth;
    pNvxVideoMixer->uDestinationHeight  = pOutPort->oPortDef.format.video.nFrameHeight;

    if (!pNvxVideoMixer->uDestinationWidth || !pNvxVideoMixer->uDestinationHeight)
    {
        NvxOverlay oOverlay;
        NvRect rtWin = {0,0,320,240};
        NvError nverr;
        NvColorFormat eColorFormat;

        if (pNvxVideoMixer->bOutputYuvOverlay)
        {
            eColorFormat= NvColorFormat_Y8;
        }
        else
        {
            eColorFormat = NvColorFormat_A8R8G8B8;
        }
        //Get Overlay Screen Dimensions.
        nverr = NvxAllocateOverlay(
                (NvU32*) &pNvxVideoMixer->uDestinationWidth,
                (NvU32*) &pNvxVideoMixer->uDestinationHeight,
                eColorFormat,
                &oOverlay,
                Rend_TypeOverlay,
                0,
                &rtWin, 0, 0, 0, 0, 0, 0, 0,
                DEFAULT_TEGRA_OVERLAY_INDEX,
                DEFAULT_TEGRA_OVERLAY_DEPTH,
                0, 0, NvRmSurfaceLayout_Pitch, 0,
                (NvUPtr) NULL, (NvUPtr) NULL, NV_FALSE);

        if (nverr != NvSuccess)
        {
            return OMX_ErrorInsufficientResources;
        }

        pNvxVideoMixer->uDestinationWidth  = oOverlay.screenWidth;
        pNvxVideoMixer->uDestinationHeight = oOverlay.screenHeight;

        pOutPort->oPortDef.format.video.nFrameWidth  = oOverlay.screenWidth;
        pOutPort->oPortDef.format.video.nFrameHeight = oOverlay.screenHeight;
        NvxReleaseOverlay(&oOverlay);
    }


    pNvxVideoMixer->hDevice = TVMRDeviceCreate(NULL);

    if(!pNvxVideoMixer->hDevice) {
        return OMX_ErrorInsufficientResources;
    }

    //For Surfaces Other than EGLImage, Have Local supplier of TVMR Surfaces
    //In case of EGLImages, EglImages will be supplied to us.
    if (pNvxVideoMixer->eBufferType[s_nvx_port_output] != BUFFER_TYPE_EGLIMAGE)
    {
        if (pNvxVideoMixer->eBufferType[s_nvx_port_output] != BUFFER_TYPE_RAW)
        {
            NvMMQueueCreate(&pNvxVideoMixer->pOutputSurfaceQueue, MAX_OUTPUT_SURFACES, sizeof(NvU32*), NV_FALSE);

            if (!pNvxVideoMixer->pOutputSurfaceQueue)
            {
                return OMX_ErrorInsufficientResources;
            }

            pNvxVideoMixer->ppSurfaceBuffers = (NvxSurface**) NvOsAlloc(MAX_OUTPUT_SURFACES * sizeof(NvxSurface*));

            if (!pNvxVideoMixer->ppSurfaceBuffers)
            {
                return OMX_ErrorInsufficientResources;
            }

            if (pNvxVideoMixer->bOutputYuvOverlay)
            {
                for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
                {
                    NvxAllocateScratchSurfaceYuv(pNvxVideoMixer, &pNvxVideoMixer->ppSurfaceBuffers[i], OMX_FALSE);

                    if (!pNvxVideoMixer->ppSurfaceBuffers[i])
                    {
                        return OMX_ErrorInsufficientResources;
                    }

                    NvMMQueueEnQ(pNvxVideoMixer->pOutputSurfaceQueue, &pNvxVideoMixer->ppSurfaceBuffers[i], 2000);
                }
            }
            else
            {
                for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
                {
                    /* eFormat should take nvcolorformat type */
                    if (NvxSurfaceAlloc(&pNvxVideoMixer->ppSurfaceBuffers[i],
                                        pNvxVideoMixer->uDestinationWidth,
                                        pNvxVideoMixer->uDestinationHeight,
                                        NvColorFormat_A8B8G8R8,
                                        NvRmSurfaceLayout_Pitch) != NvSuccess)
                    {
                        return OMX_ErrorInsufficientResources;
                    }

                    if (!pNvxVideoMixer->ppSurfaceBuffers[i])
                    {
                        return OMX_ErrorInsufficientResources;
                    }
                    NvMMQueueEnQ(pNvxVideoMixer->pOutputSurfaceQueue, &pNvxVideoMixer->ppSurfaceBuffers[i], 2000);
                }
            }
        }
        else
        {
            if (pNvxVideoMixer->bOutputYuvOverlay)
            {
                NvxAllocateScratchSurfaceYuv(pNvxVideoMixer, &pNvxVideoMixer->pDestinationSurface, OMX_FALSE);

                if (!pNvxVideoMixer->pDestinationSurface)
                {
                    return OMX_ErrorInsufficientResources;
                }
            }
            else
            {
                /* eFormat should take nvcolorformat type */
                if (NvxSurfaceAlloc(&pNvxVideoMixer->pDestinationSurface,
                                    pNvxVideoMixer->uDestinationWidth,
                                    pNvxVideoMixer->uDestinationHeight,
                                    NvColorFormat_A8B8G8R8,
                                    NvRmSurfaceLayout_Pitch) != NvSuccess)
                {
                    return OMX_ErrorInsufficientResources;
                }
            }
        }
    }
    else
    {
        //In case of EglImage, Allocate NvxSurface
        pNvxVideoMixer->pDestinationSurface = NvOsAlloc(sizeof(NvxSurface));
        if (!pNvxVideoMixer->pDestinationSurface)
        {
            return OMX_ErrorInsufficientResources;
        }
        NvOsMemset(pNvxVideoMixer->pDestinationSurface, 0, sizeof(NvxSurface));
    }

    pNvxVideoMixer->hMixer = TVMRVideoMixerCreate(pNvxVideoMixer->hDevice, pNvxVideoMixer->eSourceColorFormat,
                                                  pNvxVideoMixer->uSourceWidth, pNvxVideoMixer->uSourceHeight,
                                                  TVMR_VIDEO_MIXER_FEATURE_ADVANCE1_DEINTERLACING);

    if(!pNvxVideoMixer->hMixer) {
        return OMX_ErrorInsufficientResources;
    }

    TVMRVideoMixerAttributes oVmrAttrib;
    NvU32 attri_mask = 0;

    attri_mask |= TVMR_VIDEO_MIXER_ATTRIBUTE_Y_INVERT;
    if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
    {
        oVmrAttrib.yInvert = NV_TRUE;
    }
    else
    {
         oVmrAttrib.yInvert = NV_FALSE;
    }

    if (pNvxVideoMixer->eDeinterlaceType != OMX_DeintMethod_NoDeinterlacing)
    {
        attri_mask |= TVMR_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE;
        if (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_BobAtFieldRate)
        {
            oVmrAttrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_BOB;
        }
        else if (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_Advanced1AtFieldRate)
        {
            oVmrAttrib.deinterlaceType = TVMR_DEINTERLACE_TYPE_ADVANCE1;
        }
    }

    TVMRVideoMixerSetAttributes(pNvxVideoMixer->hMixer, attri_mask , &oVmrAttrib);

    pNvxVideoMixer->fenceDone = TVMRFenceCreate();

    pNvxVideoMixer->pPrev2VideoSurface = NULL;
    pNvxVideoMixer->pPrevVideoSurface  = NULL;
    pNvxVideoMixer->pNextVideoSurface  = NULL;
    pNvxVideoMixer->ePictureStructure = TVMR_PICTURE_STRUCTURE_FRAME;
    pNvxVideoMixer->bSecondField      = OMX_FALSE;
    pNvxVideoMixer->bInitialized = OMX_TRUE;

    /* convert from Q16 to float */
    pNvxVideoMixer->fFrameRate   = pInPort->oPortDef.format.video.xFramerate / (1 << 16);
    pNvxVideoMixer->sDisplaytimeStamp = (OMX_TICKS) INVALID_TIMESTAMP;
    return eError;
}

static
OMX_ERRORTYPE NvxVideoMixerReturnBuffers(OMX_IN NvxComponent *pComponent,
                                         OMX_U32 nPortIndex,
                                         OMX_BOOL bWorkerLocked)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    NvxPort *pInPort  = &pComponent->pPorts[s_nvx_port_input];
    OMX_BOOL bNvidiaTunneling;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoMixerReturnBuffers\n"));

    /* get videoMixer instance */
    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoMixer);

    if (!pNvxVideoMixer->bInitialized)
        return OMX_ErrorNone;

    bNvidiaTunneling = pInPort->bNvidiaTunneling && (pInPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);

    if (pNvxVideoMixer->eDeinterlaceType != OMX_DeintMethod_NoDeinterlacing &&
        nPortIndex == (OMX_U32) s_nvx_port_input)
    {
        if (pNvxVideoMixer->pCurrBufferHdr)
        {
            if (!bNvidiaTunneling)
            {
                NvxPortReleaseBuffer(pInPort, pNvxVideoMixer->pCurrBufferHdr);
            }
            else
            {
                OMX_EmptyThisBuffer(pComponent, pNvxVideoMixer->pCurrBufferHdr);
            }
            pNvxVideoMixer->pCurrBufferHdr = NULL;
        }

        if (pNvxVideoMixer->bSecondField)
        {
            if (pNvxVideoMixer->pPrevVideoSurface && pNvxVideoMixer->pPrevVideoSurface != pNvxVideoMixer->pCurrVideoSurface)
            {
                if (pNvxVideoMixer->pPrevBufferHdr)
                {
                    if (!bNvidiaTunneling)
                    {
                        NvxPortReleaseBuffer(pInPort, pNvxVideoMixer->pPrevBufferHdr);
                    }
                    else
                    {
                        OMX_EmptyThisBuffer(pComponent, pNvxVideoMixer->pPrevBufferHdr);
                    }
                    pNvxVideoMixer->pPrevBufferHdr = NULL;
                }
            }
        }
        else
        {
            if (pNvxVideoMixer->pNextVideoSurface &&  pNvxVideoMixer->pCurrVideoSurface != pNvxVideoMixer->pNextVideoSurface)
            {
                if (pNvxVideoMixer->pNextBufferHdr)
                {
                    if (!bNvidiaTunneling)
                    {
                        NvxPortReleaseBuffer(pInPort, pNvxVideoMixer->pNextBufferHdr);
                    }
                    else
                    {
                        OMX_EmptyThisBuffer(pComponent, pNvxVideoMixer->pNextBufferHdr);
                    }
                    pNvxVideoMixer->pNextBufferHdr = NULL;
                }
            }
        }
    }

    if (pNvxVideoMixer->pCurrentOutputSurface)
    {
        NvMMQueueEnQ(pNvxVideoMixer->pOutputSurfaceQueue, &pNvxVideoMixer->pCurrentOutputSurface, 2000);
    }

    pNvxVideoMixer->pCurrentOutputSurface = NULL;
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoMixerReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32  i;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoMixerReleaseResources\n"));

    /* get videoMixer instance */
    pNvxVideoMixer = (SNvxVideoMixerData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoMixer);

    if (!pNvxVideoMixer->bInitialized)
        return OMX_ErrorNone;

    if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_EGLIMAGE)
    {
        NvOsFree(pNvxVideoMixer->pDestinationSurface);
        pNvxVideoMixer->pDestinationSurface = NULL;
    }
    else
    {
        if (pNvxVideoMixer->pDestinationSurface)
        {
            NvxSurfaceFree(&pNvxVideoMixer->pDestinationSurface);
            pNvxVideoMixer->pDestinationSurface = NULL;
        }
    }

    if (pNvxVideoMixer->pCurrentSourceSurface)
    {
        NvxSurfaceFree(&pNvxVideoMixer->pCurrentSourceSurface);
        pNvxVideoMixer->pCurrentSourceSurface = NULL;
    }

    if (pNvxVideoMixer->pPrevSourceSurface)
    {
        NvxSurfaceFree(&pNvxVideoMixer->pPrevSourceSurface);
        pNvxVideoMixer->pPrevSourceSurface = NULL;
    }

    if (pNvxVideoMixer->pNextSourceSurface)
    {
        NvxSurfaceFree(&pNvxVideoMixer->pNextSourceSurface);
        pNvxVideoMixer->pNextSourceSurface = NULL;
    }

    if (pNvxVideoMixer->hMixer)
    {
        TVMRVideoMixerDestroy(pNvxVideoMixer->hMixer);
        pNvxVideoMixer->hMixer = NULL;
    }

    if (pNvxVideoMixer->pOutputSurfaceQueue)
    {
        for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
        {
            NvxSurfaceFree(&pNvxVideoMixer->ppSurfaceBuffers[i]);
            pNvxVideoMixer->ppSurfaceBuffers[i] = NULL;
        }

        NvOsFree(pNvxVideoMixer->ppSurfaceBuffers);

        NvMMQueueDestroy(&pNvxVideoMixer->pOutputSurfaceQueue);
        pNvxVideoMixer->pOutputSurfaceQueue = NULL;
    }

    TVMRFenceDestroy(pNvxVideoMixer->fenceDone);

    if (pNvxVideoMixer->hDevice)
    {
        TVMRDeviceDestroy(pNvxVideoMixer->hDevice);
        pNvxVideoMixer->hDevice = NULL;
    }

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    pNvxVideoMixer->bInitialized = OMX_FALSE;
    return eError;
}

static
OMX_ERRORTYPE NvxVideoMixerFlush(OMX_IN NvxComponent *pNvComp,
                                 OMX_U32 nPort)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoMixerReleaseResources\n"));

    /* get videoMixer instance */
    pNvxVideoMixer = (SNvxVideoMixerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxVideoMixer);

    if (!pNvxVideoMixer->bInitialized)
        return OMX_ErrorNone;

    if (nPort == (OMX_U32)s_nvx_port_input)
    {

        //These are needed when input buffer type is not MPEG2VLD
        pNvxVideoMixer->pPrev2BufferHdr = NULL;
        pNvxVideoMixer->pPrevBufferHdr  = NULL;
        pNvxVideoMixer->pCurrBufferHdr  = NULL;
        pNvxVideoMixer->pNextBufferHdr  = NULL;
        pNvxVideoMixer->pReleaseBufferHdr = NULL;

        //Flush the Video Surface Queue and Enqueue all Surfaces again.
        if (pNvxVideoMixer->eDeinterlaceType == OMX_DeintMethod_NoDeinterlacing)
        {
            pNvxVideoMixer->pPrev2VideoSurface = NULL;
            pNvxVideoMixer->pPrevVideoSurface  = NULL;
            pNvxVideoMixer->pCurrVideoSurface  = NULL;
            pNvxVideoMixer->pNextVideoSurface  = NULL;

            if (pNvxVideoMixer->pReleaseVideoSurface)
            {
                FreeTVMRVideoSurface(pNvxVideoMixer->pReleaseVideoSurface);
                pNvxVideoMixer->pReleaseVideoSurface = NULL;
            }
        }
        else
        {
            if (pNvxVideoMixer->bSecondField)
            {
                if (pNvxVideoMixer->pPrevVideoSurface && pNvxVideoMixer->pPrevVideoSurface != pNvxVideoMixer->pCurrVideoSurface)
                {
                    FreeTVMRVideoSurface(pNvxVideoMixer->pPrevVideoSurface);
                    pNvxVideoMixer->pPrevVideoSurface  = NULL;
                }
            }
            else
            {
                if (pNvxVideoMixer->pNextVideoSurface &&  pNvxVideoMixer->pCurrVideoSurface != pNvxVideoMixer->pNextVideoSurface)
                {
                    FreeTVMRVideoSurface(pNvxVideoMixer->pNextVideoSurface);
                    pNvxVideoMixer->pNextVideoSurface  = NULL;
                }
            }

            if (pNvxVideoMixer->pCurrVideoSurface)
            {
                FreeTVMRVideoSurface(pNvxVideoMixer->pCurrVideoSurface);
                pNvxVideoMixer->pCurrVideoSurface  = NULL;
            }

            pNvxVideoMixer->pPrev2VideoSurface = NULL;
            pNvxVideoMixer->pReleaseVideoSurface = NULL;

            pNvxVideoMixer->bSecondField = OMX_FALSE;
        }

    }
    else
    {
        //Nothing to do for Output. This is handled Outside this call.
    }

    pNvxVideoMixer->sDisplaytimeStamp = (OMX_TICKS) INVALID_TIMESTAMP;

    if (pNvxVideoMixer->pCurrentOutputSurface)
    {
        NvMMQueueEnQ(pNvxVideoMixer->pOutputSurfaceQueue, &pNvxVideoMixer->pCurrentOutputSurface, 2000);
    }

    pNvxVideoMixer->pCurrentOutputSurface = NULL;
    //For now nothing to be done.
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoMixerFillThisBuffer(NvxComponent *pNvComp,
                                          OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    NvMMBuffer *pMMBuffer = NULL;
    NvxSurface *pOutputSurface = NULL;

    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoMixerFillThisBuffer\n"));

    pNvxVideoMixer = (SNvxVideoMixerData*) pNvComp->pComponentData;

    if (pNvxVideoMixer->eBufferType[s_nvx_port_output] == BUFFER_TYPE_NVMM)
    {
        OMX_BOOL bSurfaceFound = OMX_FALSE;

        if(pNvxVideoMixer->bEmbedRmSurface)
        {
            pOutputSurface = (NvxSurface*) pBufferHdr->pInputPortPrivate;
        }
        else
        {
            pMMBuffer  = (NvMMBuffer*) pBufferHdr->pBuffer;
            pOutputSurface = pMMBuffer->pCore;
        }

        if (pOutputSurface)
        {
            int i = 0;
            //match with any of available output surfaces.
            for (i = 0; i < MAX_OUTPUT_SURFACES; i++)
            {
                if (pOutputSurface == pNvxVideoMixer->ppSurfaceBuffers[i])
                {
                    bSurfaceFound = OMX_TRUE;
                    break;
                }
            }

            if (bSurfaceFound)
            {
                NvMMQueueEnQ(pNvxVideoMixer->pOutputSurfaceQueue, &pNvxVideoMixer->ppSurfaceBuffers[i], 2000);
            }
            else
            {
                return OMX_ErrorResourcesLost;
            }
        }
    }

    NvOsMemset(pBufferHdr->pBuffer, 0, pBufferHdr->nAllocLen);

    NvxWorkerTrigger(&(pNvComp->oWorkerData));

    //For now nothing to be done.
    return OMX_ErrorNone;
}

static
OMX_ERRORTYPE NvxVideoMixerCanUseEglImage(OMX_IN NvxComponent
                                          *pNvComp,
                                          OMX_IN OMX_U32 nPortIndex,
                                          OMX_IN void* eglImage)
{
    SNvxVideoMixerData *pNvxVideoMixer = 0;
    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoMixerFillThisBuffer\n"));
    pNvxVideoMixer = (SNvxVideoMixerData *)pNvComp->pComponentData;
    pNvxVideoMixer->bUseEglImage[nPortIndex] = OMX_TRUE;

    return OMX_ErrorNone;
}

/*****************************************************************************/

OMX_ERRORTYPE NvxVideoMixerInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxVideoMixerData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_VIDEO_MIXER,"ERROR:NvxVideoMixerInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_VIDEO_MIXER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVideoMixerData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxVideoMixerData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxVideoMixerData));

    pNvComp->DeInit             = NvxVideoMixerDeInit;
    pNvComp->GetParameter       = NvxVideoMixerGetParameter;
    pNvComp->SetParameter       = NvxVideoMixerSetParameter;
    pNvComp->ReturnBuffers      = NvxVideoMixerReturnBuffers;
    pNvComp->FillThisBufferCB   = NvxVideoMixerFillThisBuffer;
    pNvComp->Flush              = NvxVideoMixerFlush;
    pNvComp->GetConfig          = NvxVideoMixerGetConfig;
    pNvComp->SetConfig          = NvxVideoMixerSetConfig;
    pNvComp->WorkerFunction     = NvxVideoMixerWorkerFunction;
    pNvComp->AcquireResources   = NvxVideoMixerAcquireResources;
    pNvComp->ReleaseResources   = NvxVideoMixerReleaseResources;
    pNvComp->CanUseEGLImage     = NvxVideoMixerCanUseEglImage;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MIN_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingAutoDetect);
    pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_Format32bitARGB8888;

    // TO DO: Fix with Other Tunnel Modes
    //pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_DEFAULT;
    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    pNvComp->pComponentName = "OMX.Nvidia.video.mixer";
    pNvComp->sComponentRoles[0] = "video_mixer.generic";
    pNvComp->nComponentRoles    = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MIN_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingAutoDetect);

    pNvComp->pPorts[s_nvx_port_input].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    data->eBufferType[s_nvx_port_output] = BUFFER_TYPE_RAW;
    data->eBufferType[s_nvx_port_input]  = BUFFER_TYPE_RAW;

    data->eInputPortDataType = INPUT_PORT_DATA_TYPE_OTHER;

    return eError;
}

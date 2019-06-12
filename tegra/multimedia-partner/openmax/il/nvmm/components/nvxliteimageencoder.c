/*
 * Copyright (c) 2006 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxImageEncoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "nvmm/components/common/nvmmlitetransformbase.h"
#include "nvmmlite_imageenc.h"
#include "nvassert.h"
#include "nvmm_camera.h"
#include "nvmm_exif.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

#define MAX_INPUT_BUFFERS    5
#define MAX_OUTPUT_BUFFERS   5
#define MIN_INPUT_BUFFERSIZE 1024*1024
#define MIN_OUTPUT_BUFFERSIZE 1024*1024*9
#define ENABLE_TUNNELLING       0

/* Number of internal frame buffers (video memory) */
static const int s_nvx_num_ports            = 3;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_output          = 1;
static const int s_nvx_port_thumbnail       = 2;


/* encoder state information */
typedef struct SNvxLiteImageEncoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;
    OMX_BOOL bThumbnailQF;
    OMX_U32  nSurfWidth;
    OMX_U32  nSurfHeight;

    OMX_U32  nThumbnailWidth;
    OMX_U32  nThumbnailHeight;

    SNvxNvMMLiteTransformData oBase;

    OMX_U32 nQFactor;
    OMX_U32 nThumbnailQFactor;
    OMX_U8  bSetCustomQTables_Luma;
    OMX_U8  bSetCustomQTables_Chroma;
    OMX_U8  nQTable_Luma[64];
    OMX_U8  nQTable_Chroma[64];
    OMX_U8  bSetCustomThumbQTables_Luma;
    OMX_U8  bSetCustomThumbQTables_Chroma;
    OMX_U8  nThumbQTable_Luma[64];
    OMX_U8  nThumbQTable_Chroma[64];
#define TYPE_JPEG 0
#define TYPE_REF  1
    int oType;
    OMX_U32 nEncodedSize;
    OMX_BOOL bThumbnailPending;
    NvMMLiteBlockType oBlockType;
    const char *sBlockName;
} SNvxLiteImageEncoderData;


static NvError s_SetExifString(SNvxNvMMLiteTransformData *pBase, const char * string, NvMMLiteImageEncAttribute index);
static NvError s_SetExifString(SNvxNvMMLiteTransformData *pBase, const char * string, NvMMLiteImageEncAttribute index)
{
    NvMMLiteJpegEncAttributeLevel oAttrLevel;

    NvOsMemset(&oAttrLevel, 0, sizeof(oAttrLevel));
    oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);

    NvOsStrncpy(oAttrLevel.AttributeLevel_Exifstring,
                string,
                NvOsStrlen(string) + 1);

    return pBase->hBlock->SetAttribute(pBase->hBlock,
                                  index,
                                  0,
                                  sizeof(NvMMLiteJpegEncAttributeLevel),
                                  &oAttrLevel);
}


static OMX_ERRORTYPE NvxImageEncoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageEncoderInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

typedef OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *QTable;

static OMX_ERRORTYPE NvxIEGetThumbQuantTable(OMX_IN NvxComponent *pComponent,
                                             QTable table)
{
    SNvxLiteImageEncoderData *coder =
        (SNvxLiteImageEncoderData *) pComponent->pComponentData;

    if (!table)
        return OMX_ErrorBadParameter;

    if (coder->bSetCustomThumbQTables_Luma &&
        table->eQuantizationTable == OMX_IMAGE_QuantizationTableLuma)
        NvOsMemcpy(table->nQuantizationMatrix, coder->nThumbQTable_Luma, 64);
    else if (coder->bSetCustomThumbQTables_Chroma &&
             table->eQuantizationTable == OMX_IMAGE_QuantizationTableChroma)
        NvOsMemcpy(table->nQuantizationMatrix, coder->nThumbQTable_Chroma, 64);
    else
        return OMX_ErrorBadParameter;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxIEGetQuantTable(OMX_IN NvxComponent *pComponent,
                                        QTable table)
{
    SNvxLiteImageEncoderData *coder =
        (SNvxLiteImageEncoderData *) pComponent->pComponentData;

    if (!table)
        return OMX_ErrorBadParameter;

    if (coder->bSetCustomQTables_Luma &&
        table->eQuantizationTable == OMX_IMAGE_QuantizationTableLuma)
        NvOsMemcpy(table->nQuantizationMatrix, coder->nQTable_Luma, 64);
    else if (coder->bSetCustomQTables_Chroma &&
             table->eQuantizationTable == OMX_IMAGE_QuantizationTableChroma)
        NvOsMemcpy(table->nQuantizationMatrix, coder->nQTable_Chroma, 64);
    else return OMX_ErrorBadParameter;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxIESetThumbQuantTable(OMX_IN NvxComponent *pComponent,
                                             QTable table)
{
    SNvxLiteImageEncoderData *coder =
        (SNvxLiteImageEncoderData *) pComponent->pComponentData;

    if (!table)
        return OMX_ErrorBadParameter;

    switch (table->eQuantizationTable)
    {
        case OMX_IMAGE_QuantizationTableLuma:
            NvOsMemcpy(coder->nThumbQTable_Luma, table->nQuantizationMatrix, 64);
            coder->bSetCustomThumbQTables_Luma = NV_TRUE;
            break;
        case OMX_IMAGE_QuantizationTableChroma:
        case OMX_IMAGE_QuantizationTableChromaCb:
        case OMX_IMAGE_QuantizationTableChromaCr:
            NvOsMemcpy(coder->nThumbQTable_Chroma, table->nQuantizationMatrix, 64);
            coder->bSetCustomThumbQTables_Chroma = NV_TRUE;
            break;
        default:
            return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxIESetQuantTable(OMX_IN NvxComponent *pComponent,
                                        QTable table)
{
    SNvxLiteImageEncoderData *coder =
        (SNvxLiteImageEncoderData *) pComponent->pComponentData;

    if (!table)
        return OMX_ErrorBadParameter;

    switch (table->eQuantizationTable) {
      case OMX_IMAGE_QuantizationTableLuma:
        NvOsMemcpy(coder->nQTable_Luma, table->nQuantizationMatrix, 64);
        coder->bSetCustomQTables_Luma = NV_TRUE;
        break;
      case OMX_IMAGE_QuantizationTableChroma:
      case OMX_IMAGE_QuantizationTableChromaCb:
      case OMX_IMAGE_QuantizationTableChromaCr:
        NvOsMemcpy(coder->nQTable_Chroma, table->nQuantizationMatrix, 64);
        coder->bSetCustomQTables_Chroma = NV_TRUE;
        break;
      default:
        return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

static void NvxIEUpdateQTables(SNvxLiteImageEncoderData *encoder,
                               SNvxNvMMLiteTransformData *pBase)
{
    NvMMLiteJpegEncCustomQuantTables QTables;
    NvError err;

    if (encoder->bSetCustomQTables_Luma) {
        NvOsMemset(&QTables, 0, sizeof(NvMMLiteJpegEncCustomQuantTables));
        QTables.StructSize = sizeof(NvMMLiteJpegEncCustomQuantTables);
        QTables.QTableType = NVMMLITE_JPEG_ENC_QuantizationTable_Luma;

        NvOsMemcpy(QTables.QuantTable, encoder->nQTable_Luma, 64);

        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                          NvMMLiteAttributeImageEnc_CustomQuantTables,
                                          0,
                                          sizeof(NvMMLiteJpegEncCustomQuantTables),
                                          &QTables);
        if (err != NvSuccess)
            NV_DEBUG_PRINTF(("JPE error: unable to set qtable_Luma\n"));
    }

    if (encoder->bSetCustomQTables_Chroma) {
        NvOsMemset(&QTables, 0, sizeof(NvMMLiteJpegEncCustomQuantTables));
        QTables.StructSize = sizeof(NvMMLiteJpegEncCustomQuantTables);
        QTables.QTableType = NVMMLITE_JPEG_ENC_QuantizationTable_Chroma;

        NvOsMemcpy(QTables.QuantTable, encoder->nQTable_Chroma, 64);

        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                          NvMMLiteAttributeImageEnc_CustomQuantTables,
                                          0,
                                          sizeof(NvMMLiteJpegEncCustomQuantTables),
                                          &QTables);
        if (err != NvSuccess)
            NV_DEBUG_PRINTF(("JPE error: unable to set qtable_Chroma\n"));
    }

    if (encoder->bSetCustomThumbQTables_Luma) {
        NvOsMemset(&QTables, 0, sizeof(NvMMLiteJpegEncCustomQuantTables));
        QTables.StructSize = sizeof(NvMMLiteJpegEncCustomQuantTables);
        QTables.QTableType = NVMMLITE_JPEG_ENC_QuantizationTable_Luma;

        NvOsMemcpy(QTables.QuantTable, encoder->nThumbQTable_Luma, 64);

        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                          NvMMLiteAttributeImageEnc_ThumbCustomQuantTables,
                                          0,
                                          sizeof(NvMMLiteJpegEncCustomQuantTables),
                                          &QTables);
        if (err != NvSuccess)
            NV_DEBUG_PRINTF(("JPE error: unable to set thumb qtable_Luma\n"));
    }

    if (encoder->bSetCustomThumbQTables_Chroma) {
        NvOsMemset(&QTables, 0, sizeof(NvMMLiteJpegEncCustomQuantTables));
        QTables.StructSize = sizeof(NvMMLiteJpegEncCustomQuantTables);
        QTables.QTableType = NVMMLITE_JPEG_ENC_QuantizationTable_Chroma;

        NvOsMemcpy(QTables.QuantTable, encoder->nThumbQTable_Chroma, 64);

        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                          NvMMLiteAttributeImageEnc_ThumbCustomQuantTables,
                                          0,
                                          sizeof(NvMMLiteJpegEncCustomQuantTables),
                                          &QTables);
        if (err != NvSuccess)
            NV_DEBUG_PRINTF(("JPE error: unable to set thumb qtable_Chroma\n"));
    }
}

static OMX_ERRORTYPE NvxImageEncoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxLiteImageEncoderData *pNvxImageEncoder;

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageEncoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageEncoderGetParameter\n"));

    switch (nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        {
            OMX_ERRORTYPE eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            if((NvU32)s_nvx_port_output == pPortDef->nPortIndex)
            {
                if (pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].bHasSize)
                {
                    pPortDef->format.image.nFrameWidth = pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    pPortDef->format.image.nFrameHeight = pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].nHeight;
                    pPortDef->format.image.nStride = pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].nWidth;
                }
            }
            else
            {
                return eError;
            }
        }
        break;
    case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor = (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;
            if (!pQFactor)
                return OMX_ErrorBadParameter;
            pQFactor->nQFactor = pNvxImageEncoder->nQFactor;
        }
        break;
    case NVX_IndexConfigThumbnailQF:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor =
                (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;

            if (!pQFactor)
                return OMX_ErrorBadParameter;
            pQFactor->nQFactor = pNvxImageEncoder->nThumbnailQFactor;
        }
        break;
    case OMX_IndexParamQuantizationTable:
        return NvxIEGetQuantTable(pComponent,
                                  (QTable) pComponentParameterStructure);

    case NVX_IndexParamThumbnailQTable:
        return NvxIEGetThumbQuantTable(pComponent,
                                       (QTable)pComponentParameterStructure);
    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxImageEncoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder;
    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageEncoderSetParameter\n"));

    NV_ASSERT(pNvxImageEncoder);

    switch(nIndex)
    {
    case OMX_IndexParamPortDefinition:
        {
            NvMMLiteJpegEncAttributeLevel AttrLevel;
            NvError Err = NvSuccess;
            OMX_PARAM_PORTDEFINITIONTYPE *pPortDef =
                (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
            SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;
            OMX_U32 portWidth = pPortDef->format.image.nFrameWidth;
            OMX_U32 portHeight = pPortDef->format.image.nFrameHeight;

            NvOsMemset(&AttrLevel, 0, sizeof(NvMMLiteJpegEncAttributeLevel));

            if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_input)
            {
                pNvxImageEncoder->nSurfWidth = portWidth;
                pNvxImageEncoder->nSurfHeight = portHeight;
                pBase->oPorts[s_nvx_port_input].nWidth = portWidth;
                pBase->oPorts[s_nvx_port_input].nHeight = portHeight;
                AttrLevel.AttributeLevel_ThumbNailFlag = NV_FALSE;
                AttrLevel.AttributeLevel1 = portWidth;
                AttrLevel.AttributeLevel2 = portHeight;

            }
            else if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_thumbnail)
            {
                pNvxImageEncoder->nThumbnailWidth = portWidth;
                pNvxImageEncoder->nThumbnailHeight = portHeight;
                pBase->oPorts[s_nvx_port_thumbnail].nWidth = portWidth;
                pBase->oPorts[s_nvx_port_thumbnail].nHeight = portHeight;
                AttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
                AttrLevel.AttributeLevel1 = portWidth;
                AttrLevel.AttributeLevel2 = portHeight;
            }

            if (pBase->hBlock)
            {
                NV_ASSERT(pPortDef->nPortIndex == (OMX_U32)s_nvx_port_input ||
                          pPortDef->nPortIndex == (OMX_U32)s_nvx_port_thumbnail);

                // change resolution in nvmm jpeg block
                Err = pBase->hBlock->SetAttribute(
                        pBase->hBlock,
                        NvMMLiteAttributeImageEnc_resolution,
                        0,
                        sizeof(NvMMLiteJpegEncAttributeLevel),
                        &AttrLevel);
                if (Err != NvSuccess)
                    return OMX_ErrorHardware;
            }
            return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
        }
    case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor = (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;
            if (!pQFactor)
                return OMX_ErrorBadParameter;
            if ( (pQFactor->nQFactor < 1) || (pQFactor->nQFactor > 100))
                return OMX_ErrorBadParameter;
            pNvxImageEncoder->nQFactor = pQFactor->nQFactor;
        }
        break;
     case NVX_IndexConfigThumbnailQF:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor =
                (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;

            if (!pQFactor)
                return OMX_ErrorBadParameter;
            if ( (pQFactor->nQFactor < 1) || (pQFactor->nQFactor > 100))
                return OMX_ErrorBadParameter;
            pNvxImageEncoder->nThumbnailQFactor = pQFactor->nQFactor;
            pNvxImageEncoder->bThumbnailQF = OMX_TRUE;
        }
        break;
    case OMX_IndexParamQuantizationTable:
        return NvxIESetQuantTable(pComponent,
                                  (QTable) pComponentParameterStructure);
    case NVX_IndexParamThumbnailQTable:
        return NvxIESetThumbQuantTable(pComponent,
                                       (QTable) pComponentParameterStructure);
    case NVX_IndexConfigUseNvBuffer:
        {   int jencPort;

            NVX_PARAM_USENVBUFFER *pParam =
                      (NVX_PARAM_USENVBUFFER *)pComponentParameterStructure;

            jencPort = (int)pParam->nPortIndex;
            if (jencPort != s_nvx_port_input &&
                jencPort != s_nvx_port_output &&
                jencPort != s_nvx_port_thumbnail)
            {
                return OMX_ErrorUndefined;
            }

            pNvxImageEncoder->oBase.oPorts[jencPort].bEmbedNvMMBuffer =
                              pParam->bUseNvBuffer;
            break;
        }
    case NVX_IndexParamEmbedRmSurface:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxImageEncoder->oBase.bEmbedRmSurface = param->bEnabled;
        }
    break;
    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxImageEncoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxLiteImageEncoderData *pNvxImageEncoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageEncoderGetConfig\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pTimestamp->bOverrideTimestamp = pNvxImageEncoder->bOverrideTimestamp;
        }
        break;
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pBlockReq = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!pNvxImageEncoder || !pNvxImageEncoder->bInitialized)
                return OMX_ErrorNotReady;

            pBlockReq->pNvMMTransform = &pNvxImageEncoder->oBase;
            pBlockReq->nNvMMPort = pBlockReq->nPortIndex; // nvmm and OMX port indexes match
        }
        break;

        case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor = (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentConfigStructure;
            if (!pQFactor)
                return OMX_ErrorBadParameter;
            pQFactor->nQFactor = pNvxImageEncoder->nQFactor;
        }
        break;
        case NVX_IndexConfigThumbnailQF:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor =
                (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentConfigStructure;
            if (!pQFactor)
                return OMX_ErrorBadParameter;
            pQFactor->nQFactor = pNvxImageEncoder->nThumbnailQFactor;
        }
        break;
        case OMX_IndexParamQuantizationTable:
          return NvxIEGetQuantTable(pNvComp,
                                    (QTable) pComponentConfigStructure);

        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxImageEncoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    NvError err;
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxLiteImageEncoderData *pNvxImageEncoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageEncoderSetConfig\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pNvxImageEncoder->bOverrideTimestamp = pTimestamp->bOverrideTimestamp;
            break;
        }
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMLiteTransformSetProfile(&pNvxImageEncoder->oBase, pProf);
            break;
        }
        case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor = (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentConfigStructure;

            if (!pQFactor)
                return OMX_ErrorBadParameter;
            if ( (pQFactor->nQFactor < 1) || (pQFactor->nQFactor > 100))
                return OMX_ErrorBadParameter;
            pNvxImageEncoder->nQFactor = pQFactor->nQFactor;

            if (pNvxImageEncoder->bInitialized)
            {
                NvMMLiteJpegEncAttributeLevel oAttrLevel;
                NvError err = NvSuccess;
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;

                oAttrLevel.AttributeLevel_ThumbNailFlag = NV_FALSE;
                oAttrLevel.AttributeLevel1 = pNvxImageEncoder->nQFactor;
                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_Quality,
                                                  0,
                                                  sizeof(NvMMLiteJpegEncAttributeLevel),
                                                  &oAttrLevel);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("JPE error: unable to set qfactor\n"));
                }
            }

            break;
        }

        case OMX_IndexConfigVideoNalSize: // borrow OMX_IndexConfigVideoNalSize here for output encoded size
        {
            OMX_VIDEO_CONFIG_NALSIZE *pEncodedSize = (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
            pNvxImageEncoder->nEncodedSize = pEncodedSize->nNaluBytes;
            break;
        }

        case OMX_IndexConfigCommonRotate:
        {
            NvMMExif_Orientation orientation;
            NvMMLiteImageOrientation sImageOrientation;

            OMX_CONFIG_ROTATIONTYPE *pRotation =
                (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;

            if (pNvxImageEncoder->bInitialized)
            {
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;
                NvError err = NvError_Success;

                sImageOrientation.eRotation = NvMMLiteImageRotatedPhysically;
                switch(pRotation->nRotation)
                {
                    case 90:
                        orientation = NvMMExif_Orientation_270_Degrees;
                        break;
                    case 180:
                        orientation = NvMMExif_Orientation_180_Degrees;
                        break;
                    case 270:
                        orientation = NvMMExif_Orientation_90_Degrees;
                        break;
                    case 0:
                    default:
                        orientation = NvMMExif_Orientation_0_Degrees;
                }

                sImageOrientation.orientation = orientation;
                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                    NvMMLiteAttributeImageEnc_Orientation,
                    0,
                    sizeof(NvMMLiteImageOrientation),
                    &sImageOrientation);
                if (NvSuccess != err)
                {
                    NV_ASSERT("!SetAttribute for Image Orientation failed");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        }

        case NVX_IndexConfigEncodeExifInfo:
        {
            NVX_CONFIG_ENCODEEXIFINFO *pExifInfo = (NVX_CONFIG_ENCODEEXIFINFO *)pComponentConfigStructure;

            if (pNvxImageEncoder->bInitialized)
            {
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;
                NvError err = NvError_Success;
                NvMMLiteJpegEncAttributeLevel oAttrLevel;
                NvOsMemset(&oAttrLevel, 0, sizeof(NvMMLiteJpegEncAttributeLevel));

                err = s_SetExifString(pBase, (const char *)pExifInfo->ImageDescription, NvMMLiteAttributeImageEnc_ImageDescrpt);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for ImageDescription failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->Make, NvMMLiteAttributeImageEnc_Make);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image Make failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->Model, NvMMLiteAttributeImageEnc_Model);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image Model failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->Copyright, NvMMLiteAttributeImageEnc_Copyright);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image Copyright failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->Artist, NvMMLiteAttributeImageEnc_Artist);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image Artist failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->Software, NvMMLiteAttributeImageEnc_Software);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image Software failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->DateTime, NvMMLiteAttributeImageEnc_DateTime);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image DateTime failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->DateTimeOriginal, NvMMLiteAttributeImageEnc_DateTime_Original);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image DateTimeOriginal failed");
                    return OMX_ErrorBadParameter;
                }
                err = s_SetExifString(pBase, (const char *)pExifInfo->DateTimeDigitized, NvMMLiteAttributeImageEnc_DateTime_Digitized);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image DateTimeDigitized failed");
                    return OMX_ErrorBadParameter;
                }
                {
                    NvMMLiteJpegEncAttributeLevel oAttrLevel;
                    NvOsMemset(&oAttrLevel, 0, sizeof(NvMMLiteJpegEncAttributeLevel));
                    oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);

                    NvOsMemcpy(oAttrLevel.AttributeLevel_Exifstring, pExifInfo->UserComment,
                        sizeof(pExifInfo->UserComment));

                    err = pBase->hBlock->SetAttribute(pBase->hBlock,
                        NvMMLiteAttributeImageEnc_UserComment,
                        0,
                        sizeof(NvMMLiteJpegEncAttributeLevel),
                        &oAttrLevel);
                    if (NvSuccess != err)
                    {
                        NV_ASSERT("!SetAttribute for Image UserComment failed");
                        return OMX_ErrorBadParameter;
                    }
                }
                {
                    NvMMLiteImageOrientation sImageOrientation;
                    NvMMExif_Orientation orientation;

                    sImageOrientation.eRotation = NvMMLiteImageRotatedByExifTag;

                    switch(pExifInfo->Orientation)
                    {
                        case 90:
                            orientation = NvMMExif_Orientation_270_Degrees;
                            break;
                        case 180:
                            orientation = NvMMExif_Orientation_180_Degrees;
                            break;
                        case 270:
                            orientation = NvMMExif_Orientation_90_Degrees;
                            break;
                        case 0:
                        default:
                            orientation = NvMMExif_Orientation_0_Degrees;
                            sImageOrientation.eRotation = NvMMLiteImageRotatedPhysically;
                    }

                    if(pExifInfo->Orientation != 0)
                    {
                        sImageOrientation.orientation = orientation;

                        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                            NvMMLiteAttributeImageEnc_Orientation,
                            0,
                            sizeof(NvMMLiteImageOrientation),
                            &sImageOrientation);
                        if (NvSuccess != err)
                        {
                            NV_ASSERT("!SetAttribute for Image Orientation failed");
                            return OMX_ErrorBadParameter;
                        }
                    }
                }

                oAttrLevel.AttributeLevel1 = (NvU32)pExifInfo->filesource;
                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_FileSource,
                                                  0,
                                                  sizeof(NvMMLiteJpegEncAttributeLevel),
                                                  &oAttrLevel);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for Image filesource failed");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        }

        case NVX_IndexConfigEncodeGPSInfo:
        {
            NVX_CONFIG_ENCODEGPSINFO *pGpsInfo = (NVX_CONFIG_ENCODEGPSINFO *)pComponentConfigStructure;

            if (pNvxImageEncoder->bInitialized)
            {
                NvError err = NvSuccess;
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;
                NvMMGPSInfo oNvMMGpsInfo;
                NvMMGPSProcMethod oNvMMGPSProcMethod = {{0}};

                NvOsMemset(&oNvMMGpsInfo, 0, sizeof(NvMMGPSInfo));
                oNvMMGpsInfo.GPSBitMapInfo = pGpsInfo->GPSBitMapInfo;
                oNvMMGpsInfo.GPSVersionID = pGpsInfo->GPSVersionID;
                NvOsMemcpy(oNvMMGpsInfo.GPSLatitudeRef, pGpsInfo->GPSLatitudeRef, sizeof(oNvMMGpsInfo.GPSLatitudeRef));
                NvOsMemcpy(oNvMMGpsInfo.GPSLatitudeNumerator, pGpsInfo->GPSLatitudeNumerator,
                           sizeof(oNvMMGpsInfo.GPSLatitudeNumerator));
                NvOsMemcpy(oNvMMGpsInfo.GPSLatitudeDenominator, pGpsInfo->GPSLatitudeDenominator,
                           sizeof(oNvMMGpsInfo.GPSLatitudeDenominator));
                NvOsMemcpy(oNvMMGpsInfo.GPSLongitudeRef, pGpsInfo->GPSLongitudeRef,
                           sizeof(oNvMMGpsInfo.GPSLongitudeRef));
                NvOsMemcpy(oNvMMGpsInfo.GPSLongitudeNumerator, pGpsInfo->GPSLongitudeNumerator,
                           sizeof(oNvMMGpsInfo.GPSLongitudeNumerator));
                NvOsMemcpy(oNvMMGpsInfo.GPSLongitudeDenominator, pGpsInfo->GPSLongitudeDenominator,
                           sizeof(oNvMMGpsInfo.GPSLongitudeDenominator));
                oNvMMGpsInfo.GPSAltitudeRef = pGpsInfo->GPSAltitudeRef;
                oNvMMGpsInfo.GPSAltitudeNumerator = pGpsInfo->GPSAltitudeNumerator;
                oNvMMGpsInfo.GPSAltitudeDenominator = pGpsInfo->GPSAltitudeDenominator;
                NvOsMemcpy(oNvMMGpsInfo.GPSTimeStampNumerator, pGpsInfo->GPSTimeStampNumerator,
                           sizeof(oNvMMGpsInfo.GPSTimeStampNumerator));
                NvOsMemcpy(oNvMMGpsInfo.GPSTimeStampDenominator, pGpsInfo->GPSTimeStampDenominator,
                           sizeof(oNvMMGpsInfo.GPSTimeStampDenominator));
                NvOsMemcpy(oNvMMGpsInfo.GPSSatellites, pGpsInfo->GPSSatellites,
                           sizeof(oNvMMGpsInfo.GPSSatellites));
                NvOsMemcpy(oNvMMGpsInfo.GPSStatus, pGpsInfo->GPSStatus, sizeof(oNvMMGpsInfo.GPSStatus));
                NvOsMemcpy(oNvMMGpsInfo.GPSMeasureMode, pGpsInfo->GPSMeasureMode,
                           sizeof(oNvMMGpsInfo.GPSMeasureMode));
                oNvMMGpsInfo.GPSDOPNumerator = pGpsInfo->GPSDOPNumerator;
                oNvMMGpsInfo.GPSDOPDenominator = pGpsInfo->GPSDOPDenominator;
                NvOsMemcpy(oNvMMGpsInfo.GPSMapDatum, pGpsInfo->GPSMapDatum, sizeof(oNvMMGpsInfo.GPSMapDatum));
                NvOsMemcpy(oNvMMGpsInfo.GPSDateStamp, pGpsInfo->GPSDateStamp, sizeof(oNvMMGpsInfo.GPSDateStamp));

                NvOsMemcpy(oNvMMGpsInfo.GPSImgDirectionRef, pGpsInfo->GPSImgDirectionRef,
                    sizeof(oNvMMGpsInfo.GPSImgDirectionRef));
                oNvMMGpsInfo.GPSImgDirectionNumerator = pGpsInfo->GPSImgDirectionNumerator;
                oNvMMGpsInfo.GPSImgDirectionDenominator = pGpsInfo->GPSImgDirectionDenominator;

                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_GPSInfo,
                                                  0,
                                                  sizeof(NvMMGPSInfo),
                                                  &oNvMMGpsInfo);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for GPS info failed");
                    return OMX_ErrorBadParameter;
                }

                NvOsMemcpy(oNvMMGPSProcMethod.GPSProcessingMethod, pGpsInfo->GPSProcessingMethod,
                    MAX_GPS_PROCESSING_METHOD_IN_BYTES);

                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_GPSInfo_ProcMethod,
                                                  0,
                                                  sizeof(NvMMGPSProcMethod),
                                                  &oNvMMGPSProcMethod);
                if (NvSuccess != err)
                {
                    NV_ASSERT(!"SetAttribute for GPSProc Method failed");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        }
        case NVX_IndexConfigEncodeInterOpInfo:
        {
            NVX_CONFIG_ENCODE_INTEROPINFO *pInterOpInfo = (NVX_CONFIG_ENCODE_INTEROPINFO *)pComponentConfigStructure;

            if (pNvxImageEncoder->bInitialized) {
                NvMMInterOperabilityInfo InterOpInfo;
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;

                NvOsStrncpy(InterOpInfo.Index,(const char *)pInterOpInfo->Index,
                    MAX_INTEROP_STRING_IN_BYTES);
                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_InterOperabilityInfo,
                                                  0,
                                                  sizeof(NvMMInterOperabilityInfo),
                                                  &InterOpInfo);
                if (NvSuccess != err) {
                    NV_ASSERT(!"SetAttribute for InterOp info failed");
                    return OMX_ErrorBadParameter;
                }
            }
            break;
        }
        case NVX_IndexConfigThumbnailQF:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor =
                (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentConfigStructure;

            if (!pQFactor)
                return OMX_ErrorBadParameter;
            if ( (pQFactor->nQFactor < 1) || (pQFactor->nQFactor > 100))
                return OMX_ErrorBadParameter;
            pNvxImageEncoder->nThumbnailQFactor = pQFactor->nQFactor;
            pNvxImageEncoder->bThumbnailQF = OMX_TRUE;

            if (pNvxImageEncoder->bInitialized)
            {
                NvMMLiteJpegEncAttributeLevel oAttrLevel;
                NvError err = NvSuccess;
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;

                oAttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
                oAttrLevel.AttributeLevel1 = pNvxImageEncoder->nThumbnailQFactor;

                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_Quality,
                                                  0,
                                                  sizeof(NvMMLiteJpegEncAttributeLevel),
                                                  &oAttrLevel);
                if (err != NvSuccess)
                {
                }
            }
            break;
        }

        case NVX_IndexConfigThumbnailEnable:
        {
            NVX_CONFIG_THUMBNAILENABLE *pThumbnailEn =
                (NVX_CONFIG_THUMBNAILENABLE *)pComponentConfigStructure;

            if (!pThumbnailEn)
            {
                return OMX_ErrorBadParameter;
            }

            if (pNvxImageEncoder->bInitialized)
            {
                NvMMLiteJpegEncAttributeLevel oAttrLevel;
                SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;

                oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
                if (pThumbnailEn->bThumbnailEnabled)
                {
                    oAttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
                }
                else
                {
                    oAttrLevel.AttributeLevel_ThumbNailFlag = NV_FALSE;
                }

                err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                                  NvMMLiteAttributeImageEnc_ThumbnailSupport,
                                                  0,
                                                  sizeof(NvMMLiteJpegEncAttributeLevel),
                                                  &oAttrLevel);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for ThumbnailSupport\n"));
                }
            }
        }

        case NVX_IndexParamStereoCaptureInfo:
        {
            NVX_PARAM_STEREOCAPTUREINFO *pStCapInfo =
                (NVX_PARAM_STEREOCAPTUREINFO *)pComponentConfigStructure;
            NVX_STEREOCAPTUREINFO StCaptureInfo = pStCapInfo->StCaptureInfo;
            NvError err = NvSuccess;
            SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;
            NvMMCameraStereoCaptureInfo NvMMCamStCapInfo;

            // Break if not initalized
            if (!pNvxImageEncoder->bInitialized)
                break;

            switch (StCaptureInfo.CameraCaptureType)
            {
                case NvxCameraCaptureType_VideoSquashed:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_VideoSquashed;
                    break;
                case NvxCameraCaptureType_Video:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_Video;
                    break;
                case NvxCameraCaptureType_Still:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_Still;
                    break;
                case NvxCameraCaptureType_StillBurst:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_StillBurst;
                    break;
                case NvxCameraCaptureType_None:
                default:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_None;
                    break;
            }

            switch (StCaptureInfo.CameraStereoType)
            {
                case Nvx_BufferFlag_Stereo_LeftRight:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_LeftRight;
                    break;
                case Nvx_BufferFlag_Stereo_RightLeft:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_RightLeft;
                    break;
                case Nvx_BufferFlag_Stereo_TopBottom:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_TopBottom;
                    break;
                case Nvx_BufferFlag_Stereo_BottomTop:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_BottomTop;
                    break;
                case Nvx_BufferFlag_Stereo_SeparateLR:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_SeparateLR;
                    break;
                case Nvx_BufferFlag_Stereo_SeparateRL:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_SeparateRL;
                    break;
                case Nvx_BufferFlag_Stereo_None:
                default:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_None;
                    break;
            }

            err = pBase->hBlock->SetAttribute(
                      pBase->hBlock,
                      NvMMCameraAttribute_StereoCaptureInfo,
                      0,
                      sizeof(NvMMCameraStereoCaptureInfo),
                      &NvMMCamStCapInfo);
            if (NvSuccess != err)
            {
                NV_ASSERT("!SetAttribute for Stereo Capture Info failed");
                return OMX_ErrorBadParameter;
            }
            break;
        }

        case OMX_IndexParamQuantizationTable:
            err = NvxIESetQuantTable(pNvComp,
                                     (QTable) pComponentConfigStructure);
            if (err == NvSuccess)
                NvxIEUpdateQTables(pNvxImageEncoder, &pNvxImageEncoder->oBase);
            return err;

        default:
            return NvxComponentBaseSetConfig(pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE nvxImageEncoderProcessBuffer(
    NvxComponent *pComponent,
    const int port,
    OMX_BOOL *pbMoreWork)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxLiteImageEncoderData *pNvxImageEncoder =
                   (SNvxLiteImageEncoderData *)pComponent->pComponentData;

    eError = NvxNvMMLiteTransformWorkerBase(&pNvxImageEncoder->oBase,
                                        port, pbMoreWork);
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
    return eError;
}

static OMX_ERRORTYPE NvxImageEncoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortThumbnail = &pComponent->pPorts[s_nvx_port_thumbnail];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    OMX_BOOL bNeedThumbnail = pPortThumbnail->oPortDef.bEnabled;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxImageEncoderWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;

    // Note: For each worker function trigger, ask NvMM component to
    // encode. Callbacks should return the actual data
    if (!(bNeedThumbnail && pNvxImageEncoder->bThumbnailPending))
    {
        eError = nvxImageEncoderProcessBuffer(pComponent,s_nvx_port_input,pbMoreWork);
        if (bNeedThumbnail)
            pNvxImageEncoder->bThumbnailPending = OMX_TRUE;
    }
    else
    {
        *pbMoreWork = OMX_TRUE;
        eError = OMX_ErrorNone;
    }

    if (OMX_ErrorNone == eError && *pbMoreWork &&
        bNeedThumbnail && pNvxImageEncoder->bThumbnailPending)
    {
        *pbMoreWork = OMX_FALSE;
        eError = nvxImageEncoderProcessBuffer(pComponent,s_nvx_port_thumbnail,pbMoreWork);
        if (bNeedThumbnail)
            pNvxImageEncoder->bThumbnailPending = OMX_FALSE;
    }

    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);

    if (bNeedThumbnail && !pPortThumbnail->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortThumbnail);

    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);

    return eError;
}

static OMX_ERRORTYPE nvxImageEncoderSetupThumbnailPort(
    NvxComponent *pComponent,
    OMX_BOOL enable)
{
    NvMMLiteJpegEncAttributeLevel attr;
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    NvError err = NvSuccess;
    SNvxLiteImageEncoderData *pEnc = NULL;
    SNvxNvMMLiteTransformData *pBase = NULL;

    pEnc = (SNvxLiteImageEncoderData *)pComponent->pComponentData;
    pBase = &pEnc->oBase;

    if (enable)
    {
        pEnc->oBase.oPorts[s_nvx_port_thumbnail].bHasSize = OMX_TRUE;
        pEnc->oBase.oPorts[s_nvx_port_thumbnail].nWidth = pEnc->nThumbnailWidth;
        pEnc->oBase.oPorts[s_nvx_port_thumbnail].nHeight = pEnc->nThumbnailHeight;
        pEnc->oBase.oPorts[s_nvx_port_thumbnail].bRequestSurfaceBuffers = OMX_TRUE;
    }

    attr.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
    attr.AttributeLevel_ThumbNailFlag = enable? NV_TRUE : NV_FALSE;
    err = pBase->hBlock->SetAttribute(pBase->hBlock,
                         NvMMLiteAttributeImageEnc_ThumbnailSupport,
                         0,
                         sizeof(NvMMLiteJpegEncAttributeLevel),
                         &attr);
    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for ThumbnailSupport\n"));
    }

    if (!enable)
    {
        return omxErr;
    }

    if (pEnc->oType == TYPE_JPEG)
    {
        attr.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
        attr.AttributeLevel_ThumbNailFlag = NV_TRUE;
        attr.AttributeLevel1 = NVMMLITE_JPEG_COLOR_FORMAT_420;
        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                             NvMMLiteAttributeImageEnc_InputFormat,
                             0,
                             sizeof(NvMMLiteJpegEncAttributeLevel),
                             &attr);
        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for InputFormat\n"));
        }

        attr.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
        attr.AttributeLevel_ThumbNailFlag = NV_TRUE;
        attr.AttributeLevel1 = pBase->oPorts[s_nvx_port_thumbnail].nWidth;
        attr.AttributeLevel2 = pBase->oPorts[s_nvx_port_thumbnail].nHeight;
        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                             NvMMLiteAttributeImageEnc_resolution,
                             0,
                             sizeof(NvMMLiteJpegEncAttributeLevel),
                             &attr);
        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for resolution\n"));
        }
    }

    omxErr = NvxNvMMLiteTransformSetupInput(&pEnc->oBase,
        s_nvx_port_thumbnail,
        &pComponent->pPorts[s_nvx_port_thumbnail],
        MAX_INPUT_BUFFERS,
        MIN_INPUT_BUFFERSIZE,
        OMX_FALSE);
    return omxErr;
}

static OMX_ERRORTYPE NvxImageEncoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageEncoderAcquireResources\n"));

    /* Get image encoder instance */
    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageEncoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxImageEncoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    eError = NvxNvMMLiteTransformOpen(&pNvxImageEncoder->oBase, pNvxImageEncoder->oBlockType, pNvxImageEncoder->sBlockName, s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    pNvxImageEncoder->oBase.bSequence = OMX_TRUE;

    // Set the input port to format data as surfaces
    // TO DO: Set dimensions for real
    pNvxImageEncoder->oBase.oPorts[s_nvx_port_input].bHasSize = OMX_TRUE;
    pNvxImageEncoder->oBase.oPorts[s_nvx_port_input].nWidth = pNvxImageEncoder->nSurfWidth;
    pNvxImageEncoder->oBase.oPorts[s_nvx_port_input].nHeight = pNvxImageEncoder->nSurfHeight;
    pNvxImageEncoder->oBase.oPorts[s_nvx_port_input].bRequestSurfaceBuffers = OMX_TRUE;

    if (pComponent->pPorts[s_nvx_port_thumbnail].oPortDef.bEnabled)
    {
        pNvxImageEncoder->oBase.oPorts[s_nvx_port_thumbnail].bHasSize = OMX_TRUE;
        pNvxImageEncoder->oBase.oPorts[s_nvx_port_thumbnail].nWidth = pNvxImageEncoder->nThumbnailWidth;
        pNvxImageEncoder->oBase.oPorts[s_nvx_port_thumbnail].nHeight = pNvxImageEncoder->nThumbnailHeight;
        pNvxImageEncoder->oBase.oPorts[s_nvx_port_thumbnail].bRequestSurfaceBuffers = OMX_TRUE;
    }

    pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].bHasSize = OMX_TRUE;
    pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].nWidth = pNvxImageEncoder->nSurfWidth;
    pNvxImageEncoder->oBase.oPorts[s_nvx_port_output].nHeight = pNvxImageEncoder->nSurfHeight;

    // JPEG encoder requires output buffers to be shared memory
    pNvxImageEncoder->oBase.bInSharedMemory = NV_TRUE;

    if ( pNvxImageEncoder->oType == TYPE_JPEG)
    {
        NvMMLiteJpegEncAttributeLevel oAttrLevel;
        NvError err = NvSuccess;
        SNvxNvMMLiteTransformData *pBase = &pNvxImageEncoder->oBase;
        if ( pBase->oPorts[s_nvx_port_input].bHasSize )
        {
            oAttrLevel.AttributeLevel_ThumbNailFlag = NV_FALSE;
            oAttrLevel.AttributeLevel1 = pBase->oPorts[s_nvx_port_input].nWidth;
            oAttrLevel.AttributeLevel2 = pBase->oPorts[s_nvx_port_input].nHeight;
            err = pBase->hBlock->SetAttribute(
                pBase->hBlock,
                NvMMLiteAttributeImageEnc_resolution,
                0,
                sizeof(NvMMLiteJpegEncAttributeLevel),
                &oAttrLevel);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("JPE error: unable to set encode resolution\n"));
            }
        }

        oAttrLevel.AttributeLevel_ThumbNailFlag = NV_FALSE;
        oAttrLevel.AttributeLevel1 = pNvxImageEncoder->nQFactor;
        err = pBase->hBlock->SetAttribute(
                pBase->hBlock,
                NvMMLiteAttributeImageEnc_Quality,
                0,
                sizeof(NvMMLiteJpegEncAttributeLevel),
                &oAttrLevel);
        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("JPE error: unable to set qfactor\n"));
        }

        if(pNvxImageEncoder->bThumbnailQF == OMX_TRUE)
        {
            oAttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
            oAttrLevel.AttributeLevel1 = pNvxImageEncoder->nThumbnailQFactor;
            err = pBase->hBlock->SetAttribute(
                    pBase->hBlock,
                    NvMMLiteAttributeImageEnc_Quality,
                    0,
                    sizeof(NvMMLiteJpegEncAttributeLevel),
                    &oAttrLevel);
            if (err != NvSuccess)
            {
            }
        }


        NvxIEUpdateQTables(pNvxImageEncoder, pBase);

        if(pNvxImageEncoder->nEncodedSize)
        {
            oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
            oAttrLevel.AttributeLevel1 = pNvxImageEncoder->nEncodedSize;
            err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                              NvMMLiteAttributeImageEnc_EncodedSize,
                                              0,
                                              sizeof(NvMMLiteJpegEncAttributeLevel),
                                              &oAttrLevel);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for EncodedSize\n"));
            }
        }

        oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
        if (pComponent->pPorts[s_nvx_port_thumbnail].oPortDef.bEnabled)
            oAttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
        else
            oAttrLevel.AttributeLevel_ThumbNailFlag = NV_FALSE;

        err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                          NvMMLiteAttributeImageEnc_ThumbnailSupport,
                                          0,
                                          sizeof(NvMMLiteJpegEncAttributeLevel),
                                          &oAttrLevel);
        if (err != NvSuccess)
        {
            NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for ThumbnailSupport\n"));
        }

        if (pComponent->pPorts[s_nvx_port_thumbnail].oPortDef.bEnabled)
        {
            oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
            oAttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
            oAttrLevel.AttributeLevel1 = NVMMLITE_JPEG_COLOR_FORMAT_420;
            err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                              NvMMLiteAttributeImageEnc_InputFormat,
                                              0,
                                              sizeof(NvMMLiteJpegEncAttributeLevel),
                                              &oAttrLevel);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for InputFormat\n"));
            }

            oAttrLevel.StructSize = sizeof(NvMMLiteJpegEncAttributeLevel);
            oAttrLevel.AttributeLevel_ThumbNailFlag = NV_TRUE;
            oAttrLevel.AttributeLevel1 = pBase->oPorts[s_nvx_port_thumbnail].nWidth;
            oAttrLevel.AttributeLevel2 = pBase->oPorts[s_nvx_port_thumbnail].nHeight;
            err = pBase->hBlock->SetAttribute(pBase->hBlock,
                                              NvMMLiteAttributeImageEnc_resolution,
                                              0,
                                              sizeof(NvMMLiteJpegEncAttributeLevel),
                                              &oAttrLevel);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("JPE error: SetAttribute failed for resolution\n"));
            }
        }
    }

    eError = NvxNvMMLiteTransformSetupInput(&pNvxImageEncoder->oBase,
        s_nvx_port_input,
        &pComponent->pPorts[s_nvx_port_input],
        MAX_INPUT_BUFFERS,
        MIN_INPUT_BUFFERSIZE,
        OMX_FALSE);
    if (eError != OMX_ErrorNone)
        return eError;

    if (pComponent->pPorts[s_nvx_port_thumbnail].oPortDef.bEnabled)
    {
        eError = NvxNvMMLiteTransformSetupInput(&pNvxImageEncoder->oBase,
            s_nvx_port_thumbnail,
            &pComponent->pPorts[s_nvx_port_thumbnail],
            MAX_INPUT_BUFFERS,
            MIN_INPUT_BUFFERSIZE,
            OMX_FALSE);
        if (eError != OMX_ErrorNone)
            return eError;
    }

    eError = NvxNvMMLiteTransformSetupOutput(&pNvxImageEncoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone)
        return eError;

    pNvxImageEncoder->bInitialized = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxImageEncoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageEncoderReleaseResources\n"));

    /* get image encoder instance */
    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxImageEncoder);

    if (!pNvxImageEncoder->bInitialized)
        return OMX_ErrorNone;

    eError = NvxNvMMLiteTransformClose(&pNvxImageEncoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxImageEncoder->bInitialized = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxImageEncoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxImageEncoderFlush\n"));

    /* get image encoder instance */
    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pComponent);

    return NvxNvMMLiteTransformFlush(&pNvxImageEncoder->oBase, nPort);
}

static OMX_ERRORTYPE NvxImageEncoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageEncoderFillThisBuffer\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    if (!pNvxImageEncoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformFillThisBuffer(&pNvxImageEncoder->oBase, pBufferHdr, s_nvx_port_output);
}

static OMX_ERRORTYPE NvxImageEncoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageEncoderPreChangeState\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    if (!pNvxImageEncoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformPreChangeState(&pNvxImageEncoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxImageEncoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxImageEncoderEmptyThisBuffer\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    if (!pNvxImageEncoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMLiteTransformIsInputSkippingWorker(&pNvxImageEncoder->oBase,
                                              pBufferHdr->nInputPortIndex))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
        *bHandled = OMX_FALSE;

    return eError;
}


static OMX_ERRORTYPE NvxImageEncoderReturnBuffers(
    NvxComponent *pComponent,
    OMX_U32 nPort,
    OMX_BOOL bWorkerLocked)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = NULL;
    NvMMLiteBlockHandle hNvMMBlock = NULL;

    NV_ASSERT(pComponent);
    NV_ASSERT(pComponent->eState != OMX_StateLoaded);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType,
        "+NvxImageEncoderReturnBuffers\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pComponent->pComponentData;
    if (!pNvxImageEncoder->bInitialized)
        return OMX_ErrorNone;

    hNvMMBlock = pNvxImageEncoder->oBase.hBlock;
#if ENABLE_TUNNELLING
    // abort nvmm buffer if it's tunneled and not the allocator
    if ((pNvxImageEncoder->oBase.oPorts[nPort].nType ==
            TF_TYPE_INPUT_TUNNELED ||
         pNvxImageEncoder->oBase.oPorts[nPort].nType ==
            TF_TYPE_OUTPUT_TUNNELED) &&
        !pNvxImageEncoder->oBase.oPorts[nPort].bTunneledAllocator)
    {
        if (bWorkerLocked)
            NvxMutexUnlock(pComponent->hWorkerMutex);
        hNvMMBlock->AbortBuffers(hNvMMBlock, nPort);
        NvOsSemaphoreWait(
            pNvxImageEncoder->oBase.oPorts[nPort].blockAbortDone);
        if (bWorkerLocked)
            NvxMutexLock(pComponent->hWorkerMutex);
    }
#endif
    return OMX_ErrorNone;
}

/**
 * uEventType == 0 => port disable
 * uEventType == 1 => port enable
 */
static OMX_ERRORTYPE NvxImageEncoderPortEventHandler(
    NvxComponent *pNvComp,
    OMX_U32 nPort,
    OMX_U32 uEventType)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMLiteBlockHandle hNvMMBlock = NULL;
    NvxPort *pPort = NULL;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType,
        "+NvxImageEncoderPortEventHandler\n"));

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    if (!pNvxImageEncoder->bInitialized)
        return OMX_ErrorNone;

    hNvMMBlock = pNvxImageEncoder->oBase.hBlock;
    pPort = pNvComp->pPorts + nPort;

    // do nothing when state is loaded
    if (pNvComp->eState == OMX_StateLoaded)
    {
        return OMX_ErrorNone;
    }

    switch (uEventType)
    {
        // port disable
        case 0:
            if (nPort == (OMX_U32)s_nvx_port_output)
            {
                // free any buffers that were allocated before, since the output port
                // workaround in the port enable case will alloc new ones
                NvxNvMMLiteTransformFreePortBuffers(
                    &pNvxImageEncoder->oBase,
                    s_nvx_port_output);
            }
            break;

        // port enable
        case 1:
            if (nPort == (OMX_U32)s_nvx_port_input)
            {
                --(&pNvxImageEncoder->oBase)->nNumInputPorts;
                // To reallocate input buffers.
                eError = NvxNvMMLiteTransformSetupInput(&pNvxImageEncoder->oBase,
                        s_nvx_port_input,
                        &pNvComp->pPorts[s_nvx_port_input],
                        MAX_INPUT_BUFFERS,
                        MIN_INPUT_BUFFERSIZE,
                        OMX_FALSE);
            }
            else if (nPort == (OMX_U32)s_nvx_port_output) {
                SNvxNvMMLiteTransformData *pData = &pNvxImageEncoder->oBase;
                // the SetupOutput function increments this, which is bad, since
                // our WAR here lets it be called many times.  we should never
                // increment it when it is called from the port event handler,
                // so reverse that operation now.
                pData->nNumOutputPorts--;
                // To reallocate output buffers.
                eError = NvxNvMMLiteTransformSetupOutput(
                        pData,
                        s_nvx_port_output,
                        &pNvComp->pPorts[s_nvx_port_output],
                        s_nvx_port_input,
                        MAX_OUTPUT_BUFFERS,
                        MIN_OUTPUT_BUFFERSIZE);
            }
            else if (nPort == (OMX_U32)s_nvx_port_thumbnail)
            {
                eError = nvxImageEncoderSetupThumbnailPort(pNvComp,OMX_TRUE);
            }
            break;

        default:
            eError = OMX_ErrorBadParameter;
    }

    return eError;
}

static OMX_ERRORTYPE
NvxImageEncoderTunnelRequest(
    OMX_IN NvxComponent *pNvComp,
    OMX_IN OMX_U32 nPort,
    OMX_IN OMX_HANDLETYPE hTunneledComp,
    OMX_IN OMX_U32 nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    SNvxLiteImageEncoderData *pNvxImageEncoder = NULL;
    SNvxNvMMLitePort *pPort = NULL;

    if (!pNvComp || !pNvComp->pComponentData)
    {
        return OMX_ErrorBadParameter;
    }

    pNvxImageEncoder = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;

    if (!pNvxImageEncoder->bInitialized)
    {
        return OMX_ErrorNone;
    }

    if (hTunneledComp == NULL || pTunnelSetup == NULL)
    {
        pPort = &pNvxImageEncoder->oBase.oPorts[nPort];
#if ENABLE_TUNNELLING
        // Set port state to untunneled:
        if (pPort->nType == TF_TYPE_INPUT_TUNNELED)
            pPort->nType = TF_TYPE_INPUT;
        else if (pPort->nType == TF_TYPE_OUTPUT_TUNNELED)
            pPort->nType = TF_TYPE_OUTPUT;

        pPort->bTunneling = OMX_FALSE;
        pPort->pTunnelBlock = NULL;
#endif
    }

    return OMX_ErrorNone;
}

/*****************************************************************************/

static OMX_ERRORTYPE NvxCommonImageEncoderInit(OMX_HANDLETYPE hComponent, NvMMLiteBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxLiteImageEncoderData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_IMAGE_ENCODER,"ERROR:NvxCommonImageEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    // FIXME: resources?

    pNvComp->eObjectType = NVXT_IMAGE_ENCODER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxLiteImageEncoderData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxLiteImageEncoderData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxLiteImageEncoderData));

    // TO DO: Fill in with image codecs
    switch (oBlockType)
    {
        case NvMMLiteBlockType_EncJPEG: data->oType = TYPE_JPEG; break;
        default: return OMX_ErrorBadParameter;
    }

    data->oBlockType  = oBlockType;
    data->sBlockName  = sBlockName;
    data->nSurfWidth  = 176; // default width/height
    data->nSurfHeight = 144;
    data->nQFactor    = 75;  // default OMX quality factor [1, 100]
    data->nThumbnailQFactor = 75;  // default OMX thumbnail quality factor [1, 100]
    data->nEncodedSize = 0;

    pNvComp->DeInit                 = NvxImageEncoderDeInit;
    pNvComp->GetParameter           = NvxImageEncoderGetParameter;
    pNvComp->SetParameter           = NvxImageEncoderSetParameter;
    pNvComp->GetConfig              = NvxImageEncoderGetConfig;
    pNvComp->SetConfig              = NvxImageEncoderSetConfig;
    pNvComp->WorkerFunction         = NvxImageEncoderWorkerFunction;
    pNvComp->AcquireResources       = NvxImageEncoderAcquireResources;
    pNvComp->ReleaseResources       = NvxImageEncoderReleaseResources;
    pNvComp->FillThisBufferCB       = NvxImageEncoderFillThisBuffer;
    pNvComp->PreChangeState         = NvxImageEncoderPreChangeState;
    pNvComp->EmptyThisBuffer        = NvxImageEncoderEmptyThisBuffer;
    pNvComp->Flush                  = NvxImageEncoderFlush;
    pNvComp->PortEventHandler       = NvxImageEncoderPortEventHandler;
    pNvComp->ReturnBuffers          = NvxImageEncoderReturnBuffers;
    pNvComp->ComponentTunnelRequest = NvxImageEncoderTunnelRequest;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[s_nvx_port_input].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_GFSURFACES;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_thumbnail], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[s_nvx_port_thumbnail].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pNvComp->pPorts[s_nvx_port_thumbnail].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_GFSURFACES;
    pNvComp->pPorts[s_nvx_port_thumbnail].oPortDef.bEnabled = OMX_FALSE;

    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxLiteJpegEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteJpegEncoderInit\n"));

    eError = NvxCommonImageEncoderInit(hComponent, NvMMLiteBlockType_EncJPEG, "BlockJpgEnc");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteJpegEncoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pNvComp->pComponentName = "OMX.Nvidia.jpeg.encoder";
    pNvComp->sComponentRoles[0] = "image_encoder.jpeg";
    pNvComp->nComponentRoles    = 1;

    NvxPortInitImage(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE, OMX_IMAGE_CodingJPEG);

    return eError;
}



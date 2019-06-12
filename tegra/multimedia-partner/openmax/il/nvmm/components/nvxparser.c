/*
 * Copyright (c) 2008 - 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxParser.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include <NVOMX_TrackList.h>

#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "components/common/NvxIndexExtensions.h"
#include "nvmm/components/common/nvmmtransformbase.h"
#include "nvmm_parser.h"
#include "nvmm_aac.h"
#include "nvmm_videodec.h"
#include "nvmm_metadata.h"
#include "nvassert.h"
#include "nvfxmath.h"
#include "nvxhelpers_int.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */


#define MAX_OUTPUT_BUFFERS      15
#define MIN_OUTPUT_BUFFERSIZE   1024*64
#define SMALL_OUTPUT_BUFFERSIZE 1024
#define BIG_OUTPUT_BUFFERSIZE   (1920*1088*3)/4

#define TYPE_AUDIO 1
#define TYPE_SUPER 2

/* Decoder State information */
typedef struct SNvxParserData
{
    OMX_BOOL    bInitialized;
    OMX_BOOL    bOpened;
    OMX_BOOL    bErrorReportingEnabled;
    OMX_STRING  szFilename;
    OMX_STRING  szUserAgent;
    OMX_STRING  szUserAgentProf;
    SNvxNvMMTransformData oBase;
    int oType;
    NvMMParserCoreType eFileType;
    const char *sBlockName;
    NvU32 streamCount;
    NvMMStreamInfo *pStreamInfo;
    OMX_U32 nPortToStreamIndexMap[2];
    OMX_TICKS nDuration;
    ENvxStreamType eStreamType[2];

    int port_video;
    int port_audio;

    OMX_BOOL bLowMem;
} SNvxParserData;                                                  // TBD : Do we have to add any other variables to the structure.


static OMX_ERRORTYPE NvxParserProbeFile(OMX_IN NvxComponent *pComponent);
static OMX_ERRORTYPE NvxParserGetFileAttributes(OMX_IN NvxComponent *pComponent);
static OMX_ERRORTYPE NvxParserSetFilename(OMX_IN NvxComponent *pComponent);

static OMX_ERRORTYPE NvxParserDeInit(NvxComponent *pNvComp)
{
    SNvxParserData *pNvxParser;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxParserDeInit\n"));

    pNvxParser = (SNvxParserData *)pNvComp->pComponentData;

    if (pNvxParser)
    {
        if (pNvxParser->bOpened)
        {
            eError = NvxNvMMTransformClose(&pNvxParser->oBase);
            if (eError != OMX_ErrorNone)
                return eError;

            pNvxParser->bOpened = OMX_FALSE;
        }

        NvOsFree(pNvxParser->szFilename);
        pNvxParser->szFilename = NULL;

        NvOsFree(pNvxParser->szUserAgent);
        pNvxParser->szUserAgent = NULL;

        NvOsFree(pNvxParser->szUserAgentProf);
        pNvxParser->szUserAgentProf = NULL;

        NvOsFree(pNvxParser->pStreamInfo);
        pNvxParser->pStreamInfo = NULL;
    }

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;

    return eError;
}


/*
TBD : Are there any other cases to be dealt i.e does the parser block has to take care of any other nparamIndex
*/
static OMX_ERRORTYPE NvxParserGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxParserData *pNvxParser;

    NV_ASSERT(pComponent);

    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxParserGetParameter\n"));

    switch (nParamIndex)
    {
        case NVX_IndexParamDuration:
        {
            NVX_PARAM_DURATION *pDur = (NVX_PARAM_DURATION *)pComponentParameterStructure;

            if (!pNvxParser->bOpened)
                return OMX_ErrorNotReady;

            pDur->nDuration = pNvxParser->nDuration;
            break;
        }
        case NVX_IndexParamStreamType:
        {
            NVX_PARAM_STREAMTYPE *pType = (NVX_PARAM_STREAMTYPE *)pComponentParameterStructure;

            if (!pNvxParser->bOpened)
                return OMX_ErrorNotReady;

            if (pType->nPort >= 2)
                return OMX_ErrorBadParameter;

            pType->eStreamType = pNvxParser->eStreamType[pType->nPort];
            break;
        }
        case NVX_IndexParamStreamCount:
            {
                NVX_PARAM_STREAMCOUNT *pCount= (NVX_PARAM_STREAMCOUNT*)pComponentParameterStructure;

                if (!pNvxParser->bOpened)
                    return OMX_ErrorNotReady;

                pCount->StreamCount= pNvxParser->streamCount;
                break;
            }
        case NVX_IndexParamAudioParams:
        {
            NVX_PARAM_AUDIOPARAMS *pType = (NVX_PARAM_AUDIOPARAMS *)pComponentParameterStructure;
            NvMMStreamInfo *pSI;

            if (!pNvxParser->bOpened)
                return OMX_ErrorNotReady;
            if (pType->nPort >= 2)
                return OMX_ErrorBadParameter;

            if (pNvxParser->port_audio >= 0)
            {
                int i = pNvxParser->nPortToStreamIndexMap[pNvxParser->port_audio];
                pSI = &(pNvxParser->pStreamInfo[i]);
                if (pSI && NVMM_ISSTREAMAUDIO(pSI->StreamType))
                {
                    pType->nSampleRate    = pSI->NvMMStream_Props.AudioProps.SampleRate;
                    pType->nBitRate       = pSI->NvMMStream_Props.AudioProps.BitRate;
                    pType->nChannels      = pSI->NvMMStream_Props.AudioProps.NChannels;
                    pType->nBitsPerSample = pSI->NvMMStream_Props.AudioProps.BitsPerSample;
                    return OMX_ErrorNone;
                }
            }
            return OMX_ErrorBadParameter;
        }
        case OMX_IndexParamNumAvailableStreams:
        {
            OMX_PARAM_U32TYPE *nas = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;
            unsigned int i=0;

            nas->nU32 = 0;

            if (nas->nPortIndex >= 2)
                return OMX_ErrorBadParameter;

            for (i = 0; i < pNvxParser->streamCount; i++)
            {
                if (NVMM_ISSTREAMVIDEO(pNvxParser->pStreamInfo[i].StreamType))
                {
                    if (nas->nPortIndex == (unsigned int) pNvxParser->port_video)
                    {
                        nas->nU32++;
                    }
                }

                if (NVMM_ISSTREAMAUDIO(pNvxParser->pStreamInfo[i].StreamType))
                {
                    if (nas->nPortIndex == (unsigned int) pNvxParser->port_audio)
                    {
                        nas->nU32++;
                    }
                }
            }

        }
        break;

        default:
            return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxParserSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxParserData *pNvxParser;

    pNvxParser = (SNvxParserData *)pComponent->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxParserSetParameter\n"));

    NV_ASSERT(pNvxParser);

    switch (nIndex)
    {
        case NVX_IndexParamLowMemMode:
            pNvxParser->bLowMem = ((NVX_PARAM_LOWMEMMODE *)pComponentParameterStructure)->bLowMemMode;
            break;
        case NVX_IndexParamFilename:
        {
            NVX_PARAM_FILENAME *pFilenameParam = pComponentParameterStructure;
            NvOsFree(pNvxParser->szFilename);
            pNvxParser->szFilename = NvOsAlloc(NvOsStrlen(pFilenameParam->pFilename) + 1 );
            if(!pNvxParser->szFilename)
            {
                NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxParserSetParameter:"
                    ":szFilename = %s:[%s(%d)]\n",pNvxParser->szFilename,__FILE__,__LINE__));
                return OMX_ErrorInsufficientResources;
            }
            NvOsStrncpy(pNvxParser->szFilename, pFilenameParam->pFilename, NvOsStrlen(pFilenameParam->pFilename) + 1);

            if (pNvxParser->bOpened)
            {
                NvxParserSetFilename(pComponent);
                return NvxParserGetFileAttributes(pComponent);
            }
            return NvxParserProbeFile(pComponent);
        }
        case NVX_IndexParamUserAgent:
        {
            NVX_PARAM_USERAGENT *pUserAgent = pComponentParameterStructure;
            NvOsFree(pNvxParser->szUserAgent);
            pNvxParser->szUserAgent = NvOsAlloc(NvOsStrlen(pUserAgent->pUserAgentStr) + 1 );
            if(!pNvxParser->szUserAgent)
            {
                NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxParserSetParameter:"
                    ":szUserAgent = %s:[%s(%d)]\n",pNvxParser->szUserAgent,__FILE__,__LINE__));
                return OMX_ErrorInsufficientResources;
            }
            NvOsStrncpy(pNvxParser->szUserAgent, pUserAgent->pUserAgentStr, NvOsStrlen(pUserAgent->pUserAgentStr) + 1);

        }

        case NVX_IndexParamUserAgentProf:
        {
            NVX_PARAM_USERAGENTPROF *pUserAgentProf = pComponentParameterStructure;
            NvOsFree(pNvxParser->szUserAgentProf);
            pNvxParser->szUserAgentProf = NvOsAlloc(NvOsStrlen(pUserAgentProf->pUserAgentProfStr) + 1 );
            if(!pNvxParser->szUserAgentProf)
            {
                NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxParserSetParameter:"
                    ":szUserAgentProf = %s:[%s(%d)]\n",pNvxParser->szUserAgentProf,__FILE__,__LINE__));
                return OMX_ErrorInsufficientResources;
            }
            NvOsStrncpy(pNvxParser->szUserAgentProf, pUserAgentProf->pUserAgentProfStr, NvOsStrlen(pUserAgentProf->pUserAgentProfStr) + 1);

        }

        default:
            return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxParserGetConfig(OMX_IN NvxComponent* pNvComp,
    OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxParserData *pNvxParser;
    OMX_U32 i=0;
    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxParserGetConfig\n"));

    pNvxParser = (SNvxParserData *)pNvComp->pComponentData;


    NV_ASSERT(pNvxParser);

    switch (nIndex)
    {
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pBlockReq = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!pNvxParser->bInitialized)
                return OMX_ErrorNotReady;

            pBlockReq->pNvMMTransform = &pNvxParser->oBase;
            pBlockReq->nNvMMPort = pNvxParser->nPortToStreamIndexMap[pBlockReq->nPortIndex];
            break;
        }
        case NVX_IndexConfigH264NALLen:
        {
            NVX_VIDEO_CONFIG_H264NALLEN *pNalLen;
            pNalLen = (NVX_VIDEO_CONFIG_H264NALLEN *)pComponentConfigStructure;
            if (!pNvxParser->bInitialized)
                return OMX_ErrorNotReady;

            if (pNvxParser->port_video >= 0)
            {
                NvS32 val = 0;
                pNvxParser->oBase.hBlock->GetAttribute(pNvxParser->oBase.hBlock,
                        NvMMVideoDecAttribute_H264NALLen, sizeof(NvS32), &val);
                pNalLen->nNalLen = val;
                return OMX_ErrorNone;
            }

            return OMX_ErrorBadParameter;
        }
        case OMX_IndexConfigVideoNalSize:
        {
            OMX_VIDEO_CONFIG_NALSIZE *pNalLen;
            pNalLen = (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
            if (!pNvxParser->bInitialized)
                return OMX_ErrorNotReady;

            if (pNvxParser->port_video >= 0)
            {
                NvS32 val = 0;
                pNvxParser->oBase.hBlock->GetAttribute(pNvxParser->oBase.hBlock,
                        NvMMVideoDecAttribute_H264NALLen, sizeof(NvS32), &val);
                pNalLen->nNaluBytes = val;
                return OMX_ErrorNone;
            }

            return OMX_ErrorBadParameter;
        }
        case NVX_IndexConfigHeader:
            {
                NVX_CONFIG_HEADER *pConfigHead =
                    (NVX_CONFIG_HEADER *)pComponentConfigStructure;
                NvError err;
                NvMMCodecInfo mCodecInfo;

                NvOsMemset(&mCodecInfo, 0, sizeof(NvMMCodecInfo));

                if (pNvxParser->port_video == (int)pConfigHead->nPortIndex)
                {
                    mCodecInfo.pBuffer = (NvU8 *) (pConfigHead->nBuffer);
                    mCodecInfo.BufferLen = pConfigHead->nBufferlen;
                    mCodecInfo.nPortIndex =  PORT_VIDEO;
                }

                else if  (pNvxParser->port_audio == (int)pConfigHead->nPortIndex)
                {
                    for (i = 0; i < pNvxParser->streamCount; i++)
                    {
                        if((pNvxParser->pStreamInfo[i].StreamType == NvMMStreamType_AAC) ||
                            (pNvxParser->pStreamInfo[i].StreamType == NvMMStreamType_AACSBR))
                        {
                            mCodecInfo.pBuffer = (NvU8 *) (pConfigHead->nBuffer);
                            mCodecInfo.BufferLen = pConfigHead->nBufferlen;
                            mCodecInfo.nPortIndex =  PORT_AUDIO;
                            break;
                        }
                       else if((pNvxParser->pStreamInfo[i].StreamType == NvMMStreamType_WMA) ||
                            (pNvxParser->pStreamInfo[i].StreamType == NvMMStreamType_WMALSL) ||
                            (pNvxParser->pStreamInfo[i].StreamType == NvMMStreamType_WMAPro) ||
                            (pNvxParser->pStreamInfo[i].StreamType == NvMMStreamType_WMAV))
                        {
                            mCodecInfo.pBuffer = (NvU8 *) (pConfigHead->nBuffer);
                            mCodecInfo.BufferLen = pConfigHead->nBufferlen;
                            mCodecInfo.nPortIndex =  PORT_AUDIO;
                            break;
                        }
                    }
                }
                err = pNvxParser->oBase.hBlock->GetAttribute(
                    pNvxParser->oBase.hBlock,
                    NvMMAttribute_Header,
                    sizeof(NvMMCodecInfo),
                    &mCodecInfo);
                if (NvSuccess != err)
                {
                    return OMX_ErrorUndefined;
                }
                pConfigHead->nBufferlen = mCodecInfo.BufferLen;
                return OMX_ErrorNone;
            }


        case NVX_IndexConfigQueryMetadata:
        {
            NVX_CONFIG_QUERYMETADATA *md = (NVX_CONFIG_QUERYMETADATA *)pComponentConfigStructure;
            NvMMMetaDataInfo mdinfo;
            NvError err;

            NvOsMemset(&mdinfo, 0, sizeof(NvMMMetaDataInfo));

            switch (md->eType)
            {
                case NvxMetadata_Artist: mdinfo.eMetadataType = NvMMMetaDataInfo_Artist; break;
                case NvxMetadata_Album: mdinfo.eMetadataType = NvMMMetaDataInfo_Album; break;
                case NvxMetadata_Genre: mdinfo.eMetadataType = NvMMMetaDataInfo_Genre; break;
                case NvxMetadata_Title: mdinfo.eMetadataType = NvMMMetaDataInfo_Title; break;
                case NvxMetadata_Year: mdinfo.eMetadataType = NvMMMetaDataInfo_Year; break;
                case NvxMetadata_TrackNum: mdinfo.eMetadataType = NvMMMetaDataInfo_TrackNumber; break;
                case NvMMetadata_Encoded: mdinfo.eMetadataType = NvMMMetaDataInfo_Encoded; break;
                case NvxMetadata_Comment: mdinfo.eMetadataType = NvMMMetaDataInfo_Comment; break;
                case NvxMetadata_Composer: mdinfo.eMetadataType = NvMMMetaDataInfo_Composer; break;
                case NvxMetadata_Publisher: mdinfo.eMetadataType = NvMMMetaDataInfo_Publisher; break;
                case NvxMetadata_OriginalArtist: mdinfo.eMetadataType = NvMMMetaDataInfo_OrgArtist; break;
                case NvxMetadata_Copyright: mdinfo.eMetadataType = NvMMMetaDataInfo_Copyright; break;
                case NvxMetadata_Url: mdinfo.eMetadataType = NvMMMetaDataInfo_Url; break;
                case NvxMetadata_BPM: mdinfo.eMetadataType = NvMMMetaDataInfo_Bpm; break;
                case NvxMetadata_AlbumArtist: mdinfo.eMetadataType = NvMMMetaDataInfo_AlbumArtist; break;
                case NvxMetadata_CoverArt: mdinfo.eMetadataType = NvMMMetaDataInfo_CoverArt; break;
                case NvxMetadata_CoverArtURL: mdinfo.eMetadataType = NvMMMetaDataInfo_CoverArtURL; break;
                case NvxMetadata_ThumbnailSeektime: mdinfo.eMetadataType = NvMMMetaDataInfo_ThumbnailSeekTime; break;
                case NvxMetadata_RtcpAppData: mdinfo.eMetadataType = NvMMMetaDataInfo_RtcpAppData; break;
                case NvxMetadata_RTCPSdesCname: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesCname; break;
                case NvxMetadata_RTCPSdesName: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesName; break;
                case NvxMetadata_RTCPSdesEmail: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesEmail; break;
                case NvxMetadata_RTCPSdesPhone: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesPhone; break;
                case NvxMetadata_RTCPSdesLoc: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesLoc; break;
                case NvxMetadata_RTCPSdesTool: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesTool; break;
                case NvxMetadata_RTCPSdesNote: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesNote; break;
                case NvxMetadata_RTCPSdesPriv: mdinfo.eMetadataType = NvMMMetaDataInfo_RTCPSdesPriv; break;
                case NvxMetadata_SeekNotPossible: mdinfo.eMetadataType = NvMMMetaDataInfo_SeekNotPossible; break;
                default: return OMX_ErrorBadParameter;
            }

            mdinfo.pClientBuffer = (NvS8 *)md->sValueStr;
            mdinfo.nBufferSize = md->nValueLen;

            err = pNvxParser->oBase.hBlock->GetAttribute(pNvxParser->oBase.hBlock,
                    NvMMAttribute_Metadata, sizeof(NvMMMetaDataInfo), &mdinfo);

            if (NvSuccess != err)
            {
                if (err == NvError_InSufficientBufferSize)
                {
                    md->nValueLen = mdinfo.nBufferSize;
                    return OMX_ErrorInsufficientResources;
                }
                return OMX_ErrorUndefined;
            }

            switch (mdinfo.eEncodeType)
            {
                case NvMMMetaDataEncode_Utf16: md->eCharSet = OMX_MetadataCharsetUTF16LE; break;
                case NvMMMetaDataEncode_Utf8: md->eCharSet = OMX_MetadataCharsetUTF8; break;
                case NvMMMetaDataEncode_NvU32: md->eCharSet = NVOMX_MetadataCharsetU32; break;
                case NvMMMetaDataEncode_NvU64: md->eCharSet = NVOMX_MetadataCharsetU64; break;
                case NvMMMetaDataEncode_Binary: md->eCharSet = OMX_MetadataCharsetBinary; break;
                case NvMMMetaDataEncode_JPEG: md->eCharSet = NVOMX_MetadataFormatJPEG; break;
                case NvMMMetaDataEncode_PNG: md->eCharSet = NVOMX_MetadataFormatPNG; break;
                case NvMMMetaDataEncode_BMP: md->eCharSet = NVOMX_MetadataFormatBMP; break;
                case NvMMMetaDataEncode_Other: md->eCharSet = NVOMX_MetadataFormatUnknown; break;
                case NvMMMetaDataEncode_GIF: md->eCharSet = NVOMX_MetadataFormatGIF; break;
                default: md->eCharSet = OMX_MetadataCharsetASCII; break;
            }

            md->nValueLen = mdinfo.nBufferSize;
            break;
        }
        case NvxIndexConfigTracklist:
        case NvxIndexConfigTracklistTrack:
        case NvxIndexConfigTracklistDelete:
        case NvxIndexConfigTracklistSize:
        case NvxIndexConfigTracklistCurrent:
            return OMX_ErrorNotImplemented;
        case NVX_IndexConfigFileCacheInfo:
        {
            NvMMAttrib_FileCacheInfo FileCacheInfo;
            NVX_CONFIG_FILECACHEINFO *pFileCacheInfo = (NVX_CONFIG_FILECACHEINFO *)pComponentConfigStructure;
            NvOsMemset(&FileCacheInfo, 0, sizeof(NvMMAttrib_FileCacheInfo));
            if (!pNvxParser->bInitialized)
                return OMX_ErrorNotReady;
            pNvxParser->oBase.hBlock->GetAttribute(pNvxParser->oBase.hBlock,
                                                   NvMMAttribute_FileCacheInfo, 
                                                   sizeof(NvMMAttrib_FileCacheInfo), 
                                                   &FileCacheInfo);

            pFileCacheInfo->nDataBegin  = (OMX_U64)FileCacheInfo.nDataBegin;
            pFileCacheInfo->nDataFirst  = (OMX_U64)FileCacheInfo.nDataFirst;
            pFileCacheInfo->nDataCur    = (OMX_U64)FileCacheInfo.nDataCur;     
            pFileCacheInfo->nDataLast   = (OMX_U64)FileCacheInfo.nDataLast;  
            pFileCacheInfo->nDataEnd    = (OMX_U64)FileCacheInfo.nDataEnd;

            return OMX_ErrorNone;
        }
        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxParserSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    SNvxParserData *pNvxParser;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxParserSetConfig\n"));

    pNvxParser = (SNvxParserData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMTransformSetProfile(&pNvxParser->oBase, pProf);
            break;
        }
        case NVX_IndexConfigUlpMode:
        {
            return OMX_ErrorBadParameter;
        }
        case NVX_IndexConfigFileCacheSize:
        {
            NVX_CONFIG_FILECACHESIZE *pProf = (NVX_CONFIG_FILECACHESIZE *)pComponentConfigStructure;
            NvxNvMMTransformSetFileCacheSize(&pNvxParser->oBase, pProf->nFileCacheSize);
            break;
        }
        case NVX_IndexConfigDisableBuffConfig:
            {
                NvMMAttribute_DisableBuffConfigFlag oflag;
                NvOsMemset(&oflag, 0, sizeof(NvMMAttribute_DisableBuffConfigFlag));
                oflag.bDisableBuffConfig = NV_TRUE;
                pNvxParser->oBase.hBlock->SetAttribute(
                                          pNvxParser->oBase.hBlock,
                                          NvMMAttribute_DisableBuffConfig,
                                          NvMMSetAttrFlag_Notification,
                                          sizeof(NvMMAttribute_DisableBuffConfigFlag),
                                          &oflag);
                NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);

            }
        break;
        case OMX_IndexConfigTimePosition:
        {
            OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp;
            NvMMAttrib_ParsePosition oPos;

            NvOsMemset(&oPos, 0, sizeof(NvMMAttrib_ParsePosition));
            pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;

            if (!pNvxParser->bInitialized)
                return OMX_ErrorNotReady;

            oPos.Position = pTimeStamp->nTimestamp * 10; // to 100ns
            pNvxParser->oBase.hBlock->SetAttribute(pNvxParser->oBase.hBlock,
                    NvMMAttribute_Position, NvMMSetAttrFlag_Notification,
                    sizeof(NvMMAttrib_ParsePosition), &oPos);

            NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);

            pNvxParser->oBase.hBlock->GetAttribute(pNvxParser->oBase.hBlock,
                    NvMMAttribute_Position, sizeof(NvMMAttrib_ParsePosition), &oPos);
            pTimeStamp->nTimestamp = oPos.Position / 10; // to 1us
            break;
        }

        case NVX_IndexConfigMp3Enable:
        {

                NvMMAttribute_Mp3EnableConfigFlag oflag;
                NvOsMemset(&oflag, 0, sizeof(NvMMAttribute_Mp3EnableConfigFlag));
                oflag.bEnableMp3TSflag = NV_TRUE;
                pNvxParser->oBase.hBlock->SetAttribute(
                                          pNvxParser->oBase.hBlock,
                                          NvMMAttribute_Mp3EnableFlag,
                                          NvMMSetAttrFlag_Notification,
                                          sizeof(NvMMAttribute_Mp3EnableConfigFlag),
                                          &oflag);
                NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);
        }
        break;
        case OMX_IndexConfigTimeScale:
        {

            OMX_TIME_CONFIG_SCALETYPE *pScale;
            NvMMAttrib_ParseRate oRate;
            float fRate;

            NvOsMemset(&oRate, 0, sizeof(NvMMAttrib_ParseRate));
            pScale= (OMX_TIME_CONFIG_SCALETYPE *)pComponentConfigStructure;
            fRate= NvSFxFixed2Float(pScale->xScale);

            /* TODO do a correct mapping between OMX and NVMM */
            if((fRate > -32.0) && (fRate <= 32.0)) 
            {
                oRate.Rate = (NvS32)(fRate * 1000.0);
            }
            else if(fRate > 32.0)
            {
                oRate.Rate = 32000;
            }
            else
            {
                oRate.Rate = -32000;
            }

            pNvxParser->oBase.hBlock->SetAttribute(pNvxParser->oBase.hBlock,
                    NvMMAttribute_Rate, NvMMSetAttrFlag_Notification,
                    sizeof(NvMMAttribute_Rate), &oRate);

            NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);

            break;
        }
        case NvxIndexConfigTracklist:
        case NvxIndexConfigTracklistSize:
        case NvxIndexConfigTracklistTrack:
        case NvxIndexConfigTracklistDelete:
        case NvxIndexConfigTracklistCurrent:
            return OMX_ErrorBadParameter;
        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxParserWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxParserWorkerFunction\n"));

    *pbMoreWork = OMX_FALSE;

    return eError;
}

static OMX_ERRORTYPE NvxParserSetFilename(OMX_IN NvxComponent *pComponent)
{
    SNvxParserData *pNvxParser = 0;

    // For file set attributes
    NvMMBlockHandle pCurBlock;
    NvMMAttrib_ParseUri inputFile;
    NvMMAttrib_UserAgentStr userAgent;
    NvMMAttribute_UserAgentProfStr userAgentProf;

    NV_ASSERT(pComponent);
    NvOsMemset(&inputFile, 0, sizeof(NvMMAttrib_ParseUri));
    NvOsMemset(&userAgent, 0, sizeof(NvMMAttrib_UserAgentStr));
    NvOsMemset(&userAgentProf, 0, sizeof(NvMMAttribute_UserAgentProfStr));

    /* Get MP4 Decoder instance */
    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    if (!pNvxParser->bOpened)
        return OMX_ErrorBadParameter;

    pCurBlock = pNvxParser->oBase.hBlock;

    if (pNvxParser->szUserAgent)
    {
        //Set the FilenameAttributes
        userAgent.pUserAgentStr = pNvxParser->szUserAgent;
        userAgent.len = NvOsStrlen(pNvxParser->szUserAgent);
        pCurBlock->SetAttribute(pCurBlock, NvMMAttribute_UserAgent,
            NvMMSetAttrFlag_Notification, sizeof(NvMMAttrib_UserAgentStr), &userAgent);
        NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);
    }

    if (pNvxParser->szUserAgentProf)
    {
        //Set the User Agent Prof
        userAgentProf.pUAProfStr = pNvxParser->szUserAgentProf;
        userAgentProf.len = NvOsStrlen(pNvxParser->szUserAgentProf);
        pCurBlock->SetAttribute(pCurBlock, NvMMAttribute_UserAgentProf,
            NvMMSetAttrFlag_Notification, sizeof(NvMMAttribute_UserAgentProfStr), &userAgentProf);
        NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);
    }

    if (pNvxParser->szFilename)
    {
        //Set the FilenameAttributes
        inputFile.szURI = pNvxParser->szFilename;
        inputFile.eParserCore = pNvxParser->eFileType;

        pCurBlock->SetAttribute(pCurBlock, NvMMAttribute_FileName, NvMMSetAttrFlag_Notification, sizeof(NvMMAttrib_ParseUri), &inputFile);
        NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxParserProbeFile(OMX_IN NvxComponent *pComponent)
{
    SNvxParserData *pNvxParser = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    // For file set attributes
    NvMMBlockHandle pCurBlock;

    NV_ASSERT(pComponent);

    /* Get MP4 Decoder instance */
    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    if (pNvxParser->bOpened)
    {
        eError = NvxNvMMTransformClose(&pNvxParser->oBase);

        if (eError != OMX_ErrorNone)
            return eError;

        pNvxParser->bOpened = OMX_FALSE;
    }

    eError = NvxNvMMTransformOpen(&pNvxParser->oBase, NvMMBlockType_SuperParser,
                                  pNvxParser->sBlockName, 5, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    pCurBlock = pNvxParser->oBase.hBlock;

    if (pNvxParser->bLowMem)
    {
        NvMMAttrib_ReduceVideoBuffers reduce;
        NvOsMemset(&reduce, 0, sizeof(NvMMAttrib_ReduceVideoBuffers));
        reduce.bReduceVideoBufs = 1;

        pCurBlock->SetAttribute(pCurBlock, NvMMAttribute_ReduceVideoBuffers, NvMMSetAttrFlag_Notification, sizeof(NvMMAttrib_ReduceVideoBuffers), &reduce);
        NvOsSemaphoreWait(pNvxParser->oBase.SetAttrDoneSema);
    }


    pNvxParser->bOpened = OMX_TRUE;

    NvxParserSetFilename(pComponent);
    return NvxParserGetFileAttributes(pComponent);
}
    
static OMX_ERRORTYPE NvxParserGetFileAttributes(OMX_IN NvxComponent *pComponent)
{
    SNvxParserData *pNvxParser = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvU32 i;
    OMX_BOOL bFoundVid = OMX_FALSE, bFoundAud = OMX_FALSE;

    // For file set attributes
    NvMMBlockHandle pCurBlock;

    NV_ASSERT(pComponent);

    /* Get MP4 Decoder instance */
    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    if (!pNvxParser->bOpened)
        return NvxParserProbeFile(pComponent);

    pCurBlock = pNvxParser->oBase.hBlock;

    pCurBlock->GetAttribute(pCurBlock, NvMMAttribute_NoOfStreams, sizeof (NvU32), &pNvxParser->streamCount);
    if (0 == pNvxParser->streamCount)
    {
        // Stream does not exist. Close parser base
        eError = NvxNvMMTransformClose(&pNvxParser->oBase);
        pNvxParser->bOpened = OMX_FALSE;
        return OMX_ErrorBadParameter;
    }

    pNvxParser->pStreamInfo = (NvMMStreamInfo *)NvOsAlloc(pNvxParser->streamCount*sizeof(NvMMStreamInfo));
    NvOsMemset(pNvxParser->pStreamInfo, 0, pNvxParser->streamCount*sizeof(NvMMStreamInfo));
    if (!pNvxParser->pStreamInfo)
    {
        eError = NvxNvMMTransformClose(&pNvxParser->oBase);
        pNvxParser->bOpened = OMX_FALSE;
        return OMX_ErrorInsufficientResources;
    }

    pCurBlock->GetAttribute(pCurBlock,
                            NvMMAttribute_StreamInfo,
                            pNvxParser->streamCount*sizeof(NvMMStreamInfo),
                            &pNvxParser->pStreamInfo);

    pNvxParser->nDuration = 0;
    pNvxParser->eStreamType[0] = NvxStreamType_NONE;
    pNvxParser->eStreamType[1] = NvxStreamType_NONE;

    for (i = 0; i < pNvxParser->streamCount; i++)
    {
        if (NVMM_ISSTREAMVIDEO(pNvxParser->pStreamInfo[i].StreamType))
        {
            if (pNvxParser->port_video >= 0 && !bFoundVid)
            {
                ENvxStreamType *type = &(pNvxParser->eStreamType[pNvxParser->port_video]);
                OMX_VIDEO_CODINGTYPE *vt = &(pComponent->pPorts[pNvxParser->port_video].oPortDef.format.video.eCompressionFormat);

                switch (pNvxParser->pStreamInfo[i].StreamType)
                {
                    case NvMMStreamType_MPEG4:
                        *type = NvxStreamType_MPEG4;
                        *vt = OMX_VIDEO_CodingMPEG4;
                        break;
                    case NvMMStreamType_MPEG4Ext:
                        *type = NvxStreamType_MPEG4Ext;
                        *vt = OMX_VIDEO_CodingMPEG4;
                        break;
                    case NvMMStreamType_H264: 
                        *type = NvxStreamType_H264;
                        *vt = OMX_VIDEO_CodingAVC;
                        break;
                    case NvMMStreamType_H264Ext:
                        *type = NvxStreamType_H264Ext;
                        *vt = OMX_VIDEO_CodingAVC;
                        break;
                    case NvMMStreamType_H263: 
                        *type = NvxStreamType_H263;
                        *vt = OMX_VIDEO_CodingH263;
                        break;
                    case NvMMStreamType_WMV: 
                        *type = NvxStreamType_WMV;
                        *vt = OMX_VIDEO_CodingWMV;
                        break;
                    case NvMMStreamType_JPG: 
                        *type = NvxStreamType_JPG;
                        *vt = OMX_VIDEO_CodingMJPEG;
                        break;
                    case NvMMStreamType_MPEG2V: 
                        *type = NvxStreamType_MPEG2V;
                        *vt = OMX_VIDEO_CodingMPEG2;
                        break;
                    case NvMMStreamType_WMV7: 
                        *type = NvxStreamType_WMV7;
                        *vt = OMX_VIDEO_CodingWMV;
                        break;
                    case NvMMStreamType_WMV8: 
                        *type = NvxStreamType_WMV8;
                        *vt = OMX_VIDEO_CodingWMV;
                        break;
                    case NvMMStreamType_MS_MPG4: 
                        *type = NvxStreamType_MS_MPG4;
                        *vt = OMX_VIDEO_CodingMPEG4;
                        break;
                    case NvMMStreamType_MJPEG: 
                        *type = NvxStreamType_MJPEG;
                        *vt = OMX_VIDEO_CodingMJPEG;
                        break;
                    default: goto loopend;
                }

                pNvxParser->nPortToStreamIndexMap[pNvxParser->port_video] = i;
                if (pNvxParser->pStreamInfo[i].TotalTime > 0 &&
                    (OMX_TICKS)(pNvxParser->pStreamInfo[i].TotalTime / 10) > pNvxParser->nDuration)
                    pNvxParser->nDuration = pNvxParser->pStreamInfo[i].TotalTime / 10;

                pComponent->pPorts[pNvxParser->port_video].oPortDef.format.video.nFrameWidth = pNvxParser->pStreamInfo[i].NvMMStream_Props.VideoProps.Width;
                pComponent->pPorts[pNvxParser->port_video].oPortDef.format.video.nFrameHeight = pNvxParser->pStreamInfo[i].NvMMStream_Props.VideoProps.Height;
                pComponent->pPorts[pNvxParser->port_video].oPortDef.format.video.nBitrate = pNvxParser->pStreamInfo[i].NvMMStream_Props.VideoProps.VideoBitRate;
                pComponent->pPorts[pNvxParser->port_video].oPortDef.format.video.xFramerate = pNvxParser->pStreamInfo[i].NvMMStream_Props.VideoProps.Fps;

                bFoundVid = OMX_TRUE;
            }
        }

        if (NVMM_ISSTREAMAUDIO(pNvxParser->pStreamInfo[i].StreamType))
        {
            if (pNvxParser->port_audio >= 0 && !bFoundAud)
            {
                ENvxStreamType *type = &(pNvxParser->eStreamType[pNvxParser->port_audio]);
                OMX_AUDIO_CODINGTYPE *ac = &(pComponent->pPorts[pNvxParser->port_audio].oPortDef.format.audio.eEncoding);

                switch (pNvxParser->pStreamInfo[i].StreamType)
                {
                    case NvMMStreamType_MP3: 
                        *type = NvxStreamType_MP3;
                        *ac = OMX_AUDIO_CodingMP3;
                        break;
                    case NvMMStreamType_MP2: 
                        *type = NvxStreamType_MP2;
                        *ac = OMX_AUDIO_CodingMP3;
                        break;
                    case NvMMStreamType_WAV: 
                        *type = NvxStreamType_WAV;
                        *ac = OMX_AUDIO_CodingPCM;
                        break;
                    case NvMMStreamType_AAC: 
                        *type = NvxStreamType_AAC;
                        *ac = OMX_AUDIO_CodingAAC;
                        break;
                    case NvMMStreamType_WMA: 
                        *type = NvxStreamType_WMA;
                        *ac = OMX_AUDIO_CodingWMA;
                        break;
                    case NvMMStreamType_WMAPro:
                        *type = NvxStreamType_WMAPro;
                        *ac = OMX_AUDIO_CodingWMA;
                        break;
                    case NvMMStreamType_WMALSL:
                        *type = NvxStreamType_WMALossless;
                        *ac = OMX_AUDIO_CodingWMA;
                        break;
                    case NvMMStreamType_WMAV:
                        *type = NvxStreamType_WMAVoice;
                        *ac = OMX_AUDIO_CodingWMA;
                        break;
                    case NvMMStreamType_WAMR: 
                        *type = NvxStreamType_AMRWB;
                        *ac = OMX_AUDIO_CodingAMR; 
                        break;
                    case NvMMStreamType_NAMR: 
                        *type = NvxStreamType_AMRNB;
                        *ac = OMX_AUDIO_CodingAMR;
                        break;
                    case NvMMStreamType_AACSBR:
                        *type = NvxStreamType_AACSBR;
                        *ac = OMX_AUDIO_CodingAAC;
                        break;
                    case NvMMStreamType_Vorbis:
                        *type = NvxStreamType_VORBIS;
                        *ac = OMX_AUDIO_CodingVORBIS;
                        break;
                    case NvMMStreamType_BSAC:
                        *type = NvxStreamType_BSAC;
                        *ac = OMX_AUDIO_CodingAAC;
                        break;
                    case NvMMStreamType_AC3:
                        *type = NvxStreamType_AC3;
                        break;
                    case NvMMStreamType_ADPCM:
                        *type = NvxStreamType_ADPCM;
                        *ac = OMX_AUDIO_CodingADPCM;
                        break;
                    case NvMMStreamType_QCELP:
                        *type = NvxStreamType_QCELP;
                        *ac = OMX_AUDIO_CodingQCELP13;
                        break;
                    case NvMMStreamType_EVRC:
                        *type = NvxStreamType_EVRC;
                        *ac = OMX_AUDIO_CodingEVRC;
                        break;
                    default: goto loopend;
                }

                pNvxParser->nPortToStreamIndexMap[pNvxParser->port_audio] = i;
                if (pNvxParser->pStreamInfo[i].TotalTime > 0 &&
                    (OMX_TICKS)(pNvxParser->pStreamInfo[i].TotalTime / 10) > pNvxParser->nDuration)
                    pNvxParser->nDuration = pNvxParser->pStreamInfo[i].TotalTime / 10;

                bFoundAud = OMX_TRUE;
            }
        }
loopend: ;
    }

    pNvxParser->bOpened = OMX_TRUE;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxParserAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxParserData *pNvxParser = 0;
    NvU32 i;
    OMX_BOOL bFoundVid = OMX_FALSE, bFoundAud = OMX_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxParserAcquireResources\n"));

    /* Get MP4 Decoder instance */
    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    if (!pNvxParser->bOpened)
        return OMX_ErrorBadParameter;

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxParserAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    for (i = 0; i < pNvxParser->streamCount; i++)
    {
        if (NVMM_ISSTREAMVIDEO(pNvxParser->pStreamInfo[i].StreamType))
        {
            if (pNvxParser->port_video >= 0 &&
                pComponent->pPorts[pNvxParser->port_video].oPortDef.bEnabled &&
                !bFoundVid)
            {
                pNvxParser->nPortToStreamIndexMap[pNvxParser->port_video] = i;
                eError = NvxNvMMTransformSetupOutput(&pNvxParser->oBase,
                                                     i,
                                                     &pComponent->pPorts[pNvxParser->port_video],
                                                     0, //NO INPUT PORT!
                                                     MAX_OUTPUT_BUFFERS,
                                                     BIG_OUTPUT_BUFFERSIZE);
                if (eError != OMX_ErrorNone)
                    return eError;

                bFoundVid = OMX_TRUE;
            }
        }

        if (NVMM_ISSTREAMAUDIO(pNvxParser->pStreamInfo[i].StreamType))
        {
            if (pNvxParser->port_audio >= 0 &&
                pComponent->pPorts[pNvxParser->port_audio].oPortDef.bEnabled &&
                !bFoundAud)
            {
                pNvxParser->nPortToStreamIndexMap[pNvxParser->port_audio] = i;
                eError = NvxNvMMTransformSetupOutput(&pNvxParser->oBase,
                                                     i,
                                                     &pComponent->pPorts[pNvxParser->port_audio],
                                                     0, //NO_INPUT_PORT
                                                     MAX_OUTPUT_BUFFERS,
                                                     MIN_OUTPUT_BUFFERSIZE);
                if (eError != OMX_ErrorNone)
                    return eError;

                bFoundAud = OMX_TRUE;
            }
        }
    }

    pNvxParser->bInitialized = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxParserReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxParserData *pNvxParser = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxParserReleaseResources\n"));

    /* Get MP4 Decoder instance */
    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    if (!pNvxParser->bInitialized && !pNvxParser->bOpened)
        return OMX_ErrorNone;

    eError = NvxNvMMTransformClose(&pNvxParser->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxParser->bInitialized = OMX_FALSE;
    pNvxParser->bOpened = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxParserFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxParserData *pNvxParser = 0;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxParserFlush\n"));

    pNvxParser = (SNvxParserData *)pComponent->pComponentData;
    NV_ASSERT(pNvxParser);

    return NvxNvMMTransformFlush(&pNvxParser->oBase, nPort);
}

static OMX_ERRORTYPE NvxParserPreChangeState(NvxComponent *pNvComp, OMX_STATETYPE oNewState)
{
    SNvxParserData *pNvxParser = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxParserPreChangeState\n"));

    pNvxParser = (SNvxParserData *)pNvComp->pComponentData;
    if (!pNvxParser->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMTransformPreChangeState(&pNvxParser->oBase, oNewState, pNvComp->eState);
}

static OMX_ERRORTYPE NvxParserFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxParserData *pNvxParser = 0;
    OMX_U32 nStreamIndex;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxParserFillThisBuffer\n"));

    pNvxParser = (SNvxParserData *)pNvComp->pComponentData;
    if (!pNvxParser->bInitialized)
        return OMX_ErrorNone;

    nStreamIndex = pNvxParser->nPortToStreamIndexMap[pBufferHdr->nOutputPortIndex];
    return NvxNvMMTransformFillThisBuffer(&pNvxParser->oBase, pBufferHdr, nStreamIndex);
}


/*****************************************************************************/

static OMX_ERRORTYPE NvxCommonParserInit(OMX_HANDLETYPE hComponent, int oType, const char *sBlockName, NvMMParserCoreType eFileType)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxParserData *data = NULL;
    int num_ports = (oType == TYPE_AUDIO) ? 1 : 2;

    eError = NvxComponentCreate(hComponent, num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_FILE_READER,"ERROR:NvxCommonParserInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_FILE_READER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxParserData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxParserData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxParserData));

    data->port_video = 0;
    data->port_audio = 1;
    data->bLowMem = OMX_TRUE;
    data->oType = oType;
    data->eFileType = eFileType;

    if (oType == TYPE_AUDIO)
    {
        data->port_video = -1;
        data->port_audio = 0;
    }

    data->sBlockName = sBlockName;
    data->oBase.enableUlpMode = OMX_TRUE;

    pNvComp->DeInit             = NvxParserDeInit;
    pNvComp->GetParameter       = NvxParserGetParameter;
    pNvComp->SetParameter       = NvxParserSetParameter;
    pNvComp->GetConfig          = NvxParserGetConfig;
    pNvComp->SetConfig          = NvxParserSetConfig;
    pNvComp->WorkerFunction     = NvxParserWorkerFunction;
    pNvComp->AcquireResources   = NvxParserAcquireResources;
    pNvComp->ReleaseResources   = NvxParserReleaseResources;
    pNvComp->FillThisBufferCB   = NvxParserFillThisBuffer;
    pNvComp->PreChangeState     = NvxParserPreChangeState;
    pNvComp->Flush              = NvxParserFlush;

    if (data->port_video >= 0)
    {
        NvxPortInitVideo(&pNvComp->pPorts[data->port_video], OMX_DirOutput,
                         MAX_OUTPUT_BUFFERS, SMALL_OUTPUT_BUFFERSIZE,
                         OMX_VIDEO_CodingAutoDetect);
        NvxPortSetNonTunneledSize(&pNvComp->pPorts[data->port_video], BIG_OUTPUT_BUFFERSIZE);
        pNvComp->pPorts[data->port_video].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
        pNvComp->pPorts[data->port_video].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;
    }

    if (data->port_audio >= 0)
    {
       NvxPortInitAudio(&pNvComp->pPorts[data->port_audio], OMX_DirOutput,
                        MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE,
                        OMX_AUDIO_CodingAutoDetect);
       pNvComp->pPorts[data->port_audio].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;
    }

    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxAsfParserInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAsfParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "BlockASFParser", NvMMParserCoreType_AsfParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.asf.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.asf";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxMkvParserInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMkvParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "BlockMKVParser", NvMMParserCoreType_MkvParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.mkv.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.mkv";
    pNvComp->nComponentRoles = 1;

    return eError;
}
OMX_ERRORTYPE Nvx3gpParserInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+Nvx3gpParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "Block3GPParser", NvMMParserCoreType_3gppParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.mp4.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.3gp";
    pNvComp->nComponentRoles = 1;

    return eError;
}


OMX_ERRORTYPE NvxMpsParserInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMpsParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "BlockMPSParser", NvMMParserCoreType_MpsParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.mps.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.mps";
    pNvComp->nComponentRoles = 1;
    return eError;
}


OMX_ERRORTYPE NvxAviParserInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAviParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "BlockAVIParser", NvMMParserCoreType_AviParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.avi.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.avi";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxFlvParserInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxFlvParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "BlockFLVParser", NvMMParserCoreType_FlvParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.flv.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.flv";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxMp3FileReaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMp3ParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_AUDIO, "BlockMP3Parser", NvMMParserCoreType_Mp3Parser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.mp3.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.mp3";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxOggParserInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxOggParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_AUDIO, "BlockOGGParser", NvMMParserCoreType_OggParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.ogg.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.ogg";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxWavParserInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxWavParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_AUDIO, "BlockWAVParser", NvMMParserCoreType_WavParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.wav.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.wav";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxAacParserInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAacParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_AUDIO, "BlockAACParser", NvMMParserCoreType_AacParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.aac.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.aac";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxAmrParserInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAmrParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_AUDIO, "BlockAMRParser", NvMMParserCoreType_AmrParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.amr.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.amr";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxAwbParserInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAwbParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_AUDIO, "BlockAMRParser", NvMMParserCoreType_AmrParser);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.awb.read";
    pNvComp->sComponentRoles[0] = "container_demuxer.awb";
    pNvComp->nComponentRoles = 1;

    return eError;
}

OMX_ERRORTYPE NvxSuperParserInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxSuperParserInit\n"));

    eError = NvxCommonParserInit(hComponent, TYPE_SUPER, "SuperParser", NvMMParserCoreType_UnKnown);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxParserInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.reader";
    pNvComp->sComponentRoles[0] = "container_demuxer.all";
    pNvComp->nComponentRoles = 1;

    return eError;
}


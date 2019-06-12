/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvmm.h"
#include "nvmm_event.h"
#include "nvrm_transport.h"
#include "nvmm_block.h"
#include "nvmm_queue.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvmm_debug.h"
#include "nvmm_oggparser_core.h"
#include "nvmm_contentpipe.h"
#include "nvmm_oggparser.h"
#include "nvmm_ogg.h"
#include "nvmm_oggwrapper.h"
#include "nvmm_oggprototypes.h"

#define READ_INPUT_BUFFER_SIZE         4*1024
#define OGG_DEC_MAX_INPUT_BUFFER_SIZE       4*1024
#define OGG_DEC_MIN_INPUT_BUFFER_SIZE       4*1024

/** Context for ogg parser block.

    First member of this struct must be the NvMMBlockContext base class,
    so base class functions (NvMMBlockXXX()) can downcast a RefBlockContext*
    to NvMMBlock*.

    A custom parser block would add implementation specific members to
    this structure.
*/
typedef struct
{
    NvString szFilename;
    CPhandle cphandle;
    CP_PIPETYPE_EXTENDED *pPipe;
    NvOggParser *pOggReaderHandle;
    NvBool   bSentEOS;
    NvU64 ParserTimeStamp;
    NvS32 updateTimeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvU32 numberOfTracks;
    // Indicates file reached to EOF while seeking
    NvBool bFileReachedEOF;
} NvMMOggParserCoreContext;


// Prototypes for Ogg implementation of parser functions
NvError OggCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError OggCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvU32* TotalTime);
NvError OggCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError OggCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate);
NvS32   OggCoreParserGetRate(NvMMParserCoreHandle hParserCore);
NvError OggCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp);
NvError OggCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp);
NvError OggCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError OggCoreParserClose(NvMMParserCoreHandle hParserCore);
static NvError GetOggMetaData(NvOggParser *pState);
void    OggCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);

NvError 
OggCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore, 
    NvU32 streamIndex, 
    NvMMBuffer *pBuffer, 
    NvU32 *size, 
    NvBool *pMoreWorkPending);

NvBool  
OggCoreParserGetBufferRequirements(
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq);

NvError 
OggCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore, 
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute);


NvError 
NvMMCreateOggCoreParser(
    NvMMParserCoreHandle *hParserCore, 
    NvMMParserCoreCreationParameters *pParams)
{
    NvMMParserCore *pParserCore        = NULL; 

    pParserCore = NvOsAlloc(sizeof(NvMMParserCore));
    if (pParserCore == NULL)
    {
        *hParserCore = NULL;
        return NvError_InsufficientMemory;
    }
    *hParserCore = pParserCore;
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = OggCoreParserOpen;
    pParserCore->GetNumberOfStreams = OggCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = OggCoreParserGetStreamInfo;
    pParserCore->SetRate            = OggCoreParserSetRate;
    pParserCore->GetRate            = OggCoreParserGetRate;
    pParserCore->SetPosition        = OggCoreParserSetPosition;
    pParserCore->GetPosition        = OggCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = OggCoreParserGetNextWorkUnit;
    pParserCore->Close              = OggCoreParserClose;
    pParserCore->GetMaxOffsets      = OggCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = OggCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = OggCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_OggParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

    return NvSuccess;
}

void NvMMDestroyOggCoreParser(NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}

NvError OggCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError status = NvSuccess;
    NvU32 StringLength = 0;
    NvMMOggParserCoreContext *pContext = NULL;

    if ((pContext = NvOsAlloc(sizeof(NvMMOggParserCoreContext))) == NULL)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pContext, 0, sizeof(NvMMOggParserCoreContext));

    StringLength = NvOsStrlen(pFilename) + 1;
    pContext->szFilename = NvOsAlloc(StringLength);
    if (!pContext->szFilename)
    {        
        status =  NvError_InsufficientMemory;
        goto NvMMOggParserOpen_err;
    }
    NvOsStrncpy(pContext->szFilename, pFilename, StringLength);

    pContext->updateTimeStamp = 0;

    if((hParserCore->MinCacheSize == 0) && (hParserCore->MaxCacheSize == 0) && (hParserCore->SpareCacheSize == 0))
    {
        hParserCore->bUsingCachedCP = NV_FALSE;   
        hParserCore->UlpKpiMode     = NV_FALSE;
        hParserCore->bEnableUlpMode = NV_FALSE;
    }
    else
    {
        hParserCore->bUsingCachedCP = NV_TRUE;
    } 

     NvmmGetFileContentPipe(&pContext->pPipe);

    if (pContext->pPipe->cpipe.Open(&pContext->cphandle,pContext->szFilename,CP_AccessRead) != NvSuccess)
    {
        status =  NvError_ParserOpenFailure;
        goto NvMMOggParserOpen_err;
    }

    if (hParserCore->bUsingCachedCP)
    {
        status = pContext->pPipe->InitializeCP(pContext->cphandle, 
                                               hParserCore->MinCacheSize,
                                               hParserCore->MaxCacheSize,
                                               hParserCore->SpareCacheSize,
                                               (CPuint64 *)&hParserCore->pActualCacheSize);
        if (status != NvSuccess)
        {
            goto NvMMOggParserOpen_err;
        }
    }

    pContext->pOggReaderHandle = NvGetOggReaderHandle(pContext->cphandle, pContext->pPipe);
    if (pContext->pOggReaderHandle == NULL)
    {
        status =  NvError_InsufficientMemory;
        goto NvMMOggParserOpen_err;
    }

    status = NvParseOggTrackInfo(pContext->pOggReaderHandle);
    if(status != NvSuccess)
    {        
        goto NvMMOggParserOpen_err;
    }

    status = GetOggMetaData(pContext->pOggReaderHandle);    
    if(status != NvSuccess)
    {
        status = NvError_ParserFailure;
        goto NvMMOggParserOpen_err;
    }    
    
    pContext->rate.Rate  = 1000;

    hParserCore->pPipe    = pContext->pPipe;
    hParserCore->cphandle = pContext->cphandle;
    hParserCore->pContext = (void*)pContext;

    return status;

NvMMOggParserOpen_err:

    if(pContext)
    {
        if(pContext->szFilename)
        {
            NvOsFree(pContext->szFilename);
            pContext->szFilename = NULL;
        }
        if(pContext->cphandle)
        {
            if( pContext->pPipe->cpipe.Close(pContext->cphandle) != NvSuccess)
                status = NvError_ParserCloseFailure;

            pContext->cphandle = 0;
        }
        if (pContext->pOggReaderHandle)
        {
            NvReleaseOggReaderHandle(pContext->pOggReaderHandle);
            NvOsFree(pContext->pOggReaderHandle);
            pContext->pOggReaderHandle = NULL;
        }
        NvOsFree(pContext);
    }
    return status;
}

NvError OggCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{
    NvError status = NvSuccess;
    NvMMOggParserCoreContext* pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;
    if(pContext == NULL)
    {
        *streams = 0;
        return NvError_ParserFailure;
    }
    *streams =   pContext->pOggReaderHandle->numberOfTracks;
    return status;    
}

NvError OggCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{
    NvError status = NvSuccess;
    NvS32 i,TotalTracks;
    NvU32 TotalTrackTime = 0;
    NvU64 TotalTrackDuration = 0;
    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;

    if (pContext == NULL)
    {
        return NvError_ParserFailure;
    }

    if(pContext->pOggReaderHandle != NULL)
    {
        TotalTracks = pContext->pOggReaderHandle->numberOfTracks;
        OggCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
        TotalTrackDuration =((NvU64)TotalTrackTime * 10000000);
        
        for (i = 0; i < TotalTracks; i++)
        {
            pInfo[i]->StreamType                                = NvMMStreamType_OGG;
            pInfo[i]->BlockType                                 = NvMMBlockType_SuperParser;
            pInfo[i]->TotalTime                                 = TotalTrackDuration;            
            pInfo[i]->NvMMStream_Props.AudioProps.SampleRate    = pContext->pOggReaderHandle->pInfo->pTrackInfo->samplingFreq;
            pInfo[i]->NvMMStream_Props.AudioProps.BitRate       = pContext->pOggReaderHandle->pInfo->pTrackInfo->bitRate;
            pInfo[i]->NvMMStream_Props.AudioProps.NChannels     = pContext->pOggReaderHandle->pInfo->pTrackInfo->noOfChannels;
            pInfo[i]->NvMMStream_Props.AudioProps.BitsPerSample = pContext->pOggReaderHandle->pInfo->pTrackInfo->sampleSize;
        }
    }
    else
    {
        return NvError_ParserFailure;
    }
    return status;
}

NvError OggCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvU32* TotalTime)
{
    NvError status = NvSuccess;
    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;

     if (pContext == NULL)
     {
         return NvError_ParserFailure;
     }

    if(pContext->pOggReaderHandle != NULL)
        *TotalTime = pContext->pOggReaderHandle->pInfo->pTrackInfo->total_time;
    else
        return NvError_ParserFailure;

    return status;
}


NvError OggCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{
    NvError status = NvSuccess;
    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;

    if (pContext == NULL)
        return NvError_ParserFailure;

    pContext->rate.Rate = rate;

    return status;
}

NvS32 OggCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{
    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
        return NvError_ParserFailure;

    return (pContext->rate.Rate);
}

NvError OggCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvS64 mSec;
    NvU32 divFactor = 10000;
    NvU64 TotalTime = 0;
    NvU32 TotalTrackTime = 0;

    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;

    if (pContext == NULL)
    {
        return NvError_ParserFailure;
    }

    mSec = (NvU32)((*timestamp)/divFactor);

    status = OggCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    if (status != NvSuccess)
    {
        return NvError_ParserFailure;
    }

    TotalTime = ((NvU64)TotalTrackTime * 10000000);

    if(*timestamp > TotalTime)
    {
        pContext->bFileReachedEOF = NV_TRUE;
        // Update the parse position with TotalTime of the track
        pContext->position.Position = TotalTime;
        return NvSuccess;
    }
    else if (*timestamp == 0)
    {
        // CEPlayer will call seek(0) for repeat mode hence reset the flag
        pContext->bFileReachedEOF = NV_FALSE;
        pContext->pOggReaderHandle->bOggHeader = NV_TRUE;      
        if (pContext->pOggReaderHandle->OggInfo.pOggParams)
        {
            NvOGGParserDeInit((void**)&pContext->pOggReaderHandle->OggInfo.pOggParams);
            pContext->pOggReaderHandle->OggInfo.pOggParams = NULL;
        }        
        status = NvParseOggTrackInfo(pContext->pOggReaderHandle);    
        if(status != NvSuccess)
        {        
            return status;
        }
    } 

    if (*timestamp != 0)
        status = NvOggSeekToTime(pContext->pOggReaderHandle,mSec);

    if (status != NvSuccess)
    {
        return NvError_ParserFailure; 
    }

    pContext->ParserTimeStamp = *timestamp;
    pContext->position.Position = *timestamp;
    pContext->updateTimeStamp = 1;

    return status;
}

NvError OggCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;

    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;

    if (pContext == NULL)
        return NvError_ParserFailure;

    *(timestamp) = pContext->position.Position;
    return status;
}

NvError 
OggCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore, 
    NvU32 streamIndex, 
    NvMMBuffer *pBuffer, 
    NvU32 *size, 
    NvBool *pMoreWorkPending)
{
    NvError status = NvSuccess;
    NvU32 askBytes = 0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;
    OggVorbis_File *pOGGParams = NULL;
    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;
    CPuint64 askBytes64 = 0;

    if (pContext == NULL)
        return NvError_ParserFailure;

    if (pBuffer == NULL)
        return NvError_ParserFailure;

    pOGGParams = (OggVorbis_File *)(pBuffer->Payload.Ref.pMem);
    // Initially pMoreWorkPending is set to FALSE
    *pMoreWorkPending = NV_FALSE; 

    if (pContext->bFileReachedEOF)
    {
        return NvError_ParserEndOfStream;
    }

    if (pBuffer)
    {
        pBuffer->Payload.Ref.startOfValidData = 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = *size;

        // Set ask bytes to the preset buffer size
        askBytes = READ_INPUT_BUFFER_SIZE;

        if( pBuffer->Payload.Ref.sizeOfBufferInBytes < askBytes)
        {
            // returning required minimum size
            *size = askBytes;
            return NvError_InSufficientBufferSize;
        }

        if (pContext->pOggReaderHandle->bOggHeader == NV_TRUE)
        {
            //This code is for Parsers providing the expected OGG structure info  for the decoder.
            NvOsMemset(pOGGParams,0,sizeof(OggVorbis_File));
            NvOsMemcpy(pOGGParams, pContext->pOggReaderHandle->OggInfo.pOggParams, sizeof(OggVorbis_File));
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = sizeof(OggVorbis_File);
            pBuffer->Payload.Ref.startOfValidData = 0;
            // Update the size with actual max frame bytes read for this work unit..
            *size = sizeof(OggVorbis_File);
            pContext->pOggReaderHandle->bOggHeader = NV_FALSE;
            pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;          
            
            return NvSuccess;
        }

        status = pContext->pPipe->cpipe.CheckAvailableBytes(pContext->cphandle, askBytes, &eResult);
        // Check if we reached end of stream.
        if (eResult == CP_CheckBytesAtEndOfStream)
        {
            return NvError_ParserEndOfStream;
        }

        if(eResult == CP_CheckBytesInsufficientBytes)
        {
            status = pContext->pPipe->GetAvailableBytes(pContext->cphandle, &askBytes64);
             if (status != NvSuccess) return NvError_ParserFailure;
             NV_ASSERT(askBytes64 <= (CPuint64)0xffffffff);
             askBytes = (NvU32)askBytes64;
        }

        if((eResult == CP_CheckBytesInsufficientBytes) || (eResult == CP_CheckBytesOk)) 
        {
            pBuffer->Payload.Ref.startOfValidData = 0;
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
            status = pContext->pPipe->cpipe.Read(pContext->cphandle, (CPbyte*)pBuffer->Payload.Ref.pMem, askBytes);
            if(status != NvSuccess)
            {
                 if(status == NvError_EndOfFile)
                 {
                     return NvError_ParserEndOfStream;
                 }
                 else
                 {
                     return NvError_ParserReadFailure;
                 }
            }
            pBuffer->Payload.Ref.sizeOfValidDataInBytes= askBytes;

            if (pContext->updateTimeStamp == 1)
            {
                pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp;           
                pContext->updateTimeStamp = 0;
                pContext->ParserTimeStamp = 0;
            }
            else
                pBuffer->PayloadInfo.TimeStamp = -1; 
            
            *size = askBytes;
        }   
    }    

    return status;
}

NvError OggCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError status = NvSuccess;
    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;

    if (pContext)
    {
        if (pContext->pOggReaderHandle)
        {
            NvReleaseOggReaderHandle(pContext->pOggReaderHandle);
            NvOsFree(pContext->pOggReaderHandle);
            pContext->pOggReaderHandle = NULL;
        }

        if (pContext->cphandle)
        {
            if (pContext->pPipe->cpipe.Close(pContext->cphandle) != NvSuccess)
            {
                status = NvError_ParserCloseFailure;
            }
            pContext->cphandle = 0;
        }

        if (pContext->szFilename)
        {
            NvOsFree(pContext->szFilename);
        }
        NvOsFree(pContext);
    }
    else
    {
        return NvError_ParserFailure;
    }
    hParserCore->pContext = NULL;
    return status;
}

NvBool 
OggCoreParserGetBufferRequirements(
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq)
{
    if (Retry > 0)
    {
        return NV_FALSE;
    }

    NvOsMemset (pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));

    pBufReq->structSize    = sizeof(NvMMNewBufferRequirementsInfo);
    pBufReq->event         = NvMMEvent_NewBufferRequirements;
    pBufReq->minBuffers    = 4;
    pBufReq->maxBuffers    = NVMMSTREAM_MAXBUFFERS;
    pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
    pBufReq->byteAlignment = 4;

    pBufReq->minBufferSize = OGG_DEC_MIN_INPUT_BUFFER_SIZE;
    pBufReq->maxBufferSize = OGG_DEC_MAX_INPUT_BUFFER_SIZE;

    return NV_TRUE;
}

NvError GetOggMetaData(NvOggParser *pState)
{
    NvError status = NvSuccess;  

    status = NvParseOggMetaData(pState);
        
    return status;
}

NvError 
OggCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore, 
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute)
{
    NvError status = NvSuccess;
    NvOggMetaDataParams *pData; 
    NvU32 size = 0;

    NvMMOggParserCoreContext *pContext = (NvMMOggParserCoreContext *)hParserCore->pContext;
   
    switch (AttributeType)
    {
        case NvMMAttribute_Metadata:
        {            
            NvMMMetaDataInfo *md = (NvMMMetaDataInfo *)pAttribute;
            NvOggMetaData *comment = &(pContext->pOggReaderHandle->pOggMetaData);            
            NvMMMetaDataInfo *src = NULL;
            pData = &(pContext->pOggReaderHandle->pData);            
            switch (md->eMetadataType)
            {
                case NvMMMetaDataInfo_Artist:
                    if(pData->pArtistMetaData == 1)
                        src = &(comment->oArtistMetaData);
                    break;  
                case NvMMMetaDataInfo_Album:     
                    if(pData->pAlbumMetaData== 1)
                        src = &(comment->oAlbumMetaData);        
                    break;
                case NvMMMetaDataInfo_Genre:      
                    if(pData->pGenreMetaData== 1)
                        src = &(comment->oGenreMetaData);        
                    break;
                case NvMMMetaDataInfo_Title:    
                    if(pData->pTitleMetaData== 1)
                        src = &(comment->oTitleMetaData);        
                    break;
                case NvMMMetaDataInfo_Year:
                    if(pData->pYearMetaData== 1)
                        src = &(comment->oYearMetaData);  
                    break;
                case NvMMMetaDataInfo_TrackNumber:
                    if(pData->pTrackMetaData== 1)
                        src = &(comment->oTrackNoMetaData); 
                    break;
                case NvMMMetaDataInfo_Encoded:
                    if(pData->pEncodedMetaData== 1)
                        src = &(comment->oEncodedMetaData); 
                    break;
                case NvMMMetaDataInfo_Comment:
                    if(pData->pCommentMetaData== 1)
                        src = &(comment->oCommentMetaData); 
                    break;
                case NvMMMetaDataInfo_Composer:
                    if(pData->pComposerMetaData== 1)
                        src = &(comment->oComposerMetaData); 
                    break;
                case NvMMMetaDataInfo_Publisher:
                    if(pData->pPublisherMetaData== 1)
                        src = &(comment->oPublisherMetaData); 
                    break;
                case NvMMMetaDataInfo_OrgArtist:
                    if(pData->pOrgArtistMetaData== 1)
                        src = &(comment->oOrgArtistMetaData); 
                    break;
                case NvMMMetaDataInfo_Copyright:
                    if(pData->pCopyRightMetaData== 1)
                        src = &(comment->oCopyRightMetaData); 
                    break;
                case NvMMMetaDataInfo_Url:
                    if(pData->pUrlMetaData== 1)
                        src = &(comment->oUrlMetaData); 
                    break;
                case NvMMMetaDataInfo_Bpm:
                    if(pData->pBpmMetaData== 1)
                        src = &(comment->oBpmMetaData);
                    break;
                case NvMMMetaDataInfo_AlbumArtist:
                    if(pData->pAlbumMetaData== 1)
                        src = &(comment->oAlbumArtistMetaData);
                    break;
                case NvMMMetaDataInfo_CoverArt:
                case NvMMMetaDataInfo_CoverArtURL:
                default:
                    return status;
            }
            
            if (!src)
            {  
                status = NvError_ParserFailure;
                return status;
             }    
           
            size = src->nBufferSize;            
            if (md->pClientBuffer == NULL || md->nBufferSize < size)
            {
                md->nBufferSize = size;
                return NvError_InSufficientBufferSize;
            }
            // Copy the MetaData
            NvOsMemcpy(md->pClientBuffer, src->pClientBuffer, size);
            md->nBufferSize = size;
            md->eEncodeType = src->eEncodeType;  
            break;
        }
        default:
            break;
    }   
   
    return status;
}


void OggCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
    *pMaxOffsets = 0;
}









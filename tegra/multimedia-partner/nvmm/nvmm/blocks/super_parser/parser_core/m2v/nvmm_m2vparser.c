
/**
* @nvmm_m2vparser.c
* @brief IMplementation of m2v parser Class.
*
* @b Description: Implementation of m2v parser public API's.
*
*/

/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nvmm_m2vparser.h"
#include "nvos.h"

enum
{
    MAX_NUM_SAMPLES = 1024
};


#define MPEG2V_FRAME_SIZE 310*1024   //720*576*3/4


NvM2vParser * NvGetM2vReaderHandle(CPhandle hContent, CP_PIPETYPE *pPipe )
{      
    NvM2vParser *pState;

    pState = (NvM2vParser *)NvOsAlloc(sizeof(NvM2vParser));
    if (pState == NULL)
    {           
        goto Exit;
    }
    NvOsMemset(pState,0,sizeof(NvM2vParser));

    pState->pInfo = (NvM2vStreamInfo*)NvOsAlloc(sizeof(NvM2vStreamInfo));
    if ((pState->pInfo) == NULL)
    {
        NvOsFree(pState);
        pState = NULL;
        goto Exit;
    }

    pState->pInfo->pTrackInfo = (NvM2vTrackInfo*)NvOsAlloc(sizeof(NvM2vTrackInfo));
    if ((pState->pInfo->pTrackInfo) == NULL)
    {
        NvOsFree(pState->pInfo);
        pState->pInfo = NULL;           
        NvOsFree(pState);    
        pState = NULL;
        goto Exit;
    }

    NvOsMemset(((pState)->pInfo)->pTrackInfo,0,sizeof(NvM2vTrackInfo));
    pState->pSrc = NvOsAlloc(MPEG2V_FRAME_SIZE);
    if (pState->pSrc  == NULL)
    {    
        NvOsFree(pState->pInfo->pTrackInfo);
        NvOsFree(pState->pInfo);
        pState->pInfo = NULL;           
        NvOsFree(pState);    
        pState = NULL;
        goto Exit;
    }
    
    pState->pPicListStart = NvOsAlloc(sizeof(PictureList));
    if (pState->pPicListStart  == NULL)
    {  
        NvOsFree(pState->pSrc);
        NvOsFree(pState->pInfo->pTrackInfo);
        NvOsFree(pState->pInfo);
        pState->pInfo = NULL;           
        NvOsFree(pState);    
        pState = NULL;
        goto Exit;
    }
    pState->pPicListStart->pNext = NULL;
    pState->pPicListRunning=pState->pPicListStart;
    pState->cphandle = hContent;
    pState->pPipe = pPipe; 
    pState->numberOfTracks = 1;
    pState->InitFlag = NV_TRUE;

Exit:    
    return pState;
}

void NvReleaseM2vReaderHandle(NvM2vParser * pState)
{
    if (pState)
    {  
        if(pState->pSrc)
            NvOsFree(pState->pSrc);
        if(pState->pInfo)
        {
            if (pState->pInfo->pTrackInfo != NULL)
            {
                NvOsFree(pState->pInfo->pTrackInfo);
                pState->pInfo->pTrackInfo = NULL;
            }
             pState->pPicListRunning = pState->pPicListStart;

             while ( pState->pPicListRunning != NULL)
             {
                 pState->pPicListStart = pState->pPicListRunning->pNext;
                 NvOsFree(pState->pPicListRunning);
                 pState->pPicListRunning = pState->pPicListStart;
             }
             NvOsFree(pState->pInfo);   
             pState->pInfo = NULL;
        }
    }   
}


NvError NvParseM2vTrackInfo(NvM2vParser *pState)
{
    NvError status = NvSuccess;   

#if  ENABLE_PRE_PARSING
#if ENABLE_PROFILING
    NvU32 StartTime, FinishTime;
    NvU32 TotalTimeParsing= 0;
#endif
    NvBool bSentEOS=NV_FALSE;
    PictureList* pPicListNew;
    NvU32 ulBytesRead;
    NvU32 last_packet_bytes = 0;
    NvU32 BytesAvailable=0;
    NvU32 pPosition = 0,endposition = 0;
    NvU32 PictureNum=0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;  
    pPicListNew= pState->pPicListStart;
#if ENABLE_PROFILING
    StartTime = NvOsGetTimeMS(); 
#endif

    NvOsDebugPrintf("m2v parser: started pre-parsing...\n");
    while(bSentEOS != NV_TRUE)
    {

ReadData:

        if(!pState->toggle)
        { 
            status = pState->pPipe->CheckAvailableBytes(pState->cphandle, (CPuint)(MPEG2V_FRAME_SIZE - pState->StartOffset),
                &eResult);
            if(eResult==CP_CheckBytesInsufficientBytes)
            { 
                status =pState->pPipe->GetPosition(pState->cphandle,(CPuint*) &pPosition);
                if(status != NvSuccess) return NvError_ParserFailure;
                status =pState->pPipe->SetPosition(pState->cphandle, 0, CP_OriginEnd) ;
                if(status != NvSuccess) return NvError_ParserFailure;
                status = pState->pPipe->GetPosition(pState->cphandle, (CPuint*)&endposition);
                if(status != NvSuccess) return NvError_ParserFailure;
                status =pState->pPipe->SetPosition(pState->cphandle, pPosition, CP_OriginBegin);
                if(status != NvSuccess) return NvError_ParserFailure;                
                status = pState->pPipe->Read(pState->cphandle,(CPbyte*)(pState->pSrc+ pState->StartOffset),
                    (CPuint)(endposition - pPosition));
                ulBytesRead = endposition - pPosition + pState->StartOffset;
                BytesAvailable=ulBytesRead;

            }
        }
        if( eResult == CP_CheckBytesOk && (!pState->toggle))
        {  
            status = pState->pPipe->Read(pState->cphandle,
                (CPbyte*)(pState->pSrc + pState->StartOffset), (CPuint)(MPEG2V_FRAME_SIZE - pState->StartOffset));
            BytesAvailable=MPEG2V_FRAME_SIZE;

        }
        {
            NvU32 bytes = 0;
            NvU8 *pbyte ;

            pState->toggle=1;
            pbyte= pState->pSrc+2+pState->FlushBytes;
            for (bytes = pState->FlushBytes; bytes+6 <= BytesAvailable; bytes++)
            {
                if (*(pbyte) == 0x00 && 
                    *(pbyte+1) == 0x00 && 
                    *(pbyte+2) == 0x01 &&
                    *(pbyte+3) == 0x00)
                {
                    last_packet_bytes = bytes+2-pState->FlushBytes;
                    break;
                }
                pbyte++;
            }
            if( (bytes+6 >= BytesAvailable) &&  (BytesAvailable == MPEG2V_FRAME_SIZE))
            { 
                NvOsMemcpy(pState->pSrc, pState->pSrc + pState->FlushBytes, MPEG2V_FRAME_SIZE - pState->FlushBytes);
                pState->StartOffset = MPEG2V_FRAME_SIZE - pState->FlushBytes;
                pState->FlushBytes = 0;
                pState->toggle=0;
                goto ReadData;

            }
            else
            {
                if( (bytes+6 >= BytesAvailable) && (BytesAvailable != MPEG2V_FRAME_SIZE))
                {
                    last_packet_bytes=BytesAvailable-pState->FlushBytes;
                    pPicListNew->PictureNum=PictureNum+1;
                    bSentEOS = NV_TRUE;
                }
                pState->FlushBytes += last_packet_bytes;
                ulBytesRead = last_packet_bytes;
                pPicListNew->PictureSize[PictureNum]=ulBytesRead;
                PictureNum++;
                if(PictureNum==MAX_PICTURE_LIST_NUM)
                {
                    pPicListNew->PictureNum=PictureNum;
                    pPicListNew->pNext = NvOsAlloc(sizeof(PictureList));
                    if( NULL ==  pPicListNew->pNext ) 
                    {
                        // in rare Occasion this should happen. Then simply decode what we have ! 
                        bSentEOS = NV_TRUE; // if this is set then also it should work. 
                        goto end_of_premature_pre_parsing;
                    }
                    pPicListNew=pPicListNew->pNext;
                    pPicListNew->pNext=NULL;
                    PictureNum=0;
                }
            }
        }
    }
end_of_premature_pre_parsing:

    status =pState->pPipe->SetPosition(pState->cphandle, 0, CP_OriginBegin);
    pState->toggle=0;
    pState->StartOffset=0;
    pState->FlushBytes=0;
#if ENABLE_PROFILING  
    FinishTime = NvOsGetTimeMS();
    TotalTimeParsing = FinishTime - StartTime;
    NvOsDebugPrintf("Time For Parsing Frame Lengths %d ms ",TotalTimeParsing);
#endif
    NvOsDebugPrintf("m2v parser: pre-parsing done\n");
    if(status != NvSuccess) return NvError_ParserFailure; 
#endif

    pState->pInfo->pTrackInfo->Width= 320;
    pState->pInfo->pTrackInfo->Height=240;
    pState->pInfo->pTrackInfo->Fps=30;
    pState->pInfo->pTrackInfo->total_time=100;
    pState->m_TotalRDB = 0;
    pState->m_CurrentTime = pState->m_PreviousTime = 0;
    pState->m_SeekSet = 0;
    NvM2vStreamInit(&(pState->stream));
    return status;
}

/**
* @brief              This function initializes the stream property structure to zero.
* @param[in]          stream  is the pointer to stream property structure
* @returns            void
*/
void NvM2vStreamInit(NvM2vStream *stream)
{
    stream->bNoMoreData = 0;
    stream->CurrentFrame = 0;
    stream->CurrentOffset = 0;
    stream->CurrentSize = 0;
    stream->FileSize = 0;
    stream->TotalSize = 0;
}

//TBD
/**
* @brief          This function gets the total file duration
* @returns        File Duration
*/
NvS64 NvM2vGetDuration(NvM2vParser *pState)
{  
    NvU32 Duration=0;
    return Duration;
}
//TBD
NvError NvM2vSeek(NvM2vParser *pState, NvS64 seekTime)
{
    NvError status = NvSuccess;  
    if(seekTime == 0)
    {
        status =pState->pPipe->SetPosition(pState->cphandle, 0, CP_OriginBegin);
        if (status != NvSuccess)
        {
            return NvError_ParserFailure;
        }
    }

    return status;
}
//TBD
NvError NvM2vSeekToTime(NvM2vParser *pState, NvS64 mSec)
{
    NvError status = NvSuccess;      

    status = NvM2vSeek(pState, mSec);

    return status;
}

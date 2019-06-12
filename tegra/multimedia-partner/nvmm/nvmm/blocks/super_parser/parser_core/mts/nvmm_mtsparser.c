
/**
* @nvmm_mtsparser.c
* @brief IMplementation of mts parser Class.
*
* @b Description: Implementation of mts parser public API's.
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

#include "nvmm_mtsparser.h"
#include "nvos.h"


enum
{
    MAX_NUM_SAMPLES = 1024
};


NvMtsParser * NvGetMtsReaderHandle(CPhandle hContent, CP_PIPETYPE *pPipe )
{      
    NvMtsParser *pState = NULL;

    pState = (NvMtsParser *)NvOsAlloc(sizeof(NvMtsParser));
    if (pState == NULL)
    {           
        goto Exit;
    }      

    pState->pInfo = (NvMtsStreamInfo*)NvOsAlloc(sizeof(NvMtsStreamInfo));
    if (pState->pInfo == NULL)
    {
       NvOsFree(pState);
       pState = NULL;
       goto Exit;
    }     

    pState->pInfo->pTrackInfo = (NvMtsTrackInfo*)NvOsAlloc(sizeof(NvMtsTrackInfo));
    if ((pState->pInfo->pTrackInfo) == NULL)
    {
       NvOsFree(pState->pInfo);
       pState->pInfo = NULL;           
       NvOsFree(pState);    
       pState = NULL;
       goto Exit;
    }      

    NvOsMemset(pState->pInfo->pTrackInfo,0,sizeof(NvMtsTrackInfo));    
    
    pState->cphandle = hContent;
    pState->pPipe = pPipe; 
    pState->numberOfTracks = 1;
    pState->InitFlag = NV_TRUE;


Exit:
    return pState;
}   

void NvReleaseMtsReaderHandle(NvMtsParser * pState)
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

            NvOsFree(pState->pInfo);   
            pState->pInfo = NULL;
        }
    }   
}

NvError NvParseMtsTrackInfo(NvMtsParser *pState)
{
    NvError status = NvSuccess;   


    
    return status;
}

/**
* @brief              This function initializes the stream property structure to zero.
* @param[in]          stream  is the pointer to stream property structure
* @returns            void
*/
void NvMtsStreamInit(NvMtsStream *stream)
{


    
}


/**
* @brief          This function gets the total file duration
* @returns        File Duration
*/
NvS64 NvMtsGetDuration(NvMtsParser *pState)
{  
    NvU32 Duration=0;
    
    return Duration;
}

NvError NvMtsSeek(NvMtsParser *pState, NvS64 seekTime)
{
    NvError status = NvSuccess;  
    
    return status;
}

NvError NvMtsSeekToTime(NvMtsParser *pState, NvS64 mSec)
{
    NvError status = NvSuccess;      

    

    return status;
}




















































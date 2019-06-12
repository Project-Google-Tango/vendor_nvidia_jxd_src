/**
 * @nvmm_oggparser.c
 * @brief IMplementation of Ogg parser Class.
 *
 * @b Description: Implementation of Ogg parser public API's.
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


#include "nvmm_oggparser.h"
#include "nvos.h"
#include "nvmm_oggwrapper.h"
#include "nvmm_oggprototypes.h"

#include <ctype.h>

#define MAX_THRESHOLD_TIME_FOR_CORRUPTED_FILES   10


NvOggParser * NvGetOggReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe )
{      
      NvOggParser *pState;

      pState = (NvOggParser *)NvOsAlloc(sizeof(NvOggParser));
      if (pState == NULL)
      {           
           goto Exit;
      }      

      pState->pInfo = (NvOggStreamInfo*)NvOsAlloc(sizeof(NvOggStreamInfo));
      if ((pState->pInfo) == NULL)
      {
           NvOsFree(pState);
           pState = NULL;
           goto Exit;
      }      

      pState->pInfo->pTrackInfo = (NvOggTrackInfo*)NvOsAlloc(sizeof(NvOggTrackInfo));
      if ((pState->pInfo->pTrackInfo) == NULL)
      {
           NvOsFree(pState->pInfo);
           pState->pInfo = NULL;           
           NvOsFree(pState);    
           pState = NULL;
           goto Exit;
      }      
     
      NvOsMemset(((pState)->pInfo)->pTrackInfo,0,sizeof(NvOggTrackInfo));
      NvOsMemset(&pState->pData, 0, sizeof(NvOggMetaDataParams));
      NvOsMemset(&pState->pOggMetaData, 0, sizeof(NvOggMetaData));

      pState->OggInfo.pOggParams = NULL;
      pState->bOggHeader = NV_TRUE;
      pState->cphandle = hContent;
      pState->pPipe = pPipe; 
      pState->numberOfTracks = 1;

Exit:    
  
      return pState;
}

void NvReleaseOggReaderHandle(NvOggParser * pState)
{
    if (pState)
    {
        if(pState->pInfo)
        {
            if (pState->pInfo->pTrackInfo != NULL)
            {
                NvOsFree(pState->pInfo->pTrackInfo);
                pState->pInfo->pTrackInfo = NULL;
            }

            NvOsFree(pState->pInfo);   
            pState->pInfo = NULL;

            if (pState->OggInfo.pOggParams)
            {
                NvOGGParserDeInit((void**)&pState->OggInfo.pOggParams);
                pState->OggInfo.pOggParams = NULL;
            }            
        }

        NvOsFree(pState->pOggMetaData.oArtistMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oAlbumMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oGenreMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oTitleMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oYearMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oTrackNoMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oEncodedMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oCommentMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oComposerMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oPublisherMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oOrgArtistMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oCopyRightMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oUrlMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oBpmMetaData.pClientBuffer);
        NvOsFree(pState->pOggMetaData.oAlbumArtistMetaData.pClientBuffer);
    }
}


NvError NvParseOggTrackInfo(NvOggParser *pState)
{
    NvError status = NvSuccess;        
    NvS32 tempSeek = 0;
    NvOggParams *OggInfo = &pState->OggInfo;
    NvS32 TotalTrackTime = 0;
    OggInfo->pOggParams = NULL;
        
    status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);   
    if (status != NvSuccess) return NvError_ParserFailure;

    status = NvOGGParserInit(pState, (void **)(&OggInfo->pOggParams));
    if (OggInfo->pOggParams != NULL) 
    {
        if (OggInfo->pOggParams->vi != NULL)
        {
            pState->pInfo->pTrackInfo->bitRate = OggInfo->pOggParams->vi->bitrate_nominal;    
            pState->pInfo->pTrackInfo->noOfChannels = OggInfo->pOggParams->vi->channels;    
            pState->pInfo->pTrackInfo->samplingFreq = OggInfo->pOggParams->vi->rate;
            pState->pInfo->pTrackInfo->data_offset = (NvS32)OggInfo->pOggParams->offset;
            pState->pInfo->pTrackInfo->sampleSize = 16;            
            NvOggGetTotalTime(pState, &TotalTrackTime);                  
            pState->pInfo->pTrackInfo->total_time = TotalTrackTime;           
        }
    }    

    return status;
}

NvError NvOggGetTotalTime(NvOggParser *pState, NvS32 *TotalTrackTime)
{
    NvError status = NvSuccess;  
    OggVorbis_File  *OGGParams = pState->OggInfo.pOggParams;

    *TotalTrackTime = ((NvS32)(NvOvTimeTotal(OGGParams,-1))/1000);              
    
   // checking for corruptness, for broken last frames PcmLength is very small. set a threshold of 10 sec and compare.
    if(*TotalTrackTime < MAX_THRESHOLD_TIME_FOR_CORRUPTED_FILES)   
    {
        if (OGGParams->vi->bitrate_nominal > 0)
            *TotalTrackTime = ((NvS32)(OGGParams->end)/(OGGParams->vi->bitrate_nominal >> 3));   
    }
        
    return status;
}


NvError NvOggSeekToTime(NvOggParser *pState, NvS64 mSec)
{
    NvError status = NvSuccess;  
   
    OggVorbis_File  *OGGParams = pState->OggInfo.pOggParams;

    NvOggSeek(pState,OGGParams, mSec);

    return status;
}

NvError  NvOggSeek(NvOggParser *pState,OggVorbis_File *vf,ogg_int64_t milliseconds)
{  
    NvError status = NvSuccess; 
    NvS32 Ret;
    Ret = NvOvTimeSeekPage(pState,vf, milliseconds);                       //ov_time_seek_page
    if (Ret == 0)
        status =  NvSuccess;
    else
        status =  NvError_ParserFailure;

    return status;
}

#define OGG_FIELD_NAME_LENGTH  32

NvError NvParseOggMetaData(NvOggParser *pState)
{
    NvError status = NvSuccess; 
    NvS32 fieldCount;    
    NvS8 *pData = NULL;
    NvS32 fieldLength;
    NvMMMetaDataTypes MetaDataType; 
    NvS32 pCommentLen;
    NvMMMetaDataInfo *pMD=NULL;

    vorbis_comment  *pComment = pState->OggInfo.pOggParams->vc;

    if (pComment->comments == 0)
    {
         NvOsMemset(&pState->pData, 0, sizeof(NvOggMetaDataParams));   
         return status;
    }

    for (fieldCount = 0; fieldCount < pComment->comments; fieldCount++)
    {
        pData = (NvS8 *)pComment->user_comments[fieldCount];     
        pCommentLen = (NvS32)pComment->comment_lengths[fieldCount];

        MetaDataType = NvOggGetFieldName(pData, &fieldLength,pCommentLen);
        pData += fieldLength;   

        if (MetaDataType != NvMMMetaDataInfo_Force32)
        {
            switch(MetaDataType)
            {
                case NvMMMetaDataInfo_Artist:
                    pMD = &(pState->pOggMetaData.oArtistMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Artist;
                    pState->pData.pArtistMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Album:
                    pMD = &(pState->pOggMetaData.oAlbumMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Album;
                    pState->pData.pAlbumMetaData = 1;
                    break;    
                case NvMMMetaDataInfo_Genre:
                    pMD = &(pState->pOggMetaData.oGenreMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Genre;
                    pState->pData.pGenreMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Title:
                    pMD = &(pState->pOggMetaData.oTitleMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Title;
                    pState->pData.pTitleMetaData = 1;
                    break;    
                case NvMMMetaDataInfo_Year:
                    pMD = &(pState->pOggMetaData.oYearMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Year;
                    pState->pData.pYearMetaData = 1;
                    break;
                case NvMMMetaDataInfo_TrackNumber:
                    pMD = &(pState->pOggMetaData.oTrackNoMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_TrackNumber;
                    pState->pData.pTrackMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Encoded:
                    pMD = &(pState->pOggMetaData.oEncodedMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Encoded;
                    pState->pData.pEncodedMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Comment:
                    pMD = &(pState->pOggMetaData.oCommentMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Comment;
                    pState->pData.pCommentMetaData = 1;
                    break;    
                case NvMMMetaDataInfo_Composer:
                    pMD = &(pState->pOggMetaData.oComposerMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Composer;
                    pState->pData.pComposerMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Publisher:
                    pMD = &(pState->pOggMetaData.oPublisherMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Publisher;
                    pState->pData.pPublisherMetaData = 1;
                    break;
                case NvMMMetaDataInfo_OrgArtist:
                    pMD = &(pState->pOggMetaData.oOrgArtistMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_OrgArtist;
                    pState->pData.pOrgArtistMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Copyright:
                    pMD = &(pState->pOggMetaData.oCopyRightMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Copyright;
                    pState->pData.pCopyRightMetaData = 1;
                    break;    
                case NvMMMetaDataInfo_Url:
                    pMD = &(pState->pOggMetaData.oCopyRightMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Url;
                    pState->pData.pUrlMetaData = 1;
                    break;
                case NvMMMetaDataInfo_Bpm:
                    pMD = &(pState->pOggMetaData.oBpmMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_Bpm;
                    pState->pData.pBpmMetaData = 1;
                    break;    
                case NvMMMetaDataInfo_AlbumArtist:
                    pMD = &(pState->pOggMetaData.oAlbumArtistMetaData);
                    pMD->eEncodeType = NvMMMetaDataEncode_Utf8;
                    pMD->eMetadataType= NvMMMetaDataInfo_AlbumArtist;
                    pState->pData.pAlbumArtistMetaData = 1;
                    break;                    
                 default:
                    break;

            }
            
            if(pMD)
            {
                if (pMD->pClientBuffer)
                    NvOsFree(pMD->pClientBuffer);

                pMD->pClientBuffer = NvOsAlloc(pCommentLen - fieldLength + 1);
                pMD->nBufferSize = pCommentLen - fieldLength + 1;
                NvOsMemcpy(pMD->pClientBuffer, pData, pMD->nBufferSize);
                pMD->pClientBuffer[pMD->nBufferSize - 1] = '\0';
                pData = NULL;
            }            
       }

    }

    return status;
}

NvMMMetaDataTypes NvOggGetFieldName(NvS8 *pData, NvS32 *Len, NvS32 CommentLen)
{
    NvS32 Length = 0;
    NvS8 *ptr;
    char fieldName[OGG_FIELD_NAME_LENGTH];
    NvMMMetaDataTypes FieldType = NvMMMetaDataInfo_Force32; 

    NvOsMemset(fieldName, 0, OGG_FIELD_NAME_LENGTH * sizeof(NvS8));

    ptr = pData;

    do
    {
        fieldName[Length] = toupper(*ptr++);
        Length++;    



        if ( (Length > CommentLen) || (Length >= OGG_FIELD_NAME_LENGTH))



            break;   
    } while (*ptr != 0x3d);




    if ((Length > CommentLen) ||  (Length >= OGG_FIELD_NAME_LENGTH))



    {
        *Len = 0;
        return FieldType; 
    }

    fieldName[Length] = 0x3d;  // Ascii of "="
    Length++;    
    
    *Len = Length;       

    if (NvOsStrcmp(fieldName, "ARTIST=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Artist;
    } 
    else if (NvOsStrcmp(fieldName, "ALBUM=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Album;
    } 
    else if (NvOsStrcmp(fieldName, "GENRE=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Genre;
    } 
    else if (NvOsStrcmp(fieldName, "TITLE=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Title;
    } 
    else if (NvOsStrcmp(fieldName, "DATE=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Year;
    } 
    else if (NvOsStrcmp(fieldName, "TRACKNUMBER=") == 0)
    {
        FieldType = NvMMMetaDataInfo_TrackNumber;
    } 
    else if (NvOsStrcmp(fieldName, "ENCODED=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Encoded;
    } 
    else if (NvOsStrcmp(fieldName, "COMMENT=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Comment;
    } 
    else if (NvOsStrcmp(fieldName, "COMPOSER=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Composer;
    } 
    else if (NvOsStrcmp(fieldName, "PUBLISHER=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Publisher;
    } 
    else if (NvOsStrcmp(fieldName, "PERFORMER=") == 0)
    {
        FieldType = NvMMMetaDataInfo_OrgArtist;
    } 
    else if (NvOsStrcmp(fieldName, "COPYRIGHT=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Copyright;
    } 
    else if (NvOsStrcmp(fieldName, "bpm=") == 0)
    {
        FieldType = NvMMMetaDataInfo_Bpm;
    } 
    else if (NvOsStrcmp(fieldName, "ALBUMARTIST=") == 0)
    {
        FieldType = NvMMMetaDataInfo_AlbumArtist;
    } 
    else if (NvOsStrcmp(fieldName, "VERSION=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_Version;                        //Not Supported yet in nvmm_metadata.h
    } 
    else if (NvOsStrcmp(fieldName, "LICENSE=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_License;
    } 
    else if (NvOsStrcmp(fieldName, "ORGANIZATION=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_Organization;
    } 
    else if (NvOsStrcmp(fieldName, "DESCRIPTION=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_Description;
    } 
    else if (NvOsStrcmp(fieldName, "LOCATION=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_Location;
    } 
    else if (NvOsStrcmp(fieldName, "CONTACT=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_Contact;
    } 
    else if (NvOsStrcmp(fieldName, "DISCNUMBER=") == 0)
    {
        //FieldType = NvMMMetaDataInfo_Discnumber;
    } 

    return FieldType;
}














































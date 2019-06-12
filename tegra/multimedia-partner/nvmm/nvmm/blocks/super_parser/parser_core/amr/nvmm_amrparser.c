
/**
 * @nvmm_amrparser.c
 * @brief IMplementation of Amr parser Class.
 *
 * @b Description: Implementation of Amr parser public API's.
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

#include "nvmm_amrparser.h"
#include "nvos.h"

#define AMR_MAGIC_NUMBER "#!AMR\n"
#define AWB_MAGIC_NUMBER        "#!AMR-WB\n"

#define BIT_0     (NvS16)-127
#define BIT_1     (NvS16)127

enum
{
    NB_SERIAL_MAX = 61,
    L_FRAME16k = 320
};



/* number of speech bits for all modes */
const NvS16 Unpacked_size[16] = {95, 103, 118, 134, 148, 159, 204, 244,
                                   35,   0,   0,   0,   0,   0,   0,   0};


/* sorting tables for all modes */

const NvS16 Sort_475[95] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
   10, 11, 12, 13, 14, 15, 23, 24, 25, 26,
   27, 28, 48, 49, 61, 62, 82, 83, 47, 46,
   45, 44, 81, 80, 79, 78, 17, 18, 20, 22,
   77, 76, 75, 74, 29, 30, 43, 42, 41, 40,
   38, 39, 16, 19, 21, 50, 51, 59, 60, 63,
   64, 72, 73, 84, 85, 93, 94, 32, 33, 35,
   36, 53, 54, 56, 57, 66, 67, 69, 70, 87,
   88, 90, 91, 34, 55, 68, 89, 37, 58, 71,
   92, 31, 52, 65, 86
};

const NvS16 Sort_515[103] = {
    7,  6,  5,   4,   3,   2,  1,  0, 15, 14,
   13, 12, 11,  10,   9,   8, 23, 24, 25, 26,
   27, 46, 65,  84,  45,  44, 43, 64, 63, 62,
   83, 82, 81, 102, 101, 100, 42, 61, 80, 99,
   28, 47, 66,  85,  18,  41, 60, 79, 98, 29,
   48, 67, 17,  20,  22,  40, 59, 78, 97, 21,
   30, 49, 68,  86,  19,  16, 87, 39, 38, 58,
   57, 77, 35,  54,  73,  92, 76, 96, 95, 36,
   55, 74, 93,  32,  51,  33, 52, 70, 71, 89,
   90, 31, 50,  69,  88,  37, 56, 75, 94, 34,
   53, 72, 91
};

const NvS16 Sort_59[118] = {
    0,   1,   4,   5,   3,   6,   7,   2,  13,  15,
    8,   9,  11,  12,  14,  10,  16,  28,  74,  29,
   75,  27,  73,  26,  72,  30,  76,  51,  97,  50,
   71,  96, 117,  31,  77,  52,  98,  49,  70,  95,
  116,  53,  99,  32,  78,  33,  79,  48,  69,  94,
  115,  47,  68,  93, 114,  46,  67,  92, 113,  19,
   21,  23,  22,  18,  17,  20,  24, 111,  43,  89,
  110,  64,  65,  44,  90,  25,  45,  66,  91, 112,
   54, 100,  40,  61,  86, 107,  39,  60,  85, 106,
   36,  57,  82, 103,  35,  56,  81, 102,  34,  55,
   80, 101,  42,  63,  88, 109,  41,  62,  87, 108,
   38,  59,  84, 105,  37,  58,  83, 104
};

const NvS16 Sort_67[134] = {
    0,   1,   4,   3,   5,   6,  13,   7,   2,   8,
    9,  11,  15,  12,  14,  10,  28,  82,  29,  83,
   27,  81,  26,  80,  30,  84,  16,  55, 109,  56,
  110,  31,  85,  57, 111,  48,  73, 102, 127,  32,
   86,  51,  76, 105, 130,  52,  77, 106, 131,  58,
  112,  33,  87,  19,  23,  53,  78, 107, 132,  21,
   22,  18,  17,  20,  24,  25,  50,  75, 104, 129,
   47,  72, 101, 126,  54,  79, 108, 133,  46,  71,
  100, 125, 128, 103,  74,  49,  45,  70,  99, 124,
   42,  67,  96, 121,  39,  64,  93, 118,  38,  63,
   92, 117,  35,  60,  89, 114,  34,  59,  88, 113,
   44,  69,  98, 123,  43,  68,  97, 122,  41,  66,
   95, 120,  40,  65,  94, 119,  37,  62,  91, 116,
   36,  61,  90, 115
};

const NvS16 Sort_74[148] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
   10,  11,  12,  13,  14,  15,  16,  26,  87,  27,
   88,  28,  89,  29,  90,  30,  91,  51,  80, 112,
  141,  52,  81, 113, 142,  54,  83, 115, 144,  55,
   84, 116, 145,  58, 119,  59, 120,  21,  22,  23,
   17,  18,  19,  31,  60,  92, 121,  56,  85, 117,
  146,  20,  24,  25,  50,  79, 111, 140,  57,  86,
  118, 147,  49,  78, 110, 139,  48,  77,  53,  82,
  114, 143, 109, 138,  47,  76, 108, 137,  32,  33,
   61,  62,  93,  94, 122, 123,  41,  42,  43,  44,
   45,  46,  70,  71,  72,  73,  74,  75, 102, 103,
  104, 105, 106, 107, 131, 132, 133, 134, 135, 136,
   34,  63,  95, 124,  35,  64,  96, 125,  36,  65,
   97, 126,  37,  66,  98, 127,  38,  67,  99, 128,
   39,  68, 100, 129,  40,  69, 101, 130
};

const NvS16 Sort_795[159] = {
    8,   7,   6,   5,   4,   3,   2,  14,  16,   9,
   10,  12,  13,  15,  11,  17,  20,  22,  24,  23,
   19,  18,  21,  56,  88, 122, 154,  57,  89, 123,
  155,  58,  90, 124, 156,  52,  84, 118, 150,  53,
   85, 119, 151,  27,  93,  28,  94,  29,  95,  30,
   96,  31,  97,  61, 127,  62, 128,  63, 129,  59,
   91, 125, 157,  32,  98,  64, 130,   1,   0,  25,
   26,  33,  99,  34, 100,  65, 131,  66, 132,  54,
   86, 120, 152,  60,  92, 126, 158,  55,  87, 121,
  153, 117, 116, 115,  46,  78, 112, 144,  43,  75,
  109, 141,  40,  72, 106, 138,  36,  68, 102, 134,
  114, 149, 148, 147, 146,  83,  82,  81,  80,  51,
   50,  49,  48,  47,  45,  44,  42,  39,  35,  79,
   77,  76,  74,  71,  67, 113, 111, 110, 108, 105,
  101, 145, 143, 142, 140, 137, 133,  41,  73, 107,
  139,  37,  69, 103, 135,  38,  70, 104, 136
};

const NvS16 Sort_102[204] = {
    7,   6,   5,   4,   3,   2,   1,   0,  16,  15,
   14,  13,  12,  11,  10,   9,   8,  26,  27,  28,
   29,  30,  31, 115, 116, 117, 118, 119, 120,  72,
   73, 161, 162,  65,  68,  69, 108, 111, 112, 154,
  157, 158, 197, 200, 201,  32,  33, 121, 122,  74,
   75, 163, 164,  66, 109, 155, 198,  19,  23,  21,
   22,  18,  17,  20,  24,  25,  37,  36,  35,  34,
   80,  79,  78,  77, 126, 125, 124, 123, 169, 168,
  167, 166,  70,  67,  71, 113, 110, 114, 159, 156,
  160, 202, 199, 203,  76, 165,  81,  82,  92,  91,
   93,  83,  95,  85,  84,  94, 101, 102,  96, 104,
   86, 103,  87,  97, 127, 128, 138, 137, 139, 129,
  141, 131, 130, 140, 147, 148, 142, 150, 132, 149,
  133, 143, 170, 171, 181, 180, 182, 172, 184, 174,
  173, 183, 190, 191, 185, 193, 175, 192, 176, 186,
   38,  39,  49,  48,  50,  40,  52,  42,  41,  51,
   58,  59,  53,  61,  43,  60,  44,  54, 194, 179,
  189, 196, 177, 195, 178, 187, 188, 151, 136, 146,
  153, 134, 152, 135, 144, 145, 105,  90, 100, 107,
   88, 106,  89,  98,  99,  62,  47,  57,  64,  45,
   63,  46,  55,  56
};

const NvS16 Sort_122[244] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
   10,  11,  12,  13,  14,  23,  15,  16,  17,  18,
   19,  20,  21,  22,  24,  25,  26,  27,  28,  38,
  141,  39, 142,  40, 143,  41, 144,  42, 145,  43,
  146,  44, 147,  45, 148,  46, 149,  47,  97, 150,
  200,  48,  98, 151, 201,  49,  99, 152, 202,  86,
  136, 189, 239,  87, 137, 190, 240,  88, 138, 191,
  241,  91, 194,  92, 195,  93, 196,  94, 197,  95,
  198,  29,  30,  31,  32,  33,  34,  35,  50, 100,
  153, 203,  89, 139, 192, 242,  51, 101, 154, 204,
   55, 105, 158, 208,  90, 140, 193, 243,  59, 109,
  162, 212,  63, 113, 166, 216,  67, 117, 170, 220,
   36,  37,  54,  53,  52,  58,  57,  56,  62,  61,
   60,  66,  65,  64,  70,  69,  68, 104, 103, 102,
  108, 107, 106, 112, 111, 110, 116, 115, 114, 120,
  119, 118, 157, 156, 155, 161, 160, 159, 165, 164,
  163, 169, 168, 167, 173, 172, 171, 207, 206, 205,
  211, 210, 209, 215, 214, 213, 219, 218, 217, 223,
  222, 221,  73,  72,  71,  76,  75,  74,  79,  78,
   77,  82,  81,  80,  85,  84,  83, 123, 122, 121,
  126, 125, 124, 129, 128, 127, 132, 131, 130, 135,
  134, 133, 176, 175, 174, 179, 178, 177, 182, 181,
  180, 185, 184, 183, 188, 187, 186, 226, 225, 224,
  229, 228, 227, 232, 231, 230, 235, 234, 233, 238,
  237, 236,  96, 199
};

const NvS16 Sort_SID[35] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
   10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
   20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
   30, 31, 32, 33, 34
};

/* pointer table for bit sorting tables */
const NvS16 *Sort_ptr[16] = {Sort_475, Sort_515, Sort_59,
                        Sort_67, Sort_74, Sort_795, Sort_102,
                        Sort_122, Sort_SID, NULL, NULL, NULL,
                        NULL, NULL, NULL, NULL};

NvAmrParser * NvGetAmrReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe )
{      
      NvAmrParser *pState;

      pState = (NvAmrParser *)NvOsAlloc(sizeof(NvAmrParser));
      if (pState == NULL)
      {           
           goto Exit;
      }

      pState->pInfo = (NvAmrStreamInfo*)NvOsAlloc(sizeof(NvAmrStreamInfo));
      if ((pState->pInfo) == NULL)
      {
           NvOsFree(pState);
           pState = NULL;
           goto Exit;
      }

      pState->pInfo->pTrackInfo = (NvAmrTrackInfo*)NvOsAlloc(sizeof(NvAmrTrackInfo));
      if ((pState->pInfo->pTrackInfo) == NULL)
      {
           NvOsFree(pState->pInfo);
           pState->pInfo = NULL;           
           NvOsFree(pState);    
           pState = NULL;
           goto Exit;
      }
     
      NvOsMemset(((pState)->pInfo)->pTrackInfo,0,sizeof(NvAmrTrackInfo));

      
      pState->cphandle = hContent;
      pState->pPipe = pPipe; 
      pState->numberOfTracks = 1;
      pState->m_Frame = 0;
      pState->m_MgkFlag = NV_FALSE;
      pState->m_SeekFlag = NV_FALSE;
      pState->TotalReadBytes = 0;
      pState->FileSize = 0;
      pState->m_CurrentTime = 0;
      pState->m_PreviousTime = 0;
      pState->bAmrAwbHeader = NV_TRUE;

Exit:    
  
      return pState;
}

void NvReleaseAmrReaderHandle(NvAmrParser * pState)
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
        }
    }
    NvOsFree(pState);
}

NvError NvAmrInit(NvAmrParser *pState)
{
    NvError status = NvSuccess;  
    NvS32 tempSeek;
    NvU8 *pBuffer;
    NvU32 BytesRead;

    NvU16 PackedSize[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};
    NvU8 ft, toc;
    NvU64 FileSize = pState->FileSize;

    pState->m_CurrentTime = pState->m_PreviousTime = 0;

    /* Check AMR Packet Size */
    pBuffer = (NvU8 *)NvOsAlloc(MAX_SERIAL_SIZE);
    if (NULL == pBuffer)
    {
        return NvError_InsufficientMemory;
    }
    tempSeek = 0;
    status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);       
    if(status != NvSuccess)
    {
        status = NvError_ParserFailure;
        goto Exit;
    }
    pState->TotalReadBytes = tempSeek;

    if (FileSize < NvOsStrlen(AMR_MAGIC_NUMBER))
    {
        status = NvError_ParserFailure;
        goto Exit;
    }

    BytesRead = NvOsStrlen(AMR_MAGIC_NUMBER);

    while (FileSize - NvOsStrlen(AMR_MAGIC_NUMBER))
    {
        status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pBuffer, BytesRead);   
        if( (status != NvSuccess) && (BytesRead != NvOsStrlen(AMR_MAGIC_NUMBER)) )
        {
            status = NvError_ParserFailure;
            goto Exit;
        }
        pState->TotalReadBytes += BytesRead;
        if (NvOsStrncmp((const char *)pBuffer, AMR_MAGIC_NUMBER, NvOsStrlen(AMR_MAGIC_NUMBER)) == 0)
        {
            // Magic Number matched i.e AMR has packed stream
            // Now parsing for the Packed stream
            pState->m_MgkFlag = NV_TRUE;
            
            BytesRead = sizeof(NvU8);
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&toc, BytesRead);
            pState->TotalReadBytes += BytesRead;
            
            if(status == NvSuccess)
            {
                    ft = (toc >> 3) & 0x0F;
                    
                    switch (PackedSize[ft] + 1)
                    {
                        case 13:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 4750;
                            break;
                        }
                        case 14:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 5150;
                            break;
                        }
                        case 16:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 5900;
                            break;
                        }
                        case 18:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 6700;
                            break;
                        }
                        case 20:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 7400;
                            break;
                        }
                        case 21:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 7950;
                            break;
                        }
                        case 27:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 10200;
                            break;
                        }
                        case 32:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 12200;
                            break;
                        }
                        default:
                        {
                            pState->pInfo->pTrackInfo->bitRate = 12200;
                            break;
                        }
                    }
                }
                else
                {
                    status = NvError_ParserFailure;
                    goto Exit;
                }

                pState->pInfo->pTrackInfo->Dtx = 0;
                pState->pInfo->pTrackInfo->samplingFreq = 8000;
                pState->pInfo->pTrackInfo->noOfChannels= 1;
                pState->pInfo->pTrackInfo->sampleSize= 16;

                BytesRead = PackedSize[ft];

                if (pState->m_Frame == 0)
                {
                    status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pBuffer, BytesRead);
                    
                    if((status != NvSuccess) && (BytesRead != PackedSize[ft]))
                    {
                        status = NvError_ParserFailure;
                        goto Exit;
                    }
                    pState->TotalReadBytes += BytesRead;
                    pState->m_Frame++;

                    NvAmrGetTotalFrames(pState);
                }

                tempSeek = 0;
                status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);    
                if(status != NvSuccess)
                {
                    status = NvError_ParserFailure;
                    goto Exit;
                }
                pState->TotalReadBytes = tempSeek;
                /* Skip the magic number */
                tempSeek = NvOsStrlen(AMR_MAGIC_NUMBER);
                status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);  
                if(status != NvSuccess) 
                {
                    status = NvError_ParserFailure;
                    goto Exit;
                }
                pState->TotalReadBytes = tempSeek;
                NvOsFree(pBuffer);
                pBuffer = NULL;
                return NvSuccess;                
            }
            else
            {
                tempSeek = -((NvS32)(NvOsStrlen(AMR_MAGIC_NUMBER) - 1));
                status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginCur);
                if(status != NvSuccess) 
                {
                    status = NvError_ParserFailure;
                    goto Exit;
                }
                
            pState->TotalReadBytes -= (NvOsStrlen(AMR_MAGIC_NUMBER) - 1);
                FileSize = FileSize - 1;
            }
        }

Exit:        
     NvOsFree(pBuffer);
     pBuffer = NULL;
     
     return status;
}

NvError NvAwbInit(NvAmrParser *pState)
{
    NvError status = NvSuccess; 
    NvS32 tempSeek;
    NvU8 *pBuffer;
    NvU32 BytesRead;
    NvU16 block_size[16] = {17, 23, 32, 36, 40, 46, 50, 58, 60,  5,  0,  0,  0,  0,  0,  0}; 
    NvU8  toc;
    NvU64 FileSize = pState->FileSize;

    pState->m_CurrentTime = pState->m_PreviousTime = 0;

    /* Check AMR Packet Size */
    pBuffer = (NvU8 *)NvOsAlloc(MAX_SERIAL_SIZE);
    if (NULL == pBuffer)
    {
        return NvError_InsufficientMemory;
    }
    tempSeek = 0;
    status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);     
    if(status != NvSuccess)
    {
        status = NvError_ParserFailure;
        goto Exit;
    }
    pState->TotalReadBytes = tempSeek;
    if (FileSize < NvOsStrlen(AWB_MAGIC_NUMBER))
    {
        status = NvError_ParserFailure;
        goto Exit;
    }

    BytesRead = NvOsStrlen(AWB_MAGIC_NUMBER);

    while (FileSize - NvOsStrlen(AWB_MAGIC_NUMBER))
    {
        status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pBuffer, BytesRead);   
        if( (status != NvSuccess) && (BytesRead != NvOsStrlen(AWB_MAGIC_NUMBER)) )
        {
            status = NvError_ParserFailure;
            goto Exit;
        }
        pState->TotalReadBytes += BytesRead;
        if (NvOsStrncmp((const char *)pBuffer, AWB_MAGIC_NUMBER, NvOsStrlen(AWB_MAGIC_NUMBER)) == 0)
        {
            pState->m_MgkFlag = NV_TRUE;
            
            BytesRead = sizeof(NvU8);
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&toc, BytesRead);

            if(status == NvSuccess)
            {
                pState->TotalReadBytes += BytesRead;
                pState->m_Mode = (NvS16)((toc >> 3) & 0x0F);

                switch (pState->m_Mode)
                {
                    case 0:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 7000;
                        break;
                    }
                    case 1:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 9000;
                        break;
                    }
                    case 2:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 12000;
                        break;
                    }
                    case 3:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 14000;
                        break;
                    }
                    case 4:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 16000;
                        break;
                    }
                    case 5:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 18000;
                        break;
                    }
                    case 6:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 20000;
                        break;
                    }
                    case 7:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 23000;
                        break;
                    }
                    case 8:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 24000;
                        break;
                    }
                    default:
                    {
                        pState->pInfo->pTrackInfo->bitRate = 24000;
                        break;
                    }
                }
            } 
            else
            {
                status = NvError_ParserFailure;
                goto Exit;
            }

            pState->pInfo->pTrackInfo->Dtx = 0;
            pState->pInfo->pTrackInfo->samplingFreq = 16000;
            pState->pInfo->pTrackInfo->noOfChannels= 1;
            pState->pInfo->pTrackInfo->sampleSize= 16;

            BytesRead = block_size[pState->m_Mode];

            if (pState->m_Frame == 0)
            {
                status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pBuffer, BytesRead);
                
                if((status != NvSuccess) && (BytesRead != (NvU32)(block_size[pState->m_Mode])))
                {
                    status = NvError_ParserFailure;
                    goto Exit;
                }
                pState->TotalReadBytes += BytesRead;
                pState->m_Frame++;

                NvAwbGetTotalFrames(pState);
            }

            tempSeek = 0;
            status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);     
            if(status != NvSuccess) 
            {
                status = NvError_ParserFailure;
                goto Exit;
            }
            pState->TotalReadBytes = tempSeek;
            /* Skip the magic number */
            tempSeek = NvOsStrlen(AWB_MAGIC_NUMBER);
            status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);  
            if(status != NvSuccess)
            {
                status = NvError_ParserFailure;
                goto Exit;
            }
            pState->TotalReadBytes = tempSeek;
            NvOsFree(pBuffer);
            pBuffer = NULL;
            return NvSuccess;      
      }  
      else
      {
            tempSeek = -((NvS32)(NvOsStrlen(AWB_MAGIC_NUMBER) - 1));
            status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginCur);
            if(status != NvSuccess) 
            {
                status = NvError_ParserFailure;
                goto Exit;
            }
            pState->TotalReadBytes -= (NvOsStrlen(AWB_MAGIC_NUMBER) - 1);
            FileSize = FileSize - 1;    
       }          

    }

Exit:        
    NvOsFree(pBuffer);
    pBuffer = NULL;
    
    return status;
}

NvU64 NvAwbGetTotalFrames(NvAmrParser *pState)
{
    NvError status = NvSuccess;     
    NvU8 pData[NB_SERIAL_MAX];
    NvU16 block_size[16] = {17, 23, 32, 36, 40, 46, 50, 58, 60,  5,  0,  0,  0,  0,  0,  0}; 
    NvU32 BytesRead;
    NvU8 ByteSize;
    NvU8 toc;

    BytesRead=ByteSize = sizeof(NvU8);
    
    if (pState->m_MgkFlag)
    {
        /* Read the First Byte from the File */
        for (;;)
        {          
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&toc, ByteSize);
            if( status != NvSuccess) 
            {                 
                break;
            }
            else 
            {
                pState->TotalReadBytes += ByteSize;
                pState->m_Mode = (NvS16)((toc >> 3) & 0x0F);

                // Read those many bytes                
                BytesRead = block_size[pState->m_Mode] ;
                status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pData, BytesRead);
                if( status != NvSuccess) 
                {                 
                    break;
                }
                else
                {
                    pState->TotalReadBytes += BytesRead;
                    pState->m_Frame++;
                }
            }
            
        }
    }

    return pState->m_Frame;

}


NvError NvParseAmrAwbTrackInfo(NvAmrParser *pState)
{
    NvError status = NvSuccess;    
    NvU8 *pBuffer;
    NvU32 BytesRead;

    /* Check AMR Packet Size */
    pBuffer = (NvU8 *)NvOsAlloc(MAX_SERIAL_SIZE);
    if (NULL == pBuffer)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    BytesRead = NvOsStrlen(AWB_MAGIC_NUMBER);
    status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pBuffer, BytesRead);   
    if( (status != NvSuccess) && (BytesRead != NvOsStrlen(AWB_MAGIC_NUMBER)) )
    {
        status = NvError_ParserFailure;
        goto cleanup;
    }
    pState->TotalReadBytes += BytesRead;
    if (NvOsStrncmp((const char *)pBuffer, AMR_MAGIC_NUMBER, NvOsStrlen(AMR_MAGIC_NUMBER)) == 0)
    {
        pState->pInfo->pTrackInfo->streamType = NvStreamTypeAMRNB;
        status = NvAmrInit(pState);
        if(status != NvSuccess)
        {
            goto cleanup;
        }
        pState->pInfo->pTrackInfo->total_time = NvAmrGetDuration(pState);

        status = pState->pPipe->cpipe.SetPosition(pState->cphandle, 0, CP_OriginBegin);  
        if(status != NvSuccess)
        {
            goto cleanup;
        }
        pState->TotalReadBytes = 0;
        /* Skip the magic number */
        status = pState->pPipe->cpipe.SetPosition(pState->cphandle, NvOsStrlen(AMR_MAGIC_NUMBER), CP_OriginBegin);
        if(status != NvSuccess)
        {
            goto cleanup;
        }
        pState->TotalReadBytes = NvOsStrlen(AMR_MAGIC_NUMBER);
    }
    else if (NvOsStrncmp((const char *)pBuffer, AWB_MAGIC_NUMBER, NvOsStrlen(AWB_MAGIC_NUMBER)) == 0)
    {
        pState->pInfo->pTrackInfo->streamType = NvStreamTypeAMRWB;
        status = NvAwbInit(pState);
        if(status != NvSuccess)
        {
           goto cleanup;
        }
        pState->pInfo->pTrackInfo->total_time = NvAwbGetDuration(pState);

         status = pState->pPipe->cpipe.SetPosition(pState->cphandle, 0, CP_OriginBegin);  
        if(status != NvSuccess)
        {
            goto cleanup;
        }
        pState->TotalReadBytes = 0;
         /* Skip the magic number */   
         status = pState->pPipe->cpipe.SetPosition(pState->cphandle, NvOsStrlen(AWB_MAGIC_NUMBER), CP_OriginBegin);  
        if(status != NvSuccess)
        {
            goto cleanup;
        }
        pState->TotalReadBytes = NvOsStrlen(AWB_MAGIC_NUMBER);
    }    
    else
    {
        status = NvError_UnSupportedStream;
    }
cleanup:
    if (pBuffer)
    {
         NvOsFree(pBuffer);
         pBuffer = NULL;
    }
    return status;
}

/**
 * @brief              This function gets the total number of AMR frames present in the file
 *                     and updates the global member variable m_Frame of the class.
 * @returns            STATUS of the function
 */
NvU64 NvAmrGetTotalFrames(NvAmrParser *pState)
{
    NvError status = NvSuccess;    
    NvU8 toc,  ft;
    NvU8 PackedBits[MAX_SERIAL_SIZE];
    NvU16 PackedSize[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};

    NvU32 BytesRead;
    NvU8 ByteSize;

    BytesRead=ByteSize = sizeof(NvU8);

    if (pState->m_MgkFlag)
    {
        /* Read the First Byte from the File */
        for (;;)
        {          
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&toc, ByteSize);
            if( status != NvSuccess) 
            {                 
                break;
            }
            else 
            {
                pState->TotalReadBytes += ByteSize;
                //q = (toc >> 2) & 0x01;
                ft = (toc >> 3) & 0x0F;

                // Read those many bytes
                BytesRead = PackedSize[ft];
                status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)PackedBits, BytesRead);
                if( status != NvSuccess) 
                {                 
                    break;
                }
                else
                {
                    pState->TotalReadBytes += BytesRead;
                    pState->m_Frame++;
                }
            }
            
        }
    }

    return pState->m_Frame;
}

/**
 * @brief          This function get the total duration of the file.
 *                 It basically calculates this on the basis of number of frames present
 * @returns        total duration of the file
 */
NvU64 NvAmrGetDuration(NvAmrParser *pState)
{
    return (pState->m_Frame * 200 * 1000);
}

/**
 * @brief          This function get the total duration of the file.
 *                 It basically calculates this on the basis of number of frames present
 * @returns        total duration of the file
 */
NvU64 NvAwbGetDuration(NvAmrParser *pState)
{
    return (pState->m_Frame * 200 * 1000);
}


/**
 * @brief          This function gets the current position of the seek bar.
 *                 It basically returns the currentTime
 * @returns        current decode time stamps
 */
NvS64 NvAmrGetCurrentPosition(NvAmrParser *pState)
{
    return pState->m_CurrentTime;
}

NvS64 NvAwbGetCurrentPosition(NvAmrParser *pState)
{
    return pState->m_CurrentTime;
}

NvError NvAmrSeek(NvAmrParser *pState, NvS64 seekTime)
{
    NvError status = NvSuccess;  
    NvU8 toc,ft, FrameCnt = 0;
    NvU8 PackedBits[MAX_SERIAL_SIZE];
    NvU16 PackedSize[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};
    NvU32 BytesToRead = 0;
    NvS64 FrameTime;
    NvS32 tempseek = NvOsStrlen(AMR_MAGIC_NUMBER);

    FrameTime = 0;

    if (seekTime == 0)
    {        
        status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempseek, CP_OriginBegin);         
        pState->TotalReadBytes = tempseek;
        goto AmrSeekExit;
    }
    
    status = NvAmrInit(pState);         

    if (status != NvSuccess)
        goto AmrSeekExit;

    if (pState->m_MgkFlag)
    {
        /* Read the First Byte from the File */
        for (;;)
        {
            BytesToRead = 1;
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&toc, BytesToRead);

            if(status != NvSuccess)
                goto AmrSeekExit;
            pState->TotalReadBytes += BytesToRead;
            ft = (toc >> 3) & 0x0F;
            
            // Read those many bytes
            BytesToRead = PackedSize[ft];
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)PackedBits, BytesToRead);

            if(status != NvSuccess)
                goto AmrSeekExit;
            pState->TotalReadBytes += BytesToRead;
            pState->m_SeekFlag = NV_TRUE;
            FrameTime += (20 * 10 * 1000);
            FrameCnt++;
            if (FrameTime >= seekTime)
            {                             
                break;
            }
        }     
    }

AmrSeekExit:

    if(status != NvSuccess)
    {
        if(status == NvError_EndOfFile)
            status = NvError_ParserEndOfStream;
        else
            status = NvError_ParserReadFailure;
    }
    return status;
}

NvError NvAwbSeek(NvAmrParser *pState, NvS64 seekTime)
{
    NvError status = NvSuccess;  
    NvU8 toc,ft, FrameCnt = 0;
    NvU8 PackedBits[MAX_SERIAL_SIZE];
    NvU16 PackedSize[16] = {17, 23, 32, 36, 40, 46, 50, 58, 60,  5,  0,  0,  0,  0,  0,  0}; 
    NvU32 BytesToRead = 0;
    NvS64 FrameTime;
    NvS32 tempseek = NvOsStrlen(AWB_MAGIC_NUMBER);

    FrameTime = 0;

    if (seekTime == 0)
    {
        status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempseek, CP_OriginBegin);         
        pState->TotalReadBytes = tempseek;
        goto AwbSeekExit;
    }    
    
    status = NvAwbInit(pState);        

    if (status != NvSuccess)
        goto AwbSeekExit;    

    if (pState->m_MgkFlag)
    {
        /* Read the First Byte from the File */
        for (;;)
        {
            BytesToRead = 1;
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&toc, BytesToRead);

            if(status != NvSuccess)
                 goto AwbSeekExit;
            pState->TotalReadBytes += BytesToRead;
            ft = (toc >> 3) & 0x0F;
            
            // Read bytes for packet
            BytesToRead = PackedSize[ft] ;                
            
            status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)PackedBits, BytesToRead);

            if(status != NvSuccess)
                 goto AwbSeekExit;
            pState->TotalReadBytes += BytesToRead;
            pState->m_SeekFlag = NV_TRUE;
            
            FrameTime += (20 * 10 * 1000);
            FrameCnt++;                        
            if (FrameTime >= seekTime)
            {                            
                break;
            }
        }     
    }

AwbSeekExit:
    if(status != NvSuccess)
    {
        if(status == NvError_EndOfFile)
            status = NvError_ParserEndOfStream;
        else
            status = NvError_ParserReadFailure;
    }
    return status;
}


/**
 * @brief               This function serializes the packed information depending
 *                      upon the frame type
 * @param[in]           q is Q-bit
 * @param[in]           ft is the value of Frame Type
 * @param[in]           PackedBits[] points to packed bit data
 * @param[out]          pMode returns the mode information
 * @param[out]          bits[] returns the serial bits
 * @returns             The frame type
 */
NvAmrFrameType NvAmrUnpackBits (NvU8 q, NvU16 ft, NvU8 PackedBits[], NvAmrMode *pMode, NvU8 bits[])
{
    NvU16 i, sid_type;
    NvU8 *pack_ptr, temp;

    pack_ptr = (NvU8 *)PackedBits;

    /* real NO_DATA frame or unspecified frame type */
    if (ft == 15 || (ft > 8 && ft < 15))
    {
        *pMode = (NvAmrMode)-1;
        return NvAmrFrameType_NoData;
    }

    temp = *pack_ptr;
    pack_ptr++;

    for (i = 1; i < Unpacked_size[ft] + 1; i++)
    {
        if (temp & 0x80)
        {
            bits[Sort_ptr[ft][i-1]] = BIT_1;
        }
        else
        {
            bits[Sort_ptr[ft][i-1]] = BIT_0;
        }

        if (i % 8)
        {
            temp <<= 1;
        }
        else
        {
            temp = *pack_ptr;
            pack_ptr++;
        }
    }

    /* SID frame */
    if (ft == NvAmrMode_Mrdtx)
    {
        if (temp & 0x80)
        {
            sid_type = 1;
        }
        else
        {
            sid_type = 0;
        }

        *pMode = (NvAmrMode)((temp >> 4) & 0x07);

        if (q)
        {
            if (sid_type)
            {
                return NvAmrFrameType_SidUpdate;
            }
            else
            {
                return NvAmrFrameType_SidFirst;
            }
        }
        else
        {
            return NvAmrFrameType_SidBad;
        }
    }
    /* speech frame */
    else
    {
        *pMode = (NvAmrMode)ft;

        if (q)
        {
            return NvAmrFrameType_SpeechGood;
        }
        else
        {
            return NvAmrFrameType_SidBad;
        }
    }

}

NvError NvAmrGetNextFrame(NvAmrParser *pState, NvU8 *pData, NvU32 askBytes, NvU32 *bytesRead)
{
    NvError status = NvSuccess;  
    NvU32 ByteToRead;
    NvU8 toc, ft;
    NvU16 PackedSize[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};
    *bytesRead = 0;
    
    ByteToRead = 1;
    status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&toc, ByteToRead);
    if(status != NvSuccess)
    {
         if(status == NvError_EndOfFile)
         {
            pState->TotalReadBytes += ByteToRead;
            return NvError_ParserEndOfStream;
        }
        else
        {
            return NvError_ParserReadFailure;
        }
    }
    pState->TotalReadBytes += ByteToRead;
    ft = (toc >> 3) & 0x0F;
    ByteToRead = (NvU32)PackedSize[ft];

    pData[0] = toc;
    
    status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&pData[1], ByteToRead);
    if(status != NvSuccess)
    {
         if(status == NvError_EndOfFile)
        {
            pState->TotalReadBytes += ByteToRead;
            return NvError_ParserEndOfStream;
        }
        else
        {
            return NvError_ParserReadFailure;
        }
    }
    pState->TotalReadBytes += ByteToRead;
    *bytesRead =ByteToRead+1;

    return status;
}



NvError NvAwbGetNextFrame(NvAmrParser *pState, NvU8 *pData, NvU32 askBytes, NvU32 *bytesRead)
{
    NvError status = NvSuccess;  
    const NvU8 block_size[16] = {17, 23, 32, 36, 40, 46, 50, 58, 60,  5,  0,  0,  0,  0,  0,  0}; 
    NvU8 m_Serial;
    NvS16 m_Mode;
    NvU32 ByteToRead;
    *bytesRead = 0;

    ByteToRead = 1;
    status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&m_Serial, ByteToRead);
    if(status != NvSuccess)
    {
         if(status == NvError_EndOfFile)
        {
            pState->TotalReadBytes += ByteToRead;
            return NvError_ParserEndOfStream;
        }
        else
        {
            return NvError_ParserReadFailure;
        }
    }
    pState->TotalReadBytes += ByteToRead;
    pData[0] = m_Serial;

    m_Mode = (NvS16)((m_Serial >> 3) & 0x0F);
    
    ByteToRead = (NvU32)(block_size[m_Mode]);

    status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)&pData[1], ByteToRead);
    if(status != NvSuccess)
    {
         if(status == NvError_EndOfFile)
        {
            pState->TotalReadBytes += ByteToRead;
            return NvError_ParserEndOfStream;
        }
        else
        {
            return NvError_ParserReadFailure;
        }
    }
    pState->TotalReadBytes += ByteToRead;
    *bytesRead = ByteToRead+1;

    return status;
}































/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "common/NvxComponent.h"
#include "common/NvxIndexExtensions.h"
#include "common/NvxTrace.h"
#include "OMX_ContentPipe.h"

#include "nvos.h"
#include "nvassert.h"

#include "../nvmm/include/nvmm_contentpipe.h"

#include "nvutil.h"
#include "nvmm_logger.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

typedef enum {
    FW_Raw = 0,
    FW_Audio,
    FW_Image,
    FW_ImageSequence,
    FW_Video,
    FW_VideoPacketHeader,
    FW_AudioPCM,
    FW_AudioAMR,
    FW_DEFAULT = 0x7fffffff
} FW_TYPE;

typedef struct {
    CPhandle cphandle;
    OMX_STRING pFilename;
    OMX_STRING pFilenameWithIndex;
    FILE *  hFile;
    CP_PIPETYPE *pPipe;
    OMX_BOOL   bSentEOS;
    OMX_BOOL   bHasBufferToDeliver;
    OMX_BOOL   bWriteSizeInStream;
    NvU32    nCurrentFileId;
    FW_TYPE  eFileWriterType;
    NvU64 FileSize;
    NvU32 WavHeaderSize;
    NvU64 MaxFileSize;
    NvU64 MaxDuration;
    NvU64 CurrentDuration;
    OMX_AUDIO_PCMMODETYPE pcmMode;
} ComponentState;

#define FILE_WRITER_BUFFER_COUNT 4
//#define FILE_WRITER_BUFFER_SIZE 50*1024
#define FILE_WRITER_BUFFER_SIZE 8*1024

#define LENGTH_INDEX        8
#define LENGTH_FILE_INDEX   (LENGTH_INDEX + 4 + 1)   // 12345678.jpg
#define FILE_SUFFIX          ".jpg"

#define _INDEX_STRING(_len)     #_len
#define INDEX_STRING(_len)      _INDEX_STRING(_len)

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM     (0x0001)
#endif
#ifndef WAVE_FORMAT_ADPCM
#define WAVE_FORMAT_ADPCM  (0x0002)
#endif
#ifndef WAVE_FORMAT_ALAW
#define WAVE_FORMAT_ALAW  (0x0006)
#endif
#ifndef WAVE_FORMAT_MULAW
#define WAVE_FORMAT_MULAW  (0x0007)
#endif

#define WAVE_FILE_COMPRESSION_TYPE_OFFSET 20

static NvU8 RawAmrHeader[]=
{
    0x23,0x21,0x41,0x4d,0x52,0x0a
};

#define NO_POSSIBLE_MODES 16
static const NvU32 IetfDecInputBytes[NO_POSSIBLE_MODES] =
{
    13, /* 4.75 */
    14, /* 5.15 */
    16, /* 5.90 */
    18, /* 6.70 */
    20, /* 7.40 */
    21, /* 7.95 */
    27, /* 10.2 */
    32, /* 12.2 */
    6, /* GsmAmr comfort noise */
    7, /* Gsm-Efr comfort noise */
    6, /* IS-641 comfort noise */
    6, /* Pdc-Efr comfort noise */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1, /* future use; 0 length but set to 1 to skip the frame type byte */
    1  /* No transmission */
};

/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

/* Init functions */
OMX_ERRORTYPE NvxRawFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAudioFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWavFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxImageFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxImageSequenceFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileWriterPacketHeaderInit(OMX_IN OMX_HANDLETYPE hComponent);
void GetPcmModeFromHeader(ComponentState *pState, NvU8* buffer);
OMX_ERRORTYPE GetAMRBufferDurationInMS(NvU8* buffer, NvU32 bufferLength, NvU64 * pDuration);

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static OMX_ERRORTYPE NvxFileWriterDeInit(NvxComponent* pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ComponentState *pState = pNvComp->pComponentData;
   
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxFileWriterDeInit"));

    /* need to close file here  if still open*/
    if (pState) {
        NvOsFree(pState->pFilename);           
        NvOsFree(pState->pFilenameWithIndex);
        NvOsFree(pState);
    }
    return eError;
}

static OMX_ERRORTYPE NvxFileWriterSetParameter(
    OMX_IN NvxComponent *pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pParam)
{
    ComponentState *pState = pNvComp->pComponentData;
       
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxFileWriterSetParameter"));

    switch (nIndex)
    {
       case NVX_IndexParamFilename: 
       {
        NVX_PARAM_FILENAME *pFilenameParam = pParam;

        NvOsFree(pState->pFilename);
        pState->pFilename = NvOsAlloc( NvOsStrlen(pFilenameParam->pFilename) + 1 );
        if(!pState->pFilename)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterSetParameter:"
               ":pFilename = %s:[%s(%d)]\n",pState->pFilename
               , __FILE__, __LINE__));
            return OMX_ErrorInsufficientResources; 
        }
        NvOsStrncpy(pState->pFilename, pFilenameParam->pFilename, NvOsStrlen(pFilenameParam->pFilename) + 1);
        
        NvOsFree(pState->pFilenameWithIndex);
        pState->pFilenameWithIndex = NvOsAlloc( NvOsStrlen(pState->pFilename) + LENGTH_FILE_INDEX );
        if(!pState->pFilenameWithIndex)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterSetParameter:"
               ":pFilenameWithIndex = %s%03d%s:[%s(%d)]\n", pState->pFilename, pState->nCurrentFileId, FILE_SUFFIX
               , __FILE__, __LINE__));
            return OMX_ErrorInsufficientResources; 
        }

        return OMX_ErrorNone;
       }
       case OMX_IndexParamCustomContentPipe:
       {
           OMX_PARAM_CONTENTPIPETYPE *pContentPipe = pParam ; 
           pState->pPipe  = (CP_PIPETYPE *)pContentPipe->hPipe;
            return OMX_ErrorNone;
       }
       case NVX_IndexParamWriterFileSize:
       {
           NVX_PARAM_WRITERFILESIZE *pFileSize = (NVX_PARAM_WRITERFILESIZE *)pParam;
           pState->MaxFileSize = pFileSize->nFileSize;
            return OMX_ErrorNone;
       }
       case NVX_IndexParamDuration:
       {
           NVX_PARAM_DURATION *pDur = (NVX_PARAM_DURATION *)pParam;
           pState->MaxDuration = pDur->nDuration;
            return OMX_ErrorNone;
       }
       default:
        return NvxComponentBaseSetParameter(pNvComp, nIndex, pParam);
    }
}

static OMX_ERRORTYPE NvxFileWriterGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    ComponentState *pState;

    NV_ASSERT(pNvComp && pComponentConfigStructure);
       
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "%s\n", "--NvxFileWriterGetConfig"));

    pState = (ComponentState *)pNvComp->pComponentData;

    switch (nIndex)
    {
        case NVX_IndexParamFilename:
        {
            NVX_PARAM_FILENAME *pFilenameParam = (NVX_PARAM_FILENAME *)pComponentConfigStructure;

            if (FW_ImageSequence == pState->eFileWriterType)
            {
                NvOsStrncpy(pFilenameParam->pFilename, pState->pFilenameWithIndex, NvOsStrlen(pState->pFilenameWithIndex) + 1);
            }
            else
            {
                NvOsStrncpy(pFilenameParam->pFilename, pState->pFilename, NvOsStrlen(pState->pFilename) + 1);
            }
        }
        break;
        default: 
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}


void GetPcmModeFromHeader(ComponentState *pState, NvU8* buffer)
{
    NvU8 *ptr = buffer + WAVE_FILE_COMPRESSION_TYPE_OFFSET;
    NvU32 CodecType;
    
    CodecType = (NvU32)*ptr++;
    CodecType |= (((NvU32)*ptr++) << 8);

   switch (CodecType)
   {
       case WAVE_FORMAT_PCM: //PCM
            pState->pcmMode = OMX_AUDIO_PCMModeLinear;
            break;
       case WAVE_FORMAT_ALAW: //a lAW
            pState->pcmMode = OMX_AUDIO_PCMModeALaw;
            break;
       case WAVE_FORMAT_MULAW: // MU law
            pState->pcmMode = OMX_AUDIO_PCMModeMULaw;
            break;
       default:
            pState->pcmMode = OMX_AUDIO_PCMModeLinear;
       break;
   }
  
}

static OMX_ERRORTYPE NvxFileWriterWorkerFunction (
    OMX_IN NvxComponent *pNvComp,
    OMX_BOOL bAllPortsReady,   
    OMX_OUT OMX_BOOL *pbMoreWork, 
    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    OMX_BUFFERHEADERTYPE *pBuffer;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ComponentState *pState = pNvComp->pComponentData;
    NvxPort* pPortIn = &pNvComp->pPorts[0];
    
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxFileWriterWorkerFunction"));

    *pbMoreWork = bAllPortsReady;
    if (!bAllPortsReady)
        return OMX_ErrorNone;

    if (!pPortIn->pCurrentBufferHdr)
        return OMX_ErrorNone;

    pBuffer = pPortIn->pCurrentBufferHdr;

    /*
    If some buffers arrive after EOS is received, discard the data and
    release the buffers. This is required for passing the BufferFlagTest 
    (conformance tests).
    */
    if(!pState->bSentEOS) 
    {
        if (FW_ImageSequence == pState->eFileWriterType && !pState->cphandle && pBuffer->nFilledLen)
        {
            NvOsStatType stat = { 0, NvOsFileType_File };
            NvError err;

            do {
                NvOsSnprintf(pState->pFilenameWithIndex, NvOsStrlen(pState->pFilename) + LENGTH_FILE_INDEX, "%s%0" INDEX_STRING(LENGTH_INDEX) "d%s", pState->pFilename, pState->nCurrentFileId, FILE_SUFFIX);
                err = NvOsStat(pState->pFilenameWithIndex, &stat);
                pState->nCurrentFileId++;
            } while (err == NvSuccess && stat.size != 0);

            eError = pState->pPipe->Open(&pState->cphandle, pState->pFilenameWithIndex, CP_AccessWrite);
            if (eError != OMX_ErrorNone)
            {
                NVXTRACE((NVXT_ERROR, pNvComp->eObjectType, "ERROR:NvxFileWriterWorkerFunction:"
                   ":eError = %x:[%s(%d)]\n", eError , __FILE__, __LINE__));
                *pbMoreWork = OMX_FALSE;
            }
        }

        if ( pState->bWriteSizeInStream && pState->cphandle && pBuffer->nFilledLen)
        {
            eError = pState->pPipe->Write(pState->cphandle, 
                                          (CPbyte *)&pBuffer->nFilledLen,
                                          sizeof(OMX_U32)); 
            if (eError != OMX_ErrorNone)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterWorkerFunction:"
                   ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
                *pbMoreWork = OMX_FALSE;
            }
        }

        if (pState->cphandle && pBuffer->nFilledLen)
        {
            if (pState->eFileWriterType == FW_AudioPCM)
            {
                pState->FileSize += pBuffer->nFilledLen;
                if (pState->WavHeaderSize == 0)
                {
                    pState->WavHeaderSize = pBuffer->nFilledLen;
                    GetPcmModeFromHeader(pState, (NvU8*)pBuffer->pBuffer);
                }
                    
            }
            if (pState->eFileWriterType == FW_AudioAMR)
            {
                if (pState->FileSize == 0)
                {
                    pState->FileSize += sizeof(RawAmrHeader);

                    eError = pState->pPipe->Write(pState->cphandle,
                                                  (CPbyte*)RawAmrHeader,
                                                  sizeof(RawAmrHeader));
                    if (eError != OMX_ErrorNone)
                    {
                        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterWorkerFunction:"
                           ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
                        *pbMoreWork = OMX_FALSE;
                    }
                }

                pState->FileSize += pBuffer->nFilledLen;
                //Check for the MaxFileSize
                if ((pState->MaxFileSize) && (pState->FileSize > pState->MaxFileSize))
                {
                    //We are exceeding the file size limit. Return an error.
                    pState->FileSize -= pBuffer->nFilledLen;
                    *pbMoreWork = OMX_FALSE;
                    return NvxError_WriterFileSizeLimitExceeded;
                }

                //Check for the MaxDuration
                if (pState->MaxDuration)
                {
                    NvU64 Duration = 0;
                    eError = GetAMRBufferDurationInMS(pBuffer->pBuffer, pBuffer->nFilledLen, &Duration);
                    if (eError != OMX_ErrorNone)
                    {
                        *pbMoreWork = OMX_FALSE;
                        return OMX_ErrorStreamCorrupt;
                    }

                    pState->CurrentDuration += Duration;
                    if (pState->MaxDuration < pState->CurrentDuration)
                    {
                        pState->CurrentDuration -= Duration;
                        *pbMoreWork = OMX_FALSE;
                        return NvxError_WriterTimeLimitExceeded;
                    }
                }
            }
            eError = pState->pPipe->Write(pState->cphandle, 
                                          (CPbyte*)pBuffer->pBuffer,
                                          pBuffer->nFilledLen);
            if (eError != OMX_ErrorNone)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterWorkerFunction:"
                   ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
                *pbMoreWork = OMX_FALSE;
            }
        }

        if ((pBuffer->nFlags & OMX_BUFFERFLAG_EOS) || (OMX_BUFFERFLAG_ENDOFFRAME & pBuffer->nFlags))
        {
            if (FW_ImageSequence == pState->eFileWriterType)
            {
                if (pState->cphandle)
                {
                    pState->pPipe->Close(pState->cphandle);
                    pState->cphandle = 0;
                }

                if (OMX_BUFFERFLAG_ENDOFFRAME & pBuffer->nFlags)
                {
                    NvxSendEvent(pNvComp, OMX_EventBufferFlag, 0, OMX_BUFFERFLAG_ENDOFFRAME, 0);
                    pBuffer->nFlags &= ~OMX_BUFFERFLAG_ENDOFFRAME;
                }
                if (OMX_BUFFERFLAG_EOS & pBuffer->nFlags)
                {
                    NvxSendEvent(pNvComp, OMX_EventBufferFlag, 0, OMX_BUFFERFLAG_EOS, 0);
                    pBuffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
                }
            }
            else if (OMX_BUFFERFLAG_EOS & pBuffer->nFlags)
            {
                pState->bSentEOS = OMX_TRUE;
                *pbMoreWork = OMX_FALSE;
            }
            //printf("\n----- FileWriter: OMX_BUFFERFLAG_EOS Received\n");
        }
    }

    NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
    NvxPortGetNextHdr(pPortIn);
    if (pPortIn->pCurrentBufferHdr) 
        *pbMoreWork = OMX_TRUE;

    return eError;
}

static OMX_ERRORTYPE NvxFileWriterOpen(OMX_IN NvxComponent *pNvComp )
{
    OMX_ERRORTYPE eError;
    ComponentState *pState = pNvComp->pComponentData;

    
   NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxFileWriterOpen"));

    if (!pState->pFilename)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterOpen:"
               ":pFilename = %s:[%s(%d)]\n",pState->pFilename , __FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    eError = NvxComponentBaseAcquireResources(pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterOpen:"
               ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
        return eError;
    }

    if (FW_ImageSequence != pState->eFileWriterType)
    {
        eError = pState->pPipe->Open(&pState->cphandle,pState->pFilename,CP_AccessWrite);
    }

    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterOpen:"
               ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
        return eError;
    }

    pState->bSentEOS = OMX_FALSE;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxFileWriterClose(OMX_IN NvxComponent *pNvComp )
{
    ComponentState *pState = pNvComp->pComponentData;
    
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxFileWriterClose"));

    if (pState->cphandle) 
    {
        if (pState->eFileWriterType == FW_AudioPCM)
        {
            OMX_ERRORTYPE eError = OMX_ErrorNone;
            NvU32 temp;
            // Update _RIFF_ chunk data size
            temp = pState->FileSize - 8;
            pState->pPipe->SetPosition(pState->cphandle, 4, CP_OriginBegin);
            eError = pState->pPipe->Write(pState->cphandle, 
                                              (CPbyte*)&temp,
                                              4);
            if (eError != OMX_ErrorNone)
            {
                    NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterClose:"
                   ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
            }
            // Update _DATA_ chunk data size
            temp = pState->FileSize - pState->WavHeaderSize;
            pState->pPipe->SetPosition(pState->cphandle, (pState->WavHeaderSize - 4), CP_OriginBegin);
            eError = pState->pPipe->Write(pState->cphandle, 
                                  (CPbyte*)&temp,
                                  4);
            if (eError != OMX_ErrorNone)
            {
                    NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterClose:"
                   ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
            }
            // Update _FACT_ chunk data size
            if (pState->pcmMode != OMX_AUDIO_PCMModeLinear)
            {
                temp = (pState->FileSize - pState->WavHeaderSize) >> 1;
                pState->pPipe->SetPosition(pState->cphandle, (pState->WavHeaderSize - 12), CP_OriginBegin);
                eError = pState->pPipe->Write(pState->cphandle, 
                                      (CPbyte*)&temp,
                                      4);
                if (eError != OMX_ErrorNone)
                {
                    NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFileWriterClose:"
                   ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
                }
            }
            pState->FileSize = 0;
            pState->WavHeaderSize = 0;
        }
        if (pState->eFileWriterType == FW_AudioAMR)
        {
            pState->FileSize = 0;
        }
        pState->pPipe->Close(pState->cphandle);
        pState->cphandle = 0; 
    }
    
    return NvxComponentBaseReleaseResources(pNvComp);
}

static OMX_ERRORTYPE CommonInitThis(NvxComponent* pThis, FW_TYPE eFileWriterType)
{
    // Need some default output file in order to pass conformance tests.
    ComponentState *pState;  
    /* #define DEFAULT_OUTPUT_FILENAME "../../../3rdparty/il/version1_0/conformance/data/test.out" */
    #define DEFAULT_OUTPUT_FILENAME "test.out"
    static const char szDefaultFilename[] = DEFAULT_OUTPUT_FILENAME;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pThis->eObjectType = NVXT_FILE_WRITER; 
  
    pThis->DeInit = NvxFileWriterDeInit;
    pThis->SetParameter = NvxFileWriterSetParameter;
    pThis->GetConfig    = NvxFileWriterGetConfig;
    pThis->WorkerFunction = NvxFileWriterWorkerFunction;
    pThis->AcquireResources = NvxFileWriterOpen;
    pThis->ReleaseResources = NvxFileWriterClose;
    
    pThis->pComponentData = NvOsAlloc(sizeof(ComponentState));
    if(!pThis->pComponentData)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
               ":pComponentData = %p:[%s(%d)]\n",pThis->pComponentData , __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    NvOsMemset(pThis->pComponentData, 0, sizeof(ComponentState));

    // Use some default file for conformance tests.
    pState = pThis->pComponentData; 
    pState->pFilename = NvOsAlloc(sizeof(DEFAULT_OUTPUT_FILENAME));
    if(!pState->pFilename)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
               ":pFilename = %s [%s(%d)]\n",pState->pFilename , __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    NvOsStrncpy(pState->pFilename, szDefaultFilename, NvOsStrlen(szDefaultFilename) + 1);
    pState->nCurrentFileId = 0;
    pState->pFilenameWithIndex = NvOsAlloc( NvOsStrlen(pState->pFilename) + LENGTH_FILE_INDEX );
    if(!pState->pFilenameWithIndex)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
               ":pFilenameWithIndex = %s%03d%s:[%s(%d)]\n", pState->pFilename, pState->nCurrentFileId, FILE_SUFFIX
               , __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    
    if (eFileWriterType == FW_AudioPCM || eFileWriterType == FW_AudioAMR)
        NvmmGetFileContentPipe((CP_PIPETYPE_EXTENDED **) &pState->cphandle);
    else 
        eError =  OMX_GetContentPipe(&pState->cphandle ,pState->pFilename);

    pState->pPipe =  (CP_PIPETYPE *)pState->cphandle;
    pState->cphandle = 0;
    pState->bWriteSizeInStream = OMX_FALSE;
    pState->eFileWriterType = eFileWriterType;
    return eError; 
}

OMX_ERRORTYPE NvxRawFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);   
     
    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_Raw);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxRawFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.raw.write";
        pNvComp->nComponentRoles = 1;
        pNvComp->sComponentRoles[0] = "raw_writer.binary";

        NvxPortInitOther(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_OTHER_FormatStats /* NVX_OTHER_FormatByteStream */);
    }
    return eError;
}

OMX_ERRORTYPE NvxAudioFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);
    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_Audio);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.audio.write";
        pNvComp->nComponentRoles = 1;     
        pNvComp->sComponentRoles[0] = "audio_writer.binary";

        NvxPortInitAudio(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_AUDIO_CodingAutoDetect);
    }
    return eError;
} 

OMX_ERRORTYPE NvxWavFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);
    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_AudioPCM);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxWavFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.wav.write";
        pNvComp->nComponentRoles = 1;     
        pNvComp->sComponentRoles[0] = "wav_writer.binary";

        NvxPortInitAudio(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_AUDIO_CodingAutoDetect);
    }
    return eError;
} 

OMX_ERRORTYPE NvxAmrFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);
    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_AudioAMR);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAmrFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError;
        }
        pNvComp->pComponentName = "OMX.Nvidia.amr.write";
        pNvComp->nComponentRoles = 1;
        pNvComp->sComponentRoles[0] = "amr_writer.binary";

        NvxPortInitAudio(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_AUDIO_CodingAMR);
    }
    return eError;
}

OMX_ERRORTYPE NvxImageFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);
    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_Image);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxImageFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.image.write";
        pNvComp->nComponentRoles = 1;
        pNvComp->sComponentRoles[0] = "image_writer.binary";

        NvxPortInitImage(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_IMAGE_CodingAutoDetect);
    }
    return eError;
} 

OMX_ERRORTYPE NvxImageSequenceFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);
    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_ImageSequence);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxImageFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.imagesequence.write";
        pNvComp->nComponentRoles = 1;
        pNvComp->sComponentRoles[0] = "image_writer.binary";

        NvxPortInitImage(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_IMAGE_CodingAutoDetect);
    }
    return eError;
} 

OMX_ERRORTYPE NvxVideoFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);

    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pNvComp, FW_Video);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxVideoFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.video.write";
        pNvComp->nComponentRoles = 1;
        pNvComp->sComponentRoles[0] = "video_writer.binary";

        NvxPortInitVideo(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_VIDEO_CodingAutoDetect);
    }
    return eError;
} 

OMX_ERRORTYPE NvxVideoFileWriterPacketHeaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,1,&pNvComp);

    if (NvxIsSuccess(eError)) 
    {
        ComponentState *pState = NULL; 
        eError = CommonInitThis(pNvComp, FW_VideoPacketHeader);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxVideoFileWriterInit:"
               ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
            return eError; 
        }
        pNvComp->pComponentName = "OMX.Nvidia.vidhdr.write";
        pNvComp->nComponentRoles = 1;
        pNvComp->sComponentRoles[0] = "video_writer.binary";

        pState = pNvComp->pComponentData; 
        pState->bWriteSizeInStream = OMX_TRUE; 

        NvxPortInitVideo(&pNvComp->pPorts[0], OMX_DirInput,
                         FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                         OMX_VIDEO_CodingAutoDetect);
    }
    return eError;
} 

OMX_ERRORTYPE GetAMRBufferDurationInMS(NvU8* buffer, NvU32 bufferLength, NvU64 * pDuration)
{
    NvU32 NumberOfFrames = 0;
    NvU32 BufferTracker = 0;
    while (BufferTracker < bufferLength)
    {
        BufferTracker += IetfDecInputBytes[((buffer[BufferTracker] >> 3) & 0x0f)];
        NumberOfFrames++;
    }
    *pDuration = NumberOfFrames * 20; //in MS

    return OMX_ErrorNone;
}

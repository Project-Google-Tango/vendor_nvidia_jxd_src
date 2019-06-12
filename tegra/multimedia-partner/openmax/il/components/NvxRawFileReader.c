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

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

typedef struct {
    CPhandle cphandle;
    OMX_STRING pFilename;
    FILE *  hFile;
    CP_PIPETYPE *pPipe;
    OMX_BOOL   bSentEOS;
    OMX_BOOL   bHasBufferToDeliver;
    OMX_BOOL   bReadSizeFromStream;
    OMX_U32    nReadSize;
    OMX_TICKS  nTimeCount;
} ComponentState;

#define FILE_READER_BUFFER_COUNT 20
#define FILE_READER_BUFFER_SIZE 2048
#define LARGE_FILE_READER_BUFFER_COUNT 3
#define LARGE_FILE_READER_BUFFER_SIZE 640*480*4
/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

/* Init functions */
OMX_ERRORTYPE NvxRawFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAudioFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxImageFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLargeVideoFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileReaderPacketHeaderInit(OMX_IN OMX_HANDLETYPE hComponent);

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static OMX_ERRORTYPE NvxFileReaderOpen(OMX_IN NvxComponent *pThis )
{  

    OMX_ERRORTYPE eError;
    ComponentState *pState = pThis->pComponentData;

    if (!pState->pFilename)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxFileReaderOpen:"
            ":pFilename = %s :[%s(%d)]\n",pState->pFilename, __FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    eError = NvxComponentBaseAcquireResources(pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxFileReaderOpen:"
            ":eError = %x :[%s(%d)]\n",eError, __FILE__, __LINE__));
        return eError;
    }

    eError = pState->pPipe->Open(&pState->cphandle,pState->pFilename,CP_AccessRead);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxFileReaderOpen:"
            ":eError = %x :[%s(%d)]\n",eError, __FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    
    pState->nTimeCount = 0; 
    pState->bSentEOS = OMX_FALSE;

    return OMX_ErrorNone;   
}

static OMX_ERRORTYPE NvxFileReaderWorkerFunction (
    OMX_IN NvxComponent *pThis,
    OMX_BOOL bAllPortsReady,            /* OMX_TRUE if all ports have a buffer ready */
    OMX_OUT OMX_BOOL *pbMoreWork,      /* function should set to OMX_TRUE if there is more work */
    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_S32 askBytes;
    ComponentState *pState = pThis->pComponentData;
    NvxPort* pPortOut = &pThis->pPorts[0];
    OMX_BUFFERHEADERTYPE *pBuffer;
    CP_CHECKBYTESRESULTTYPE eResult;
    OMX_U32 pPosition,endposition;

    NVXTRACE((NVXT_CALLGRAPH,pThis->eObjectType,"%s\n","--NvxFileReaderWorkerFunction"));  

    if (!bAllPortsReady || pState->bSentEOS)
        return OMX_ErrorNone;

    if (!pPortOut->pCurrentBufferHdr)
        return OMX_ErrorNone;

    pBuffer = pPortOut->pCurrentBufferHdr;

    if (!pState->bHasBufferToDeliver)
    {
        OMX_U32 nFrameLimit = (pState->nReadSize) ? pState->nReadSize : pBuffer->nAllocLen;

        // FIXME: Most of this goes away when CP's ReadBuffer is implemented
        askBytes = (OMX_S32)(nFrameLimit - pBuffer->nFilledLen);

        if ( pState->bReadSizeFromStream )
        {
            OMX_U32 nReadSize = 0;
            eError = pState->pPipe->CheckAvailableBytes(pState->cphandle, 
                sizeof(OMX_U32),
                &eResult);
            if( eResult == CP_CheckBytesOk)
            {
                eError = pState->pPipe->Read(pState->cphandle,
                                         (CPbyte *)&nReadSize, sizeof(OMX_U32));
                askBytes = (OMX_S32)nReadSize;
            }
            else if (eResult == CP_CheckBytesInsufficientBytes) 
            {
                askBytes = 0; 
            }
        }

        if (askBytes > 0)
        {
            eError = pState->pPipe->CheckAvailableBytes(pState->cphandle, askBytes,
                                                    &eResult); 
            if( eResult == CP_CheckBytesOk)
            {
                eError = pState->pPipe->Read(pState->cphandle,
                                         (CPbyte*)pBuffer->pBuffer, askBytes);
                pBuffer->nFilledLen = askBytes;
            }  

            if (eResult == CP_CheckBytesInsufficientBytes) 
            {
                pState->pPipe->GetPosition(pState->cphandle, &pPosition);
                pState->pPipe->SetPosition(pState->cphandle, 0, CP_OriginEnd) ;
                eError = pState->pPipe->GetPosition(pState->cphandle, &endposition);
                pState->pPipe->SetPosition(pState->cphandle, pPosition, SEEK_SET);

                eError = pState->pPipe->Read(pState->cphandle, 
                                         (CPbyte*)pBuffer->pBuffer,
                                         endposition - pPosition);
                pBuffer->nFilledLen = endposition - pPosition;
                pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
                *pbMoreWork = OMX_FALSE;
                pState->bSentEOS = OMX_TRUE;
            }

            if (pState->bReadSizeFromStream)
            {
                pBuffer->nTimeStamp = pState->nTimeCount;
                pState->nTimeCount += (OMX_TICKS)((OMX_TICKS)OMX_TICKS_PER_SECOND/(OMX_TICKS)30);
            }
        }
        else
        {
            pBuffer->nFilledLen = 0;
            pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
            *pbMoreWork = OMX_FALSE;
            pState->bSentEOS = OMX_TRUE;
        }
    }

    eError = NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
    if (eError == OMX_ErrorInsufficientResources)
    {
        pState->bHasBufferToDeliver = OMX_TRUE;
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxFileReaderWorkerFunction:"
            ":eError = %x :[%s(%d)]\n",eError, __FILE__, __LINE__));
        return eError;
    }
    pState->bHasBufferToDeliver = OMX_FALSE;

    NvxPortGetNextHdr(pPortOut);
    *pbMoreWork = !!(pPortOut->pCurrentBufferHdr);

    return eError;
}

static OMX_ERRORTYPE NvxFileReaderClose(OMX_IN NvxComponent *pThis )
{
    ComponentState *pState = pThis->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH,pThis->eObjectType,"%s\n","--NvxFileReaderClose"));
    if(pState->cphandle) 
    {
        pState->pPipe->Close(pState->cphandle);
        pState->cphandle = 0;
    }

    return NvxComponentBaseReleaseResources(pThis);
}

static OMX_ERRORTYPE NvxFileReaderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    OMX_U32 uPortIndex = 0;

    switch(nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        uPortIndex = pPortDef->nPortIndex;
        if(uPortIndex >= pComponent->nPorts) 
        {
            NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxCameraGetParameter:"
                ":uPortIndex (%d) >= nPorts (%d):[%s(%d)]\n",
                uPortIndex ,pComponent->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;
        }
        NvOsMemcpy(pPortDef, &pComponent->pPorts[uPortIndex].oPortDef, 
                   pComponent->pPorts[uPortIndex].oPortDef.nSize);
        break;
    default:
        return NvxComponentBaseGetParameter( pComponent, nParamIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}


static OMX_ERRORTYPE NvxFileReaderSetParameter(
    OMX_IN NvxComponent *pThis, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pParam)
{
    ComponentState *pState = pThis->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH,pThis->eObjectType,"%s\n","--NvxFileReaderSetParameter\n"));

    switch (nIndex)
    {
       case NVX_IndexParamFilename: 
       {
            NVX_PARAM_FILENAME *pFilenameParam = pParam;

            NvOsFree(pState->pFilename);
            pState->pFilename = NvOsAlloc(NvOsStrlen(pFilenameParam->pFilename) + 1 );
            if(!pState->pFilename)
            {
                NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxFileReaderSetParameter:"
                    ":pFilename = %s:[%s(%d)]\n",pState->pFilename,__FILE__, __LINE__));
                return OMX_ErrorInsufficientResources; 
            }
            NvOsStrncpy(pState->pFilename, pFilenameParam->pFilename, NvOsStrlen(pFilenameParam->pFilename) + 1);
            NVXTRACE((NVXT_CALLGRAPH,pThis->eObjectType,"--NvxFileReaderSetParameter fname is %s\n",pState->pFilename));
        
            return OMX_ErrorNone;
       }
       case OMX_IndexParamCustomContentPipe:
       {
           OMX_PARAM_CONTENTPIPETYPE *pContentPipe = pParam ; 
           pState->pPipe  = (CP_PIPETYPE *)pContentPipe->hPipe;
            return OMX_ErrorNone;
       }

    default:
        return NvxComponentBaseSetParameter(pThis, nIndex, pParam);
    }
}

static OMX_ERRORTYPE NvxFileReaderDeInit(NvxComponent* pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ComponentState *pState = pThis->pComponentData;

    /* need to close file here  if still open*/

    NVXTRACE((NVXT_CALLGRAPH,pThis->eObjectType,"%s\n","--NvxFileReaderDeInit"));
 
    if (pState)
    {
        NvOsFree(pState->pFilename);
        NvOsFree(pState);
    }
    return eError;
}

static OMX_ERRORTYPE NvxFileReaderGetConfig(
                                             OMX_IN NvxComponent* pNvComp, 
                                             OMX_IN OMX_INDEXTYPE nIndex, 
                                             OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ComponentState *pState = NULL; 

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxFileReaderGetConfig\n"));

    pState = pNvComp->pComponentData;
    switch (nIndex)
    {
        case OMX_IndexConfigVideoNalSize:
            {
                OMX_VIDEO_CONFIG_NALSIZE *pNalSize = (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
                pNalSize->nNaluBytes = pState->nReadSize;
            }
            break;
        default: 
            eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
            break;
    }
    return eError;
}

static OMX_ERRORTYPE NvxFileReaderSetConfig(
                                             OMX_IN NvxComponent* pNvComp, 
                                             OMX_IN OMX_INDEXTYPE nIndex, 
                                             OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ComponentState *pState = NULL; 

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxFileReaderGetConfig\n"));

    pState = pNvComp->pComponentData;
    switch (nIndex)
    {
        case OMX_IndexConfigTimePosition:
        {
            eError = OMX_ErrorNotImplemented;
            break;
        }
        case OMX_IndexConfigVideoNalSize:
            {
                // Change the Network Abstraction Layer size
                OMX_VIDEO_CONFIG_NALSIZE *pNalSize = (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
                //if (pNalSize->nNaluBytes > LARGE_FILE_READER_BUFFER_SIZE)
                //    return OMX_ErrorBadParameter; 
                pNvComp->pPorts[0].nReqBufferSize = pNvComp->pPorts[0].nMinBufferSize = pNalSize->nNaluBytes;
                pNvComp->pPorts[0].oPortDef.nBufferSize = pNalSize->nNaluBytes;
                pState->nReadSize = pNalSize->nNaluBytes;
            }
            break;
        default:  
            eError =  NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
            break;
    }
    return eError;
}

static OMX_ERRORTYPE CommonInitThis(NvxComponent* pThis)
{
    // Need some default input file in order to pass conformance tests.
    ComponentState *pState;  
//#define DEFAULT_INPUT_FILENAME "../../../3rdparty/il/version1_0/conformance/data/test.mp3"
#define DEFAULT_INPUT_FILENAME "test.mp3"
    static const char szDefaultFilename[] = DEFAULT_INPUT_FILENAME;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pThis->eObjectType = NVXT_FILE_READER;
 
    NVXTRACE((NVXT_CALLGRAPH,pThis->eObjectType,"%s\n","--CommonInitThis"));
    pThis->DeInit = NvxFileReaderDeInit;
    pThis->SetParameter = NvxFileReaderSetParameter;
    pThis->WorkerFunction = NvxFileReaderWorkerFunction;
    pThis->AcquireResources = NvxFileReaderOpen;
    pThis->ReleaseResources = NvxFileReaderClose;
    pThis->GetParameter = NvxFileReaderGetParameter;
    pThis->GetConfig    = NvxFileReaderGetConfig;
    pThis->SetConfig    = NvxFileReaderSetConfig;

    pThis->pComponentData = NvOsAlloc(sizeof(ComponentState));
    if(!pThis->pComponentData)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
          ":pComponentData = %p:[%s(%d)]\n",pThis->pComponentData,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    NvOsMemset(pThis->pComponentData, 0, sizeof(ComponentState));

    // Use some default file for conformance tests.
    pState = pThis->pComponentData; 
    pState->pFilename = NvOsAlloc(sizeof(DEFAULT_INPUT_FILENAME));
    if(!pState->pFilename)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
           ":pFilename = %s:[%s(%d)]\n",pState->pFilename,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    NvOsStrncpy(pState->pFilename, szDefaultFilename, NvOsStrlen(szDefaultFilename) + 1);

    eError = OMX_GetContentPipe(&pState->cphandle ,pState->pFilename);
    pState->pPipe =  (CP_PIPETYPE *)pState->cphandle;
    pState->cphandle = 0;
    pState->nReadSize = 0; 
    pState->nTimeCount = 0; 
    pState->bReadSizeFromStream = OMX_FALSE; 
    return eError; 
}

OMX_ERRORTYPE NvxRawFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    eError = NvxComponentCreate(hComponent,1,&pThis);

    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pThis);
        if(NvxIsError(eError))
        {
            NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxRawFileReaderInit:"
              ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
            return eError; 
        }

        pThis->pComponentName = "OMX.Nvidia.raw.read"; 
        pThis->nComponentRoles = 1;
        pThis->sComponentRoles[0] = "raw_reader.binary";

        NvxPortInitOther(&pThis->pPorts[0], OMX_DirOutput, 
                         FILE_READER_BUFFER_COUNT, FILE_READER_BUFFER_SIZE, 
                         OMX_OTHER_FormatStats);
    }
    return eError;
}

OMX_ERRORTYPE NvxAudioFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    eError = NvxComponentCreate(hComponent,1,&pThis);

    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pThis);
        if(NvxIsError(eError))
            return eError; 
        pThis->pComponentName = "OMX.Nvidia.audio.read";
        pThis->nComponentRoles = 1;
        pThis->sComponentRoles[0] = "audio_reader.binary";

        NvxPortInitAudio(&pThis->pPorts[0], OMX_DirOutput, 
                         FILE_READER_BUFFER_COUNT, FILE_READER_BUFFER_SIZE, 
                         OMX_AUDIO_CodingAutoDetect);
    }
    return eError;
}

OMX_ERRORTYPE NvxImageFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    eError = NvxComponentCreate(hComponent,1,&pThis);

    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pThis);
        if(NvxIsError(eError))
            return eError; 
        pThis->pComponentName = "OMX.Nvidia.image.read";
        pThis->nComponentRoles = 1;
        pThis->sComponentRoles[0] = "image_reader.binary";

        NvxPortInitImage(&pThis->pPorts[0], OMX_DirOutput,
                         FILE_READER_BUFFER_COUNT, FILE_READER_BUFFER_SIZE,
                         OMX_IMAGE_CodingAutoDetect);
    }
    return eError;
}

OMX_ERRORTYPE NvxVideoFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    eError = NvxComponentCreate(hComponent,1,&pThis);

    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pThis);
        if(NvxIsError(eError))
            return eError; 
        pThis->pComponentName = "OMX.Nvidia.video.read";
        pThis->nComponentRoles = 1;
        pThis->sComponentRoles[0] = "video_reader.binary";

        NvxPortInitVideo(&pThis->pPorts[0], OMX_DirOutput,
                         FILE_READER_BUFFER_COUNT, FILE_READER_BUFFER_SIZE,
                         OMX_VIDEO_CodingAutoDetect);
    } 
    return eError; 
} 

OMX_ERRORTYPE NvxLargeVideoFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    eError = NvxComponentCreate(hComponent,1,&pThis);

    if (NvxIsSuccess(eError)) {
        eError = CommonInitThis(pThis);
        if(NvxIsError(eError))
            return eError; 
        pThis->pComponentName = "OMX.Nvidia.video.read.large";
        pThis->nComponentRoles = 1;
        pThis->sComponentRoles[0] = "video_reader.binary";

        NvxPortInitVideo(&pThis->pPorts[0], OMX_DirOutput,
                         LARGE_FILE_READER_BUFFER_COUNT, 
                         LARGE_FILE_READER_BUFFER_SIZE,
                         OMX_VIDEO_CodingAutoDetect);
    } 
    return eError; 
} 

OMX_ERRORTYPE NvxVideoFileReaderPacketHeaderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    eError = NvxComponentCreate(hComponent,1,&pThis);

    if (NvxIsSuccess(eError)) 
    {
        ComponentState *pState = NULL; 
        eError = CommonInitThis(pThis);
        if(NvxIsError(eError))
            return eError; 
        pThis->pComponentName = "OMX.Nvidia.vidhdr.read";
        pThis->nComponentRoles = 1;
        pThis->sComponentRoles[0] = "video_reader.binary";

        pState = pThis->pComponentData; 
        pState->bReadSizeFromStream = OMX_TRUE; 

        NvxPortInitVideo(&pThis->pPorts[0], OMX_DirOutput,
                         FILE_READER_BUFFER_COUNT, FILE_READER_BUFFER_SIZE,
                         OMX_VIDEO_CodingAutoDetect);
    } 
    return eError; 
}


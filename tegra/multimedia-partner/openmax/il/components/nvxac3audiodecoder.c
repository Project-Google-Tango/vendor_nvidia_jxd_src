/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** nvxeac3audiodecoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "nvxac3audiodecoder.h"

#include "OMX_IndexExt.h"
#include "OMX_AudioExt.h"

#include "common/NvxComponent.h"
#include "common/NvxTrace.h"
#include "nvmm/components/common/NvxCompMisc.h"
#include "nvos.h"
#include "nvassert.h"

#include "dlfcn.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

#define IN_PORT    0
#define OUT_PORT   1
#define NUM_PORTS  2
#define MAX_INPUT_BUFFERS      5
#define MAX_OUTPUT_BUFFERS     10
#define MIN_INPUT_BUFFERSIZE   24000
#define MIN_OUTPUT_BUFFERSIZE  36864

// Only using the first (main) pcm output
#define MAX_PCM 1

typedef struct NvxAc3WrapperFuncs
{
    NvError (*NvDdpWrapperOpen)(void **pDecHandle, void **pDecHandleAlign, NvS8 **pOutBuf, void **pOutBufAlign);
    NvError (*NvDdpWrapperClose)(void *pDecHandle, void *pDecHandleAlign, NvS8 *pOutBuf);
    NvError (*NvDdpWrapperSetProcessParamOutmode)(void *pDecHandle, NVX_AC3_OUTPUT_MODE);
    unsigned int (*NvDdpWrapperProcessData)
        (void *pDecHandle, const char *pInBuffer, size_t inBufSize,
         NvS8 *pOutBuffer, NVX_AC3_BUFFER_DESC *pChannelBuf);
} NvxAc3WrapperFuncs;

typedef struct SNvxAc3DecoderData
{
    void               *pWrapperLib; //< Dolby wrapper library
    struct NvxAc3WrapperFuncs wrap;  //< Dolby wrapper symbols

    void          *pDecHandle;       //< Dolby decoder handle
    void          *pDecHandleAlign;  //< Dolby decoder handle, memory aligned
    void          *pOutBuf;          //< Dolby output buffer
    void          *pOutBufAlign;     //< Dolby output buffer, memory aligned
    OMX_TICKS     nTimeStamp;        //< Current timestamp in microseconds

    NVX_AC3_TIMESLICE_OUTPUT processOutput; //< Output parameters for Process Time Slice function

    NVX_AUDIO_CONFIG_CAPS renderCaps;       //< Audio renderer capabilities
    NVX_AUDIO_DECODE_DATA decodeData;       //< AC3 frame information
} SNvxAc3DecoderData;

/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

static OMX_ERRORTYPE loadSymbols(void* pWrapperLib, NvxAc3WrapperFuncs *pWrapperFuncs);
static OMX_ERRORTYPE loadSymbol(void* pWrapperLib, void** pWrapperSymbol, char const* pName);
static OMX_ERRORTYPE decodeAC3(SNvxAc3DecoderData *pNvxAudioDecoder,
                               NvS16* pInPtr,
                               NvU32  inputSize,
                               NvS16* pOutPtr,
                               NvU32  outputSize);
static void fixByteOrder(OMX_U16 *pData, int length);

OMX_ERRORTYPE NvxAc3AudioDecoderInit(OMX_IN OMX_HANDLETYPE hComponent);

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static OMX_ERRORTYPE NvxAc3AudioDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAc3DecoderData *pNvxAudioDecoder = 0;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+%s\n",__FUNCTION__));

    pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    if (pNvxAudioDecoder == NULL)
    {
        return eError;
    }

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;

    return eError;
}

static OMX_ERRORTYPE NvxAc3AudioDecoderGetParameter(OMX_IN NvxComponent *pNvComp,
                                                    OMX_IN OMX_INDEXTYPE nIndex,
                                                    OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxAc3DecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+%s\n",__FUNCTION__));

    pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);
    switch (nIndex)
    {
        case OMX_IndexParamAudioAc3:
        {
            OMX_AUDIO_PARAM_AC3TYPE *pAC3Param, *pLocalMode;
            NvxPort *pPortIn = NULL;
            pAC3Param = (OMX_AUDIO_PARAM_AC3TYPE *)pComponentParameterStructure;

            if (pAC3Param->nPortIndex != IN_PORT)
            {
                NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:%s:"
                    ":OMX_ErrorBadParameter:[%s(%d)]\n",__FUNCTION__, __FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }

            pPortIn = &pNvComp->pPorts[IN_PORT];
            pLocalMode = (OMX_AUDIO_PARAM_AC3TYPE *)pPortIn->pPortPrivate;
            NvOsMemcpy(pAC3Param, pLocalMode, sizeof(OMX_AUDIO_PARAM_AC3TYPE));
            break;
        }
        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pPcmparam, *pLocalMode;
            NvxPort *pPortOut = NULL;
            pPcmparam = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            if (pPcmparam->nPortIndex != OUT_PORT)
            {
                NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:%s:"
                    ":OMX_ErrorUndefined:[%s(%d)]\n",__FUNCTION__, __FILE__, __LINE__));
                return OMX_ErrorUndefined;
            }
            pPortOut = &pNvComp->pPorts[OUT_PORT];
            pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;
            NvOsMemcpy(pPcmparam, pLocalMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            break;
        }
        default:
            return NvxComponentBaseGetParameter(pNvComp, nIndex, pComponentParameterStructure);
    }

    return eError;
}

static OMX_ERRORTYPE NvxAc3AudioDecoderSetParameter(OMX_IN NvxComponent *pNvComp,
                                                    OMX_IN OMX_INDEXTYPE nIndex,
                                                    OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxAc3DecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+%s\n",__FUNCTION__));

    pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);
    switch (nIndex)
    {
        case OMX_IndexParamAudioAc3:
        {
            OMX_AUDIO_PARAM_AC3TYPE *pAC3Param = (OMX_AUDIO_PARAM_AC3TYPE *)pComponentParameterStructure;
            if (pAC3Param->nPortIndex != IN_PORT)
            {
                NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:%s:"
                    ":OMX_ErrorBadParameter:[%s(%d)]\n",__FUNCTION__,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }

            NvxPort *pPortIn = &pNvComp->pPorts[IN_PORT];
            OMX_AUDIO_PARAM_AC3TYPE *pLocalMode = (OMX_AUDIO_PARAM_AC3TYPE *)pPortIn->pPortPrivate;
            NvOsMemcpy(pLocalMode, pAC3Param, sizeof(OMX_AUDIO_PARAM_AC3TYPE));
            break;
        }
        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pPCM =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            if (pPCM->nPortIndex != OUT_PORT)
            {
                NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:%s:"
                    ":OMX_ErrorBadParameter:[%s(%d)]\n", __FUNCTION__, __FILE__, __LINE__));
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }
        default:
            return NvxComponentBaseSetParameter(pNvComp, nIndex, pComponentParameterStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAc3AudioDecoderWorkerFunction(OMX_IN NvxComponent *pNvComp,
                                                      OMX_IN OMX_BOOL bAllPortsReady,
                                                      OMX_OUT OMX_BOOL *pbMoreWork,
                                                      OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxAc3DecoderData *pNvxAudioDecoder = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pNvComp->pPorts[IN_PORT];
    NvxPort *pPortOut = &pNvComp->pPorts[OUT_PORT];

    pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);

    if (pPortIn->pCurrentBufferHdr && pPortOut->pCurrentBufferHdr)
    {
        if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            pPortIn->pCurrentBufferHdr->nFilledLen = 0;
            eError = NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
            if (eError == OMX_ErrorNone)
            {
                pPortIn->pCurrentBufferHdr = NULL;
            }

            pPortOut->pCurrentBufferHdr->nFilledLen = 0;
            pPortOut->pCurrentBufferHdr->nOffset = 0;
            pPortOut->pCurrentBufferHdr->nFlags = OMX_BUFFERFLAG_EOS;
            eError = NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
            if (eError == OMX_ErrorNone)
            {
                pPortOut->pCurrentBufferHdr = NULL;
            }
            return OMX_ErrorNone;
        }

        // update current timestamp
        if (pPortIn->pCurrentBufferHdr->nOffset == 0)
        {
            pNvxAudioDecoder->nTimeStamp = pPortIn->pCurrentBufferHdr->nTimeStamp;
        }

        // Use 16 bit pointer for convenience, as ddp uses words, not bytes
        OMX_U16 *pInPtr  = (OMX_U16 *)(pPortIn->pCurrentBufferHdr->pBuffer  + pPortIn->pCurrentBufferHdr->nOffset);
        OMX_U16 *pOutPtr = (OMX_U16 *)(pPortOut->pCurrentBufferHdr->pBuffer + pPortOut->pCurrentBufferHdr->nOffset);

        // fix the endianness in place so that we can properly read/parse the data
        fixByteOrder(&pInPtr[0], pPortIn->pCurrentBufferHdr->nFilledLen / 2);

        // Check sync word, and make sure there is enough data to read the header size
        if (pInPtr[0] != NVX_AC3_SYNCWORD || pPortIn->pCurrentBufferHdr->nFilledLen < NVX_AC3_FRAME_SIZE_WORDS * 2)
        {
            NVXTRACE((NVXT_ERROR, pNvComp->eObjectType, "ERROR:%s: Invalid Data", __FUNCTION__));
            return OMX_ErrorUndefined;
        }

        // Determine whether this is AC-3 or EAC-3. BSID is bits 40-44
        OMX_U16 bsid = (pInPtr[2] & 0xF8) >> 3;
        OMX_BOOL eac3 = OMX_FALSE;
        if (bsid == 0x10)
        {
            eac3 = OMX_TRUE;
        }

        // check whether passthrough is available
        OMX_BOOL const passthru = (eac3 && pNvxAudioDecoder->renderCaps.supportEac3)
                                  || (!eac3 && pNvxAudioDecoder->renderCaps.supportAc3);

        // If the renderer is not able to render the ac3/eac3 content, and the ddp decoding
        // library is available to us, decode into PCM
        if (pNvxAudioDecoder->pWrapperLib && !passthru)
        {
            decodeAC3(pNvxAudioDecoder,
                      pInPtr,
                      pPortIn->pCurrentBufferHdr->nFilledLen,
                      pOutPtr,
                      pPortOut->pCurrentBufferHdr->nAllocLen);
        }
        else
        {
            OMX_AUDIO_CODINGTYPE type = passthru ? OMX_AUDIO_CodingAC3 : OMX_AUDIO_CodingPCM;

            // Send to passthrough. This will either pass silence or the original
            // data, depending on the renderer's capabilities
            NvxBypassProcessAC3(&pNvxAudioDecoder->decodeData,
                                type,
                                pInPtr,
                                pPortIn->pCurrentBufferHdr->nFilledLen,
                                pOutPtr,
                                pPortOut->pCurrentBufferHdr->nAllocLen);
        }

        if (pNvxAudioDecoder->decodeData.bFrameOK && pNvxAudioDecoder->decodeData.outputBytes > 0)
        {
            pPortOut->pCurrentBufferHdr->nFilledLen = pNvxAudioDecoder->decodeData.outputBytes;
            pPortOut->pCurrentBufferHdr->nOffset = 0;
            pPortOut->pCurrentBufferHdr->nTimeStamp =
                pNvxAudioDecoder->nTimeStamp + (pNvxAudioDecoder->decodeData.outputBytes / 2)
                                               * 1000000ll / pNvxAudioDecoder->decodeData.nSampleRate;
        }

        pPortIn->pCurrentBufferHdr->nOffset += pNvxAudioDecoder->decodeData.bytesConsumed;
        pPortIn->pCurrentBufferHdr->nFilledLen -= pNvxAudioDecoder->decodeData.bytesConsumed;
        if (pPortIn->pCurrentBufferHdr->nFilledLen == 0)
        {
            eError = NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
            if (eError == OMX_ErrorNone)
            {
                pPortIn->pCurrentBufferHdr = NULL;
                NvxPortGetNextHdr(pPortIn);
            }
        }
        pPortOut->pCurrentBufferHdr->nOffset = 0;
        pPortOut->pCurrentBufferHdr->nFlags = 0;
        eError = NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
        if (eError == OMX_ErrorNone)
        {
            pPortOut->pCurrentBufferHdr = NULL;
            NvxPortGetNextHdr(pPortOut);
        }
    }

    if (!pPortIn->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortIn);
    }

    if (!pPortOut->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortOut);
    }

    return eError;
}

static OMX_ERRORTYPE NvxAc3AudioDecoderAcquireResources(OMX_IN NvxComponent *pNvComp)
{
    SNvxAc3DecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+%s\n",__FUNCTION__));

    // Get decoder instance
    pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);

    eError = NvxComponentBaseAcquireResources(pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:%s:"
           ":eError = %x:[%s(%d)]\n",__FUNCTION__, eError,__FILE__, __LINE__));
        return eError;
    }

    // Check to see if we have the DDP wrapper library available
    pNvxAudioDecoder->pWrapperLib = dlopen("libnvddpwrapper.so", RTLD_LAZY);
    if (!pNvxAudioDecoder->pWrapperLib)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:%s:"
                  " Could not load Dolby wrapper. dlerror()=%s\n", __FUNCTION__, dlerror()));
        return eError;
    };

    // load the dynamic symbol pointers into memory
    eError = loadSymbols(pNvxAudioDecoder->pWrapperLib, &pNvxAudioDecoder->wrap);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:%s:",
                 " Could not load dynamic symbols from wrapper library.\n", __FUNCTION__));
        return eError;
    }

    // Initialize dolby digital plus library
    pNvxAudioDecoder->pDecHandle = NULL;
    pNvxAudioDecoder->pOutBuf    = NULL;
    if (pNvxAudioDecoder->wrap.NvDdpWrapperOpen(&pNvxAudioDecoder->pDecHandle,
                                                &pNvxAudioDecoder->pDecHandleAlign,
                                                &pNvxAudioDecoder->pOutBuf,
                                                &pNvxAudioDecoder->pOutBufAlign) != NvSuccess)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:%s:" __FUNCTION__, " Could not initialize ddp library"));
        return OMX_ErrorUndefined;
    }

    // Set the decoder to output 2 downmixed channels
    if (pNvxAudioDecoder->wrap.NvDdpWrapperSetProcessParamOutmode(pNvxAudioDecoder->pDecHandleAlign,
                                                                  NVX_AC3_OUTPUT_MODE_LR) != NvSuccess)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:%s:" __FUNCTION__, " Could not set output mode"));
        return OMX_ErrorUndefined;
    }

    // Allocate memory for decoding buffer pointers
    int pcm;
    memset(&pNvxAudioDecoder->processOutput, 0, sizeof(NVX_AC3_TIMESLICE_OUTPUT));
    for (pcm = 0; pcm < MAX_PCM; ++pcm)
    {
        pNvxAudioDecoder->processOutput.pPcmBufferDesc[pcm].ppBuffer = NvOsAlloc(sizeof(void*) * NVX_AC3_MAX_PCM_CHANNELS);
    }

    return eError;
}

static OMX_ERRORTYPE NvxAc3AudioDecoderReleaseResources(OMX_IN NvxComponent *pNvComp)
{
    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+%s\n",__FUNCTION__));

    // Get decoder instance
    SNvxAc3DecoderData *pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);

    // Free decoding buffer pointers
    int pcm;
    for (pcm = 0; pcm < MAX_PCM; ++pcm)
    {
        if (pNvxAudioDecoder->processOutput.pPcmBufferDesc[pcm].ppBuffer)
        {
            NvOsFree(pNvxAudioDecoder->processOutput.pPcmBufferDesc[pcm].ppBuffer);
            pNvxAudioDecoder->processOutput.pPcmBufferDesc[pcm].ppBuffer = NULL;
        }
    }

    // Shut down the dolby digital plus library
    if (pNvxAudioDecoder->pDecHandle)
    {
        if (pNvxAudioDecoder->wrap.NvDdpWrapperClose(pNvxAudioDecoder->pDecHandle,
                                                     pNvxAudioDecoder->pDecHandleAlign,
                                                     pNvxAudioDecoder->pOutBuf) != NvSuccess)
        {
            NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:%s: Could not close dolby decoder library", __FUNCTION__));
            return OMX_ErrorUndefined;
        }
        pNvxAudioDecoder->pDecHandle = NULL;

        // free the wrapper library
        dlclose(pNvxAudioDecoder->pWrapperLib);
        pNvxAudioDecoder->pWrapperLib = NULL;
        NvOsMemset(&pNvxAudioDecoder->wrap, 0, sizeof(pNvxAudioDecoder->wrap));
    }

    return NvxComponentBaseReleaseResources(pNvComp);
}

static OMX_ERRORTYPE NvxAc3AudioDecoderPreChangeState(NvxComponent *pNvComp, OMX_STATETYPE oNewState)
{
    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+%s\n",__FUNCTION__));

    // Get decoder instance
    SNvxAc3DecoderData *pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);

    // On each transition to executing, determine whether or not we're going to decode the
    // ac3/eac3 data. There are three cases:
    // 1. Pass through the ac3 or eac3 data when the downstream renderer is tunneled and
    //     able to render the format directly:
    // 2. Decode to 2 channels PCM when the downstream renderer is not tunneled, or is tunneled
    //     and is not able to render the format directly, and the ddp decoder library is present
    // 3. Pass silent PCM in all other cases. This also uses the bypass functionality, but
    //     sets the coding type to PCM, which will always come out empty.
    if (oNewState == OMX_StateExecuting)
    {
        // Default to PCM, only set to the correct ac3/eac3 codec if the renderer is able
        // to render it
        NvxPort *pPortOut = &pNvComp->pPorts[OUT_PORT];
        if (pPortOut->bNvidiaTunneling)
        {
            OMX_HANDLETYPE hRender = pPortOut->hTunnelComponent;

            // Find out the capabilities of the renderer
            OMX_INDEXTYPE eAudioCapsIndex;
            OMX_ERRORTYPE err = OMX_GetExtensionIndex(hRender, NVX_INDEX_CONFIG_AUDIO_CAPS, &eAudioCapsIndex);
            if (err == OMX_ErrorNone)
            {
                err = OMX_GetConfig(hRender, eAudioCapsIndex, &pNvxAudioDecoder->renderCaps);
            }
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAc3AudioDecoderFlush(OMX_IN NvxComponent *pNvComp, OMX_IN OMX_U32 nPort)
{
    SNvxAc3DecoderData *pNvxAudioDecoder = NULL;
    NV_ASSERT(pNvComp && pNvComp->pComponentData);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAc3AudioDecoderFlush\n"));

    pNvxAudioDecoder = (SNvxAc3DecoderData *)pNvComp->pComponentData;
    if (pNvxAudioDecoder == NULL)
    {
        return OMX_ErrorNone;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxAc3AudioDecoderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAc3AudioDecoderInit\n"));

    eError = NvxComponentCreate(hComponent, NUM_PORTS, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:NvxAc3AudioDecoderInit:",
                  ":eError = %d[%s(%d)]\n", eError, __FILE__, __LINE__));
        return eError;
    }

    pThis->eObjectType = NVXT_AUDIO_DECODER;

    SNvxAc3DecoderData *pNvxAudioDecoder = NvOsAlloc(sizeof(SNvxAc3DecoderData));
    if (!pNvxAudioDecoder)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:NvxAc3AudioDecoderInit:"
                  ":SNvxAc3DecoderData = %d:[%s(%d)]\n", pNvxAudioDecoder, __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(pNvxAudioDecoder, 0, sizeof(SNvxAc3DecoderData));
    pThis->pComponentData   = pNvxAudioDecoder;
    pThis->DeInit           = NvxAc3AudioDecoderDeInit;
    pThis->GetParameter     = NvxAc3AudioDecoderGetParameter;
    pThis->SetParameter     = NvxAc3AudioDecoderSetParameter;
    pThis->WorkerFunction   = NvxAc3AudioDecoderWorkerFunction;
    pThis->AcquireResources = NvxAc3AudioDecoderAcquireResources;
    pThis->ReleaseResources = NvxAc3AudioDecoderReleaseResources;
    pThis->PreChangeState   = NvxAc3AudioDecoderPreChangeState;
    pThis->Flush            = NvxAc3AudioDecoderFlush;

    pThis->pPorts[OUT_PORT].oPortDef.nPortIndex = OUT_PORT;

    NvxPortInitAudio(&pThis->pPorts[OUT_PORT], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5,
                     MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);

    // Allocate the default PCM parameter structure, for the output port
    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;
    ALLOC_STRUCT(pThis, pNvxPcmParameters, OMX_AUDIO_PARAM_PCMMODETYPE);
    if (!pNvxPcmParameters)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:%s:"
                 ":OMX_ErrorInsufficientResources:[%s(%d)]\n",__FUNCTION__, __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    // Iniitialize the PCM param structure with default values
    pNvxPcmParameters->nPortIndex = pThis->pPorts[OUT_PORT].oPortDef.nPortIndex;
    pNvxPcmParameters->nChannels = 2;
    pNvxPcmParameters->eNumData = OMX_NumericalDataSigned;
    pNvxPcmParameters->ePCMMode = OMX_AUDIO_PCMModeLinear;
    pNvxPcmParameters->bInterleaved = OMX_TRUE;
    pNvxPcmParameters->nBitPerSample = 16;
    pNvxPcmParameters->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pNvxPcmParameters->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    pThis->pPorts[OUT_PORT].pPortPrivate = pNvxPcmParameters;

    // Set up component roles. DDP decoder is only valid for ac3/eac3 streams
    pThis->pComponentName = "OMX.Nvidia.ac3.decoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_decoder.ac3";

    // Allocate the default AC3 parameter structure, for the input port
    OMX_AUDIO_PARAM_AC3TYPE *pNvxAc3Parameters;
    ALLOC_STRUCT(pThis, pNvxAc3Parameters, OMX_AUDIO_PARAM_AC3TYPE);
    if (!pNvxAc3Parameters)
    {
        return OMX_ErrorInsufficientResources;
    }

    // Initialize the AC3 param structure with default values
    pNvxAc3Parameters->nPortIndex  = pThis->pPorts[IN_PORT].oPortDef.nPortIndex;
    pNvxAc3Parameters->nChannels   = 2;
    pNvxAc3Parameters->nSampleRate = 48000;
    pThis->pPorts[IN_PORT].pPortPrivate = pNvxAc3Parameters;

    NvxPortInitAudio(&pThis->pPorts[IN_PORT], OMX_DirInput, MAX_INPUT_BUFFERS, 8192, NVX_AUDIO_CodingAC3);
    pThis->pPorts[IN_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    return eError;
}

static OMX_ERRORTYPE loadSymbols(void* pWrapperLib, NvxAc3WrapperFuncs *pWrapperFuncs)
{
    NV_ASSERT(pWrapperLib);
    NV_ASSERT(pWrapperFuncs);
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NvxCheckError(eError, loadSymbol(pWrapperLib, (void**)&pWrapperFuncs->NvDdpWrapperOpen, "NvDdpWrapperOpen"));
    NvxCheckError(eError, loadSymbol(pWrapperLib, (void**)&pWrapperFuncs->NvDdpWrapperClose, "NvDdpWrapperClose"));
    NvxCheckError(eError, loadSymbol(pWrapperLib, (void**)&pWrapperFuncs->NvDdpWrapperSetProcessParamOutmode,
                                     "NvDdpWrapperSetProcessParamOutmode"));
    NvxCheckError(eError, loadSymbol(pWrapperLib, (void**)&pWrapperFuncs->NvDdpWrapperProcessData,
                                     "NvDdpWrapperProcessData"));

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE loadSymbol(void* pWrapperLib, void** pWrapperSymbol, char const* pName)
{
    NV_ASSERT(pWrapperLib);
    NV_ASSERT(pWrapperSymbol);
    NV_ASSERT(pName);

    dlerror();
    *pWrapperSymbol = dlsym(pWrapperLib, pName);
    char const* err = dlerror();
    if (err)
    {
        NVXTRACE((NVXT_ERROR, pThis->eObjectType, "ERROR:%s:" " dlsym error. dlerror()=%s", __FUNCTION__, err));
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE decodeAC3(SNvxAc3DecoderData *pNvxAudioDecoder,
                               NvS16* pInPtr,
                               NvU32  inputSize,
                               NvS16* pOutPtr,
                               NvU32  outputSize)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    unsigned int bytesRead = 0;
    bytesRead = pNvxAudioDecoder->wrap.NvDdpWrapperProcessData(pNvxAudioDecoder->pDecHandleAlign,
                                                               pInPtr,
                                                               inputSize,
                                                               pNvxAudioDecoder->pOutBufAlign,
                                                               &pNvxAudioDecoder->processOutput);

    // We're only going to copy two of the channels to achieve 2 channel PCM
    // playback. We'll use the left and right channels (index 0 and 2)
    int const nchans = 2;
    int const channels[] = {0, 2};

    int pcm;
    for (pcm = 0; pcm < MAX_PCM; ++pcm)
    {
        if (!pNvxAudioDecoder->processOutput.bPcmValid[pcm])
        {
            continue;
        }

        // initialize the channel buffer pointers which will be modified during write
        int ch;
        OMX_U32 *pChannelBuf[NVX_AC3_MAX_PCM_CHANNELS];
        for (ch = 0; ch < NVX_AC3_MAX_PCM_CHANNELS; ch++)
        {
            pChannelBuf[ch] = (OMX_U32*)pNvxAudioDecoder->processOutput.pPcmBufferDesc[pcm].ppBuffer[ch];
        }
        OMX_U16 *pOut = pOutPtr;

        int samp;
        for (samp = 0; samp < pNvxAudioDecoder->processOutput.nBlocksPerTimeSlice * NVX_AC3_BLOCK_LENGTH; ++samp)
        {
            // The output mode should be set to 2 output channels PCM. Interleave those two
            // buffers into the pcm output buffer
            for (ch = 0; ch < nchans; ++ch)
            {
                // This will support 16 bits PCM output only
                OMX_U32 tmp = *pChannelBuf[channels[ch]];

                // Round according to PCM output bits
                // y = (int)(x + 0.5)
                if (tmp <= 0x7fffffff - 0x8000)
                {
                    // From dolby sample app
                    tmp += 0x8000;
                }

                *pOut++ = (tmp >> 16);
                pChannelBuf[channels[ch]] += pNvxAudioDecoder->processOutput.pPcmBufferDesc[pcm].nStride;
            }
        }

        pNvxAudioDecoder->decodeData.bFrameOK    = OMX_TRUE;
        pNvxAudioDecoder->decodeData.nSampleRate = pNvxAudioDecoder->processOutput.nSampleRate;
    }

    pNvxAudioDecoder->decodeData.bytesConsumed = bytesRead;
    pNvxAudioDecoder->decodeData.outputBytes   = pNvxAudioDecoder->processOutput.nBlocksPerTimeSlice * NVX_AC3_BLOCK_LENGTH * nchans * 2;
    pNvxAudioDecoder->decodeData.nChannels     = nchans;

errout:

    return err;
}

static void fixByteOrder(OMX_U16 *pData, int length)
{
    // check for sync word reversed to see if we need to swap bytes
    if (pData[0] == NVX_AC3_SYNCWORD_REV)
    {
        int i;
        for (i = 0; i < length; ++i)
        {
            pData[i] = ((pData[i] >> 8) & 0x00FF) | ((pData[i] << 8) & 0xFF00);
        }
    }
}


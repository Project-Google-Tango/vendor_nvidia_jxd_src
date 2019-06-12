/* Copyright (c) 2010-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "OMX_Core.h"
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvrm_surface.h"
#include "nvmm_util.h"
#include "nvmm_parser.h"

#include "nvos.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "tvmr.h"
#include "nvmm_queue.h"

#define MAX_NVMM_BUFFERS    32

#define TF_NUM_STREAMS   6

#define TF_TYPE_NONE            0
#define TF_TYPE_INPUT           1
#define TF_TYPE_OUTPUT          2
#define TF_TYPE_INPUT_TUNNELED  3
#define TF_TYPE_OUTPUT_TUNNELED 4
#define FRAME_VMR_FREE                  101
#define FRAME_VMR_SENT_FOR_DISPLAY      102
#define NUM_SURFACES 5 //surfaces required for deinterlacing interlaced streams
#define TOTAL_NUM_SURFACES NUM_SURFACES+2
#define TVMR_SURFACES 7

#if NV_ENABLE_DEBUG_PRINTS
#define NVMMVERBOSE(x) if (pData->bVerbose) NvOsDebugPrintf x
#else
#define NVMMVERBOSE(x) do {} while (0)
#endif

typedef void (*InputSurfaceHookFunction)(OMX_BUFFERHEADERTYPE *pSrcSurf, NvMMSurfaceDescriptor *pDestSurf);

typedef struct SNvxNvMMPort
{
    int nType;
    NvMMBuffer *pBuffers[MAX_NVMM_BUFFERS];
    NvMMBuffer *pNvMMBufMap[MAX_NVMM_BUFFERS];
    OMX_BUFFERHEADERTYPE *pOMXBufMap[MAX_NVMM_BUFFERS];
    OMX_BOOL bBufNeedsFlush[MAX_NVMM_BUFFERS];
    int nBufsTot;
    int nBufSize;
    int nCurrentBuffer;
    int nBufferWritePos;
    int nBufsInUse;
    NvMMQueueHandle pMarkQueue;
    OMX_BOOL bSentEOS;
    OMX_BOOL bEOS;
    OMX_BOOL bAborting;
    OMX_BOOL bError;
    OMX_BOOL bFirstBuffer;
    OMX_BOOL bFirstOutBuffer;
    OMX_HANDLETYPE oMutex;
    NvOsSemaphoreHandle buffConfigSem;
    NvOsSemaphoreHandle buffReqSem;
    NvOsSemaphoreHandle blockAbortDone;
    int nInputPortNum; // If output port, this is where the data's coming from
    NvxPort *pOMXPort;
    OMX_BOOL bNvMMClientAlloc;
    OMX_BOOL bTunneledAllocator;

    OMX_BOOL bCanSkipWorker;
    OMX_BOOL bSkipWorker;
    OMX_BOOL bSentInitialBuffers;
    OMX_BOOL bRequestSurfaceBuffers;
    OMX_BOOL bUsesRmSurfaces;
    OMX_BOOL bWillCopyToANB;

    OMX_BOOL bSetAudioParams;
    OMX_U32  nSampleRate;
    OMX_U32  nChannels;
    OMX_U32  nBitsPerSample;

    NvMMAudioMetadata audioMetaData;
    OMX_BOOL bAudioMDValid;

    OMX_TICKS position;

    NvMMNewBufferRequirementsInfo bufReq;
    OMX_BOOL bInSharedMemory;

    OMX_BOOL bHasSize;
    OMX_U32 nWidth, nHeight;
    OMX_CONFIG_RECTTYPE  outRect;

    OMX_BOOL bTunneling;
    void *pTunnelBlock;
    int nTunnelPort;

    OMX_BUFFERHEADERTYPE *pSavedEOSBuf;
    OMX_BOOL bEmbedNvMMBuffer;
    OMX_BOOL bEnc2DCopy;

    // OMX_TRUE, if OMX/NvMM stream corresponding to this port
    //           has successfully completed buffer negotiation.
    OMX_BOOL bBufferNegotiationDone;

    OMX_BOOL bUsesAndroidMetaDataBuffers;

    OMX_S32 xScaleWidth;
    OMX_S32 xScaleHeight;

    OMX_U32 nBufsANB;
    OMX_BOOL bUsesANBs;
    OMX_BOOL bUsesNBHs;
    OMX_BOOL bBlockANBImport;
    NvRmSurfaceLayout SurfaceLayout;
} SNvxNvMMPort;

typedef enum {
    DEINTERLACING_FRAME_RATE = 1,
    DEINTERLACING_FIELD_RATE = 2,
    NvMMClockState_Force32 = 0x7fffffff
}eDeinterlacingRate;

typedef struct {
    TVMRDevice           *pDevice;
    TVMRVideoMixer       *pMixer;
    TVMRVideoSurface     *pPrev2VideoSurface;
    TVMRVideoSurface     *pPrevVideoSurface;
    TVMRVideoSurface     *pCurrVideoSurface;
    TVMRVideoSurface     *pNextVideoSurface;
    TVMRVideoSurface     *pVideoSurfaceList[TVMR_SURFACES];
    NvMMBuffer           *pPrev2Surface;
    NvMMBuffer           *pPrevSurface;
    NvMMBuffer           *pCurrSurface;
    NvMMBuffer           *pNextSurface;
    TVMRFence             fenceDone;
    NvU32                 frameWidth;
    NvU32                 frameHeight;
    NvU64                 prevTimeStamp;
    NvS32                 DeltaTime;
    NvU32                 DeltaCount;
/*  The TOTAL_NUM_SURFACES = NUM_SURFACES+2 because in HC the renderer always allocates more no of buffers (+2)
    than requested */
    NvU32                 Renderstatus[TOTAL_NUM_SURFACES];
    NvU32                 VideoSurfaceIdx;
    NvMMBuffer           *pVideoSurf[TOTAL_NUM_SURFACES];
    eDeinterlacingRate    DeinterlacingRate;
    TVMRDeinterlaceType   deinterlaceType;
} TVMRCtx;

typedef struct SNvxNvMMTransformData
{
    NvRmDeviceHandle hRmDevice;
    NvMMBlockHandle hBlock;
    char sName[32];

    // weak reference to parent component (to support proper callback translations)
    NvxComponent *pParentComp;

    void (*EventCallback)(NvxComponent *pComp, NvU32 eventType, NvU32 eventSize, void *eventInfo);
    void *pEventData;
    InputSurfaceHookFunction InputSurfaceHook;

    TransferBufferFunction TransferBufferToBlock;
    NvOsSemaphoreHandle stateChangeDone;
    NvOsSemaphoreHandle SetAttrDoneSema;
    OMX_BOOL blockCloseDone;
    OMX_BOOL blockErrorEncountered;

    NvU32 nNumStreams;

    NvMMBlockType oBlockType;
    OMX_BOOL bSequence;
    NvMMLocale locale;
    OMX_U64 BlockSpecific;

    // Buffers
    SNvxNvMMPort oPorts[TF_NUM_STREAMS];
    int nNumInputPorts;
    int nNumOutputPorts;

    OMX_BOOL bHasInputRunning;
    OMX_BOOL bVideoTransform;
    OMX_BOOL bAudioTransform;
    OMX_BOOL bInSharedMemory;
    OMX_BOOL bThumbnailPreferred;
    OMX_BOOL bWantTunneledAlloc;

    OMX_U32 nThumbnailWidth;
    OMX_U32 nThumbnailHeight;
    OMX_U32 nColorFormat;
    int nNalStreamFlag;
    int nNalSize;

    OMX_S32 xScaleFactorWidth;
    OMX_S32 xScaleFactorHeight;

    OMX_BOOL bFirstBuffer;
    OMX_BOOL bPausing;
    OMX_BOOL bStopping;
    OMX_BOOL bFlushing;

    OMX_BOOL bProfile;
    OMX_BOOL bVerbose;
    OMX_BOOL bDiscardOutput;
    OMX_U32  nNvMMProfile;
    OMX_BOOL enableUlpMode;
    OMX_U32 ulpkpiMode;
    NvString fileName;
    int nForceLocale;

    NvU64 llFirstTBTB;
    NvU64 llLastTBTB;
    NvU64 llFinalOutput;
    NvU64 LastTimeStamp;
    int nNumTBTBPackets;

    OMX_U32 nFileCacheSize;
    OMX_BOOL bSetFileCache;
    OMX_U8  isEXIFPresent;

    //Original Image Width/Image Height/Image Color Format
    OMX_U32    nPrimaryImageWidth;
    OMX_U32    nPrimaryImageHeight;
    OMX_COLOR_FORMATTYPE eColorFormat;

    NvMMEventNewVideoStreamFormatInfo VideoStreamInfo;
    OMX_BOOL bReconfigurePortBuffers;
    OMX_U32  nReconfigurePortNum;
    OMX_U32  nExtraSurfaces;
    OMX_U32  NumBuffersWithBlock;
    OMX_BOOL bAvoidOldFormatChangePath;
    OMX_BOOL bEmbedRmSurface;
    OMX_BOOL bUsesSyncFDFromBuffer;

    /*de-interlace related stuff */
    NvOsThreadHandle hDeinterlaceThread;
    NvOsSemaphoreHandle TvmrInitDone;
    NvOsSemaphoreHandle InpOutAvail;
    NvOsSemaphoreHandle DeintThreadPaused;
    NvMMQueueHandle  pReleaseOutSurfaceQueue;
    OMX_BOOL InterlaceStream;
    OMX_BOOL InterlaceThreadActive;
    volatile OMX_BOOL DeinterlaceThreadClose;
    volatile OMX_BOOL Deinterlacethreadtopuase;
    volatile OMX_BOOL StopDeinterlace;
    // TVMR context
    TVMRCtx vmr;

    NvMMQueueHandle pRenderHoldQ; // Hold new decoder buffers before render.
    OMX_U32  CurIdx;              // Index of decoder.
} SNvxNvMMTransformData;

typedef struct NvxAesMetadataStruct
{
    /* specifies the app private structure */
    OMX_BOOL bEncrypted;
    OMX_U8 metadataCount;
    DRM_EXTRADATA_ENCRYPTION_METADATA metadataList[10];
} NvxAesMetadata;

OMX_ERRORTYPE NvxNvMMTransformWorkerBase(SNvxNvMMTransformData *pData, int nNumStream, OMX_BOOL *pbNextWouldFail);

OMX_ERRORTYPE NvxNvMMTransformOpen(SNvxNvMMTransformData *pData, NvMMBlockType oBlockType, const char *sBlockName, int nNumStreams, NvxComponent *pNvComp);
OMX_ERRORTYPE NvxNvMMTransformSetupInput(SNvxNvMMTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs, int nInputBufSize, OMX_BOOL bCanSkipWorker);
OMX_ERRORTYPE NvxNvMMTransformSetupCameraInput(SNvxNvMMTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs, int nInputBufSize, OMX_BOOL bCanSkipWorker);
void NvxNvMMTransformReturnNvMMBuffers(void *pContext, int nNumStream);
OMX_BOOL NvxNvMMTransformIsInputSkippingWorker(SNvxNvMMTransformData *pData, int nStreamNum);
OMX_ERRORTYPE NvxNvMMTransformSetEventCallback(SNvxNvMMTransformData *pData,     void (*EventCB)(NvxComponent *pComp, NvU32 eventType, NvU32 eventSize, void *eventInfo), void *pEventData);
OMX_ERRORTYPE NvxNvMMTransformSetInputSurfaceHook(SNvxNvMMTransformData *pData, InputSurfaceHookFunction ISHook);
OMX_ERRORTYPE NvxNvMMTransformSetupOutput(SNvxNvMMTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nInputStreamNum, int nNumOutputBufs, int nOutputBufSize);
OMX_ERRORTYPE NvxNvMMTransformSetStreamUsesSurfaces(SNvxNvMMTransformData *pData, int nStreamNum, OMX_BOOL bUseSurface);
OMX_ERRORTYPE NvxNvMMTransformTunnelBlocks(SNvxNvMMTransformData *pSource, int nSourcePort, SNvxNvMMTransformData *pDest, int nDestPort);
OMX_ERRORTYPE NvxNvMMTransformSetupAudioParams(SNvxNvMMTransformData *pData, int nStreamNum, OMX_U32 sampleRate, OMX_U32 bitsPerSample, OMX_U32 nChannels);

OMX_ERRORTYPE NvxNvMMTransformClose(SNvxNvMMTransformData *pData);

OMX_TICKS NvxNvMMTransformGetMediaTime(SNvxNvMMTransformData *pData, int nPort);

OMX_ERRORTYPE NvxNvMMTransformFillThisBuffer(SNvxNvMMTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream);

OMX_ERRORTYPE NvxNvMMTransformFreeBuffer(SNvxNvMMTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream);

OMX_ERRORTYPE NvxNvMMTransformSetProfile(SNvxNvMMTransformData *pData, NVX_CONFIG_PROFILE *pProf);

OMX_ERRORTYPE NvxNvMMTransformSetFileName(SNvxNvMMTransformData *pData, NVX_PARAM_FILENAME *pProf);

OMX_ERRORTYPE NvxNvMMTransformSetUlpMode(SNvxNvMMTransformData *pData, NVX_CONFIG_ULPMODE *pProf);
OMX_ERRORTYPE NvxNvMMTransformSetFileCacheSize(SNvxNvMMTransformData *pData, OMX_U32 nSize);


OMX_ERRORTYPE NvxNvMMTransformRegisterDeliverBuffer(SNvxNvMMTransformData *pData, int nStreamNum);

OMX_ERRORTYPE NvxNvMMTransformPreChangeState(SNvxNvMMTransformData *pData,
                                             OMX_STATETYPE oNewState,
                                             OMX_STATETYPE oOldState);
OMX_ERRORTYPE NvxNvMMTransformFlush(SNvxNvMMTransformData *pData, OMX_U32 nPort);

void NvxNvMMTransformPortEventHandler(SNvxNvMMTransformData *pData,
                                      int nPort, OMX_U32 uEventType);


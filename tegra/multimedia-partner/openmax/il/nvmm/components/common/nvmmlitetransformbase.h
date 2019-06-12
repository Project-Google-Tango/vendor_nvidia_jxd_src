/* Copyright (c) 2010-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "OMX_Core.h"
#include "nvmmlite.h"
#include "nvmmlite_event.h"
#include "nvrm_surface.h"
#include "nvmmlite_util.h"

#include "nvos.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/nvxlitevideopostprocess.h"

#include "nvmm_queue.h"

#define MAX_NVMMLITE_BUFFERS    32

#define TF_NUM_STREAMS   6

#define TF_TYPE_NONE            0
#define TF_TYPE_INPUT           1
#define TF_TYPE_OUTPUT          2

#if NV_ENABLE_DEBUG_PRINTS
#define NVMMLITEVERBOSE(x) if (pData->bVerbose) NvOsDebugPrintf x
#else
#define NVMMLITEVERBOSE(x) do {} while (0)
#endif

typedef void (*LiteInputSurfaceHookFunction)(OMX_BUFFERHEADERTYPE *pSrcSurf, NvMMSurfaceDescriptor *pDestSurf);

typedef struct SNvxNvMMLitePort
{
    int nType;
    NvMMBuffer *pBuffers[MAX_NVMMLITE_BUFFERS];
    OMX_BUFFERHEADERTYPE *pOMXBufMap[MAX_NVMMLITE_BUFFERS];
    OMX_BUFFERHEADERTYPE *pOMXANBBufMap[MAX_NVMMLITE_BUFFERS];
    OMX_BOOL bBufNeedsFlush[MAX_NVMMLITE_BUFFERS];
    int nBufsTot;
    int nBufSize;
    int nBufferWritePos;
    int nBufsInUse;
    NvMMQueueHandle pMarkQueue;
    OMX_BOOL bSentEOS;
    OMX_BOOL bEOS;
    OMX_BOOL bAborting;
    OMX_BOOL bError;
    OMX_BOOL bFirstBuffer;
    OMX_BOOL bFirstOutBuffer;
    OMX_BOOL bAvoidCopy;
    OMX_HANDLETYPE oMutex;
    int nInputPortNum; // If output port, this is where the data's coming from
    NvxPort *pOMXPort;

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

    NvMMLiteNewBufferRequirementsInfo bufReq;
    OMX_BOOL bInSharedMemory;

    OMX_BOOL bHasSize;
    OMX_U32 nWidth, nHeight;
    OMX_CONFIG_RECTTYPE  outRect;
    OMX_TIME_SEEKMODETYPE nSeekType;
    OMX_TICKS nTimestamp;

    OMX_BUFFERHEADERTYPE *pSavedEOSBuf;
    OMX_BOOL bEmbedNvMMBuffer;
    OMX_BOOL bEmbedANBSurface;
    OMX_BOOL bEnc2DCopy;
    OMX_BOOL bUsesAndroidMetaDataBuffers;

    OMX_S32 xScaleWidth;
    OMX_S32 xScaleHeight;
    OMX_U32 DispWidth;
    OMX_U32 DispHeight;

    OMX_U32 nBufsANB;
    OMX_BOOL bUsesANBs;
    OMX_BOOL bUsesNBHs;
    OMX_BOOL bBlockANBImport;
    NVX_CONFIG_VIDEO2DPROCESSING Video2DProc;
    OMX_U32 nBackgroundDoneMap;
    OMX_HANDLETYPE hVPPMutex;
    OMX_U32 nOutputQueueIndex;
    OMX_U32 nInputQueueIndex;
#ifdef BUILD_GOOGLETV
    NvxListHandle pPreviousBuffersList;
    OMX_HANDLETYPE  hPreviousBuffersMutex;
    int nNumPreviousBuffs;
    int nNumPortSetting;
    OMX_BOOL bPortSettingChanged;
#endif
} SNvxNvMMLitePort;

typedef struct SNvxNvMMLiteTransformData
{
    NvRmDeviceHandle hRmDevice;
    NvMMLiteBlockHandle hBlock;
    char sName[32];

    // weak reference to parent component (to support proper callback translations)
    NvxComponent *pParentComp;

    void (*EventCallback)(NvxComponent *pComp, NvU32 eventType, NvU32 eventSize, void *eventInfo);
    void *pEventData;
    LiteInputSurfaceHookFunction InputSurfaceHook;

    LiteTransferBufferFunction TransferBufferToBlock;
    OMX_BOOL blockCloseDone;
    OMX_BOOL blockErrorEncountered;

    NvU32 nNumStreams;
    OMX_BOOL bSkipBFrame;

    NvMMLiteBlockType oBlockType;
    OMX_BOOL bSequence;
    OMX_U64 BlockSpecific;

    // Buffers
    SNvxNvMMLitePort oPorts[TF_NUM_STREAMS];
    int nNumInputPorts;
    int nNumOutputPorts;

    OMX_BOOL bHasInputRunning;
    OMX_BOOL bVideoTransform;
    OMX_BOOL bAudioTransform;
    OMX_BOOL bInSharedMemory;
    OMX_BOOL bThumbnailPreferred;

    OMX_U32 nThumbnailWidth;
    OMX_U32 nThumbnailHeight;
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
    OMX_U32  nNvMMLiteProfile;
    char *fileName;
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

    NvMMLiteEventNewVideoStreamFormatInfo VideoStreamInfo;
    OMX_BOOL bReconfigurePortBuffers;
    OMX_U32  nReconfigurePortNum;
    OMX_U32  nExtraSurfaces;
    OMX_U32  NumBuffersWithBlock;
    OMX_BOOL bAvoidOldFormatChangePath;
    OMX_BOOL bEmbedRmSurface;
    OMX_BOOL bSecure;
    OMX_BOOL bWaitOnFence;
    OMX_BOOL bVideoProtect;
    OMX_BOOL bThumbnailMode;
    OMX_BOOL bUsesSyncFDFromBuffer;
    OMX_BOOL bUseNvBuffer2;

    OMX_U32  CurIdx;              // Index of decoder.
    NvMMQueueHandle pOutputQ[TF_NUM_STREAMS];
    NvMMQueueHandle pInputQ[TF_NUM_STREAMS];
    OMX_BOOL is_MVC_present_flag;
    OMX_BOOL bUseLowOutBuff;
    OMX_BOOL bFilterTimestamps;
    OMX_MIRRORTYPE mirror_type; //Support mirroring support for EGL buffers for now
    OMX_U32  LastFrameQP;
    NvMMBuffer *pScratchBuffer;
    SNvxNvMMLiteVPP    sVpp;      /*VPP Data structure*/
} SNvxNvMMLiteTransformData;

typedef struct NvxLiteAesMetadataStruct
{
    /* specifies the app private structure */
    OMX_BOOL bEncrypted;
    OMX_U8 metadataCount;
    DRM_EXTRADATA_ENCRYPTION_METADATA metadataList[10];
} NvxLiteAesMetadata;

typedef struct NvxLiteOutputBuffer
{
    NvMMBuffer Buffer;
    NvU32 streamIndex;
}NvxLiteOutputBuffer;

OMX_ERRORTYPE NvxNvMMLiteTransformWorkerBase(SNvxNvMMLiteTransformData *pData, int nNumStream, OMX_BOOL *pbNextWouldFail);

OMX_ERRORTYPE NvxNvMMLiteTransformOpen(SNvxNvMMLiteTransformData *pData, NvMMLiteBlockType oBlockType, const char *sBlockName, int nNumStreams, NvxComponent *pNvComp);
OMX_ERRORTYPE NvxNvMMLiteTransformSetupInput(SNvxNvMMLiteTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs, int nInputBufSize, OMX_BOOL bCanSkipWorker);
OMX_ERRORTYPE NvxNvMMLiteTransformSetupCameraInput(SNvxNvMMLiteTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nNumInputBufs, int nInputBufSize, OMX_BOOL bCanSkipWorker);
OMX_BOOL NvxNvMMLiteTransformIsInputSkippingWorker(SNvxNvMMLiteTransformData *pData, int nStreamNum);
OMX_ERRORTYPE NvxNvMMLiteTransformSetEventCallback(SNvxNvMMLiteTransformData *pData,     void (*EventCB)(NvxComponent *pComp, NvU32 eventType, NvU32 eventSize, void *eventInfo), void *pEventData);
OMX_ERRORTYPE NvxNvMMLiteTransformSetInputSurfaceHook(SNvxNvMMLiteTransformData *pData, LiteInputSurfaceHookFunction ISHook);
OMX_ERRORTYPE NvxNvMMLiteTransformSetupOutput(SNvxNvMMLiteTransformData *pData, int nStreamNum, NvxPort *pOMXPort, int nInputStreamNum, int nNumOutputBufs, int nOutputBufSize);
OMX_ERRORTYPE NvxNvMMLiteTransformSetStreamUsesSurfaces(SNvxNvMMLiteTransformData *pData, int nStreamNum, OMX_BOOL bUseSurface);
OMX_ERRORTYPE NvxNvMMLiteTransformSetupAudioParams(SNvxNvMMLiteTransformData *pData, int nStreamNum, OMX_U32 sampleRate, OMX_U32 bitsPerSample, OMX_U32 nChannels);

OMX_ERRORTYPE NvxNvMMLiteTransformClose(SNvxNvMMLiteTransformData *pData);

OMX_TICKS NvxNvMMLiteTransformGetMediaTime(SNvxNvMMLiteTransformData *pData, int nPort);

OMX_ERRORTYPE NvxNvMMLiteTransformFillThisBuffer(SNvxNvMMLiteTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream);

OMX_ERRORTYPE NvxNvMMLiteTransformFreeBuffer(SNvxNvMMLiteTransformData *pData, OMX_BUFFERHEADERTYPE *pBuffer, int nStream);

OMX_ERRORTYPE NvxNvMMLiteTransformSetProfile(SNvxNvMMLiteTransformData *pData, NVX_CONFIG_PROFILE *pProf);

OMX_ERRORTYPE NvxNvMMLiteTransformSetFileName(SNvxNvMMLiteTransformData *pData, NVX_PARAM_FILENAME *pProf);

OMX_ERRORTYPE NvxNvMMLiteTransformSetFileCacheSize(SNvxNvMMLiteTransformData *pData, OMX_U32 nSize);


OMX_ERRORTYPE NvxNvMMLiteTransformRegisterDeliverBuffer(SNvxNvMMLiteTransformData *pData, int nStreamNum);

OMX_ERRORTYPE NvxNvMMLiteTransformPreChangeState(SNvxNvMMLiteTransformData *pData,
                                             OMX_STATETYPE oNewState,
                                             OMX_STATETYPE oOldState);
OMX_ERRORTYPE NvxNvMMLiteTransformFlush(SNvxNvMMLiteTransformData *pData, OMX_U32 nPort);

void NvxNvMMLiteTransformPortEventHandler(SNvxNvMMLiteTransformData *pData,
                                      int nPort, OMX_U32 uEventType);

OMX_ERRORTYPE NvxNvMMLiteTransformFreePortBuffers(SNvxNvMMLiteTransformData *pData,
                                                     int nStreamNum);
NvError NvxNvMMLiteTransformProcessOutputBuffers(SNvxNvMMLiteTransformData *pData,
     NvU32 streamIndex,
     void *pBuffer);

/*
* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NVMM_SUPERPARSERBLOCK_H
#define INCLUDED_NVMM_SUPERPARSERBLOCK_H

#include "nvmm.h"
#include "nvos.h"
#include "nvmm_block.h"
#include "nvmm_parser.h"
#include "nvmm_parser_core.h"
#include "nvmm_ulp_util.h"
#include "nvlocalfilecontentpipe.h"
#include "nvdrm.h"

#define CACHE_SPARE_SIZE        (256 * 1024)
#define MIN_CACHE_SIZE          (1*1024*1024)

#define MAX_AUDIO_CACHE_SIZE    (15 * 1024 * 1024)
#define MAX_VIDEO_CACHE_SIZE    (5*1024*1024)

#define MAXSTREAMS 10

#define MAX_SP_OFFSET_LISTS 1024
#define MAX_MSG_LEN 64
#define MAX_QUEUE_LEN 64

#define MAX_PAUSE_PRECACHE_OFFSETS 4

#define STREAMING_CACHE_HIGH_THRESHOLD_PERCENT  80
#define STREAMING_CACHE_LOW_THRESHOLD_PERCENT   50

#define MAX_DECODER_DRM_CONTEXTS 2

/** 
* Super parser block's stream indices.
* Stream indices must start at 0.
*/
typedef enum
{
    NvMMBlockSuperParserStream_AudOut = 0, //!< Audio Output
    NvMMBlockSuperParserStream_VidOut,    //!< Video Output
    NvMMBlockSuperParserStreamCount

} NvMMBlockSuperParserStream;

typedef enum
{
    NvMMSuperParserError_Success = 0,
    NvMMSuperParserError_Force32 = 0x7FFFFFFF
} NvMMSuperParserError;

typedef enum
{
    NvMMSuperParserMediaType_Unknown    = 0,
    NvMMSuperParserMediaType_Audio      = 1,
    NvMMSuperParserMediaType_Video      = 2,
    NvMMSuperParserMediaType_AudioVideo = 3,
    NvMMSuperParserMediaType_Image      = 4,
    NvMMSuperParserMediaType_Force32    = 0x7FFFFFFF
} NvMMSuperParserMediaType;

typedef enum
{
    NvMMSuperParserOffsetListState_NotCreated = 0,
    NvMMSuperParserOffsetListState_Created = 1,
    NvMMSuperParserOffsetListState_CreatedAndSend = 2,
    NvMMSuperParserOffsetListState_Destroyed = 3,
    NvMMSuperParserOffsetListState_Force32 = 0x7FFFFFFF
} NvMMSuperParserOffsetListState;

typedef struct NvMMTrackInfoRec
{
    NvU32 eTrackFlag;
    NvMMParserCoreType eParserCore;
    void *pClientPrivate;
    NvMMErrorEventFromDomain  domainName;
    NvU32 uSize;
    NvU32 trackListIndex;
    char *pPath;
} NvMMTrackInfo;

typedef struct OffsetListRec
{
    NvMMBuffer *pOffsetList;
    NvBool bDataReady;
} OffsetList;


typedef struct OffsetListPerStreamRec
{
    NvU32 ProduceIndex;
    NvU32 ConsumeIndex;
    OffsetList **ppOffsetList;
    NvU32 NumberofOffsetLists;
    NvBool bEndOfStream;
} OffsetListPerStream;


typedef struct NvMMParserCoreContextRec
{
    NvMMParserCoreType eCoreType;
    NvMMParserCoreHandle pCore;
    NvU64 FileSize;
    OffsetListPerStream **ppOffsetListPerStream;
    NvBool bPlayBackDone;
    NvBool bEndOfStream[MAXSTREAMS];
    NvBool bEosEventSent[MAXSTREAMS];
    NvMMSuperParserOffsetListState offsetListState;
    NvMMTrackInfo *pTrackInfo;
    NvBool bEmpty;
    NvBool bPreCacheOffsets;
    NvBool bDRMContextSentToDecoder;
    NvBool bPrepareOffsets;
    NvU32 NumberofOffsetLists;
} NvMMParserCoreContext;


typedef enum
{
    NvMMSuperParserMsgType_StartParsing = 1,
    NvMMSuperParserMsgType_PauseParsing,    
    NvMMSuperParserMsgType_ResumeParsing,   
    NvMMSuperParserMsgType_DestroyThread,    
    NvMMSuperParserMsgType_UlpMode,

    NvMMSuperParserMsgType_CreateParserCore,
    NvMMSuperParserMsgType_DeleteParserCore,
    NvMMSuperParserMsgType_CreateOffsets,
    NvMMSuperParserMsgType_DeleteOffsets,
    NvMMSuperParserMsgType_AdjustOffsetsForSeek,

    NvMMSuperParserMsgType_Force32 = 0x7FFFFFFF
} NvMMSuperParserMsgType;

typedef struct NvMMSuperParserMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
} NvMMSuperParserMsg;

typedef struct NvMMSuperParserStartParsingMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize;
    NvBool bDeleteCores;
    NvBool bUpdateParseIndex;
    NvU32  ParseIndex;
    NvOsSemaphoreHandle hMessageAckSema;
    NvError *pParsingStatus;
    NvError ParsingStatus;
    NvBool bNeedAck;
    NvBool bPrepareOffsets;

} NvMMSuperParserStartParsingMsg;

typedef struct NvMMSuperParserDestroyThreadMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
} NvMMSuperParserDestroyThreadMsg;

typedef struct NvMMSuperParserUlpModeMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
    NvBool bUlpMode;
    NvBool bUlpKpiMode;
} NvMMSuperParserUlpModeMsg;

typedef struct NvMMSuperParserCreateParserCoreMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
    NvError *pStatus;
    NvError Status;
    NvMMParserCoreHandle pCore;
    NvMMTrackInfo pTrackInfo;
} NvMMSuperParserCreateParserCoreMsg;

typedef struct NvMMSuperParserDeleteParserCoreMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
    NvError *pStatus;
    NvError Status;
    NvMMParserCoreHandle pCore;
} NvMMSuperParserDeleteParserCoreMsg;

typedef struct NvMMSuperParserCreateOffsetsMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
    NvError *pStatus;
    NvError Status;
    NvMMParserCoreHandle pCore;
} NvMMSuperParserCreateOffsetsMsg;

typedef struct NvMMSuperParserDeleteOffsetsMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
    NvError *pStatus;
    NvError Status;
    NvMMParserCoreHandle pCore;
} NvMMSuperParserDeleteOffsetsMsg;

typedef struct NvMMSuperParserAdjustOffsetsMsgRec
{
    NvMMSuperParserMsgType eMessageType;
    NvU32 StructSize; 
    NvOsSemaphoreHandle hMessageAckSema;
    NvBool bNeedAck;
    NvError *pStatus;
    NvError Status;
    NvU64 SeekPos;
    NvMMParserCoreHandle pCore;
} NvMMSuperParserAdjustOffsetsMsg;

typedef struct NvMMSuperParserDRMContextInfoRec
{
    NvBool bEmpty;
    NvBool bMeteringDone;
    NvBool bPlayBackDone;
    NvU32 NvDrmContext;
    NvU32 NvDrmUpdateMeteringInfo;
    NvU32 NvDrmDestroyContext;

} NvMMSuperParserDRMContextInfo;

typedef enum NvMMBufferingDurationInfo
{
    NvBufferingDuration_High = 12,
    NvBufferingDuration_Low = 4,
    NvBufferingDuration_Start = 6,
    NvBufferingDuration_Low_AudioOnly = 1,
    NvBufferingDuration_Force32 = 0x7FFFFFFFUL
}NvMMBufferingDuration;

typedef struct NvMMStreamingInfo
{   
    NvBool IsStreaming;
    NvBool m_Buffering;
    NvBool m_Streaming;
    NvBool m_FirstTimeCache;
    NvBool m_InitialBuffering;
    NvU32 m_BufferingPercent;
    NvU32 m_bBufferBeforePlay;
    NvU32 m_FileBitRate;
    NvU64 m_StreamingLowMark;
    NvU64 m_StreamingHighMark;
    NvU64 m_nFileSize;
    NvU64 m_StreamingStartMark;
    NvBool m_InterTransition;
    NvU64 m_rtDuration;
    NvOsSemaphoreHandle BufferingShutdownSema;    
    NvOsSemaphoreHandle BufferingStartSema; 
    NvOsThreadHandle bufferingMonitorThread;
    NvBool audioWriteSemaInitialized;       
    NvBool m_AudioOnly;
    NvBool m_GotVBitRate;
    NvU32 m_BufferingThreshold;
    NvU64 SeekTS;
    NvU64 CurrentTS;
    NvBool bStartPlayback;
    NvBool bTimeStampBuffering;
    NvU32 nWaitCounter;
    NvU32 VideoWidth;
    NvU32 VideoHeight;
    NvBool bCanSeekByTime;
    NvU64 nSeekBytes;
    NvU32 VideoBitRate;
    NvU32 AudioBitRate;
}NvMMStreamingInfo;


/** Context for parser block.
 * 
 * First member of this struct must be the NvMMBlockContext base class,
 * so base class functions (NvMMBlockXXX()) can downcast a RefBlockContext*
 * to NvMMBlock*.
 * 
 * A custom parser block would add implementation specific members to
 * this structure.
 */
typedef struct
{
    NvMMBlockContext block; //!< NvMMBlock base class, must be first member to allow down-casting

    NvMMBlockHandle hBlock;
    NvMMParserCoreContext ParserCore;
    NvMMSuperParserDRMContextInfo DrmContextInfo;
    NvU32 CoreIndex;

    NvMMParserCoreHandle pCurPlaybackCore;
    NvBool bParsingPaused;
    NvMMParserCoreHandle pPausedCore;

    NvBool bFlushBuffers;
    NvBool errorFlag;
    NvOsThreadHandle    hThread;
    NvOsSemaphoreHandle hOffsetThreadSema;
    NvOsSemaphoreHandle hCpWaitSema;
    NvOsSemaphoreHandle hFlushBufferCompleteWaitSema;
    NvOsMutexHandle     hLock;
    NvOsMutexHandle     hCoreLock;
    NvMMQueueHandle     hMsgQueue;
    NvMMQueueHandle     hSuperParserEventsMsgQueue;
    NvMMSuperParserMsg *pSuperParserEventsMsg;

    NvU32 ParseIndex;
    NvU32 PlayIndex;
    NvU32 CurrentCoreIndex;
    NvBool bInvalidateIndex;
    NvU32 getCoreFailureCount;
    NvBool isCoreIndexChanged;
    NvError TrackListError;
    NvBool bResetEOS;
    NvBool bGetNewParserCore;
    NvMMTrackInfo pTrackInfo;

    NvBool bRelaseOffsets;
    NvMMParserCoreHandle pPreviousCore;
 
    NvBool bSendFlushBufferEvent;
    NvBool bFlushBufferEventSend;
    NvBool flushBufferAckRequired;
    NvBool adjustCoresForSeek;

    //To store device handle
    NvRmDeviceHandle hRmDevice;

    NvBool isVideoEOS;
    // This signifies if buffer manager memory needs to be released before further reads
    NvBool waitForBufferRelease;

    /* TODO: move these to Core when mutiple core support is added */
    //Enable KPI mode
    NvU32 UlpKpiMode;

    // reduced buffer mode for video
    NvU32 bReduceVideoBuffers;

    // previous stream type stored for multitrack
    NvMMStreamType prevStreamType;

    // CP cache size
    NvU32 MaxCacheSize;
    // CP cache size set by Client?
    NvBool bCacheSizeSetByClient;
    // DRM handle, right now only ASF parser core uses this.
    NvOsLibraryHandle hDrmHandle;
    NvMMErrorEventFromDomain  domainName;
    NvS32 errorCount;
    NvU16 *licenseUrl;
    NvU8  *licenseChallenge;
    NvU32 licenseUrlSize;
    NvU32 licenseChallengeSize;
    NvMMStreamingInfo SInfo;
    NvBool bEnableBufferingForStreaming;
    // asfhandle to load asf library
    NvOsLibraryHandle hAsfHandle;
    // avihandle to load avi library
    NvOsLibraryHandle hAviHandle;

    /**
    * NvMMParserCore::Create. Create asf parser
    *
    * @param hParserCore   - NvMMParserCoreHandle
    * @param pParams creation parameters
    * @retval NvSuccess operation was successful
    */
    NvError (*CreateCore)(NvMMParserCoreHandle *hParserCore, NvMMParserCoreCreationParameters *pParams);

    /**
    * NvMMParserCore::Destroy. Destroy asf parser
    *
    * @param hParserCore   - NvMMParserCoreHandle
    */

    void (*DestroyCore)(NvMMParserCoreHandle hParserCore);

} NvMMSuperParserBlockContext;

/**
 * @brief Open function for the reference block. This function is 
 * called by the owner of the block's thread to create the block.
 * 
 * @param [in] hBlock 
 *      Block handle 
 * @param [in] semaphore
 *      Block's semaphore used as a trigger when for any external event
 *      which requires the block to do more work, e.g. an interupt fired
 *      upon the hardware's completion of work.
 * @param [out] pDoWorkFunction
 *      Function for the block's thread owner to call to facilitate the 
 *      block doing either critical or non-critical work.
 */ 
NvError 
NvMMSuperParserBlockOpen(
    NvMMBlockHandle *phBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle semaphore,
    NvMMDoWorkFunction *pDoWorkFunction);


/** 
 * NvMMSuperParserBlockClose function.
 *
 * It performs the following operations:
 * - free memory for hBlock->pContext and hBlock structs
 * This function is safe to call on an already closed block.
 * @param hBlock Block handle, can be NULL
 */
void 
NvMMSuperParserBlockClose(
    NvMMBlockHandle hBlock, 
    NvMMInternalDestructionParameters *pParams);

/** 
 * Defining NvMMSuperParserBlockGetAttribute prototype here, which is required by ASF parser block.
 */
NvError 
NvMMSuperParserBlockGetAttribute(
    NvMMBlockHandle hBlock, 
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute);

/** 
 * This function posts an event at the client side,when the block wants to report the error to client.
 */
void SuperparserBlockReportError(NvMMBlockContext block, NvError RetType, NvMMErrorEventFromDomain  domainName, void * pErrorInfo);

/**
 * @brief SuperParserAllocateFileCacheMemory This function allocates memory from the heap and 
 * gets both physical and virutal addresses for that memory
 * 
 * @param [in] hRmDevice 
 * @param [out] hMemHandle - Handle used to create the memory
 * @param [out] pAddress - Variable used to store the physical address of the created memory
 * @param [in/out] vAddress - Address to store the virtual memory. If this is zero, virtual 
 *                            address is not required. If the address is valid, this is used 
 *                            to store the virtual address of the created memory.
 * @param [in] size - Size of buffer that needs to be created.
 * 
 * Returns NvError or NvSuccess.
 */ 
NvError SuperParserAllocateFileCacheMemory(
    NvRmDeviceHandle hRmDevice,
    NvRmMemHandle *hMemHandle,
    NvRmPhysAddr *pAddress,
    NvU32 *vAddress,
    NvU32 size);

/**
 * @brief SuperParserFreeFileCacheMemory This function allocates memory from the heap and gets both physical and 
 * virutal addresses for that memory
 * 
 * @param [in/out] hMemHandle - Memory handle to be released.
 * @param [in/out] pAddress - Variable represents the physical address of the created memory
 * @param [in/out] vAddress - If this is zero, virtual address is not un mapped.
 *                  If the address is valid, this is used to unmap the virtual address of 
 *                  the created memory.
 * @param [in] size - Size of buffer that needs to be created.
 * 
 * Returns NvError or NvSuccess.
 */ 
NvError 
SuperParserFreeFileCacheMemory(
    NvRmMemHandle *hMemHandle,
    NvRmPhysAddr *pAddress,
    NvU32 *vAddress,
    NvU32 size);

/**
 * @brief SuperParserIsUlpSupported This function checks if ULP mode is supported by the derived parser core
 * 
 * @param [in] eCoreType - Signifies the parser core type.
 * @param [in] eStreamType - Signifies the stream type.
 * 
 * Returns NV_TRUE if the core supports ULP mode else return NV_FALSE
 */ 
NvBool
SuperParserIsUlpSupported(NvMMParserCoreType eCoreParserType, NvMMStreamType eStreamType);

#endif


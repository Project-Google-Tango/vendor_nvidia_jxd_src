/*
* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NVMM_PARSER_CORE_H
#define INCLUDED_NVMM_PARSER_CORE_H

#include "nvmm.h"
#include "nvos.h"
#include "nvmm_block.h"
#include "nvmm_parser.h"
#include "nvmm_ulp_util.h"
#include "nvlocalfilecontentpipe.h"

/**
* @brief Defines creation parameters for NvMMOpenCoreParserFunction
*/
typedef struct
{
    NvU32 UlpKpiMode;
    NvBool bEnableUlpMode;
    NvU32 bReduceVideoBuffers;
    void *pPrivate;
    NvOsLibraryHandle hDrmHandle;
} NvMMParserCoreCreationParameters;

/* Parser core handle */
typedef struct NvMMParserCoreRec *NvMMParserCoreHandle;

/* Parser core structure */
typedef struct NvMMParserCoreRec
{

    /* Private context of a particular core parser */
    void *pContext;

    NvMMParserCoreType eCoreType;
    /* Stream info for this core parser */
    NvMMStreamInfo *pStreamInfo;
    NvMMErrorEventFromDomain  domainName;

    NvU16 *licenseUrl;
    NvU8  *licenseChallenge;
    NvU32 licenseUrlSize;
    NvU32 licenseChallengeSize;

    /* URI Path */
    NvString szURI;

    /* Enable KPI mode */
    NvU32 UlpKpiMode;

    /* status signifying ULP mode */
    NvBool bEnableUlpMode;

    /* Total number of Streams */
    NvU32 totalStreams;

    /* Reduced buffer mode for video */
    NvU32 bReduceVideoBuffers;

    /* To store device handle */
    NvRmDeviceHandle hRmDevice;

    /* To store Buffer Manager Base memory handle */
    NvRmMemHandle hBaseMemHandle;

    /* To store the buffer manager physical address */
    NvRmPhysAddr pBufferMgrBaseAddress;

    /* To store buffer manager virtual address */
    NvU32 vBufferMgrBaseAddress;

    /* Cache size for this core */
    NvU64 CacheSize;

    /* Whether entire file is cached or partially cached*/
    NvBool bPatiallyCached;

    /* To find CP BuffMgr required or not */
    NvBool isBuffMgrNotRequired;

    /* To store whether Video Init Event sent or not */
    NvBool isVideoInitEventSent;

    /* video per frame time (100ns) */
    NvS64 perFrameTime;

    /* Contains the content pipe handle */
    CPhandle cphandle;

    /* Contains content pipe reference */
    CP_PIPETYPE *cPipe;

    CP_PIPETYPE_EXTENDED *pPipe;

    /* Minimum cache size for content pipe */
    NvU64 MinCacheSize; 

    /* Maximum cache size for content pipe */
    NvU64 MaxCacheSize; 

    /* Spare area size for overlapping data in content pipe */
    NvU32 SpareCacheSize;

    /* Actaul cache size allocated by content pipe */
    NvU64 pActualCacheSize;

    // indicates if this core is using the cached contentpipe
    NvBool bUsingCachedCP;

    // DRM handle, right now only ASF parser core uses this.
    NvOsLibraryHandle hDrmHandle;
    NvU32 NvDrmContext;
    NvU32 NvDrmUpdateMeteringInfo;
    NvU32 NvDrmDestroyContext;
    // indicates if this core is using linear reading
    NvBool bUsingMultipleStreamReads;
    // Stores the intermediate buffer size
    NvU32 StoredDataSize;
    // Disable Dynamic Buffer Configuration flag
    NvBool bDisableBuffConfig;
    // enable /disable the mp3 TS from avi parser
    NvBool Mp3Enable;

    /** 
    * NvMMParserCore::Open. Opens/Inits all parser realted info.
    *  
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param pURI          - URI to FileName. URI path for the parser to get content 
    *                        specific info. 
    */
    NvError (*Open)(NvMMParserCoreHandle hParserCore, NvString pURI);

    /** 
    * NvMMParserCore::GetProperties. Opens the core in non-caching mode, does minimal parsing and 
    * return properties
    *  
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param pURI          - URI to FileName. URI path for the parser to get content 
    *                        specific info. 
    */
    NvError (*GetProperties)(NvMMParserCoreHandle hParserCore, 
                             NvString pURI, 
                             NvMMParserCoreProperties *pParserCoreProperties);

    /** 
    * NvMMParserCore::Close. closes/destroys all parser realted info.
    *   
    * @param hParserCore   - NvMMParserCoreHandle; 
    */
    NvError (*Close)(NvMMParserCoreHandle hParserCore);

    /** 
    * NvMMParserCore::GetNumberOfStreams
    *
    * GetNumberOfStreams gives the total number streams available in a track.
    * e.g., A track having both Audio and Video, the GetNumberOfStreams would 
    * return two as total streams.
    *
    * @param hParserCore   - NvMMParserCoreHandle
    * @param streams       - Gives the total streams available in a track.
    */
    NvError (*GetNumberOfStreams)(NvMMParserCoreHandle hParserCore, NvU32 *streams);

    /** 
    * NvMMParserCore::GetStreamInfo
    * 
    * GetStreamInfo gives the per stream related information like 
    * StreamType(H264, MPEG4, AAC, MP3, WMA,WMV etc.,), 
    * Stream Properties(Audio, Video, IMage properties like 
    * SR,BR,Width,Height,TotalTime etc.,)
    *   
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param Stream index  - Stream index for to get stream info.
    * @param pInfo         - pointer to fill in the NvMMStreamInfo structure with stream type 
    *                        and stream properties, TotalTime etc...
    * 
    */
    NvError (*GetStreamInfo)(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);

    /** 
    * NvMMParserCore::GetAttribute
    * 
    * @param hParserCore   - NvMMParserCoreHandle 
    * 
    */

    NvError (*GetAttribute)(NvMMParserCoreHandle hParserCore, 
        NvU32 AttributeType, 
        NvU32 AttributeSize, 
        void *pAttribute);

    /** 
    * NvMMParserCore::SetRate
    * 
    * SetRate used to set the playback rate. 
    * rate/1000 = rate, e.g. 0 Pause, 1000 = 1X, 2000 = 2X FF, -2000 = 2X // REW
    *
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param  rate         - rate/1000 = rate, e.g. 0= pause, 1000 = 1X, 2000 = 2X FF, -2000 = 
    *                        2X // REW
    */
    NvError (*SetRate)(NvMMParserCoreHandle hParserCore, NvS32 rate); 

    /** 
    * NvMMParserCore::GetRate
    * 
    * GetRate used to get the playback rate that is currently set to. e.g., 
    * PLAY, FFW or REW.
    *  
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param Stream index  - Stream index to get rate for.
    * @return              - returns the playback rate. If it's 0= Pause,  
    *                        1000 = 1X Normal Play, 2000 = 2X FF, -2000 = 2X // REW
    */
    NvS32 (*GetRate)(NvMMParserCoreHandle hParserCore);

    /** 
    * NvMMParserCore::SetPosition
    *
    * SetPosition -SetPosition does seek to a particular location/postion. SetPosition internally 
    * correlates to the seek location specified by the client in msec to a byteoffset location in the
    * file stream. It also upDates back the timeStamp with the byte Offset location.
    * e.g., In trick modes and RepeatAP Playback and in cases where it is 
    * required to jump and start playback from particular position.
    * When resume Playback from FFW/REW to happen, the time stamp passed to 
    * video and in turn to audio using  SetPosition to figure out the absolute byte 
    * offset position to start playback.
    * REPEAT A-B: To start playback from time A to B.
    *
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param timestamp     - Time specified by client in msec. SetPostion internally 
    *                        correlates this time(msec)to an absolute offset in bytes location 
    *                        that should be seeked to in the file stream. In the end, The timeStamp 
    *                        value is updated back again after converting ByteOffset value  time(in ms).
    */
    NvError (*SetPosition)(NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 

    /** 
    * NvMMParserCore::GetPosition
    * 
    * GetPosition - Gets the current offset postion in time. It Internally correlates the current 
    * Bytes Offset postion value to time in ms and fills in timestamp.
    * 
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param timestamp     - param to store the correlated bytes offset loaction in msec.
    */
    NvError (*GetPosition)(NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 

    /** 
    * NvMMParserCore::GetBufferRequirements
    * 
    * GetNextWorkUnit -Gets the Next Work Unit of Data to be processed for the 
    * requested size to pBuffer. It also updates the actual size of data it has read.
    *      
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param Stream index  - Stream index to get next work unit for
    * @param Retry         - 
    * @param pBufReq       - 
    * 
    */
    NvBool (*GetBufferRequirements)(NvMMParserCoreHandle hParserCore,
        NvU32 StreamIndex,
        NvU32 Retry,
        NvMMNewBufferRequirementsInfo *pBufReq);
    /** 
    * NvMMParserCore::GetNextWorkUnit
    * 
    * GetNextWorkUnit -Gets the Next Work Unit of Data to be processed for the 
    * requested size to pBuffer. It also updates the actual size of data it has read.
    *      
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param Stream index  - Stream index to get next work unit for
    * @param pBuffer       - pointer to NvMMBuffer
    * @param size          - value that specifies the size of data to be read to pBuffer. 
    *                        Once successful this also stores the actual size read.
    * 
    */
    NvError (*GetNextWorkUnit)(
        NvMMParserCoreHandle hParserCore,
        NvU32 streamIndex,
        NvMMBuffer *pBuffer, 
        NvU32 *size,
        NvBool *pMoreWorkPending); 

    /** 
    * NvMMParserCore::GetMaxOffsets. 
    * Gets the maximum number of offsets per offset list in ULP mode
    * 
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param pOffsets      - Used to return max number of offsets. 
    */
    void (*GetMaxOffsets)(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);


    /** 
    * NvMMParserCore::ReleaseOffsetList. 
    * Releases the number of offsets per offset list in ULP mode
    * 
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param pListBuffer   - OffsetList Buffer
    * @param StreamIndex   - Stream index to find buffer returned from
    */

    NvError (*ReleaseOffsetList)(
        NvMMParserCoreHandle hParserCore,
        NvMMBuffer* pListBuffer,
        NvU32 StreamIndex);

    /** 
    * NvMMParserCore::commitDrm. 
    * call DRM Commit API 
    * 
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param pListBuffer   - OffsetList Buffer
    */

    NvError (*commitDrm)(
        NvMMParserCoreHandle hParserCore,
        NvU32 drmCore
        );

    /**
    * NvMMParserCore::ResetUlpMode
    * to Reset the Ulpmode flags of core parser and disable offsetlist
    * @param hParserCore   - NvMMParserCoreHandle 
    * @param bUlpMode   - UlpMode flag
    */
    void (*ResetUlpMode)(
        NvMMParserCoreHandle hParserCore,
        NvBool bUlpMode);

    NvError (*GetNextWorkUnitMultipleBufs)(
        NvMMParserCoreHandle hParserCore,
        NvU32 *streamIndex,
        NvMMBuffer **pBuffer, 
        NvU32 *size,
        NvBool *pMoreWorkPending); 

    /**
    * NvMMParserCore::Enabling specific config
    */
    void (*EnableConfig)(NvMMParserCoreHandle hParserCore);

} NvMMParserCore;

/**
* @brief Each core implementation will expose an open entrypoint with the following
* signature.
*
* @param hParserCore Pointer to a NvMMParserCore handle to be created
* @param pParams creation parameters
* @retval NvSuccess operation was successful.
*
*/
typedef 
NvError (*NvMMCreateCoreParser)(NvMMParserCoreHandle *hParserCore, 
                                NvMMParserCoreCreationParameters *pParams);

/**
* @brief Each core implementation will expose a close exit point with the following
* signature.
*
* @param hParserCore NvMMParserCore handle to be destroyed.
*
*/
typedef void (*NvMMDestroyCoreParser)(NvMMParserCoreHandle hParserCore);


#endif  //INCLUDED_NVMM_PARSER_CORE_H


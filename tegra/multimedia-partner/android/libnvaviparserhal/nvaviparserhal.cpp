/* Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "NVAVIParserHal"

#include "nvmm.h"
#include "nvaviparserhal.h"
#include "nvparserhal_utils.h"
#include "nvmm_aac.h"
#include "nvmm_wave.h"
#include <dlfcn.h>

/******************************************************************************
* Macros
******************************************************************************/

#define MAX_NUM_BUFFERS 15

#define MAX_MP2_INPUT_BUFFER_SIZE 4096
#define MIN_MP2_INPUT_BUFFER_SIZE 4096

#define CHECK_BUFFER_VALIDITY(x)    \
    if (x == NULL) {\
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);\
        return BAD_VALUE;\
    }

namespace android {

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
* Data structures
******************************************************************************/
typedef struct {
    NvAviCoreParserInit_HAL pNvAviCoreParserInit_HAL;
    NvAviCoreParserDeInit pNvAviCoreParserDeInit;
    NvAviCoreParserDemux pNvAviCoreParserDemux;
    NvAviCoreParserNextWorkUnit pNvAviCoreParserNextWorkUnit;
    AviCoreParserGetNextSyncUnit pAviCoreParserGetNextSyncUnit;
    AviCoreParserGetdVideowSuggestedBufferSize pAviCoreParserGetdVideowSuggestedBufferSize;
    NvAviCoreParserSeekToAudioFrame pNvAviCoreParserSeekToAudioFrame;
}NvAviCoreParserApi;

typedef struct {
    uint32_t                        streamIndex;
    uint32_t                        parserStreamIndex;
    NvMMBuffer*                     pBuffer;
    NvParserHalBufferRequirements   bufReq;
}NvAviParserHalStream;

typedef struct {
    bool                            bHasAudioTrack;
    bool                            bHasVideoTrack;
    bool                            bHasSupportedVideo;
    int32_t                         AudioIndex;
    int32_t                         VideoIndex;
    uint32_t                        streamCount;
    uint64_t                        streamDuration;
    NvAviParserHalStream            **pStreams;
    NvMMAviParserCoreContext        *pCoreContext;
    NvU8                            *filename;
    void*                           pNvAviCoreParserLib;
    NvAviCoreParserApi              pNvAviCoreParserApi;
}NvAviParserHalCore;

static const NvS32 Avi_AacSamplingRateTable[16] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025,  8000,  7350,    -1,    -1,     0
};

/******************************************************************************
* Function Declarations
******************************************************************************/
static status_t NvAviParserHal_OpenCore(
                NvAviParserHalCore* pNvAviParserHalCoreHandle);
static status_t NvAviParserHal_DestroyCore(
                NvAviParserHalCore* pNvAviParserHalCoreHandle);
static status_t NvAviParserHal_CalculateBufRequirements(
                NvAviParserHalCore* pNvAviParserHalCoreHandle,
                NvAviParserHalStream* pStream,
                int32_t streamIndex);
static status_t NvAviParserHal_CreateStream(
                NvAviParserHalCore* pNvAviParserHalCoreHandle,
                int32_t streamIndex,
                int32_t parserStreamIndex);
static status_t NvAviParserHal_DestroyStream(
                NvAviParserHalCore* pNvAviParserHalCoreHandle,
                int32_t streamIndex);
static NvError NvAviParserHal_ReadAVPacket(
               NvAviParserHalCore* pNvAviParserHalCoreHandle,
               int32_t StreamIndex, NvMMBuffer* pBuffer);
static status_t NvAviParserHal_SetPosition(
                void* aviParserHalHandle,
                int32_t streamIndex,
                uint64_t seek_position,
                MediaSource::ReadOptions::SeekMode mode);
static uint64_t NvAviParserHal_GetDuration(
                NvAviParserHalCore* pNvAviParserHalCoreHandle);
static status_t NvAviParserHal_SetTrackMimeType(
                void* aviParserHalHandle,
                int32_t StreamIndex,
                sp<MetaData> metaData);

static NvU16 CalcESDS(NvMMAudioAacTrackInfo *paacTrackInfo);
static NvError NvAviParserHal_InitializeHeaders(
               NvMMAviParserCoreContext *pContext,
               NvU32 streamIndex,
               NvMMBuffer* pBuffer,
               NvMMStreamType StreamType, NvU32 *size);
/******************************************************************************
* API Definitions
******************************************************************************/
/******************************************************************************
* Function    : NvAviParserHal_GetInstanceFunc
* Description : Returns handles to AVI HAL API's to the caller
******************************************************************************/
void NvAviParserHal_GetInstanceFunc(NvParseHalImplementation* halImpl) {
    halImpl->nvParserHalCreateFunc           = NvAviParserHal_Create;
    halImpl->nvParserHalDestroyFunc          = NvAviParserHal_Destroy;
    halImpl->nvParserHalGetStreamCountFunc   = NvAviParserHal_GetStreamCount;
    halImpl->nvParserHalGetMetaDataFunc      = NvAviParserHal_GetMetaData;
    halImpl->nvParserHalGetTrackMetaDataFunc = NvAviParserHal_GetTrackMetaData;
    halImpl->nvParserHalSetTrackHeaderFunc   = NvAviParserHal_SetTrackHeader;
    halImpl->nvParserHalReadFunc             = NvAviParserHal_Read;
    halImpl->nvParserHalIsSeekableFunc       = NvAviParserHal_IsSeekable;
}

/******************************************************************************
* Function    : NvAviParserHal_Create
* Description : Instantiates the AVI Parser hal and the core parser.
*               Parses the header objects
******************************************************************************/
status_t NvAviParserHal_Create(char* Url, void **pAviParserHalHandle) {
    NvU32 nBytesRead = 0, len = 0, i;
    status_t ret = OK;
    NvError err = NvSuccess;
    NvAviParserHalCore* pNvAviParserHalCoreHandle = NULL;
    NvMMAviParserCoreContext* pContext = NULL;
    ALOGV("+++ %s +++",__FUNCTION__);

    pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)NvOsAlloc(sizeof(NvAviParserHalCore));
    if (pNvAviParserHalCoreHandle == NULL) {
        ret = NO_MEMORY;
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }
    NvOsMemset(pNvAviParserHalCoreHandle, 0, sizeof(NvAviParserHalCore));
    // Initializing to non-zero number.
    pNvAviParserHalCoreHandle->AudioIndex = 0xDeadBeef;
    pNvAviParserHalCoreHandle->VideoIndex = 0xDeadBeef;

    // Clear the old errors, no need to handle return for clearing
    dlerror();
    pNvAviParserHalCoreHandle->pNvAviCoreParserLib = dlopen("libnvmm_aviparser.so", RTLD_NOW);
    if (pNvAviParserHalCoreHandle->pNvAviCoreParserLib == NULL) {
        ALOGE("===== %s[%d] Error =====, %s",__FUNCTION__, __LINE__,dlerror());
        ret = NO_MEMORY;
        goto cleanup;
    }

    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserInit_HAL =
        (NvAviCoreParserInit_HAL) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"NvAviParserInit_HAL");
    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserDeInit =
        (NvAviCoreParserDeInit) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"NvAviParserDeInit");
    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserDemux =
        (NvAviCoreParserDemux) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"NvAviParserDemux");
    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserNextWorkUnit =
        (NvAviCoreParserNextWorkUnit) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"NvAviParserNextWorkUnit");
    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pAviCoreParserGetNextSyncUnit =
        (AviCoreParserGetNextSyncUnit) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"AviParserGetNextSyncUnit");
    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pAviCoreParserGetdVideowSuggestedBufferSize =
        (AviCoreParserGetdVideowSuggestedBufferSize) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"AviParserGetdVideowSuggestedBufferSize");
    pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserSeekToAudioFrame =
        (NvAviCoreParserSeekToAudioFrame) dlsym(pNvAviParserHalCoreHandle->pNvAviCoreParserLib,"NvAviParserSeekToAudioFrame");

    pNvAviParserHalCoreHandle->pCoreContext =
        (NvMMAviParserCoreContext *)NvOsAlloc(sizeof(NvMMAviParserCoreContext));
    if (pNvAviParserHalCoreHandle->pCoreContext == NULL) {
        ret = NO_MEMORY;
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }
    NvOsMemset(pNvAviParserHalCoreHandle->pCoreContext, 0,
               sizeof(NvMMAviParserCoreContext));
    pContext = pNvAviParserHalCoreHandle->pCoreContext;

    len = NvOsStrlen((const char*)Url);

    pNvAviParserHalCoreHandle->filename =
        (NvU8 *)NvOsAlloc((len+1)*sizeof(NvU8));
    if (!pNvAviParserHalCoreHandle->filename) {
        ret = NO_MEMORY;
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }

    // Copy the filename
    NvOsStrncpy((char *)pNvAviParserHalCoreHandle->filename,
                (const char*)Url, len+1);

    // Open the core parser
    ret = NvAviParserHal_OpenCore(pNvAviParserHalCoreHandle);
    if (ret != OK) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }

    // Check if the stream is valid
    if (pContext->Parser.AviHeader.dwStreams <= 0) {
        ret = UNKNOWN_ERROR;
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }

    pNvAviParserHalCoreHandle->pStreams =
        (NvAviParserHalStream**)NvOsAlloc(sizeof(NvAviParserHalStream*) * pContext->Parser.AviHeader.dwStreams);
    if (pNvAviParserHalCoreHandle->pStreams == NULL) {
        ret = NO_MEMORY;
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }
    NvOsMemset(pNvAviParserHalCoreHandle->pStreams, 0,
               sizeof(NvAviParserHalStream*) * pContext->Parser.AviHeader.dwStreams);
    if (pContext->Parser.AviHeader.dwStreams > 2) {

        i = 0;
        if (NVMM_ISSTREAMVIDEO(pContext->Parser.AllTrackInfo[pContext->Parser.VideoId].ContentType)
           && (pContext->Parser.AllTrackInfo[pContext->Parser.VideoId].ContentType !=
                   NvMMStreamType_UnsupportedVideo)) {

            ret = NvAviParserHal_CreateStream(pNvAviParserHalCoreHandle, i,
                        pContext->Parser.VideoId);

            if (ret != OK) {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                goto cleanup;
            }
            i++;
        }
        if (NVMM_ISSTREAMAUDIO(pContext->Parser.AllTrackInfo[pContext->Parser.AudioId].ContentType)
           && (pContext->Parser.AllTrackInfo[pContext->Parser.AudioId].ContentType !=
                   NvMMStreamType_UnsupportedAudio)) {

            ret = NvAviParserHal_CreateStream(pNvAviParserHalCoreHandle, i,
                        pContext->Parser.AudioId);

            if (ret != OK) {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                goto cleanup;
            }
        }
    } else {
        for(i=0; i<pContext->Parser.AviHeader.dwStreams; i++) {
            ret = NvAviParserHal_CreateStream(pNvAviParserHalCoreHandle,i,i);
            if (ret != OK) {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                goto cleanup;
            }
        }
    }

    // Get the stream Duration
    pNvAviParserHalCoreHandle->streamDuration =
        NvAviParserHal_GetDuration(pNvAviParserHalCoreHandle);

    // Return the handle to caller
    *pAviParserHalHandle = pNvAviParserHalCoreHandle;
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;

cleanup:
    if (pNvAviParserHalCoreHandle) {
        if (pContext) {
            if (pNvAviParserHalCoreHandle->pStreams) {
                for(i=0; i<pContext->Parser.AviHeader.dwStreams; i++) {
                    if (pNvAviParserHalCoreHandle->pStreams[i]) {
                        NvAviParserHal_DestroyStream(pNvAviParserHalCoreHandle,i);
                    }
                }
                NvOsFree(pNvAviParserHalCoreHandle->pStreams);
                pNvAviParserHalCoreHandle->pStreams = NULL;
            }

            NvAviParserHal_DestroyCore(pNvAviParserHalCoreHandle);

            //NvOsFree(pNvAviParserHalCoreHandle->pCoreContext);
            //pNvAviParserHalCoreHandle->pCoreContext = NULL;
        }

        if (pNvAviParserHalCoreHandle->filename) {
            NvOsFree(pNvAviParserHalCoreHandle->filename);
            pNvAviParserHalCoreHandle->filename = NULL;
        }

        NvOsFree(pNvAviParserHalCoreHandle);
        pNvAviParserHalCoreHandle = NULL;
    }
    *pAviParserHalHandle = NULL;
    ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
    return ret;
}

/******************************************************************************
* Function    : NvAviParserHal_IsSeekable
* Description : Tells the caller if the avi stream is seekable.
                Extractor can set the flags accordingly.
******************************************************************************/
bool NvAviParserHal_IsSeekable(void* aviParserHalHandle)
{
    NvAviParserHalCore* pNvAviParserHalCoreHandle =
            (NvAviParserHalCore*)aviParserHalHandle;
    NvMMAviParserCoreContext* pContext = NULL;
    ALOGV("+++ %s +++",__FUNCTION__);

    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    pContext = pNvAviParserHalCoreHandle->pCoreContext;

    bool isSeekable = false;

    if (pContext) {
       if (pContext->Parser.AviHasIndex ||
            pContext->Parser.AviMustUseIndex ||
            pContext->Parser.Odmlflag) {
           isSeekable = true;
       }
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return isSeekable;
}

/******************************************************************************
* Function    : NvAviParserHal_Destroy
* Description : Destroys the AVI Parser hal and the core parser.
*               Releases the resources
******************************************************************************/
status_t NvAviParserHal_Destroy(void* aviParserHalHandle) {
    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;
    uint32_t i;
    NvMMAviParserCoreContext* pContext = NULL;
    ALOGV("+++ %s +++",__FUNCTION__);

    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    pContext = pNvAviParserHalCoreHandle->pCoreContext;

    if (pContext) {
        if (pNvAviParserHalCoreHandle->pStreams) {
            for(i=0; i<pContext->Parser.AviHeader.dwStreams; i++) {
                if (pNvAviParserHalCoreHandle->pStreams[i]) {
                    NvAviParserHal_DestroyStream(pNvAviParserHalCoreHandle,i);
                }
            }
            NvOsFree(pNvAviParserHalCoreHandle->pStreams);
            pNvAviParserHalCoreHandle->pStreams = NULL;
        }

        NvAviParserHal_DestroyCore(pNvAviParserHalCoreHandle);

        NvOsFree(pNvAviParserHalCoreHandle->pCoreContext);
        pNvAviParserHalCoreHandle->pCoreContext = NULL;
    }

    if (pNvAviParserHalCoreHandle->filename) {
        NvOsFree(pNvAviParserHalCoreHandle->filename);
        pNvAviParserHalCoreHandle->filename = NULL;
    }

    NvOsFree(pNvAviParserHalCoreHandle);
    pNvAviParserHalCoreHandle = NULL;

    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/******************************************************************************
* Function    : NvAviParserHal_GetMetaData
* Description : Sends the meta data from a media file to caller
******************************************************************************/
status_t NvAviParserHal_GetMetaData(
            void* aviParserHalHandle,
            sp<MetaData> mFileMetaData) {

    ALOGV("+++ %s +++",__FUNCTION__);
    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;
    size_t kNumMapEntries, j, i;
    uint32_t streamIndex;
    status_t err = OK;

    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    NvMMAviParserCoreContext* pContext = pNvAviParserHalCoreHandle->pCoreContext;

    struct Map {
        int to;
        NvMMMetaDataTypes from;
        int type;
    };
    Map kMap[] = {
    {kKeyCDTrackNumber, NvMMMetaDataInfo_TrackNumber, MetaData::TYPE_C_STRING},
    {kKeyAlbum, NvMMMetaDataInfo_Album, MetaData::TYPE_C_STRING},
    {kKeyArtist, NvMMMetaDataInfo_Artist, MetaData::TYPE_C_STRING},
    {kKeyAlbumArtist, NvMMMetaDataInfo_AlbumArtist, MetaData::TYPE_C_STRING},
    {kKeyComposer, NvMMMetaDataInfo_Composer, MetaData::TYPE_C_STRING},
    {kKeyGenre, NvMMMetaDataInfo_Genre, MetaData::TYPE_C_STRING},
    {kKeyTitle, NvMMMetaDataInfo_Title, MetaData::TYPE_C_STRING},
    {kKeyYear, NvMMMetaDataInfo_Year, MetaData::TYPE_C_STRING},
    {kKeyAlbumArt, NvMMMetaDataInfo_CoverArt, MetaData::TYPE_NONE},
    };
    kNumMapEntries = sizeof(kMap) / sizeof(kMap[0]);

    if (pNvAviParserHalCoreHandle->bHasVideoTrack) {
        // If video is present set container mime type, no need to check for audio
        ALOGV("%s[%d], video is avaliable = %d",
             __FUNCTION__,__LINE__,
             pContext->Parser.AllTrackInfo[pNvAviParserHalCoreHandle->VideoIndex].ContentType);
        mFileMetaData->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_AVI);
    } else if (pNvAviParserHalCoreHandle->bHasAudioTrack) {
        // In case of audio only streams,
        // need to set mime as per audio type to list it into music app
        ALOGV("%s[%d], audio is avaliable = %d",
             __FUNCTION__,__LINE__,
             pContext->Parser.AllTrackInfo[pNvAviParserHalCoreHandle->AudioIndex].ContentType);
        err = NvAviParserHal_SetTrackMimeType(pNvAviParserHalCoreHandle,
                                              pNvAviParserHalCoreHandle->AudioIndex, mFileMetaData);
        if (err != OK) {
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            return BAD_VALUE;
        }
    } else {
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            return BAD_VALUE;
    }

    for(j=0;j<kNumMapEntries;j++) {
        for(i = 0; i< MAX_META_DATA_TYPES; i++) {
            if (kMap[j].from == pContext->Parser.AviMetadata[i].MetaDataInfo.eMetadataType) {
                if (pContext->Parser.AviMetadata[i].MetaDataInfo.pClientBuffer &&
                    pContext->Parser.AviMetadata[i].MetaDataInfo.nBufferSize) {
                        if (kMap[j].type == MetaData::TYPE_C_STRING) {
                            if(utf8_length((const char*)pContext->Parser.AviMetadata[i].MetaDataInfo.pClientBuffer) >= 0 ){
                                String8 str8((const char*)pContext->Parser.AviMetadata[i].MetaDataInfo.pClientBuffer);
                                mFileMetaData->setCString(kMap[j].to, (const char *)str8.string());
                            } else {
                                String8 str8((const char16_t*)pContext->Parser.AviMetadata[i].MetaDataInfo.pClientBuffer);
                                mFileMetaData->setCString(kMap[j].to, (const char *)str8.string());
                            }
                        } else {
                            mFileMetaData->setData(kMap[j].to, kMap[j].type,
                                                   pContext->Parser.AviMetadata[i].MetaDataInfo.pClientBuffer,
                                                   pContext->Parser.AviMetadata[i].MetaDataInfo.nBufferSize);
                        }
                }
                break;
            }
        }
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/******************************************************************************
* Function    : NvAviParserHal_GetTrackMetaData
* Description : Sends the meta data from a specified media track to caller
******************************************************************************/
status_t NvAviParserHal_GetTrackMetaData(
            void* aviParserHalHandle,
            int32_t streamIndex,
            bool setThumbnailTime,
            sp<MetaData> mTrackMetaData) {

    NvAviParserHalCore* pNvAviParserHalCoreHandle = (NvAviParserHalCore*)aviParserHalHandle;
    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    NvMMAviParserCoreContext* pContext = pNvAviParserHalCoreHandle->pCoreContext;
    if(!pNvAviParserHalCoreHandle->pStreams)
        return BAD_VALUE;
    NvAviParserHalStream* pStream = pNvAviParserHalCoreHandle->pStreams[streamIndex];
    streamIndex = pStream->parserStreamIndex;
    status_t err = OK;

    if (!setThumbnailTime) {
        err = NvAviParserHal_SetTrackMimeType(pNvAviParserHalCoreHandle,
                                              streamIndex, mTrackMetaData);
        if (err != OK) {
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            return BAD_VALUE;
        }
        if (NVMM_ISSTREAMVIDEO(pContext->Parser.AllTrackInfo[streamIndex].ContentType) &&
            (pContext->Parser.AllTrackInfo[streamIndex].ContentType != NvMMStreamType_UnsupportedVideo)) {
            // Fill in Video Properties info.
            uint32_t height, width;
            width = pContext->Parser.AllTrackInfo[streamIndex].BmpWidth;
            height = pContext->Parser.AllTrackInfo[streamIndex].BmpHeight;
            mTrackMetaData->setInt32(kKeyWidth, width);
            mTrackMetaData->setInt32(kKeyHeight, height);
            mTrackMetaData->setInt32(kKeyBitRate, pContext->Parser.AllTrackInfo[streamIndex].BitRate);
            mTrackMetaData->setInt64(kKeyDuration, pNvAviParserHalCoreHandle->streamDuration);
            ALOGV("%s[%d], streamDuration = %llu, width = %d, height = %d, BitRate = %d",
                  __FUNCTION__,__LINE__,
                  pNvAviParserHalCoreHandle->streamDuration,
                  width,height,
                  pContext->Parser.AllTrackInfo[streamIndex].BitRate);

            mTrackMetaData->setInt32(kKeyMaxInputSize, pStream->bufReq.maxBufferSize);

            pNvAviParserHalCoreHandle->bHasSupportedVideo = true;
            pContext->ParseState[streamIndex] = AVIPARSE_HEADER;
        } else if (NVMM_ISSTREAMAUDIO(pContext->Parser.AllTrackInfo[streamIndex].ContentType)) {
            // Fill in Audio Properties info.
            mTrackMetaData->setInt32(kKeySampleRate, pContext->Parser.AllTrackInfo[streamIndex].SamplingFrequency);
            mTrackMetaData->setInt32(kKeyChannelCount, pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels);
            mTrackMetaData->setInt32(kKeyBitRate, pContext->Parser.AllTrackInfo[streamIndex].BitRate);
            mTrackMetaData->setInt32(kKeyMaxInputSize, pStream->bufReq.maxBufferSize);
            mTrackMetaData->setInt64(kKeyDuration, pNvAviParserHalCoreHandle->streamDuration);
            mTrackMetaData->setInt32(kKeyBitsPerSample, pContext->Parser.AllTrackInfo[streamIndex].SampleSize);

            ALOGV("%s[%d], streamDuration = %llu, channels = %d, SampleRate = %d, BitRate = %d, sampleSize - %d",
                 __FUNCTION__,__LINE__,
                 pNvAviParserHalCoreHandle->streamDuration,
                 pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels,
                 pContext->Parser.AllTrackInfo[streamIndex].SamplingFrequency,
                 pContext->Parser.AllTrackInfo[streamIndex].BitRate,
                 pContext->Parser.AllTrackInfo[streamIndex].SampleSize);
            pContext->ParseState[streamIndex] = AVIPARSE_HEADER;
        } else {
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            return BAD_VALUE;
        }
    } else {
        if (pContext->Parser.TimeStampToReturnOnThumbNailRequest == (NvU64)-1) {
            ALOGV("===== %s[%d] Error =====, No thumbnail seek time available",__FUNCTION__, __LINE__);
            mTrackMetaData->setInt64(kKeyThumbnailTime, 0);
        } else {
            mTrackMetaData->setInt64(kKeyThumbnailTime, pContext->Parser.TimeStampToReturnOnThumbNailRequest/10);
            ALOGV("%s[%d], thumbnail seek time = %llu",
                 __FUNCTION__,__LINE__,pContext->Parser.TimeStampToReturnOnThumbNailRequest);
        }
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}
/******************************************************************************
* Function    : NvAviParserHal_GetStreamCount
* Description : Returns the number of streams present in media to caller
******************************************************************************/
size_t NvAviParserHal_GetStreamCount(void* aviParserHalHandle) {
    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;
    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);

    if (pNvAviParserHalCoreHandle->pCoreContext) {
        ALOGV("--- %s[%d] ---, streamCount = %d",__FUNCTION__,__LINE__, pNvAviParserHalCoreHandle->streamCount);
        return pNvAviParserHalCoreHandle->streamCount;
    } else {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return 0;
    }
}

/******************************************************************************
* Function    : NvAviParserHal_Read
* Description : Returns the buffer containing encoded data for one frame of a particular
*               stream to caller
******************************************************************************/
status_t NvAviParserHal_Read(
            void* aviParserHalHandle,
            int32_t streamIndex,
            MediaBuffer *mBuffer,
            const MediaSource::ReadOptions* options) {

    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;
    int64_t timeStamp = 0, seekTimeUs = 0;
    status_t ret = OK;
    NvError err = NvSuccess;
    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    CHECK_BUFFER_VALIDITY(mBuffer);
    uint8_t * temp = (uint8_t *) mBuffer->data();
    CHECK_BUFFER_VALIDITY(temp);

    // Handle seek condition
    MediaSource::ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs,&mode)) {
        ret = NvAviParserHal_SetPosition(pNvAviParserHalCoreHandle,
                                         (pNvAviParserHalCoreHandle->pStreams[streamIndex])->parserStreamIndex,
                                         seekTimeUs, mode);
        if (ret == ERROR_END_OF_STREAM)
        {
            ALOGV("--- %s[%d] -- not allowing seek for vid+aud file when EOS is reached",
                 __FUNCTION__, __LINE__);
            ALOGV("--- %s[%d] -- Playback will continue",
                 __FUNCTION__, __LINE__);
        } else if (ret != OK) {
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            return ret;
        }
    }

    NvAviParserHalStream* pStream =
        pNvAviParserHalCoreHandle->pStreams[streamIndex];
    streamIndex = pStream->parserStreamIndex;

    if(mBuffer->size() < pStream->bufReq.maxBufferSize) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return ERROR_BUFFER_TOO_SMALL;
    }
    // Pass handle to MediaBuffer's data buffer to get data
    pStream->pBuffer->Payload.Ref.sizeOfBufferInBytes = mBuffer->size();
    pStream->pBuffer->Payload.Ref.pMem = temp;

    err = NvAviParserHal_ReadAVPacket(pNvAviParserHalCoreHandle, streamIndex, pStream->pBuffer);
    if ((pStream->pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_EndOfStream) || (err == NvError_ParserEndOfStream)) {
        mBuffer->set_range(0,0);
        ALOGV("--- %s[%d] ---, Sending EOS",__FUNCTION__,__LINE__);
        return ERROR_END_OF_STREAM;
    } else if (err != NvSuccess) {
        return err;
    }

    mBuffer->set_range(0, pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes);
    mBuffer->meta_data()->clear();
    if (pStream->pBuffer->PayloadInfo.TimeStamp > 0) {
        timeStamp = (int64_t)(pStream->pBuffer->PayloadInfo.TimeStamp)/10;
    }
    mBuffer->meta_data()->setInt64(kKeyTime, timeStamp);
    ALOGV("%s[%d] :: pStream->pBuffer->PayloadInfo.TimeStamp = %llu",
         __FUNCTION__, __LINE__,pStream->pBuffer->PayloadInfo.TimeStamp);
    ALOGV("%s[%d] :: pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes = %d",
          __FUNCTION__, __LINE__,pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes);

    if (pStream->pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_PTSCalculationRequired) {
        mBuffer->meta_data()->setInt32(kKeyDecoderFlags, OMX_BUFFERFLAG_NEED_PTS);
    }
    if (pStream->pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag) {
        mBuffer->meta_data()->setInt32(kKeyDecoderFlags, OMX_BUFFERFLAG_CODECCONFIG);
    }
    if (pStream->pBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame == NV_TRUE) {
        mBuffer->meta_data()->setInt32(kKeyDecoderFlags, OMX_BUFFERFLAG_SYNCFRAME);
    }

    // Clear some fields
    pStream->pBuffer->PayloadInfo.BufferFlags = 0;
    pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    pStream->pBuffer->PayloadInfo.TimeStamp = 0;
    pStream->pBuffer->Payload.Ref.sizeOfBufferInBytes = 0;
    pStream->pBuffer->Payload.Ref.pMem = NULL;

    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/******************************************************************************
* Function    : NvAviParserHal_SetTrackHeader
* Description : Sets the header info of a particular track
******************************************************************************/
status_t NvAviParserHal_SetTrackHeader(
            void* aviParserHalHandle,
            int32_t streamIndex,
            sp<MetaData> meta) {

    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;
    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    NvMMAviParserCoreContext* pContext = pNvAviParserHalCoreHandle->pCoreContext;
    NvError Status = NvSuccess;
    NvU32 i;
    NvMMAudioAacTrackInfo paacTrackInfo;

    streamIndex = (pNvAviParserHalCoreHandle->pStreams[streamIndex])->parserStreamIndex;

    ALOGV("%s[%d], streamIndex = %d, ContentType = %d",
         __FUNCTION__,__LINE__,
         streamIndex,pContext->Parser.AllTrackInfo[streamIndex].ContentType);

    if (NVMM_ISSTREAMVIDEO(pContext->Parser.AllTrackInfo[streamIndex].ContentType) &&
       (pContext->Parser.AllTrackInfo[streamIndex].ContentType != NvMMStreamType_UnsupportedVideo)) {
        if (NULL != pContext->Parser.h264CodecData) {
            if (pContext->Parser.h264CodecDataSz > 0) {
                if ((pContext->Parser.h264CodecData[0] == 0x00 &&
                     pContext->Parser.h264CodecData[1] == 0x00 &&
                     pContext->Parser.h264CodecData[2] == 0x00 &&
                     pContext->Parser.h264CodecData[3] == 0x01)
                    ||
                    (pContext->Parser.h264CodecData[0] == 0x00 &&
                     pContext->Parser.h264CodecData[1] == 0x00 &&
                     pContext->Parser.h264CodecData[2] == 0x01)) {
                    uint32_t type;
                    const void *data;
                    size_t size;
                    sp<MetaData> metahdr;
                    sp<ABuffer> csd = new ABuffer(pContext->Parser.h264CodecDataSz);

                    memcpy(csd->data(),pContext->Parser.h264CodecData,
                           pContext->Parser.h264CodecDataSz);
                    metahdr = MakeAVCCodecSpecificData(csd);
                    if (metahdr != NULL) {
                        metahdr->findData(kKeyAVCC, &type,&data,&size);
                        ALOGV("%s[%d], setting H264 video header data size = %d",
                              __FUNCTION__,__LINE__,size);
                        meta->setData(kKeyAVCC, kTypeHeader, data, size);
                    }
                    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
                    return OK;
                } else {
                    meta->setData(kKeyAVCC, kTypeHeader,
                                  pContext->Parser.h264CodecData,
                                  pContext->Parser.h264CodecDataSz);
                    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
                    return OK;
                }
            } else {
                meta->setData(kKeyAVCC, kTypeHeader, NULL, 0);
                ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
                return OK;
            }
        }
        Status = (NvError)pContext->Parser.pPipe->SetPosition64 (
                                    pContext->Parser.hContentPipe,
                                    (CPint64) pContext->Parser.VolStartCodeOffset,
                                    CP_OriginBegin);
        if (Status != NvSuccess) {
            if (Status == NvError_FileOperationFailed) {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                return UNKNOWN_ERROR;
            } else if (Status == NvError_EndOfFile) {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                return BAD_VALUE;
            } else {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                return UNKNOWN_ERROR;
            }
        }

        NvU8 *tempHeader= (NvU8 *)malloc(pContext->Parser.VolHeaderSize);
        if (NULL == tempHeader)
            return NO_MEMORY;

        if (pContext->Parser.pPipe->cpipe.Read (
                                    pContext->Parser.hContentPipe,
                                    (CPbyte *) tempHeader,
                                    pContext->Parser.VolHeaderSize) != NvSuccess) {
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            free(tempHeader);
            return UNKNOWN_ERROR;
        }

        meta->setData(kKeyHeader, kTypeHeader, tempHeader, pContext->Parser.VolHeaderSize);
        free(tempHeader);
        ALOGV("AVI PARSER: Video header length %d",pContext->Parser.VolHeaderSize);
    } else if (NVMM_ISSTREAMAUDIO(pContext->Parser.AllTrackInfo[streamIndex].ContentType)) {
        if (pContext->Parser.AllTrackInfo[streamIndex].ContentType == NvMMStreamType_AAC) {
            NvOsMemset (&paacTrackInfo, 0, sizeof (NvMMAudioAacTrackInfo));
            paacTrackInfo.objectType = 2;

            if (pContext->Parser.IsAdtsHeader) {
                paacTrackInfo.profile = pContext->Parser.ADTSHeader.profile;
                paacTrackInfo.samplingFreqIndex =
                    pContext->Parser.ADTSHeader.sampling_freq_idx;
                paacTrackInfo.samplingFreq =
                    Avi_AacSamplingRateTable[pContext->Parser.ADTSHeader.sampling_freq_idx];
                paacTrackInfo.noOfChannels =
                    pContext->Parser.ADTSHeader.channel_config;
                paacTrackInfo.sampleSize =
                    pContext->Parser.AllTrackInfo[streamIndex].SampleSize;
                paacTrackInfo.channelConfiguration =
                    pContext->Parser.ADTSHeader.channel_config;
                paacTrackInfo.bitRate =
                    pContext->Parser.AllTrackInfo[streamIndex].BitRate;
                paacTrackInfo.objectType = paacTrackInfo.profile + 1;
                meta->setInt32(kKeyAACProfile, paacTrackInfo.objectType);
                meta->setInt32(kKeyIsADTS, true);
            } else {
                paacTrackInfo.profile = 0x40;
                paacTrackInfo.samplingFreq =
                    pContext->Parser.AllTrackInfo[streamIndex].SamplingFrequency;
                for (i = 0; i < 16; i++) {
                    if (Avi_AacSamplingRateTable[i] == (NvS32) paacTrackInfo.samplingFreq) {
                        paacTrackInfo.samplingFreqIndex = i;
                        break;
                    }
                }
                paacTrackInfo.noOfChannels =
                    pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels;
                paacTrackInfo.sampleSize =
                    pContext->Parser.AllTrackInfo[streamIndex].SampleSize;
                paacTrackInfo.channelConfiguration =
                    pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels;
                paacTrackInfo.bitRate =
                    pContext->Parser.AllTrackInfo[streamIndex].BitRate;
                paacTrackInfo.objectType = 2;
            }

            ALOGV("%s , streamIndex = %d, cbSize = %d",
                  __FUNCTION__,streamIndex,
                  pContext->Parser.AllTrackInfo[streamIndex].CbSize);
            if (pContext->Parser.AllTrackInfo[streamIndex].CbSize) {
                meta->setData(kKeyHeader, kTypeHeader,
                              (NvU8*) pContext->Parser.AllTrackInfo[streamIndex].pCodecSpecificData,
                              pContext->Parser.AllTrackInfo[streamIndex].CbSize);
                ALOGV("Audio AAC non ESDS --- %s ---, size = %d",
                      __FUNCTION__,pContext->Parser.AllTrackInfo[streamIndex].CbSize);
            } else {
                NvU16 data = 0;
                data = CalcESDS(&paacTrackInfo);
                meta->setData(kKeyHeader, kTypeHeader, (NvU8*) &data, 2);
                ALOGV("Audio AAC ESDS --- %s ---,data = 0x%x",__FUNCTION__,data);
            }
        }
    } else {
        meta->setData(kKeyAVCC, kTypeHeader, NULL, 0);
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/******************************************************************************
* Static Functions
******************************************************************************/
static status_t NvAviParserHal_OpenCore(
            NvAviParserHalCore* pNvAviParserHalCoreHandle) {

    NvError err = NvSuccess;

    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    NvMMAviParserCoreContext* pContext = pNvAviParserHalCoreHandle->pCoreContext;
    NvParserHal_GetFileContentPipe(&pContext->Parser.pPipe);
    err = pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserInit_HAL(
                                &pContext->Parser,
                                (NvString)pNvAviParserHalCoreHandle->filename);
    if (err != NvSuccess) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }

    if (pContext->Parser.pPipe->IsStreaming (pContext->Parser.hContentPipe)) {
        pContext->Parser.IsStreaming = NV_TRUE;
    }

    err = pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserDemux(
                                        &pContext->Parser);
    if (err != NvSuccess) {
        ALOGE("===== %s[%d] Error =====, 0x%x",__FUNCTION__, __LINE__, err);
        goto cleanup;
    }

    pContext->rate.Rate = 1000;
    pContext->Parser.bMp3Enable  = NV_TRUE;//enabled by default//hParserCore->Mp3Enable;

cleanup:
    if (err != NvSuccess) {
        NvAviParserHal_DestroyCore(pNvAviParserHalCoreHandle);
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return UNKNOWN_ERROR;
    }

    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/*****************************************************************************/
static status_t NvAviParserHal_DestroyCore(
            NvAviParserHalCore* pNvAviParserHalCoreHandle) {

    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);

    if (pNvAviParserHalCoreHandle->pCoreContext) {
        pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserDeInit (
                            &pNvAviParserHalCoreHandle->pCoreContext->Parser);
        NvOsFree (pNvAviParserHalCoreHandle->pCoreContext);
        pNvAviParserHalCoreHandle->pCoreContext = NULL;
        dlclose(pNvAviParserHalCoreHandle->pNvAviCoreParserLib);
        pNvAviParserHalCoreHandle->pNvAviCoreParserLib = NULL;
    }

cleanup:
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/*****************************************************************************/
static status_t NvAviParserHal_CalculateBufRequirements(
            NvAviParserHalCore* pNvAviParserHalCoreHandle,
            NvAviParserHalStream* pStream,
            int32_t streamIndex) {

    NvError Status = NvSuccess;
    NvMMAviParserCoreContext* pContext = NULL;
    NvMMStreamType type;
    NvU32 dwSuggestedBufSz;

    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    pContext = pNvAviParserHalCoreHandle->pCoreContext;

    NvParserHalBufferRequirements* pBufReq = &pStream->bufReq;
    pBufReq->minBuffers    = 4;
    pBufReq->maxBuffers    = MAX_NUM_BUFFERS;//NVMMSTREAM_MAXBUFFERS;
    pBufReq->byteAlignment = 4;

    type = pContext->Parser.AllTrackInfo[streamIndex].ContentType;

    if (NVMM_ISSTREAMAUDIO (type)) {
        pNvAviParserHalCoreHandle->bHasAudioTrack = true;
        pNvAviParserHalCoreHandle->AudioIndex = streamIndex;
        ALOGV("AviCoreParserGetBufferRequirements : STREAMAUDIO Type = %d, pNvAviParserHalCoreHandle->AudioIndex = %d", type, pNvAviParserHalCoreHandle->AudioIndex);
        if (type == NvMMStreamType_WAV) {
            /*  MAX_OUT_BUFFER_SIZE for wav dec is 8192.
             *  Since WavDec we will only copy as much from the input as the output can handle
             *  which means if there is more than 8K in the input, it is thrown
             *  away. This can cause garbage to playback if avi sends >8192
             *  Since wav dec cannot handle input buffer sizes >8192, So LIMITING FROM PARSER TO 8192.
             */
            pBufReq->maxBufferSize = 8192;
            pBufReq->minBufferSize = 8192;
        } else if (type == NvMMStreamType_MP2) {
            pBufReq->maxBufferSize = MAX_MP2_INPUT_BUFFER_SIZE;
            pBufReq->minBufferSize = MIN_MP2_INPUT_BUFFER_SIZE;
        } else {
            pBufReq->maxBufferSize = NVMM_COMMON_MAX_INPUT_BUFFER_SIZE;
            pBufReq->minBufferSize = NVMM_COMMON_MIN_INPUT_BUFFER_SIZE;
        }
    } else if (NVMM_ISSTREAMVIDEO (type)) {
        pNvAviParserHalCoreHandle->bHasVideoTrack = true;
        pNvAviParserHalCoreHandle->VideoIndex = streamIndex;
        ALOGV("AviCoreParserGetBufferRequirements : STREAMVIDEIO Type = %d, pNvAviParserHalCoreHandle->VideoIndex = %d", type, streamIndex);
        if ( (pContext->Parser.AllTrackInfo[streamIndex].BmpWidth == 0) ||
             (pContext->Parser.AllTrackInfo[streamIndex].BmpHeight == 0)) {
            pBufReq->minBufferSize = 32768;
            pBufReq->maxBufferSize = 512000 * 2 - 32768;
        } else {
            if ( (pContext->Parser.AllTrackInfo[streamIndex].BmpWidth > 320) &&
                 (pContext->Parser.AllTrackInfo[streamIndex].BmpHeight > 240)) {
                pBufReq->minBufferSize =
                    (pContext->Parser.AllTrackInfo[streamIndex].BmpWidth * pContext->Parser.AllTrackInfo[streamIndex].BmpHeight * 3) >> 2;
                pBufReq->maxBufferSize =
                    (pContext->Parser.AllTrackInfo[streamIndex].BmpWidth * pContext->Parser.AllTrackInfo[streamIndex].BmpHeight * 3) >> 2;
            } else {
                pBufReq->minBufferSize =
                    (pContext->Parser.AllTrackInfo[streamIndex].BmpWidth * pContext->Parser.AllTrackInfo[streamIndex].BmpHeight * 3) >> 1;
                pBufReq->maxBufferSize =
                    (pContext->Parser.AllTrackInfo[streamIndex].BmpWidth * pContext->Parser.AllTrackInfo[streamIndex].BmpHeight * 3) >> 1;
            }
        }
        dwSuggestedBufSz = pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pAviCoreParserGetdVideowSuggestedBufferSize(&pContext->Parser);

        if (dwSuggestedBufSz >  pBufReq->maxBufferSize) {
            pBufReq->maxBufferSize = dwSuggestedBufSz;
        }
    } else {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

cleanup:
    if (Status != NvSuccess) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/*****************************************************************************/
static status_t NvAviParserHal_CreateStream(
            NvAviParserHalCore* pNvAviParserHalCoreHandle,
            int32_t streamIndex,
            int32_t parserStreamIndex) {

    NvAviParserHalStream* pStream = NULL;
    status_t ret = OK;
    uint32_t i,size;
    NvMMBuffer* pBuffer = NULL;

    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);

    // If the stream already exist, delete it.
    if (pNvAviParserHalCoreHandle->pStreams[streamIndex]) {
        NvAviParserHal_DestroyStream(pNvAviParserHalCoreHandle, streamIndex);
    }

    pStream = (NvAviParserHalStream*)NvOsAlloc(sizeof(NvAviParserHalStream));
    if (pStream == NULL) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return NO_MEMORY;
    }
    NvOsMemset(pStream,0,sizeof(NvAviParserHalStream));

    pStream->streamIndex = streamIndex;
    pStream->parserStreamIndex = parserStreamIndex;

    NvAviParserHal_CalculateBufRequirements(pNvAviParserHalCoreHandle,
                                            pStream, parserStreamIndex);

    pStream->pBuffer = (NvMMBuffer*) NvOsAlloc(sizeof(NvMMBuffer));
    if (pStream->pBuffer == NULL) {
        ret = NO_MEMORY;
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        goto cleanup;
    }
    NvOsMemset(pStream->pBuffer, 0, sizeof(NvMMBuffer));
    pStream->pBuffer->StructSize = sizeof(NvMMBuffer);
    pStream->pBuffer->Payload.Ref.PhyAddress = NV_RM_INVALID_PHYS_ADDRESS;

    pNvAviParserHalCoreHandle->pStreams[streamIndex] = pStream;
    pNvAviParserHalCoreHandle->streamCount++;

cleanup:
    if (ret != OK) {
        if (pStream->pBuffer != NULL) {
            NvOsFree(pStream->pBuffer);
        }
        NvOsFree(pStream);
        pNvAviParserHalCoreHandle->pStreams[streamIndex] = NULL;
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return ret;
}

/*****************************************************************************/
static status_t NvAviParserHal_DestroyStream(
            NvAviParserHalCore* pNvAviParserHalCoreHandle,
            int32_t streamIndex) {

    NvAviParserHalStream* pStream =
        pNvAviParserHalCoreHandle->pStreams[streamIndex];

    ALOGV("+++ %s +++",__FUNCTION__);

    if (pStream) {
        if (pStream->pBuffer != NULL) {
            NvOsFree(pStream->pBuffer);
            pStream->pBuffer = NULL;
        }
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/*****************************************************************************/
static status_t NvAviParserHal_SetTrackMimeType(
            void* aviParserHalHandle,
            int32_t StreamIndex,
            sp<MetaData> metaData) {

    ALOGV("+++ %s +++",__FUNCTION__);
    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;

    switch(pNvAviParserHalCoreHandle->pCoreContext->Parser.AllTrackInfo[StreamIndex].ContentType) {
        case NvMMStreamType_MP3:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_MPEG);
            break;
        case NvMMStreamType_OGG:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_VORBIS);
            break;
        case NvMMStreamType_WAV:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_RAW);
            break;
        case NvMMStreamType_U_LAW:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_G711_MLAW);
            break;
        case NvMMStreamType_A_LAW:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_G711_ALAW);
            break;
        case NvMMStreamType_AAC:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_AAC);
            break;
        case NvMMStreamType_AC3:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_AC3);
            break;
        case NvMMStreamType_MP2:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_MPEG);
            break;
        case NvMMStreamType_MPEG4:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_MPEG4);
            break;
        case NvMMStreamType_MJPEG:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_MJPEG);
            break;
        case NvMMStreamType_H264:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_AVC);
            break;
        case NvMMStreamType_H263:
            // setting to Mpeg 4 type to open up mpeg4 decoder block that handles H263.
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_MPEG4);
            break;
        case NvMMStreamType_MPEG2V:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_MPEG2);
            break;
        case NvMMStreamType_ADPCM:
            ALOGV("--- %s[%d] ---, ADPCM not supported",__FUNCTION__,__LINE__);
        default:
            metaData->setCString(kKeyMIMEType,"application/error");
            ALOGE("===== %s[%d] Error =====, ContentType = %d",
                 __FUNCTION__, __LINE__,
                 pNvAviParserHalCoreHandle->pCoreContext->Parser.AllTrackInfo[StreamIndex].ContentType);
            pNvAviParserHalCoreHandle->pCoreContext->Parser.AllTrackInfo[StreamIndex].ContentType =
             NvMMStreamType_OTHER;
            return BAD_VALUE;
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/*****************************************************************************/
static uint64_t NvAviParserHal_GetDuration(
                            NvAviParserHalCore* pNvAviParserHalCoreHandle) {
    ALOGV("+++ %s +++",__FUNCTION__);
    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return pNvAviParserHalCoreHandle->pCoreContext->Parser.MediaTime/10;
}

/*****************************************************************************/
static NvError NvAviParserHal_ReadAVPacket(
            NvAviParserHalCore* pNvAviParserHalCoreHandle,
            int32_t StreamIndex, NvMMBuffer* pBuffer) {

    NvError Status = NvSuccess;
    NvMMAviParserCoreContext  *pContext = NULL;
    NvAviParserHalStream* pStream = NULL;
    NvMMStreamType StreamType;
    NvU32 size = 0;
    NvS32 rate = 0;

    ALOGV("+++ %s +++",__FUNCTION__);
    if ((pNvAviParserHalCoreHandle == NULL) || (pBuffer == NULL)) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return NvError_BadParameter;
    }

    pContext = pNvAviParserHalCoreHandle->pCoreContext;
    StreamType = pContext->Parser.AllTrackInfo[StreamIndex].ContentType;
    pStream = pNvAviParserHalCoreHandle->pStreams[StreamIndex];

    pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    pBuffer->Payload.Ref.startOfValidData = 0;
    pBuffer->PayloadInfo.TimeStamp = 0;
    pBuffer->PayloadInfo.BufferFlags = 0;

    rate = pContext->rate.Rate;

    switch (pContext->ParseState[StreamIndex]) {
        case AVIPARSE_DATA:
            Status =  pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserNextWorkUnit (
                          &pContext->Parser,
                          StreamIndex,
                          pBuffer,
                          &size,
                          rate,
                          NV_FALSE);
            break;
        case AVIPARSE_HEADER:
            Status = NvAviParserHal_InitializeHeaders (
                         pContext,
                         StreamIndex,
                         pBuffer,
                         StreamType,
                         &size);
            pContext->ParseState[StreamIndex] = AVIPARSE_DATA;
            break;
        default:
            break;
    }

    if (Status == NvError_ParserEndOfStream) {
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
        pBuffer->Payload.Ref.startOfValidData = 0;
        pBuffer->PayloadInfo.TimeStamp = 0;
        pBuffer->PayloadInfo.BufferFlags = 0;
        pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_EndOfStream;
        ALOGV("--- %s[%d] ---, Sending EOS",__FUNCTION__,__LINE__);
        return Status;
    }

    if (Status != NvSuccess) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return Status;
    }

    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return Status;
}

/*************************************************************************************/
status_t NvAviParserHal_SetPosition(
            void* aviParserHalHandle,
            int32_t streamIndex,
            uint64_t seek_position,
            MediaSource::ReadOptions::SeekMode mode) {

    NvAviParserHalCore* pNvAviParserHalCoreHandle =
        (NvAviParserHalCore*)aviParserHalHandle;
    NvMMAviParserCoreContext *pContext = NULL;
    NvError Status = NvSuccess;
    status_t ret = OK;
    NvU32 i;
    ALOGV("+++ %s +++, seek_position = %llu",__FUNCTION__, seek_position);

    NvU32 frameCounter = 0, currentFrame = 0;
    NvS32 direction = 1000, rate = 1000;
    NvU32 totalPackets = 0, desiredPacketToSeek = 0;
    NvU64 NewSeekTimeStamp = seek_position * 10;
    NvU64  currentTs = 0;
    NvAviFramingInfo *framingInfo;

    CHECK_BUFFER_VALIDITY(pNvAviParserHalCoreHandle);

    if ((pNvAviParserHalCoreHandle->bHasSupportedVideo) &&
       (streamIndex == pNvAviParserHalCoreHandle->AudioIndex)) {
           //Seek on video track for aud+vid files.
           ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
           return OK;
    }

    pContext = pNvAviParserHalCoreHandle->pCoreContext;

    //We are going to seek for audio only files. Does not metter if we had reached EOF.
    pContext->Parser.bFileReachedEOF = NV_FALSE;

    if (NewSeekTimeStamp == 0) {
        pContext->Parser.FramingInfo[pContext->Parser.VideoId].FrameCounter = 0;
        pContext->Parser.FramingInfo[pContext->Parser.AudioId].FrameCounter = 0;
        pContext->Parser.FramingInfo[pContext->Parser.VideoId].IsTableValueToRead = 0;
        pContext->Parser.FramingInfo[pContext->Parser.AudioId].IsTableValueToRead = 0;
        // Need to reset the below flag which will be required for repeat mode
        pContext->Parser.bFileReachedEOF = NV_FALSE;
        pContext->Parser.CurrentTimeStamp = 0;
        pContext->Parser.TotalPacketNums = 0;
        pContext->Parser.AudioSeekSet = 1;
        //pDemux->videoFramingInfo.TruncatedIdx1 = 0;
        //pDemux->audioFramingInfo.TruncatedIdx1 = 0;
        pContext->Position.Position = NewSeekTimeStamp;
        for (i = 0; i < pContext->Parser.AviHeader.dwStreams; i++) {
           if ((NVMM_ISSTREAMVIDEO (pContext->Parser.AllTrackInfo[i].ContentType) &&
               (pContext->Parser.AllTrackInfo[i].ContentType != NvMMStreamType_UnsupportedVideo)) ||
                (NVMM_ISSTREAMAUDIO (pContext->Parser.AllTrackInfo[i].ContentType))) {
                pContext->ParseState[i] = AVIPARSE_HEADER;
                ALOGV("AviPaser:pTimeStamp == 0: Reset ParseState AVIPARSE_HEADER");
            } else {
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            }
        }
        ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
        Status = NvSuccess;
        goto cleanup;
    }
    if ( NewSeekTimeStamp >= pContext->Parser.MediaTime) {
        ALOGV("AviCoreParserSetPosition: ReqTime GT MedTime");
        NewSeekTimeStamp = pContext->Parser.MediaTime;
        pContext->Parser.FramingInfo[pContext->Parser.VideoId].FrameCounter =
            pContext->Parser.FramingInfo[pContext->Parser.VideoId].TotalNoFrames;
        pContext->Parser.FramingInfo[pContext->Parser.AudioId].FrameCounter =
            pContext->Parser.FramingInfo[pContext->Parser.AudioId].TotalNoFrames;
        pContext->Parser.CurrentTimeStamp = pContext->Parser.MediaTime;
        pContext->Position.Position = NewSeekTimeStamp;
        // Set the below flag saying file reached EOF
        pContext->Parser.bFileReachedEOF = NV_TRUE;
        ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
        Status = NvSuccess;
        goto cleanup;
    }

    framingInfo = pContext->Parser.FramingInfo;
    i = pNvAviParserHalCoreHandle->VideoIndex;
    //StreamType = pContext->Parser.AllTrackInfo[i].ContentType;
    pContext->Position.Position = NewSeekTimeStamp;
    if ((pNvAviParserHalCoreHandle->bHasVideoTrack) &&
        NVMM_ISSTREAMVIDEO (pContext->Parser.AllTrackInfo[i].ContentType) &&
        (pContext->Parser.AllTrackInfo[i].ContentType!= NvMMStreamType_UnsupportedVideo)) {
        if (pContext->Parser.AviHasIndex ||
            pContext->Parser.AviMustUseIndex ||
            pContext->Parser.Odmlflag) {
            rate = pContext->rate.Rate;
            currentTs = pContext->Parser.CurrentTimeStamp;
            ALOGV("AviCoreParserSetPosition: BeforeSYNCUnit: CurrentTSMsec = %llu", currentTs / 10000);
            ALOGV("AviCoreParserSetPosition: BeforeSYNCUnit: RequestTSMsec = %llu", NewSeekTimeStamp / 10000);
            direction = 1000;
            if (NewSeekTimeStamp < currentTs) {
                direction = -1000;
            }

            if (mode == MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC) {
                direction = -1000;
                ALOGV("%s[%d], Forcing backward seek",__FUNCTION__,__LINE__);
            }

            frameCounter = pContext->Parser.FramingInfo[i].FrameCounter;
            ALOGV("AviCoreParserSetPosition: CurrentVFC = %d - rate = %d - dir = %d", frameCounter, rate, direction);
            if (pContext->Parser.AviStrmHdr[i].dwScale) {
                frameCounter = (NvU32) ((NewSeekTimeStamp * pContext->Parser.AviStrmHdr[i].dwRate) /
                (10 * (NvU64)pContext->Parser.AviStrmHdr[i].dwScale * TICKS_PER_SECOND));
            }

            ALOGV("AviCoreParserSetPosition: RequestedVFC = %d - rate = %d - dir = %d", frameCounter, rate, direction);
            if (rate > 0 && rate != 1000) {
                direction = 1000;
            } else if (rate < 0) {
                //seek to previous I frame
                direction = -1000;
            }
            if (framingInfo[i].TruncatedIdx1) {
                if ( (frameCounter > framingInfo[i].MaxIdx1Frames) && (rate > 0)) {
                    Status = NvError_ParserFailure;
                } else {
                    Status = pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pAviCoreParserGetNextSyncUnit (&pContext->Parser, &frameCounter , rate, direction, i);
                }
            } else {
                Status = pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pAviCoreParserGetNextSyncUnit (&pContext->Parser, &frameCounter , rate,
                    direction, i);
            }

            ALOGV("AviCoreParserSetPosition: UpdatedVFC = %d - rate = %d - dir = %d", frameCounter, rate, direction);
            if ( (Status != NvSuccess) || (frameCounter >= pContext->Parser.FramingInfo[i].TotalNoFrames)) {
                ALOGV("AviCoreParserSetPosition: AviParserGetNextSyncUnit Status = %x", Status);
                NewSeekTimeStamp = pContext->Parser.MediaTime;
                pContext->Parser.CurrentTimeStamp = pContext->Parser.MediaTime;
                pContext->Parser.FramingInfo[pContext->Parser.VideoId].FrameCounter =
                    pContext->Parser.FramingInfo[pContext->Parser.VideoId].TotalNoFrames;
                pContext->Parser.FramingInfo[pContext->Parser.AudioId].FrameCounter =
                    pContext->Parser.FramingInfo[pContext->Parser.AudioId].TotalNoFrames;
                pContext->Parser.TotalPacketNums =
                    pContext->Parser.AllTrackInfo[pContext->Parser.AudioId].TotalAudioLength;
                pContext->Position.Position = NewSeekTimeStamp;
                // Set the below flag saying file reached EOF
                pContext->Parser.bFileReachedEOF = NV_TRUE;
                Status = NvSuccess;
                ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
                goto cleanup;
            }
            pContext->Parser.FramingInfo[i].FrameCounter = frameCounter;
            if (pContext->Parser.AviStrmHdr[i].dwRate) {
                NewSeekTimeStamp =
                    (NvU64) (10 * (NvU64)pContext->Parser.FramingInfo[i].FrameCounter * (((NvU64)pContext->Parser.AviStrmHdr[i].dwScale * TICKS_PER_SECOND) /
                    pContext->Parser.AviStrmHdr[i].dwRate));
            }
        } else {
            ALOGV("AviCoreParserSetPosition SeekUnSupported: No Index To Seek");
            NewSeekTimeStamp = pContext->Parser.MediaTime;
            pContext->Parser.FramingInfo[pContext->Parser.VideoId].FrameCounter =
                pContext->Parser.FramingInfo[pContext->Parser.VideoId].TotalNoFrames;
            pContext->Parser.FramingInfo[pContext->Parser.AudioId].FrameCounter =
                pContext->Parser.FramingInfo[pContext->Parser.AudioId].TotalNoFrames;
            pContext->Parser.CurrentTimeStamp = pContext->Parser.MediaTime;;
            pContext->Parser.TotalPacketNums =
                pContext->Parser.AllTrackInfo[pContext->Parser.AudioId].TotalAudioLength;
            Status = NvError_ParserSeekUnSupported;
            ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
            goto cleanup;
        }
    }

    pContext->Position.Position = NewSeekTimeStamp;
    pContext->Parser.CurrentTimeStamp = NewSeekTimeStamp;
    ALOGV("AviCoreParserSetPosition: UpdatedSeekTimeMsec - %llu --- TotalMediaTimeMsec - %llu",
          NewSeekTimeStamp / 10000, pContext->Parser.MediaTime / 10000);

    if (pNvAviParserHalCoreHandle->bHasAudioTrack) {
        i = pNvAviParserHalCoreHandle->AudioIndex;
        //StreamType = pContext->Parser.AllTrackInfo[i].ContentType;
        if ((NVMM_ISSTREAMAUDIO (pContext->Parser.AllTrackInfo[i].ContentType)) &&
            (pContext->Parser.AllTrackInfo[i].ContentType != NvMMStreamType_UnsupportedAudio)) {
            if (pContext->Parser.AviHasIndex || pContext->Parser.AviMustUseIndex || pContext->Parser.Odmlflag) {
                ALOGV("AviCoreParserSetPosition: AFC=%d, APC =%d",
                     pContext->Parser.FramingInfo[pContext->Parser.AudioId].FrameCounter,
                     pContext->Parser.TotalPacketNums);
                desiredPacketToSeek =
                    (NvU32) ( (NewSeekTimeStamp * (NvU64) pContext->Parser.AviStrmHdr[i].dwRate) /
                    (10 * (NvU64) pContext->Parser.AviStrmHdr[i].dwScale * TICKS_PER_SECOND));
                ALOGV("AviCoreParserSetPosition: desiredPacketToSeek =%d", desiredPacketToSeek);
                Status = pNvAviParserHalCoreHandle->pNvAviCoreParserApi.pNvAviCoreParserSeekToAudioFrame (&pContext->Parser,
                                                      desiredPacketToSeek,
                                                      i, &currentFrame, &totalPackets);
                pContext->Parser.bSeekDone = NV_TRUE;
                if (pContext->Parser.AllTrackInfo[i].ContentType == NvMMStreamType_MP3) {
                    pContext->Parser.AudioSeekSet = 1;
                    pContext->Parser.partialFrameLength = 0;
                }
                if ( (Status != NvSuccess) ||
                     (currentFrame >= pContext->Parser.FramingInfo[pContext->Parser.AudioId].TotalNoFrames)) {
                    if (Status == NvError_ParserEndOfStream) {
                        pContext->Parser.FramingInfo[i].FrameCounter =
                            pContext->Parser.FramingInfo[i].TotalNoFrames;
                        ALOGV("AviCoreParserSetPosition: NvAviParserSeekToAudioFrame EOS");
                        pContext->Parser.TotalPacketNums =
                            pContext->Parser.AllTrackInfo[i].TotalAudioLength;
                        // Set the below flag saying file reached EOF
                        pContext->Parser.bFileReachedEOF = NV_TRUE;
                        Status = NvSuccess;
                        ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
                        goto cleanup;
                    } else {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        goto cleanup;
                    }
                }
                pContext->Parser.TotalPacketNums = totalPackets;
                pContext->Parser.FramingInfo[i].FrameCounter = currentFrame;
            } else {
                ALOGV("AviCoreParserSetPosition SeekUnSupported: No Index To Seek");
                NewSeekTimeStamp = pContext->Parser.MediaTime;
                pContext->Parser.FramingInfo[i].FrameCounter =
                    pContext->Parser.FramingInfo[i].TotalNoFrames;
                pContext->Parser.TotalPacketNums =
                    pContext->Parser.AllTrackInfo[i].TotalAudioLength;
                Status = NvError_ParserSeekUnSupported;
                ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                goto cleanup;
            }
        }
    }


cleanup:
#if (NV_DEBUG_AVI_PARSER)
    NV_LOGGER_PRINT((NVLOG_AVI_PARSER, NVLOG_VERBOSE, "AviCoreParserSetPosition: Updated AFC=%d, APC =%d\n", pContext->Parser.FramingInfo[pContext->Parser.AudioId].FrameCounter, pContext->Parser.TotalPacketNums));
    pContext->Parser.SeekAudioFlag = NV_TRUE;
    pContext->Parser.SeekVideoFlag = NV_TRUE;
#endif

    if (Status != NvSuccess) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return UNKNOWN_ERROR;
    }

    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return OK;
}

/*****************************************************************************/
//TBD: enhance this for cases where esds is more than 2 bytes!
static NvU16 CalcESDS(NvMMAudioAacTrackInfo *paacTrackInfo) {
    NvU16 data = 0;
    ALOGV("+++ %s +++",__FUNCTION__);
    // AudioSpecificInfo follows
    // oooo offf fccc c000
    // o - audioObjectType
    // f - samplingFreqIndex
    // c - channelConfig
    if (NULL == paacTrackInfo) {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
        return data;
    }

    data |= paacTrackInfo->channelConfiguration<<3;
    data |= paacTrackInfo->samplingFreqIndex<<7;
    data |= paacTrackInfo->objectType<<11;
    data = (data>>8)|(data<<8);
    ALOGV("--- %s[%d] ---, data = 0x%x",__FUNCTION__,__LINE__,data);
    return data;
}

/*****************************************************************************/
static NvError NvAviParserHal_InitializeHeaders (
            NvMMAviParserCoreContext *pContext,
            NvU32 streamIndex,
            NvMMBuffer* pBuffer,
            NvMMStreamType StreamType,
            NvU32 *size) {

    NvError Status = NvSuccess;

    NvMMWaveStreamProperty  wavTrackInfo;
    NvMMPayloadMetadata payload;
    CP_PIPETYPE_EXTENDED *pPipe;
    NvU32 i = 0;
    NvU16 data = 0;
    NvMMAudioAacTrackInfo aacTrackInfo;

    ALOGV("+++ %s +++, StreamType = %d",__FUNCTION__,StreamType);

    *size = 0;
    pPipe = pContext->Parser.pPipe;

    if (NVMM_ISSTREAMVIDEO (StreamType)) {
        if (StreamType == NvMMStreamType_MPEG4) {
            if (!pContext->Parser.IsVideoHeader) {
                pContext->Parser.Mpeg4HeadSize = pContext->Parser.VolHeaderSize;
                Status = (NvError)pPipe->SetPosition64 (pContext->Parser.hContentPipe,
                                                        (CPint64) pContext->Parser.VolStartCodeOffset,
                                                        CP_OriginBegin);
                if (Status != NvSuccess) {
                    if (Status == NvError_FileOperationFailed) {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        return  NvError_FileOperationFailed;
                    } else if (Status == NvError_EndOfFile) {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        return NvError_EndOfFile;
                    } else {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        return NvError_ParserReadFailure;
                    }
                }
                if (pPipe->cpipe.Read (pContext->Parser.hContentPipe,
                                       (CPbyte *) pBuffer->Payload.Ref.pMem,
                                       pContext->Parser.Mpeg4HeadSize) != NvSuccess) {
                    ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                    return NvError_ParserReadFailure;
                }
                pBuffer->Payload.Ref.sizeOfValidDataInBytes = pContext->Parser.Mpeg4HeadSize;
                *size = pContext->Parser.Mpeg4HeadSize;
                pBuffer->Payload.Ref.startOfValidData = 0;
                pBuffer->PayloadInfo.TimeStamp = 0;
                pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
                pContext->Parser.IsVideoHeader = NV_TRUE;
                ALOGV("Video MPEG4 --- %s ---",__FUNCTION__);
                return NvSuccess;
            }
        }
    } else if (NVMM_ISSTREAMAUDIO (StreamType)) {
        ALOGV("NvInitializeHeaders NVMM_ISSTREAMAUDIO StreamType = %d", StreamType);
        if (StreamType == NvMMStreamType_AAC) {
            if (!pContext->Parser.IsAudioHeader) {
                NvOsMemset (&aacTrackInfo, 0, sizeof (NvMMAudioAacTrackInfo));
                aacTrackInfo.objectType = 2;
                if (pContext->Parser.IsAdtsHeader) {
                    aacTrackInfo.profile = pContext->Parser.ADTSHeader.profile;
                    aacTrackInfo.samplingFreqIndex = pContext->Parser.ADTSHeader.sampling_freq_idx;
                    aacTrackInfo.samplingFreq =
                        Avi_AacSamplingRateTable[pContext->Parser.ADTSHeader.sampling_freq_idx];
                    aacTrackInfo.noOfChannels =
                        pContext->Parser.ADTSHeader.channel_config;
                    aacTrackInfo.sampleSize =
                        pContext->Parser.AllTrackInfo[streamIndex].SampleSize;
                    aacTrackInfo.channelConfiguration =
                        pContext->Parser.ADTSHeader.channel_config;
                    aacTrackInfo.bitRate =
                        pContext->Parser.AllTrackInfo[streamIndex].BitRate;
                } else {
                    aacTrackInfo.profile = 0x40;
                    aacTrackInfo.samplingFreq =
                        pContext->Parser.AllTrackInfo[streamIndex].SamplingFrequency;
                    for (i = 0; i < 16; i++) {
                        if (Avi_AacSamplingRateTable[i] == (NvS32) aacTrackInfo.samplingFreq) {
                            aacTrackInfo.samplingFreqIndex = i;
                            break;
                        }
                    }
                    aacTrackInfo.noOfChannels =
                        pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels;
                    aacTrackInfo.sampleSize =
                        pContext->Parser.AllTrackInfo[streamIndex].SampleSize;
                    aacTrackInfo.channelConfiguration =
                        pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels; //pContext->Parser.pH.channel_config;
                    aacTrackInfo.bitRate =
                        pContext->Parser.AllTrackInfo[streamIndex].BitRate;
                }

                NvOsMemcpy (pBuffer->Payload.Ref.pMem, (NvU8*) (&aacTrackInfo), sizeof (NvMMAudioAacTrackInfo));

                pBuffer->Payload.Ref.sizeOfValidDataInBytes = sizeof (NvMMAudioAacTrackInfo);
                *size = sizeof (NvMMAudioAacTrackInfo); // Update the size with actual max frame bytes read for this work unit..

                if (pContext->Parser.AllTrackInfo[streamIndex].CbSize) {
                    NvOsMemcpy (pBuffer->Payload.Ref.pMem,
                                (NvU8*) pContext->Parser.AllTrackInfo[streamIndex].pCodecSpecificData,
                                pContext->Parser.AllTrackInfo[streamIndex].CbSize);
                    *size =   pContext->Parser.AllTrackInfo[streamIndex].CbSize;
                    pBuffer->Payload.Ref.sizeOfValidDataInBytes =
                        pContext->Parser.AllTrackInfo[streamIndex].CbSize;
                    ALOGV("Audio AAC non ESDS --- %s ---, size = %d",
                         __FUNCTION__,pContext->Parser.AllTrackInfo[streamIndex].CbSize);
                } else {
                    //Now we know that handlefirstpacketAAC expects ESDS, not aacTrackInfo
                    data = CalcESDS(&aacTrackInfo);
                    ALOGV("Audio AAC ESDS --- %s ---,data = 0x%x",__FUNCTION__,data);
                    NvOsMemcpy (pBuffer->Payload.Ref.pMem, (NvU8*) &data, 2);
                    *size =   2;
                    pBuffer->Payload.Ref.sizeOfValidDataInBytes = 2;
                }

                pBuffer->Payload.Ref.startOfValidData = 0;
                pBuffer->PayloadInfo.TimeStamp = 0;
                pContext->Parser.IsAudioHeader = NV_TRUE;
                pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
                ALOGV("AAC header detected --- %s ---",__FUNCTION__);
                return NvSuccess;
            }
        } else if ( (StreamType == NvMMStreamType_WAV) || (StreamType == NvMMStreamType_AC3)) {
            if (StreamType == NvMMStreamType_WAV) {
                ALOGV("Audio is WAV --- %s ---",__FUNCTION__);
            } else {
                ALOGV("Audio is AC3 --- %s ---",__FUNCTION__);
            }

            if (!pContext->Parser.IsAudioHeader) {
                NvOsMemset (&wavTrackInfo, 0, sizeof (NvMMWaveStreamProperty));
                wavTrackInfo.SampleRate = pContext->Parser.AllTrackInfo[streamIndex].SamplingFrequency;
                wavTrackInfo.Channels = pContext->Parser.AllTrackInfo[streamIndex].NoOfChannels;
                wavTrackInfo.BitsPerSample = pContext->Parser.AllTrackInfo[streamIndex].SampleSize;
                wavTrackInfo.BlockAlign  = pContext->Parser.AllTrackInfo[streamIndex].NblockAlign;
                wavTrackInfo.samplesPerBlock = pContext->Parser.AllTrackInfo[streamIndex].SamplesPerBlock;
                wavTrackInfo.nCoefs = pContext->Parser.AllTrackInfo[streamIndex].Ncoefs;
                wavTrackInfo.uDataLength = pContext->Parser.AllTrackInfo[streamIndex].TotalAudioLength;
                wavTrackInfo.audioFormat = pContext->Parser.WaveHdr.wFormatTag;

                NvOsMemcpy (pBuffer->Payload.Ref.pMem, (NvU8*) (&wavTrackInfo), sizeof (NvMMWaveStreamProperty));
                pBuffer->Payload.Ref.sizeOfValidDataInBytes = sizeof (NvMMWaveStreamProperty);
                *size = sizeof (NvMMWaveStreamProperty); // Update the size with actual max frame bytes read for this work unit..
                pBuffer->Payload.Ref.startOfValidData = 0;
                pBuffer->PayloadInfo.TimeStamp = 0;
                pContext->Parser.IsAudioHeader = NV_TRUE;
                pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
                ALOGV("WAV/AC3 header detected --- %s ---",__FUNCTION__);
                return NvSuccess;
            }
        } else if (StreamType == NvMMStreamType_OGG) {
            // Enable this to write header along with frame off sets info as .dat file. This is required to test Ogg Pal;yback in WMP
            if (!pContext->Parser.IsAudioHeader) {
                // Enable this to play the ogg file in WMP or other PC palyer like totalvideo player etc.,
                Status = (NvError)pContext->Parser.pPipe->SetPosition64 (pContext->Parser.hContentPipe,
                                                                         (CPint64) pContext->Parser.OggObjectOffset,
                                                                         CP_OriginBegin);
                if (Status != NvSuccess) {
                    if (Status == NvError_FileOperationFailed) {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        return  NvError_FileOperationFailed;
                    } else if (Status == NvError_EndOfFile) {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        return NvError_EndOfFile;
                    } else {
                        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                        return NvError_ParserReadFailure;
                    }
                }
                if (pContext->Parser.pPipe->cpipe.Read (pContext->Parser.hContentPipe,
                                                        (CPbyte *) pBuffer->Payload.Ref.pMem,
                                                        pContext->Parser.AllTrackInfo[streamIndex].StreamObjectSize) != NvSuccess) {
                    ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
                    return NvError_ParserReadFailure;
                }

                pBuffer->Payload.Ref.sizeOfValidDataInBytes = pContext->Parser.AllTrackInfo[streamIndex].StreamObjectSize;
                *size =  pContext->Parser.AllTrackInfo[streamIndex].StreamObjectSize;
                pContext->Parser.IsAudioHeader = NV_TRUE;
                ALOGV("OGG header detected --- %s ---",__FUNCTION__);
            }
        }
    } else {
        ALOGE("===== %s[%d] Error =====",__FUNCTION__, __LINE__);
    }
    ALOGV("--- %s[%d] ---",__FUNCTION__,__LINE__);
    return Status;
}

#ifdef __cplusplus
}
#endif

} // namespace android

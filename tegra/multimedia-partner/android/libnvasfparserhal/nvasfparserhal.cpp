/* Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "NvASFParserHal"

#include "nvasfparserhal.h"
#include "nvparserhal_utils.h"
#include <dlfcn.h>

/*************************************************************************************
* Macros
 *************************************************************************************/
#define MAX_NUM_BUFFERS 15
#define MIN_BUF_SIZE_VIDEO 32768
#define MAX_BUF_SIZE_VIDEO (512000 * 2 - 32768)
#define FREE_BUFFER(x)    \
        if (x != NULL) {\
        NvOsFree(x);\
        x = NULL;\
        }

namespace android {

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************
* Data structures
 *************************************************************************************/
typedef struct {
    CreateNvAsfParserFunc           pCreateNvAsfParser;
    DestroyNvAsfParserFunc          pDestroyNvAsfParser;
    NvAsfOpenFunc                   pNvAsfOpen;
    NvAsfVideoNewSeekTimeFunc       pNvAsfVideoNewSeekTime;
    NvAsfAudioNewSeekTimeFunc       pNvAsfAudioNewSeekTime;
    NvAsfParseFunc                  pNvAsfParse;
    NvAsfFillInDefaultCallbacksFunc pNvAsfFillInDefaultCallbacks;
    NvAsfFindLargestIframeFunc      pNvAsfFindLargestIframe;
}NvAsfCoreParserApi;

typedef struct {
    uint32_t                        coreStreamIndex;
    NvMMBuffer*                     pBuffer;
    NvParserHalBufferRequirements   bufReq;
}NvAsfParserHalStream;

typedef struct {
    bool                            bHasSupportedVideo;
    int32_t                         AudioIndex;
    uint32_t                        streamCount;
    uint64_t                        streamDuration;
    NvAsfParserHalStream            **pStreams;
    NvMMAsfParserCoreContext        *pCoreContext;
    NvAsfCoreParserApi              pNvAsfCoreParserApi;
    void*                           pNvAsfCoreParserLib;
}NvAsfParserHalCore;

/**************************************************************************************
* Function Declarations
 *************************************************************************************/
NvAsfResult AsfOnAsfParserGetData(
            void *data,
            NvU8* pBuffer,
            NvU32 uSize,
            NvU32* puRead);
NvAsfResult AsfOnAsfParserSetOffset(
            void *data,
            NvU64 uOffset);
NvAsfResult AsfOnAsfParserPayloadData(
            void *data,
            CAsfPayloadData* pPayloadData,
            NvU32* puRead,
            NvU64 OffsetinBytes,
            NvU8** pSrcBuffer,
            NvBool LastBuffer);
static status_t NvAsfParserHal_OpenCore(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle);
static status_t NvAsfParserHal_DestroyCore(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle);
static status_t NvAsfParserHal_CalculateBufReq(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                NvAsfParserHalStream* pStream,
                int32_t StreamIndex);
static status_t NvAsfParserHal_CreateStream(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                int32_t streamIndex);
static status_t NvAsfParserHal_DestroyStream(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                int32_t StreamIndex);
static NvError ReadAVPacket(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                int32_t StreamIndex,
                NvMMBuffer *pBuffer);
static status_t NvAsfParserHal_SetPosition(
                void* asfParserHalHandle,
                int32_t streamIndex,
                uint64_t seek_position,
                MediaSource::ReadOptions::SeekMode mode);
static uint64_t NvAsfParserHal_GetDuration(
                NvAsfParserHalCore* pNvAsfParserHalCoreHandle);
static status_t NvAsfParserHal_SetTrackMimeType(
                void* asfParserHalHandle,
                int32_t coreStreamIndex,
                sp<MetaData> metaData);

/**************************************************************************************
* API Definitions
 *************************************************************************************/
/**************************************************************************************
* Function    : NvAsfParserHal_GetInstanceFunc
* Description : Returns handles to ASF HAL API's to the caller
 *************************************************************************************/
void NvAsfParserHal_GetInstanceFunc(NvParseHalImplementation *halImpl) {
    halImpl->nvParserHalCreateFunc              = &NvAsfParserHal_Create;
    halImpl->nvParserHalDestroyFunc             = &NvAsfParserHal_Destroy;
    halImpl->nvParserHalGetStreamCountFunc      = &NvAsfParserHal_GetStreamCount;
    halImpl->nvParserHalGetMetaDataFunc         = &NvAsfParserHal_GetMetaData;
    halImpl->nvParserHalGetTrackMetaDataFunc    = &NvAsfParserHal_GetTrackMetaData;
    halImpl->nvParserHalSetTrackHeaderFunc      = &NvAsfParserHal_SetTrackHeader;
    halImpl->nvParserHalReadFunc                = &NvAsfParserHal_Read;
}
/**************************************************************************************
* Function    : NvAsfParserHal_Create
* Description : Instantiates the ASF Parser hal and the core parser.
*               Parses the header objects
 *************************************************************************************/
status_t NvAsfParserHal_Create(
            char* Url,
            void **pAsfParserHalHandle) {

    NvU32 len = 0, streamIndex = 0;
    status_t ret = OK;
    NvAsfResult err = RESULT_OK;
    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = NULL;
    CAsf* asf = NULL;

    pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)NvOsAlloc(sizeof(NvAsfParserHalCore));
    if(pNvAsfParserHalCoreHandle == NULL) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle, 0, sizeof(NvAsfParserHalCore));

    // Clear the old errors, no need to handle return for clearing
    dlerror();
    // Get handle of core asf library
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib = dlopen("libnvmm_asfparser.so", RTLD_NOW);
    if(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib == NULL) {
        ALOGE("Function %s : Loading libnvmm_asfparser.so failed with error %s \n",__func__,dlerror());
        ret = UNKNOWN_ERROR;
        goto cleanup;
    }

    // Get interface hadles
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pCreateNvAsfParser =
        (CreateNvAsfParserFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"CreateNvAsfParser");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pDestroyNvAsfParser =
        (DestroyNvAsfParserFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"DestroyNvAsfParser");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfOpen =
        (NvAsfOpenFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"NvAsfOpen");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfVideoNewSeekTime =
        (NvAsfVideoNewSeekTimeFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"NvAsfVideoNewSeekTime");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfAudioNewSeekTime =
        (NvAsfAudioNewSeekTimeFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"NvAsfAudioNewSeekTime");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfParse =
        (NvAsfParseFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"NvAsfParse");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfFillInDefaultCallbacks =
        (NvAsfFillInDefaultCallbacksFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"NvAsfFillInDefaultCallbacks");
    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfFindLargestIframe =
        (NvAsfFindLargestIframeFunc) dlsym(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib,"NvAsfFindLargestIframe");

    pNvAsfParserHalCoreHandle->pCoreContext =
        (NvMMAsfParserCoreContext *)NvOsAlloc(sizeof(NvMMAsfParserCoreContext));
    if(pNvAsfParserHalCoreHandle->pCoreContext == NULL) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle->pCoreContext, 0, sizeof(NvMMAsfParserCoreContext));

    len = NvOsStrlen((const char*)Url);
    pNvAsfParserHalCoreHandle->pCoreContext->filename = (NvU8 *)NvOsAlloc((len+1)*sizeof(NvU8));
    if(!pNvAsfParserHalCoreHandle->pCoreContext->filename) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }

    // Copy the filename
    NvOsStrncpy((char *)pNvAsfParserHalCoreHandle->pCoreContext->filename, (const char*)Url, len+1);

    // Open the core parser
    ret = NvAsfParserHal_OpenCore(pNvAsfParserHalCoreHandle);
    if(ret != OK) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    asf = (CAsf* )pNvAsfParserHalCoreHandle->pCoreContext->asf;

    // Parse the header and other ASF objects
    err = pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfParse(pNvAsfParserHalCoreHandle->pCoreContext);
    if(err > RESULT_OK_BREAK) {
        // Header parsing failed
        ret = UNKNOWN_ERROR;
        ALOGE("Function %s line %d : Header parsing failed with error %x \n",__func__,__LINE__,err);
        goto cleanup;
    }

    // Check if the stream is valid
    if(asf->NoOfStreams <= 0) {
        ret = UNKNOWN_ERROR;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }

    pNvAsfParserHalCoreHandle->pStreams =
        (NvAsfParserHalStream**)NvOsAlloc(sizeof(NvAsfParserHalStream*) * asf->NoOfStreams);
    if(pNvAsfParserHalCoreHandle->pStreams == NULL) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle->pStreams, 0, sizeof(NvAsfParserHalStream*)*asf->NoOfStreams);

    // Initialize the audio stream index
    pNvAsfParserHalCoreHandle->AudioIndex = -1;

    for(streamIndex = 0; streamIndex < asf->NoOfStreams; streamIndex++) {
        if (((asf->m_oAudioStreamNumber >= 0) &&
             (asf->StreamNumberMapping[streamIndex] == asf->m_oAudioStreamNumber)) ||
            ((asf->m_oVideoStreamNumber >= 0) &&
             (asf->StreamNumberMapping[streamIndex] == asf->m_oVideoStreamNumber))) {

            ret = NvAsfParserHal_CreateStream(pNvAsfParserHalCoreHandle,streamIndex);
            if(ret != OK) {
                ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
                goto cleanup;
            }
        }
    }

    // Get the stream Duration
    pNvAsfParserHalCoreHandle->streamDuration =
        NvAsfParserHal_GetDuration(pNvAsfParserHalCoreHandle);

    // Max Packet size is known, we can now allocate buffer for parsing data packets
    asf->NvAsfParser->m_ScratchBuffSize =
        (asf->NvAsfParser->m_oFilePropertiesObject.uMaximumDataPacketSize) * NUMBER_OF_PACKETS;
    asf->NvAsfParser->m_ScratchBuff = (NvU8*)NvOsAlloc(sizeof(NvU8) * asf->NvAsfParser->m_ScratchBuffSize);
    if(!asf->NvAsfParser->m_ScratchBuff) {
        ret = NO_MEMORY;
        goto cleanup;
    }

    ALOGV("Function %s : Returning handle %p \n",__func__,pNvAsfParserHalCoreHandle);
    // Return the handle to caller
    *pAsfParserHalHandle = pNvAsfParserHalCoreHandle;
    return OK;

cleanup:
    if(pNvAsfParserHalCoreHandle) {
        if(pNvAsfParserHalCoreHandle->pCoreContext) {
            if(asf) {
                if(asf->NvAsfParser) {
                    FREE_BUFFER(asf->NvAsfParser->m_ScratchBuff);
                }
            }
            if(pNvAsfParserHalCoreHandle->pStreams) {
                for(streamIndex = 0; streamIndex < pNvAsfParserHalCoreHandle->streamCount; streamIndex++) {
                    if(pNvAsfParserHalCoreHandle->pStreams[streamIndex]) {
                        NvAsfParserHal_DestroyStream(pNvAsfParserHalCoreHandle,streamIndex);
                    }
                }
                FREE_BUFFER(pNvAsfParserHalCoreHandle->pStreams);
            }

            NvAsfParserHal_DestroyCore(pNvAsfParserHalCoreHandle);

            FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->filename);
            FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext);
        }
        if(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib) {
        // Close the asf core lib
            dlclose(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib);
        }
        FREE_BUFFER(pNvAsfParserHalCoreHandle);
    }
    *pAsfParserHalHandle = NULL;
    return ret;
}

/**************************************************************************************
* Function    : NvAsfParserHal_Destroy
* Description : Destroys the ASF Parser hal and the core parser.
*               Releases the resources
 *************************************************************************************/
status_t NvAsfParserHal_Destroy(
            void* asfParserHalHandle) {
    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;
    uint32_t i;

    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    if(pNvAsfParserHalCoreHandle->pCoreContext) {
        FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->asf->NvAsfParser->m_ScratchBuff);

        if(pNvAsfParserHalCoreHandle->pStreams) {
            for(i=0; i<pNvAsfParserHalCoreHandle->streamCount; i++) {
                if(pNvAsfParserHalCoreHandle->pStreams[i]) {
                    NvAsfParserHal_DestroyStream(pNvAsfParserHalCoreHandle,i);
                }
            }
            FREE_BUFFER(pNvAsfParserHalCoreHandle->pStreams);
        }

        NvAsfParserHal_DestroyCore(pNvAsfParserHalCoreHandle);
        FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->filename);
        FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext);
    }

    if(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib) {
    // Close the asf core lib
        dlclose(pNvAsfParserHalCoreHandle->pNvAsfCoreParserLib);
    }

    NvOsFree(pNvAsfParserHalCoreHandle);
    return OK;
}
/**************************************************************************************
* Function    : NvAsfParserHal_GetMetaData
* Description : Sends the meta data from a media file to caller
 *************************************************************************************/
status_t NvAsfParserHal_GetMetaData(
            void* asfParserHalHandle,
            sp<MetaData> mFileMetaData) {

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;
    CAsf* asf = NULL;
    size_t kNumMapEntries, j;
    uint32_t streamIndex;
    int32_t coreStreamIndex;
    NvMMMetaDataInfo *src = NULL;
    NvMMAsfMetaData *meta = NULL;
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

    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }
    if(!pNvAsfParserHalCoreHandle->pCoreContext->HeaderFinished) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)NO_INIT);
        return NO_INIT;
    }

    asf = (CAsf* )pNvAsfParserHalCoreHandle->pCoreContext->asf;
    // Set mime type of media
    for(streamIndex = 0; streamIndex< pNvAsfParserHalCoreHandle->streamCount; streamIndex++) {
        coreStreamIndex = pNvAsfParserHalCoreHandle->pStreams[streamIndex]->coreStreamIndex;
        if(NVMM_ISSTREAMVIDEO(asf->StreamInfo[coreStreamIndex].StreamType)) {
            mFileMetaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_CONTAINER_ASF);
            break;
        } else {
            NvAsfParserHal_SetTrackMimeType(pNvAsfParserHalCoreHandle, coreStreamIndex, mFileMetaData);
        }
    }

    meta = asf->AsfMetadata;
    kNumMapEntries = sizeof(kMap) / sizeof(kMap[0]);

    for(j=0;j<kNumMapEntries;j++) {
        switch(kMap[j].from) {
        case NvMMMetaDataInfo_TrackNumber:
            src = &(meta->oTrackNoMetaData);
            break;
        case NvMMMetaDataInfo_Album:
            src = &(meta->oAlbumMetaData);
            break;
        case NvMMMetaDataInfo_Artist:
            src = &(meta->oArtistMetaData);
            break;
        case NvMMMetaDataInfo_AlbumArtist:
            src = &(meta->oAlbumArtistMetaData);
            break;
        case NvMMMetaDataInfo_Composer:
            src = &(meta->oComposerMetaData);
            break;
        case NvMMMetaDataInfo_Genre:
            src = &(meta->oGenreMetaData);
            break;
        case NvMMMetaDataInfo_Title:
            src = &(meta->oTitleMetaData);
            break;
        case NvMMMetaDataInfo_Year:
            src = &(meta->oYearMetaData);
            break;
        case NvMMMetaDataInfo_CoverArt:
            src = &(meta->oCoverArt);
            break;
        default:
            break;
        }

        if(src == NULL) {
            continue;
        } else if((src->pClientBuffer == NULL) || (src->nBufferSize == 0)) {
            continue;
        }

        if(kMap[j].type == MetaData::TYPE_C_STRING) {
            if (src->eEncodeType == NvMMMetaDataEncode_NvU32) {
                char str32[11];
                sprintf(str32, "%u", *(NvU32 *)(src->pClientBuffer));
                mFileMetaData->setCString(kMap[j].to, str32);
            } else {
                String8 str8((const char16_t*)src->pClientBuffer);
                mFileMetaData->setCString(kMap[j].to, (const char *)str8.string());
            }
        } else {
            mFileMetaData->setData(kMap[j].to, kMap[j].type, src->pClientBuffer, src->nBufferSize);
        }
    }

    return OK;
}
/**************************************************************************************
* Function    : NvAsfParserHal_GetTrackMetaData
* Description : Sends the meta data from a specified media track to caller
 *************************************************************************************/
status_t NvAsfParserHal_GetTrackMetaData(
            void* asfParserHalHandle,
            int32_t streamIndex,
            bool setThumbnailTime,
            sp<MetaData> mTrackMetaData) {

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;
    CAsf* asf = NULL;
    int MaxInputSize = 0;
    NvAsfResult err = RESULT_OK;
    int32_t coreStreamIndex = 0;

    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }
    coreStreamIndex = pNvAsfParserHalCoreHandle->pStreams[streamIndex]->coreStreamIndex;
    asf = (CAsf* )pNvAsfParserHalCoreHandle->pCoreContext->asf;

    if(!setThumbnailTime) {
        NvAsfParserHal_SetTrackMimeType(pNvAsfParserHalCoreHandle, coreStreamIndex, mTrackMetaData);
        mTrackMetaData->setInt64(kKeyDuration, pNvAsfParserHalCoreHandle->streamDuration);
        mTrackMetaData->setInt32(kKeyMaxInputSize,
            pNvAsfParserHalCoreHandle->pStreams[streamIndex]->bufReq.maxBufferSize);
        if(NVMM_ISSTREAMVIDEO(asf->StreamInfo[coreStreamIndex].StreamType)) {
            uint32_t height, width;
            pNvAsfParserHalCoreHandle->bHasSupportedVideo = true;
            height = asf->StreamInfo[coreStreamIndex].NvMMStream_Props.VideoProps.Height;
            width = asf->StreamInfo[coreStreamIndex].NvMMStream_Props.VideoProps.Width;
            mTrackMetaData->setInt32(kKeyWidth, width);
            mTrackMetaData->setInt32(kKeyHeight, height);
            mTrackMetaData->setInt32(kKeyBitRate,
                asf->StreamInfo[coreStreamIndex].NvMMStream_Props.VideoProps.VideoBitRate);
            mTrackMetaData->setInt64(kKeyDuration, pNvAsfParserHalCoreHandle->streamDuration);
        } else if(NVMM_ISSTREAMAUDIO(asf->StreamInfo[coreStreamIndex].StreamType)) {
            mTrackMetaData->setInt32(kKeySampleRate,
                asf->StreamInfo[coreStreamIndex].NvMMStream_Props.AudioProps.SampleRate);
            mTrackMetaData->setInt32(kKeyChannelCount,
                asf->StreamInfo[coreStreamIndex].NvMMStream_Props.AudioProps.NChannels);
            mTrackMetaData->setInt32(kKeyBitRate,
                asf->StreamInfo[coreStreamIndex].NvMMStream_Props.AudioProps.BitRate);
        } else {
            ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
            return BAD_VALUE;
        }
    } else {
        uint64_t thumbNailTime = 0;

        if(asf->IndexObjPresent == NV_FALSE) {
            mTrackMetaData->setInt64(kKeyThumbnailTime, 0);
        } else {
            err = pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfFindLargestIframe(pNvAsfParserHalCoreHandle->pCoreContext);
            if(err == RESULT_OK) {
                thumbNailTime = asf->LargestIFrameTimestamp / 10;
                mTrackMetaData->setInt64(kKeyThumbnailTime, thumbNailTime);
            } else {
                mTrackMetaData->setInt64(kKeyThumbnailTime, 0);
            }
        }
        ALOGV("Function %s : Setting thumbnail time %lld\n",__func__,thumbNailTime);
    }
    return OK;
}
/**************************************************************************************
* Function    : NvAsfParserHal_GetStreamCount
* Description : Returns the number of streams present in media to caller
 *************************************************************************************/
size_t NvAsfParserHal_GetStreamCount(
        void* asfParserHalHandle) {

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;
    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    ALOGV("Function %s : Returning stream count %d\n",__func__,pNvAsfParserHalCoreHandle->streamCount);
    return pNvAsfParserHalCoreHandle->streamCount;
}
/**************************************************************************************
* Function    : NvAsfParserHal_Read
* Description : Returns the buffer containing encoded data for one frame of a particular
*               stream to caller
 *************************************************************************************/
status_t NvAsfParserHal_Read(
            void* asfParserHalHandle,
            int32_t streamIndex,
            MediaBuffer *mBuffer,
            const MediaSource::ReadOptions* options) {

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;
    int64_t timeStamp = 0, seekTimeUs = 0;
    uint8_t * temp = NULL;
    NvAsfParserHalStream* pStream = NULL;
    status_t ret = OK;
    int32_t decodeflags = 0;
    NvError err = NvSuccess;

    if((pNvAsfParserHalCoreHandle == NULL) || (mBuffer == NULL)) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }
    temp = (uint8_t *) mBuffer->data();
    if(temp == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }
    // Handle seek condition
    MediaSource::ReadOptions::SeekMode mode;
    if((!pNvAsfParserHalCoreHandle->pCoreContext->asf->IsBroadcast) &&
        (options && options->getSeekTo(&seekTimeUs,&mode))) {
        ret = NvAsfParserHal_SetPosition(pNvAsfParserHalCoreHandle, streamIndex, seekTimeUs, mode);
        if(ret != OK) {
            ALOGE("Function %s line %d : NvAsfParserHal_SetPosition returned error %x \n",__func__,__LINE__,ret);
            return ret;
        }
    }

    pStream = pNvAsfParserHalCoreHandle->pStreams[streamIndex];

    if(mBuffer->size() < pStream->bufReq.maxBufferSize) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)ERROR_BUFFER_TOO_SMALL);
        return ERROR_BUFFER_TOO_SMALL;
    }
    // Pass handle to MediaBuffer's data buffer to get data
    pStream->pBuffer->Payload.Ref.sizeOfBufferInBytes = mBuffer->size();
    pStream->pBuffer->Payload.Ref.pMem = temp;

    err = ReadAVPacket(pNvAsfParserHalCoreHandle, streamIndex, pStream->pBuffer);

    if((pStream->pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_EndOfStream) || (err == NvError_ParserEndOfStream)) {
        mBuffer->set_range(0,0);
        ALOGV("Function %s : Returning END_OF_STREAM \n",__func__);
        return ERROR_END_OF_STREAM;
    }

    mBuffer->set_range(0, pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes);
    mBuffer->meta_data()->clear();
    if(pStream->pBuffer->PayloadInfo.TimeStamp > 0) {
        timeStamp = (int64_t)(pStream->pBuffer->PayloadInfo.TimeStamp)/10;
    }
    mBuffer->meta_data()->setInt64(kKeyTime, timeStamp);

    // Set decoder flags
    decodeflags |= OMX_BUFFERFLAG_NEED_PTS;
    if(pStream->pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag) {
        decodeflags |= OMX_BUFFERFLAG_CODECCONFIG;
    }
    if(pStream->pBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame == NV_TRUE) {
        decodeflags |= OMX_BUFFERFLAG_SYNCFRAME;
    }
    mBuffer->meta_data()->setInt32(kKeyDecoderFlags, decodeflags);
    ALOGV("Function %s : Returning buffer with size %d, PTS %lld and Index %d \n",
        __func__,pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes,timeStamp,streamIndex);

    // Clear some fields
    pStream->pBuffer->PayloadInfo.BufferFlags = 0;
    pStream->pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    pStream->pBuffer->PayloadInfo.TimeStamp = 0;
    pStream->pBuffer->Payload.Ref.sizeOfBufferInBytes = 0;
    pStream->pBuffer->Payload.Ref.pMem = NULL;
    return OK;
}
/**************************************************************************************
* Function    : NvAsfParserHal_SetTrackHeader
* Description : Sets the header info of a particular track
 *************************************************************************************/
status_t NvAsfParserHal_SetTrackHeader(
            void* asfParserHalHandle,
            int32_t streamIndex,
            sp<MetaData> meta) {

    int32_t coreStreamIndex = 0;
    CAsf* asf = NULL;

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;

    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    coreStreamIndex = pNvAsfParserHalCoreHandle->pStreams[streamIndex]->coreStreamIndex;
    asf = (CAsf* )pNvAsfParserHalCoreHandle->pCoreContext->asf;

    if(NVMM_ISSTREAMVIDEO(asf->StreamInfo[coreStreamIndex].StreamType)) {
        if(asf->StreamInfo[coreStreamIndex].StreamType == NvMMStreamType_H264) {
                uint8_t *pBuf = (uint8_t *)pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff;
                //check non-nal mode
                if ((pBuf[0] == 0x00 && pBuf[1] == 0x00 && pBuf[2] == 0x00 && pBuf[3] == 0x01)
                   || (pBuf[0] == 0x00 && pBuf[1] == 0x00 && pBuf[2] == 0x01)) {
                    uint32_t type;
                    const void *data;
                    size_t size;
                    sp<MetaData> metahdr;
                    sp<ABuffer> csd = new ABuffer(asf->VideoHeadLength);
                    memcpy(csd->data(),pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff,
                                asf->VideoHeadLength);
                    metahdr = MakeAVCCodecSpecificData(csd);
                    if (metahdr != NULL) {
                        metahdr->findData(kKeyAVCC, &type,&data,&size);
                        meta->setData(kKeyAVCC, kTypeHeader,
                                       data,size);
                    }
                } else {
                    meta->setData(kKeyAVCC, kTypeHeader,
                    pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff, asf->VideoHeadLength);
                }
        } else {
            meta->setData(kKeyHeader, kTypeHeader,
                pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff, asf->VideoHeadLength);
        }
    } else if(NVMM_ISSTREAMAUDIO(asf->StreamInfo[coreStreamIndex].StreamType)) {
        meta->setData(kKeyHeader, kTypeHeader,
            pNvAsfParserHalCoreHandle->pCoreContext->TempAudBuff, asf->AudioHeadLength);
    } else {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    return OK;
}

/*****************************************************************************************************
* Static Functions
*****************************************************************************************************/
static status_t NvAsfParserHal_OpenCore(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle) {

    status_t ret = OK;
    NvError err = NvSuccess;
    CAsf* asf = NULL;

    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    pNvAsfParserHalCoreHandle->pCoreContext->coreHandle =
        (NvMMParserCoreHandle)NvOsAlloc(sizeof(NvMMParserCore));
    if(pNvAsfParserHalCoreHandle->pCoreContext->coreHandle == NULL) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle->pCoreContext->coreHandle, 0, sizeof(NvMMParserCore));

    pNvAsfParserHalCoreHandle->pCoreContext->asf = (CAsf *)NvOsAlloc(sizeof(CAsf));
    if(pNvAsfParserHalCoreHandle->pCoreContext->asf == NULL) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle->pCoreContext->asf, 0, sizeof(CAsf));
    asf = (CAsf *)pNvAsfParserHalCoreHandle->pCoreContext->asf;

    NvParserHal_GetFileContentPipe(&asf->pPipe);

    if(asf->pPipe->cpipe.Open(&asf->cphandle,
        (char*)pNvAsfParserHalCoreHandle->pCoreContext->filename,CP_AccessRead) != NvSuccess) {
        ret = UNKNOWN_ERROR;
        ALOGE("Function %s line %d : content pipe open Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }

    asf->m_bSentFirstAudioPacket = asf->m_bSentFirstVideoPacket = 0;
    asf->NvAsfParser = NULL;
    asf->m_oAudioStreamNumber = asf->m_oVideoStreamNumber = -1;
    asf->m_bDoneVideoPacket = asf->m_bDoneAudioPacket = 0;
    asf->IndexObjPresent = NV_FALSE;
    asf->UlpMode = NV_FALSE;
    asf->bDisableBuffConfig = NV_TRUE;

    asf->NvAsfIndexObject = (NvAsfSimpleIndexObject*)NvOsAlloc(sizeof(NvAsfSimpleIndexObject));
    if(!asf->NvAsfIndexObject) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(asf->NvAsfIndexObject,0,sizeof(NvAsfSimpleIndexObject));

    asf->NvAsfAudioBuffer = (AsfBufferPropsType*) NvOsAlloc(sizeof(AsfBufferPropsType));
    if(!asf->NvAsfAudioBuffer) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        ret = NO_MEMORY;
        goto cleanup;
    }
    NvOsMemset(asf->NvAsfAudioBuffer,0,sizeof(AsfBufferPropsType));

    asf->NvAsfVideoBuffer = (AsfBufferPropsType*) NvOsAlloc(sizeof(AsfBufferPropsType));
    if(!asf->NvAsfAudioBuffer) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        ret = NO_MEMORY;
        goto cleanup;
    }
    NvOsMemset(asf->NvAsfVideoBuffer,0,sizeof(AsfBufferPropsType));

    asf->AsfMetadata = (NvMMAsfMetaData*) NvOsAlloc(sizeof(NvMMAsfMetaData));
    if(!asf->AsfMetadata) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(asf->AsfMetadata,0,sizeof(NvMMAsfMetaData));

    asf->pMarkerDataInfo = (NvMMMarkerDataInfo*) NvOsAlloc(sizeof(NvMMMarkerDataInfo));
    if(!asf->pMarkerDataInfo) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(asf->pMarkerDataInfo,0,sizeof(NvMMMarkerDataInfo));

    asf->pWmPicture  = (NvAsfWmPicture*) NvOsAlloc(sizeof(NvAsfWmPicture));
    if(!asf->pWmPicture) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(asf->pWmPicture,0,sizeof(NvAsfWmPicture));

    // Allocate TempVidBuff Aswell
    pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff =
        (NvU8*)NvOsAlloc(NVMMBLOCK_ASFPARSER_MAX_HEADER_SIZE * sizeof(NvU8));
    if(!pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff,0,
                sizeof(NVMMBLOCK_ASFPARSER_MAX_HEADER_SIZE * sizeof(NvU8)));

    pNvAsfParserHalCoreHandle->pCoreContext->TempAudBuff =
        (NvU8*)NvOsAlloc(NVMMBLOCK_ASFPARSER_MAX_HEADER_SIZE * sizeof(NvU8));
    if(!pNvAsfParserHalCoreHandle->pCoreContext->TempAudBuff) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pNvAsfParserHalCoreHandle->pCoreContext->TempAudBuff,0,
                sizeof(NVMMBLOCK_ASFPARSER_MAX_HEADER_SIZE * sizeof(NvU8)));

    pNvAsfParserHalCoreHandle->pCoreContext->DRMInitialized = NV_FALSE;
    asf->drm_Interface.pNvDrmCreateContext =NULL;
    asf->drm_Interface.pNvDrmDestroyContext=NULL;
    asf->drm_Interface.pNvDrmBindLicense=NULL;
    asf->drm_Interface.pNvDrmUpdateMeteringInfo=NULL;
    asf->drm_Interface.pNvDrmDecrypt=NULL;
    asf->drm_Interface.pNvDrmBindContent=NULL;
    asf->drm_Interface.pNvDrmGenerateLicenseChallenge=NULL;

    if(pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pCreateNvAsfParser(&asf->NvAsfParser) != RESULT_OK) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Core parser create failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }

    asf->NvAsfParser->bIsNvlib = NV_FALSE;
    asf->NvAsfParser->DrmError = 0;
    asf->NvAsfParser->pTempCachedBuff = NULL;
    asf->NvAsfParser->tempPayloadCount =0;

    asf->pPipe->GetSize(asf->cphandle, &(asf->NvAsfParser->m_FileSize));

    pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfFillInDefaultCallbacks(&asf->m_oCallbacks);
    asf->m_oCallbacks.OnAsfParserGetData = AsfOnAsfParserGetData;
    asf->m_oCallbacks.OnAsfParserSetOffset = AsfOnAsfParserSetOffset;
    asf->m_oCallbacks.OnAsfParserPayloadData = AsfOnAsfParserPayloadData;

    if(pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfOpen(asf->NvAsfParser,&asf->m_oCallbacks,asf) != RESULT_OK) {
        ret = UNKNOWN_ERROR;
        ALOGE("Function %s line %d : Core parser open failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    return OK;

cleanup:
    if(pNvAsfParserHalCoreHandle->pCoreContext) {
        NvAsfParserHal_DestroyCore(pNvAsfParserHalCoreHandle);
    }

    return ret;
}

/*****************************************************************************************************/
static status_t NvAsfParserHal_DestroyCore(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle) {
    CAsf* asf = NULL;
    uint32_t i = 0;

    if (pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    asf = (CAsf* )pNvAsfParserHalCoreHandle->pCoreContext->asf;

    if (pNvAsfParserHalCoreHandle->pCoreContext) {
        FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->TempAudBuff);
        FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->TempVidBuff);
        if (asf) {
           if (asf->NvAsfParser) {
               pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pDestroyNvAsfParser
                   (asf->NvAsfParser);
           }
           if (asf->pWmPicture) {
               FREE_BUFFER(asf->pWmPicture->pMIMEType);
               FREE_BUFFER(asf->pWmPicture->pwszDescription);
               FREE_BUFFER(asf->pWmPicture);
           }
           if (asf->pMarkerDataInfo) {
               FREE_BUFFER(asf->pMarkerDataInfo->pData);
               FREE_BUFFER(asf->pMarkerDataInfo);
           }
           if (asf->AsfMetadata) {
               FREE_BUFFER(asf->AsfMetadata->oArtistMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oAlbumMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oGenreMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oTitleMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oYearMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oTrackNoMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oEncodedMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oCommentMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oComposerMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oPublisherMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oOrgArtistNoMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oCopyRightMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oUrlMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oBpmMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oAlbumArtistMetaData.pClientBuffer);
               FREE_BUFFER(asf->AsfMetadata->oCoverArt.pClientBuffer);

               NvOsFree(asf->AsfMetadata);
           }
           FREE_BUFFER(asf->NvAsfVideoBuffer);
           FREE_BUFFER(asf->NvAsfAudioBuffer);
           if (asf->NvAsfIndexObject) {
               FREE_BUFFER(asf->NvAsfIndexObject->pIndexEntries);
               NvOsFree(asf->NvAsfIndexObject);
           }
           FREE_BUFFER(asf->oBitrateMutualExclusionObject.puStreamNumbers);
           FREE_BUFFER(asf->oProtectionIdentifierObject.pData);
           FREE_BUFFER(asf->oExtendedContentEncryptionObject.pData);
           FREE_BUFFER(asf->oContentEncryptionObject.pSecretData);
           FREE_BUFFER(asf->oContentEncryptionObject.szLicenseURL.szString);
           FREE_BUFFER(asf->oContentEncryptionObject.szKeyID.szString);
           FREE_BUFFER(asf->oContentEncryptionObject.szProtectionType.szString);

           if (asf->cphandle) {
               asf->pPipe->cpipe.Close(asf->cphandle);
           }

           for(i = 0; i < pNvAsfParserHalCoreHandle->streamCount; i++) {
               FREE_BUFFER(asf->oVideoMediaTypeSpecificData[i].oFormatData.pCodecSpecificData);
           }

           NvOsFree(asf);
           pNvAsfParserHalCoreHandle->pCoreContext->asf = NULL;
       }

       FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->coreHandle->licenseUrl);
       FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->coreHandle->licenseChallenge);
       FREE_BUFFER(pNvAsfParserHalCoreHandle->pCoreContext->coreHandle);
    }

    return OK;
}

/*****************************************************************************************************/
static status_t NvAsfParserHal_CalculateBufReq(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                    NvAsfParserHalStream* pStream,
                    int32_t streamIndex) {

    uint32_t Width     = 0;
    uint32_t Height    = 0;
    NvParserHalBufferRequirements* pBufReq = &pStream->bufReq;
    CAsf* asf = pNvAsfParserHalCoreHandle->pCoreContext->asf;

    pBufReq->minBuffers  = 3;
    pBufReq->maxBuffers = MAX_NUM_BUFFERS;

    if(NVMM_ISSTREAMVIDEO(asf->StreamInfo[pStream->coreStreamIndex].StreamType)) {
        Width = asf->StreamInfo[pStream->coreStreamIndex].NvMMStream_Props.VideoProps.Width;
        Height = asf->StreamInfo[pStream->coreStreamIndex].NvMMStream_Props.VideoProps.Height;

        if((Width == 0) && (Height == 0)) {
            pBufReq->minBufferSize = MIN_BUF_SIZE_VIDEO;
            pBufReq->maxBufferSize = MAX_BUF_SIZE_VIDEO;
        } else if((Width > 320) && (Height > 240)) {
        /*  for larged size streams 3/4th of Height and Width works with most of the streams */
            pBufReq->minBufferSize = (Width * Height * 3) >> 2;
            pBufReq->maxBufferSize = (Width * Height * 3) >> 2;
        } else {
        /* for less than QVGA size buffers, its better to allocate YUV sized buffes,
            as the input buffer for intra frames might be large */
            pBufReq->minBufferSize = (Width * Height * 3) >> 1;
            pBufReq->maxBufferSize = (Width * Height * 3) >> 1;
        }
    }
    if(NVMM_ISSTREAMAUDIO(asf->StreamInfo[pStream->coreStreamIndex].StreamType)) {
        pNvAsfParserHalCoreHandle->AudioIndex = streamIndex;
        if(asf->StreamInfo[pStream->coreStreamIndex].StreamType == NvMMStreamType_WMA) {
            pBufReq->minBufferSize = WMA_DEC_MIN_INPUT_BUFFER_SIZE;
            pBufReq->maxBufferSize = WMA_DEC_MAX_INPUT_BUFFER_SIZE;
        } else if(asf->StreamInfo[pStream->coreStreamIndex].StreamType == NvMMStreamType_WMAPro) {
            pBufReq->minBufferSize = WMAPRO_DEC_MIN_INPUT_BUFFER_SIZE;
            pBufReq->maxBufferSize = WMAPRO_DEC_MAX_INPUT_BUFFER_SIZE;
        } else if(asf->StreamInfo[pStream->coreStreamIndex].StreamType == NvMMStreamType_WAV) {
            pBufReq->minBufferSize = WAV_DEC_MIN_INPUT_BUFFER_SIZE;
            pBufReq->maxBufferSize = WAV_DEC_MAX_INPUT_BUFFER_SIZE;
        } else {
            pBufReq->minBufferSize = WMALSL_DEC_MIN_INPUT_BUFFER_SIZE;
            pBufReq->maxBufferSize = WMALSL_DEC_MAX_INPUT_BUFFER_SIZE;
        }
    }

    return OK;
}

/*****************************************************************************************************/
static status_t NvAsfParserHal_CreateStream(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                    int32_t streamIndex) {

    NvAsfParserHalStream* pStream = NULL;
    status_t ret = OK;
    NvMMBuffer* pBuffer = NULL;
    uint32_t i,size;

    // If the stream already exist, delete it.
    if(pNvAsfParserHalCoreHandle->pStreams[pNvAsfParserHalCoreHandle->streamCount]) {
        NvAsfParserHal_DestroyStream(pNvAsfParserHalCoreHandle, pNvAsfParserHalCoreHandle->streamCount);
    }

    pStream = (NvAsfParserHalStream*)NvOsAlloc(sizeof(NvAsfParserHalStream));
    if(pStream == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)NO_MEMORY);
        return NO_MEMORY;
    }
    NvOsMemset(pStream,0,sizeof(NvAsfParserHalStream));

    pStream->coreStreamIndex = streamIndex;

    NvAsfParserHal_CalculateBufReq(pNvAsfParserHalCoreHandle,pStream,pNvAsfParserHalCoreHandle->streamCount);

    pStream->pBuffer = (NvMMBuffer*) NvOsAlloc(sizeof(NvMMBuffer));
    if(pStream->pBuffer == NULL) {
        ret = NO_MEMORY;
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,ret);
        goto cleanup;
    }
    NvOsMemset(pStream->pBuffer, 0, sizeof(NvMMBuffer));
    pStream->pBuffer->StructSize = sizeof(NvMMBuffer);
    pStream->pBuffer->Payload.Ref.PhyAddress = NV_RM_INVALID_PHYS_ADDRESS;

    pNvAsfParserHalCoreHandle->pStreams[pNvAsfParserHalCoreHandle->streamCount] = pStream;
    pNvAsfParserHalCoreHandle->streamCount++;

cleanup:
    if(ret != OK) {
        FREE_BUFFER(pStream->pBuffer);
        NvOsFree(pStream);
        pNvAsfParserHalCoreHandle->pStreams[pNvAsfParserHalCoreHandle->streamCount] = NULL;
    }
    return ret;
}

/*****************************************************************************************************/
static status_t NvAsfParserHal_DestroyStream(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                    int32_t streamIndex) {

    NvAsfParserHalStream* pStream = pNvAsfParserHalCoreHandle->pStreams[streamIndex];

    if(pStream) {
        FREE_BUFFER(pStream->pBuffer);
        NvOsFree(pStream);
        pNvAsfParserHalCoreHandle->pStreams[streamIndex] = NULL;
    }
    return OK;
}

/*****************************************************************************************************/
static status_t NvAsfParserHal_SetTrackMimeType(
                    void* asfParserHalHandle,
                    int32_t coreStreamIndex,
                    sp<MetaData> metaData) {

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;

    switch(pNvAsfParserHalCoreHandle->pCoreContext->asf->StreamInfo[coreStreamIndex].StreamType) {
        case NvMMStreamType_WMA:
        case NvMMStreamType_WMAPro:
        case NvMMStreamType_WMALSL:
        case NvMMStreamType_WMAV:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_WMA);
            break;
        case NvMMStreamType_WAV:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_RAW);
            break;
        case NvMMStreamType_AAC:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_AUDIO_AAC);
            break;
        case NvMMStreamType_H264:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_AVC);
            break;
        case NvMMStreamType_WMV:
            metaData->setCString(kKeyMIMEType,MEDIA_MIMETYPE_VIDEO_WMV);
            break;
        default:
            pNvAsfParserHalCoreHandle->pCoreContext->asf->StreamInfo[coreStreamIndex].StreamType =
                NvMMStreamType_OTHER;
            ALOGV("Function %s : Unsupported codec type %x \n",__func__,
                pNvAsfParserHalCoreHandle->pCoreContext->asf->StreamInfo[coreStreamIndex].StreamType);
            metaData->setCString(kKeyMIMEType,"application/error");
            break;
    }
    return OK;
}
/*****************************************************************************************************/
static uint64_t NvAsfParserHal_GetDuration(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle) {
    uint64_t i = 0, streamDuration = 0;
    CAsf* asf = (CAsf* )pNvAsfParserHalCoreHandle->pCoreContext->asf;

    for(i = 0; i < asf->NoOfStreams; i++) {
        if(asf->StreamInfo[i].TotalTime > 0 && (uint64_t)(asf->StreamInfo[i].TotalTime / 10)
             > streamDuration)
                streamDuration = asf->StreamInfo[i].TotalTime/10;
    }

    ALOGV("Function %s : Returning stream duration %lld \n",__func__,streamDuration);
    return streamDuration;
}
/*****************************************************************************************************/
static NvError ReadAVPacket(
                    NvAsfParserHalCore* pNvAsfParserHalCoreHandle,
                    int32_t StreamIndex,
                    NvMMBuffer* pBuffer) {

    NvError status                      = NvSuccess;
    NvMMAsfParserCoreContext *pContext  =
       (NvMMAsfParserCoreContext *)pNvAsfParserHalCoreHandle->pCoreContext;
    CAsf *asf                           = pContext->asf;
    NvAsfResult er;

    NvAsfParserHalStream* pStream = pNvAsfParserHalCoreHandle->pStreams[StreamIndex];
    pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    pBuffer->Payload.Ref.startOfValidData = 0;
    pBuffer->PayloadInfo.TimeStamp = 0;
    pBuffer->PayloadInfo.BufferFlags = 0;

    if(NVMM_ISSTREAMAUDIO(asf->StreamInfo[pStream->coreStreamIndex].StreamType)) {
        pContext->pWorkingOutputAudioBuffer = pBuffer;
        pContext->pPacketType = NVMM_ASFAUDIO_ONLY;
    } else if(NVMM_ISSTREAMVIDEO(asf->StreamInfo[pStream->coreStreamIndex].StreamType)) {
        pContext->pWorkingOutputVideoBuffer = pBuffer;
        pContext->pPacketType = NVMM_ASFVIDEO_ONLY;
    } else {
        ALOGV("Function %s : stream not supported \n",__func__);
    }

    er = pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfParse(pContext);
    if(er == RESULT_END_OF_FILE ) {
        pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_EndOfStream;
        status = NvError_ParserEndOfStream;
    } else if(er ==  RESULT_OK_BREAK) {
        // If Audio/Video Header not sent, send header first
        if((pContext->VidHeaderNotSent) && (pContext->HeaderFinished) &&
            (pContext->pPacketType == NVMM_ASFVIDEO_ONLY)) {
            NvOsMemcpy(pContext->pWorkingOutputVideoBuffer->Payload.Ref.pMem,
                pContext->TempVidBuff, asf->VideoHeadLength);
            pContext->pWorkingOutputVideoBuffer->Payload.Ref.sizeOfValidDataInBytes =
                asf->VideoHeadLength;
            pContext->pWorkingOutputVideoBuffer->Payload.Ref.startOfValidData = 0;
            pContext->pWorkingOutputVideoBuffer->PayloadInfo.TimeStamp = 0;
            pContext->pWorkingOutputVideoBuffer->PayloadInfo.BufferFlags |=
                NvMMBufferFlag_HeaderInfoFlag;
            pContext->VidHeaderNotSent = 0;
        } else if((pContext->AudHeaderNotSent) && (pContext->HeaderFinished)&&
            (pContext->pPacketType == NVMM_ASFAUDIO_ONLY)) {
            NvOsMemcpy(pContext->pWorkingOutputAudioBuffer->Payload.Ref.pMem,
                pContext->TempAudBuff, asf->AudioHeadLength);
            pContext->pWorkingOutputAudioBuffer->Payload.Ref.sizeOfValidDataInBytes =
                asf->AudioHeadLength;
            pContext->pWorkingOutputAudioBuffer->Payload.Ref.startOfValidData = 0;
            pContext->pWorkingOutputAudioBuffer->PayloadInfo.TimeStamp = 0;
            pContext->pWorkingOutputAudioBuffer->PayloadInfo.BufferFlags |=
                NvMMBufferFlag_HeaderInfoFlag;
            pContext->AudHeaderNotSent = 0;
        } else if((asf->m_oVideoStreamNumber > 0 && asf->m_bDoneVideoPacket) &&
            (pContext->pPacketType == NVMM_ASFVIDEO_ONLY)) {
            asf->m_bDoneVideoPacket = 0;
        } else if((asf->m_oAudioStreamNumber > 0 && asf->m_bDoneAudioPacket) &&
            (pContext->pPacketType == NVMM_ASFAUDIO_ONLY)) {
            asf->m_bDoneAudioPacket   = 0;
        }
    } else if(er == RESULT_OUT_OF_MEMORY) {
        status = NvError_InsufficientMemory;
    }

    return status;
}

/*********************************************************************************************************/
status_t NvAsfParserHal_SetPosition(
            void* asfParserHalHandle,
            int32_t streamIndex,
            uint64_t seekPosition,
            MediaSource::ReadOptions::SeekMode mode) {

    NvAsfParserHalCore* pNvAsfParserHalCoreHandle = (NvAsfParserHalCore*)asfParserHalHandle;
    NvMMAsfParserCoreContext *pContext = NULL;
    CAsf *asf = NULL;
    NvError status = NvSuccess;
    status_t ret = OK;
    NvAsfResult er;
    NvU32 i;
    NvU64 NewSeekTime = 0;
    NvU64 AudioNewSeekTime = 0;
    NvBool Video_Flag = NV_FALSE;
    NvS32 direction;
    NvU64 NewSeekTimeStamp = seekPosition * 10;

    if(pNvAsfParserHalCoreHandle == NULL) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)BAD_VALUE);
        return BAD_VALUE;
    }

    pContext = (NvMMAsfParserCoreContext *)pNvAsfParserHalCoreHandle->pCoreContext;
    asf = (CAsf*) pContext->asf;

    if(!pContext->HeaderFinished) {
        ALOGE("Function %s line %d : Failed with error %x \n",__func__,__LINE__,(status_t)NO_INIT);
        return NO_INIT;
    }

    if((pNvAsfParserHalCoreHandle->bHasSupportedVideo) && (streamIndex == pNvAsfParserHalCoreHandle->AudioIndex)) {
        /* If media file contains video, do seek for video stream and audio stream at the same time
           This will fine tune audio seek time with video I frame time */
        return OK;
    }

    if(pContext->CurrentTimeStamp <= NewSeekTimeStamp) {
        direction = 1;
    } else {
        direction = -1;
    }

    if (mode == MediaSource::ReadOptions::SEEK_PREVIOUS_SYNC) {
        direction = -1;
        ALOGV("Function %s : Forcing backward seek",__func__);
    }

    ALOGV("Function %s : Seeking called at timestamp %lld",__func__,NewSeekTimeStamp);
    pContext->CurrentOffsetList = 0; // Reset the CurrentOffsetList counter
    asf->EOSOccurred  = 0; //reset the EOSOccured

    pContext->ParsePosition.Position = NewSeekTimeStamp;

    for(i = 0; i<asf->NoOfStreams; i++) {
        if(NVMM_ISSTREAMVIDEO(asf->StreamInfo[i].StreamType)) {
            Video_Flag = NV_TRUE;
            asf->NvAsfParser->vid_Payload = 0; // Reset the video payload whenever seek is  called

            if(NewSeekTimeStamp == 0) {
                pContext->VidHeaderNotSent = 1;
                asf->NvAsfVideoBuffer->nFilledLen = asf->VideoHeadLength;
                asf->NvAsfParser->m_VideoPacketno = 0;
                asf->NvAsfParser->vid_Payload = 0;
                NewSeekTime = 0;
                // Reset the following flag
                pContext->bFileReachedEOF = NV_FALSE;
                // Update NewSeekTime
                pContext->ParsePosition.Position = NewSeekTime;
                status = NvSuccess;
                break;
            }

            er = pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfVideoNewSeekTime(pContext,
                    &NewSeekTimeStamp,&NewSeekTime,direction);

            if(er == RESULT_OK) {
                asf->NvAsfParser->NoOfPktsToConsume = 0; // ResetBack
                // Update NewSeekTime
                pContext->ParsePosition.Position = NewSeekTime;
                ALOGV("Function %s : Video is seeked to timestamp %lld",__func__,NewSeekTime);
                status = NvSuccess;
                break;
            } else if(er == RESULT_END_OF_FILE) {
                pContext->bFileReachedEOF = NV_TRUE;
                // Update the postion
                asf->NvAsfParser->m_VideoPacketno = (NvU32)asf->NvAsfParser->m_oFilePropertiesObject.uDataPacketsCount;
                NewSeekTime = asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration
                              - asf->m_oPreRoll * MS_TO_100NS_MULTIPLY;
                pContext->ParsePosition.Position = NewSeekTime;
                pContext->CurrentTimeStamp = NewSeekTime;
                ALOGV("Function %s : Video reached EOF and return timestamp %lld",__func__,NewSeekTime);
                status = NvSuccess;
                break;
            } else {
                ret = UNKNOWN_ERROR;
                goto cleanup;
            }
        }
    }
    for(i = 0; i < asf->NoOfStreams; i++) {
        if(NVMM_ISSTREAMAUDIO(asf->StreamInfo[i].StreamType)) {
            asf->NvAsfParser->aud_Payload = 0; // Reset the audio payload whenever seek is  called
            if(NewSeekTimeStamp == 0) {
                pContext->AudHeaderNotSent = 1;
                asf->NvAsfAudioBuffer->nFilledLen = asf->AudioHeadLength;
                pContext->bFileReachedEOF = NV_FALSE;
                asf->NvAsfParser->m_AudioPacketno = 0;
                asf->NvAsfParser->aud_Payload = 0;
                pContext->ParsePosition.Position = 0;
                pContext->CurrentTimeStamp = 0;
                // After completeion of seek set the below flag
                pContext->bSeekDone = NV_TRUE;
                ret = OK;
                goto cleanup;
            }
            if(Video_Flag) {// If stream Contains Video also calc timestamp based on video timestamp
                // Calculate AudioPacket Number based on NewSeekTime from Video
                if(NewSeekTime >= (asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration
                    - asf->m_oPreRoll * MS_TO_100NS_MULTIPLY)) {
                    pContext->bFileReachedEOF = NV_TRUE;
                    // Update the postion
                    pContext->ParsePosition.Position =
                        (asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration
                        - asf->m_oPreRoll * MS_TO_100NS_MULTIPLY);
                    // After completeion of seek set the below flag
                    pContext->bSeekDone = NV_TRUE;
                    asf->NvAsfParser->m_AudioPacketno =
                        (NvU32)asf->NvAsfParser->m_oFilePropertiesObject.uDataPacketsCount;
                    pContext->CurrentTimeStamp = pContext->ParsePosition.Position;
                    ret = OK;
                    goto cleanup;
                } else  if(NewSeekTime == 0) {
                    pContext->AudHeaderNotSent = 1;
                    asf->NvAsfAudioBuffer->nFilledLen = asf->AudioHeadLength;
                    pContext->bFileReachedEOF = NV_FALSE;
                    asf->NvAsfParser->m_AudioPacketno = 0;
                    asf->NvAsfParser->aud_Payload = 0;
                    pContext->ParsePosition.Position = 0;
                    pContext->CurrentTimeStamp = 0;
                    // After completeion of seek set the below flag
                    pContext->bSeekDone = NV_TRUE;
                    ret = OK;
                    goto cleanup;
                }

                // Calculate the packet number according to the given postion(timestamp)
                if(asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration > 0 )
                    asf->NvAsfParser->m_AudioPacketno =
                        (NvU32)((NewSeekTime) *
                        asf->NvAsfParser->m_oFilePropertiesObject.uDataPacketsCount /
                        asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration);

                er = pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfAudioNewSeekTime(pContext,
                        NewSeekTime,&AudioNewSeekTime);
                ALOGV("Function %s : Audio is seeked to timestamp %lld",__func__,AudioNewSeekTime);

                // reset the below flag whenever setpos called.
                asf->NvAsfParser->AudSyncToVidFlag = NV_FALSE;

                // Assign always audio TS as a new seektim
                if(er == RESULT_OK){
                    NewSeekTime = AudioNewSeekTime;
                }
                break;
            } else {/* If stream contains only Audio calculate audio packet no
                    based on given timestamp, Time stamp is in 100 nSec */
                if(NewSeekTimeStamp >= asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration) {
                    asf->NvAsfParser->m_AudioPacketno =
                        (NvU32)asf->NvAsfParser->m_oFilePropertiesObject.uDataPacketsCount;
                    NewSeekTime = asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration -
                                    asf->m_oPreRoll * MS_TO_100NS_MULTIPLY;
                    status = NvError_ParserEndOfStream;
                    ALOGV("Function %s : Audio EOF reached",__func__);
                    ret = ERROR_END_OF_STREAM;
                    goto cleanup;
                }

                // Calculate the packet number according to the given postion(timestamp)
                else if(asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration > 0 ) {
                    asf->NvAsfParser->m_AudioPacketno =
                        (NvU32)((NewSeekTimeStamp) *
                        asf->NvAsfParser->m_oFilePropertiesObject.uDataPacketsCount /
                        asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration);
                    asf->NvAsfParser->aud_Payload = 0;

                    if((asf->NvAsfParser->bHasDRM) && (asf->AudioOnly))
                        asf->pPipe->InvalidateCache(asf->cphandle);

                    er = pNvAsfParserHalCoreHandle->pNvAsfCoreParserApi.pNvAsfAudioNewSeekTime(pContext,
                            NewSeekTimeStamp,&NewSeekTime);

                    if(er == RESULT_END_OF_FILE) {
                        asf->NvAsfParser->m_AudioPacketno =
                            (NvU32)asf->NvAsfParser->m_oFilePropertiesObject.uDataPacketsCount;
                        NewSeekTime = asf->NvAsfParser->m_oFilePropertiesObject.uPlayDuration -
                                        asf->m_oPreRoll * MS_TO_100NS_MULTIPLY;
                        pContext->bFileReachedEOF = NV_TRUE;
                        ALOGV("Function %s : Audio EOF reached",__func__);
                        status = NvSuccess;
                    } else if(er != RESULT_OK) {
                        pContext->DeferSeek = NV_TRUE;
                        pContext->RequestedSeekTime = NewSeekTimeStamp;
                        pContext->NewAudioPktno = asf->NvAsfParser->m_AudioPacketno;
                        // After completeion of seek set the below flag
                        pContext->bSeekDone = NV_TRUE;
                        ret = OK;
                        goto cleanup;
                    }
                }
                // Update NewSeekTime
                pContext->ParsePosition.Position = NewSeekTime;
            } // end of else audio only Timestamp calculation
        } // end of main if loop
    } // end of for loop
cleanup:
    return ret;
}

/*****************************************************************************************************
* Callback Functions
*****************************************************************************************************/
NvAsfResult AsfOnAsfParserGetData(
                void *data,
                NvU8* pBuffer,
                NvU32 uSize,
                NvU32* puRead) {

    CPresult status;
    CAsf* asf = (CAsf*)data;

    status = asf->pPipe->cpipe.Read(asf->cphandle,(CPbyte *)pBuffer, uSize);

    if(status == NvSuccess) {
        *puRead = uSize;
        return RESULT_OK;
    } else if(status == NvError_EndOfFile) {
        return RESULT_END_OF_FILE;
    } else
        return RESULT_ERROR_FILE_READ;
}

/*****************************************************************************************************/
NvAsfResult AsfOnAsfParserSetOffset(
                void *data,
                NvU64 uOffset) {

    CPresult status;
    CAsf *asf = (CAsf *)data;
    status = asf->pPipe->SetPosition64(asf->cphandle, (CPint64)uOffset, CP_OriginBegin);
    if((status == NvSuccess) || (status == NvError_NotSupported))
        return RESULT_OK;
    else
        return RESULT_FAIL;
}

/*****************************************************************************************************/
NvAsfResult AsfOnAsfParserPayloadData(
                void *data,
                CAsfPayloadData* pPayloadData,
                NvU32* puRead,
                NvU64 OffsetinBytes,
                NvU8** pSrcBuffer,
                NvBool LastBuffer) {

    NvAsfResult er = RESULT_OK;
    CPresult err = NvSuccess;
    NvError status = NvSuccess;
    NvU32 uStream = pPayloadData->pPayload->uStreamNumber;
    NvBool FallBackOnOriginalBuff = NV_FALSE;
    AsfBufferPropsType *pBuffer = NULL;
    NvU8 *destBuffer = NULL, **destStart = NULL;
    CAsf *asf = (CAsf *)data;
    NvU64 CurrPos;

    NvDrmContextHandle  pDrmContext = asf->pDrmContext ;

    if(uStream == (NvU32)asf->m_oVideoStreamNumber) {
        pBuffer = asf->NvAsfVideoBuffer;
        destStart = &asf->m_pVidStart;
        asf->VideoOnHold = NV_TRUE;
    } else if(uStream == (NvU32)asf->m_oAudioStreamNumber) {
        pBuffer = asf->NvAsfAudioBuffer;
        destStart = &asf->m_pAudStart;
    } else {
        pBuffer = NULL;
        destStart = NULL;
    }

    if(!pBuffer) {
        err = asf->pPipe->GetPosition64(asf->cphandle, &CurrPos);
        if(err == NvSuccess) {
            er = AsfOnAsfParserSetOffset(asf, CurrPos + pPayloadData->uPayloadDataSize);
            *puRead = pPayloadData->uPayloadDataSize;
            er = RESULT_OK_BREAK;
            return er;
        } else
            return RESULT_FAIL;
    }
    if(pPayloadData->bMediaObjectStart) {
        if(pPayloadData->pPayload->uMediaObjectLength + 8 + pBuffer->nFilledLen >
            pBuffer->nAllocLen) {
            return RESULT_OUT_OF_MEMORY;
        }

        if((asf->Vc1AdvProfile) && (uStream == (NvU32)asf->m_oVideoStreamNumber)) {
            NvU8 frameLayer[4];
            NvU32 TempVal;

            NvOsMemcpy(frameLayer,*pSrcBuffer,4);
            TempVal = ((frameLayer[0] << 24) | (frameLayer[1] << 16) | (frameLayer[2] << 8) |(frameLayer[3]));
            // Check whether first 4 bytes of payload data contain the start code of frame/EntryPoint
            if((TempVal == 0x0000010D) || (TempVal == 0x0000010E)) {
                NV_DEBUG_PRINTF(("First 4 bytes is start code of frame"));
            } else {
            // else case add the first 4 bytes as a start code of the frame
                NvU32 *Magic = (NvU32*)frameLayer;
                *Magic = 0x0D010000;
                NvOsMemcpy(pBuffer->pBuffer + pBuffer->nFilledLen, frameLayer, 4);
                pBuffer->nFilledLen += 4;
            }
        }
        // Check for the condition if presentation time is less than preroll time
        if(pPayloadData->uPresentationTime < (NvU32)asf->m_oPreRoll) {
            asf->m_oPreRoll = 0;
        }

        pBuffer->nTimeStamp = (NvU64)((pPayloadData->uPresentationTime - asf->m_oPreRoll) * 10000);
        *destStart = pBuffer->pBuffer + pBuffer->nFilledLen;
    }

    if(pPayloadData->uPayloadDataSize + pBuffer->nFilledLen > pBuffer->nAllocLen)
        return RESULT_OUT_OF_MEMORY;

    destBuffer = *destStart;
    if(!destBuffer)
        destBuffer = pBuffer->pBuffer;

    if((pPayloadData->uMediaObjectOffset + pPayloadData->uPayloadDataSize) < pBuffer->nAllocLen) {
            NvOsMemcpy(&destBuffer[pPayloadData->uMediaObjectOffset],*pSrcBuffer,pPayloadData->uPayloadDataSize);

        pBuffer->nFilledLen += pPayloadData->uPayloadDataSize;
        *pSrcBuffer += pPayloadData->uPayloadDataSize;
        *puRead = pPayloadData->uPayloadDataSize;

    } else {
        pPayloadData->bMediaObjectEnd = 1;
    }

    if(pPayloadData->bMediaObjectEnd) {
        if(uStream == (NvU32)asf->m_oVideoStreamNumber) {
            if(pPayloadData->pPayload->bKeyFrame)
                asf->pWorkingOutputVideoBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_KeyFrame;
            asf->m_bDoneVideoPacket = 1;
            return RESULT_OK_BREAK;
        } else if(uStream == (NvU32)asf->m_oAudioStreamNumber) {
            asf->m_bDoneAudioPacket = 1;
            return RESULT_OK_BREAK;
        }
    }
    return er;
}

#ifdef __cplusplus
}
#endif

} // namespace android

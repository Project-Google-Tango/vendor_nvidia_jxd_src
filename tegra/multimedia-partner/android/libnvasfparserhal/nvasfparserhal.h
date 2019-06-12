/*
* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_ASF_PARSER_HAL_H
#define INCLUDED_ASF_PARSER_HAL_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/NvParserHalDefines.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <utils/List.h>
#include <avc_utils.h>
#include <NVOMX_IndexExtensions.h>
#include "nvparserhal_content_pipe.h"

namespace android {

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmm.h"
#include "nvmm_asfparser_api.h"

typedef NvAsfResult (*CreateNvAsfParserFunc) (
    CNvAsfParser **parser);
typedef void (*DestroyNvAsfParserFunc) (
    CNvAsfParser *parser);
typedef NvAsfResult (*NvAsfOpenFunc) (
    CNvAsfParser *parser,
    NvAsfCallbacks *pCallback,
    void *pCallbackData);
typedef NvAsfResult (*NvAsfParseFunc) (
    NvMMAsfParserCoreContext * pContext);
typedef NvAsfResult (*NvAsfVideoNewSeekTimeFunc) (
    NvMMAsfParserCoreContext* pContext,
    NvU64* timestamp,
    NvU64 *NewSeekTime,
    NvS32 direction);
typedef NvAsfResult (*NvAsfAudioNewSeekTimeFunc) (
    NvMMAsfParserCoreContext* pContext,
    NvU64 CurrentTimeStamp,
    NvU64 *NewSeekTime);
typedef NvAsfResult (*NvAsfFindLargestIframeFunc) (
    NvMMAsfParserCoreContext* pContext);
typedef void (*NvAsfFillInDefaultCallbacksFunc) (
    NvAsfCallbacks *pCallbacks);

void NvAsfParserHal_GetInstanceFunc(NvParseHalImplementation* halImpl);
status_t NvAsfParserHal_Create(char* Url, void** pAsfParserHalHandle);
status_t NvAsfParserHal_Destroy(void* asfParserHalHandle);
size_t NvAsfParserHal_GetStreamCount(void* asfParserHalHandle);
status_t NvAsfParserHal_GetMetaData(void* asfParserHalHandle, sp<MetaData> mFileMetaData);
status_t NvAsfParserHal_GetTrackMetaData(void* asfParserHalHandle, int32_t streamIndex, bool setThumbnailTime, sp<MetaData> mTrackMetaData);
status_t NvAsfParserHal_SetTrackHeader(void* asfParserHalHandle, int32_t streamIndex, sp<MetaData> meta);
status_t NvAsfParserHal_Read(void* asfParserHalHandle, int32_t streamIndex, MediaBuffer* mBuffer, const MediaSource::ReadOptions* options);

#ifdef __cplusplus
}
#endif

} // namespace android

#endif // #ifndef INCLUDED_ASF_PARSER_HAL_H

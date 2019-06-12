/*
* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_AVI_PARSER_HAL_H
#define INCLUDED_AVI_PARSER_HAL_H

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
#include "nvmm_aviparser.h"
#include "nvmm_aviparser_core.h"
#include "nvmm_aviparser_defines.h"

typedef NvError (*NvAviCoreParserSeekToAudioFrame) (
    NvAviParser *pAviParser,
    NvU32 DesiredPacketToSeek,
    NvU32 StreamIndex,
    NvU32 *PacketNumber,
    NvU32 *TotalPackets);

typedef NvError (*AviCoreParserGetNextSyncUnit) (
    NvAviParser * pAviParser,
    NvU32 *frameToSeek,
    NvS32 rate,
    NvS32 direction,
    NvU32 StreamIndex);

typedef NvError (*NvAviCoreParserDemux) (
    NvAviParser *pAviParserHandle);

typedef NvError (*NvAviCoreParserNextWorkUnit) (
    NvAviParser *pAviParser ,
    NvU32 StreamIndex,
    NvMMBuffer *pBuffer,
    NvU32 *Size,
    NvS32 Rate,
     NvBool bEnableUlpMode);

typedef NvError (*NvAviCoreParserInit_HAL) (
    NvAviParser *pAviParser,
    NvString pFileName);

typedef void (*NvAviCoreParserDeInit) (
    NvAviParser * pAviParser);

typedef NvU32 (*AviCoreParserGetdVideowSuggestedBufferSize) (
     NvAviParser *pAviParser);

void NvAviParserHal_GetInstanceFunc(NvParseHalImplementation* halImpl);
status_t NvAviParserHal_Create(char* Url, void** pAviParserHalHandle);
status_t NvAviParserHal_Destroy(void* aviParserHalHandle);
size_t NvAviParserHal_GetStreamCount(void* aviParserHalHandle);
status_t NvAviParserHal_GetMetaData(void* aviParserHalHandle, sp<MetaData> mFileMetaData);
status_t NvAviParserHal_GetTrackMetaData(void* aviParserHalHandle, int32_t streamIndex, bool setThumbnailTime, sp<MetaData> mTrackMetaData);
status_t NvAviParserHal_SetTrackHeader(void* aviParserHalHandle, int32_t streamIndex, sp<MetaData> meta);
status_t NvAviParserHal_Read(void* aviParserHalHandle, int32_t streamIndex, MediaBuffer *mBuffer, const MediaSource::ReadOptions* options);
bool NvAviParserHal_IsSeekable(void* aviParserHalHandle);

#ifdef __cplusplus
}
#endif

} // namespace android

#endif // #ifndef INCLUDED_AVI_PARSER_HAL_H

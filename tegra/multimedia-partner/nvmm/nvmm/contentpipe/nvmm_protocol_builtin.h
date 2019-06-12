/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVMM_PROTOCOL_BUILTIN_H_
#define NVMM_PROTOCOL_BUILTIN_H_

#include "nvcustomprotocol.h"

void NvGetLocalFileProtocol(NV_CUSTOM_PROTOCOL **proto);
void NvGetHTTPProtocol(NV_CUSTOM_PROTOCOL **proto);
void NvGetRTSPProtocol(NV_CUSTOM_PROTOCOL **proto);

#ifdef BUILD_DROID_PROTO
void NvGetDroidProtocol(NV_CUSTOM_PROTOCOL **proto);
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ANDROID
void NvGetStagefrightProtocol(NV_CUSTOM_PROTOCOL **proto);
#endif

#ifdef __cplusplus
}
#endif

#endif

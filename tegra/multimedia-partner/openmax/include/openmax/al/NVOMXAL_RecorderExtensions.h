/* Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * This is a header file for any NV Recorder extension we are going to add.
 *
 */

#ifndef NVOMXAL_RECORDEREXTENSIONS_H_
#define NVOMXAL_RECORDEREXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Burst Modes

#define NVXA_BURSTMODE_BURST_CAPTURE       ((XAuint32) 0x00000001)
#define NVXA_BURSTMODE_EV_BRACKET_CAPTURE  ((XAuint32) 0x00000002)
#define NVXA_BURSTMODE_NSL_CAPTURE         ((XAuint32) 0x00000003)

// XA_RECORDEVENT nvidia extensions
#define NVXA_RECORDEVENT_HEADATEND         ((XAuint32) 0x10000000)

typedef struct NVXABurstCaptureSettings_ {
    XAuint32 mBurstCount;
    XAuint32 mSkipCount;
} NVXABurstCaptureSettings;

typedef struct NVXAEvBracketCaptureSettings_ {
    XAuint32 mBurstCount;
    float *pEvArray;
} NVXAEvBracketCaptureSettings;

typedef struct NVXANslCaptureSettings_ {
    XAuint32 mBurstCount;
    XAuint32 mSkipCount;
    XAuint32 mBufferCount;
} NVXANslCaptureSettings;

XA_API extern const XAInterfaceID XA_IID_NVBURSTMODE;

struct NVXANvBurstModeItf_;
typedef const struct NVXANvBurstModeItf_
    * const * NVXANvBurstModeItf;

struct NVXANvBurstModeItf_ {
    // Burst mode
    XAresult (*getBurstSettings) (
        NVXANvBurstModeItf self,
        XAuint32 burst_mode,
        void * pSettings
    );

    XAresult (*setBurstSettings) (
        NVXANvBurstModeItf self,
        XAuint32 burst_mode,
        void *pSettings
    );

};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_RECORDEREXTENSIONS_H_ */


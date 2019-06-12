/* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 *
 * This is the NVIDIA OpenMAX AL Data Tap extensions interface.
 *
 */

#ifndef NVOMXAL_DATATAPEXTENSIONS_H_
#define NVOMXAL_DATATAPEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NVXA_OBJECTID_DATATAP ((XAuint32) 0x0000000a)

/** Data Tap type is the default data-path. */
#define NVXA_DATATAPTYPE_DEFAULT ((XAuint32) 0x00000001)

#define MAX_DATATAPPOINTS   8
#define MAX_DATATAPFORMATS_SUPPORTED 256

typedef union {
    XAuint32 mFormatType;
    XADataFormat_PCM mPCM;
    XADataFormat_MIME mMIME;
    XADataFormat_RawImage mRawImage;
} NVXADataFormat;

/** Capabilities */
typedef struct NvXADataTapPointDescriptor_
{
    XAuint32 mType; // NVXA_DATATAPTYPE_DEFAULT and Other values..
    XAuint32 mNumFormats;  // 0 - means, this dataTap point is not valid one.
    NVXADataFormat mFormat[MAX_DATATAPFORMATS_SUPPORTED];
} NVXADataTapPointDescriptor;


/** Data Tap Creation Parameters */
typedef struct NVXADataTapCreationParameters_ {
       XAObjectItf mediaObject;
       XAuint32 dataTapPointIndex;
       XAuint32 formatIndex;
} NVXADataTapCreationParameters;

/** PULL AL Interface */
XA_API extern const XAInterfaceID XA_IID_NVPULLAL;

struct NVXAPullALItf_;
typedef const struct NVXAPullALItf_
    * const * NVXAPullALItf;

struct NVXAPullALItf_ {
    XAresult (*PullDataFromAL) (
        NVXAPullALItf self,
        const XADataSink * pBuffer,
        void * pBufferMetaData
    );
};

/** PULL APP Interface */
XA_API extern const XAInterfaceID XA_IID_NVPULLAPP;

struct NVXAPullAppItf_;
typedef const struct NVXAPullAppItf_
    * const * NVXAPullAppItf;

typedef void (XAAPIENTRY * nvxaPullDataFromAppCallback) (
    NVXAPullAppItf caller,
    void * pContext,
    const XADataSink * pBuffer,
    void * pBufferMetaData
);

struct NVXAPullAppItf_ {
    XAresult (*RegisterPullDataFromAppCallback) (
        NVXAPullAppItf self,
        nvxaPullDataFromAppCallback callback,
        void *pContext
    );
};

/** PUSH AL Interface */
XA_API extern const XAInterfaceID XA_IID_NVPUSHAL;

struct NVXAPushALItf_;
typedef const struct NVXAPushALItf_
    * const * NVXAPushALItf;

struct NVXAPushALItf_ {
    XAresult (*PushDataToAL) (
        NVXAPushALItf self,
        const XADataSink * pBuffer,
        void * pBufferMetaData
    );
};

/** PUSH APP Interface */
XA_API extern const XAInterfaceID XA_IID_NVPUSHAPP;

struct NVXAPushAppItf_;
typedef const struct NVXAPushAppItf_
    * const * NVXAPushAppItf;

typedef void (XAAPIENTRY * nvxaPushDataToAppCallback) (
    NVXAPushAppItf caller,
    void * pContext,
    const XADataSink * pBuffer,
    void * pBufferMetaData
);

struct NVXAPushAppItf_ {
    XAresult (*RegisterPushDataToAppCallback) (
        NVXAPushAppItf self,
        nvxaPushDataToAppCallback callback,
        void *pContext
    );
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_DATATAPEXTENSIONS_H_ */

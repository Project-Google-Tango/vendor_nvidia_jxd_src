/* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * This is the NVIDIA OpenMAX AL Video extensions interface.
 *
 */

#ifndef NVOMXAL_VIDEOEXTENSIONS_H_
#define NVOMXAL_VIDEOEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NVXA_PROFILEIOP_CONSTRAINTS_SET0 ((XAuint32) 0x00000080)
#define NVXA_PROFILEIOP_CONSTRAINTS_SET1 ((XAuint32) 0x00000040)
#define NVXA_PROFILEIOP_CONSTRAINTS_SET2 ((XAuint32) 0x00000020)
#define NVXA_PROFILEIOP_CONSTRAINTS_SET3 ((XAuint32) 0x00000010)

#define NVXA_STREAMMODE_NAL              ((XAuint32) 0x00000001)
#define NVXA_STREAMMODE_BYTESTREAM       ((XAuint32) 0x00000002)

#define NVXA_APPLICATIONTYPE_DEFAULT           ((XAuint32) 0x00000001)
#define NVXA_APPLICATIONTYPE_VIDEOTELEPHONY    ((XAuint32) 0x00000002)

typedef struct NVXAVideoCodecExtDescriptor_AVC_ {
    XAuint32 codecId;
    XAuint32 profileIopSetting;
    XAuint32 profileSetting;
    XAuint32 levelSetting;
    XAuint32 streamMode;
    XAuint32 applicationType;
    XAuint32 minWidth;
    XAuint32 minHeight;
} NVXAVideoCodecExtDescriptor_AVC;

typedef struct NVXAVideoSettings_AVC_ {
    XAuint32 codecId;
    XAuint32 profileSetting;
    XAuint32 levelSetting;
    XAuint32 streamMode;
    XAuint32 applicationType;
} NVXAVideoSettings_AVC;

typedef struct NVXAVideoExtSettings_AVC_ {
    XAuint32 codecId;
    XAuint32 profileIopSetting;
    XAuint32 profileSetting;
    XAuint32 levelSetting;
    XAuint32 streamMode;
    XAuint32 applicationType;
    XAuint32 packetSize;
    XAuint32 minQPI;
    XAuint32 maxQPI;
    XAuint32 minQPP;
    XAuint32 maxQPP;
    XAuint32 minQPB;
    XAuint32 maxQPB;
} NVXAVideoExtSettings_AVC;

/** Video Encoder Extended Capabilities Interface */
XA_API extern const XAInterfaceID XA_IID_NVVIDEOENCODEREXTCAPABILITIES;

struct NVXAVideoEncoderExtCapabilitiesItf_;
typedef const struct NVXAVideoEncoderExtCapabilitiesItf_
    * const * NVXAVideoEncoderExtCapabilitiesItf;

struct NVXAVideoEncoderExtCapabilitiesItf_ {
    XAresult (*GetVideoEncoderExtCapabilities) (
        NVXAVideoEncoderExtCapabilitiesItf self,
        XAuint32 encoderId,
        XAuint32 * pIndex,
        void * pDescriptor
    );
};

/** Video Decoder Extended Capabilities Interface */
XA_API extern const XAInterfaceID XA_IID_NVVIDEODECODEREXTCAPABILITIES;

struct NVXAVideoDecoderExtCapabilitiesItf_;
typedef const struct NVXAVideoDecoderExtCapabilitiesItf_
    * const * NVXAVideoDecoderExtCapabilitiesItf;

struct NVXAVideoDecoderExtCapabilitiesItf_ {
    XAresult (*GetVideoDecoderExtCapabilities) (
        NVXAVideoDecoderExtCapabilitiesItf self,
        XAuint32 decoderId,
        XAuint32 * pIndex,
        void * pDescriptor
    );
};

/** Video Decoder Interface */
XA_API extern const XAInterfaceID XA_IID_NVVIDEODECODER;

struct NVXAVideoDecoderItf_;
typedef const struct NVXAVideoDecoderItf_
    * const * NVXAVideoDecoderItf;

struct NVXAVideoDecoderItf_ {
    XAresult (*SetVideoSettings) (
        NVXAVideoDecoderItf self,
        void * pDescriptor
    );
    XAresult (*GetVideoSettings) (
        NVXAVideoDecoderItf self,
        void * pDescriptor
    );
};

/** Video Encoder Extended Interface */
XA_API extern const XAInterfaceID XA_IID_NVVIDEOENCODEREXT;

struct NVXAVideoEncoderExtItf_;
typedef const struct NVXAVideoEncoderExtItf_
    * const * NVXAVideoEncoderExtItf;

struct NVXAVideoEncoderExtItf_ {
    XAresult (*SetVideoExtSettings) (
        NVXAVideoEncoderExtItf self,
        void * pDescriptor
    );
    XAresult (*GetVideoExtSettings) (
        NVXAVideoEncoderExtItf self,
        void * pDescriptor
    );
    XAresult (*RequestKeyFrame) (
        NVXAVideoEncoderExtItf self
    );
    XAresult (*RequestRecoveryFrame) (
        NVXAVideoEncoderExtItf self
    );
    XAresult (*SetBitrate) (
        NVXAVideoEncoderExtItf self,
        XAuint32 bitRate
    );
    XAresult (*SetFramerate) (
        NVXAVideoEncoderExtItf self,
        XAuint32 frameRate
    );
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_VIDEOEXTENSIONS_H_ */

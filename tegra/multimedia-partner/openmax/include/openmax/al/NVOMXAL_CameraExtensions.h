/* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * This is a header file for any NV Camera extension we are going to add.
 *
 */

#ifndef NVOMXAL_CAMERAEXTENSIONS_H_
#define NVOMXAL_CAMERAEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

//Additional Focus Modes
#define NVXA_CAMERA_FOCUSMODE_INFINITY          ((XAuint32) 0x00000020)
#define NVXA_CAMERA_FOCUSMODE_MACRO             ((XAuint32) 0x00000040)
#define NVXA_CAMERA_FOCUSMODE_FIXED             ((XAuint32) 0x00000080)
#define NVXA_CAMERA_FOCUSMODE_EDOF              ((XAuint32) 0x00000100)
#define NVXA_CAMERA_FOCUSMODE_CONTINUOUS_VIDEO  ((XAuint32) 0x00000200)

//Additional Whitebalance Settings
#define NVXA_CAMERA_WHITEBALANCEMODE_WARM_FLUORESCENT    ((XAuint32) 0x00000400)

//Additional Exposure Modes
#define NVXA_CAMERA_EXPOSUREMODE_ACTION         ((XAuint32) 0x00001000)
#define NVXA_CAMERA_EXPOSUREMODE_LANDSCAPE      ((XAuint32) 0x00002000)
#define NVXA_CAMERA_EXPOSUREMODE_THEATRE        ((XAuint32) 0x00004000)
#define NVXA_CAMERA_EXPOSUREMODE_SUNSET         ((XAuint32) 0x00008000)
#define NVXA_CAMERA_EXPOSUREMODE_STEADYPHOTO    ((XAuint32) 0x00010000)
#define NVXA_CAMERA_EXPOSUREMODE_FIREWORKS      ((XAuint32) 0x00020000)
#define NVXA_CAMERA_EXPOSUREMODE_PARTY          ((XAuint32) 0x00040000)
#define NVXA_CAMERA_EXPOSUREMODE_CANDLELIGHT    ((XAuint32) 0x00080000)
#define NVXA_CAMERA_EXPOSUREMODE_BARCODE        ((XAuint32) 0x00100000)

typedef struct NVXACameraResolutionDescriptor_ {
    XAuint32 width;
    XAuint32 height;
} NVXACameraResolutionDescriptor;

typedef struct NVXACameraPreviewResolutionDescriptor_ {
    XAuint32 previewwidth;
    XAuint32 previewheight;
} NVXACameraPreviewResolutionDescriptor;

/** Camera Extended Capabilities Interface */
XA_API extern const XAInterfaceID XA_IID_NVCAMERAEXTCAPABILITIES;

struct NVXACameraExtCapabilitiesItf_;
typedef const struct NVXACameraExtCapabilitiesItf_
    * const * NVXACameraExtCapabilitiesItf;

struct NVXACameraExtCapabilitiesItf_ {
    XAresult (*GetSupportedResolutions) (
        NVXACameraExtCapabilitiesItf self,
        XAuint32 cameraDeviceID,
        XAuint32 * pIndex,
        NVXACameraResolutionDescriptor * pDescriptor
    );

    XAresult (*GetSupportedPreviewResolutions) (
        NVXACameraExtCapabilitiesItf self,
        XAuint32 cameraDeviceID,
        XAuint32 * pIndex,
        NVXACameraPreviewResolutionDescriptor * pDescriptor
    );
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_CAMERAEXTENSIONS_H_ */

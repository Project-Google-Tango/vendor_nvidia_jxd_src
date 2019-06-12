/* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * This is the NVIDIA OpenMAX AL Capabilities extensions
 * interface
 *
 */

#ifndef NVOMXAL_CAPABILITIESEXTENSIONS_H_
#define NVOMXAL_CAPABILITIESEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Capabilities Defines                                                      */
/*---------------------------------------------------------------------------*/

// Version 1 CapabilityType are returned as a XAuint32 pointed to by pCapabilities
// CapabilityType
#define XA_GETCAPSOFINTERFACE_V1  1

// Capabilities
#define XA_GETCAPSOFINTERFACE_SEAMLESS_FORMAT_CHANGE    0x00000001
#define XA_GETCAPSOFINTERFACE_ES_RAW_H264_NAL           0x00000002
#define XA_GETCAPSOFINTERFACE_ES_RAW_AAC_ADTS           0x00000004
#define XA_GETCAPSOFINTERFACE_ES_MUXED                  0x00000008
#define XA_GETCAPSOFINTERFACE_ES_ENCRYPTED              0x00000010
#define XA_GETCAPSOFINTERFACE_ES_RAW_PCM                0x00000020

// new CapabilityType are returned as NvDataTapPointCaps  pointed to by pCapabilities
// CapabilityType
#define XA_GETCAPSOFINTERFACE_V1_DATATAP              2
// Notes: caller of itf, using this CapabilityType XA_GETCAPSOFINTERFACE_V1_DATATAP,
// will send array of NvXADataTapPointDescriptor of size MAX_DATATAPPOINTS, which will be filled by itf.

/*---------------------------------------------------------------------------*/
/* Capabilities Interface                                                    */
/*---------------------------------------------------------------------------*/
XA_API extern const XAInterfaceID XA_IID_GETCAPABILITIESOFINTERFACE;

struct XAGetCapabilitiesOfInterfaceItf_;
typedef const struct XAGetCapabilitiesOfInterfaceItf_
    * const * XAGetCapabilitiesOfInterfaceItf;

struct XAGetCapabilitiesOfInterfaceItf_ {
    XAresult (*GetCapabilities) (
        XAGetCapabilitiesOfInterfaceItf self,  // engine's pointer to its instantiation querying interface
        XAObjectItf pObject,                   // instance of the object interface for the object about which we're querying
        const XAInterfaceID InterfaceID,       // interface ID of the requested interface of pObject
        XAuint32 CapabilityType,               // version of the capabilities
        void * pCapabilities                   // pointer to the capabilities data
    );
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_CAPABILITIESEXTENSIONS_H_ */

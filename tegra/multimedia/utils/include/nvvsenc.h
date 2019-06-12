/*
* Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

#ifndef __NV_VIDEO_STREAM_ENCODER_API_H__
#define __NV_VIDEO_STREAM_ENCODER_API_H__

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "nvvscommon.h"

#ifdef __cplusplus
extern "C" {
#endif

// Handle to Video Encoder Device
typedef void* NvVideoStreamEnc;

// Enumeration for video encoders
typedef enum {
    NV_ENCODER_TYPE_H264,

    /* Below are not implemented */
    NV_ENCODER_TYPE_VC1,
    NV_ENCODER_TYPE_RLE,
    NV_ENCODER_TYPE_ZLIB,
    NV_ENCODER_TYPE_FORCE_32 = 0x7FFFFFFF
} NvEncoderType;

// Enumeration for different supported picture types
typedef enum {
    NV_PICTURE_FORMAT_RGBA,     /* Interleaved RGBA each of 8 bits */
    NV_PICTURE_FORMAT_FORCE_32 = 0x7FFFFFFF
} NvPictureFormat;

typedef enum {
    NV_ENCODER_STREAM_TYPE_H264_NAL,   /* H264 NAL stream */
    NV_ENCODER_STREAM_TYPE_H264_TS,    /* H264 TS stream */
} NvStreamType;

// Enumeration for encoder message
typedef enum {
    NV_ENCODER_MESSAGE_PACKET_READY,   /* Valid packet is available */
    NV_ENCODER_MESSAGE_ERROR,          /* Processing error */
} NvMessageType;


// Structure prototype for H264 specific encoder attributes
typedef struct NvEncoderAttributesH264_t{
    unsigned short  uFramesPerGOP;   /* Set FramesPerGOP to 1 to encode each frame
                                        as I-frame, else I+P BP encode. */
    unsigned int    uBytesPerFrame;  /* BytesPerGOP = BytesPerFrame * FramesPerGOP.
                                        This acts as a guidance value, the actual
                                        number of encoded bits will be different */
} NvEncoderAttributesH264;

// Structure prototype for VC1  specific encoder attributes
// Not implemented
typedef struct NvEncoderAttributesVC1_t{
} NvEncoderAttributesVC1;

// Structure prototype for RLE  specific encoder attributes
// Not implemented
typedef struct NvEncoderAttributesRLE_t{
} NvEncoderAttributesRLE;

// Structure prototype for ZLIB  specific encoder attributes
// Not implemented
typedef struct NvEncoderAttributesZLIB_t{
} NvEncoderAttributesZLIB;

// The following represents configuration paramters specific to each compression method
typedef union NvEncoderAttributes_t{
    NvEncoderAttributesH264 AttributesH264;
    NvEncoderAttributesVC1  AttributesVc1;
    NvEncoderAttributesRLE  AttributesRle;
    NvEncoderAttributesZLIB AttributesZlib;
} NvEncoderAttributes;

// Structure prototype for selecting a rectangle on a picture frame
typedef struct NvVideoRect_t{
    unsigned short    uX0;    /*   left edge inclusive */
    unsigned short    uY0;    /*    top edge inclusive */
    unsigned short    uX1;    /*  right edge exclusive */
    unsigned short    uY1;    /* bottom edge exclusive */
} NvVideoRect;

// Structure prototype for encoder message packet
typedef struct NvEncodePacket_t {
    unsigned int   uSize;
    unsigned char *pBuffer;
    void          *hAppHandle;
    NvStreamType   eStreamType;
    NvMessageType  eMessageType;
} NvEncodePacket;

// ICallback:
//
//      Callback interface for message from encoder.
//          pPacket->messageType indicates type of message.
//          NV_ENCODER_MESSAGE_PACKET_READY: A packet of type pPacket->streamType
//          of size pPacket->size poinyrt to by buffer pPacket->pBuffer
//
//          NV_ENCODER_MESSAGE_ERROR: An error has occured while processing
//
//
typedef void (*NvVideoStreamEncCallback)(NvVideoStreamEnc hEncoder, NvEncodePacket *pPacket);
typedef NvVSResult (*NvVideoStreamProcessingCallback)(void *hAppHandle,
                            unsigned char   *pInpBuf, unsigned int   uInpSize,
                            unsigned char **ppOutBuf, unsigned int *puOutSize);

typedef struct NvEncoderParam_t {
    unsigned short      uSrcStrideBytes; /* Input Picture Stride in bytes */
    unsigned short      uSrcWidth;       /* Input Picture Width */
    unsigned short      uSrcHeight;      /* Input Picture Height */
    NvVideoRect         oSrcRect;        /* Select a rectangle to be encoded in the source */
    unsigned short      uEncWidth;       /* Picture width of encoded picture, if different from
                                          * min(uSrcWidth, (rect.X1 - rect.X0)) then resampled */
    unsigned short      uEncHeight;      /* Picture height of encoded picture */
    NvPictureFormat     ePictureType;    /* Picture type */
    NvStreamType        eStreamType;     /* Stream type */
    NvEncoderAttributes oAttributes;     /* Encoder Attributes */
    void                *hAppHandle;     /* Opaque application handle */

    unsigned int        uTsOutputBufSize; /* Output buffer size of ts muxer */
    unsigned int        uChannelDataRate; /* Input channel data rate */
    unsigned int        uEncoderDataRate; /* Input encoder data rate, it should be less then channel
                                           * data rate */
    unsigned int        uMaintainMuxRate; /* To enable NULL packet stuffing, set to 1 */
    unsigned int        uApplyProcessing; /* To enable h264 stream processing, set to 1 */

    NvVideoStreamProcessingCallback pProcessingCB; /* h264 processing callback function  */
    NvVideoStreamEncCallback        pCallback;     /* Callback message handler */
} NvEncoderParam;

// NvVideoStreamEncoderOpen:
//      Create a new encoder object of specified encoder type.
//
//  eType
//      Encoder type. Currently only NV_ENCODER_TYPE_H264 is supported.
//  pEncoderParam
//      Parameters for the corresponding encoder type.
//  pEncoder
//      Pointer to encoder handle
//
//  Returns NVVSRESULT_FAIL if unsuccessful.
//
NvVSResult NvVideoStreamEncoderOpen(NvEncoderType eType,
                NvEncoderParam *pEncoderParam, NvVideoStreamEnc *pEncoder);

// NvVideoStreamEncoderClose:
//      Close the video encoder.
//
//  hEncoder
//      Encoder handle
//
//  Returns NVVSRESULT_OK or NVVSRESULT_FAIL
//
NvVSResult NvVideoStreamEncoderClose(NvVideoStreamEnc hEncoder);

// NvVideoStreamEncoderSetAttributes:
//      Run-time update of encoder parameters - frames per GOP and
//      byte per frame.
//
//  hEncoder
//      Encoder handle
//  pAttributes
//      These go into effect only at the start of the next GOP.
//      This is defined only for H264 (and VC1 in future) and it
//      has no impact on lossless encoders like ZLIB, PNG or RLE.
//
//  Returns NVVSRESULT_OK or NVVSRESULT_FAIL or NVVSRESULT_BAD_PARAMETER
//
NvVSResult NvVideoStreamEncoderSetAttributes(NvVideoStreamEnc hEncoder,
                NvEncoderAttributes *pAttributes);

// NvVideoStreamEncoderEncodeFrame:
//      NvVideoStreamEncoderEncodeFrame() is a non-blocking call so
//      that multiple frames can be enqueued.
//
//      For H264, since only I and P frames are used the frame
//      input order is the frame output order.
//
//  pEGLBuffer
//      Input Video Frame to be encoded.
//  ubForceIFrame
//      If encoding parameters needed to be changed it is recommended
//      force an I frame,so that changes take effect right away.
//      This is ignored for encoders like RLE, ZLIB, PNG.
//
//  Returns NVVSRESULT_OK or NVVSRESULT_FAIL or NVVSRESULT_INSUFFICIENT_BUFFERING.
//
NvVSResult NvVideoStreamEncoderEncodeFrame(NvVideoStreamEnc hEncoder,
                EGLImageKHR pEGLBuffer, unsigned int ubForceIFrame);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NV_VIDEO_STREAM_ENCODER_API_H__ */


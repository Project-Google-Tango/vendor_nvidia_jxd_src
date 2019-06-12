/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __NV_VIDEO_STREAM_DECODER_API_H__
#define __NV_VIDEO_STREAM_DECODER_API_H__

#include "nvvscommon.h"

#ifdef __cplusplus
extern "C" {
#endif

// Handle to Video Decoder Device
typedef void* NvVideoStreamDec;

// Enumeration for stream types
typedef enum {
    NV_DECODER_STREAM_TYPE_H264_NAL,
    NV_DECODER_STREAM_TYPE_TRANSPORT_STREAM,
    NV_DECODER_STREAM_TYPE_VC1,
    NV_DECODER_STREAM_TYPE_RLE,
    NV_DECODER_STREAM_TYPE_ZLIB,
    NV_DECODER_STREAM_TYPE_NOT_SUPPORTED,
    NV_DECODER_STREAM_TYPE_FORCE_32 = 0x7FFFFFFF
} NvStreamType;

// Enumeration for decoder message
// The messages NV_DECODER_MESSAGE_AVP_HANG and NV_DECODER_MESSAGE_TS_FATAL_ERROR
// are not used with T30, but exist to maintain T20 compatibility
typedef enum {
    NV_DECODER_MESSAGE_ERROR,            /* An error has occured while decoding */
    NV_DECODER_MESSAGE_READY,            /* Decoder is Ready to accept new buffers */
    NV_DECODER_MESSAGE_STREAM_INFO,      /* Stream format information is available */
    NV_DECODER_MESSAGE_AVP_HANG,         /* AVP is stuck */
    NV_DECODER_MESSAGE_TS_FATAL_ERROR,   /* TS parser cannot recover */
    NV_DECODER_MESSAGE_FORCE_32 = 0x7FFFFFFF
} NvDecoderMessageType;

// Enumeration for decoding profiles
typedef enum {
    NV_DECODER_PROFILE_SMOOTH,
    NV_DECODER_PROFILE_LOW_LATENCY
} NvDecoderProfile;

typedef struct {
    NvDecoderMessageType eMessage; /* Message type */
    unsigned int uWidth;           /* Optional: Source width for decoded stream */
    unsigned int uHeight;          /* Optional: Source height for decoded stream */;
} NvDecoderMessage;

// Structure for aspect ratio
typedef struct {
    unsigned int uNumerator;     /* Aspect Ratio Numerator */
    unsigned int uDenominator;   /* Aspect Ratio Denomintor */
} NvStreamAspectRatio;

// Structure for decoder attributes
typedef struct {
    unsigned int        uUpdateAspect;      /* Aspect ratio control */
    NvStreamAspectRatio oAspectRatio;       /* Aspect ratio */

    unsigned int        uUpdateRects;       /* Cropping control */
    unsigned int        uXSrc, uYSrc;       /* Source Rectangle */
    unsigned int        uWSrc, uHSrc;
    unsigned int        uXDst, uYDst;       /* Destination Rectangle */
    unsigned int        uWDst, uHDst;

    unsigned int        uUpdateFrameRate;   /* Frame Rate Control */
    unsigned int        uFramerate;         /* Frame Rate of the stream */

    unsigned int        uUpdateColors;      /* Color Controls */
    float               fBrightness;        /* -1 < fBrightness < 1 */
    float               fContrast;          /*  0 < fContrast < 10 */
    float               fSaturation;        /*  0 < fSaturation < 10 */

    unsigned int        uUpdateDisplay;     /* Display control */
    unsigned int        uDisplayWidth;
    unsigned int        uDisplayHeight;

    unsigned int        uUpdateRenderState; /* Render control */
    unsigned int        uDisableRendering;  /* Disable Rendering */

    unsigned int        uUpdateProfile;     /* Quality profile */
    NvDecoderProfile    eProfile;

    unsigned char       uOverlayDepth;      /* Overlay Depth */
    unsigned int        uDisplayableId;     /* Display ID to be communicated to display manager */
    NvStreamType        eStreamType;        /* Stream type */

    unsigned int        uSetStreamInfo;     /* Initialise TS stream information */
    unsigned int        uVideoPID;          /* Set video to this fixed PID */
    unsigned int        uVideoType;         /* Set video to this fixed stream type */

    unsigned int        uConfigureBuffers;  /* Configure stream buffers */
    unsigned int        uPacketSize;        /* Size of a packet in bytes */
    unsigned int        uPacketsPerBuffer;  /* Number of packets per buffer */
    unsigned int        uNumBuffers;        /* Number of input buffers */
} NvDecoderAttributes;

// Callback Interface for messages from decoder
//
//  hDecoder
//      Decoder handle
//  eMessage
//      Message from decoder
//
typedef void (*NvVideoStreamDecCallback)(NvVideoStreamDec hDecoder,
                NvDecoderMessage *poMessage);

// NvVideoStreamDecoderOpen:
//      Create a new decoder object with specified message handler
//
//  pDecoderAttrib
//      Pointer to decoder attributes
//      if uUpdateAspect is set to 1, use oAspectRatio
//              uNumerator = uDenomiator = 0 scales source stream to
//              display rectangle while preserving aspect ratio
//      else if uUpdateRect set to 1, use source and destination rectangles
//              source rectangle should lie completely within
//               dimensions of the source stream
//              destination rectangle should lie completely within
//               dimensions of display rectangle.
//              if either of source or destination rectangles are invalid
//               both of these are ignored.
//      else, use default settings, uNumerator = uDenominator = 0
//
//      if uUpdateFrameRate is set to 1 and if uFrameRate != 0, use uFrameRate
//      else, use default (PTS or uFrameRate = 60).
//
//      if uUpdateColors is set to 1
//          use colors clamped to range as:
//             -1 < fBrightness < 1, 0 < fContrast < 10,
//              0 < fSaturation < 10
//      else, use default, fBrightness = 0,
//          fContrast = 1, fSaturation = 1
//
//      if uUpdateDisplay is set to 1
//          use display params if these are within range:
//          0 < uDisplayWidth < 1920
//          0 < uDisplayHeight < 1080
//      else use defaults,
//          uDisplayWidth = 800
//          uDisplayHeight = 480
//
//      if uUpdateRenderState is set to 1
//           set uDisableRendering to 1 to disable the rendering
//      else Rendering will be enabled
//      default Rendering is enabled i.e. uDisableRendering is set to 0
//
//      if uUpdateProfile is set to 1
//          use eProfile to set a decoding profile:
//              NV_DECODER_PROFILE_SMOOTH
//                  Decoder renders the video as smooth as possible.
//                  This requires some buffering which causes some delay.
//              NV_DECODER_PROFILE_LOW_LATENCY
//                  Decoder renders new frames as soon as possible.
//                  This reduces latency, but may cause stuttering.
//
//      if uSetStreamInfo is set to 1
//          use uVideoPID and uVideoType to initialise TS to default values.
//          This will allow the TS parser to decode video data with PID uVideoPID
//          and type uVideoType at any point in a stream without waiting for
//          PAT/PMT information.
//      else
//          TS parser will extract stream information from PAT/PMT and ignore
//          uVideoPID and uVideoType.
//
//      if uConfigureBuffers is set to 1
//          use buffer parameters to configure internal buffers with
//              uPacketSize > 0         (TS: 188 bytes)
//              uPacketsPerBuffer > 0
//              uNumBuffers > 0
//      else
//          use default values for TS stream with 20 Mbit/s
//
//  hCallback
//      Callback for recieving notifications (Error Conditions, Decoder Ready
//      to accept new buffers)
//  pDecoder
//      Pointer to decoder handle
//
//  Returns NVVSRESULT_FAIL if unsuccessful.
//
NvVSResult NvVideoStreamDecoderOpen(NvDecoderAttributes *pDecoderAttrib,
                NvVideoStreamDecCallback hCallback, NvVideoStreamDec *pDecoder);

// NvVideoStreamDecoderClose:
//      Closes the video decoder.
//
//  pDecoder
//      Pointer to decoder handle
//
//  Returns NVVSRESULT_OK or NVVSRESULT_FAIL
//
NvVSResult NvVideoStreamDecoderClose(NvVideoStreamDec *pDecoder);

// NvVideoStreamDecoderDecode:
//      A non-blocking call to feed bitstream data to the decoder
//
//  hDecoder
//      Decoder handle
//  pBuffer
//      Input video bitstream to be decoded.
//  uNumBytes
//      Number of bytes available in the bistream buffer
//  bDiscontinuityFlag
//      Specifies if data was lost between the last data chunk and this data chunk
//      0: no data was lost / continious data
//      1: data was lost / discontinuity
//  pConsumedBytes
//      Number of bytes consumed by decoder
//
//  Returns NVVSRESULT_OK or NVVSRESULT_FAIL or NVVSRESULT_BUSY
//
NvVSResult NvVideoStreamDecoderDecode(NvVideoStreamDec hDecoder,
                unsigned char *pBuffer, unsigned int uNumBytes,
                unsigned char bDiscontinuityFlag, unsigned int *pConsumedBytes);

// NvVideoStreamDecoderUpdateAttribs:
//
// pDecoderAttrib
//      Updated decoder attributes.
//          uUpdateXYZ set to 1 indicates the attribute to be updated.
//          Where XYZ is one of (Display, AspectRatio, Rects, FrameRate, Color,
//          Profile, RenderState)
//          Same restriction as explained for the NvVideoDecoderOpen() are applicable
//          uSetStreamInfo cannot be updated
//
//  Returns NVRESULT_OK or NVRESULT_FAIL
//
NvVSResult NvVideoStreamDecoderUpdateAttribs(NvVideoStreamDec hDecoder,
                NvDecoderAttributes *pDecoderAttrib);


// NvVideoStreamDecoderReset:
//      Does a soft reset of decoder
//
//  Returns NVRESULT_OK or NVRESULT_FAIL
//
NvVSResult NvVideoStreamDecoderReset(NvVideoStreamDec hDecoder);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NV_VIDEO_STREAM_DECODER_API_H__ */

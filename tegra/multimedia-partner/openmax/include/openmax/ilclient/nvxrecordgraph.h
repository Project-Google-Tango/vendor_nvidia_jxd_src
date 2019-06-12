/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _NVX_RECORDER_GRAPH_H
#define _NVX_RECORDER_GRAPH_H

#include "nvxframework.h"
#include "nvxgraph.h"
#include "NVOMX_IndexExtensions.h"

#define INVALID_PARAM               0x7FFFFFF
#define MAX_OUTPUT_BUFFERS         10

typedef enum  {
    NvxOUTPUT_FORMAT_DEFAULT = 0,
    NvxOUTPUT_FORMAT_THREE_GPP = 1,
    /* These are audio only file formats */
    NvxOUTPUT_FORMAT_RAW_AMR = 2,
    NvxOUTPUT_FORMAT_MP4 = 3,
    NvxOUTPUT_FORMAT_JPG = 4,
    NvxOUTPUT_FORMAT_Force32 = 0x7FFFFFF // must be last - used to validate format type
}NvxOutputFormat;

typedef enum  {
    NvxRecorderInputType_File = 1,
    NvxRecorderInputType_Mic = 2,
    NvxRecorderInputType_Camera = 3,
    NvxRecorderInputType_Force32 = 0x7FFFFFF // must be last - used to validate format type
}NvxRecorderInputType;


// To have the backward compatibility, deviated from coding standards
typedef enum {
   VIDEO_ENCODER_TYPE_H264 = 1,
   VIDEO_ENCODER_TYPE_MPEG4 = 2,
   VIDEO_ENCODER_TYPE_H263 = 3,
   IMAGE_ENCODER_TYPE_JPEG = 4
} NvxRecoderEncoderType;

typedef enum
{
    NvxRecorderGraphState_Initialized = 0,
    NvxRecorderGraphState_DataSourceConfiguired,
    NvxRecorderGraphState_Prepared,
    NvxRecorderGraphState_Start,
    NvxRecorderGraphState_stop,
    NvxRecorderGraphState_Pause
} NvxRecorderGraphState;

typedef struct
{
    void *pClientContext;
    NvxFillBufferDone pFillBufferDone;
    NvxEmptyBufferDone pEmptyBufferDone;
    NvxEventHandler pEventHandler;
}NvxRecorderAppContext;

typedef struct NvxRecorderParam_ {
    int nWidthCapture;
    int nHeightCapture;
    OMX_U32 nBitRate;
    OMX_U32 nPeakBitrate;
    OMX_U32 nFrameRate;
    OMX_U32 nAppType;
    OMX_U32 nQualityLevel;
    OMX_U32 nErrorResilLevel;
    OMX_U32 videoEncoderType;
    OMX_U32 videoEncodeLevel;
    OMX_U32 TemporalTradeOffLevel;
    OMX_S32  nRotation;
    OMX_MIRRORTYPE eMirror;
    OMX_U32 nInitialQPI;
    OMX_U32 nInitialQPP;
    NVX_VIDEO_RATECONTROL_MODE eRCMode;
    OMX_U32 nMinQpI;
    OMX_U32 nMaxQpI;
    OMX_U32 nMinQpP;
    OMX_U32 nMaxQpP;
    OMX_U32 nMinQpB;
    OMX_U32 nMaxQpB;
    OMX_U32 nSliceHeaderSpacing;
    OMX_U32 nIFrameInterval;
    OMX_BOOL bStringentBitrate;
    OMX_BOOL bSetMaxEncClock;
    OMX_U32 nFavorInterBias;
    OMX_U32 nFavorIntraBias;
    OMX_U32 nFavorIntraBias_16X16;
    OMX_BOOL bFrameSkip;
    OMX_BOOL bAllIFrames;
    OMX_BOOL bBitBasedPacketization;
    OMX_BOOL bInsertSPSPPSAtIDR;
    OMX_BOOL bUseConstrainedBP;
    OMX_BOOL bInsertVUI;
    OMX_BOOL bInsertAUD;
    OMX_BOOL bEnableStuffing;
    OMX_U32 imageEncoderType;
} NvxRecorderParam;

typedef struct _NvxPreviewParam
{
    OMX_U32 nWidth;
    OMX_U32 nHeight;
    //Need to add a lot here
}NvxPreviewParam;


typedef struct _NvxRecorderGraph
{
    // Signifies the Framework
    NvxFramework        m_pOmxFramework;
    // Stores the reference the graph
    NvxGraph               *m_pGraph;
    // Pointer to Audio Capture Component
    NvxComponent       *m_pAudioCapturer;
    // Pointer to Audio Encoder Component
    NvxComponent       *m_pAudioEncoder;
    // Pointer to Video Encoder Component
    NvxComponent       *m_pVideoEncoder;
    // Pointer to Image Encoder Component
    NvxComponent       *m_pImageEncoder;
    // Pointer to Raw Audio Reader Component
    NvxComponent       *m_pAudioReader;
    // Pointer to Raw Video/Image Reader Component
    NvxComponent       *m_pyuvReader;
    // Pointer to Raw AudioVideo Writer Component
    NvxComponent       *m_pAVWriter;
    // Pointer to Raw Image Writer Component
    NvxComponent       *m_pImageWriter;
    // Pointer to Raw Image Writer Component
    NvxComponent       *m_pCamera;

    // Recorder Graph State
    NvxRecorderGraphState eRecState;
    // Recorder App Context
    NvxRecorderAppContext RecordAppContext;
    // Recoder Params
    NvxRecorderParam RecorderParam;
    // Preview Params
    NvxPreviewParam PreviewParam;
    // Signifies graph should be completely tunneled
    OMX_BOOL   bAllTunneled;

    // Audio Encoding Type
    OMX_AUDIO_CODINGTYPE eAudioCodingType;
    // Audio Sample Rate
    OMX_U32 nAudioSampleRate;
    // Audio Bit Rate
    OMX_U32 nAudioBitRate;
    // Audio Channels
    OMX_U32 nChannels;
    // Audio Source
    OMX_U32 eAudioSource;
    // AAC Audio Profile Type
    OMX_AUDIO_AACPROFILETYPE eAacProfileType;
    // Audio Writer Port
    OMX_U32     audiowriterport;
    // External Audio Source
    char *externalAudioSource;
    // Signifies if external Audio Source is used
    OMX_BOOL useExternalAudioSource;

    // Recoder Input Type
    NvxRecorderInputType RecInputType;
    // Output File Path
    char *outfile;
    // Specifies whether Audio Encoder is enabled
    OMX_BOOL   AudioEncEnable;
    // Specifies whether Video Encoder is enabled
    OMX_BOOL   VideoEncEnable;
    // Specifies whether Image Encoder is enabled
    OMX_BOOL   ImageEncEnable;
    // Specifies whether Image Encoder is enabled
    OMX_BOOL   PreviewEnable;
    // Use NV buffers
    OMX_BOOL   bUseNvBuffers;

    // Video Encoder Buffers
    OMX_BUFFERHEADERTYPE  *VideoEncoderInputBuffer[MAX_OUTPUT_BUFFERS];
    // Video Encoder Input Port Def
    OMX_PARAM_PORTDEFINITIONTYPE oVideoEncInputPortDef;
    // Number of Video Input Buffers
    OMX_U32 nVideoInputBuffers;

    // Output format/file type
    NvxOutputFormat eOutputFormat;
    // Signifies Output File size
    OMX_U64 nFileSize;
    // Signifies Recording Duration
    OMX_U64 nDuration;
    // Signifies Video Input File name
    char *pVideoInputFileName;
    // Signifies Audio Input File name
    char *pAudioInputFileName;
    // Signifies Image Input File name
    char *pImageInputFileName;
} NvxRecorderGraph;

// Audio Recorder Parameters
typedef struct _NvxRecorderAudioParams
{
    OMX_AUDIO_CODINGTYPE eAudioCodingType;
    OMX_AUDIO_AACPROFILETYPE eAacProfileType;
    OMX_U32 nAudioSampleRate;
    OMX_U32 nAudioBitRate;
    OMX_U32 nChannels;
    OMX_U32 eAudioSource;
}NvxRecorderAudioParams;

// Video Recorder Parameters
typedef struct _NvxRecorderVideoParams
{
    OMX_U32 nVideoCaptureWidth;
    OMX_U32 nVideoCaptureHeight;
    OMX_U32 nVideoFrameRate;
    OMX_BOOL bUseNvBuffers;
    OMX_U32 videoEncoderType;
    OMX_U32 nBitRate;
}NvxRecorderVideoParams;

// Image Recorder Params
typedef struct _NvxRecorderImageParams
{
    OMX_U32 nImageCaptureWidth;
    OMX_U32 nImageCaptureHeight;
    OMX_U32 imageEncoderType;
}NvxRecorderImageParams;


OMX_ERRORTYPE NvxInitRecorderStruct(NvxRecorderGraph *pRecorder,
                                    NvxFramework pOmxFramework);

void NvxRecorderGraphUseExternalAudioSource(NvxRecorderGraph *pRecorder,
                                                    char *audRenderName);
OMX_ERRORTYPE NvxCreateTunneledRecorderGraph(NvxRecorderGraph *pGraph);
OMX_ERRORTYPE NvxCreateRecorderGraph(NvxRecorderGraph *pGraph);

OMX_ERRORTYPE NvxChangeStateRecorderGraph(NvxRecorderGraph *m_pGraph,
                                        NvxRecorderGraphState eRecGraphState);

OMX_ERRORTYPE NvxReleaseRecorderGraph(NvxRecorderGraph *pRecGraph);

void NvxRecorderRegisterClientCallbacks(NvxRecorderGraph *pGraph,
                                       void *pThis,
                                       NvxFillBufferDone pFillBufferDone,
                                       NvxEmptyBufferDone pEmptyBufferDone,
                                       NvxEventHandler pEventHandler);

OMX_ERRORTYPE NvxRecorderSetVideoParams(NvxRecorderGraph *pGraph,
                                        NvxRecorderVideoParams *pParams);

OMX_ERRORTYPE NvxRecorderSetAudioParams(NvxRecorderGraph *pGraph,
                                        NvxRecorderAudioParams *pParams);

OMX_ERRORTYPE NvxRecorderSetImageParams(NvxRecorderGraph *pRecorder,
   NvxRecorderImageParams *pParams);
OMX_ERRORTYPE NvxRecorderSetPreviewParams(NvxRecorderGraph *pRecorder,
   NvxPreviewParam *pParams);

OMX_ERRORTYPE NvxRecorderSetOutputFile(NvxRecorderGraph *pRecorder,
                                       char *sPath);
OMX_ERRORTYPE NvxRecorderSetOutputFormat(NvxRecorderGraph *pRecorder,
                                            NvxOutputFormat Params);

OMX_BUFFERHEADERTYPE *NvxRecorderGetVideoEncoderBuffer(
                                        NvxRecorderGraph *pRecorder,
                                        OMX_U32 index);

NvxComponent* NvxRecorderGetVideoEncoder(NvxRecorderGraph *pRecorder);

OMX_U32 NvxRecorderVideoEncoderGetBufferCount(NvxRecorderGraph *pRecorder);

OMX_ERRORTYPE NvxRecorderSetOutputFileSize(NvxRecorderGraph *pRecorder,
                                            OMX_U64 nSizeInBytes);

OMX_ERRORTYPE NvxRecorderSetMaxDuration(NvxRecorderGraph *pRecorder,
                                            OMX_U64 nDuration);
OMX_ERRORTYPE NvxRecorderSetVideoInputFileName(NvxRecorderGraph *pRecorder,
                                            char *pFileName);
OMX_ERRORTYPE NvxRecorderSetAudioInputFileName(NvxRecorderGraph *pRecorder,
                                            char *pFileName);
OMX_ERRORTYPE NvxRecorderSetImageInputFileName(NvxRecorderGraph *pRecorder,
                                            char *pFileName);
OMX_ERRORTYPE NvxRecorderEnablePort(NvxPort *port, OMX_BOOL enable);

void CheckAudioParams(NvxRecorderGraph *pRecorder);
void CheckVideoParams(NvxRecorderGraph *pRecorder);
void CheckImageParams(NvxRecorderGraph *pRecorder);

OMX_ERRORTYPE s_SetupVideoEncoder(NvxRecorderGraph *pRecorder, OMX_HANDLETYPE hVideoEncoder);
OMX_ERRORTYPE NvxTunneledInitializeWriter(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxTunneledInitializeAudioChain(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxTunneledInitializeVideoChain(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxTunneledAudioCapture(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxReleaseRecorderComponents(NvxRecorderGraph *pRecorder);
#endif


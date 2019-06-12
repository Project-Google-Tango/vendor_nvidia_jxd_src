/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef OMX_TEST_LIB_h
#define OMX_TEST_LIB_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "testassert.h"
#include "samplebase.h"
#include "samplewindow.h"
#include "nvodm_imager.h"
#include "nvtest.h"
#include "nvfxmath.h"

// Test 'types'
#define NONE                  -1
#define PLAYER                 0
#define RAWPLAYER              1
#define ISDBT_PLAYER           2
#define AV_RECORDER            3
#define JPEG_DECODER           4

// test 'commands'
#define NONE                  -1
#define PLAY                   0
#define PAUSE                  1
#define SEEK                   2
#define STOP                   3
#define VOLUME                 4
#define TRICKMODE              5
#define STEP                   6
#define ULPTEST                7
#define COMPLEX_SEEK           8
#define TRICKMODESANITY        9
#define CACHEINFO              10
#define RATECONTROLTEST        11
#define CRAZY_SEEK_TEST        12
#define PAUSE_STOP             13
#define LOADED_IDLE_LOADED     14
#define FLUSH_PORTS_IN_IDLE    15

typedef struct NvxProfile
{
    OMX_BOOL bProfile;
    char     ProfileFileName[PROFILE_FILE_NAME_LENGTH];
    OMX_BOOL bVerbose;
    OMX_BOOL bStubOutput;
    OMX_U32  nForceLocale;
    OMX_U32  nProfileOpts;
    OMX_BOOL bNoAVSync;
    OMX_U32  nAVSyncOffset;
    OMX_BOOL bFlip;
    OMX_U32  nFrameDrop;

    OMX_BOOL bSanity;
    OMX_U32  nAvgFPS;
    OMX_U32  nTotFrameDrops;
    OMX_BOOL bDisableRendering;

    // for camera
    OMX_U64  nTSPreviewStart;        //
    OMX_U64  nTSCaptureStart;
    OMX_U64  nTSCaptureEnd;
    OMX_U64  nTSPreviewEnd;
    OMX_U64  nTSStillConfirmationFrame;
    OMX_U64  nTSFirstPreviewFrameAfterStill;
    OMX_U32  nPreviewStartFrameCount;
    OMX_U32  nPreviewEndFrameCount;
    OMX_U32  nCaptureStartFrameCount;
    OMX_U32  nCaptureEndFrameCount;
    OMX_S32  xExposureTime;
    OMX_S32  nExposureISO;
    OMX_U32  nBadFrameCount;
} NvxProfile;

typedef struct NvxGraph
{
    NvxCompGraph oCompGraph;
    OMX_HANDLETYPE hClock;
    int nRotate;
    int nX;
    int nY;
    int nWidth;
    int nHeight;
    int nCropX;
    int nCropY;
    int nCropWidth;
    int nCropHeight;
    int testtype;
    int testcmd;
    OMX_STATETYPE playState;
    char *parser;
    char *video;
    char *audio;
    OMX_BOOL novid;
    OMX_BOOL noaud;
    char *renderer;
    char *infile;
    int delinfile;
    char *outfile;
    char *splitfile;
    char *recordfile;
    ENvxRecordingMode RecordMode;
    OMX_U32 ChannelID;
    int stubrender;
    int volume;
    float playSpeed;
    int ffRewTimeInSec;
    NVX_AUDIO_OUTPUTTYPE eAudOutputType;
    int sanity;
    int desiredfps;
    OMX_HANDLETYPE hRenderer;
    OMX_HANDLETYPE hDecoder;
    OMX_HANDLETYPE hFileParser;
    void *hWnd;
    OMX_BOOL bNoWindow;
    OMX_BOOL bHideDesktop;
    OMX_BOOL bThumbnailPreferred;
    OMX_BOOL bLowMem;
    OMX_U32 ulpkpiMode;
    OMX_S32 xScaleFactor;
    OMX_U32 nThumbnailWidth;
    OMX_U32 nThumbnailHeight;
    OMX_U32 nEncodedSize;
    OMX_U32 nReadBufferSize;
    OMX_BOOL bAlphaBlend;
    OMX_U8 nAlphaValue;

    int dBGain[EQ_NO_CHANNEL][EQBANDS];
    OMX_BOOL bSetDB;

    OMX_BOOL bKeepAspect;
    OMX_BOOL bSaveCoverArt;

    OMX_BOOL streaming;
     
    OMX_BOOL bMobileTVUseCmdFromFile;
    OMX_BOOL bMobileTVListProgs;
    OMX_BOOL bMobileTVProgIsSet;
    OMX_U32 nMobileTVProgNum; 
    OMX_U32 nCrazySeekCount;
    OMX_U32 nCrazySeekSeed;
    OMX_U32 nCrazySeekDelay;
    OMX_U32 nFileDuration;
    unsigned int MobileTVDelay;
    unsigned int MobileTVCmd;
    char *pMobileTVCmdFile;

    OMX_BOOL bCaptureFrame;
    char *capturefile;

    OMX_U32 nFileCacheSize;
    OMX_BOOL bEnableFileCache;
    OMX_BOOL bAudioOnly;
    OMX_BOOL bPlayTime;

    char *custcomp;
    int startTimeInSec;
    OMX_BOOL  bIsM2vPlayback;
    OMX_U32   DeinterlaceMethod;  
    OMX_U32   SplitMode;

    NvBool bSmartDimmerEnable;
    OMX_U32 StereoRenderMode;
} NvxGraph;

void SetLowMem(OMX_HANDLETYPE hComp, OMX_BOOL bLowMem);

// Create a file reader/writer, and set the input/output filename
OMX_ERRORTYPE InitReaderWriter(char *component, char *filename,
                               OMX_HANDLETYPE *hComp, NvxGraph* pGraph,
                               TestContext *pAppData, OMX_CALLBACKTYPE *pCallbacks);

// Create a clock component, disable all ports, and set it to use audio as
// reference
OMX_ERRORTYPE InitClock(char *component, NvxGraph *pGraph, TestContext *pAppData, OMX_CALLBACKTYPE *pCallbacks);

void SetProfileMode(OMX_HANDLETYPE hComp, NvxProfile *pProf);

void GetProfile(OMX_HANDLETYPE hComp, NvxProfile *pProf);

void SetAudioOutput(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

void SetVolume(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

void SetEQ(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

void SetSize(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

void SetCrop(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

void SetKeepAspect(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

void SetAudioOnlyHint(OMX_HANDLETYPE hComp, OMX_BOOL bAudioOnlyHint);

void SetDisableTimestampUpdates(OMX_HANDLETYPE hComp, OMX_BOOL bDisableUpdates);

void SetFileCacheSize(OMX_HANDLETYPE hComp, OMX_U32 nSize);

void SetSmartDimmer(OMX_HANDLETYPE hComp, NvxGraph *pGraph);

NvF32 GetCurrentMediaTime(OMX_HANDLETYPE hClock);
OMX_ERRORTYPE PlaybackPause(NvxGraph *pCurGraph, TestContext *pAppData);

OMX_ERRORTYPE PlaybackResumeFromPause(NvxGraph *pCurGraph, TestContext *pAppData);

OMX_ERRORTYPE RemoveFile(OMX_STRING filename);

OMX_ERRORTYPE RemoveJPEGFiles(char *basename);

void GetFileCacheInfo(OMX_HANDLETYPE hComp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OMXAL_LINUX_MEDIAPLAYER_H
#define OMXAL_LINUX_MEDIAPLAYER_H

#include "linux_common.h"
#include <gst/interfaces/streamvolume.h>

typedef struct
{
    XAuint32 mState;
    XAmillisecond mDuration;
    void *mContext;
    xaPlayCallback mCallback;
    XAuint32 mEventFlags;
    XAmillisecond mMarkerPosition;
    XAmillisecond mPositionUpdatePeriod;
    XAmillisecond mLastSeekPosition;
    XAuint32 mFramesSinceLastSeek;
    XAuint32 mFramesSincePositionUpdate;
    GThread *hCallbackHandle;
    GCond *playThreadSema;
    GMutex *playThreadMutex;
    GCond *playStateChange;
    GMutex *playStateChangeMutex;
    XAboolean stopThread;
    XAboolean reloadValues;
} PlayerData;

#define XA_SUFFICIENTDATA_LOWERBOUND      ((XAuint32) 0x00000014)
#define XA_SUFFICIENTDATA_UPPERBOUND       ((XAuint32) 0x000003E7)

typedef struct
{
    XAuint32 mBufferLevel;
    XAuint32 mEventFlags;
    XApermille mFillUpdatePeriod;
    xaPrefetchCallback mCallback;
    void *pContext;
} PrefetchData;

#define MAX_SUPPORTED_STREAMS 20

typedef struct
{
    XAuint32 DomainType[MAX_SUPPORTED_STREAMS + 1];
    XAMediaContainerInformation ContainerInfo;
    XAAudioStreamInformation AudioStreamInfo;
    XAVideoStreamInformation VideoStreamInfo;
    XAImageStreamInformation ImageStreamInfo;
    XATimedTextStreamInformation TextStreamInfo;
    XAMIDIStreamInformation MidiStreamInfo;
    XAVendorStreamInformation VendorStreamInfo;
    XAboolean mActiveStreams[MAX_SUPPORTED_STREAMS + 1];
    XAboolean IsStreamInfoProbed;
    const XAchar *pUri;
    XAuint32 UriSize;
    xaStreamEventChangeCallback mCallback;
    void *mContext;
    XAboolean VideoStreamChangedCBNotify;
    XAboolean AudioStreamChangedCBNotify;
} StreamInfoData;

typedef struct
{
    GstElement *mPlayer;
    GMainLoop *mLoop;
    GstBus *mBus;
    GThread *mLoopThread;
    XAuint32 mSigBusAsync;
    GstClockID mPeriodicUpdate;
    GstClockID mMarkerUpdate;
    GstElement *pVideoSinkBin;
    GstElement *pVideoGamma;
    GstElement *pVideoEffects;
    GstElement *pVideoCrop;
    GstElement *pVideoFlip;
    GstElement *pVideoScale;
    GstElement *pVideoSink;
    GstElement *pCameraSrc;
    XAboolean bImagePlayback;
} XAMediaPlayer;

typedef struct
{
    /**< Enable/Disable Looping of track */
    XAboolean loopEnable;
    XAmillisecond startPos;
    XAmillisecond endPos;
} SeekData;

#define MAX_PLAYRATERANGE 5
#define DEFAULT_PLAYRATEINDEX 2
typedef struct
{
    XApermille CurrentRate;
    XApermille PreviousRate;
    XApermille MinRate[MAX_PLAYRATERANGE];
    XApermille MaxRate[MAX_PLAYRATERANGE];
    XApermille StepSize[MAX_PLAYRATERANGE];
    XAuint32 SupportedCapabilities[MAX_PLAYRATERANGE];
    XAuint32 CurrentCapabilities[MAX_PLAYRATERANGE];
} PlayRateData;

typedef struct
{
    XApermille MinRate;
    XApermille MaxRate;
    XApermille CurrentRate;
    XApermille StepSize;
    XAuint8 index;
    XAint32 Capabilities ;
}PlayRate;

typedef struct
{
    XAboolean loopEnable;   /**< Enable/Disable Looping of track */
    XAmillisecond startPos;
    XAmillisecond endPos;
} PlayerConfigLoop;

typedef struct
{
    XAmillidegree rotateAngle;
    XAuint32 scaleOptions;
    XAuint32 backgroundColor;
    XAuint32 renderingHints;
    XARectangle * pSrcRect;
    XARectangle * pDestRect;
    XAuint32 mirror;
} VideoPostProcessingData;

extern XAresult ALBackendMediaPlayerCreate(CMediaPlayer *pCMediaPlayer);

extern void ALBackendMediaPlayerDestroy(CMediaPlayer *pCMediaPlayer);

extern XAresult ALBackendMediaPlayerSignalThread(CMediaPlayer *pCMediaPlayer);

extern XAresult
ALBackendMediaPlayerRealize(
    CMediaPlayer *pCMediaPlayer,
    XAboolean bAsync);

extern XAresult
ALBackendMediaPlayerResume(
    CMediaPlayer *pCMediaPlayer,
    XAboolean bAsync);

extern XAresult
ALBackendMediaPlayerGetDuration(
    CMediaPlayer *pCMediaPlayer,
    XAmillisecond *pDurMsec);

extern XAresult
ALBackendMediaPlayerGetPosition(
    CMediaPlayer *pCMediaPlayer,
    XAmillisecond *pPosMsec);

extern XAresult
ALBackendMediaPlayerSetLoop(
    CMediaPlayer *pCMediaPlayer);

extern XAresult
ALBackendMediaPlayerSetRate(
    CMediaPlayer *pCMediaPlayer,
    XApermille newRate);

extern XAresult
ALBackendMediaPlayerSeek(
    CMediaPlayer *pCMediaPlayer,
    XAmillisecond position,
    XAuint32 seekMode);

extern XAresult
ALBackendMediaPlayerSetPlayState(
    CMediaPlayer *pCMediaPlayer,
    XAuint32 playState);

extern XAresult
ALBackendMediaPlayerSetPlayRate(
    CMediaPlayer *pCMediaPlayer,
    XApermille rate);

extern XAresult
ALBackendVideoPostProcessingCommit(
   CMediaPlayer *pCMediaPlayer);

extern XAresult
ALBackendMediaPlayerSetVolume(
    CMediaPlayer *pCMediaPlayer,
    XAmillibel level);

extern XAresult
ALBackendMediaPlayerGetVolume(
    CMediaPlayer *pCMediaPlayer,
    XAmillibel *pLevel);

extern XAresult
ALBackendMediaPlayerGetMaxVolume(
    CMediaPlayer *pCMediaPlayer,
    XAmillibel *pMaxLevel);

extern XAresult
ALBackendMediaPlayerSetMute(
    CMediaPlayer *pCMediaPlayer,
    XAboolean mute);

extern XAresult
ALBackendMediaPlayerGetMute(
    CMediaPlayer *pCMediaPlayer,
    XAboolean *pMute);

/* DynamicSource Interface */
extern XAresult
ALBackendMediaPlayerSetSource(
    CMediaPlayer *pCMediaPlayer,
    XADataSource *pDataSource);

/* ImageControls Interface */
extern XAresult
ALBackendMediaPlayerSetBrightness(
    CMediaPlayer *pCMediaPlayer,
    XAuint32 brightness);

extern XAresult
ALBackendMediaPlayerGetBrightness(
    CMediaPlayer *pCMediaPlayer,
    XAuint32 *pBrightness);

extern XAresult
ALBackendMediaPlayerSetContrast(
    CMediaPlayer *pCMediaPlayer,
    XAint32 contrast);

extern XAresult
ALBackendMediaPlayerGetContrast(
    CMediaPlayer *pCMediaPlayer,
    XAint32 *pContrast);

extern XAresult
ALBackendMediaPlayerSetGamma(
    CMediaPlayer *pCMediaPlayer,
    XApermille gamma);

extern XAresult
ALBackendMediaPlayerGetGamma(
    CMediaPlayer *pCMediaPlayer,
    XApermille *pGamma);

extern XAresult
ALBackendMediaPlayerGetSupportedGammaSettings(
    CMediaPlayer *pCMediaPlayer,
    XApermille *pMinValue,
    XApermille *pMaxValue);

extern XAresult
ALBackendMediaPlayerQueryContainerInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo);

extern XAresult
ALBackendMediaPlayerQueryActiveStreams(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo);

extern XAresult
ALBackendMediaPlayerGetVideoStreamInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo);

extern XAresult
ALBackendMediaPlayerGetAudioStreamInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo);

extern XAresult
ALBackendMediaPlayerGetTextStreamInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo);

extern void
ALBackendMediaPlayerVideoTagsChangedCallback(
    GstElement *playbin2,
    gint streamid,
    gpointer data);

extern void
ALBackendMediaPlayerAudioTagsChangedCallback(
    GstElement *playbin2,
    gint streamid,
    gpointer data);

extern void
ALBackendMediaPlayerTextTagsChangedCallback(
    GstElement *playbin2,
    gint streamid,
    gpointer data);

extern void
ALBackendMediaPlayerAudioChangedCallback(
    GstElement *playbin2,
    gpointer data);

extern void
ALBackendMediaPlayerVideoChangedCallback(
    GstElement *playbin2,
    gpointer data);

extern void
ALBackendMediaPlayerTextChangedCallback(
    GstElement *playbin2,
    gpointer data);

extern XAresult
ALBackendMediaPlayerSignalThread(
    CMediaPlayer *pCMediaPlayer);
#endif


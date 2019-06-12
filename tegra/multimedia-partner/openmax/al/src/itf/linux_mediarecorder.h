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

#ifndef OMXAL_LINUX_MEDIARECORDER_H
#define OMXAL_LINUX_MEDIARECORDER_H

#include "linux_common.h"
#include <gst/interfaces/streamvolume.h>
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/descriptions.h>

#define CAPTURE_MODE 1
#define RECORDING_MODE 2

typedef struct
{
    XAuint32 mState;
    XAmillisecond mDurationLimit;
    XAmillisecond mMarkerPosition;
    XAmillisecond mPositionUpdatePeriod;
    xaRecordCallback mCallback;
    void *pContext;
    XAuint32 mEventFlags;
} RecordData;

typedef struct
{
    XAchar *pURI;
    xaSnapshotInitiatedCallback initiatedCallback;
    xaSnapshotTakenCallback takenCallback;
    XAuint32 mNumberOfPictures;
    XAuint32 mFps;
    XAboolean bFreezeViewFinder;
    void *pContext;
    XAboolean bShutterFeedback;
    XADataSink mDataSink;
    XAuint32 mPictureCount;
    XAboolean bInitiateSnapshotCalled;
} SnapshotData;

typedef struct
{
    GstElement *mRecorder;
    GstElement *pAudioEncodeBin;
    GstElement *pAudioSrcElem;
    GstElement *pFilesinkElem;
    GstElement *pAudioConvertElem;
    GstElement *pAudioCapsfilterElem;
    GstElement *pAudioVolumeElem;
    GstElement *pCameraSrc;
    GMainLoop *pLoop;
    GstBus *pBus;
    GThread *pLoopThread;
    XAuint32 mSigBusAsync;
    GstEncodingVideoProfile *pVideoProfile;
    GstEncodingAudioProfile *pAudioProfile;
    GstEncodingContainerProfile *pContainerProfile;
    GstPadTemplate *pPadTemplate;
    GstCaps *pVideoCaps;
    GstCaps *pAudioCaps;
    GstCaps *pContainerCaps;
    GstClockTime mBaseTime;
    GstClockTime mRecordTime;
    XAuint32 mRecordState;
    XAboolean bPaused;
    XAboolean bUpdateRequired;
    XAboolean bStopThread;
    XAboolean bReloadValues;
    XAuint32 mFileCount;
    GThread *pCallbackHandle;
    GMutex *pRecordThreadMutex;
    GCond *pRecordThreadSema;
    GCond *pExitSema;
    GMutex *pExitMutex;
    GCond *pStateChangeSema;
    GMutex *pStateChangeMutex;
} XAMediaRecorder;

/*********************************
 * CMediaRecorder for AV recording
 *********************************/
extern XAresult
ALBackendMediaRecorderCreate(
    CMediaRecorder *pCMediaRecorder);

extern XAresult
ALBackendMediaRecorderRealize(
    CMediaRecorder *pCMediaRecorder,
    XAboolean async);

extern XAresult
ALBackendMediaRecorderResume(
    CMediaRecorder *pCMediaRecorder,
    XAboolean async);

extern void
ALBackendMediaRecorderDestroy(
    CMediaRecorder *pCMediaRecorder);

/**********************************
 * CMediaRecorder in snapshot mode
 *********************************/

extern XAresult
ALBackendMediaRecorderTakeSnapshot(
    CMediaRecorder *pCMediaRecorder);

extern XAresult
ALBackendMediaRecorderInitiateSnapshot(
    CMediaRecorder *pCMediaRecorder,
    void *pData);


/****************************
 * IRecord
 ****************************/
extern XAresult
ALBackendMediaRecorderSetRecordState(
    CMediaRecorder *pCMediaRecorder,
    XAuint32 state);

extern  XAresult
ALBackendMediaRecorderGetClockTime(
    CMediaRecorder *pCMediaRecorder,
    XAmillisecond *pMsec);

/****************************
 * IVideoEncoder
 ****************************/
extern  XAresult
ALBackendMediaRecorderSetVideoSettings(
    CMediaRecorder *pCMediaRecorder,
    XAVideoSettings *pVideoSettings);

/****************************
 * IAudioEncoder
 ****************************/
extern  XAresult
ALBackendMediaRecorderSetAudioSettings(
    CMediaRecorder *pCMediaRecorder,
    XAAudioEncoderSettings *pAudioSettings);

extern XAresult
ALBackendMediaRecorderSetVolume(
    CMediaRecorder *pCMediaRecorder,
    XAmillibel level);

extern XAresult
ALBackendMediaRecorderGetVolume(
    CMediaRecorder *pCMediaRecorder,
    XAmillibel *pLevel);

extern XAresult
ALBackendMediaRecorderGetMaxVolume(
    CMediaRecorder *pCMediaRecorder,
    XAmillibel *pMaxLevel);

extern XAresult
ALBackendMediaRecorderSetMute(
    CMediaRecorder *pCMediaRecorder,
    XAboolean mute);

extern XAresult
ALBackendMediaRecorderGetMute(
    CMediaRecorder *pCMediaRecorder,
    XAboolean *pMute);

extern XAresult
ALBackendMediaRecorderSignalThread(
    CMediaRecorder *pCMediaRecorder);

#endif


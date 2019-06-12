/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
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

#ifndef OMXAL_LINUX_OBJECTS_H
#define OMXAL_LINUX_OBJECTS_H

// Initialize gstreamer
void InitializeFramework(void);


// MediaPlayer Object
extern XAresult ALBackendMediaPlayerCreate(CMediaPlayer *pCMediaPlayer);

extern void ALBackendMediaPlayerDestroy(CMediaPlayer *pCMediaPlayer);

extern XAresult
ALBackendMediaPlayerRealize(
    CMediaPlayer *pCMediaPlayer,
    XAboolean bAsync);

extern XAresult
ALBackendMediaPlayerResume(
    CMediaPlayer *pCMediaPlayer,
    XAboolean bAsync);

// OutputMix Object
extern XAresult ALBackendOutputmixCreate(COutputMix* pCOutputMix);

extern XAresult
ALBackendOutputmixRealize(
    COutputMix* pCOutputMix,
    XAboolean bAsync);

extern XAresult
ALBackendOutputmixResume(
    COutputMix* pCOutputMix,
    XAboolean bAsync);

extern void ALBackendOutputmixDestroy(COutputMix* pCOutputMix);

// CameraDevice Object
extern XAresult
ALBackendCameraDeviceCreate(
    CCameraDevice *pCameraDevice);

extern XAresult
ALBackendCameraDeviceRealize(
    CCameraDevice *pCameraDevice,
    XAboolean async);

extern XAresult
ALBackendCameraDeviceResume(
    CCameraDevice *pCameraDevice,
    XAboolean async);

extern void
ALBackendCameraDeviceDestroy(
    CCameraDevice *pCameraDevice);

// MediaRecorder Object
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

#endif


/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All Rights Reserved.
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

#ifndef OMXAL_LINUX_OUTPUTMIX_H
#define OMXAL_LINUX_OUTPUTMIX_H

#include "linux_common.h"
#include <gst/interfaces/streamvolume.h>

typedef struct
{
    GstElement *pOutputMixBin;
    GstElement *pEqualizer;
    GstElement *pVolume;
    GstElement *pAudioSink;
} COutputMixData;

#define DEFAULT_EQUALIZER_BANDS 3
#define DEFAULT_EQUALIZER_NAME "equalizer-3bands"

typedef struct
{
    const char *presetName;
    gdouble bandValue[DEFAULT_EQUALIZER_BANDS];
} EqualizerPresets;

extern XAresult ALBackendOutputmixCreate(COutputMix* pCOutputMix);

extern XAresult ALBackendOutputmixRealize(COutputMix* pCOutputMix, XAboolean bAsync);

extern XAresult ALBackendOutputmixResume(COutputMix* pCOutputMix, XAboolean bAsync);

extern void ALBackendOutputmixDestroy(COutputMix* pCOutputMix);

/* IVolume Interface support */
XAresult
ALBackendOutputMixSetVolume(
    COutputMix *pCOutputMix,
    XAmillibel level);

XAresult
ALBackendOutputMixGetVolume(
    COutputMix *pCOutputMix,
    XAmillibel *plevel);

XAresult
ALBackendOutputMixGetMaxVolume(
    COutputMix *pCOutputMix,
    XAmillibel *pMaxLevel);

XAresult
ALBackendOutputMixSetMute(
    COutputMix *pCOutputMix,
    XAboolean mute);

XAresult
ALBackendOutputMixGetMute(
    COutputMix *pCOutputMix,
    XAboolean *pMute);

/* IEqualizer Interface support */
XAresult
ALBackendEqualizerSetEnabled(
    COutputMix *pCOutputMix,
    XAboolean enabled,
    XAuint16 index);

XAresult
ALBackendEqualizerGetNumberOfBands(
    COutputMix *pCOutputMix,
    XAuint16 *pNumBands);

XAresult
ALBackendEqualizerGetBandLevelRange(
    COutputMix *pCOutputMix,
    XAmillibel *pMin,
    XAmillibel *pMax);

XAresult
ALBackendEqualizerSetBandLevel(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmillibel level);

XAresult
ALBackendEqualizerGetBandLevel(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmillibel *pLevel);

XAresult
ALBackendEqualizerGetCenterFreq(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmilliHertz *pCenter);

XAresult
ALBackendEqualizerGetBandFreqRange(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmilliHertz *pMin,
    XAmilliHertz *pMax);

XAresult
ALBackendEqualizerGetBandFreqRange(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmilliHertz *pMin,
    XAmilliHertz *pMax);

XAresult
ALBackendEqualizerGetBand(
    COutputMix *pCOutputMix,
    XAmilliHertz frequency,
    XAuint16 *pBand);

XAresult
ALBackendEqualizerUsePreset(
    COutputMix *pCOutputMix,
    XAuint16 index);

XAresult
ALBackendEqualizerGetNumberOfPresets(
    COutputMix *pCOutputMix,
    XAuint16 *pNumPresets);

XAresult
ALBackendEqualizerGetPresetName(
    COutputMix *pCOutputMix,
    XAuint16 index,
    const XAchar **ppName);
#endif

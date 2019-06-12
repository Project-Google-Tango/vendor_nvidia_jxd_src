/*
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All Rights Reserved.
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

#ifndef OMXAL_LINUX_COMMON_H
#define OMXAL_LINUX_COMMON_H

#include "sles_allinclusive.h"
#include "linux_objects.h"
#include <gst/gst.h>

#ifndef AL_CHK_ARG
#define AL_CHK_ARG(expr) {               \
    if (((expr) == 0))   \
    {                       \
        printf("%s[%d] Assert Failed!", __FUNCTION__, __LINE__); \
        return XA_RESULT_PARAMETER_INVALID;\
    }                       \
}
#endif

typedef struct {
    XAuint32 mCodecID;  ///< The codec ID
    const XAAudioCodecDescriptor *mDescriptor;  ///< The corresponding descriptor
} AudioCodecDescriptor;

typedef struct {
    XAuint32 mCodecID;  ///< The codec ID
    const XAImageCodecDescriptor *mDescriptor;  ///< The corresponding descriptor
} ImageCodecDescriptor;

typedef struct {
    XAuint32 mCodecID;  ///< The codec ID
    const XAVideoCodecDescriptor *mDescriptor;  ///< The corresponding descriptor
} VideoCodecDescriptor;

#define MAX_AUDIO_IO_DEVICES 16

typedef struct
{
    xaAvailableAudioInputsChangedCallback mAvailableAudioInputsChangedCallback;
    void *mAvailableAudioInputsChangedContext;
    xaAvailableAudioOutputsChangedCallback mAvailableAudioOutputsChangedCallback;
    void *mAvailableAudioOutputsChangedContext;
    xaDefaultDeviceIDMapChangedCallback mDefaultDeviceIDMapChangedCallback;
    void *mDefaultDeviceIDMapChangedContext;
    XAuint32 mDevices;
    XAuint32 mOutputDevices;
    XAuint32 mInputDevices;
    XAchar *mDefaultInputDeviceName[MAX_AUDIO_IO_DEVICES];
    XAchar *mDefaultOutputDeviceName[MAX_AUDIO_IO_DEVICES];
    XAuint32 mDefaultOutputDevices;
    XAuint32 mDefaultInputDevices;
} AudioIODeviceCapabilitiesData;

typedef struct
{
    XAuint32 volume[MAX_AUDIO_IO_DEVICES];

} DeviceVolumeData;

typedef struct
{
    // Values as specified by the application
    XAmillibel mLevel;
    XApermille mStereoPosition;
    XAboolean mMute;
    XAmillibel mMaxVolumeLevel;
    XAmillibel mMinVolumeLevel;
    XAboolean mEnableStereoPosition;
} VolumeData;

typedef struct
{
    XAboolean mEnabled;
    XAint16 mCurrentPreset;
} EqualizerData;

typedef struct
{
    XAuint32 nNumOfEffects;
} ImageEffectsData;

XAresult
ChangeGraphState(
    GstElement *pGraph,
    GstState newState);

#endif


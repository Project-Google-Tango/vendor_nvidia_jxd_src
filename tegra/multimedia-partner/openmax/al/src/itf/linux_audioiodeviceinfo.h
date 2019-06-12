/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef OMXAL_LINUX_AUDIO_IO_DEVICES_H
#define OMXAL_LINUX_AUDIO_IO_DEVICES_H

// #define PULSE_AUDIO_ENABLE

#include "linux_common.h"

#ifdef PULSE_AUDIO_ENABLE
#include <pulse/pulseaudio.h>
#endif

enum
{
    AUDIO_SINK = 0x1,
    AUDIO_SOURCE = 0x2
};

typedef struct
{
#ifdef PULSE_AUDIO_ENABLE
    pa_threaded_mainloop *mMainLoop;
    pa_context *mContext;
#endif
} PulseAudioContextData;

typedef struct
{
    XAuint32 mDeviceId;
    XAuint32 mDeviceType;
    XAuint32 mCardId;
    XAuint32 mSampleRate;
    XAuint32 mSampleFormat;
    XAuint32 mChannels;
    XAuint32 mStreamId;
#ifdef PULSE_AUDIO_ENABLE
    pa_cvolume mCVolume;
#endif
    XAAudioInputDescriptor mInputDescriptor;
    XAAudioOutputDescriptor mOutputDescriptor;
    XAuint32 mLinuxDefaultAudioInputDeviceID;
    XAuint32 mLinuxDefaultAudioOutputDeviceID;
} AudioDevicesInputOutputDescriptor;

typedef struct
{
    const struct XAOutputMixItf_ *mItf;
    void *pContext;
    xaMixDeviceChangeCallback mCallback;
} OutputMixRegisterCallbackData;

extern void
ALBackendInitializePulseAudio(
    void *self);

extern void
ALBackendDeInitializePulseAudio(
    void *self);

// Device volume
extern XAresult
ALBackendGetDeviceVolumeScale(
    void *self,
    XAuint32 DeviceID,
    XAint32 *pMinValue,
    XAint32 *pMaxValue);

extern XAresult
ALBackendSetDeviceVolume(
    void *self,
    XAuint32 DeviceID,
    XAint32 Volume);

extern XAresult
ALBackendGetDeviceVolume(
    void *self,
    XAuint32 DeviceID,
    XAint32 *pVolume);

// AudioIOCapabilities
extern XAresult
ALBackendQuerySampleFormatsSupported(
    XAuint32 DeviceID,
    XAmilliHertz SamplingRate,
    XAint32 *pNumSampleFormats,
    XAint32 *pSampleFormats);

extern XAresult
ALBackendGetAssociatedAudioOutputs(
    void *self,
    XAuint32 DeviceID,
    XAint32 *NumOutputs,
    XAuint32 *pOutputDeviceIDs);

extern XAresult
ALBackendGetAssociatedAudioInputs(
    void *self,
    XAuint32 DeviceID,
    XAint32 *NumInputs,
    XAuint32 *pInputDeviceIDs);

extern XAresult
ALBackendQueryAudioInputCapabilities(
    void *self,
    XAuint32 DeviceID,
    XAAudioInputDescriptor *pDescriptor);

extern XAresult
ALBackendQueryAudioOutputCapabilities(
    void *self,
    XAuint32 DeviceID,
    XAAudioOutputDescriptor *pDescriptor);

extern XAresult
ALBackendGetAvailableAudioOutputs(
    XAint32 *NumOutputs,
    XAuint32 *pOutputDeviceIDs);

extern XAresult
ALBackendGetAvailableAudioInputs(
    XAint32 *NumInputs,
    XAuint32 *pInputDeviceIDs);

extern XAresult
ALBackendGetDefaultAudioDevices(
    void *self,
    XAuint32 DefaultDeviceID,
    XAint32 *NumDefaults,
    XAuint32 *pAudioDeviceIDs);

// OutputMix
extern void
ALBackendOutputMixInitialize(
    void *self);

extern void
ALBackendOutputMixDeInitialize(
    void *self);

extern XAresult
ALBackendReRoute(
    XAint32 NumOutputDevices,
    XAuint32 *pOutputDeviceIDs);

extern void
ALBackendOutmixRegisterDeviceChangeCallback(
    void *self,
    xaMixDeviceChangeCallback callback,
    void *pContext);
#endif


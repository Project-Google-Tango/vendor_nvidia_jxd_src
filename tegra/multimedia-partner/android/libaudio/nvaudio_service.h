/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef LIBAUDIO_NVAUDIOSERVICE_H
#define LIBAUDIO_NVAUDIOSERVICE_H

//#define LOG_NDEBUG 0

#include <hardware_legacy/AudioHardwareBase.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryHeapBase.h>
#include <binder/MemoryBase.h>
#include <media/IAudioPolicyService.h>
#include <media/IMediaPlayerService.h>
#include <media/mediaplayer.h>
#include <binder/ProcessState.h>
#include "invaudioservice.h"
#include "nvcap_audio.h"

class NvAudioALSAService: public BnNvAudioALSAService
{
public:
    NvAudioALSAService();
    ~NvAudioALSAService();
    virtual void setWFDAudioBuffer(const sp<IMemory>& mem);
    virtual status_t setWFDAudioParams(uint32_t sampleRate, uint32_t channels,
                                 uint32_t bitsPerSample);
    virtual void registerClient(const sp<INvAudioALSAClient>& client);
    virtual void unregisterClient(const sp<INvAudioALSAClient>& client);
    virtual status_t setAudioParameters(audio_io_handle_t ioHandle,
                                        const String8& keyValuePairs);
    virtual String8 getAudioParameters(audio_io_handle_t ioHandle,
                                       const String8& keys);
};

class NvAudioALSAClientWFD: public IBinder::DeathRecipient
{
public:
    // DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);
};

#endif //LIBAUDIO_NVAUDIOALSA_H

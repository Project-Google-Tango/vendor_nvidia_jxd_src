/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * invaudioservice.h
 *
 * Binder based communications interface definition for NvAudioALSAService.
 *
 */

#ifndef NVAUDIO_SERVICE_H
#define NVAUDIO_SERVICE_H

using namespace android;

#define NV_AUDIO_ALSA_SRVC_NAME "media.nvidia.audio_alsa"
#define NV_AUDIO_WFD_SRVC_NAME "media.nvidia.wfd_audio"


/* INvAudioALSAClient */

typedef enum {
    NV_AUDIO_ALSA_CLIENT_TYPE_UNKNOWN = 0,
    NV_AUDIO_ALSA_CLIENT_TYPE_WFD,
    NV_AUDIO_ALSA_CLIENT_TYPE_MAX,
} NV_AUDIO_ALSA_CLIENT_TYPE;

class INvAudioALSAClient : public IInterface
{
public:
    DECLARE_META_INTERFACE(NvAudioALSAClient);

    // get audio client's type
    virtual NV_AUDIO_ALSA_CLIENT_TYPE getClientType() = 0;
};

class BnNvAudioALSAClient : public BnInterface<INvAudioALSAClient>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

class NvAudioALSAClient: public BnNvAudioALSAClient
{
public:
    NvAudioALSAClient(NV_AUDIO_ALSA_CLIENT_TYPE type)
        : mClientType(type) {}

    virtual NV_AUDIO_ALSA_CLIENT_TYPE getClientType() { return mClientType; }

private:
    NV_AUDIO_ALSA_CLIENT_TYPE mClientType;
};

/* NvAudioALSA Service */
class INvAudioALSAService: public IInterface
{
public:
    DECLARE_META_INTERFACE(NvAudioALSAService);

    virtual void setWFDAudioBuffer(const sp<IMemory>& mem) = 0;
    virtual status_t setWFDAudioParams(uint32_t sampleRate, uint32_t channels,
                                       uint32_t bitsPerSample) = 0;
    virtual void registerClient(const sp<INvAudioALSAClient>& client) = 0;
    virtual void unregisterClient(const sp<INvAudioALSAClient>& client) = 0;
    virtual status_t setAudioParameters(audio_io_handle_t ioHandle,
                                        const String8& keyValuePairs) = 0;
    virtual String8 getAudioParameters(audio_io_handle_t ioHandle,
                                       const String8& keys) = 0;
};

/* --- Server side --- */

class BnNvAudioALSAService: public BnInterface<INvAudioALSAService>
{
public:
  virtual status_t    onTransact( uint32_t code,
                                  const Parcel& data,
                                  Parcel* reply,
                                  uint32_t flags = 0);
};

#endif

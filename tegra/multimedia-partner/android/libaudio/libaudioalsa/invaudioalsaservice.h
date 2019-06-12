/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * invaudioalsaservice.h
 *
 * Binder based communications interface definition for NvAudioALSAService.
 *
 */

using namespace android;

#define NV_AUDIO_ALSA_SRVC_NAME "media.nvidia.audio_alsa"

/* NvAudioALSA Service */
class INvAudioALSAService: public IInterface
{
public:
    DECLARE_META_INTERFACE(NvAudioALSAService);

    virtual void setWFDAudioBuffer(const sp<IMemory>& mem) = 0;
    virtual status_t setWFDAudioParams(uint32_t sampleRate, uint32_t channels,
                                       uint32_t bitsPerSample) = 0;
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

/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvAudioALSAService"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"

using namespace android;

NvAudioALSAService::NvAudioALSAService(NvAudioALSADevice* device) :
    m_Device(device)
{
    ALOGV("NvAudioALSAService");
}

NvAudioALSAService::~NvAudioALSAService()
{
    ALOGV("~NvAudioALSAService");
}

void NvAudioALSAService::instantiate(NvAudioALSADevice* device)
{
    ALOGV("NvAudioALSAService::instantiate");

    sp<IBinder> binder =
        defaultServiceManager()->getService(String16(NV_AUDIO_ALSA_SRVC_NAME));
    if (binder == 0) {
        defaultServiceManager()->addService(String16(NV_AUDIO_ALSA_SRVC_NAME),
                                            new NvAudioALSAService(device));
        ProcessState::self()->startThreadPool();
    }
}

void NvAudioALSAService::setWFDAudioBuffer(const sp<IMemory>& mem)
{
    ALOGV("setWFDAudioBuffer");

    NvAudioALSADev* hDev = m_Device->getRenderer()->getALSADev(WFD_PCM_INDEX);
    ssize_t offset = 0;
    size_t size = 0;

    AutoMutex lock(hDev->device->m_Lock);

    hDev->device->memHeap = mem->getMemory(&offset, &size);
    hDev->device->sharedMem = (uint8_t*)hDev->device->memHeap->getBase() +
                              offset;
}

status_t NvAudioALSAService::setWFDAudioParams(uint32_t sampleRate,
                                               uint32_t channels,
                                               uint32_t bitsPerSample)
{
    ALOGV("setAudioParams rate %d chan %d bps %d",
         sampleRate, channels, bitsPerSample);

    NvAudioALSADev* hDev = m_Device->getRenderer()->getALSADev(WFD_PCM_INDEX);
    snd_pcm_format_t format = (bitsPerSample == 16) ? SND_PCM_FORMAT_S16_LE :
                                                      SND_PCM_FORMAT_S8;

    AutoMutex lock(hDev->device->m_Lock);

    if (hDev->hpcm) {
        if ((hDev->sampleRate != sampleRate) || (hDev->channels != channels) ||
            (hDev->format != format)) {
            ALOGE("setAudioParams: Requested params don't match with already"
                 "open WFD device params");
            return -EINVAL;
        }
    }
    else {
        hDev->sampleRate = sampleRate;
        hDev->channels = channels;
        hDev->format = format;
    }

    return NO_ERROR;
}

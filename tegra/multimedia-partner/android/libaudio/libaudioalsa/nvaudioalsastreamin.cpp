/*
 * Copyright (c) 2009-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 ** Based upon AudioStreamInALSA.cpp, provided under the following terms:
 **
 ** Copyright 2008-2009 Wind River Systems
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "NvAudioALSAStreamIn"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"

using namespace android;


NvAudioALSAStreamIn::NvAudioALSAStreamIn(NvAudioALSADevice *parent) :
    m_Parent(parent),
    m_Handle(0),
    m_FramesLost(0)
{
}

NvAudioALSAStreamIn::~NvAudioALSAStreamIn()
{
    if (m_Handle) {
        if (m_Handle->hpcm)
            m_Parent->m_ALSADev->close(m_Handle);

        free(m_Handle);
        m_Handle = 0;
    }
}

status_t NvAudioALSAStreamIn::setGain(float gain)
{
    return mixer() ? mixer()->setMasterGain(gain) : (status_t)NO_INIT;
}

ssize_t NvAudioALSAStreamIn::read(void *buffer, ssize_t bytes)
{
    AutoMutex lock(m_Lock);

    if (!m_Handle->hpcm) {
        status_t err = m_Parent->m_ALSADev->open(m_Handle,
                                                m_Handle->curDev,
                                                m_Handle->curMode);
        if (err < 0) {
            ALOGE("Failed to open alsa device for input stream \n");
            return 0;
        }
    }

    snd_pcm_sframes_t n, frames;
    status_t          err;

    frames = snd_pcm_bytes_to_frames((snd_pcm_t*)m_Handle->hpcm, bytes);
    do {
        n = snd_pcm_readi((snd_pcm_t*)m_Handle->hpcm, buffer, frames);
        if (n < frames) {
            if (n == -EBADFD) {
                // Somehow the stream is in a bad state. The driver probably
                // has a bug and snd_pcm_recover() doesn't seem to handle this.
                ALOGE("read: failed to snd_pcm_readi, %d, %s", (int)n, snd_strerror((int)n));
                m_Handle->device->close(m_Handle);
                status_t err = m_Handle->device->open(m_Handle, m_Handle->curDev, m_Handle->curMode);
                if (err < 0) {
                    ALOGE("Failed to open alsa device for input stream, %d", err);
                    return 0;
                }
            } else if (n == -EBADF) {
                ALOGE("read: failed to snd_pcm_readi, %d, %s", (int)n, snd_strerror((int)n));
                // For Bad file number case don't try to close alsa device
                m_Handle->hpcm = 0;
                status_t err = m_Handle->device->open(m_Handle, m_Handle->curDev, m_Handle->curMode);
                if (err < 0) {
                    ALOGE("Failed to open alsa device for output stream, %d", err);
                    return 0;
                }
            } else if (n < 0) {
                ALOGV("alsa: recovering pcm stream n = %ld", n);
                if (m_Handle->hpcm)
                    n = snd_pcm_recover((snd_pcm_t*)m_Handle->hpcm, n, 0);
            } else {
                ALOGW("alsa: call snd_pcm_prepare n = %ld frames = %ld", n, frames);
                n = snd_pcm_prepare((snd_pcm_t*)m_Handle->hpcm);
            }
            return static_cast<ssize_t>(n);
        }
    } while (n == -EAGAIN);
    //ALOGV("alsa: read_bytes %d",
    //     (int)snd_pcm_frames_to_bytes((snd_pcm_t*)m_Handle->hpcm, n));

    return static_cast<ssize_t>(snd_pcm_frames_to_bytes(
                                       (snd_pcm_t*)m_Handle->hpcm, n));
}

status_t NvAudioALSAStreamIn::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

status_t NvAudioALSAStreamIn::standby()
{
    AutoMutex lock(m_Lock);

    ALOGV("standby: IN");

    if (m_Handle->hpcm)
        m_Handle->device->close(m_Handle);

    return NO_ERROR;
}

void NvAudioALSAStreamIn::resetFramesLost()
{
    AutoMutex lock(m_Lock);
    m_FramesLost = 0;
}

unsigned int NvAudioALSAStreamIn::getInputFramesLost() const
{
    unsigned int count = m_FramesLost;
    // Stupid interface wants us to have a side effect of clearing the count
    // but is defined as a const to prevent such a thing.
    if (count)
        ((NvAudioALSAStreamIn *)this)->resetFramesLost();

    return count;
}

status_t NvAudioALSAStreamIn::addAudioEffect(effect_handle_t effect)
{
	ALOGI("addAudioEffect");
	return NO_ERROR;
}

status_t NvAudioALSAStreamIn::removeAudioEffect(effect_handle_t effect)
{
	ALOGI("removeAudioEffect");
	return NO_ERROR;
}


NvAudioALSAMixer *NvAudioALSAStreamIn::mixer()
{
    return m_Parent->m_Mixer;
}

NvAudioALSADev *NvAudioALSAStreamIn::getALSADev()
{
    return m_Handle;
}

status_t NvAudioALSAStreamIn::open(int *format,
                            uint32_t *channels,
                            uint32_t *rate,
                            uint32_t devices)
{
    status_t err = NO_ERROR;

    ALOGV("open: IN");

    m_Handle = (NvAudioALSADev*)malloc(sizeof(NvAudioALSADev));
    if (!m_Handle) {
        ALOGE("Failed to allocate NvAudioALSADev \n");
        return -ENOMEM;
    }
    memset(m_Handle, 0, sizeof(NvAudioALSADev));

    m_Handle->device = m_Parent->m_ALSADev;
    m_Handle->streamType = SND_PCM_STREAM_CAPTURE;

    m_Handle->format = TEGRA_DEFAULT_IN_FORMAT;
    if (format) {
        switch(*format) {
        case AudioSystem::FORMAT_DEFAULT:
            break;

        case AudioSystem::PCM_16_BIT:
            m_Handle->format = SND_PCM_FORMAT_S16_LE;
            break;

        case AudioSystem::PCM_8_BIT:
            m_Handle->format = SND_PCM_FORMAT_S8;
            break;

        default:
            ALOGW("Unknown PCM format %d. Forcing default %d",
                *format, m_Handle->format);
            break;
        }
    }

    m_Handle->channels = TEGRA_DEFAULT_IN_CHANNELS;
    if (channels) {
        if (popCount(*channels) > 0)
            m_Handle->channels = popCount(*channels);
        else
            ALOGW("Invalid channels %u. Forcing default %u",
                *channels, m_Handle->channels);
    }


    m_Handle->sampleRate = TEGRA_DEFAULT_IN_SAMPLE_RATE;
    if (rate) {
        if (*rate > 0)
            m_Handle->sampleRate = *rate;
        else
            ALOGW("Invalid rate %u. Forcing default %u",
                *rate, m_Handle->sampleRate);
    }

    err = m_Handle->device->open(m_Handle, devices, m_Parent->mode());
    if (err < 0) {
        ALOGE("Failed to open alsadev with supplied params.Trying to open with default params");
        m_Handle->format = TEGRA_DEFAULT_IN_FORMAT;
        m_Handle->channels =  TEGRA_DEFAULT_IN_CHANNELS;
        m_Handle->sampleRate = TEGRA_DEFAULT_IN_SAMPLE_RATE;

        err = m_Handle->device->open(m_Handle, devices, m_Parent->mode());
        if (err < 0) {
            ALOGE("Failed to open alsadev with default params\n");
            return err;
        }
    }

    err = m_Handle->device->route(m_Handle, devices, m_Parent->mode());
    if (err < 0) {
        ALOGE("Failed to route to %d device\n",devices);
        return err;
    }

    // Close ALSA device for power saving
    m_Handle->device->close(m_Handle);

    if (channels && (m_Handle->channels != popCount(*channels)))
        *channels = this->channels();

    if (rate && (m_Handle->sampleRate != *rate))
        *rate = m_Handle->sampleRate;

    if (format && (this->format() != *format))
        *format = this->format();

    return NO_ERROR;
}

status_t NvAudioALSAStreamIn::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    status_t status = NO_ERROR;
    int device;
    ALOGV("setParameters() %s", keyValuePairs.string());

    if (param.getInt(key, device) == NO_ERROR) {
        device &= AudioSystem::DEVICE_IN_ALL;
        if (device) {
            m_Handle->device->route(m_Handle, (uint32_t)device,
                                    m_Parent->mode());
            m_Parent->setParameters(keyValuePairs);
        }
        param.remove(key);
    }

    if (param.size()) {
        status = BAD_VALUE;
    }
    return status;
}

String8 NvAudioALSAStreamIn::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)m_Handle->curDev);
    }

    ALOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

uint32_t NvAudioALSAStreamIn::sampleRate() const
{
    return m_Handle->sampleRate;
}

//
// Return the number of bytes (not frames)
//
size_t NvAudioALSAStreamIn::bufferSize() const
{
    size_t frameSize = (m_Handle->channels *
            ((m_Handle->format == SND_PCM_FORMAT_S8) ? (8 >> 3) : (16 >> 3)));
    size_t bytes = (m_Handle->bufferSize / m_Handle->periods) * frameSize;

    bytes += (frameSize - 1);
    bytes -= (bytes % frameSize);

    return bytes;
}

int NvAudioALSAStreamIn::format() const
{
    int pcmFormatBitWidth;
    int audioSystemFormat;

    snd_pcm_format_t ALSAFormat = m_Handle->format;

    pcmFormatBitWidth = snd_pcm_format_physical_width(ALSAFormat);
    switch(pcmFormatBitWidth) {
        case 8:
            audioSystemFormat = AudioSystem::PCM_8_BIT;
            break;

        default:
            LOG_FATAL("Unknown AudioSystem bit width %i!", pcmFormatBitWidth);

        case 16:
            audioSystemFormat = AudioSystem::PCM_16_BIT;
            break;
    }

    return audioSystemFormat;
}

uint32_t NvAudioALSAStreamIn::channels() const
{
    unsigned int count = m_Handle->channels;
    uint32_t channels = 0;

         switch(count) {
            default:
            case 2:
                channels |= AudioSystem::CHANNEL_IN_RIGHT;
                // Fall through...
            case 1:
                channels |= AudioSystem::CHANNEL_IN_LEFT;
                break;
        }

    return channels;
}


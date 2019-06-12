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
 ** Based upon AudioHardwareALSA.cpp, provided under the following terms:
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

#define LOG_TAG "NvAudioALSADevice"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"

using namespace android;

static void NvAudioALSAErrorHandler(const char *file,
                             int line,
                             const char *function,
                             int err,
                             const char *fmt,
                             ...)
{
    char buf[BUFSIZ];
    va_list arg;
    int l;

    va_start(arg, fmt);
    l = snprintf(buf, BUFSIZ, "%s:%i:(%s) ", file, line, function);
    vsnprintf(buf + l, BUFSIZ - l, fmt, arg);
    buf[BUFSIZ-1] = '\0';
    ALOGV("ALSALib Error: %s\n", buf);
    va_end(arg);
}

NvAudioALSADevice::NvAudioALSADevice() :
    m_ALSADev(0),
    m_WFDDev(0),
    m_Renderer(0),
    m_Mixer(0),
    m_Control(0),
    m_HDAControl(0)
{
    hw_device_t* device;
    int err = 0;
    extern const struct hw_module_t TEGRA_ALSA_MODULE;
    extern const struct hw_module_t TEGRA_WFD_MODULE;

    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++)
        m_VoiceCallAlsaDev[i] = 0;

    snd_lib_error_set_handler(&NvAudioALSAErrorHandler);
    m_Mixer = new NvAudioALSAMixer();
    if (!m_Mixer)
        ALOGE("Failed to create mixer object!!!");

    if (m_Mixer->initCheck() != NO_ERROR)
        goto exit;

    m_Control = new NvAudioALSAControl();
    if (!m_Control)
        ALOGE("Failed to create alsa control object!!!");

    if (m_Control->initCheck() != NO_ERROR)
        goto exit;

    m_HDAControl = new NvAudioALSAControl("hw:0");
    if (!m_HDAControl)
        ALOGE("Failed to create alsa control object!!!");

    if (m_HDAControl->initCheck() != NO_ERROR)
        ALOGE("The HDA Alsa Control is not properly initialized");

    TEGRA_ALSA_MODULE.methods->open(&TEGRA_ALSA_MODULE,
                                    TEGRA_ALSA_HARDWRAE_NAME, &device);
    if (err == 0) {
        m_ALSADev = (tegra_alsa_device_t *)device;
        m_ALSADev->alsaControl = m_Control;
        m_ALSADev->alsaHDAControl = m_HDAControl;
        m_StreamOutList.clear();
        m_StreamInList.clear();
    } else {
        ALOGE("ALSA Module could not be opened!!!");
        goto exit;
    }

#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
    TEGRA_WFD_MODULE.methods->open(&TEGRA_WFD_MODULE,
                                   TEGRA_ALSA_HARDWRAE_NAME, &device);
    if (err == 0) {
        m_WFDDev = (tegra_alsa_device_t *)device;
        m_WFDDev->default_sampleRate = m_ALSADev->default_sampleRate;
    } else {
        ALOGE("WFD Module could not be opened!!!");
        goto exit;
    }
#endif

    m_Renderer = new NvAudioALSARenderer(this);
    if (!m_Renderer) {
        ALOGE("Failed to create Alsa Renderer!!!");
        goto exit;
    }

    if (m_Renderer->initCheck() != NO_ERROR)
        goto exit;

#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
    NvAudioALSAService::instantiate(this);
    if (defaultServiceManager()->getService(String16(NV_AUDIO_ALSA_SRVC_NAME))
        == NULL)
        ALOGE("Failed to start service");
#endif

    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {
        m_VoiceCallAlsaDev[i] = (NvAudioALSADev*)malloc(sizeof(NvAudioALSADev));
        if (!m_VoiceCallAlsaDev[i]) {
            ALOGE("Failed to allocate NvAudioALSADev \n");
            break;
        }
        memset(m_VoiceCallAlsaDev[i], 0, sizeof(NvAudioALSADev));

        m_VoiceCallAlsaDev[i]->device = m_ALSADev;
        m_VoiceCallAlsaDev[i]->streamType = (snd_pcm_stream_t)i;
        m_VoiceCallAlsaDev[i]->isVoiceCallDevice = true;
        m_VoiceCallAlsaDev[i]->format = TEGRA_VOICE_CALL_FORMAT;
        m_VoiceCallAlsaDev[i]->channels = TEGRA_VOICE_CALL_CHANNELS;
        m_VoiceCallAlsaDev[i]->sampleRate = TEGRA_VOICE_CALL_SAMPLE_RATE;
        m_VoiceCallAlsaDev[i]->bufferSize = TEGRA_VOICE_CALL_BUFFER_SIZE;
        m_VoiceCallAlsaDev[i]->periods = TEGRA_VOICE_CALL_PERIODS;

        if (i == SND_PCM_STREAM_PLAYBACK) {
            m_VoiceCallAlsaDev[i]->curDev = AudioSystem::DEVICE_OUT_DEFAULT;
            m_VoiceCallAlsaDev[i]->earpieceVol = 0;
            m_VoiceCallAlsaDev[i]->speakerVol = 0;
            m_VoiceCallAlsaDev[i]->headsetVol = 0;
        } else {
            m_VoiceCallAlsaDev[i]->curDev = AudioSystem::DEVICE_IN_DEFAULT;
        }
    }
    return;

exit:
    if (m_Mixer) {
        delete m_Mixer;
        m_Mixer = NULL;
    }
    if (m_Control) {
        delete m_Control;
        m_Control = NULL;
    }
    if (m_HDAControl) {
        delete m_HDAControl;
        m_HDAControl = NULL;
    }
    if (m_Renderer) {
        delete m_Renderer;
        m_Renderer = NULL;
    }
#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
    if (m_WFDDev)
        m_ALSADev->common.close(&m_WFDDev->common);
#endif
    if (m_ALSADev)
        m_ALSADev->common.close(&m_ALSADev->common);
}

NvAudioALSADevice::~NvAudioALSADevice()
{
    ALOGV("%s ", __FUNCTION__);
    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {
        if (m_VoiceCallAlsaDev[i]) {
            if (m_VoiceCallAlsaDev[i]->hpcm)
                if (m_ALSADev)
                    m_ALSADev->close(m_VoiceCallAlsaDev[i]);
                free(m_VoiceCallAlsaDev[i]);
                m_VoiceCallAlsaDev[i] = 0;
        }
    }

    if (m_Mixer) {
        delete m_Mixer;
        m_Mixer = NULL;
    }
    if (m_Control) {
        delete m_Control;
        m_Control = NULL;
    }
    if (m_HDAControl) {
        delete m_HDAControl;
        m_HDAControl = NULL;
    }
    if (m_Renderer) {
        delete m_Renderer;
        m_Renderer = NULL;
    }
#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
    if (m_WFDDev)
        m_ALSADev->common.close(&m_WFDDev->common);
#endif
    if (m_ALSADev)
        m_ALSADev->common.close(&m_ALSADev->common);
}

status_t NvAudioALSADevice::initCheck()
{
#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
    if (m_ALSADev && m_WFDDev && m_Renderer && m_Mixer && m_Mixer->isValid())
#else
    if (m_ALSADev && m_Renderer && m_Mixer && m_Mixer->isValid())
#endif
        return NO_ERROR;
    else
        return NO_INIT;
}

status_t NvAudioALSADevice::setVoiceVolume(float volume)
{
    status_t status = NO_ERROR;
    ALOGV("%s: volume %f", __FUNCTION__, volume);

    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {
        if (m_ALSADev && m_ALSADev->set_voice_volume)
            status = m_ALSADev->set_voice_volume(m_VoiceCallAlsaDev[i], volume);
    }
    return status;
}

status_t NvAudioALSADevice::setMasterVolume(float volume)
{
    ALOGV("%s: volume %f", __FUNCTION__, volume);
    if (m_Mixer)
        return m_Mixer->setMasterVolume(volume);
    else
        return INVALID_OPERATION;
}

status_t NvAudioALSADevice::setMode(int mode)
{
    status_t status = NO_ERROR;

    ALOGV("setMode: IN");

    if (mode != mMode) {
        NvAudioALSADev *handle = NULL;
        status = AudioHardwareBase::setMode(mode);

        if (status == NO_ERROR) {
            for(NvAudioALSAStreamOutList::iterator it = m_StreamOutList.begin();
                    it != m_StreamOutList.end(); ++it) {
                handle = (*it)->getALSADev();
                status = m_ALSADev->route(handle, handle->curDev, mode);
                if (status != NO_ERROR)
                    break;
            }

            for(NvAudioALSAStreamInList::iterator it = m_StreamInList.begin();
                    it != m_StreamInList.end(); ++it) {
                handle = (*it)->getALSADev();
                status = m_ALSADev->route(handle, handle->curDev, mode);
                if (status != NO_ERROR)
                    break;
            }

            for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {
                status = m_ALSADev->route(m_VoiceCallAlsaDev[i],
                                          m_VoiceCallAlsaDev[i]->curDev,
                                          mode);
            }
        }
    }
    ALOGV("setMode: OUT");
    return status;
}

AudioStreamOut *
NvAudioALSADevice::openOutputStream(uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status)
{
    AutoMutex lock(m_Lock);

    ALOGV("openOutputStream: devices=0x%x, format=0x%x, channels=0x%x, sampleRate=%u",
        devices, *format, *channels, *sampleRate);

    status_t err = BAD_VALUE;
    NvAudioALSAStreamOut *out = 0;

    if (devices & (devices - 1)) {
        if (status)
            *status = err;
        ALOGD("openOutputStream: called with bad devices");
        return out;
    }

    out = new NvAudioALSAStreamOut(this);
    err = out->open(format, channels, sampleRate, devices);
    m_StreamOutList.push_back(out);

    if (status)
        *status = err;

    return out;
}

void
NvAudioALSADevice::closeOutputStream(AudioStreamOut* out)
{
    AutoMutex lock(m_Lock);

    ALOGV("closeOutputStream: IN");

    for(NvAudioALSAStreamOutList::iterator it = m_StreamOutList.begin();
        it != m_StreamOutList.end(); ++it) {
        if (*it == out) {
            m_StreamOutList.erase(it);
            break;
        }
    }

    delete out;
}

AudioStreamIn *
NvAudioALSADevice::openInputStream (uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status,
                                    AudioSystem::audio_in_acoustics acoustics)
{
    AutoMutex lock(m_Lock);

    ALOGV("openInputStream: devices=0x%x, format=0x%x, channels=0x%x, sampleRate=%u",
        devices, *format, *channels, *sampleRate);

    status_t err = BAD_VALUE;
    NvAudioALSAStreamIn *in = 0;

    if ((mMode == AudioSystem::MODE_IN_CALL) &&
        !(devices & AudioSystem::DEVICE_IN_VOICE_CALL)) {
        ALOGE("openInputStream: Require Call Mode-Input device\n");
        if (status)
            *status = err;
        return in;
    }

    if (devices & (devices - 1)) {
        if (status)
            *status = err;
        return in;
    }

    m_ALSADev->isMicMute = false;

    in = new NvAudioALSAStreamIn(this);
    err = in->open(format, channels, sampleRate, devices);
    m_StreamInList.push_back(in);

    if (status)
        *status = err;

    return in;
}

void NvAudioALSADevice::closeInputStream(AudioStreamIn* in)
{
    AutoMutex lock(m_Lock);

    ALOGV("closeInputStream: IN");

    for(NvAudioALSAStreamInList::iterator it = m_StreamInList.begin();
        it != m_StreamInList.end(); ++it) {
        if (*it == in) {
            m_StreamInList.erase(it);
            break;
        }
    }

    delete in;
}

status_t NvAudioALSADevice::setMicMute(bool state)
{
    NvAudioALSADev *handle = NULL;
    status_t status = NO_ERROR;

    for(NvAudioALSAStreamInList::iterator it = m_StreamInList.begin();
            it != m_StreamInList.end(); ++it) {
        handle = (*it)->getALSADev();
        status = m_ALSADev->set_mute(handle, handle->curDev, state);
    }

    m_ALSADev->isMicMute = state;

    return status;
}

status_t NvAudioALSADevice::getMicMute(bool *state)
{
    *state = m_ALSADev->isMicMute;
    return NO_ERROR;
}

status_t NvAudioALSADevice::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    status_t status = NO_ERROR;
    int device;
    ALOGV("setParameters() %s", keyValuePairs.string());

    if (param.getInt(key, device) == NO_ERROR) {
#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
        device &= ~(AudioSystem::DEVICE_OUT_AUX_DIGITAL |
                    AudioSystem::DEVICE_OUT_WIFI_DISPLAY);
#else
        device &= ~(AudioSystem::DEVICE_OUT_AUX_DIGITAL);
#endif

        // Set input and output voice call device routing to same device
        if (device & DEVICE_OUT_BLUETOOTH_SCO_ALL)
            device |= AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET;
        else if (device & AudioSystem::DEVICE_OUT_WIRED_HEADSET)
            device |= AudioSystem::DEVICE_IN_WIRED_HEADSET;
        else
            device |= AudioSystem::DEVICE_IN_BUILTIN_MIC;

        ALOGV("Switch voice call audio to device :0x%x\n", device);
        if (device & AudioSystem::DEVICE_OUT_ALL)
            m_ALSADev->route(m_VoiceCallAlsaDev[SND_PCM_STREAM_PLAYBACK],
                            (uint32_t)(device & AudioSystem::DEVICE_OUT_ALL),
                            mMode);

        if (device & AudioSystem::DEVICE_IN_ALL)
            m_ALSADev->route(m_VoiceCallAlsaDev[SND_PCM_STREAM_CAPTURE],
                            (uint32_t)(device & AudioSystem::DEVICE_IN_ALL),
                            mMode);

        param.remove(key);
    }

    return status;
}

String8 NvAudioALSADevice::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);

    return param.toString();
}

size_t NvAudioALSADevice::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    if ((sampleRate < 8000 || 48000 < sampleRate) ||
            (format != AudioSystem::PCM_16_BIT &&
                format != AudioSystem::PCM_8_BIT) ||
                    (channelCount != 1 && channelCount != 2))
        return 0;

    return 4096;
}


status_t NvAudioALSADevice::dumpState(int Fd, const Vector<String16>& Args)
{
    return NO_ERROR;
}

status_t NvAudioALSADevice::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

NvAudioALSARenderer* NvAudioALSADevice::getRenderer()
{
    return m_Renderer;
}

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
 ** Based upon ALSAMixer.cpp, provided under the following terms:
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

#define LOG_TAG "NvAudioNvAudioALSAMixer"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"

#define ALSA_CTL_DEFAULT_DEVICE_NAME "default"
#define ALSA_CTL_DEFAULT_DEVICE_NAME1 "default1"
#define ALSA_CTL_DEFAULT_DEVICE_MAX98095 "default_max98095"
#define ALSA_CTL_DEFAULT_DEVICE_AIC326X "default_aic326x"

#define SND_MIXER_VOL_RANGE_MIN  (0)
#define SND_MIXER_VOL_RANGE_MAX  (100)

#define ALSA_NAME_MAX 128

#define ALSA_STRCAT(x,y) \
    if (strlen(x) + strlen(y) < ALSA_NAME_MAX) \
        strcat(x, y);

using namespace android;
// ----------------------------------------------------------------------------

struct tegra_mixer_info_t;

struct tegra_alsa_properties_t
{
    const AudioSystem::audio_devices device;
    const char         *propName;
    const char         *propDefault;
    tegra_mixer_info_t       *mInfo;
};

#define ALSA_PROP(dev, name, out, in) \
    {\
        {dev, "alsa.mixer.playback." name, out, NULL},\
        {dev, "alsa.mixer.capture." name, in, NULL}\
    }

static tegra_alsa_properties_t
mixerMasterProp[SND_PCM_STREAM_LAST+1] =
        ALSA_PROP(AudioSystem::DEVICE_OUT_ALL, "master", "PCM", "Capture");

static tegra_alsa_properties_t
mixerProp[][SND_PCM_STREAM_LAST+1] = {
    ALSA_PROP(AudioSystem::DEVICE_OUT_EARPIECE, "earpiece", "Earpiece", "Capture"),
    ALSA_PROP(AudioSystem::DEVICE_OUT_SPEAKER, "speaker", "Speaker",  ""),
    ALSA_PROP(AudioSystem::DEVICE_OUT_WIRED_HEADSET, "headset", "Headphone", "Headphone"),
    ALSA_PROP(AudioSystem::DEVICE_OUT_BLUETOOTH_SCO, "bluetooth.sco", "Bluetooth", "Bluetooth Capture"),
    ALSA_PROP(AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP, "bluetooth.a2dp", "Bluetooth A2DP", "Bluetooth A2DP Capture"),
    ALSA_PROP(static_cast<AudioSystem::audio_devices>(0), "", NULL, NULL)
};

struct tegra_mixer_info_t
{
    tegra_mixer_info_t() :
        elem(0),
        min(SND_MIXER_VOL_RANGE_MIN),
        max(SND_MIXER_VOL_RANGE_MAX),
        mute(false)
    {
    }

    snd_mixer_elem_t *elem;
    long              min;
    long              max;
    long              volume;
    bool              mute;
    char              name[ALSA_NAME_MAX];
};

static int initMixer (snd_mixer_t **mixer, const char *name)
{
    int err;

    if ((err = snd_mixer_open(mixer, 0)) < 0) {
        ALOGE("Unable to open mixer: %s", snd_strerror(err));
        return err;
    }

    if ((err = snd_mixer_attach(*mixer, name)) < 0) {
        ALOGW("Unable to attach mixer to device %s: %s",
            name, snd_strerror(err));

        snd_mixer_close (*mixer);
        *mixer = NULL;
        return err;
    }

    if ((err = snd_mixer_selem_register(*mixer, NULL, NULL)) < 0) {
        ALOGE("Unable to register mixer elements: %s", snd_strerror(err));
        snd_mixer_close (*mixer);
        *mixer = NULL;
        return err;
    }

    // Get the mixer controls from the kernel
    if ((err = snd_mixer_load(*mixer)) < 0) {
        ALOGE("Unable to load mixer elements: %s", snd_strerror(err));
        snd_mixer_close (*mixer);
        *mixer = NULL;
        return err;
    }

    return 0;
}

typedef int (*hasVolume_t)(snd_mixer_elem_t*);

static const hasVolume_t hasVolume[] = {
    snd_mixer_selem_has_playback_volume,
    snd_mixer_selem_has_capture_volume
};

typedef int (*getVolumeRange_t)(snd_mixer_elem_t*, long int*, long int*);

static const getVolumeRange_t getVolumeRange[] = {
    snd_mixer_selem_get_playback_volume_range,
    snd_mixer_selem_get_capture_volume_range
};

typedef int (*setVolume_t)(snd_mixer_elem_t*, long int);

static const setVolume_t setVol[] = {
    snd_mixer_selem_set_playback_volume_all,
    snd_mixer_selem_set_capture_volume_all
};

NvAudioALSAMixer::NvAudioALSAMixer() :
    mInit(false)
{
    int err;

    m_Mixer[SND_PCM_STREAM_PLAYBACK] = 0;
    m_Mixer[SND_PCM_STREAM_CAPTURE] = 0;

    err = initMixer (&m_Mixer[SND_PCM_STREAM_PLAYBACK],
                                             ALSA_CTL_DEFAULT_DEVICE_NAME);
    if (err != NO_ERROR) {
        //If the intialisation of Mixer is failed for default alsa device for
        //a platform/device, try intialise it for another device(default1).
        //Added to support multiple codec on the same platform/device.
        err = initMixer (&m_Mixer[SND_PCM_STREAM_PLAYBACK],
                                             ALSA_CTL_DEFAULT_DEVICE_NAME1);
        if (err != NO_ERROR)
            err = initMixer (&m_Mixer[SND_PCM_STREAM_PLAYBACK],
                                             ALSA_CTL_DEFAULT_DEVICE_MAX98095);

        if (err != NO_ERROR) {
            err = initMixer (&m_Mixer[SND_PCM_STREAM_PLAYBACK],
                                             ALSA_CTL_DEFAULT_DEVICE_AIC326X);
            if (err != NO_ERROR)
                goto exit;
        }

    }

    err = initMixer (&m_Mixer[SND_PCM_STREAM_CAPTURE],
                                                ALSA_CTL_DEFAULT_DEVICE_NAME);
    if (err != NO_ERROR) {
        err = initMixer (&m_Mixer[SND_PCM_STREAM_CAPTURE],
                                                ALSA_CTL_DEFAULT_DEVICE_NAME1);
        if (err != NO_ERROR)
            err = initMixer (&m_Mixer[SND_PCM_STREAM_CAPTURE],
                                             ALSA_CTL_DEFAULT_DEVICE_MAX98095);
        if (err != NO_ERROR) {
            err = initMixer (&m_Mixer[SND_PCM_STREAM_CAPTURE],
                                             ALSA_CTL_DEFAULT_DEVICE_AIC326X);
            if (err != NO_ERROR)
                goto exit;
        }

    }

    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);

    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {

        tegra_mixer_info_t *info = mixerMasterProp[i].mInfo = new tegra_mixer_info_t;

        property_get (mixerMasterProp[i].propName,
                      info->name,
                      mixerMasterProp[i].propDefault);

        for (snd_mixer_elem_t *elem = snd_mixer_first_elem(m_Mixer[i]);
             elem;
             elem = snd_mixer_elem_next(elem)) {

            if (!snd_mixer_selem_is_active(elem))
                continue;

            snd_mixer_selem_get_id(elem, sid);

            // Find PCM playback volume control element.
            const char *elementName = snd_mixer_selem_id_get_name(sid);

            if (info->elem == NULL &&
                strcmp(elementName, info->name) == 0 &&
                hasVolume[i] (elem)) {

                info->elem = elem;
                if (i == SND_PCM_STREAM_PLAYBACK &&
                    snd_mixer_selem_has_playback_switch (elem))
                    snd_mixer_selem_set_playback_switch_all (elem, 1);
                break;
            }
        }

        ALOGV("Mixer: master '%s' %s.", info->name, info->elem ? "found" : "not found");

        for (int j = 0; mixerProp[j][i].device; j++) {

            tegra_mixer_info_t *info = mixerProp[j][i].mInfo = new tegra_mixer_info_t;

            property_get (mixerProp[j][i].propName,
                          info->name,
                          mixerProp[j][i].propDefault);

            for (snd_mixer_elem_t *elem = snd_mixer_first_elem(m_Mixer[i]);
                 elem;
                 elem = snd_mixer_elem_next(elem)) {

                if (!snd_mixer_selem_is_active(elem))
                    continue;

                snd_mixer_selem_get_id(elem, sid);

                // Find PCM playback volume control element.
                const char *elementName = snd_mixer_selem_id_get_name(sid);

               if (info->elem == NULL &&
                    strcmp(elementName, info->name) == 0 &&
                    hasVolume[i] (elem)) {

                    info->elem = elem;
                    if (i == SND_PCM_STREAM_PLAYBACK &&
                        snd_mixer_selem_has_playback_switch (elem))
                        snd_mixer_selem_set_playback_switch_all (elem, 1);
                    break;
                }
            }
            ALOGV("Mixer: route '%s' %s.", info->name, info->elem ? "found" : "not found");
        }
    }
    mInit = true;
    ALOGV("mixer initialized.");
    return;

exit:
    if (m_Mixer[SND_PCM_STREAM_PLAYBACK]) {
        snd_mixer_close (m_Mixer[SND_PCM_STREAM_PLAYBACK]);
        m_Mixer[SND_PCM_STREAM_PLAYBACK] = 0;
    }

    if (m_Mixer[SND_PCM_STREAM_CAPTURE]) {
        snd_mixer_close (m_Mixer[SND_PCM_STREAM_CAPTURE]);
        m_Mixer[SND_PCM_STREAM_CAPTURE] = 0;
    }
    return;
}

NvAudioALSAMixer::~NvAudioALSAMixer()
{
    ALOGV("%s ", __FUNCTION__);
    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {
        if (m_Mixer[i]) snd_mixer_close (m_Mixer[i]);
        if (mixerMasterProp[i].mInfo) {
            delete mixerMasterProp[i].mInfo;
            mixerMasterProp[i].mInfo = NULL;
        }
        for (int j = 0; mixerProp[j][i].device; j++) {
            if (mixerProp[j][i].mInfo) {
                delete mixerProp[j][i].mInfo;
                mixerProp[j][i].mInfo = NULL;
            }
        }
    }
    ALOGV("mixer destroyed.");
}

status_t NvAudioALSAMixer::initCheck()
{
    return mInit ? NO_ERROR : NO_INIT;
}

status_t NvAudioALSAMixer::setMasterVolume(float volume)
{
    tegra_mixer_info_t *info = mixerMasterProp[SND_PCM_STREAM_PLAYBACK].mInfo;
    if (!info || !info->elem) return INVALID_OPERATION;

    long minVol = info->min;
    long maxVol = info->max;

    // Make sure volume is between bounds.
    long vol = minVol + volume * (maxVol - minVol);
    if (vol > maxVol) vol = maxVol;
    if (vol < minVol) vol = minVol;

    info->volume = vol;
    snd_mixer_selem_set_playback_volume_all (info->elem, vol);

    return NO_ERROR;
}

status_t NvAudioALSAMixer::setMasterGain(float gain)
{
    tegra_mixer_info_t *info = mixerMasterProp[SND_PCM_STREAM_CAPTURE].mInfo;
    if (!info || !info->elem) return INVALID_OPERATION;

    long minVol = info->min;
    long maxVol = info->max;

    // Make sure volume is between bounds.
    long vol = minVol + gain * (maxVol - minVol);
    if (vol > maxVol) vol = maxVol;
    if (vol < minVol) vol = minVol;

    info->volume = vol;
    snd_mixer_selem_set_capture_volume_all (info->elem, vol);

    return NO_ERROR;
}

status_t NvAudioALSAMixer::setVolume(uint32_t device, float left, float right)
{
    for (int j = 0; mixerProp[j][SND_PCM_STREAM_PLAYBACK].device; j++)
        if (mixerProp[j][SND_PCM_STREAM_PLAYBACK].device & device) {

            tegra_mixer_info_t *info = mixerProp[j][SND_PCM_STREAM_PLAYBACK].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            long minVol = info->min;
            long maxVol = info->max;

            // Make sure volume is between bounds.
            long vol = minVol + left * (maxVol - minVol);
            if (vol > maxVol) vol = maxVol;
            if (vol < minVol) vol = minVol;

            info->volume = vol;
            snd_mixer_selem_set_playback_volume_all (info->elem, vol);
        }

    return NO_ERROR;
}

status_t NvAudioALSAMixer::setGain(uint32_t device, float gain)
{
    for (int j = 0; mixerProp[j][SND_PCM_STREAM_CAPTURE].device; j++)
        if (mixerProp[j][SND_PCM_STREAM_CAPTURE].device & device) {

            tegra_mixer_info_t *info = mixerProp[j][SND_PCM_STREAM_CAPTURE].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            long minVol = info->min;
            long maxVol = info->max;

            // Make sure volume is between bounds.
            long vol = minVol + gain * (maxVol - minVol);
            if (vol > maxVol) vol = maxVol;
            if (vol < minVol) vol = minVol;

            info->volume = vol;
            snd_mixer_selem_set_capture_volume_all (info->elem, vol);
        }

    return NO_ERROR;
}

status_t NvAudioALSAMixer::setCaptureMuteState(uint32_t device, bool state)
{
    for (int j = 0; mixerProp[j][SND_PCM_STREAM_CAPTURE].device; j++)
        if (mixerProp[j][SND_PCM_STREAM_CAPTURE].device & device) {

            tegra_mixer_info_t *info = mixerProp[j][SND_PCM_STREAM_CAPTURE].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            if (snd_mixer_selem_has_capture_switch (info->elem)) {

                int err = snd_mixer_selem_set_capture_switch_all (info->elem, static_cast<int>(!state));
                if (err < 0) {
                    ALOGE("Unable to %s capture mixer switch %s",
                        state ? "enable" : "disable", info->name);
                    return INVALID_OPERATION;
                }
            }

            info->mute = state;
        }

    return NO_ERROR;
}

status_t NvAudioALSAMixer::getCaptureMuteState(uint32_t device, bool *state)
{
    if (!state) return BAD_VALUE;

    for (int j = 0; mixerProp[j][SND_PCM_STREAM_CAPTURE].device; j++)
        if (mixerProp[j][SND_PCM_STREAM_CAPTURE].device & device) {

            tegra_mixer_info_t *info = mixerProp[j][SND_PCM_STREAM_CAPTURE].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            *state = info->mute;
            return NO_ERROR;
        }

    return BAD_VALUE;
}

status_t NvAudioALSAMixer::setPlaybackMuteState(uint32_t device, bool state)
{
    for (int j = 0; mixerProp[j][SND_PCM_STREAM_PLAYBACK].device; j++)
        if (mixerProp[j][SND_PCM_STREAM_PLAYBACK].device & device) {

            tegra_mixer_info_t *info = mixerProp[j][SND_PCM_STREAM_PLAYBACK].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            if (snd_mixer_selem_has_playback_switch (info->elem)) {

                int err = snd_mixer_selem_set_playback_switch_all (info->elem, static_cast<int>(!state));
                if (err < 0) {
                    ALOGE("Unable to %s playback mixer switch %s",
                        state ? "enable" : "disable", info->name);
                    return INVALID_OPERATION;
                }
            }

            info->mute = state;
        }

    return NO_ERROR;
}

status_t NvAudioALSAMixer::getPlaybackMuteState(uint32_t device, bool *state)
{
    if (!state) return BAD_VALUE;

    for (int j = 0; mixerProp[j][SND_PCM_STREAM_PLAYBACK].device; j++)
        if (mixerProp[j][SND_PCM_STREAM_PLAYBACK].device & device) {

            tegra_mixer_info_t *info = mixerProp[j][SND_PCM_STREAM_PLAYBACK].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            *state = info->mute;
            return NO_ERROR;
        }

    return BAD_VALUE;
}



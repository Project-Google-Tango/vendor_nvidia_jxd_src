/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 ** Based upon AudioStreamOutALSA.cpp, provided under the following terms:
 **
 ** Copyright 2008, Wind River Systems
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

#define LOG_TAG "NvAudioALSARenderer"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"
using namespace android;

static const uint32_t channelMask[8] =
{ 0xF, 0xFF, 0xFFF, 0xFFFF, 0xFFFFF, 0xFFFFFF, 0xFFFFFFF, 0xFFFFFFFF};

// ----------------------------------------------------------------------------

static inline int16_t sat16(int32_t val)
{
    if ((val >> 15) ^ (val >> 31))
        val = 0x7FFF ^ (val >> 31);
    return val;
}

NvAudioALSARenderer::NvAudioALSARenderer(NvAudioALSADevice *parent) :
    m_Parent(parent),
    m_Timeoutns(0),
    m_MixStreamCount(0),
    m_threadExit(false),
    mInit(false)
{
    ALOGV("NvAudioALSARenderer");

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        m_Handle[i] = (NvAudioALSADev*)malloc(sizeof(NvAudioALSADev));
        if (!m_Handle[i]) {
            ALOGE("Failed to allocate NvAudioALSADev \n");
            return;
        }

        memset(m_Handle[i], 0, sizeof(NvAudioALSADev));
        m_Handle[i]->streamType = SND_PCM_STREAM_PLAYBACK;
        m_Handle[i]->format = TEGRA_DEFAULT_OUT_FORMAT;
        m_Handle[i]->channels = TEGRA_DEFAULT_OUT_CHANNELS;
        m_Handle[i]->sampleRate = TEGRA_DEFAULT_OUT_SAMPLE_RATE;
        m_Handle[i]->curMode = AudioSystem::MODE_NORMAL;
        m_Handle[i]->max_out_channels = TEGRA_DEFAULT_OUT_CHANNELS;

#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
        if (i == WFD_PCM_INDEX) {
            m_Handle[i]->device = m_Parent->m_WFDDev;
            m_Handle[i]->validDev = m_Handle[i]->curDev =
                                    AudioSystem::DEVICE_OUT_WIFI_DISPLAY;
        } else {
            m_Handle[i]->device = m_Parent->m_ALSADev;
            if (i == AUX_PCM_INDEX)
                m_Handle[i]->validDev = m_Handle[i]->curDev =
                                        AudioSystem::DEVICE_OUT_AUX_DIGITAL;
            else {
                m_Handle[i]->validDev = AudioSystem::DEVICE_OUT_ALL &
                                        ~(AudioSystem::DEVICE_OUT_WIFI_DISPLAY |
                                        AudioSystem::DEVICE_OUT_AUX_DIGITAL);
                m_Handle[i]->curDev = AudioSystem::DEVICE_OUT_DEFAULT;
            }
        }
#else
        m_Handle[i]->device = m_Parent->m_ALSADev;
        if (i == AUX_PCM_INDEX)
            m_Handle[i]->validDev = m_Handle[i]->curDev =
                                    AudioSystem::DEVICE_OUT_AUX_DIGITAL;
        else {
                m_Handle[i]->validDev = AudioSystem::DEVICE_OUT_ALL &
                                        ~(AudioSystem::DEVICE_OUT_AUX_DIGITAL);
                m_Handle[i]->curDev = AudioSystem::DEVICE_OUT_DEFAULT;
        }
#endif
        if (m_Handle[i]->device->default_sampleRate)
            m_Handle[i]->sampleRate = m_Handle[i]->device->default_sampleRate;
    }

    m_AvailableDevices = AudioSystem::DEVICE_OUT_EARPIECE |
                        AudioSystem::DEVICE_OUT_SPEAKER;

    char prop_val[PROPERTY_VALUE_MAX];
    sprintf(prop_val, "%d", TEGRA_DEFAULT_OUT_CHANNELS);
    property_set(TEGRA_MAX_OUT_CHANNELS_PROPERTY, prop_val);

    memset(prop_val, 0, PROPERTY_VALUE_MAX * sizeof(char));
    property_set(TEGRA_CHANNEL_MAP_PROPERTY, prop_val);

    createThreadEtc(MixThread, this, "Mixer Thread");
    mInit = true;
}

NvAudioALSARenderer::~NvAudioALSARenderer()
{
    ALOGV("~NvAudioALSARenderer");

    AutoMutex lock(m_Lock);

    m_threadExit = true;
    m_BufferReady.signal();
    m_WriteDone.wait(m_Lock);

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if (m_Handle[i]) {
                m_Handle[i]->device->close(m_Handle[i]);
            free(m_Handle[i]);
            m_Handle[i] = 0;
        }
    }

    for(NvAudioALSABufferList::iterator it = m_FreeTmpBufferList.begin();
        it != m_FreeTmpBufferList.end(); ++it) {
        free((*it)->pData);
        (*it)->pData = NULL;
        free(*it);
        m_FreeTmpBufferList.erase(it);
    }
}

status_t NvAudioALSARenderer::initCheck()
{
    return mInit ? NO_ERROR : NO_INIT;
}

int NvAudioALSARenderer::MixThread(void* p)
{
    ALOGV("MixThread");
    return ((NvAudioALSARenderer*)p)->MixThreadLoop();
}

void NvAudioALSARenderer::start(NvAudioALSAStreamOut* pStream)
{
    ALOGV("start");
    AutoMutex lock(m_Lock);
    uint32_t device = pStream->getDevice();
    uint64_t timeoutns = 0;
    status_t err  = 0;
    char prop_val[PROPERTY_VALUE_MAX];
    NvAudioALSAControl *alsaHDACtrl =
                                m_Handle[AUX_PCM_INDEX]->device->alsaHDAControl;

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if (!(device & m_Handle[i]->validDev))
            continue;

        m_Handle[i]->activeCount++;
        m_MixStreamCount = MAX(m_MixStreamCount, m_Handle[i]->activeCount);

        if (m_Handle[i]->hpcm) {
            timeoutns = (m_Handle[i]->latency /
                                m_Handle[i]->periods) * 1000;
            m_Timeoutns = MAX(m_Timeoutns, timeoutns);
        }

        if (i == AUX_PCM_INDEX) {
            uint32_t srcCh = popCount(pStream->channels());
            if ((m_Handle[i]->channels < srcCh) &&
                (m_Handle[i]->max_out_channels >= srcCh)) {
                if (m_Handle[i]->hpcm)
                    m_Handle[i]->device->close(m_Handle[i]);
                m_Handle[i]->channels = srcCh;
            }
            if (alsaHDACtrl) {
                err = alsaHDACtrl->get ("HDA Decode Capability",
                                         m_Handle[i]->recDecCapable,0);
                if (err < 0)
                    ALOGE("HDA Query has failed");

                sprintf(prop_val, "%d",
                        !!(m_Handle[i]->recDecCapable & REC_DEC_CAP_AC3));
                property_set(TEGRA_HW_AC3DEC_PROPERTY, prop_val);

                sprintf(prop_val, "%d",
                        !!(m_Handle[i]->recDecCapable & REC_DEC_CAP_DTS));
                property_set(TEGRA_HW_DTSDEC_PROPERTY, prop_val);
            }
        }
        else if (i == OUT_PCM_INDEX)
            m_Handle[i]->device->isPlaybackActive = true;
    }
}

ssize_t NvAudioALSARenderer::write(NvAudioALSAStreamOut* pStream, NvAudioALSABuffer *pBuf)
{
    AutoMutex lock(m_Lock);

    uint32_t device = pStream->getDevice();
    uint32_t mixTriggered = 0;

    // ALOGV("write:size = %d channel = %d device = %d",
    //      pBuf->dataSize, pBuf->channels, device);

    if ((pBuf->volume[0] != MAX_Q15) &&
            (pBuf->volume[1] != MAX_Q15) &&
            (AudioSystem::PCM_16_BIT == pStream->format())) {
        applyVolume(pBuf);
    }

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if (!(device & m_Handle[i]->curDev))
            continue;

        if (pStream->sampleRate() != m_Handle[i]->sampleRate)
            continue;

        if (!m_Handle[i]->hpcm) {
            status_t err = m_Handle[i]->device->open(m_Handle[i],
                                                     m_Handle[i]->curDev,
                                                     m_Parent->mode());
            if (err < 0) {
                if (i == AUX_PCM_INDEX || err == -EBUSY) {
                    // AUX device open may fail few times. In such cases Sleep
                    // for the duration of buffer instead of returning error.
                    usleep(((uint64_t)pBuf->dataSize * 1000000) /
                            (pBuf->sampleSize * m_Handle[i]->sampleRate));
                    return pBuf->dataSize;
                }
                ALOGE("Failed to open alsa device for output stream: %d", err);
                return err;
            }
            uint64_t timeoutns = (m_Handle[i]->latency / m_Handle[i]->periods)
                                                     * 1000;
            m_Timeoutns = MAX(m_Timeoutns, timeoutns);

            if (i == AUX_PCM_INDEX) {
                char prop_val[PROPERTY_VALUE_MAX];

                sprintf(prop_val, "%d",
                        !!(m_Handle[i]->recDecCapable & REC_DEC_CAP_AC3));
                property_set(TEGRA_HW_AC3DEC_PROPERTY, prop_val);

                sprintf(prop_val, "%d",
                        !!(m_Handle[i]->recDecCapable & REC_DEC_CAP_DTS));
                property_set(TEGRA_HW_DTSDEC_PROPERTY, prop_val);

                sprintf(prop_val, "%d", m_Handle[i]->max_out_channels);
                property_set(TEGRA_MAX_OUT_CHANNELS_PROPERTY, prop_val);
            }
        }

        // no mixing when there is only one stream or the stream contains
        // compressed audio
        if (m_Handle[i]->activeCount == 1 ||
            (pStream->format() != AudioSystem::PCM_16_BIT)) {
            if (pBuf->channels == m_Handle[i]->channels) {
                // bypass
                if ((pBuf->channels > 2) &&
                    (pBuf->channelMap !=
                        TegraDstChannelMap[m_Handle[i]->channels - 1]))
                    shuffleChannels(pBuf,
                        TegraDstChannelMap[m_Handle[i]->channels - 1]);

                m_Lock.unlock();
                m_Handle[i]->device->write(m_Handle[i], pBuf->pData,
                                           pBuf->dataSize);
                m_Lock.lock();
            }
            else {
                // downsampling/upsampling without mixing
                NvAudioALSABuffer* pTmpBuf = getTempBuffer();

                pTmpBuf->channels = m_Handle[i]->channels;
                pTmpBuf->channelMap =
                        TegraDstChannelMap[m_Handle[i]->channels - 1];
                pTmpBuf->dataSize = 0;

                if (pBuf->channels < pTmpBuf->channels)
                    upmix(pBuf, pTmpBuf);
                else
                    downmix(pBuf, pTmpBuf);

                m_Lock.unlock();
                m_Handle[i]->device->write(m_Handle[i], pTmpBuf->pData,
                                           pTmpBuf->dataSize);
                m_Lock.lock();

                releaseTempBuffer(pTmpBuf);
            }
        }
        else if (!mixTriggered) {
            // mixing
            m_QueuedList.push_back(pBuf);
            m_BufferReady.signal();
            mixTriggered = 1;
        }
    }

    if (mixTriggered)
        m_WriteDone.wait(m_Lock);

    return pBuf->dataSize;
}

void NvAudioALSARenderer::stop(NvAudioALSAStreamOut* pStream)
{
    ALOGV("stop");
    AutoMutex lock(m_Lock);
    uint32_t device = pStream->getDevice();

    if (!pStream->isActive())
        return;

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if (!(device & m_Handle[i]->validDev) || !m_Handle[i]->activeCount)
            continue;

        m_Handle[i]->activeCount--;
        if (m_Handle[i]->hpcm && !m_Handle[i]->activeCount) {
            if ((i == OUT_PCM_INDEX) &&
               !(((m_Parent->mode() == AudioSystem::MODE_IN_CALL) ||
               (m_Parent->mode() == AudioSystem::MODE_IN_COMMUNICATION)) &&
               !(m_Handle[i]->curDev & DEVICE_OUT_BLUETOOTH_SCO_ALL))) {
                NvAudioALSADev *VoicePBDev =
                    m_Parent->m_VoiceCallAlsaDev[SND_PCM_STREAM_PLAYBACK];

                m_Handle[i]->device->isPlaybackActive = false;
                m_Handle[i]->device->close(m_Handle[i]);

                // check for playback voice device, if its open then close it
                if (VoicePBDev && VoicePBDev->hpcm)
                    VoicePBDev->device->close(VoicePBDev);
            }

            if ((i == AUX_PCM_INDEX) &&
                (m_AvailableDevices & m_Handle[i]->validDev))
                m_Handle[i]->isSilent = true;
            else if ((i != OUT_PCM_INDEX) && m_Handle[i]->hpcm)
                m_Handle[i]->device->close(m_Handle[i]);
        }
    }

    m_Timeoutns = 0;
    m_MixStreamCount = 0;
    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if (m_Handle[i]->hpcm) {
            uint64_t timeoutns = (m_Handle[i]->latency /
                            m_Handle[i]->periods) * 1000;
            m_Timeoutns = MAX(m_Timeoutns, timeoutns);
        }
        m_MixStreamCount = MAX(m_MixStreamCount, m_Handle[i]->activeCount);
    }
}

void NvAudioALSARenderer::applyVolume(NvAudioALSABuffer *pBuf)
{
    int16_t* pData = (int16_t*)pBuf->pData;
    uint32_t ch = pBuf->channels;
    uint32_t frames = (pBuf->dataSize / pBuf->sampleSize);
    uint32_t* vol = pBuf->volume;

    do {
        for (uint32_t i = 0; i < ch; i++)
            pData[i] = (int16_t)(((int32_t)pData[i] * vol[i]) >> SHIFT_Q15);
        pData += ch;
    } while(--frames);
}

status_t NvAudioALSARenderer::shuffleChannels(NvAudioALSABuffer *pBuf,
                                          uint32_t dstChMap)
{
    int16_t* pData = (int16_t*)pBuf->pData;
    uint32_t ch = pBuf->channels;
    uint32_t srcChMap = pBuf->channelMap;
    uint32_t frames = (pBuf->dataSize / pBuf->sampleSize);
    int16_t tmp[TEGRA_AUDIO_CHANNEL_ID_LAST] = {0};

    if (srcChMap == dstChMap)
        return -EINVAL;

    do {
        uint32_t i, mask;
        for (i = 0, mask = 0xF; i < ch; i++, mask <<= 4) {
            uint32_t srcChID = srcChMap & mask;
            if ((dstChMap & mask) == srcChID)
                continue;

            tmp[srcChID >> (i << 2)] = pData[i];
        }

        for (i = 0, mask = 0xF; i < ch; i++, mask <<= 4) {
            uint32_t dstChID = dstChMap & mask;
            if ((srcChMap & mask) == dstChID)
                continue;

            pData[i] = tmp[dstChID >> (i << 2)];
        }
        pData += ch;
    } while(--frames);

    pBuf->channelMap = dstChMap;
    return NO_ERROR;
}

/* Zero fill unused channels */
status_t NvAudioALSARenderer::upmix(NvAudioALSABuffer *pSrcBuf,
                                        NvAudioALSABuffer *pDstBuf)
{
    int16_t* pSrc = (int16_t*)pSrcBuf->pData;
    int16_t* pDst = (int16_t*)pDstBuf->pData;

    uint32_t srcCh = pSrcBuf->channels;
    uint32_t dstCh = pDstBuf->channels;

    uint32_t srcChMap = pSrcBuf->channelMap;
    uint32_t dstChMap = pDstBuf->channelMap;

    if (srcCh >= dstCh)
        return -EINVAL;

    int frames = (pSrcBuf->dataSize / pSrcBuf->sampleSize);

    pDstBuf->sampleSize = ((pSrcBuf->sampleSize / srcCh) * dstCh);
    pDstBuf->dataSize = frames * pDstBuf->sampleSize;

    if (pDstBuf->dataSize > pDstBuf->size) {
        pDstBuf->dataSize = 0;
        return -ENOMEM;
    }

    memset((void*)pDst, 0, pDstBuf->dataSize);
    if ((dstChMap & channelMask[srcCh - 1]) == srcChMap) {
        do {
            memcpy(pDst, pSrc, srcCh * sizeof(int16_t));

            pSrc += srcCh;
            pDst += dstCh;
        } while (--frames);
    }
    else {
        int dstIndexTab[8] = {0};
        for (uint32_t i = 0, mask = 0xF; i < srcCh; i++, mask <<= 4) {
            if ((srcChMap & mask) == (dstChMap & mask)) {
                dstIndexTab[i] = i;
                continue;
            }

            for (uint32_t j = 0, mask2 = 0xF; j < dstCh; j++, mask2 <<= 4) {
                if (((srcChMap & mask) >> (i << 2)) ==
                    ((dstChMap & mask2) >> (j << 2))) {
                    dstIndexTab[i] = j;
                    break;
                }
            }
        }

        do {
            for (uint32_t i = 0; i < srcCh; i++)
               pDst[dstIndexTab[i]] = pSrc[i];

            pSrc += srcCh;
            pDst += dstCh;
        } while(--frames);
    }

    pDstBuf->devices = pSrcBuf->devices;
    return NO_ERROR;
}

status_t NvAudioALSARenderer::downmix(NvAudioALSABuffer *pSrcBuf,
                                        NvAudioALSABuffer *pDstBuf)
{
    int16_t* pSrc = (int16_t*)pSrcBuf->pData;
    int16_t* pDst = (int16_t*)pDstBuf->pData;

    uint32_t srcCh = pSrcBuf->channels;
    uint32_t dstCh = pDstBuf->channels;

    uint32_t srcChMap = pSrcBuf->channelMap;
    uint32_t dstChMap = pDstBuf->channelMap;

    if (srcCh <= dstCh)
        return -EINVAL;

    int frames = (pSrcBuf->dataSize / pSrcBuf->sampleSize);

    pDstBuf->sampleSize = ((pSrcBuf->sampleSize / srcCh) * dstCh);
    pDstBuf->dataSize = frames * pDstBuf->sampleSize;

    if (pDstBuf->dataSize > pDstBuf->size) {
        pDstBuf->dataSize = 0;
        return -ENOMEM;
    }

    memset((void*)pDst, 0, pDstBuf->dataSize);
    if (dstCh == 2) {
        do {
            for (uint32_t i = 0; i < dstCh; i++) {
                int32_t tmp = 0;
                for (uint32_t j = 0; j < srcCh; j++) {
                    int index = ((srcChMap >> (j << 2)) & 0xF) - 1;
                    if (index >= 0)
                        tmp += (((int32_t)pSrc[j] *
                            pSrcBuf->stereoDownmixTab[index][i]) >> SHIFT_Q15);
                }
                pDst[i] = sat16(tmp);
            }
            pSrc += srcCh;
            pDst += dstCh;
        } while(--frames);
    }
    else {
        do {
            for (uint32_t i = 0, mask = 0xF; i < dstCh; i++, mask <<= 4) {
                int32_t tmp = 0;
                if ((dstChMap & mask) == LFE)
                    continue;

                for (uint32_t j = 0, mask2 = 0xF; j < srcCh; j++, mask2 <<= 4) {
                    if ((srcChMap & mask2) != LFE)
                         tmp += pSrc[j];
                }
                pDst[i] = sat16(tmp);
            }
            pSrc += srcCh;
            pDst += dstCh;
        } while(--frames);
    }
    pDstBuf->devices = pSrcBuf->devices;
    return NO_ERROR;
}

status_t NvAudioALSARenderer::mix(NvAudioALSABuffer *pSrcBuf,
                                        NvAudioALSABuffer *pDstBuf)
{
    int16_t* pSrc = (int16_t*)pSrcBuf->pData;
    int16_t* pDst = (int16_t*)pDstBuf->pData;

    uint32_t srcCh = pSrcBuf->channels;
    uint32_t dstCh = pDstBuf->channels;

    if (srcCh != dstCh)
        return -EINVAL;

    int frames = (pSrcBuf->dataSize / pSrcBuf->sampleSize);

    pDstBuf->sampleSize = ((pSrcBuf->sampleSize / srcCh) * dstCh);
    pDstBuf->dataSize = frames * pDstBuf->sampleSize;

    if (pDstBuf->dataSize > pDstBuf->size) {
        pDstBuf->dataSize = 0;
        return -ENOMEM;
    }

    do {
        for (uint32_t i = 0; i < srcCh; i++)
            pDst[i] = sat16((int32_t)pDst[i] + (int32_t)pSrc[i]);

        pSrc += srcCh;
        pDst += dstCh;
    } while(--frames);

    pDstBuf->devices |= pSrcBuf->devices;
    return NO_ERROR;
}

status_t NvAudioALSARenderer::open(int *format,
                               uint32_t *channels,
                               uint32_t *rate,
                               uint32_t devices)
{
    status_t err = NO_ERROR;
    char prop_val[PROPERTY_VALUE_MAX];
    uint32_t maxChannels = 0;

    ALOGV("open: format 0x%x, channels 0x%x, rate %d, devices 0x%x",
        *format, *channels, *rate, devices);

    if (!format || !channels || !rate)
        return -EINVAL;

    AutoMutex lock(m_Lock);

    if (!(*format))
        *format = getAudioSystemFormat(TEGRA_DEFAULT_OUT_FORMAT);

    if (!(*channels))
        *channels = getAudioSystemChannels(TEGRA_DEFAULT_OUT_CHANNELS);

    if (!(*rate))
        *rate = m_Handle[OUT_PCM_INDEX]->device->default_sampleRate ?
            m_Handle[OUT_PCM_INDEX]->device->default_sampleRate :
            TEGRA_DEFAULT_OUT_SAMPLE_RATE;

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        uint32_t newDevice = devices & m_Handle[i]->validDev;
        if (!newDevice)
            continue;

    if (!m_Handle[i]->hpcm) {
            // set format to PCM even for compressed stream
            m_Handle[i]->format = (*format == AudioSystem::PCM_SUB_8_BIT) ?
                SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_S16_LE;
            m_Handle[i]->channels = popCount(*channels);
            m_Handle[i]->sampleRate = *rate;

            err = m_Handle[i]->device->open(m_Handle[i], newDevice,
                                            m_Parent->mode());
            if (err < 0) {
                if (i == AUX_PCM_INDEX) {
                   uint32_t timeOutMs = 5000;
                   uint32_t timeWaitMs = 0, timeWaitStemMs = 20;
                   // For AUX device failure to open few times is acceptable
                   while ((err < 0) && (timeWaitMs <= timeOutMs)) {
                       usleep(timeWaitStemMs * 1000);
                       timeOutMs += timeWaitStemMs;

                       err = m_Handle[i]->device->open(m_Handle[i], newDevice,
                                                       m_Parent->mode());
                   }
                }
              if (err < 0) {
                 ALOGE("Failed to open alsadev with supplied params.\
                    Trying to open with default params");
                m_Handle[i]->format = TEGRA_DEFAULT_OUT_FORMAT;
                m_Handle[i]->channels =  TEGRA_DEFAULT_OUT_CHANNELS;
                m_Handle[i]->sampleRate =
                    m_Handle[i]->device->default_sampleRate ?
                    m_Handle[i]->device->default_sampleRate :
                    TEGRA_DEFAULT_OUT_SAMPLE_RATE;

                err = m_Handle[i]->device->open(m_Handle[i], newDevice,
                                                m_Parent->mode());
                if (err < 0) {
                    ALOGE("Failed to open alsa device with default params\n");
                    return err;
                }
                *format = getAudioSystemFormat(m_Handle[i]->format);
                *rate = m_Handle[i]->sampleRate;
            }
          }

            err = m_Handle[i]->device->route(m_Handle[i], newDevice,
                                                 m_Parent->mode());
            if (err < 0) {
                ALOGE("Failed to route to %d device\n", newDevice);
                return err;
            }
            // Close ALSA device for power saving
            if (i == OUT_PCM_INDEX)
                m_Handle[i]->device->close(m_Handle[i]);
        }

        //TODO: format change without closing alsa device
        if (m_Handle[i]->hpcm) {
            bool isFormatChange = false;
            if (i == AUX_PCM_INDEX) {
                if ((*format == AudioSystem::PCM_16_BIT) &&
                        m_Handle[i]->isCompressed) {
                    isFormatChange = true;
                    m_Handle[i]->isCompressed = false;
                    ALOGV("Format changed to PCM");
                }
                else if ((*format != AudioSystem::PCM_16_BIT) &&
                        !m_Handle[i]->isCompressed) {
                    isFormatChange = true;
                    m_Handle[i]->isCompressed = true;
                    ALOGV("Format changed to 0x%x", *format);
                }
            }

#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
            if (i == WFD_PCM_INDEX) {
                /* WFD cannot change stream format runtime */
                *rate = m_Handle[i]->sampleRate;
                *format = (m_Handle[i]->format == SND_PCM_FORMAT_S8) ?
                    AudioSystem::PCM_SUB_8_BIT : AudioSystem::PCM_SUB_16_BIT;
            }
            else if (popCount(*channels) &&
#else
            if (popCount(*channels) &&
#endif
                (m_Handle[i]->channels < popCount(*channels)) &&
                (m_Handle[i]->max_out_channels >= popCount(*channels))) {
                m_Handle[i]->device->close(m_Handle[i]);
            }
            else if ((*rate != m_Handle[i]->sampleRate) || isFormatChange) {
                m_Handle[i]->sampleRate = *rate;
                m_Handle[i]->device->close(m_Handle[i]);
            }
            else {
                *rate = m_Handle[i]->sampleRate;
                if (!m_Handle[i]->isCompressed && audio_is_linear_pcm(*format)) {
                    // update format only for LPCM streams
                    *format = (m_Handle[i]->format == SND_PCM_FORMAT_S8) ?
                        AudioSystem::PCM_8_BIT : AudioSystem::PCM_16_BIT;
                }
            }
        }

        maxChannels = MAX(maxChannels, m_Handle[i]->max_out_channels);

        if (i == AUX_PCM_INDEX) {
            sprintf(prop_val, "%d",!!(m_Handle[i]->recDecCapable
                    & REC_DEC_CAP_AC3));
            property_set(TEGRA_HW_AC3DEC_PROPERTY, prop_val);

            sprintf(prop_val, "%d",!!(m_Handle[i]->recDecCapable
                    & REC_DEC_CAP_DTS));
            property_set(TEGRA_HW_DTSDEC_PROPERTY, prop_val);
        }
    }

#ifdef TEGRA_TEST_LIBAUDIO
    {
        char prop_test_mc[PROPERTY_VALUE_MAX] = "";
        property_get(TEGRA_TEST_LIBAUDIO_PROPERTY, prop_test_mc, "");

        if (!strcmp(prop_test_mc, "1")) {
            property_get(TEGRA_MAX_OUT_CHANNELS_PROPERTY, prop_val, "");
            maxChannels = atoi(prop_val);
        }
    }
#endif
    sprintf(prop_val, "%d", maxChannels);
    property_set(TEGRA_MAX_OUT_CHANNELS_PROPERTY, prop_val);

    return NO_ERROR;
}

status_t NvAudioALSARenderer::setParameters(NvAudioALSAStreamOut* pStream,
                                            const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    status_t status = NO_ERROR;
    int keyVal;
    ALOGV("setParameters() %s", keyValuePairs.string());

    AutoMutex lock(m_Lock);

    if (param.getInt(key, keyVal) == NO_ERROR) {
        char prop_val[PROPERTY_VALUE_MAX];
        uint32_t maxChannels = 0;
        uint32_t newDevice = keyVal;
        uint32_t oldDevice = pStream->getDevice();

        ALOGV("Switch audio to device :0x%x -> 0x%x \n", oldDevice, newDevice);

        for (int i = 0; i < MAX_PCM_INDEX; i++) {
            if ((newDevice & m_Handle[i]->validDev) &&
                (pStream->sampleRate() != m_Handle[i]->sampleRate)) {
                m_Handle[i]->sampleRate = pStream->sampleRate();
                if (m_Handle[i]->hpcm) {
                    m_Handle[i]->device->close(m_Handle[i]);
                    m_Handle[i]->device->open(m_Handle[i],
                                        (uint32_t)newDevice & m_Handle[i]->validDev,
                                        m_Parent->mode());
                }
            }

            if ((oldDevice & m_Handle[i]->validDev) ==
                (newDevice & m_Handle[i]->validDev)) {
                if (newDevice & m_Handle[i]->validDev)
                    maxChannels = MAX(maxChannels,
                                      m_Handle[i]->max_out_channels);
                continue;
            }

            if (pStream->isActive()) {
                if (oldDevice & m_Handle[i]->validDev)
                    m_Handle[i]->activeCount--;
                if (newDevice & m_Handle[i]->validDev)
                    m_Handle[i]->activeCount++;
            }

            if (m_Handle[i]->hpcm && !m_Handle[i]->activeCount) {
                if ((i == AUX_PCM_INDEX) &&
                    (m_AvailableDevices & m_Handle[i]->curDev))
                    m_Handle[AUX_PCM_INDEX]->isSilent = true;
                else
                    m_Handle[i]->device->close(m_Handle[i]);
            }
            else if (!m_Handle[i]->hpcm && m_Handle[i]->activeCount) {
                if (i == AUX_PCM_INDEX)
                    m_Handle[i]->device->open(m_Handle[i],
                                    (uint32_t)newDevice & m_Handle[i]->validDev,
                                    m_Parent->mode());
                m_Handle[AUX_PCM_INDEX]->isCompressed = false;
            }

            if (i == AUX_PCM_INDEX) {
                sprintf(prop_val, "%d", !!(m_Handle[i]->recDecCapable
                        & REC_DEC_CAP_AC3) && m_Handle[i]->activeCount);
                property_set(TEGRA_HW_AC3DEC_PROPERTY, prop_val);

                sprintf(prop_val, "%d", !!(m_Handle[i]->recDecCapable
                        & REC_DEC_CAP_DTS) && m_Handle[i]->activeCount);
                property_set(TEGRA_HW_DTSDEC_PROPERTY, prop_val);
            }

            m_Handle[i]->device->route(m_Handle[i],
                        (uint32_t)newDevice & m_Handle[i]->validDev,
                        m_Parent->mode());

            if ((i == OUT_PCM_INDEX) && m_Handle[i]->activeCount)
                m_Handle[i]->device->isPlaybackActive = true;
            else if ((i == OUT_PCM_INDEX) && !m_Handle[i]->activeCount)
                m_Handle[i]->device->isPlaybackActive = false;

            if (newDevice & m_Handle[i]->curDev)
                maxChannels = MAX(maxChannels, m_Handle[i]->max_out_channels);
         }

        m_MixStreamCount = 0;
        for (int i = 0; i < MAX_PCM_INDEX; i++)
            m_MixStreamCount = MAX(m_MixStreamCount, m_Handle[i]->activeCount);

        sprintf(prop_val, "%d", maxChannels);
        property_set(TEGRA_MAX_OUT_CHANNELS_PROPERTY, prop_val);

        AudioParameter param = AudioParameter();
        param.addInt(String8(AudioParameter::keyRouting), keyVal);
        m_Parent->setParameters(param.toString());

        param.remove(key);
    }

    key = String8("output_available");
    if (param.getInt(key, keyVal) == NO_ERROR) {
        ALOGV("Available output device :0x%x ->0x%x\n",
            m_AvailableDevices, keyVal);

        if ((keyVal & AudioSystem::DEVICE_OUT_AUX_DIGITAL) &&
                !(m_AvailableDevices & AudioSystem::DEVICE_OUT_AUX_DIGITAL)) {
            // AUX is plugged in
            if (!m_Handle[AUX_PCM_INDEX]->hpcm) {
                status = m_Handle[AUX_PCM_INDEX]->device->open(
                                        m_Handle[AUX_PCM_INDEX],
                                        AudioSystem::DEVICE_OUT_AUX_DIGITAL,
                                        m_Parent->mode());
                m_Handle[AUX_PCM_INDEX]->isCompressed = false;
            }
        }
        else if (!(keyVal & AudioSystem::DEVICE_OUT_AUX_DIGITAL) &&
                (m_AvailableDevices & AudioSystem::DEVICE_OUT_AUX_DIGITAL) &&
                !m_Handle[AUX_PCM_INDEX]->activeCount) {
            // AUX is plugged out
            m_Handle[AUX_PCM_INDEX]->device->close(m_Handle[AUX_PCM_INDEX]);
            m_Handle[AUX_PCM_INDEX]->isCompressed = false;
            m_Handle[AUX_PCM_INDEX]->recDecCapable = 0;

            char prop_val[PROPERTY_VALUE_MAX];
            sprintf(prop_val, "%d", m_Handle[AUX_PCM_INDEX]->recDecCapable);
            property_set(TEGRA_HW_AC3DEC_PROPERTY, prop_val);
            property_set(TEGRA_HW_DTSDEC_PROPERTY, prop_val);
        }
        m_AvailableDevices = keyVal;
        param.remove(key);
    }

    key = String8(AudioParameter::keySamplingRate);
    if (param.getInt(key, keyVal) == NO_ERROR) {
        ALOGV("Switch audio samplerate to :%d\n", keyVal);

        //TODO: samplerate change without closing alsa device
        for (int i = 0; i < MAX_PCM_INDEX; i++) {
#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
            /* WFD does not support runtime samplerate change */
            if (i == WFD_PCM_INDEX)
                continue;
#endif

            if (!m_Handle[i]->hpcm)
                m_Handle[i]->sampleRate = keyVal;
            else if (m_Handle[i]->sampleRate != (uint32_t) keyVal){
                m_Handle[i]->device->close(m_Handle[i]);
                m_Handle[i]->sampleRate = keyVal;
                status = m_Handle[i]->device->open(m_Handle[i],
                                            m_Handle[i]->curDev,
                                            m_Parent->mode());
            }
        }
        param.remove(key);
    }

    if (param.size()) {
        status = BAD_VALUE;
    }

    return status;
}

int NvAudioALSARenderer::MixThreadLoop(void)
{
    NvAudioALSABuffer* pSrcBuf = 0;
    NvAudioALSABuffer* pMixBuf = 0;
    status_t ret = NO_ERROR;

    AutoMutex lock(m_Lock);

    pMixBuf = getTempBuffer();
    while(!m_threadExit) {
        do {
            ret = NO_ERROR;
            if (m_MixStreamCount &&
                (m_QueuedList.size() >= m_MixStreamCount))
                break;
            else if (m_QueuedList.size())  {
                ret = m_BufferReady.waitRelative(m_Lock, m_Timeoutns);
                if (ret == -ETIMEDOUT)
                    break;
            }
            else
                m_BufferReady.wait(m_Lock);
        } while (!m_threadExit);

        if (m_threadExit)
            break;

        for (int i = 0; i < MAX_PCM_INDEX; i++) {
            if((m_Handle[i]->activeCount <= 1) || m_Handle[i]->isCompressed)
                continue;

            pMixBuf->dataSize = 0;
            for(NvAudioALSABufferList::iterator it = m_QueuedList.begin();
                it != m_QueuedList.end(); ++it) {
                pSrcBuf = *it;

                if (!pSrcBuf || !(pSrcBuf->devices & m_Handle[i]->curDev))
                    continue;

                if (pSrcBuf->channels == m_Handle[i]->channels) {
                    pMixBuf->channels = m_Handle[i]->channels;
                    pMixBuf->channelMap =
                        TegraDstChannelMap[m_Handle[i]->channels - 1];
                    if ((pSrcBuf->channels > 2) &&
                        (pSrcBuf->channelMap != pMixBuf->channelMap))
                        shuffleChannels(pSrcBuf, pMixBuf->channelMap);

                    mix(pSrcBuf, pMixBuf);
                }
                else {
                    // upmixing/downmixing
                    NvAudioALSABuffer* pTmpBuf = getTempBuffer();

                    pTmpBuf->channels = m_Handle[i]->channels;
                    pMixBuf->channels = m_Handle[i]->channels;
                    pTmpBuf->channelMap =
                        TegraDstChannelMap[m_Handle[i]->channels - 1];
                    pMixBuf->channelMap =
                        TegraDstChannelMap[m_Handle[i]->channels - 1];
                    pTmpBuf->dataSize = 0;

                    if (pSrcBuf->channels < pTmpBuf->channels)
                        upmix(pSrcBuf, pTmpBuf);
                    else
                        downmix(pSrcBuf, pTmpBuf);

                    mix(pTmpBuf, pMixBuf);

                    releaseTempBuffer(pTmpBuf);
                }
            }
            m_Lock.unlock();
            m_Handle[i]->device->write(m_Handle[i], pMixBuf->pData,
                                       pMixBuf->dataSize);
            m_Lock.lock();
            memset(pMixBuf->pData, 0, pMixBuf->dataSize);
            pMixBuf->devices = 0;
        }
        m_QueuedList.clear();
        m_WriteDone.broadcast();
    }
    releaseTempBuffer(pMixBuf);
    m_WriteDone.signal();

    return NO_ERROR;
}

int NvAudioALSARenderer::getAudioSystemFormat(snd_pcm_format_t format)
{
    switch (format) {
        case SND_PCM_FORMAT_S8:
            return AudioSystem::PCM_SUB_8_BIT;
        default:
            ALOGE("getAudioSystemFormat: unsupported format %x", format);
        case SND_PCM_FORMAT_S16_LE:
            return AudioSystem::PCM_SUB_16_BIT;
    }
}

uint32_t NvAudioALSARenderer::getAudioSystemChannels(uint32_t channels)
{
    switch (channels) {
        case 1:
            return AudioSystem::CHANNEL_OUT_MONO;
        case 4:
            return AudioSystem::CHANNEL_OUT_SURROUND;
        case 6:
            return AudioSystem::CHANNEL_OUT_5POINT1;
        case 8:
            return AudioSystem::CHANNEL_OUT_7POINT1;
        default:
            ALOGE("getAudioSystemChannels: unsupported channels %x", channels);
        case 2:
            return AudioSystem::CHANNEL_OUT_STEREO;
    }
}

NvAudioALSABuffer* NvAudioALSARenderer::getTempBuffer()
{
    NvAudioALSABuffer* pBuf = NULL;

    if (m_FreeTmpBufferList.size()) {
        NvAudioALSABufferList::iterator iter = m_FreeTmpBufferList.begin();

        pBuf = *iter;
        m_FreeTmpBufferList.erase(iter);
    }
    else {
        pBuf = (NvAudioALSABuffer*)malloc(sizeof(NvAudioALSABuffer));
        if (!pBuf) {
            ALOGE("getTempBuffer :: Failed to allocate NvAudioALSABuffer\n");
            return NULL;
        }
        memset(pBuf, 0, sizeof(NvAudioALSABuffer));

        pBuf->pData = (void*)malloc(TEGRA_MAX_MIX_BUFFER_SIZE * sizeof(char));
        if (!pBuf->pData) {
            ALOGE("getTempBuffer :: Failed to allocate data buffer\n");
            return NULL;
        }
        pBuf->size = TEGRA_MAX_MIX_BUFFER_SIZE;
        pBuf->devices = 0;
    }

    return pBuf;
}

void NvAudioALSARenderer::releaseTempBuffer(NvAudioALSABuffer* pBuf)
{
    if (pBuf && pBuf->pData)
        m_FreeTmpBufferList.push_back(pBuf);
}

NvAudioALSADev* NvAudioALSARenderer::getALSADev(uint32_t index)
{
    return m_Handle[index];
}

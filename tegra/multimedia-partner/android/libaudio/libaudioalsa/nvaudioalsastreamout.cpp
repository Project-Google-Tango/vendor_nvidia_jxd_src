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
 ** Based upon AudioStreamOutALSA.cpp, provided under the following terms:
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

#define LOG_TAG "NvAudioALSAStreamOut"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"
using namespace android;

// ----------------------------------------------------------------------------
#ifdef TEGRA_TEST_LIBAUDIO
void crcInit(unsigned int* crc);
void crcCompute(unsigned char octet, unsigned int* crc);
void crcComplete(unsigned int* crc);
#endif

NvAudioALSAStreamOut::NvAudioALSAStreamOut(NvAudioALSADevice *parent) :
    m_Parent(parent),
    m_Renderer(parent->m_Renderer),
    m_Started(false),
    m_FrameCount(0),
    m_Devices(0),
    m_Channels(0),
    m_Format(0),
    m_SampleRate(0)
{
}

NvAudioALSAStreamOut::~NvAudioALSAStreamOut()
{
}

uint32_t NvAudioALSAStreamOut::channels() const
{
    ALOGV("channels %x", m_Channels);
    return m_Channels;
}

status_t NvAudioALSAStreamOut::setVolume(float left, float right)
{
    ALOGV("setVolume:left %f right %f",left,right);

    m_Buffer.volume[0] = INT_Q15(left);
    m_Buffer.volume[1] = INT_Q15(right);

    switch (m_Buffer.channels) {
        case 8:
            m_Buffer.volume[7] = m_Buffer.volume[1];
        case 7:
            m_Buffer.volume[6] = m_Buffer.volume[0];
        case 6:
            m_Buffer.volume[5] = (m_Buffer.volume[1] + m_Buffer.volume[0]) >> 1;
        case 5:
            m_Buffer.volume[4] = (m_Buffer.volume[1] + m_Buffer.volume[0]) >> 1;
        case 4:
            m_Buffer.volume[3] = m_Buffer.volume[1];
        case 3:
            m_Buffer.volume[2] = m_Buffer.volume[0];
        default:
            break;
    }

    for (int i = m_Buffer.channels; i < TEGRA_MAX_CHANNELS; i++)
        m_Buffer.volume[i] = MAX_Q15;

    return 0;
}

ssize_t NvAudioALSAStreamOut::write(const void *buffer, size_t bytes)
{
    ssize_t sent = 0;

    AutoMutex lock(m_Lock);

    if (!m_Started) {
        m_Renderer->start(this);
        m_Started = true;

#ifdef TEGRA_TEST_LIBAUDIO
        {
            char prop_val[PROPERTY_VALUE_MAX];
            property_get(TEGRA_TEST_LIBAUDIO_PROPERTY, prop_val, "");
            if (!strcmp(prop_val, "1"))
                m_Test = true;
            else
                m_Test = false;

            if (m_Test) {
                crcInit(&m_Crc);
                m_CrcFinal = m_Crc;
            }
        }
#endif

        if (m_Buffer.channels <= 2)
            m_ChannelMap = TegraDefaultChannelMap[m_Buffer.channels - 1];
        else {
            char prop_val[PROPERTY_VALUE_MAX] = "";

            m_ChannelMap = 0x0;
            property_get(TEGRA_CHANNEL_MAP_PROPERTY, prop_val, "");

            if (!strcmp(prop_val, "")) {
                m_ChannelMap = TegraDefaultChannelMap[m_Buffer.channels - 1];
            }
            else {
                char chanId[PROPERTY_VALUE_MAX] = "";
                uint32_t propStrPos = 0, chMapPos = 0, i;
                uint32_t propValLen = strlen(prop_val);

                do {
                    sscanf(&prop_val[propStrPos], "%s", chanId);
                    for (i = LF; i < TEGRA_AUDIO_CHANNEL_ID_LAST; i++) {
                        if (!strcmp(chanId, TegraAudioChannelIdStr[i])) {
                            m_ChannelMap |= (i << chMapPos);
                            chMapPos += 4;
                            propStrPos += (strlen(chanId) + 1);
                            break;
                        }
                    }

                    if (i == TEGRA_AUDIO_CHANNEL_ID_LAST) {
                        ALOGE("Invalid channel map prop str %s\n", prop_val);
                        break;
                    }
                } while(propStrPos < propValLen);
            }
            ALOGV("write: %d chanenls with 0x%08x channel map (%s)\n",
                            m_Buffer.channels, m_ChannelMap, prop_val);

            memset(m_Buffer.stereoDownmixTab, 0,
                sizeof(m_Buffer.stereoDownmixTab));
            // populate downmix table with normalized weights
            for (uint32_t i = 0; i < 2; i++) {
                uint32_t sum = 0;
                for (uint32_t j = 0; j < m_Buffer.channels; j++) {
                    int index = ((m_ChannelMap >> (j << 2)) & 0xF) - 1;
                    if (index >= 0)
                        sum += StereodownmixTab[index][i];
                }
                for (uint32_t j = 0; j < m_Buffer.channels; j++) {
                    int index = ((m_ChannelMap >> (j << 2)) & 0xF) - 1;
                    if (index >= 0)
                        m_Buffer.stereoDownmixTab[index][i] =
                            (StereodownmixTab[index][i] << SHIFT_Q15)/ sum;
                }
            }
        }
    }

#ifdef TEGRA_TEST_LIBAUDIO
    if (m_Test) {

        unsigned char *pbuff = (unsigned char*) buffer;
        unsigned int i;

        for (i=0; i < bytes; i++)
            crcCompute(pbuff[i], &m_Crc);

        for (i=0; i < bytes; i++) {
            if (pbuff[i]) {
                m_CrcFinal = m_Crc;
                break;
            }
        }
    }
#endif

    m_Buffer.pData = (void*)buffer;
    m_Buffer.dataSize = m_Buffer.size = bytes;
    m_Buffer.channelMap = m_ChannelMap;

    sent = m_Renderer->write(this, &m_Buffer);
    m_FrameCount += sent;
    m_Buffer.dataSize = 0;

    return sent;
}

status_t NvAudioALSAStreamOut::standby()
{
    ALOGV("standby: IN");

    AutoMutex lock(m_Lock);

    if (m_Started) {
        m_Renderer->stop(this);
        m_FrameCount = 0;
        m_Started = false;
#ifdef TEGRA_TEST_LIBAUDIO
        if (m_Test) {
            char prop_val[PROPERTY_VALUE_MAX] = "";
            crcComplete(&m_CrcFinal);
            sprintf(prop_val, "%08X", m_CrcFinal);
            property_set(TEGRA_TEST_LIBAUDIO_CRC_PROPERTY, prop_val);
        }
#endif
    }

    return NO_ERROR;
}

status_t NvAudioALSAStreamOut::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

#define USEC_TO_MSEC(x) ((x + 999) / 1000)
uint32_t NvAudioALSAStreamOut::latency() const
{
    uint32_t latency = 0;
    // Android wants latency in milliseconds.
    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if ((m_Devices == m_Renderer->m_Handle[i]->curDev) ||
            (i == OUT_PCM_INDEX)) {
            latency = USEC_TO_MSEC (m_Renderer->m_Handle[i]->latency);
            break;
        }
    }

    ALOGV("latency %d ms",latency);
    return latency;
}

// return the number of audio frames written by the audio dsp to DAC since
// the output has exited standby
status_t NvAudioALSAStreamOut::getRenderPosition(uint32_t *dspFrames)
{
    ALOGV("getRenderPosition %d", m_FrameCount);
    *dspFrames = m_FrameCount;
    return NO_ERROR;
}

NvAudioALSAMixer *NvAudioALSAStreamOut::mixer()
{
    return m_Parent->m_Mixer;
}

NvAudioALSADev *NvAudioALSAStreamOut::getALSADev()
{
    return m_Renderer->m_Handle[OUT_PCM_INDEX];
}

status_t NvAudioALSAStreamOut::open(int      *format,
                                    uint32_t *channels,
                                    uint32_t *rate,
                                    uint32_t devices)
{
    status_t err = NO_ERROR;
    ALOGV("open: format 0x%x, channels 0x%x, rate %d, devices 0x%x",
            *format, *channels, *rate, devices);

    AutoMutex lock(m_Lock);

    m_Renderer->open(format, channels, rate, devices);
    m_Channels = *channels;
    m_SampleRate = *rate;
    m_Format = *format;
    m_Devices = devices;

    memset(&m_Buffer, 0, sizeof(NvAudioALSABuffer));
    m_Buffer.channels = popCount(m_Channels);
    m_Buffer.sampleSize = (m_Format == AudioSystem::PCM_SUB_8_BIT) ?
        m_Buffer.channels : (m_Buffer.channels << 1);

    for (int i = 0; i < TEGRA_MAX_CHANNELS; i++)
        m_Buffer.volume[i] = MAX_Q15;

    m_Buffer.devices = m_Devices;

    return NO_ERROR;
}

status_t NvAudioALSAStreamOut::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    status_t status = NO_ERROR;
    int keyVal;
    ALOGV("setParameters() %s", keyValuePairs.string());

    AutoMutex lock(m_Lock);

    if (param.getInt(key, keyVal) == NO_ERROR) {
        ALOGV("Switch audio to device :0x%x\n", keyVal);

        keyVal &= AudioSystem::DEVICE_OUT_ALL;
        if (keyVal) {
            m_Renderer->setParameters(this, keyValuePairs);
            m_Buffer.devices = m_Devices = keyVal;
        }
        param.remove(key);
    }

    key = String8("output_available");
    if (param.getInt(key, keyVal) == NO_ERROR) {
        ALOGV("Available output device :0x%x\n", keyVal);

        m_Renderer->setParameters(this, keyValuePairs);
        param.remove(key);
    }

    key = String8(AudioParameter::keySamplingRate);
    if (param.getInt(key, keyVal) == NO_ERROR) {
        ALOGV("Switch audio samplerate to :%d\n", keyVal);

        status = m_Renderer->setParameters(this, keyValuePairs);
        if (status == NO_ERROR)
            m_SampleRate = keyVal;
        param.remove(key);
    }

    if (param.size()) {
        status = BAD_VALUE;
    }

    return status;
}

String8 NvAudioALSAStreamOut::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, m_Devices);
    }

    key = String8(AudioParameter::keySamplingRate);
    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)m_SampleRate);
    }

    ALOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

uint32_t NvAudioALSAStreamOut::sampleRate() const
{
    ALOGV("sampleRate %d",m_SampleRate);
    return m_SampleRate;
}

//
// Return the number of bytes (not frames)
//
size_t NvAudioALSAStreamOut::bufferSize() const
{
    size_t bytes = 0;
    size_t frameSize = (popCount(m_Channels) *
            ((m_Format == AudioSystem::PCM_8_BIT) ? (8 >> 3) : (16 >> 3)));

    for (int i = 0; i < MAX_PCM_INDEX; i++) {
        if ((m_Devices == m_Renderer->m_Handle[i]->curDev) ||
            (i == OUT_PCM_INDEX)) {
            bytes = (m_Renderer->m_Handle[i]->bufferSize/ 4) * frameSize;
            break;
        }
    }

    bytes += (frameSize - 1);
    bytes -= (bytes % frameSize);

    ALOGV("bufferSize %d",bytes);
    return bytes;
}

int NvAudioALSAStreamOut::format() const
{
    ALOGV("format %x",m_Format);
    return m_Format;
}

uint32_t NvAudioALSAStreamOut::getDevice()
{
    return m_Devices;
}

bool NvAudioALSAStreamOut::isActive()
{
    return m_Started;
}

#ifdef TEGRA_TEST_LIBAUDIO
static unsigned int crc_32_tab[] =  // CRC polynomial 0xedb88320
{
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

void crcInit(unsigned int* crc)
{
    *crc = 0xFFFFFFFF;
}

void crcCompute(unsigned char octet, unsigned int* crc)
{
      *crc = (crc_32_tab[((*crc)^((unsigned char)octet)) & 0xff] ^ ((*crc) >> 8));
}

void crcComplete(unsigned int* crc)
{
    *crc = ~(*crc);
}
#endif

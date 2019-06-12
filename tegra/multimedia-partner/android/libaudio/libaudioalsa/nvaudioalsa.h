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
 ** Based upon AudioHardwareALSA.h, provided under the following terms:
 **
 ** Copyright 2008-2009, Wind River Systems
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

#ifndef LIBAUDIO_NVAUDIOALSA_H
#define LIBAUDIO_NVAUDIOALSA_H

//#define LOG_NDEBUG 0

#include <errno.h>
#include <utils/List.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/AudioHardwareBase.h>
#include <alsa/asoundlib.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include "invaudioalsaservice.h"
#include "nvcap_audio.h"

#define AudioHardwareBase android_audio_legacy::AudioHardwareBase
#define AudioSystem android_audio_legacy::AudioSystem
#define AudioStreamIn android_audio_legacy::AudioStreamIn
#define AudioStreamOut android_audio_legacy::AudioStreamOut

#define NVDEV_NAME "libnvaudio: "
#define TEGRA_ALSA_HARDWARE_MODULE_ID "tegra_alsa"
#define TEGRA_ALSA_HARDWRAE_NAME "tegra_alsa"

#define TEGRA_MAX_OUT_CHANNELS_PROPERTY "media.tegra.max.out.channels"
#define TEGRA_CHANNEL_MAP_PROPERTY "media.tegra.out.channel.map"
#define TEGRA_TEST_LIBAUDIO_PROPERTY "media.tegra.libaudio.test"
#define TEGRA_TEST_LIBAUDIO_CRC_PROPERTY "media.tegra.libaudio.crc"
#define TEGRA_HW_AC3DEC_PROPERTY "media.tegra.hw.ac3dec"
#define TEGRA_HW_DTSDEC_PROPERTY "media.tegra.hw.dtsdec"

#define TEGRA_DEFAULT_OUT_FORMAT        SND_PCM_FORMAT_S16_LE
#define TEGRA_DEFAULT_OUT_SAMPLE_RATE   44100           //in Hz
#define TEGRA_DEFAULT_OUT_CHANNELS      2
#define TEGRA_DEFAULT_OUT_MAX_LATENCY   100000          //in us
#define TEGRA_DEFAULT_OUT_PERIODS       4

#define TEGRA_DEFAULT_IN_FORMAT         SND_PCM_FORMAT_S16_LE
#define TEGRA_DEFAULT_IN_SAMPLE_RATE    AudioRecord::DEFAULT_SAMPLE_RATE
#define TEGRA_DEFAULT_IN_CHANNELS       2
#define TEGRA_DEFAULT_IN_MAX_LATENCY    100000          //in us
#define TEGRA_DEFAULT_IN_PERIODS        4

#define TEGRA_VOICE_CALL_FORMAT         SND_PCM_FORMAT_S16_LE
#define TEGRA_VOICE_CALL_SAMPLE_RATE    8000
#define TEGRA_VOICE_CALL_CHANNELS       1
#define TEGRA_VOICE_CALL_BUFFER_SIZE    2048            //in frames
#define TEGRA_VOICE_CALL_PERIODS        4

#define TEGRA_ALSA_DEVICE_NAME_MAX_LEN  100

#define TEGRA_MAX_CHANNELS              8
// frames per buffer * bytes per sample * max channels
#define TEGRA_MAX_MIX_BUFFER_SIZE       (8192 * sizeof(uint16_t) * \
                                        TEGRA_MAX_CHANNELS)

#define SHIFT_Q15                       15
#define MAX_Q15                         (1 << SHIFT_Q15)
#define INT_Q15(f)                      (int32_t)((float)f * MAX_Q15)
#define MAX(a, b)                       ((a) > (b)) ? (a) : (b)

#define REC_DEC_CAP_AC3   (1<<2)
#define REC_DEC_CAP_DTS   (1<<7)

enum {
#ifdef FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT
    WFD_PCM_INDEX,
#endif
    AUX_PCM_INDEX,
    OUT_PCM_INDEX, /* This should always be the last member of this enum */
    MAX_PCM_INDEX,
};

enum TEGRA_AUDIO_CHANNEL_ID {
    LF = 1,                           // Left Front
    RF,                               // Right Front
    CF,                               // Center Front
    LS,                               // Left Surround
    RS,                               // Right Surround
    LFE,                              // Low Frequency Effect
    CS,                               // Center Surround
    LR,                               // Rear Left
    RR,                               // Rear Right
    TEGRA_AUDIO_CHANNEL_ID_LAST
};

static const char* TegraAudioChannelIdStr[TEGRA_AUDIO_CHANNEL_ID_LAST] =
{"", "LF", "RF", "CF", "LS", "RS", "LFE", "CS", "LR", "RR"};

// HDA channel order
static const uint32_t TegraDstChannelMap[TEGRA_MAX_CHANNELS] = {
    (LF << 0),
    (LF << 0) | (RF << 4),
    (LF << 0) | (RF << 4) | (CF << 8),
    (LF << 0) | (RF << 4) | (LS << 8) | (RS << 12),
    (LF << 0) | (RF << 4) | (LS << 8) | (RS << 12) | (CF << 16),
    (LF << 0) | (RF << 4) | (LS << 8) | (RS << 12) | (CF << 16) | (LFE << 20),
    (LF << 0) | (RF << 4) | (LR << 8) | (RR << 12) | (CF << 16) | (LS << 20) |\
    (RS << 24),
    (LF << 0) | (RF << 4) | (LR << 8) | (RR << 12) | (CF << 16) | (LFE << 20) |\
    (LS << 24) | (RS << 28)
};

static const uint32_t TegraDefaultChannelMap[TEGRA_MAX_CHANNELS] = {
    (LF << 0),
    (LF << 0) | (RF << 4),
    (LF << 0) | (RF << 4) | (CF << 8),
    (LF << 0) | (RF << 4) | (LS << 8) | (RS << 12),
    (LF << 0) | (RF << 4) | (CF << 8) | (LS << 12) | (RS << 16),
    (LF << 0) | (RF << 4) | (CF << 8) | (LFE << 12) | (LS << 16) | (RS << 20),
    (LF << 0) | (RF << 4) | (CF << 8) | (LS << 12) | (RS << 16) | (LR << 20) |\
    (RR << 24),
    (LF << 0) | (RF << 4) | (CF << 8) | (LFE << 12) | (LS << 16) | (RS << 20) |\
    (LR << 24) | (RR << 28)
};

// LF RF CF LS RS LFE CS LR RR
static const int32_t StereodownmixTab[TEGRA_AUDIO_CHANNEL_ID_LAST][2] = {
{INT_Q15(1.0),          INT_Q15(0.0)},
{INT_Q15(0.0),          INT_Q15(1.0)},
{INT_Q15(0.70703125),   INT_Q15(0.70703125)},
{INT_Q15(0.70703125),   INT_Q15(0.0)},
{INT_Q15(0.0),          INT_Q15(0.70703125)},
{INT_Q15(0.0),          INT_Q15(0.0)},
{INT_Q15(0.5),          INT_Q15(0.5)},
{INT_Q15(0.5),          INT_Q15(0.0)},
{INT_Q15(0.0),          INT_Q15(0.5)}};

using namespace android;

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | \
                            AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | \
                            AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

struct tegra_alsa_device_t;
class NvAudioALSAControl;

typedef struct
{
    tegra_alsa_device_t *   device;
    snd_pcm_stream_t        streamType;
    char                    alsaDevName[TEGRA_ALSA_DEVICE_NAME_MAX_LEN];
    bool                    isVoiceCallDevice;
    void *                  hpcm;
    uint32_t                validDev;
    uint32_t                curDev;
    int                     curMode;
    snd_pcm_format_t        format;
    uint32_t                channels;
    uint32_t                max_out_channels;
    uint32_t                sampleRate;
    unsigned int            latency;         // Delay in usec
    unsigned int            bufferSize;      // Size of sample buffer
    unsigned int            periods;
    void *                  modPrivate;
    float                   earpieceVol;
    float                   speakerVol;
    float                   headsetVol;
    int                     volume;
    uint32_t                activeCount;
    bool                    isSilent;
    char *                  cardname;
    bool                    isCompressed;
    int                     recDecCapable;
} NvAudioALSADev;

typedef struct
{
    void* pData;
    uint32_t size;              // bytes
    uint32_t dataSize;          // bytes

    uint32_t channels;
    uint32_t channelMap;
    uint32_t sampleSize;
    uint32_t devices;

    uint32_t volume[TEGRA_MAX_CHANNELS];        // Q15
    int32_t stereoDownmixTab[TEGRA_MAX_CHANNELS][2];

} NvAudioALSABuffer;


struct tegra_alsa_device_t {
    hw_device_t             common;
    NvAudioALSAControl*     alsaControl;
    NvAudioALSAControl*     alsaHDAControl;
    sp<IMemoryHeap>         memHeap;
    void*                   sharedMem;
    Mutex                   m_Lock;
    uint32_t                default_sampleRate;
    bool                    isPlaybackActive;
    bool                    isMicMute;
    char                    lp_state_default[4];

    status_t (*open)(NvAudioALSADev *, uint32_t, int);
    void (*close)(NvAudioALSADev *);
    status_t (*route)(NvAudioALSADev *, uint32_t, int);
    ssize_t  (*write)(NvAudioALSADev *, void *, size_t);
    status_t (*set_volume)(NvAudioALSADev *, float left, float right);
    status_t (*set_voice_volume)(NvAudioALSADev *, float volume);
    status_t (*set_gain)(NvAudioALSADev *, float gain);
    status_t (*set_mute)(NvAudioALSADev *, uint32_t, bool);

};


class NvAudioALSADevice;
class NvAudioALSAStreamOut;
class NvAudioALSAStreamIn;
class NvAudioALSARenderer;
class NvAudioALSAMixer;

typedef List<NvAudioALSAStreamIn *> NvAudioALSAStreamInList;
typedef List<NvAudioALSAStreamOut *> NvAudioALSAStreamOutList;
typedef List<NvAudioALSABuffer *> NvAudioALSABufferList;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// use emulated popcount optimization
// http://www.df.lth.se/~john_e/gems/gem002d.html
static inline uint32_t popCount(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

//*****************************************************************************
// NvAudioDevice
//*****************************************************************************
class NvAudioALSADevice : public AudioHardwareBase
{

///*************************************************************************
//// NvAudioDevice
////*************************************************************************
    private:
    Mutex                       m_Lock;

    protected:
    tegra_alsa_device_t *       m_ALSADev;
    tegra_alsa_device_t *       m_WFDDev;
    NvAudioALSARenderer*        m_Renderer;
    NvAudioALSAStreamOutList    m_StreamOutList;
    NvAudioALSAStreamInList     m_StreamInList;
    NvAudioALSADev *            m_VoiceCallAlsaDev[SND_PCM_STREAM_LAST + 1];

    NvAudioALSAMixer *          m_Mixer;
    NvAudioALSAControl *        m_Control;
    NvAudioALSAControl *        m_HDAControl;

    friend class NvAudioALSAStreamOut;
    friend class NvAudioALSAStreamIn;
    friend class NvAudioALSARenderer;

    public:
    NvAudioALSADevice(void);
    virtual ~NvAudioALSADevice(void);

    //*************************************************************************
    // AudioHardwareInterface
    //*************************************************************************

    protected:
    virtual status_t dump(int fd, const Vector<String16>& args);


    public:
    virtual status_t initCheck(void);
    virtual status_t setMasterVolume(float volume);
    virtual status_t setVoiceVolume(float volume);
    virtual status_t setMode(int mode);

    virtual status_t getMicMute(bool* state);
    virtual status_t setMicMute(bool state);

    virtual status_t setParameters(const String8& keyValuePairs);
    virtual String8  getParameters(const String8& keys);
    virtual size_t   getInputBufferSize(uint32_t sampleRate, int format, int channelCount);

    virtual AudioStreamOut* openOutputStream(uint32_t devices, int* format = 0,
                                uint32_t* channels = 0, uint32_t* sampleRate = 0,
                                status_t* status = 0);
    virtual void closeOutputStream(AudioStreamOut* out);
    virtual AudioStreamIn* openInputStream(uint32_t devices, int* format, uint32_t* channels,
                                                uint32_t* sampleRate, status_t* status,
                                                AudioSystem::audio_in_acoustics acoustics);
    virtual void closeInputStream(AudioStreamIn* in);

    virtual status_t dumpState(int fd, const Vector<String16>& args);

    NvAudioALSARenderer* getRenderer();
    int mode()
    {
        return mMode;
    }
    //*************************************************************************
    // AudioHardwareBase
    //*************************************************************************

};

class NvAudioALSARenderer
{
    public:
    NvAudioALSARenderer(NvAudioALSADevice *);
    virtual ~NvAudioALSARenderer(void);

    status_t initCheck();
    NvAudioALSADev* getALSADev(uint32_t index);

    protected:
    status_t    open(int *format, uint32_t *channels,
                    uint32_t *rate, uint32_t devices);
    void        start(NvAudioALSAStreamOut *pStream);
    ssize_t     write(NvAudioALSAStreamOut *pStream, NvAudioALSABuffer *pBuf);
    void        stop(NvAudioALSAStreamOut *pStream);
    status_t    setParameters(NvAudioALSAStreamOut *pStream,
                                const String8& keyValuePairs);

    NvAudioALSADev *        m_Handle[MAX_PCM_INDEX];

    friend class NvAudioALSAStreamOut;

    private:
    static int              MixThread(void *p);
    int                     MixThreadLoop(void);
    void                    applyVolume(NvAudioALSABuffer *pBuf);
    status_t                shuffleChannels(NvAudioALSABuffer *pBuf,
                                                    uint32_t dstChMap);
    status_t                upmix(NvAudioALSABuffer *pSrcBuf,
                                        NvAudioALSABuffer *pDstBuf);
    status_t                downmix(NvAudioALSABuffer *pSrcBuf,
                                        NvAudioALSABuffer *pDstBuf);
    status_t                mix(NvAudioALSABuffer *pSrcBuf,
                                NvAudioALSABuffer *pDstBuf);
    int                     getAudioSystemFormat(snd_pcm_format_t format);
    uint32_t                getAudioSystemChannels(uint32_t channels);
    NvAudioALSABuffer*      getTempBuffer(void);
    void                    releaseTempBuffer(NvAudioALSABuffer* pBuf);

    NvAudioALSADevice*      m_Parent;
    NvAudioALSABufferList   m_QueuedList;
    NvAudioALSABufferList   m_FreeTmpBufferList;

    Mutex                   m_Lock;
    Condition               m_BufferReady;
    Condition               m_WriteDone;

    uint32_t                m_AvailableDevices;
    uint64_t                m_Timeoutns;
    uint32_t                m_MixStreamCount;
    bool                    m_threadExit;
    bool                    mInit;
};

//*****************************************************************************
// NvAudioALSAStreamOut
//*****************************************************************************
class NvAudioALSAStreamOut : public AudioStreamOut
{
    //*************************************************************************
    // NvAudioStreamOut
    //*************************************************************************

    public:
    NvAudioALSAStreamOut(NvAudioALSADevice *);
    virtual ~NvAudioALSAStreamOut(void);

    status_t                open(int *format, uint32_t *channels,
                                uint32_t *rate, uint32_t devices);
    NvAudioALSADev*         getALSADev();
    uint32_t                getDevice();
    bool                    isActive();

    private:
    NvAudioALSAMixer        *mixer();

    NvAudioALSADevice *     m_Parent;
    NvAudioALSARenderer*    m_Renderer;
    NvAudioALSABuffer       m_Buffer;

    Mutex                   m_Lock;
    bool                    m_Started;
    uint32_t                m_FrameCount;
    uint32_t                m_Devices;
    uint32_t                m_Channels;
    uint32_t                m_ChannelMap;
    int                     m_Format;
    uint32_t                m_SampleRate;
#ifdef TEGRA_TEST_LIBAUDIO
    bool                  m_Test;
    unsigned int          m_Crc;
    unsigned int          m_CrcFinal;
#endif

    //*************************************************************************
    // AudioStreamOut
    //*************************************************************************

    public:
    virtual uint32_t sampleRate(void) const;
    virtual size_t bufferSize(void) const;
    virtual uint32_t channels(void) const;
    virtual int format(void) const;
    virtual uint32_t latency(void) const;
    virtual status_t setVolume(float, float);
    virtual ssize_t write(const void*, size_t);
    virtual status_t standby(void);
    virtual status_t dump(int, const Vector<String16>&);
    virtual status_t setParameters(const String8&);
    virtual String8 getParameters(const String8&);
    virtual status_t getRenderPosition(uint32_t *dspFrames);
};

//*****************************************************************************
// NvAudioALSAStreamIn
//*****************************************************************************
class NvAudioALSAStreamIn : public AudioStreamIn
{
    //*************************************************************************
    // NvAudioStreamIn
    //*************************************************************************
    public:
    NvAudioALSAStreamIn(NvAudioALSADevice *);
    virtual ~NvAudioALSAStreamIn(void);

    status_t open(int *format, uint32_t *channels, uint32_t *rate, uint32_t devices);
    NvAudioALSADev* getALSADev();

    private:
    void                    resetFramesLost();
    NvAudioALSAMixer        *mixer();

    NvAudioALSADevice *     m_Parent;
    NvAudioALSADev *        m_Handle;

    unsigned int            m_FramesLost;
    Mutex                   m_Lock;

    //*************************************************************************
    // AudioStreamIn
    //*************************************************************************

    public:
    virtual uint32_t sampleRate(void) const;
    virtual size_t bufferSize(void) const;
    virtual uint32_t channels(void) const;
    virtual int format(void) const;
    virtual status_t setGain(float);
    virtual ssize_t read(void*, ssize_t);
    virtual status_t standby();
    virtual status_t dump(int fd, const Vector<String16>& args);
    virtual status_t setParameters(const String8&);
    virtual String8 getParameters(const String8&);
    virtual unsigned int  getInputFramesLost() const;
    virtual status_t addAudioEffect(effect_handle_t effect);
    virtual status_t removeAudioEffect(effect_handle_t effect);
};

class NvAudioALSAMixer
{
public:
    NvAudioALSAMixer();
    virtual                ~NvAudioALSAMixer();

    bool                    isValid() { return !!m_Mixer[SND_PCM_STREAM_PLAYBACK]; }
    status_t                setMasterVolume(float volume);
    status_t                setMasterGain(float gain);

    status_t                setVolume(uint32_t device, float left, float right);
    status_t                setGain(uint32_t device, float gain);

    status_t                setCaptureMuteState(uint32_t device, bool state);
    status_t                getCaptureMuteState(uint32_t device, bool *state);
    status_t                setPlaybackMuteState(uint32_t device, bool state);
    status_t                getPlaybackMuteState(uint32_t device, bool *state);
    status_t                initCheck();

private:
    snd_mixer_t *           m_Mixer[SND_PCM_STREAM_LAST+1];
    bool                    mInit;
};

class NvAudioALSAControl
{
public:
    NvAudioALSAControl(const char *device = "default");
    virtual                ~NvAudioALSAControl();

    status_t                get(const char *name, int &value, int index = 0);
    status_t                get(const char *name, int &value, int &val_min,
                                int &val_max, int index = 0);
    status_t                set(const char *name, unsigned int value, int index, int items);
    status_t                set(const char *name, unsigned int value, int index = -1);

    status_t                set(const char *name, const char *);
    status_t                initCheck();

private:
    snd_ctl_t *             mHandle;
    bool                    mInit;
};

class NvAudioALSAService: public BnNvAudioALSAService
{
public:
    NvAudioALSAService(NvAudioALSADevice* device);
    ~NvAudioALSAService();

    static void instantiate(NvAudioALSADevice* device);

    virtual void setWFDAudioBuffer(const sp<IMemory>& mem);
    virtual status_t setWFDAudioParams(uint32_t sampleRate, uint32_t channels,
                                    uint32_t bitsPerSample);

private:
    NvAudioALSADevice* m_Device;
};

#endif //LIBAUDIO_NVAUDIOALSA_H

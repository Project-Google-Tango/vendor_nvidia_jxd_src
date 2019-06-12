/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "nvaudio_service"
//#define LOG_NDEBUG 0

#include "nvaudio.h"
#include "nvaudio_service.h"
#include "invcapaudioservice.h"

using namespace android;

struct audio_svc_param
{
    audio_hw_device_t *hw_dev;

    void* sharedMem;
    uint32_t channels;
    uint32_t sampleRate;
    uint32_t bitsPerSample;
    sp<IMemoryHeap> memHeap;
    sp<NvAudioALSAClientWFD> clientWFD;
    sp<INvCapAudioService> audioNvCapSrvc;
    sp<INvAudioALSAClient>  client;
};

/******************************************************************************
 * WFD audio related APIs
 ******************************************************************************/
extern "C" void instantiate_nvaudio_service(audio_hw_device_t *dev);
extern "C" void* get_wfd_audio_buffer();
extern "C" int get_wfd_audio_params(uint32_t* sampleRate, uint32_t* channels,
                                    uint32_t* bitsPerSample);

extern "C" audio_stream_t *get_audio_stream(audio_hw_device_t *dev,
                                            audio_io_handle_t ioHandle);

static NvAudioALSAService* g_ALSAServiceObj;
struct audio_svc_param g_audio_svc_param;

NvAudioALSAService::NvAudioALSAService()
{
    ALOGV("NvAudioALSAService");

    g_audio_svc_param.sampleRate = 0;
    g_audio_svc_param.channels = 0;
    g_audio_svc_param.bitsPerSample = 0;
    g_audio_svc_param.sharedMem = 0;
    g_audio_svc_param.clientWFD = new NvAudioALSAClientWFD();
    g_audio_svc_param.client =
        new NvAudioALSAClient(NV_AUDIO_ALSA_CLIENT_TYPE_WFD);
}

NvAudioALSAService::~NvAudioALSAService()
{
    ALOGV("~NvAudioALSAService");
}

void NvAudioALSAService::setWFDAudioBuffer(const sp<IMemory>& mem)
{
    ALOGV("setWFDAudioBuffer");

    ssize_t offset = 0;
    size_t size = 0;

    g_audio_svc_param.memHeap = mem->getMemory(&offset, &size);
    g_audio_svc_param.sharedMem = (uint8_t*)g_audio_svc_param.memHeap->getBase()
                                                                   +  offset;
}

void NvAudioALSAService::registerClient(const sp<INvAudioALSAClient>& client)
{
    ALOGV("NvAudioALSAService::registerClient");
    if (client == 0)
    {
        ALOGE("NvAudioALSAService::registerClient: invalid client");
        return;
    }
    switch (client->getClientType())
    {
        case NV_AUDIO_ALSA_CLIENT_TYPE_WFD:
        {
            client->asBinder()->linkToDeath(g_audio_svc_param.clientWFD);
            break;
        }
        default:
            break;
    }
}

void NvAudioALSAService::unregisterClient(const sp<INvAudioALSAClient>& client)
{
    ALOGV("NvAudioALSAService::unregisterClient");
    if (client == 0)
    {
        ALOGE("NvAudioALSAService::unregisterClient: invalid client");
        return;
    }
    switch (client->getClientType())
    {
        case NV_AUDIO_ALSA_CLIENT_TYPE_WFD:
        {
            client->asBinder()->unlinkToDeath(g_audio_svc_param.clientWFD);
            break;
        }
        default:
            break;
    }
}

void NvAudioALSAClientWFD::binderDied(const wp<IBinder>& who)
{
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16("media.audio_policy"));
    if (binder != 0) {
        ALOGE("NvAudioALSAClientWFD::binderDied: "
                "setting WFD audio as unavailable");
        sp<IAudioPolicyService> audioPolicySrvc =
                interface_cast<IAudioPolicyService>(binder);
        audioPolicySrvc->setDeviceConnectionState(
                AUDIO_DEVICE_OUT_REMOTE_SUBMIX,
                AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
                "");
    } else {
        ALOGE("NvAudioALSAClientWFD::binderDied: "
                "Failed to get media.audio_policy");
    }
}

status_t NvAudioALSAService::setWFDAudioParams(uint32_t sampleRate,
                                               uint32_t channels,
                                               uint32_t bitsPerSample)
{
    ALOGV("setWFDAudioParams rate %d chan %d bps %d",
         sampleRate, channels, bitsPerSample);

    if (g_audio_svc_param.sampleRate !=0 || g_audio_svc_param.channels != 0
         || g_audio_svc_param.bitsPerSample != 0) {
        if ((g_audio_svc_param.sampleRate != sampleRate) ||
             (g_audio_svc_param.channels != channels) ||
            (g_audio_svc_param.bitsPerSample != bitsPerSample)) {
            ALOGE("setAudioParams: Requested params don't match with already"
                 "open WFD device params");
            return -EINVAL;
        }
    }
    else {
        g_audio_svc_param.sampleRate = sampleRate;
        g_audio_svc_param.channels = channels;
        g_audio_svc_param.bitsPerSample = bitsPerSample;
    }

    return NO_ERROR;
}

status_t NvAudioALSAService::setAudioParameters(audio_io_handle_t ioHandle,
                                                const String8& keyValuePairs)
{
    struct nvaudio_hw_device *hw_dev =
            (struct nvaudio_hw_device*)g_audio_svc_param.hw_dev;
    node_handle n;

    ALOGV("setAudioParameters ioHandle %d key %s",
         ioHandle, keyValuePairs.string());

    if (ioHandle == 0)
        return hw_dev->hw_device.set_parameters(&hw_dev->hw_device,
                                                keyValuePairs);
    else {
        struct audio_stream *stream;

        pthread_mutex_lock(&hw_dev->lock);
        stream = get_audio_stream(&hw_dev->hw_device, ioHandle);
        pthread_mutex_unlock(&hw_dev->lock);

        if (stream)
            return stream->set_parameters(stream, keyValuePairs);
    }

    return -EINVAL;
}

String8 NvAudioALSAService::getAudioParameters(audio_io_handle_t ioHandle,
                                                const String8& keys)
{
    struct nvaudio_hw_device *hw_dev =
            (struct nvaudio_hw_device*)g_audio_svc_param.hw_dev;
    status_t ret = NO_ERROR;
    node_handle n;
    char *tmp = NULL;
    String8 tmp_s8 = String8();

    ALOGV("getAudioParameters ioHandle %d key %s",
         ioHandle, keys.string());

    if (ioHandle == 0)
        tmp = hw_dev->hw_device.get_parameters(&hw_dev->hw_device, keys);
    else {
        struct audio_stream *stream;

        pthread_mutex_lock(&hw_dev->lock);
        stream = get_audio_stream(&hw_dev->hw_device, ioHandle);
        pthread_mutex_unlock(&hw_dev->lock);

        if (stream)
            tmp = stream->get_parameters(stream, keys);
    }

    if (tmp) {
        tmp_s8 = String8(tmp);
        free(tmp);
    }

    return tmp_s8;
}

void* get_wfd_audio_buffer()
{
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = NULL;

    ALOGV("get_wfd_audio_buffer");
    binder = sm->getService(String16(NV_AUDIO_WFD_SRVC_NAME));
    if (binder <= 0) {
        ALOGE("get_wfd_audio_buffer: Failed to get %s", NV_AUDIO_WFD_SRVC_NAME);
    } else {
        g_audio_svc_param.audioNvCapSrvc =
            INvCapAudioService::asInterface(binder);
        g_audio_svc_param.audioNvCapSrvc->registerClient(
            g_audio_svc_param.client);
    }

    return (g_audio_svc_param.sharedMem);
}

int get_wfd_audio_params(uint32_t* sampleRate, uint32_t* channels,
                                    uint32_t* bitsPerSample)
{
    ALOGV("get_wfd_audio_params");

    if (g_audio_svc_param.sampleRate ==0 || g_audio_svc_param.channels == 0
        || g_audio_svc_param.bitsPerSample == 0)
        return -EINVAL;

    *sampleRate = g_audio_svc_param.sampleRate;
    *channels = g_audio_svc_param.channels;
    *bitsPerSample = g_audio_svc_param.bitsPerSample;

    return NO_ERROR;
}

void instantiate_nvaudio_service(audio_hw_device_t *dev)
{
    ALOGV("instantiate_nvaudio_service");

    g_ALSAServiceObj = new NvAudioALSAService();
    g_audio_svc_param.hw_dev = dev;

    sp<IBinder> binder =
        defaultServiceManager()->getService(String16(NV_AUDIO_ALSA_SRVC_NAME));
    if (binder == 0) {
        defaultServiceManager()->addService(String16(NV_AUDIO_ALSA_SRVC_NAME),
                                            g_ALSAServiceObj);
        ProcessState::self()->startThreadPool();
    }
}

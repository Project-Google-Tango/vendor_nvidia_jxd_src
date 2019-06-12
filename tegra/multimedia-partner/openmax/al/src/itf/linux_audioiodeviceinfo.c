/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "linux_audioiodeviceinfo.h"
#include "linux_outputmix.h"

static AudioDevicesInputOutputDescriptor s_AudioIODevicesDescriptor[MAX_AUDIO_IO_DEVICES];
#ifdef PULSE_AUDIO_ENABLE
static PulseAudioContextData s_PulseAudioContext;
#endif
static OutputMixRegisterCallbackData s_OutputMixRegisterCallback;

enum
{
    DEVICE_ADDED = 0x1,
    DEVICE_REMOVED = 0x2,
    DEVICE_NONE = 0x3
};

#ifdef PULSE_AUDIO_ENABLE

static void
ALBackendServerInfoCallback(
    pa_context *pContext,
    const pa_server_info *pServerInfo,
    void *pUserData)
{
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData = NULL;
    XAuint32 Size = 0;
    XAuint32 NDevices = 0;
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (!(pContext && pServerInfo && pUserData))
    {
         return;
    }
    pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)pAudioIODeviceCapabilities->pData;

    if (!pAudioIODeviceCapabilitiesData)
    {
        XA_LOGD("Failed to get server information: %s",
            pa_strerror(pa_context_errno(pContext)));
        return;
    }

    Size = strlen(pServerInfo->default_sink_name);
    NDevices = pAudioIODeviceCapabilitiesData->mDefaultOutputDevices;
    pAudioIODeviceCapabilitiesData->mDefaultOutputDeviceName[NDevices] =
        (XAchar *)malloc(Size * sizeof(XAchar));
    if (pAudioIODeviceCapabilitiesData->mDefaultOutputDeviceName[NDevices])
    {
        strncpy(
            (char *)&pAudioIODeviceCapabilitiesData->
                mDefaultOutputDeviceName[NDevices],
                (const char *)pServerInfo->default_sink_name, Size + 1);
    }
    pAudioIODeviceCapabilitiesData->mDefaultOutputDevices++;

    Size = strlen(pServerInfo->default_source_name);
    NDevices = pAudioIODeviceCapabilitiesData->mDefaultInputDevices;

    pAudioIODeviceCapabilitiesData->mDefaultInputDeviceName[NDevices] =
        (XAchar *)malloc(Size * sizeof(XAchar));
    if (pAudioIODeviceCapabilitiesData->mDefaultInputDeviceName[NDevices])
    {
        strncpy(
            (char *)&pAudioIODeviceCapabilitiesData->
                mDefaultInputDeviceName[NDevices],
                (const char *)pServerInfo->default_source_name, Size + 1);
    }
    pAudioIODeviceCapabilitiesData->mDefaultInputDevices++;
}

static void
ALBackendUpdateAudioInputDescriptor(
    AudioDevicesInputOutputDescriptor *pAIODescriptor,
    const pa_source_info *SourceInfo)
{
    XAAudioInputDescriptor *pInputDescriptor = NULL;
    XAuint32 size = 0;

    if (pAIODescriptor && SourceInfo)
    {
        pInputDescriptor = &pAIODescriptor->mInputDescriptor;
        if (pInputDescriptor)
        {
            pInputDescriptor->deviceConnection = XA_DEVCONNECTION_ATTACHED_WIRED;
            pInputDescriptor->deviceScope = XA_DEVSCOPE_USER;
            pInputDescriptor->deviceLocation = XA_DEVLOCATION_HEADSET;
            pInputDescriptor->isForTelephony = XA_BOOLEAN_TRUE;
            pInputDescriptor->maxSampleRate = SourceInfo->sample_spec.rate;
            pInputDescriptor->minSampleRate = SourceInfo->sample_spec.rate;
            pInputDescriptor->isFreqRangeContinuous = XA_BOOLEAN_TRUE;
            //XAmilliHertz *samplingRatesSupported;
            pInputDescriptor->samplingRatesSupported = NULL;
            pInputDescriptor->numOfSamplingRatesSupported = 1;
            pInputDescriptor->maxChannels = SourceInfo->sample_spec.channels;
            size = strlen(SourceInfo->description);
            pInputDescriptor->deviceName = (XAchar *)malloc(size * sizeof(XAuint32));
            strncpy((char *)pInputDescriptor->deviceName,
                (const char *)SourceInfo->description, size + 1);
            pAIODescriptor->mSampleFormat = SourceInfo->sample_spec.format;
            pAIODescriptor->mSampleRate = SourceInfo->sample_spec.rate;
            pAIODescriptor->mDeviceId = (SourceInfo->index | (AUDIO_SOURCE << 16));
            pAIODescriptor->mDeviceType = AUDIO_SOURCE;
            pAIODescriptor->mCardId = SourceInfo->card;
            pAIODescriptor->mCVolume = SourceInfo->volume;
            pAIODescriptor->mChannels = SourceInfo->sample_spec.channels;
        }
    }
}

static void
ALBackendAddDeviceToSourceList(
    IAudioIODeviceCapabilities *pUserData,
    const pa_source_info *SourceInfo)
{
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData = NULL;
    AudioDevicesInputOutputDescriptor *pAIODescriptor = NULL;
    const XAAudioIODeviceCapabilitiesItf *const *IAudioIODeviceCapabilitiesItf = NULL;
    XAuint32 i = 0;
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (!(SourceInfo && pUserData && pUserData->pData))
    {
         return;
    }
    IAudioIODeviceCapabilitiesItf =
        (const XAAudioIODeviceCapabilitiesItf *const *)pAudioIODeviceCapabilities->mItf;

    pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)pAudioIODeviceCapabilities->pData;

    if (pAudioIODeviceCapabilitiesData->mInputDevices > MAX_AUDIO_IO_DEVICES)
        return;

    pAIODescriptor =
        &s_AudioIODevicesDescriptor[pAudioIODeviceCapabilitiesData->mDevices];

    if (pAIODescriptor)
    {
        ALBackendUpdateAudioInputDescriptor(
            pAIODescriptor,
            SourceInfo);

        for (i = 0; i < pAudioIODeviceCapabilitiesData->mDefaultInputDevices; i++)
        {
            if (!strcmp(
                (char *)&pAudioIODeviceCapabilitiesData->mDefaultInputDeviceName[i],
                SourceInfo->name))
            {
                pAIODescriptor->mLinuxDefaultAudioInputDeviceID =
                    (SourceInfo->index | (AUDIO_SOURCE << 16));
                break;
            }
        }
        pAudioIODeviceCapabilitiesData->mDevices++;
        pAudioIODeviceCapabilitiesData->mInputDevices++;
        if (pAudioIODeviceCapabilitiesData->mAvailableAudioInputsChangedCallback)
        {
            pAudioIODeviceCapabilitiesData->mAvailableAudioInputsChangedCallback(
                (XAAudioIODeviceCapabilitiesItf)IAudioIODeviceCapabilitiesItf,
                (void *)pAudioIODeviceCapabilitiesData,
                (SourceInfo->index | (AUDIO_SOURCE << 16)),
                pAudioIODeviceCapabilitiesData->mInputDevices,
                XA_BOOLEAN_TRUE);
        }
    }
}

static void
ALBackendNotifyIODeviceChangeCallbacks(
    void *pUserData,
    XAuint32 DeviceIndex,
    XAboolean bDeviceConnection)
{
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData = NULL;
    AudioDevicesInputOutputDescriptor *pAIODescriptor = NULL;
    const XAAudioIODeviceCapabilitiesItf *const *IAudioIODeviceCapabilitiesItf = NULL;
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;
    XAboolean bNewDevice = bDeviceConnection;
    XAuint32 NDevices = 0;
    XAuint32 i = 0;

    if (!(pAudioIODeviceCapabilities && pAudioIODeviceCapabilities->pData))
    {
         return;
    }
    IAudioIODeviceCapabilitiesItf =
        (const XAAudioIODeviceCapabilitiesItf *const *)pAudioIODeviceCapabilities->mItf;

    pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)pAudioIODeviceCapabilities->pData;

    NDevices = pAudioIODeviceCapabilitiesData->mDevices;
    for (i =0; i < NDevices; i++)
    {
        pAIODescriptor = &s_AudioIODevicesDescriptor[i];
        if (DeviceIndex == pAIODescriptor->mDeviceId)
        {
            if (pAIODescriptor->mDeviceType == AUDIO_SINK)
            {
                pAudioIODeviceCapabilitiesData->mOutputDevices--;
                // Check if call backs registered from AL and update..
                if (pAudioIODeviceCapabilitiesData->mAvailableAudioOutputsChangedCallback)
                {
                    pAudioIODeviceCapabilitiesData->mAvailableAudioOutputsChangedCallback(
                        (XAAudioIODeviceCapabilitiesItf)IAudioIODeviceCapabilitiesItf,
                        (void *)pAudioIODeviceCapabilitiesData,
                        DeviceIndex,
                        pAudioIODeviceCapabilitiesData->mOutputDevices,
                        bNewDevice);
                }
            }
            else if (pAIODescriptor->mDeviceType == AUDIO_SOURCE)
            {
                pAudioIODeviceCapabilitiesData->mInputDevices--;
                // Check if call backs registered from AL and update..
                if (pAudioIODeviceCapabilitiesData->mAvailableAudioInputsChangedCallback)
                {
                    pAudioIODeviceCapabilitiesData->mAvailableAudioInputsChangedCallback(
                        (XAAudioIODeviceCapabilitiesItf)IAudioIODeviceCapabilitiesItf,
                        (void *)pAudioIODeviceCapabilitiesData,
                        DeviceIndex,
                        pAudioIODeviceCapabilitiesData->mInputDevices,
                        bNewDevice);
                }
            }
            while(i+1 < NDevices)
            {
                s_AudioIODevicesDescriptor[i] = s_AudioIODevicesDescriptor[i+1];
                i++;
            }
            pAudioIODeviceCapabilitiesData->mDevices--;
            if (s_OutputMixRegisterCallback.mCallback)
            {
                s_OutputMixRegisterCallback.mCallback(
                    (XAOutputMixItf)s_OutputMixRegisterCallback.mItf,
                    (void *)s_OutputMixRegisterCallback.pContext);
            }
            break;
        }
    }
    NDevices = (pAIODescriptor->mDeviceType == AUDIO_SINK) ?
        pAudioIODeviceCapabilitiesData->mDefaultOutputDevices :
        pAudioIODeviceCapabilitiesData->mDefaultInputDevices;

    for (i = 0; i < NDevices && i < MAX_AUDIO_IO_DEVICES; i++)
    {
        pAIODescriptor = &s_AudioIODevicesDescriptor[i];
        if (DeviceIndex == pAIODescriptor->mLinuxDefaultAudioOutputDeviceID)
        {
            pAudioIODeviceCapabilitiesData->mDefaultOutputDevices--;
            // Check if default call backs registered from AL.
            if (pAudioIODeviceCapabilitiesData->mDefaultDeviceIDMapChangedCallback)
            {
                pAudioIODeviceCapabilitiesData->mDefaultDeviceIDMapChangedCallback(
                    (XAAudioIODeviceCapabilitiesItf)IAudioIODeviceCapabilitiesItf,
                    (void *)pAudioIODeviceCapabilitiesData,
                    XA_BOOLEAN_TRUE,
                    pAudioIODeviceCapabilitiesData->mDefaultOutputDevices);
            }
            break;
        }
        else if (DeviceIndex == pAIODescriptor->mLinuxDefaultAudioInputDeviceID)
        {
            pAudioIODeviceCapabilitiesData->mDefaultInputDevices--;
            // Check if default call backs registered from AL.
            if (pAudioIODeviceCapabilitiesData->mDefaultDeviceIDMapChangedCallback)
            {
                pAudioIODeviceCapabilitiesData->mDefaultDeviceIDMapChangedCallback(
                    (XAAudioIODeviceCapabilitiesItf)IAudioIODeviceCapabilitiesItf,
                    (void *)pAudioIODeviceCapabilitiesData,
                    XA_BOOLEAN_FALSE,
                    pAudioIODeviceCapabilitiesData->mDefaultInputDevices);
            }
            break;
        }
    }
}

static void
ALBackendSourceInfoCallback(
    pa_context *pContext,
    const pa_source_info *SourceInfo,
    int IsLast,
    void *pUserData)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (!(pContext && SourceInfo && pUserData) || IsLast)
    {
         return;
    }

    ALBackendAddDeviceToSourceList(pAudioIODeviceCapabilities, SourceInfo);
}

static void
ALBackendUpdateAudioOutputDescriptor(
    AudioDevicesInputOutputDescriptor *pAIODescriptor,
    const pa_sink_info *SinkInfo)
{
    XAAudioOutputDescriptor *pOutputDescriptor = NULL;
    XAuint32 size = 0;

    if (pAIODescriptor && SinkInfo)
    {
        pOutputDescriptor = &pAIODescriptor->mOutputDescriptor;
        if (pOutputDescriptor)
        {
            pOutputDescriptor->deviceConnection = XA_DEVCONNECTION_ATTACHED_WIRED;
            pOutputDescriptor->deviceScope = XA_DEVSCOPE_USER;
            pOutputDescriptor->deviceLocation = XA_DEVLOCATION_HEADSET;
            pOutputDescriptor->isForTelephony = XA_BOOLEAN_TRUE;
            pOutputDescriptor->maxSampleRate = SinkInfo->sample_spec.rate;
            pOutputDescriptor->minSampleRate = SinkInfo->sample_spec.rate;
            pOutputDescriptor->isFreqRangeContinuous = XA_BOOLEAN_TRUE;
            //XAmilliHertz *samplingRatesSupported;
            pOutputDescriptor->samplingRatesSupported = NULL;
            pOutputDescriptor->numOfSamplingRatesSupported = 1;
            pOutputDescriptor->maxChannels = SinkInfo->sample_spec.channels;
            size = strlen(SinkInfo->description);
            pOutputDescriptor->pDeviceName = (XAchar *)malloc(size * sizeof(XAuint32));
            strncpy((char *)pOutputDescriptor->pDeviceName,
                (const char *)SinkInfo->description, size + 1);
            pAIODescriptor->mSampleFormat = SinkInfo->sample_spec.format;
            pAIODescriptor->mSampleRate = SinkInfo->sample_spec.rate;
            pAIODescriptor->mDeviceId = (SinkInfo->index | (AUDIO_SINK<<16));
            pAIODescriptor->mDeviceType = AUDIO_SINK;
            pAIODescriptor->mCardId = SinkInfo->card;
            pAIODescriptor->mCVolume = SinkInfo->volume;
            pAIODescriptor->mChannels = SinkInfo->sample_spec.channels;
        }
    }
}

static void
ALBackendAddDeviceToSinkList(
    IAudioIODeviceCapabilities *pUserData,
    const pa_sink_info *SinkInfo)
{
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData = NULL;
    AudioDevicesInputOutputDescriptor *pAIODescriptor = NULL;
    const XAAudioIODeviceCapabilitiesItf *const *IAudioIODeviceCapabilitiesItf = NULL;
    XAuint32 i = 0;
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (!(SinkInfo && pUserData && pUserData->pData))
    {
         return;
    }
    IAudioIODeviceCapabilitiesItf =
        (const XAAudioIODeviceCapabilitiesItf *const *)pAudioIODeviceCapabilities->mItf;

    pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)pAudioIODeviceCapabilities->pData;

    if (pAudioIODeviceCapabilitiesData->mDevices > MAX_AUDIO_IO_DEVICES)
        return;

    pAIODescriptor =
        &s_AudioIODevicesDescriptor[pAudioIODeviceCapabilitiesData->mDevices];

    if (pAIODescriptor)
    {
        ALBackendUpdateAudioOutputDescriptor(
            pAIODescriptor,
            SinkInfo);


        for (i = 0; i < pAudioIODeviceCapabilitiesData->mDefaultOutputDevices; i++)
        {
            if (!strcmp(
                (char *)&pAudioIODeviceCapabilitiesData->mDefaultOutputDeviceName[i],
                SinkInfo->name))
            {
                pAIODescriptor->mLinuxDefaultAudioOutputDeviceID =
                    (SinkInfo->index | (AUDIO_SINK << 16));
                break;
            }
        }
        pAudioIODeviceCapabilitiesData->mDevices++;
        pAudioIODeviceCapabilitiesData->mOutputDevices++;
        if (pAudioIODeviceCapabilitiesData->mAvailableAudioOutputsChangedCallback)
        {
            pAudioIODeviceCapabilitiesData->mAvailableAudioOutputsChangedCallback(
                (XAAudioIODeviceCapabilitiesItf)IAudioIODeviceCapabilitiesItf,
                (void *)pAudioIODeviceCapabilitiesData,
                (SinkInfo->index | (AUDIO_SINK << 16)),
                pAudioIODeviceCapabilitiesData->mOutputDevices,
                XA_BOOLEAN_TRUE);
        }
    }
    // Notify AL OutputMix registered device change call back
    if (s_OutputMixRegisterCallback.mCallback)
    {
        s_OutputMixRegisterCallback.mCallback(
            (XAOutputMixItf)s_OutputMixRegisterCallback.mItf,
            (void *)s_OutputMixRegisterCallback.pContext);
    }
}

static void
ALBackendDeInitializeSinkInput(XAuint32 StreamId)
{
    XAuint32 Index = 0;

    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        if (AUDIO_SINK == s_AudioIODevicesDescriptor[Index].mDeviceType &&
            StreamId == s_AudioIODevicesDescriptor[Index].mStreamId)
        {
             s_AudioIODevicesDescriptor[Index].mStreamId = -1;
        }
    }
}

static void
ALBackendUpdateSinkInput(
    IAudioIODeviceCapabilities *pUserData,
    const pa_sink_input_info *SinkInputInfo)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index = 0;

    if (SinkInputInfo)
    {
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];
            if (AUDIO_SINK == pAudioInputOutPutDescriptor->mDeviceType)
            {
                pAudioInputOutPutDescriptor->mStreamId = SinkInputInfo->index;
            }
        }
    }
}

static void
ALBackendSinkInfoCallback(
    pa_context *pContext,
    const pa_sink_info *SinkInfo,
    int IsLast,
    void *pUserData)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (!pContext || !SinkInfo || IsLast || !pUserData)
    {
         return;
    }

    ALBackendAddDeviceToSinkList(
        pAudioIODeviceCapabilities,
        SinkInfo);
}

static void
ALBackendSinkInputInfoCallback(
    pa_context *pContext,
    const pa_sink_input_info *SinkInputInfo,
    int IsLast,
    void *pUserData)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (!pContext || !SinkInputInfo || IsLast || !pUserData)
    {
         return;
    }
    ALBackendUpdateSinkInput(pAudioIODeviceCapabilities, SinkInputInfo);
}

static void
ALBackendQueryDeviceInfo(pa_context *pContext, void *pUserData)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (pContext && pUserData)
    {
        pa_operation_unref(
            pa_context_get_server_info(
                pContext,
                ALBackendServerInfoCallback,
                (void *)pAudioIODeviceCapabilities));
        pa_operation_unref(
            pa_context_get_sink_input_info_list(
                pContext,
                ALBackendSinkInputInfoCallback,
                (void *)pAudioIODeviceCapabilities));
        pa_operation_unref(
            pa_context_get_sink_info_list(
                pContext,
                ALBackendSinkInfoCallback,
                (void *)pAudioIODeviceCapabilities));
        pa_operation_unref(
            pa_context_get_source_info_list(
                pContext,
                ALBackendSourceInfoCallback,
                (void *)pAudioIODeviceCapabilities));
    }
}

static void
ALBackendContextSubscribeCallback(
    pa_context *pContext,
    pa_subscription_event_type_t EventType,
    uint32_t Index,
    void *pUserData)
{
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData = NULL;
    XAuint32 DeviceConnection = DEVICE_NONE;
    pa_operation *pOpeartion = NULL;
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;

    if (pContext && pAudioIODeviceCapabilities &&
        pAudioIODeviceCapabilities->pData)
    {
        pAudioIODeviceCapabilitiesData =
            (AudioIODeviceCapabilitiesData *)pAudioIODeviceCapabilities->pData;

        switch (EventType & PA_SUBSCRIPTION_EVENT_TYPE_MASK)
        {
            case PA_SUBSCRIPTION_EVENT_NEW:
                DeviceConnection = DEVICE_ADDED;
                break;
            case PA_SUBSCRIPTION_EVENT_REMOVE:
                DeviceConnection = DEVICE_REMOVED;
                break;
            default:
                return;
        }

        switch (EventType & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
        {
            case PA_SUBSCRIPTION_EVENT_SINK:
                XA_LOGD("\n sink inserted or removed device/card %d",
                    Index | (AUDIO_SINK << 16));

                if (DeviceConnection == DEVICE_REMOVED)
                {
                    ALBackendNotifyIODeviceChangeCallbacks(
                        (void *)pAudioIODeviceCapabilities,
                        Index | (AUDIO_SINK << 16),
                        XA_BOOLEAN_FALSE);
                }
                else if (DeviceConnection == DEVICE_ADDED)
                {
                    if (!(pOpeartion = pa_context_get_sink_info_by_index(
                        pContext,
                        Index,
                        ALBackendSinkInfoCallback,
                        (void *)pAudioIODeviceCapabilities)))
                    {
                        XA_LOGD("\nFailed Pacontext_get_sink_info_by_index\n");
                        break;
                    }
                }
                break;
            case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
                XA_LOGD("\n sink input inserted or removed device/card %d", Index);
                if (DeviceConnection == DEVICE_REMOVED)
                {
                    XA_LOGD("\n sink input removed device/card %d", Index);
                    ALBackendDeInitializeSinkInput(Index);
                }
                else if (DeviceConnection == DEVICE_ADDED)
                {
                    XA_LOGD("\n sink input added device/card %d\n", Index);
                    if (!(pOpeartion = pa_context_get_sink_input_info(
                        pContext,
                        Index,
                        ALBackendSinkInputInfoCallback,
                        (void *)pAudioIODeviceCapabilities)))
                    {
                        XA_LOGD("\nFailed Pacontext_get_sink_info_by_index\n");
                        break;
                    }
                }
                break;
            case PA_SUBSCRIPTION_EVENT_SOURCE:
                XA_LOGD("\n source inserted or removed device/card %d",
                    Index | (AUDIO_SOURCE << 16));
                if (DeviceConnection == DEVICE_REMOVED)
                {
                    ALBackendNotifyIODeviceChangeCallbacks(
                        (void *)pAudioIODeviceCapabilities,
                        Index | (AUDIO_SOURCE << 16),
                        XA_BOOLEAN_FALSE);
                }
                else if (DeviceConnection == DEVICE_ADDED)
                {
                    if (!(pOpeartion = pa_context_get_source_info_by_index(
                        pContext,
                        Index,
                        ALBackendSourceInfoCallback,
                        (void *)pAudioIODeviceCapabilities)))
                    {
                        XA_LOGD("\nfail pacontext_get_source_info_by_index\n");
                        break;
                    }
                }
                break;
            case PA_SUBSCRIPTION_EVENT_SERVER:
                XA_LOGD("\nPulseAudio: Server changed");
                pa_operation_unref(
                    pa_context_get_server_info(
                    pContext,
                    ALBackendServerInfoCallback,
                    (void *)pAudioIODeviceCapabilities));
                break;
            default:
                return;
        }
    }
}

void ALBackendContextStateCallback(
    pa_context *pContext,
    void *pUserData)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCapabilities =
        (IAudioIODeviceCapabilities *)pUserData;
    pa_operation *pOperation = NULL;

    if (pContext && pAudioIODeviceCapabilities)
    {
        switch (pa_context_get_state(pContext))
        {
            case PA_CONTEXT_CONNECTING:
            case PA_CONTEXT_AUTHORIZING:
            case PA_CONTEXT_SETTING_NAME:
                break;
            case PA_CONTEXT_READY:
                XA_LOGD("\nPA_CONTEXT_READY:%s %d\n", __FUNCTION__, __LINE__);

                if (!(pOperation = pa_context_subscribe(
                    pContext,
                    PA_SUBSCRIPTION_MASK_SERVER,
                    NULL,
                    (void *)pAudioIODeviceCapabilities)))
                {
                    XA_LOGD("\nFailed to subscribe server events: %s",
                        pa_strerror(pa_context_errno(pContext)));
                }
                if (!(pOperation = pa_context_subscribe(
                    pContext,
                    PA_SUBSCRIPTION_MASK_SOURCE,
                    NULL,
                    (void *)pAudioIODeviceCapabilities)))
                {
                    XA_LOGD("\nFailed to subscribe AUDIO_SOURCE events: %s",
                        pa_strerror(pa_context_errno(pContext)));
                }
                if (!(pOperation = pa_context_subscribe(
                    pContext,
                    PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT,
                    NULL,
                    (void *)pAudioIODeviceCapabilities)))
                {
                    XA_LOGD("\nFailed to subscribe AUDIO_SINK events: %s",
                        pa_strerror(pa_context_errno(pContext)));
                }

                ALBackendQueryDeviceInfo(
                    pContext,
                    (void *)pAudioIODeviceCapabilities);
                break;
            case PA_CONTEXT_TERMINATED:
                XA_LOGD("\nPulseAudio: Force disconnect from PulseAudio\n");
                break;
            case PA_CONTEXT_FAILED:
                XA_LOGD("\nPulseAudio: Connection failure: %s",
                    pa_strerror(pa_context_errno(pContext)));
                break;
            default:
                break;
         }
     }
}

static void
ALBackendCreatePulseContext(
    void *pCObjectContext,
    PulseAudioContextData * pPulseAudioContextData)
{
    pa_mainloop_api *pMainLoopApi;

    if (pPulseAudioContextData && pCObjectContext)
    {
        pPulseAudioContextData->mMainLoop = pa_threaded_mainloop_new();
        if (pPulseAudioContextData->mMainLoop)
        {
            pMainLoopApi = pa_threaded_mainloop_get_api(
                pPulseAudioContextData->mMainLoop);

            pPulseAudioContextData->mContext =
                pa_context_new(pMainLoopApi, NULL);

            pa_context_set_subscribe_callback(
                pPulseAudioContextData->mContext,
                ALBackendContextSubscribeCallback,
                (void *)pCObjectContext);

            pa_context_set_state_callback(
                pPulseAudioContextData->mContext,
                ALBackendContextStateCallback,
                (void *)pCObjectContext);

            // Connect the context
            if (pa_context_connect(
                pPulseAudioContextData->mContext,
                NULL,
                PA_CONTEXT_NOAUTOSPAWN,
                NULL) < 0)
            {
                XA_LOGD("pa_context_connect() failed.\n");
                return;
            }
            // Start the mainloop
            pa_threaded_mainloop_start(
                pPulseAudioContextData->mMainLoop);
        }
    }
}
#endif

XAresult
ALBackendQuerySampleFormatsSupported(
    XAuint32 DeviceID,
    XAmilliHertz SamplingRate,
    XAint32 *pNumSampleFormats,
    XAint32 *pSampleFormats)
{
    XAuint32 Index = 0;
    XAuint32 NFormats = 0;
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumSampleFormats);
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];
        if ((DeviceID == pAudioInputOutPutDescriptor->mDeviceId)
            && (SamplingRate == pAudioInputOutPutDescriptor->mSampleRate))
        {
            switch (pAudioInputOutPutDescriptor->mSampleFormat)
            {
#ifdef PULSE_AUDIO_ENABLE
                case PA_SAMPLE_U8:
                    if (pSampleFormats)
                        pSampleFormats[NFormats] = XA_PCMSAMPLEFORMAT_FIXED_8;
                    NFormats += 1;
                    break;
                case PA_SAMPLE_S16LE:
                case PA_SAMPLE_S16BE:
                    if (pSampleFormats)
                        pSampleFormats[NFormats] = XA_PCMSAMPLEFORMAT_FIXED_16;
                    NFormats += 1;
                    break;
#endif
                default:
                    result = XA_RESULT_IO_ERROR;
                    break;
            }
            result = *pNumSampleFormats < NFormats ?
                XA_RESULT_BUFFER_INSUFFICIENT : XA_RESULT_SUCCESS;
            *pNumSampleFormats = NFormats;
            break;
        }
    }
    XA_LEAVE_INTERFACE
}

static XAresult
ALBackendProbeNDevicesType(
    XAuint32 DeviceType,
    XAint32 *pNumDevices)
{
    XAuint32 Index;
    *pNumDevices = 0;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumDevices)
    if (NULL != pNumDevices)
    {
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            if (DeviceType == s_AudioIODevicesDescriptor[Index].mDeviceType)
            {
                *pNumDevices += 1;
            }
        }
    }
    XA_LEAVE_INTERFACE
}

static void
ALBackendProbeCardID(
    AudioIODeviceCapabilitiesData *pCapsData,
    XAuint32 DeviceID,
    XAuint32 *pCardID)
{
    XAuint32 Index;
    if (pCardID && pCapsData)
    {
        switch (DeviceID)
        {
            case XA_DEFAULTDEVICEID_AUDIOOUTPUT:
                for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
                {
                    if (XA_DEFAULTDEVICEID_AUDIOOUTPUT !=
                        s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID)
                    {
                        DeviceID =
                            s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID;
                        break;
                    }
                }
                break;
            case XA_DEFAULTDEVICEID_AUDIOINPUT:
                for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
                {
                    if (XA_DEFAULTDEVICEID_AUDIOINPUT !=
                        s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioInputDeviceID)
                    {
                        DeviceID =
                            s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioInputDeviceID;
                        break;
                    }
                }
                break;
        }
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            // Retrieve card id for source or sink device.
            if (DeviceID == s_AudioIODevicesDescriptor[Index].mDeviceId)
            {
                *pCardID = s_AudioIODevicesDescriptor[Index].mCardId;
                break;
            }
        }
    }
}

XAresult
ALBackendProbeNDevicesIDs(
    XAuint32 DeviceType,
    XAint32 *pNumDevices,
    XAuint32 *pDeviceIDs)
{
    XAuint32 Index;
    XAint32 NDevices = 0;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumDevices && pDeviceIDs);
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        if (DeviceType == s_AudioIODevicesDescriptor[Index].mDeviceType)
        {
            if (NDevices < *pNumDevices)
                pDeviceIDs[NDevices] = s_AudioIODevicesDescriptor[Index].mDeviceId;
            NDevices++;
        }
    }
    result = *pNumDevices < NDevices ?
        XA_RESULT_BUFFER_INSUFFICIENT : XA_RESULT_SUCCESS;
    *pNumDevices = NDevices;

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendGetAssociatedAudioOutputs(
    void *self,
    XAuint32 DeviceID,
    XAint32 *pNumOutputs,
    XAuint32 *pOutputDeviceIDs)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index;
    XAint32 OutputDevices = 0;
    XAuint32 CardID = -1;
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pAudioIODeviceCapabilitiesData && pNumOutputs);
    if (NULL == pOutputDeviceIDs)
    {
        result = ALBackendProbeNDevicesType(AUDIO_SINK, pNumOutputs);
        XA_LEAVE_INTERFACE
    }
    ALBackendProbeCardID(pAudioIODeviceCapabilitiesData, DeviceID, &CardID);
    if (CardID == -1)
    {
        *pNumOutputs = 0;
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE
    }
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];
        // compute assosciated sink devices
        if (AUDIO_SINK == pAudioInputOutPutDescriptor->mDeviceType &&
            DeviceID != pAudioInputOutPutDescriptor->mDeviceId &&
            CardID == pAudioInputOutPutDescriptor->mCardId)
        {
            if (OutputDevices < *pNumOutputs)
            {
                pOutputDeviceIDs[OutputDevices] =
                    pAudioInputOutPutDescriptor->mDeviceId;
            }
            OutputDevices++;
        }
    }
    result = *pNumOutputs < OutputDevices ?
        XA_RESULT_BUFFER_INSUFFICIENT : XA_RESULT_SUCCESS;
    *pNumOutputs = OutputDevices;
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendGetAssociatedAudioInputs(
    void *self,
    XAuint32 DeviceID,
    XAint32 *pNumInputs,
    XAuint32 *pInputDeviceIDs)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index;
    XAuint32 InputDevices = 0;
    XAuint32 CardID = -1;
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pAudioIODeviceCapabilitiesData && pNumInputs);
    if (NULL == pInputDeviceIDs)
    {
        result = ALBackendProbeNDevicesType(AUDIO_SOURCE, pNumInputs);
        XA_LEAVE_INTERFACE
    }
    ALBackendProbeCardID(pAudioIODeviceCapabilitiesData, DeviceID, &CardID);
    if (CardID == -1)
    {
        *pNumInputs = 0;
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE
    }
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];
        // compute assosciated source devices
        if (AUDIO_SOURCE == pAudioInputOutPutDescriptor->mDeviceType &&
            DeviceID != pAudioInputOutPutDescriptor->mDeviceId &&
            CardID == pAudioInputOutPutDescriptor->mCardId)
        {
            if (InputDevices < *pNumInputs)
            {
                pInputDeviceIDs[InputDevices] =
                    pAudioInputOutPutDescriptor->mDeviceId;
            }
            InputDevices++;
        }
    }
    result = *pNumInputs < InputDevices ?
        XA_RESULT_BUFFER_INSUFFICIENT : XA_RESULT_SUCCESS;
    *pNumInputs = InputDevices;
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendQueryAudioInputCapabilities(
    void *self,
    XAuint32 DeviceID,
    XAAudioInputDescriptor *pDescriptor)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index;
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pAudioIODeviceCapabilitiesData && pDescriptor);
    result = XA_RESULT_PARAMETER_INVALID;
    if (DeviceID == XA_DEFAULTDEVICEID_AUDIOINPUT)
    {
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            if (XA_DEFAULTDEVICEID_AUDIOINPUT !=
                s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioInputDeviceID)
            {
                DeviceID =
                    s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioInputDeviceID;
                break;
            }
        }
    }
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];

        if (AUDIO_SOURCE == pAudioInputOutPutDescriptor->mDeviceType &&
            DeviceID == pAudioInputOutPutDescriptor->mDeviceId)
        {
            memcpy(
                pDescriptor,
                &pAudioInputOutPutDescriptor->mInputDescriptor,
                sizeof(XAAudioInputDescriptor));

            result = XA_RESULT_SUCCESS;
            break;
        }
    }
    XA_LEAVE_INTERFACE
}


XAresult
ALBackendQueryAudioOutputCapabilities(
    void *self,
    XAuint32 DeviceID,
    XAAudioOutputDescriptor *pDescriptor)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index;
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapabilitiesData =
        (AudioIODeviceCapabilitiesData *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pAudioIODeviceCapabilitiesData && pDescriptor);
    result = XA_RESULT_PARAMETER_INVALID;
    if (DeviceID == XA_DEFAULTDEVICEID_AUDIOOUTPUT)
    {
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            if (XA_DEFAULTDEVICEID_AUDIOINPUT !=
                s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID)
            {
                DeviceID =
                    s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID;
                break;
            }
        }
    }
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];
        if (AUDIO_SINK == pAudioInputOutPutDescriptor->mDeviceType &&
            DeviceID == pAudioInputOutPutDescriptor->mDeviceId)
        {
            memcpy(
                pDescriptor,
                &pAudioInputOutPutDescriptor->mOutputDescriptor,
                sizeof(XAAudioOutputDescriptor));

            result = XA_RESULT_SUCCESS;
            break;
        }
    }
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendGetAvailableAudioOutputs(
    XAint32 *pNumOutputs,
    XAuint32 *pOutputDeviceIDs)
{

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumOutputs);
    if (NULL == pOutputDeviceIDs)
    {
        result = ALBackendProbeNDevicesType(
            AUDIO_SINK,
            pNumOutputs);
        XA_LEAVE_INTERFACE
    }
    result = ALBackendProbeNDevicesIDs(
        AUDIO_SINK,
        pNumOutputs,
        pOutputDeviceIDs);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendGetAvailableAudioInputs(
    XAint32 *pNumInputs,
    XAuint32 *pInputDeviceIDs)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumInputs);
    if ( NULL == pInputDeviceIDs)
    {
        result = ALBackendProbeNDevicesType(
            AUDIO_SOURCE,
            pNumInputs);
        XA_LEAVE_INTERFACE
    }
    result = ALBackendProbeNDevicesIDs(
        AUDIO_SOURCE,
        pNumInputs,
        pInputDeviceIDs);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendGetDefaultAudioDevices(
    void *self,
    XAuint32 DefaultDeviceID,
    XAint32 *NumDefaults,
    XAuint32 *pAudioDeviceIDs)
{
    XAuint32 Index;
    XAint32 NDefaults = 0;
    AudioIODeviceCapabilitiesData *pData =
        (AudioIODeviceCapabilitiesData *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pData && NumDefaults);
    switch (DefaultDeviceID)
    {
        case XA_DEFAULTDEVICEID_AUDIOINPUT:
        {
            if (pAudioDeviceIDs != NULL)
            {
                for (Index = 0; Index < MAX_AUDIO_IO_DEVICES &&
                    NDefaults < *NumDefaults; Index++)
                {
                    if (XA_DEFAULTDEVICEID_AUDIOINPUT !=
                        s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioInputDeviceID)
                    {
                        pAudioDeviceIDs[NDefaults] =
                            s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioInputDeviceID;
                        NDefaults++;
                    }
                }
                result = *NumDefaults < pData->mDefaultInputDevices ?
                    XA_RESULT_BUFFER_INSUFFICIENT : XA_RESULT_SUCCESS;
            }
            *NumDefaults = pData->mDefaultInputDevices;
            break;
        }
        case XA_DEFAULTDEVICEID_AUDIOOUTPUT:
        {
            if (pAudioDeviceIDs != NULL)
            {
                for (Index = 0; Index < MAX_AUDIO_IO_DEVICES &&
                    NDefaults < *NumDefaults; Index++)
                {
                    if (XA_DEFAULTDEVICEID_AUDIOOUTPUT !=
                        s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID)
                    {
                        pAudioDeviceIDs[NDefaults] =
                            s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID;
                        NDefaults++;
                        break;
                    }
                }
                result = *NumDefaults < pData->mDefaultOutputDevices ?
                    XA_RESULT_BUFFER_INSUFFICIENT : XA_RESULT_SUCCESS;
            }
            *NumDefaults = pData->mDefaultOutputDevices;
            break;
        }
        default:
            result = XA_RESULT_PARAMETER_INVALID;
            break;
    }
    XA_LEAVE_INTERFACE
}

void
ALBackendOutmixRegisterDeviceChangeCallback(
    void *self,
    xaMixDeviceChangeCallback callback,
    void *pContext)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;
    if (pIOutputMix)
    {
        s_OutputMixRegisterCallback.mItf = pIOutputMix->mItf;
        s_OutputMixRegisterCallback.mCallback = callback;
        s_OutputMixRegisterCallback.pContext = pContext;
    }
}

XAresult
ALBackendReRoute(
    XAint32 NumOutputDevices,
    XAuint32 *pOutputDeviceIDs)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 i;
    XAuint32 Index;
    XAuint32 SinkIndex;
    XAuint32 Identified = 0x0;
#ifdef PULSE_AUDIO_ENABLE
    PulseAudioContextData *pPulseAudioContextData = &s_PulseAudioContext;
    pa_operation* pPulseOperation = NULL;
#endif

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pOutputDeviceIDs);
    result = XA_RESULT_PARAMETER_INVALID;
    for (i = 0; i < NumOutputDevices; i++)
    {
        SinkIndex = pOutputDeviceIDs[i];
        if (SinkIndex == XA_DEFAULTDEVICEID_AUDIOOUTPUT)
        {
            for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
            {
                if (XA_DEFAULTDEVICEID_AUDIOOUTPUT !=
                    s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID)
                {
                    SinkIndex =
                        s_AudioIODevicesDescriptor[Index].mLinuxDefaultAudioOutputDeviceID;
                    break;
                }
            }
        }
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES && !Identified; Index++)
        {
            pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];
            if (AUDIO_SINK == pAudioInputOutPutDescriptor->mDeviceType &&
                SinkIndex == pAudioInputOutPutDescriptor->mDeviceId)
            {
#ifdef PULSE_AUDIO_ENABLE
                if (!(pPulseOperation = pa_context_move_sink_input_by_index(
                    pPulseAudioContextData->mContext,
                    pAudioInputOutPutDescriptor->mStreamId,
                    (SinkIndex & 0xFFFF),
                    NULL,
                    NULL)))
                {
                    XA_LOGD("\npa_context_move_sink_input_by_index() failed\n");
                    XA_LEAVE_INTERFACE
                }
                pa_operation_unref(pPulseOperation);
#endif
                result = XA_RESULT_SUCCESS;
                Identified = 0x1;
            }
        }
        if (Identified)
            break;
    }

    XA_LEAVE_INTERFACE
}

void
ALBackendOutputMixInitialize(void *self)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;
    if (pIOutputMix)
    {
        s_OutputMixRegisterCallback.mItf = pIOutputMix->mItf;
        s_OutputMixRegisterCallback.mCallback = NULL;
        s_OutputMixRegisterCallback.pContext = NULL;
    }
}

void
ALBackendOutputMixDeInitialize(void *self)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;
    if (pIOutputMix)
    {
        s_OutputMixRegisterCallback.mItf = NULL;
        s_OutputMixRegisterCallback.mCallback = NULL;
        s_OutputMixRegisterCallback.pContext = NULL;
    }
}

XAresult
ALBackendGetDeviceVolumeScale(
    void *self,
    XAuint32 DeviceID,
    XAint32 *pMinValue,
    XAint32 *pMaxValue)
{
    XA_ENTER_INTERFACE
#ifdef PULSE_AUDIO_ENABLE
    *pMinValue = PA_VOLUME_MUTED;
    *pMaxValue = PA_VOLUME_NORM;
#endif
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendSetDeviceVolume(
    void *self,
    XAuint32 DeviceID,
    XAint32 Volume)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index;
#ifdef PULSE_AUDIO_ENABLE
    XAuint8 Channels = 2;
    pa_cvolume CVolume;
    PulseAudioContextData *pPulseAudioContextData = &s_PulseAudioContext;
#endif
    IDeviceVolume *pDeviceVolume = (IDeviceVolume *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pDeviceVolume);
    result = XA_RESULT_PARAMETER_INVALID;
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];

        if (DeviceID == pAudioInputOutPutDescriptor->mDeviceId)
        {
#ifdef PULSE_AUDIO_ENABLE
            if (Volume > PA_VOLUME_NORM)
                Volume = PA_VOLUME_NORM;
            else if (Volume < PA_VOLUME_MUTED)
                Volume = PA_VOLUME_MUTED;

            Channels = pAudioInputOutPutDescriptor->mChannels;
            pa_cvolume_set(&CVolume, Channels, (pa_volume_t)Volume);
            pa_operation_unref(pa_context_set_sink_volume_by_index(
                pPulseAudioContextData->mContext,
                (DeviceID & 0xFFFF),
                &CVolume,
                NULL,
                NULL));
            pAudioInputOutPutDescriptor->mCVolume = CVolume;
            pAudioInputOutPutDescriptor->mCVolume.values[0] = Volume;
#endif
            result = XA_RESULT_SUCCESS;
            break;
        }
    }
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendGetDeviceVolume(
    void *self,
    XAuint32 DeviceID,
    XAint32 *pVolume)
{
    AudioDevicesInputOutputDescriptor *pAudioInputOutPutDescriptor = NULL;
    XAuint32 Index;
#ifdef PULSE_AUDIO_ENABLE
    pa_cvolume CVolume;
#endif
    IDeviceVolume *pDeviceVolume = (IDeviceVolume *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pDeviceVolume && pVolume);
    result = XA_RESULT_PARAMETER_INVALID;
    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioInputOutPutDescriptor = &s_AudioIODevicesDescriptor[Index];

        if (DeviceID == pAudioInputOutPutDescriptor->mDeviceId)
        {
#ifdef PULSE_AUDIO_ENABLE
            CVolume = pAudioInputOutPutDescriptor->mCVolume;
            *pVolume = (XAint32)CVolume.values[0];
            if (*pVolume > PA_VOLUME_NORM)
                *pVolume = PA_VOLUME_NORM;
            else if (*pVolume < PA_VOLUME_MUTED)
                *pVolume = PA_VOLUME_MUTED;
#endif
            result = XA_RESULT_SUCCESS;
            break;
        }
    }
    XA_LEAVE_INTERFACE
}

void
ALBackendInitializePulseAudio(
    void *self)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCaps =
        (IAudioIODeviceCapabilities *)self;
    AudioDevicesInputOutputDescriptor *pAudioIODescriptor = NULL;
    XAuint32 Index;

    if (pAudioIODeviceCaps)
    {
        for (Index = 0 ; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            pAudioIODescriptor = &s_AudioIODevicesDescriptor[Index];
            pAudioIODescriptor->mDeviceId = -1;
            pAudioIODescriptor->mCardId = -1;
            pAudioIODescriptor->mStreamId = -1;
            pAudioIODescriptor->mInputDescriptor.deviceName = NULL;
            pAudioIODescriptor->mOutputDescriptor.pDeviceName = NULL;
            pAudioIODescriptor->mLinuxDefaultAudioInputDeviceID  =
                XA_DEFAULTDEVICEID_AUDIOINPUT;
            pAudioIODescriptor->mLinuxDefaultAudioOutputDeviceID =
                XA_DEFAULTDEVICEID_AUDIOOUTPUT;
        }
#ifdef PULSE_AUDIO_ENABLE
        ALBackendCreatePulseContext(
            (void *)pAudioIODeviceCaps,
            &s_PulseAudioContext);
#endif
    }
}

void
ALBackendDeInitializePulseAudio(
    void *self)
{
    XAuint32 Index = 0;
    AudioDevicesInputOutputDescriptor *pAudioIODescriptor = NULL;
    AudioIODeviceCapabilitiesData *pAudioIODeviceCapsData = NULL;
#ifdef PULSE_AUDIO_ENABLE
    PulseAudioContextData *pPulseAudioContextData = &s_PulseAudioContext;
#endif
    IAudioIODeviceCapabilities *pAudioIODeviceCaps =
        (IAudioIODeviceCapabilities *)self;

    for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
    {
        pAudioIODescriptor = &s_AudioIODevicesDescriptor[Index];
        if (pAudioIODescriptor->mInputDescriptor.deviceName)
            free(pAudioIODescriptor->mInputDescriptor.deviceName);
        if (pAudioIODescriptor->mOutputDescriptor.pDeviceName)
            free(pAudioIODescriptor->mOutputDescriptor.pDeviceName);
    }
    if (pAudioIODeviceCaps && pAudioIODeviceCaps->pData)
    {
        pAudioIODeviceCapsData =
            (AudioIODeviceCapabilitiesData *)pAudioIODeviceCaps->pData;
        for (Index = 0; Index < MAX_AUDIO_IO_DEVICES; Index++)
        {
            if (pAudioIODeviceCapsData->mDefaultInputDeviceName[Index])
                free(pAudioIODeviceCapsData->mDefaultInputDeviceName[Index]);
            if (pAudioIODeviceCapsData->mDefaultOutputDeviceName[Index])
                free(pAudioIODeviceCapsData->mDefaultOutputDeviceName[Index]);
        }
    }
#ifdef PULSE_AUDIO_ENABLE
    if (pPulseAudioContextData)
    {
        if (pPulseAudioContextData->mContext)
        {
            pa_context_disconnect(pPulseAudioContextData->mContext);
            pPulseAudioContextData->mContext = NULL;
        }
        if (pPulseAudioContextData->mMainLoop)
        {
            pa_threaded_mainloop_stop(pPulseAudioContextData->mMainLoop);
            pa_threaded_mainloop_free(pPulseAudioContextData->mMainLoop);
            pPulseAudioContextData->mMainLoop = NULL;
        }
    }
#endif
}


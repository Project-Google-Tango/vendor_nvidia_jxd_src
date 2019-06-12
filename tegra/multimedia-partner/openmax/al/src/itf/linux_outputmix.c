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
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "linux_outputmix.h"

static const EqualizerPresets equalizerPresets[] =
{
    {"Default", {0.0, 0.0, 0.0}},
    {"Bass", {11.0, 1.0, -12.0}},
    {"Treble", {-9.0, 1.0, 12.0}}
};

static const XAuint16 kNumEqPresets =
    sizeof(equalizerPresets) / sizeof(equalizerPresets[0]);

XAresult ALBackendOutputmixCreate(COutputMix *pCOutputMix)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix)
    pCOutputMixData = (COutputMixData *)malloc(sizeof(COutputMixData));
    if (!pCOutputMixData)
    {
        result = XA_RESULT_MEMORY_FAILURE;
        XA_LEAVE_INTERFACE
    }
    memset(pCOutputMixData, 0, sizeof(COutputMixData));
    pCOutputMix->pData = (void *)pCOutputMixData;

    XA_LEAVE_INTERFACE
}

XAresult ALBackendOutputmixRealize(COutputMix *pCOutputMix, XAboolean bAsync)
{
    GstPad *gpad = NULL;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    //Create AudioSink element
    pCOutputMixData->pAudioSink =
        gst_element_factory_make("pulsesink", "audiosink");
    if (pCOutputMixData->pAudioSink == NULL)
    {
        pCOutputMixData->pAudioSink =
            gst_element_factory_make("autoaudiosink", "audiosink");
        if (pCOutputMixData->pAudioSink == NULL)
        {
            pCOutputMixData->pAudioSink =
                gst_element_factory_make("alsasink", "audiosink");
        }
    }
    //Return error incase audio sink is not created
    if (pCOutputMixData->pAudioSink == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE
    }

    //Create Volume element
    pCOutputMixData->pVolume =
        gst_element_factory_make("volume", "volume");
    if (pCOutputMixData->pVolume == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE
    }

    //Create Equalizer element
    pCOutputMixData->pEqualizer =
        gst_element_factory_make(DEFAULT_EQUALIZER_NAME, "equalizer");
    if (pCOutputMixData->pEqualizer == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE
    }

    pCOutputMixData->pOutputMixBin =
        gst_bin_new("outputmixbin");
    if (pCOutputMixData->pOutputMixBin == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE
    }
    gst_bin_add_many(
        GST_BIN(pCOutputMixData->pOutputMixBin),
        pCOutputMixData->pEqualizer,
        pCOutputMixData->pVolume,
        pCOutputMixData->pAudioSink,
        NULL);

    gpad = gst_element_get_pad(pCOutputMixData->pEqualizer, "sink");
    gst_element_add_pad(
        pCOutputMixData->pOutputMixBin, gst_ghost_pad_new("sink", gpad));
    gst_object_unref(gpad);

    gst_element_link_many(
        pCOutputMixData->pEqualizer,
        pCOutputMixData->pVolume,
        pCOutputMixData->pAudioSink,
        NULL);

    XA_LEAVE_INTERFACE
}

void ALBackendOutputmixDestroy(COutputMix *pCOutputMix)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE_VOID
    if (pCOutputMix && pCOutputMix->pData)
    {
        pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
        gst_bin_remove_many(
            GST_BIN(pCOutputMixData->pOutputMixBin),
            pCOutputMixData->pEqualizer,
            pCOutputMixData->pVolume,
            pCOutputMixData->pAudioSink,
            NULL);

        if (pCOutputMixData->pOutputMixBin)
            gst_object_unref(pCOutputMixData->pOutputMixBin);

        free(pCOutputMixData);
        pCOutputMix->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}

XAresult ALBackendOutputmixResume(COutputMix *pCOutputMix, XAboolean bAsync)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    if (pCOutputMixData->pOutputMixBin)
    {
        result = ChangeGraphState(
            pCOutputMixData->pOutputMixBin, GST_STATE_PLAYING);
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendOutputMixSetVolume(
    COutputMix *pCOutputMix,
    XAmillibel level)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    gst_stream_volume_set_volume(
        GST_STREAM_VOLUME(pCOutputMixData->pVolume),
        GST_STREAM_VOLUME_FORMAT_DB,
        (gdouble)(level/100));

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendOutputMixGetVolume(
    COutputMix *pCOutputMix,
    XAmillibel *pLevel)
{
    COutputMixData *pCOutputMixData = NULL;
    gdouble level;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData && pLevel)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    level = gst_stream_volume_get_volume(
        GST_STREAM_VOLUME(pCOutputMixData->pVolume),
        GST_STREAM_VOLUME_FORMAT_DB);
    *pLevel = (XAmillibel)(level * 100);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendOutputMixGetMaxVolume(
    COutputMix *pCOutputMix,
    XAmillibel *pMaxLevel)
{
    COutputMixData *pCOutputMixData = NULL;
    gdouble level;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData && pMaxLevel)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    level = gst_stream_volume_convert_volume(
        GST_STREAM_VOLUME_FORMAT_LINEAR,
        GST_STREAM_VOLUME_FORMAT_DB,
        1.0);
    *pMaxLevel = (XAmillibel)(level * 100);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendOutputMixSetMute(
    COutputMix *pCOutputMix,
    XAboolean mute)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    gst_stream_volume_set_mute(
        GST_STREAM_VOLUME(pCOutputMixData->pVolume),
        mute);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendOutputMixGetMute(
    COutputMix *pCOutputMix,
    XAboolean *pMute)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData && pMute)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    *pMute = gst_stream_volume_get_mute(
        GST_STREAM_VOLUME(pCOutputMixData->pVolume));

    XA_LEAVE_INTERFACE
}

/* Equalizer Implementation */

static XAresult
ALBackendEqualizerSetGain(
    COutputMixData *pCOutputMixData,
    XAuint16 index)
{
    GstObject *band;
    XAuint16 i;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMixData && pCOutputMixData->pEqualizer)

    for (i = 0; i< DEFAULT_EQUALIZER_BANDS; i++)
    {
        band = gst_child_proxy_get_child_by_index(
            GST_CHILD_PROXY(pCOutputMixData->pEqualizer), i);
        if (band)
        {
            g_object_set(G_OBJECT(band), "gain",
                (gdouble)equalizerPresets[index].bandValue[i], NULL);
            g_object_unref(G_OBJECT(band));
        }
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerSetEnabled(
    COutputMix *pCOutputMix,
    XAboolean enabled,
    XAuint16 index)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)

    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    if (enabled)
    {
        //Enable any preset settings
        if ((0 <= index) && (index < kNumEqPresets))
            result = ALBackendEqualizerSetGain(pCOutputMixData, index);
    }
    else
    {
        //Disable equalizer gain for all bands
        result = ALBackendEqualizerSetGain(pCOutputMixData, 0);
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetNumberOfBands(
    COutputMix *pCOutputMix,
    XAuint16 *pNumBands)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumBands)

    *pNumBands = DEFAULT_EQUALIZER_BANDS;

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetBandLevelRange(
    COutputMix *pCOutputMix,
    XAmillibel *pMin,
    XAmillibel *pMax)
{
    GParamSpec *pParamSpec;
    GParamSpecDouble *pDoubleParamSpec;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    //find gain for band0, it will be same for others
    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pCOutputMixData->pEqualizer), "band0");
    if (pParamSpec && G_IS_PARAM_SPEC_DOUBLE(pParamSpec))
    {
        pDoubleParamSpec = (GParamSpecDouble *)pParamSpec;
        if (pMin != NULL)
            *pMin = (XAmillibel)(pDoubleParamSpec->minimum * 100);

        if (pMax != NULL)
            *pMax = (XAmillibel)(pDoubleParamSpec->maximum * 100);
    }
    else
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerSetBandLevel(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmillibel level)
{
    GstObject *gstBand = NULL;
    GParamSpec *pParamSpec = NULL;
    GParamSpecDouble *pDoubleParamSpec = NULL;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    gstBand = gst_child_proxy_get_child_by_index(
        GST_CHILD_PROXY(pCOutputMixData->pEqualizer), band);
    if (gstBand)
    {
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(gstBand), "gain");
        if (pParamSpec && G_IS_PARAM_SPEC_DOUBLE(pParamSpec))
        {
            pDoubleParamSpec = (GParamSpecDouble *)pParamSpec;
            AL_CHK_ARG((level >= pDoubleParamSpec->minimum * 100)
                && (level <= pDoubleParamSpec->maximum * 100))
            g_object_set(G_OBJECT(gstBand), "gain",
                (gdouble)(level/100), NULL);
        }
        g_object_unref(G_OBJECT(gstBand));
    }
    else
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetBandLevel(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmillibel *pLevel)
{
    GstObject *gstBand = NULL;
    gdouble level;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData && pLevel)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    gstBand = gst_child_proxy_get_child_by_index(
        GST_CHILD_PROXY(pCOutputMixData->pEqualizer), band);
    if (gstBand)
    {
        g_object_get(G_OBJECT(gstBand), "gain", &level, NULL);
        *pLevel = (XAmillibel)(level * 100);
        g_object_unref(G_OBJECT(gstBand));
    }
    else
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetCenterFreq(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmilliHertz *pCenter)
{
    GstObject *gstBand = NULL;
    gdouble freq;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData && pCenter)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    gstBand = gst_child_proxy_get_child_by_index(
        GST_CHILD_PROXY(pCOutputMixData->pEqualizer), band);
    if (gstBand)
    {
        g_object_get(G_OBJECT(gstBand), "freq", &freq, NULL);
        *pCenter = (XAmilliHertz)(freq * 1000);
        g_object_unref(G_OBJECT(gstBand));
    }
    else
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetBandFreqRange(
    COutputMix *pCOutputMix,
    XAuint16 band,
    XAmilliHertz *pMin,
    XAmilliHertz *pMax)
{
    GstObject *gstBand = NULL;
    GParamSpec *pParamSpec = NULL;
    GValue bandwidth;
    gdouble freq = 0.0;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    gstBand = gst_child_proxy_get_child_by_index(
        GST_CHILD_PROXY(pCOutputMixData->pEqualizer), band);
    if (gstBand)
    {
        g_object_get(G_OBJECT(gstBand), "freq", &freq, NULL);

        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(gstBand), "bandwidth");
        if (pParamSpec)
        {
            bandwidth.g_type = G_TYPE_DOUBLE;
            g_object_get_property(G_OBJECT(gstBand), "bandwidth", &bandwidth);
            if (pMin != NULL)
                *pMin = (XAmilliHertz)((freq - (g_value_get_double(
                    &bandwidth) / 2)) * 1000);
            if (pMax != NULL)
                *pMax = (XAmilliHertz)((freq + (g_value_get_double(
                    &bandwidth) / 2)) * 1000);
        }
        else
        {
            result = XA_RESULT_INTERNAL_ERROR;
        }
        g_object_unref(G_OBJECT(gstBand));
    }
    else
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetBand(
    COutputMix *pCOutputMix,
    XAmilliHertz frequency,
    XAuint16 *pBand)
{
    GstObject *gstBand = NULL;
    GParamSpec *pParamSpec = NULL;
    GValue bandwidth;
    XAint16 index = 0;
    gdouble freq, minFreq, maxFreq;
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData && pBand)
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;

    for (index = 0; index < DEFAULT_EQUALIZER_BANDS; index++)
    {
        gstBand = gst_child_proxy_get_child_by_index(
            GST_CHILD_PROXY(pCOutputMixData->pEqualizer), index);
        if (gstBand)
        {
            g_object_get(G_OBJECT(gstBand), "freq", &freq, NULL);
            pParamSpec = g_object_class_find_property(
                G_OBJECT_GET_CLASS(gstBand), "bandwidth");
            if (pParamSpec)
            {
                bandwidth.g_type = G_TYPE_DOUBLE;
                g_object_get_property(G_OBJECT(gstBand),
                    "bandwidth", &bandwidth);
                minFreq = (freq - (g_value_get_double(&bandwidth) / 2)) * 1000;
                maxFreq = (freq + (g_value_get_double(&bandwidth) / 2)) * 1000;
                if ((frequency >= (XAmilliHertz)minFreq) &&
                    (frequency <= (XAmilliHertz)maxFreq))
                {
                    break;
                }
            }
            g_object_unref(G_OBJECT(gstBand));
        }
    }

    if (index == DEFAULT_EQUALIZER_BANDS)
        result = XA_RESULT_INTERNAL_ERROR;
    else
        *pBand = index;

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerUsePreset(
    COutputMix *pCOutputMix,
    XAuint16 index)
{
    COutputMixData *pCOutputMixData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCOutputMix && pCOutputMix->pData)
    AL_CHK_ARG((0 <= index) && (index < kNumEqPresets))
    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
    result = ALBackendEqualizerSetGain(pCOutputMixData, index);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetNumberOfPresets(
    COutputMix *pCOutputMix,
    XAuint16 *pNumPresets)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCOutputMix && pNumPresets)

    *pNumPresets = kNumEqPresets;

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendEqualizerGetPresetName(
    COutputMix *pCOutputMix,
    XAuint16 index,
    const XAchar **ppName)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(index < kNumEqPresets)
    AL_CHK_ARG(pCOutputMix && ppName)

    *ppName = (const XAchar *)equalizerPresets[index].presetName;

    XA_LEAVE_INTERFACE
}


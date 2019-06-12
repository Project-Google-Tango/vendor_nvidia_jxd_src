/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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

#include "linux_mediaplayer.h"

void
ALBackendMediaPlayerVideoTagsChangedCallback(
    GstElement *playbin2,
    gint streamid,
    gpointer data)
{
    GstTagList *VideoTags = NULL;
    gint CurrentStream = -1;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    gchar *str;
    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData))
        return;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(G_OBJECT(
        pXAMediaPlayer->mPlayer), "current-video", &CurrentStream, NULL);
    /* Only get the updated tags if it's for current stream id */
    if (CurrentStream != streamid)
    {
        XA_LOGE("\nMismatch Ids VideoTagsChangedCallback not updated\n");
        return;
    }
    g_signal_emit_by_name(
        G_OBJECT(pXAMediaPlayer->mPlayer), "get-video-tags", streamid, &VideoTags);
    str = gst_structure_to_string((GstStructure *)VideoTags);
    // XA_LOGD("\n\ng_signal_emit_by_name:: %s\n",str);
    if (VideoTags)
    {
        // consolidate and update tags. Issue gst_message_new_app, element_post_msg
        gst_tag_list_free(VideoTags);
    }
}

void
ALBackendMediaPlayerAudioTagsChangedCallback(
    GstElement *playbin2,
    gint streamid,
    gpointer data)
{
    GstTagList *AudioTags = NULL;
    gint CurrentStream = -1;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData))
        return;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "current-audio", &CurrentStream, NULL);
    /* Only get the updated tags if it's for our current stream id */
    if (CurrentStream != streamid)
    {
        XA_LOGE("\nError Mismatch ids MediaPlayerAudioTagsChangedCallback\n");
        return;
    }
    g_signal_emit_by_name(
        G_OBJECT(pXAMediaPlayer->mPlayer), "get-audio-tags", streamid, &AudioTags);
    if (AudioTags)
    {
        // consolidate and update tags. Issue gst_message_new_app, element_post_msg
        gst_tag_list_free(AudioTags);
    }
}

void
ALBackendMediaPlayerTextTagsChangedCallback(
    GstElement *playbin2,
    gint streamid,
    gpointer data)
{
    GstTagList *TextTags = NULL;
    gint CurrentStream = -1;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData))
        return;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "current-text", &CurrentStream, NULL);
    /* Only get the updated tags if it's for our current stream id */
    if (CurrentStream != streamid)
    {
        XA_LOGD("\nError Mismatch ids MediaPlayerTextTagsChangedCallback\n");
        return;
    }
    g_signal_emit_by_name(
        G_OBJECT(pXAMediaPlayer->mPlayer), "get-text-tags", streamid, &TextTags);
    if (TextTags)
    {
        // consolidate and update tags. Issue gst_message_new_app, element_post_msg
        gst_tag_list_free(TextTags);
    }
}

void
ALBackendMediaPlayerAudioChangedCallback(
    GstElement *playbin2,
    gpointer data)
{
    gint CurrentStream = -1;
    gint Tracks;
    GstMessage *msg;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    StreamInfoData *pAlStreamInfo = NULL;
    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData &&
        pCMediaPlayer->mStreamInfo.pData))
    {
        XA_LOGE("\nError %s %d\n", __FUNCTION__, __LINE__);
        return;
    }
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pAlStreamInfo =
        (StreamInfoData *)pCMediaPlayer->mStreamInfo.pData;
    g_object_get(G_OBJECT(pXAMediaPlayer->mPlayer), "n-audio", &Tracks, NULL);
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "current-audio", &CurrentStream, NULL);
    if (Tracks > 0)
    {
        pAlStreamInfo->AudioStreamChangedCBNotify = XA_BOOLEAN_TRUE;
        // This being called from the streaming thread, so don't do anything here
        msg = gst_message_new_application(GST_OBJECT(pXAMediaPlayer->mPlayer),
            gst_structure_new("audio-stream-changed", NULL));
        gst_element_post_message(pXAMediaPlayer->mPlayer, msg);
    }
}

void
ALBackendMediaPlayerVideoChangedCallback(
    GstElement *playbin2,
    gpointer data)
{
    gint CurrentStream = -1;
    gint Tracks;
    GstMessage *msg;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    StreamInfoData *pAlStreamInfo = NULL;
    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData &&
        pCMediaPlayer->mStreamInfo.pData))
    {
        XA_LOGE("\nError %s %d\n", __FUNCTION__, __LINE__);
        return;
    }
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pAlStreamInfo =
        (StreamInfoData *)pCMediaPlayer->mStreamInfo.pData;
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "n-video", &Tracks, NULL);
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "current-video", &CurrentStream, NULL);
    XA_LOGD("\nvideo-stream-changed: id %d: n-video = %d\n",
        CurrentStream, Tracks);
    if (Tracks > 0)
    {
        pAlStreamInfo->VideoStreamChangedCBNotify = XA_BOOLEAN_TRUE;
        // This being called from the streaming thread, so don't do anything here
        msg = gst_message_new_application(GST_OBJECT(pXAMediaPlayer->mPlayer),
        gst_structure_new("video-stream-changed", NULL));
        gst_element_post_message(pXAMediaPlayer->mPlayer, msg);
    }
}

void
ALBackendMediaPlayerTextChangedCallback(
    GstElement *playbin2,
    gpointer data)
{
    GstMessage *msg;
    gint CurrentStream = -1;
    gint Tracks;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData))
        return;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "n-text", &Tracks, NULL);
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "current-text", &CurrentStream, NULL);
    if (Tracks > 0)
    {
        // We're being called from the streaming thread, so don't do anything here
        msg = gst_message_new_application(GST_OBJECT(pXAMediaPlayer->mPlayer),
            gst_structure_new("text-stream-changed", NULL));
        gst_element_post_message(pXAMediaPlayer->mPlayer, msg);
    }
}

static XAresult
ALBackendMediaPlayerMapCodecTagInfoString(
    GstTagList *Tags,
    StreamInfoData *ParamStreamInfo,
    gchar *GstCodectag)
{
    gchar *StringName = NULL;
    gint size = 0;
    gchar *st = NULL;
    gint j;
    const gchar *GstTag = GstCodectag;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(Tags && ParamStreamInfo && GstCodectag);
    if ((size = gst_tag_list_get_tag_size(Tags, GstTag)) > 0)
    {
        for (j = 0; j < size; ++j)
        {
            if (gst_tag_list_get_string_index(Tags, GstTag, j, &st) && st)
            {
                if (StringName)
                {
                    StringName = g_strconcat(StringName, st, NULL);
                }
                else
                {
                    StringName = g_strdup(st);
                }
                g_free(st);
            }
        }
    }
    if (StringName)
    {
        XA_LOGD("\ncodec_name: %s\n", StringName);
        if (!strcmp(StringName, "MPEG-4 AAC audio") ||
            !strcmp(StringName, "AAC"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_AAC;
            XA_LOGD("\nAL Codec Id: XA_AUDIOCODEC_AAC\n");
        }
        else if (!strcmp(StringName, "MPEG-1 Layer 3") ||
            !strcmp(StringName, "MPEG 1 Audio, Layer 3 (MP3)") ||
            !strcmp(StringName, "MPEG-1 Audio, Layer 2 (MP2)") ||
            !strcmp(StringName, "MPEG 2 Audio, Layer 3 (MP3)"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_MP3;
            XA_LOGD("\nAL Codec Id : XA_AUDIOCODEC_MP3\n");
        }
        else if (!strcmp(StringName, "WMA Version 9") ||
            !strcmp(StringName, "WMA Version 8"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_WMA;
            XA_LOGD("\nAL Codec Id: XA_AUDIOCODEC_WMA\n");
        }
        else if (!strcmp(StringName, "AMR audio") ||
            !strcmp(StringName, "AMR narrow band"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_AMR;
            XA_LOGD("\nAL Codec Id : XA_AUDIOCODEC_AMR\n");
        }
        else if (!strcmp(StringName, "AMRWB audio") ||
            !strcmp(StringName, "AMR wide band"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_AMRWB;
            XA_LOGD("\nAL Codec Id : XA_AUDIOCODEC_AMRWB\n");
        }
        else if (!strcmp(StringName, "OGG audio") ||
            !strcmp(StringName, "VORBIS audio") ||
            !strcmp(StringName, "Vorbis"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_VORBIS;
            XA_LOGD("\nAL Codec Id : XA_AUDIOCODEC_VORBIS\n");
        }
        else if (!strcmp(StringName, "A-law audio") ||
            !strcmp(StringName, "Uncompressed 16-bit PCM audio"))
        {
            ParamStreamInfo->AudioStreamInfo.codecId = XA_AUDIOCODEC_PCM;
            XA_LOGD("\nAL Codec Id: XA_AUDIOCODEC_PCM\n");
        }
        else if (!strcmp(StringName, "MPEG-4 video")
            || !strcmp(StringName, "XVID MPEG-4")
            || !strcmp(StringName, "DivX MPEG-4 Version 5")
            || !strcmp(StringName, "DivX MPEG-4 Version 4")
            || !strcmp(StringName, "DivX MPEG-4 Version 3")
            || !strcmp(StringName, "MPEG-4 advanced simple profile"))
        {
            ParamStreamInfo->VideoStreamInfo.codecId = XA_VIDEOCODEC_MPEG4;
            XA_LOGD("\nAL Codec Id: XA_VIDEOCODEC_MPEG4\n");
        }
        else if (!strcmp(StringName, "H.264 / AVC")
            || !strcmp(StringName, "H.264")
            || !strcmp(StringName, "AVC")
            || !strcmp(StringName, "X.264 / AVC"))
        {
            ParamStreamInfo->VideoStreamInfo.codecId = XA_VIDEOCODEC_AVC;
            XA_LOGD("\nAL Codec Id: XA_VIDEOCODEC_AVC\n");
        }
        else if (!strcmp(StringName, "H.263"))
        {
            ParamStreamInfo->VideoStreamInfo.codecId = XA_VIDEOCODEC_H263;
            XA_LOGD("\nAL Codec Id: XA_VIDEOCODEC_H263\n");
        }
        else if (!strcmp(StringName, "Microsoft Windows Media 9") ||
            !strcmp(StringName, "Microsoft Windows Media VC-1"))
        {
            ParamStreamInfo->VideoStreamInfo.codecId = XA_VIDEOCODEC_VC1;
            XA_LOGD("\nAL Codec Id: XA_VIDEOCODEC_VC1\n");
        }
        else if (!strcmp(GstCodectag, "language-code"))
        {
            XA_LOGD("\nlanguage_name %s : length %d\n",
                StringName, strlen(StringName));
            size = strlen(StringName);
            if (size > 15)
                size = 15;
            if (*StringName != 0 && strlen(StringName) != 0)
            {
                memcpy(&ParamStreamInfo->AudioStreamInfo.langCountry[0],
                    StringName, size + 1);
                XA_LOGD("\nParamStreamInfo->AudioStreamInfo.langCountry %s\n",
                    ParamStreamInfo->AudioStreamInfo.langCountry);
            }
        }
        else if (!strcmp(GstCodectag, "location"))
        {
            XA_LOGD("\nlocation Uri %s : length %d\n", StringName, strlen(StringName));
            if (*StringName != 0 && strlen(StringName) != 0)
            {
                memcpy(
                    &ParamStreamInfo->pUri, StringName, strlen(StringName) + 1);
                ParamStreamInfo->UriSize = strlen(StringName);
                XA_LOGD("\nParamStreamInfo->pUri  %s\n", ParamStreamInfo->pUri);
            }
        }
    }
    XA_LEAVE_INTERFACE
}

static XAresult
ALBackendMediaPlayerContainerType(
    GstTagList *Tags,
    StreamInfoData *pPlayerStreamInfo)
{
    gchar *ContainerName = NULL;
    gint size = 0;
    gchar *st = NULL;
    gint i;
    XA_ENTER_INTERFACE

    if ((size = gst_tag_list_get_tag_size(Tags, GST_TAG_CONTAINER_FORMAT)) > 0)
    {
        for (i = 0; i < size; ++i)
        {
            if (gst_tag_list_get_string_index(
                Tags, GST_TAG_CONTAINER_FORMAT, i, &st) && st)
            {
                if (ContainerName)
                {
                    ContainerName = g_strconcat(ContainerName, st, NULL);
                }
                else
                {
                    ContainerName = g_strdup(st);
                }
                g_free(st);
            }
        }
    }
    XA_LOGD ("\n\nfile ContainerName  %s \n", ContainerName);

    pPlayerStreamInfo->ContainerInfo.containerType =
        XA_CONTAINERTYPE_UNSPECIFIED;
    if (!strcmp(ContainerName, "MP3"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_MP3;
    }
    else if (!strcmp(ContainerName, "ISO MP4/M4A"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_MP4;
    }
    else if (!strcmp(ContainerName, "3GP") || !strcmp(ContainerName, "3GPP"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_3GPP;
    }
    else if (!strcmp(ContainerName, "3GA"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_3GA;
    }
    else if (!strcmp(ContainerName, "ISO M4A") ||
        !strcmp(ContainerName, "ISO M4B"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_M4A;
    }
    else if (!strcmp(ContainerName, "Quicktime") ||
        !strcmp(ContainerName, "mov"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_QT;
    }
    else if (!strcmp(ContainerName, "ASF") || !strcmp(ContainerName, "WMV") ||
        !strcmp(ContainerName, "WMA"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_ASF;
    }
    else if (!strcmp(ContainerName, "AMR") || !strcmp(ContainerName, "AWB"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_AMR;
    }
    else if (!strcmp(ContainerName, "AVI") || !strcmp(ContainerName, "DIVX"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_AVI;
    }
    else if (!strcmp(ContainerName, "AAC"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_AAC;
    }
    else if (!strcmp(ContainerName, "Ogg") || !strcmp(ContainerName, "Oga"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_OGG;
    }
    else if (!strcmp(ContainerName, "WAV"))
    {
        pPlayerStreamInfo->ContainerInfo.containerType = XA_CONTAINERTYPE_WAV;
    }
    else
    {
        pPlayerStreamInfo->ContainerInfo.containerType =
            XA_CONTAINERTYPE_UNSPECIFIED;
    }
    XA_LEAVE_INTERFACE
}

static void
ALBackendMediaPlayerGstAudioTagsInfo(
    GstTagList *tagsList,
    StreamInfoData *ParamStreamInfo)
{
    gchar *Language = NULL;
    gchar *AudioCodec = NULL;
    guint Bitrate = 0;
    gchar *ContanerFormat;
    gchar *Location = NULL;
    guint TrackNum;
    guint TrackCount = 0;
    guint64 TagDuration = 0;
    gchar *Codec = NULL;
    GstTagList *tags = tagsList;

    if (ParamStreamInfo == NULL || tagsList == NULL)
        return;
    if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &AudioCodec))
    {
        ALBackendMediaPlayerMapCodecTagInfoString(
            tags,
            ParamStreamInfo,
            (gchar *)GST_TAG_AUDIO_CODEC);
        g_free(AudioCodec);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &Language))
    {
        ALBackendMediaPlayerMapCodecTagInfoString(
            tags,
            ParamStreamInfo,
            (gchar *)GST_TAG_LANGUAGE_CODE);
        g_free(Language);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_LOCATION, &Location))
    {
        ALBackendMediaPlayerMapCodecTagInfoString(
            tags,
            ParamStreamInfo,
            (gchar *)GST_TAG_LOCATION);
        g_free(Location);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &ContanerFormat))
    {
        XA_LOGD ("\nGST_TAG_CONTAINER_FORMAT %s\n", ContanerFormat);
        ALBackendMediaPlayerContainerType(
            tags,
            ParamStreamInfo);
        g_free(ContanerFormat);
    }
    if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &Bitrate))
    {
        XA_LOGD("\nexact or average bitrate audio: %d bits/s\n", Bitrate);
        ParamStreamInfo->AudioStreamInfo.bitRate = (XAuint32)Bitrate;
    }
    if (gst_tag_list_get_uint(tags, GST_TAG_TRACK_NUMBER, &TrackNum))
    {
        XA_LOGD("\nGST_TAG_AUDIO :tracknumber %d\n", TrackNum);
    }
    if (gst_tag_list_get_uint(tags, GST_TAG_TRACK_COUNT, &TrackCount))
    {
        XA_LOGD("\nGST_TAG_TRACK_COUNT %d\n", TrackCount);
    }
    if (gst_tag_list_get_uint64(tags, GST_TAG_DURATION, &TagDuration))
    {
        XA_LOGD("\nGST_TAG_DURATION %llu\n", TagDuration/GST_MSECOND);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_CODEC, &Codec))
    {
        XA_LOGD("\nGST_TAG_CODEC %s\n", Codec);
        g_free(Codec);
    }
}

static void
ALBackendMediaPlayerGstVideoTagsInfo(
    GstTagList *tagsList,
    StreamInfoData *ParamStreamInfo)
{
    gchar *VideoCodec = NULL;
    gchar *Codec = NULL;
    guint Bitrate = 0;
    guint TrackNum;
    guint TrackCount = 0;
    guint64 TagDuration = 0;
    gchar* ContanerFormat;
    GstTagList *tags = tagsList;
    if (ParamStreamInfo == NULL  || tagsList == NULL)
        return;
    if (gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &VideoCodec))
    {
        XA_LOGD("\nGST_TAG_VIDEO_CODEC %s\n", VideoCodec);
        ALBackendMediaPlayerMapCodecTagInfoString(
            tags,
            ParamStreamInfo,
            (gchar *)GST_TAG_VIDEO_CODEC);
        g_free(VideoCodec);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &ContanerFormat))
    {
        XA_LOGD("\nGST_TAG_CONTAINER_FORMAT %s\n", ContanerFormat);
        ALBackendMediaPlayerContainerType(
            tags,
            ParamStreamInfo);
        g_free(ContanerFormat);
    }
    if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &Bitrate))
    {
        XA_LOGD("\nexact or average bitrate video: %d bits/s\n", Bitrate);
        ParamStreamInfo->VideoStreamInfo.bitRate =  (XAuint32)Bitrate;
    }
    if (gst_tag_list_get_uint(tags, GST_TAG_TRACK_NUMBER, &TrackNum))
    {
        XA_LOGD("\nGST_TAG_AUDIO :tracknumber %d\n", TrackNum);
    }
    if (gst_tag_list_get_uint(tags, GST_TAG_TRACK_COUNT, &TrackCount))
    {
        XA_LOGD("\nGST_TAG_TRACK_COUNT %d\n", TrackCount);
    }
    if (gst_tag_list_get_uint64(tags, GST_TAG_DURATION, &TagDuration))
    {
        XA_LOGD("\nGST_TAG_DURATION %llu\n", TagDuration/GST_MSECOND);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_CODEC, &Codec))
    {
        XA_LOGD("\nGST_TAG_CODEC %s\n", Codec);
        g_free(Codec);
    }
}

static void
ALBackendMediaPlayerGstPadAudioCapsInfo(
    GObject * obj,
    GParamSpec * pspec,
    CMediaPlayer *pCMediaPlayer)
{
    StreamInfoData *pParamStreamInfo = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    GstStructure *capstr = NULL;
    GstCaps *caps = NULL;
    GstQuery *Query = NULL;
    gint AudioId = -1;
    gint Channel = 0;
    gint SampleRate = 0;
    gint64 Time = 0;
    XAint32 AudioIndex;
    const  XAStreamInformationItf * const* IStreamInformationItf;
    GstPad *pad = GST_PAD(obj);

    if (pad == NULL || pCMediaPlayer == NULL ||
        pCMediaPlayer->pData == NULL ||
        pCMediaPlayer->mStreamInfo.pData == NULL)
    {
        return;
    }
    pParamStreamInfo =
      (StreamInfoData *)pCMediaPlayer->mStreamInfo.pData;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    if (!(caps = gst_pad_get_negotiated_caps(pad)))
    {
        return;
    }
    /* Get Audio or video decoder caps */
    capstr = gst_caps_get_structure(caps, 0);
    if (capstr)
    {
        g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "current-audio", &AudioId, NULL);
        AudioIndex = (AudioId >= 0) ? AudioId : 0;
        gst_structure_get_int(capstr, "channels", &Channel);
        gst_structure_get_int(capstr, "rate", &SampleRate);
        Query = gst_query_new_duration(GST_FORMAT_TIME);
        if (gst_pad_peer_query(pad, Query))
        {
            gst_query_parse_duration(Query, NULL, &Time);
            pParamStreamInfo->AudioStreamInfo.duration =
                   (XAmillisecond)Time/GST_MSECOND;
            gst_query_unref(Query);
        }
        pParamStreamInfo->AudioStreamInfo.channels = Channel;
        pParamStreamInfo->AudioStreamInfo.sampleRate = SampleRate;
        XA_LOGD("\nAUDIO CAPS::ch = %d - sr = %d - duration = %d\n",
            pParamStreamInfo->AudioStreamInfo.channels,
            pParamStreamInfo->AudioStreamInfo.sampleRate,
            pParamStreamInfo->AudioStreamInfo.duration);
        gst_caps_unref (caps);
        XA_LOGD("\nAudioStreamChangedCBNotify = %d\n",
            pParamStreamInfo->AudioStreamChangedCBNotify);
        if (pParamStreamInfo->AudioStreamChangedCBNotify == XA_BOOLEAN_TRUE)
        {
            XA_LOGD("\nSTART STREAMCBEVENT_PROPERTYCHANGE\n");
            if (pParamStreamInfo == NULL || pParamStreamInfo->mCallback == NULL)
            {
                XA_LOGD("\nRETURN _NO_ STREAM CB REGISTERED\n");
                return;
            }
            IStreamInformationItf =
                (const XAStreamInformationItf * const*)
                    pCMediaPlayer->mStreamInfo.mItf;
            pParamStreamInfo->DomainType[AudioIndex+1] =
                XA_DOMAINTYPE_AUDIO;
            pParamStreamInfo->AudioStreamChangedCBNotify = XA_BOOLEAN_FALSE;
            pParamStreamInfo->mCallback(
                (XAStreamInformationItf)IStreamInformationItf,
                XA_STREAMCBEVENT_PROPERTYCHANGE, /*call back eventId*/
                AudioIndex+1,     /*streamindex 0 indicates container type*/
                NULL, /*pEventData, always NULL in OpenMAX AL 1.0.1*/
                pParamStreamInfo->mContext);
        }
    }
}

static void
ALBackendMediaPlayerGstPadVideoCapsInfo(
    GObject * obj,
    GParamSpec * pspec,
    CMediaPlayer *pCMediaPlayer)
{
    StreamInfoData *pParamStreamInfo = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    GstStructure *capstr = NULL;
    GstCaps *caps = NULL;
    GstQuery *Query = NULL;
    gint VideoId = -1;
    gint Width = 0;
    gint Height = 0;
    gint FpsNum;
    gint FpsDen;
    gint64 Time = 0;
    const GValue *FrameRate;
    XAint32 VideoIndex;
    const  XAStreamInformationItf * const* IStreamInformationItf;
    GstPad *pad = GST_PAD(obj);

    if (pad == NULL || pCMediaPlayer == NULL ||
        pCMediaPlayer->pData == NULL ||
        pCMediaPlayer->mStreamInfo.pData == NULL)
    {
        return;
    }
    pParamStreamInfo =
      (StreamInfoData *)pCMediaPlayer->mStreamInfo.pData;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    if (!(caps = gst_pad_get_negotiated_caps(pad)))
    {
        return;
    }
    /* Get Audio or video decoder caps */
    capstr = gst_caps_get_structure (caps, 0);
    if (capstr)
    {
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "current-video", &VideoId, NULL);
        VideoIndex = (VideoId >= 0) ? VideoId : 0;
        /* We need at least width/height and FrameRate */
        gst_structure_get_int(capstr, "width", &Width);
        gst_structure_get_int(capstr, "height", &Height);
        pParamStreamInfo->VideoStreamInfo.width = Width;
        pParamStreamInfo->VideoStreamInfo.height = Height;
        /* Get the frame rate if available */
        FrameRate = gst_structure_get_value(capstr, "framerate");
        if (FrameRate)
        {
            FpsNum = gst_value_get_fraction_numerator(FrameRate);
            FpsDen = gst_value_get_fraction_denominator(FrameRate);
            XA_LOGD("\nfps_num = %d - FpsDen = %d\n", FpsNum, FpsDen);
            if (FpsDen)
            {
                gdouble framerate;
                gst_util_fraction_to_double(FpsNum, FpsDen, &framerate);
                pParamStreamInfo->VideoStreamInfo.frameRate =
                    (XAuint32)((framerate * (1 << 16)) + 0.5);
            }
        }
        Query = gst_query_new_duration(GST_FORMAT_TIME);
        if (gst_pad_peer_query(pad, Query))
        {
            gst_query_parse_duration(Query, NULL, &Time);
            pParamStreamInfo->VideoStreamInfo.duration =
                (XAmillisecond)Time/GST_MSECOND;
            gst_query_unref(Query);
        }
        XA_LOGD("\nVIDEO CAPS::h = %d - w = %d - fr = %d dur= %d\n",
            pParamStreamInfo->VideoStreamInfo.height,
            pParamStreamInfo->VideoStreamInfo.width,
            pParamStreamInfo->VideoStreamInfo.frameRate>>16,
            pParamStreamInfo->VideoStreamInfo.duration);
        gst_caps_unref(caps);

        XA_LOGD("\nVideoStreamChangedCBNotify = %d\n",
            pParamStreamInfo->VideoStreamChangedCBNotify);
        if (pParamStreamInfo->VideoStreamChangedCBNotify == XA_BOOLEAN_TRUE)
        {
            if (pParamStreamInfo == NULL || pParamStreamInfo->mCallback == NULL)
            {
                return;
            }
            IStreamInformationItf =
                (const XAStreamInformationItf * const*)
                    pCMediaPlayer->mStreamInfo.mItf;
            pParamStreamInfo->DomainType[VideoIndex+1] =
                XA_DOMAINTYPE_VIDEO;
            pParamStreamInfo->VideoStreamChangedCBNotify = XA_BOOLEAN_FALSE;
            pParamStreamInfo->mCallback(
                (XAStreamInformationItf)IStreamInformationItf,
                XA_STREAMCBEVENT_PROPERTYCHANGE, /*call back eventId*/
                VideoIndex+1,     /*streamindex 0 indicates container type*/
                NULL, /*pEventData, always NULL in OpenMAX AL 1.0.1*/
                pParamStreamInfo->mContext);
        }
    }
}

XAresult
ALBackendMediaPlayerGetAudioStreamInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pStreamInfo)
{
    XAint32 i;
    GstPad *AudioPad = NULL;
    XAint32 Naudio = 0;
    GstCaps *caps = NULL;
    GstTagList *tags = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pStreamInfo && pCMediaPlayer &&
        pCMediaPlayer->pData);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(G_OBJECT(pXAMediaPlayer->mPlayer), "n-audio", &Naudio, NULL);
    if (Naudio > 0)
    {
        for (i = 0; i < Naudio &&AudioPad == NULL; i++)
        {
            g_signal_emit_by_name(
                G_OBJECT(pXAMediaPlayer->mPlayer), "get-audio-pad", i, &AudioPad);
            if (AudioPad)
            {
                if ((caps = gst_pad_get_negotiated_caps(AudioPad)))
                {
                    ALBackendMediaPlayerGstPadAudioCapsInfo(
                        G_OBJECT(AudioPad), NULL, pCMediaPlayer);
                    gst_caps_unref(caps);
                }
                g_signal_connect(
                    AudioPad, "notify::caps",
                    G_CALLBACK(ALBackendMediaPlayerGstPadAudioCapsInfo),
                    pCMediaPlayer);
                gst_object_unref(AudioPad);
            }
        }
    }
    if (Naudio > 0)
    {
        for (i = 0; i < Naudio &&tags == NULL; i++)
        {
            g_signal_emit_by_name(
                G_OBJECT(pXAMediaPlayer->mPlayer), "get-audio-tags", i, &tags);
            if (tags && gst_is_tag_list(tags) )
            {
               ALBackendMediaPlayerGstAudioTagsInfo(tags, pStreamInfo);
               gst_tag_list_free(tags);
            }
        }
    }
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetVideoStreamInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pStreamInfo)
{
    XAint32 i;
    GstPad *VideoPad = NULL;
    XAint32 Nvideo = 0;
    GstCaps *caps = NULL;
    GstTagList *tags = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pStreamInfo && pCMediaPlayer &&
        pCMediaPlayer->pData);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "n-video", &Nvideo, NULL);
    if (Nvideo > 0)
    {
        for (i = 0; i < Nvideo && VideoPad == NULL; i++)
        {
            g_signal_emit_by_name(
                G_OBJECT(pXAMediaPlayer->mPlayer), "get-video-pad", i, &VideoPad);
            if (Nvideo)
            {
                if ((caps = gst_pad_get_negotiated_caps(VideoPad)))
                {
                    ALBackendMediaPlayerGstPadVideoCapsInfo(
                        G_OBJECT(VideoPad), NULL, pCMediaPlayer);
                    gst_caps_unref(caps);
                }
                g_signal_connect(
                    VideoPad, "notify::caps",
                    G_CALLBACK(ALBackendMediaPlayerGstPadVideoCapsInfo),
                    pCMediaPlayer);
                gst_object_unref(VideoPad);
            }
        }
        for (i = 0; i < Nvideo && tags == NULL; i++)
        {
            g_signal_emit_by_name(
                G_OBJECT(pXAMediaPlayer->mPlayer), "get-video-tags", i, &tags);
            if (tags && gst_is_tag_list(tags) )
            {
                ALBackendMediaPlayerGstVideoTagsInfo(tags, pStreamInfo);
                gst_tag_list_free(tags);
            }
        }
    }
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetTextStreamInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pStreamInfo)
{
    XAint32 i;
    XAint32 Ntext = 0;
    GstTagList *tags = NULL;
    gchar *Language;
    XAint32 CurrentStream = -1;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pStreamInfo && pCMediaPlayer &&
        pCMediaPlayer->pData);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    g_object_get(G_OBJECT(pXAMediaPlayer->mPlayer), "n-text", &Ntext, NULL);
    for (i=0; i< Ntext; i++)
    {
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "current-text", &CurrentStream, NULL);
        g_signal_emit_by_name(
            G_OBJECT(pXAMediaPlayer->mPlayer), "get-text-tags", i, &tags);
        Language = g_strdup_printf("und");
        if (tags && gst_is_tag_list(tags))
            gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &Language);
        XA_LOGD("Text stream=%i language=%s", i, Language);
        if (Language)
            g_free(Language);
        if (tags)
            gst_tag_list_free(tags);
    }
    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerQueryActiveStreams(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo)
{
    gint i;
    gint Nvideo = 0;
    gint Naudio = 0;
    gint Ntext = 0;
    gint Stream = -1;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData &&
        pParamStreamInfo);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    if (pXAMediaPlayer->mPlayer)
    {
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "n-video", &Nvideo, NULL);
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "n-audio", &Naudio, NULL);
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "n-text", &Ntext, NULL);
    }
    XA_LOGD("\nTotal streams::Nvideo = %d: Naudio = %d Ntext = %d\n",
        Nvideo, Naudio, Ntext);
    pParamStreamInfo->ContainerInfo.numStreams = Nvideo + Naudio + Ntext;
    pParamStreamInfo->DomainType[0] = XA_DOMAINTYPE_CONTAINER;
    for (i=0; i < Naudio; i++)
    {
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "current-audio", &Stream, NULL);
        if (Stream < MAX_SUPPORTED_STREAMS &&
            Naudio < MAX_SUPPORTED_STREAMS)
        {
            pParamStreamInfo->DomainType[i+1] = XA_DOMAINTYPE_AUDIO;
            pParamStreamInfo->mActiveStreams[Stream] = XA_BOOLEAN_TRUE;
        }
    }
    for (i=0; i < Nvideo; i++)
    {
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "current-video", &Stream, NULL);
        if ((Stream+Naudio) < MAX_SUPPORTED_STREAMS &&
            (Naudio + Nvideo) < MAX_SUPPORTED_STREAMS)
        {
            pParamStreamInfo->DomainType[i+Naudio+1] =
                XA_DOMAINTYPE_VIDEO;
            pParamStreamInfo->mActiveStreams[Stream+Naudio] =
                XA_BOOLEAN_TRUE;
        }
    }
    for (i=0; i < Ntext; i++)
    {
        g_object_get(
            G_OBJECT(pXAMediaPlayer->mPlayer), "current-text", &Stream, NULL);
        if ((Stream + Naudio + Nvideo) < MAX_SUPPORTED_STREAMS &&
            (Naudio + Nvideo + Ntext) < MAX_SUPPORTED_STREAMS)
        {
            pParamStreamInfo->DomainType[i+Naudio+Nvideo+1] =
                XA_DOMAINTYPE_TIMEDTEXT;
            pParamStreamInfo->mActiveStreams[Stream+Naudio+Nvideo] =
                XA_BOOLEAN_TRUE;
        }
    }
    XA_LEAVE_INTERFACE
}

static XAresult
ALBackendMediaPlayerGetContainerInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo)
{
    XAmillisecond duration = XA_TIME_UNKNOWN;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pParamStreamInfo && pCMediaPlayer
        && pCMediaPlayer->pData);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    result = ALBackendMediaPlayerGetDuration(pCMediaPlayer,&duration);
    if (result != XA_RESULT_SUCCESS)
    {
        XA_LOGD("Error %x @ %s %d", result, __FUNCTION__, __LINE__);
        XA_LEAVE_INTERFACE
    }
    pParamStreamInfo->ContainerInfo.mediaDuration = (XAmillisecond)duration;
    XA_LOGD("\nALToGSTQueryContainerInfo:mediaDuration= %d\n",
        pParamStreamInfo->ContainerInfo.mediaDuration);
    result = ALBackendMediaPlayerQueryActiveStreams(
        pCMediaPlayer,
        pParamStreamInfo);
    if (result != XA_RESULT_SUCCESS)
    {
        XA_LOGD("Error %x @ %s %d", result, __FUNCTION__, __LINE__);
        XA_LEAVE_INTERFACE
    }
    g_object_get(pXAMediaPlayer->mPlayer, "uri", &pParamStreamInfo->pUri, NULL);
    pParamStreamInfo->UriSize = strlen((char *)pParamStreamInfo->pUri);
    XA_LOGD("\nParamStreamInfo->UriSize = %d - pPlayerStreamInfo->pUri  %s\n",
        pParamStreamInfo->UriSize, pParamStreamInfo->pUri);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerQueryContainerInfo(
    CMediaPlayer *pCMediaPlayer,
    StreamInfoData *pParamStreamInfo)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XAint32 Nvideo = 0;
    XAint32 Naudio = 0;
    XAint32 Ntext = 0;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pParamStreamInfo && pCMediaPlayer
        && pCMediaPlayer->pData);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    result = ALBackendMediaPlayerGetContainerInfo(
        pCMediaPlayer,
        pParamStreamInfo);
    if (result != XA_RESULT_SUCCESS)
    {
        XA_LOGD("Error %x @ %s %d", result, __FUNCTION__, __LINE__);
        XA_LEAVE_INTERFACE
    }
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "n-audio", &Naudio, NULL);
    if (Naudio > 0)
    {
        result = ALBackendMediaPlayerGetAudioStreamInfo(
            pCMediaPlayer,
            pParamStreamInfo);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGD("Error %x @ %s %d", result, __FUNCTION__, __LINE__);
            XA_LEAVE_INTERFACE
        }
    }
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "n-video", &Nvideo, NULL);
    if (Nvideo > 0)
    {
        result = ALBackendMediaPlayerGetVideoStreamInfo(
            pCMediaPlayer,
            pParamStreamInfo);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGD("Error %x @ %s %d", result, __FUNCTION__, __LINE__);
            XA_LEAVE_INTERFACE
        }
    }
    g_object_get(
        G_OBJECT(pXAMediaPlayer->mPlayer), "n-text", &Ntext, NULL);
    if (Ntext > 0)
    {
        result = ALBackendMediaPlayerGetTextStreamInfo(
            pCMediaPlayer,
            pParamStreamInfo);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGD("Error %x @ %s %d", result, __FUNCTION__, __LINE__);
            XA_LEAVE_INTERFACE
        }
    }
    pParamStreamInfo->IsStreamInfoProbed = XA_BOOLEAN_TRUE;
    XA_LEAVE_INTERFACE
}


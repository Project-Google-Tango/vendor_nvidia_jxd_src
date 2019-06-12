/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
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
#include "linux_cameradevice.h"
#include "linux_outputmix.h"
#include <unistd.h>

#define MAX_PLAYER_DATATAPPOINTS 2
#define PLAYER_AUDIO_DATATAP_INDEX 0
#define PLAYER_VIDEO_DATATAP_INDEX 1

#define DUMP_MP_PIPELINE_GRAPH 1



static XAresult TimedCallBackThread(void *arg)
{

    CMediaPlayer *pCMediaPlayer = (CMediaPlayer *)arg;
    GstElement *pPlayerGraph = NULL;
    GstState state;
    GstState pending;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XAPlayItf IPlayItf;
    PlayerData *pPlayerData;
    PlayRateData *pPlayRate;
    gdouble rate;
    XAmillisecond PosMsec = 0;
    XAmillisecond delta ;
    XAmillisecond periodic ;
    XAmillisecond updatePeriod;
    XAint32 waitTime = 0;
    XAint32 sleepTime = 0;
    XAmillisecond markerPos;
    GTimeVal ts;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaPlayer);
    IPlayItf = (XAPlayItf)pCMediaPlayer->mPlay.mItf;
    pPlayerData = (PlayerData *)pCMediaPlayer->mPlay.pData;
    pPlayRate = (PlayRateData *)pCMediaPlayer->mPlaybackRate.pData;
    AL_CHK_ARG(pPlayerData && pPlayRate);
    rate = (gdouble)pPlayRate->CurrentRate / 1000;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    delta = 40 + (4 * rate);
    periodic = pPlayerData->mPositionUpdatePeriod;
    updatePeriod = periodic;
    markerPos = pPlayerData->mMarkerPosition;

    while (!pPlayerData->stopThread)
    {
        result = ALBackendMediaPlayerGetPosition(pCMediaPlayer, &PosMsec);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGE("\n%s[%d] GetPosition failed", __FUNCTION__, __LINE__);
        }
        if ((periodic - delta) <= (XAmillisecond)PosMsec &&
            (periodic + delta) >= (XAmillisecond)PosMsec)
        {
            sleepTime = (XAint32)(periodic - PosMsec ) / rate;
            if (sleepTime > 0)
                usleep(sleepTime * 1000);
            if ((pPlayerData->mEventFlags & XA_PLAYEVENT_HEADATNEWPOS) &&
                pPlayerData->mCallback)
            {
                pPlayerData->mCallback(
                    IPlayItf, pPlayerData->mContext, XA_PLAYEVENT_HEADATNEWPOS);
            }
            periodic += updatePeriod;
        }

        if (markerPos != XA_TIME_UNKNOWN &&
            ((markerPos - delta) <= PosMsec && (markerPos + delta) >= PosMsec))
        {
            result = ALBackendMediaPlayerGetPosition(pCMediaPlayer, &PosMsec);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LOGE("\n%s[%d] GetPosition failed", __FUNCTION__, __LINE__);
            }

            sleepTime = (XAint32)(markerPos - PosMsec)/rate;
            XA_LOGD("\nMarker position is hit sleepTime:%d", sleepTime);
            if (sleepTime > 0)
                usleep(sleepTime * 1000);

            if ((pPlayerData->mEventFlags & XA_PLAYEVENT_HEADATMARKER) &&
                pPlayerData->mCallback)
            {
                pPlayerData->mCallback(
                    IPlayItf, pPlayerData->mContext, XA_PLAYEVENT_HEADATMARKER);
            }
            markerPos = XA_TIME_UNKNOWN;
        }

        result = ALBackendMediaPlayerGetPosition(pCMediaPlayer, &PosMsec);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGE("\n%s[%d] GetPosition failed", __FUNCTION__, __LINE__);
        }

        if (PosMsec > periodic && updatePeriod)
            periodic = PosMsec - (PosMsec % updatePeriod) + updatePeriod;
        waitTime = (periodic >= markerPos ? markerPos : periodic) - PosMsec;

        if (waitTime > (XAint32)delta)
        {
            waitTime -= 2 * rate ;
            waitTime /= rate;
            if (sleepTime < 0)
                waitTime += sleepTime;
            g_get_current_time(&ts);
            g_time_val_add(&ts, (glong)waitTime * 1000);
            g_mutex_lock(pPlayerData->playThreadMutex);
            g_cond_timed_wait(
                pPlayerData->playThreadSema,
                pPlayerData->playThreadMutex,
                (GTimeVal *)&ts);
            g_mutex_unlock(pPlayerData->playThreadMutex);
        }
        if (pPlayerData->stopThread)
            break;
        gst_element_get_state(GST_ELEMENT(pPlayerGraph), &state, &pending, GST_CLOCK_TIME_NONE);
        if ((state != GST_STATE_PLAYING ) || (pending == GST_STATE_PAUSED))
        {
            g_mutex_lock(pPlayerData->playStateChangeMutex);
            g_cond_wait(pPlayerData->playStateChange, pPlayerData->playStateChangeMutex);
            g_mutex_unlock(pPlayerData->playStateChangeMutex);
            pPlayerData->reloadValues = XA_BOOLEAN_TRUE;
        }

        if (pPlayerData->reloadValues == XA_BOOLEAN_TRUE &&
            pPlayerData->stopThread != XA_BOOLEAN_TRUE)
        {
            XA_LOGD("\n$$%s[%d] Reloading values in thread ", __FUNCTION__, __LINE__);

            rate = pPlayRate->CurrentRate / 1000;
            delta = 40 + 4 * rate;
            updatePeriod = pPlayerData->mPositionUpdatePeriod;
            markerPos = (XAmicrosecond)pPlayerData->mMarkerPosition;
            pPlayerData->reloadValues = XA_BOOLEAN_FALSE;

            result = ALBackendMediaPlayerGetPosition(pCMediaPlayer, &PosMsec);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LOGE("\n%s[%d] GetPosition failed", __FUNCTION__, __LINE__);
            }

            if (updatePeriod)
                periodic = PosMsec - (PosMsec % updatePeriod) + updatePeriod;

            if (markerPos != XA_TIME_UNKNOWN && PosMsec > (XAtime)markerPos)
                markerPos = XA_TIME_UNKNOWN;
        }

        waitTime = 0;
        sleepTime = 0;
    }

    XA_LEAVE_INTERFACE;
}


XAresult
ALBackendMediaPlayerSignalThread(CMediaPlayer *pCMediaPlayer)
{
    PlayerData *pPlayerData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaPlayer);
    pPlayerData = (PlayerData *)pCMediaPlayer->mPlay.pData;
    AL_CHK_ARG(pPlayerData);
    pPlayerData->reloadValues = XA_BOOLEAN_TRUE;
    g_mutex_lock(pPlayerData->playThreadMutex);
    g_cond_signal(pPlayerData->playThreadSema);
    g_mutex_unlock(pPlayerData->playThreadMutex);
    XA_LEAVE_INTERFACE;
}

static gboolean
MediaPlayerGstBusCallback(
    GstBus *bus,
    GstMessage *message,
    gpointer data)
{
    CMediaPlayer *pCMediaPlayer;
    XAMediaPlayer *pXAMediaPlayer;
    PlayerData *pPlayerData;
    GstMessageType msgType = GST_MESSAGE_UNKNOWN;
    const XAPlayItf *const *IPlayItf;

    pCMediaPlayer = (CMediaPlayer *)data;
    if (!(pCMediaPlayer && pCMediaPlayer->pData))
        return XA_BOOLEAN_FALSE;

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerData = (PlayerData *)pCMediaPlayer->mPlay.pData;
    IPlayItf = (const XAPlayItf *const *)pCMediaPlayer->mPlay.mItf;
    if (!(pPlayerData && IPlayItf))
        return XA_BOOLEAN_FALSE;

    msgType = GST_MESSAGE_TYPE(message);
    if (msgType != GST_MESSAGE_STATE_CHANGED)
    {
        gchar *srcName = gst_object_get_name(message->src);
        XA_LOGD("Received %s message from element %s",
            gst_message_type_get_name(msgType), srcName);
        g_free(srcName);
    }

    switch (msgType)
    {
        case GST_MESSAGE_ERROR:
        {
            GST_DEBUG("Error message");
            XA_LOGV("AL_To_IL MediaPlayerCallBack: MEDIA_ERROR\n");
            if (pPlayerData && pPlayerData->mCallback)
            {
                pPlayerData->mCallback(
                    (XAPlayItf)IPlayItf,
                    pPlayerData->mContext,
                    XA_PLAYEVENT_HEADSTALLED);
            }
            break;
        }
        case GST_MESSAGE_WARNING:
        {
            GST_WARNING("Warning message");
            break;
        }
        case GST_MESSAGE_TAG:
        {
            XA_LOGD("Warning message");
            break;
        }
        case GST_MESSAGE_EOS:
        {
            XAmillisecond positionMilliSec = XA_TIME_UNKNOWN;
            SeekData *pSeekData = NULL;
            ALBackendMediaPlayerGetPosition(
                pCMediaPlayer, &positionMilliSec);
            XA_LOGD("EOS message AND POS:%u", positionMilliSec);
            pSeekData = (SeekData *)pCMediaPlayer->mSeek.pData;
            if (pSeekData && (pSeekData->loopEnable == XA_BOOLEAN_TRUE))
            {
                ALBackendMediaPlayerSeek(
                    pCMediaPlayer,
                    pSeekData->startPos,
                    XA_SEEKMODE_ACCURATE);
            }
            else
            {
                if (pPlayerData && pPlayerData->mCallback)
                {
                    pPlayerData->mCallback(
                        (XAPlayItf)IPlayItf,
                        pPlayerData->mContext,
                        XA_PLAYEVENT_HEADATEND);
                }
            }
            break;
        }
        case GST_MESSAGE_BUFFERING:
        {
            guint CurrentLevel = 0;
            guint PreviousLevel = 0;
            guint PeriodicCheck = 0;
            guint FillPeriod = 0;
            PrefetchData *pPrefetchData =
                (PrefetchData *)pCMediaPlayer->mPrefetchStatus.pData;
            const XAPrefetchStatusItf *const *IPrefetchStatusItf =
                (const XAPrefetchStatusItf *const *)pCMediaPlayer->mPrefetchStatus.mItf;
            if (pPrefetchData == NULL)
            {
                XA_LOGD("\npPrefetchData is found null in callback for buffering message");
                return XA_BOOLEAN_TRUE;
            }
            FillPeriod = pPrefetchData->mFillUpdatePeriod;
            gst_message_parse_buffering(message, (gint *)&CurrentLevel);
            XA_LOGD("\nBuffering (%u percent done)", CurrentLevel);
            CurrentLevel *= 10;
            PreviousLevel = pPrefetchData->mBufferLevel;
            pPrefetchData->mBufferLevel = CurrentLevel;

            if (pPrefetchData->mCallback == NULL)
                return XA_BOOLEAN_TRUE;

            if (PreviousLevel == 0 && CurrentLevel == 0)
            {
                pPrefetchData->mCallback(
                    (XAPrefetchStatusItf)IPrefetchStatusItf,
                    pPrefetchData->pContext,
                    XA_PREFETCHEVENT_STATUSCHANGE);
                return XA_BOOLEAN_TRUE;
            }

            if (PreviousLevel >= XA_SUFFICIENTDATA_LOWERBOUND &&
                PreviousLevel <= XA_SUFFICIENTDATA_UPPERBOUND)
            {
                if ((CurrentLevel < XA_SUFFICIENTDATA_LOWERBOUND ||
                    CurrentLevel > XA_SUFFICIENTDATA_UPPERBOUND) &&
                    (pPrefetchData->mEventFlags & XA_PREFETCHEVENT_STATUSCHANGE))
                {
                    pPrefetchData->mCallback(
                        (XAPrefetchStatusItf)IPrefetchStatusItf,
                        pPrefetchData->pContext,
                        XA_PREFETCHEVENT_STATUSCHANGE);
                }
            }
            else
            {
                if (((PreviousLevel < XA_SUFFICIENTDATA_LOWERBOUND &&
                    CurrentLevel >= XA_SUFFICIENTDATA_LOWERBOUND) ||
                    (PreviousLevel > XA_SUFFICIENTDATA_UPPERBOUND &&
                    CurrentLevel <= XA_SUFFICIENTDATA_UPPERBOUND)) &&
                    (pPrefetchData->mEventFlags & XA_PREFETCHEVENT_STATUSCHANGE))
                {
                    pPrefetchData->mCallback(
                        (XAPrefetchStatusItf)IPrefetchStatusItf,
                        pPrefetchData->pContext,
                        XA_PREFETCHEVENT_STATUSCHANGE);
                }
            }

            if (FillPeriod == 0 || FillPeriod >= 1000)
                return XA_BOOLEAN_TRUE;

            if (PreviousLevel < CurrentLevel)
            {
                PeriodicCheck = (PreviousLevel % FillPeriod) +
                    (CurrentLevel - PreviousLevel);
            }
            else
            {
                PeriodicCheck = (CurrentLevel % FillPeriod) +
                    (PreviousLevel - CurrentLevel);
            }

            if (PeriodicCheck >= FillPeriod)
            {
                if (pPrefetchData->mEventFlags & XA_PREFETCHEVENT_STATUSCHANGE)
                {
                    pPrefetchData->mCallback(
                        (XAPrefetchStatusItf)IPrefetchStatusItf,
                        pPrefetchData->pContext,
                        XA_PREFETCHEVENT_FILLLEVELCHANGE);
                }
            }
            break;
        }
        case GST_MESSAGE_APPLICATION:
        {
            const gchar *name;
            const GstStructure *gstMsgStructure;
            StreamInfoData *pParamStreamInfo =
                (StreamInfoData *)pCMediaPlayer->mStreamInfo.pData;
            gstMsgStructure = gst_message_get_structure(message);
            name = gst_structure_get_name(gstMsgStructure);
            if (pParamStreamInfo == NULL || pParamStreamInfo->mCallback == NULL)
            {
                break;
            }
            if (name && !strcmp(name, "video-stream-changed"))
            {
                ALBackendMediaPlayerGetVideoStreamInfo(
                    pCMediaPlayer,
                    pParamStreamInfo);
            }
            else if (name && !strcmp(name, "audio-stream-changed"))
            {
                ALBackendMediaPlayerGetAudioStreamInfo(
                    pCMediaPlayer,
                    pParamStreamInfo);
            }
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            gchar *srcName = NULL;
            GstState oldState;
            GstState newState;
            gst_message_parse_state_changed(
                message, &oldState, &newState, NULL);
            if (oldState == newState)
                break;
            /* we only care about playbin (pipeline) state changes */
            if (GST_MESSAGE_SRC(message) !=
                GST_OBJECT(pXAMediaPlayer->mPlayer))
                break;

            srcName = gst_object_get_name(message->src);
            XA_LOGD("%s changed state from %s to %s", srcName,
                gst_element_state_get_name(oldState),
                gst_element_state_get_name(newState));
            g_free(srcName);
            if (newState == GST_STATE_PLAYING)
            {
                if (pPlayerData->hCallbackHandle == NULL)
                {
                    XA_LOGD("\nMarker and Periodic Update Thread is created");
                    pPlayerData->hCallbackHandle = g_thread_create(
                        (GThreadFunc)&TimedCallBackThread,
                        (void *)pCMediaPlayer,
                        TRUE,
                        NULL);
                }
                else if (oldState == GST_STATE_PAUSED)
                {
                    g_mutex_lock(pPlayerData->playStateChangeMutex);
                    g_cond_signal(pPlayerData->playStateChange);
                    g_mutex_unlock(pPlayerData->playStateChangeMutex);
                }
                pPlayerData->mCallback(
                    (XAPlayItf)IPlayItf,
                    pPlayerData->mContext,
                    XA_PLAYEVENT_HEADMOVING);
#if DUMP_MP_PIPELINE_GRAPH
                GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pXAMediaPlayer->mPlayer),
                    GST_DEBUG_GRAPH_SHOW_ALL, "omxalmediaplayergraph");
#endif
            }
            else if (newState == GST_STATE_PAUSED)
            {
                pPlayerData->mCallback(
                    (XAPlayItf)IPlayItf,
                    pPlayerData->mContext,
                    XA_PLAYEVENT_HEADSTALLED);
            }
            break;
        }
        case GST_MESSAGE_ELEMENT:
        case GST_MESSAGE_DURATION:
        case GST_MESSAGE_ASYNC_DONE:
        case GST_MESSAGE_CLOCK_PROVIDE:
        case GST_MESSAGE_CLOCK_LOST:
        case GST_MESSAGE_NEW_CLOCK:
        case GST_MESSAGE_STATE_DIRTY:
        case GST_MESSAGE_STREAM_STATUS:
        case GST_MESSAGE_UNKNOWN:
        case GST_MESSAGE_INFO:
        case GST_MESSAGE_STEP_DONE:
        case GST_MESSAGE_STRUCTURE_CHANGE:
        case GST_MESSAGE_SEGMENT_START:
        case GST_MESSAGE_SEGMENT_DONE:
        case GST_MESSAGE_LATENCY:
        case GST_MESSAGE_ASYNC_START:
        case GST_MESSAGE_REQUEST_STATE:
        case GST_MESSAGE_STEP_START:
        case GST_MESSAGE_ANY:
        default:
        {
            XA_LOGD("Unhandled message: %d", msgType);
            break;
        }
    }
    return XA_BOOLEAN_TRUE;
}

static gpointer
MediaPlayerMainLoopRun(gpointer data)
{
    XAMediaPlayer *pXAMediaPlayer = (XAMediaPlayer *)data;
    pXAMediaPlayer->mLoop = g_main_loop_new(NULL, FALSE);
    if(pXAMediaPlayer->mLoop)
    {
        g_main_loop_run(pXAMediaPlayer->mLoop);
    }
    XA_LOGD("gst_player_main_loop_run\n");
    return pXAMediaPlayer->mLoopThread;
}

static void
MediaPlayerMainLoopQuit(gpointer data)
{
    XAMediaPlayer *pXAMediaPlayer = (XAMediaPlayer *)data;
    if (g_main_loop_is_running(pXAMediaPlayer->mLoop))
    {
        XA_LOGI("player main loop is terminating\n");
        g_main_loop_quit(pXAMediaPlayer->mLoop);
    }
    if (pXAMediaPlayer->mLoopThread)
        g_thread_join(pXAMediaPlayer->mLoopThread);
    pXAMediaPlayer->mLoopThread = NULL;
}

static XAresult
createPlaybackVideoSink(XAMediaPlayer *pXAMediaPlayer)
{
    GstPad *gpad = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pXAMediaPlayer);

    //Create Video Sink element. First try NV OMX video sink
    pXAMediaPlayer->pVideoSink = gst_element_factory_make(
        "nv_omx_videosink", "videoSink");
    if (!pXAMediaPlayer->pVideoSink)
    {
        //Try autovideosink
        pXAMediaPlayer->pVideoSink = gst_element_factory_make(
            "autovideosink", "videoSink");
    }
    if (!pXAMediaPlayer->pVideoSink)
        return XA_RESULT_INTERNAL_ERROR;

    pXAMediaPlayer->pVideoSinkBin = gst_bin_new("videosinkbin");
    if (!pXAMediaPlayer->pVideoSinkBin)
        return XA_RESULT_INTERNAL_ERROR;

    gst_bin_add_many(
        GST_BIN(pXAMediaPlayer->pVideoSinkBin),
        pXAMediaPlayer->pVideoSink,
        NULL);

    gpad = gst_element_get_pad(pXAMediaPlayer->pVideoSink, "sink");
    if (gpad)
    {
        gst_element_add_pad(
            pXAMediaPlayer->pVideoSinkBin, gst_ghost_pad_new("sink", gpad));
        gst_object_unref(gpad);
    }

    XA_LEAVE_INTERFACE;
}

static XAresult
createViewfinderVideoSink(XAMediaPlayer *pXAMediaPlayer)
{
    GstPad *gpad = NULL;

    XA_ENTER_INTERFACE;

    //Create Video Sink element. First try NV OMX video sink
    pXAMediaPlayer->pVideoSink = gst_element_factory_make(
        "nv_omx_videosink", "videoSink");
    if (!pXAMediaPlayer->pVideoSink)
    {
        //Try autovideosink
        pXAMediaPlayer->pVideoSink = gst_element_factory_make(
            "autovideosink", "videoSink");
    }
    if (!pXAMediaPlayer->pVideoSink)
        return XA_RESULT_INTERNAL_ERROR;

    g_object_set(G_OBJECT(pXAMediaPlayer->pVideoSink), "async", FALSE, NULL);
    g_object_set(G_OBJECT(pXAMediaPlayer->pVideoSink), "sync", FALSE, NULL);

    pXAMediaPlayer->pVideoSinkBin = gst_bin_new("videosinkbin");
    if (!pXAMediaPlayer->pVideoSinkBin)
        return XA_RESULT_INTERNAL_ERROR;

    gst_bin_add_many(
        GST_BIN(pXAMediaPlayer->pVideoSinkBin),
        pXAMediaPlayer->pVideoSink,
        NULL);

    gpad = gst_element_get_pad(pXAMediaPlayer->pVideoSink, "sink");
    if (gpad)
    {
        gst_element_add_pad(
            pXAMediaPlayer->pVideoSinkBin, gst_ghost_pad_new("sink", gpad));
        gst_object_unref(gpad);
    }

    XA_LEAVE_INTERFACE;
}

XAresult ALBackendMediaPlayerCreate(CMediaPlayer *pCMediaPlayer)
{
    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaPlayer);

    pCMediaPlayer->pData = (XAMediaPlayer *)malloc(
        sizeof(XAMediaPlayer));
    if(!pCMediaPlayer->pData)
        return XA_RESULT_MEMORY_FAILURE;
    memset(pCMediaPlayer->pData, 0, sizeof(XAMediaPlayer));


    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaPlayerRealize(
    CMediaPlayer *pCMediaPlayer,
    XAboolean bAsync)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XADataLocator_URI *uri = NULL;
    XADataFormat_MIME *mime = NULL;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;

    //set source
    if (pCMediaPlayer->mDataSrc.pLocator != NULL)
    {
        XAint32 *locator = NULL;
        CameraDeviceData *pCamData = NULL;
        XADataLocator_IODevice* ioDevice = NULL;
        CCameraDevice *pCameraDevice = NULL;
        locator = (XAint32*)(pCMediaPlayer->mDataSrc.pLocator);
        ioDevice = (XADataLocator_IODevice*)
            (pCMediaPlayer->mDataSrc.pLocator);
        AL_CHK_ARG(ioDevice);
        if (*locator == XA_DATALOCATOR_IODEVICE &&
            (ioDevice->deviceType == XA_IODEVICE_CAMERA))
        {
            //check the device type
            pCameraDevice = (CCameraDevice* )(ioDevice->device);
            AL_CHK_ARG(pCameraDevice);
            pCamData = (CameraDeviceData *)(pCameraDevice->pData);
            pXAMediaPlayer->mPlayer = pCamData->pCamerabin;
            pXAMediaPlayer->pCameraSrc = pCamData->pCameraSrc;

            result = createViewfinderVideoSink(pXAMediaPlayer);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LEAVE_INTERFACE;
            }
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(
                pXAMediaPlayer->mPlayer), "viewfinder-caps"))
            {
                GstCaps *pCameraCaps = NULL;
                pCameraCaps = gst_caps_new_simple("video/x-raw-yuv",
                    "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('N', 'V', 'B', '1'),
                    "width", G_TYPE_INT, 640,
                    "height", G_TYPE_INT, 480,
                    NULL);
                g_object_set(G_OBJECT(pXAMediaPlayer->mPlayer),
                    "viewfinder-caps", pCameraCaps, NULL);
                gst_caps_unref(pCameraCaps);
            }
        }
        else if (*locator == XA_DATALOCATOR_URI)
        {
            guint flags;
            uri = (XADataLocator_URI *)pCMediaPlayer->mDataSrc.pLocator;
            mime = (XADataFormat_MIME *)pCMediaPlayer->mDataSrc.pFormat;

            if (!strcmp((const char *)mime->mimeType, "image/jpeg"))
            {
                pXAMediaPlayer->bImagePlayback = XA_BOOLEAN_TRUE;
            }

            pXAMediaPlayer->mPlayer = gst_element_factory_make(
                "playbin2", "OMXALPlaybin2");
            if (!pXAMediaPlayer->mPlayer)
            {
                result = XA_RESULT_INTERNAL_ERROR;
                XA_LEAVE_INTERFACE;
            }
            pXAMediaPlayer->mBus = gst_element_get_bus(
                pXAMediaPlayer->mPlayer);
            gst_bus_add_signal_watch(pXAMediaPlayer->mBus);

            pXAMediaPlayer->mSigBusAsync =
                g_signal_connect(pXAMediaPlayer->mBus, "message",
                G_CALLBACK(MediaPlayerGstBusCallback),
                pCMediaPlayer);

            //Create Video Sink with one element only for now
            result = createPlaybackVideoSink(pXAMediaPlayer);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LOGE("Could not create Playback Videosink\n");
                XA_LEAVE_INTERFACE;
            }

            if (uri->URI)
            {
                g_object_set(pXAMediaPlayer->mPlayer,
                    "uri", uri->URI, NULL);
            }
            //If AudioSink is provided
            if (pCMediaPlayer->mAudioSnk.pLocator != NULL)
            {
                locator = (XAint32*)(pCMediaPlayer->mAudioSnk.pLocator);
                //If AudioSink is OutputMix object
                if (*locator == XA_DATALOCATOR_OUTPUTMIX)
                {
                    COutputMix *pCOutputMix = NULL;
                    COutputMixData *pCOutputMixData = NULL;
                    XADataLocator_OutputMix *outputMix = NULL;
                    outputMix = (XADataLocator_OutputMix *)
                        pCMediaPlayer->mAudioSnk.pLocator;
                    pCOutputMix = (COutputMix *)outputMix->outputMix;
                    pCOutputMixData = (COutputMixData *)pCOutputMix->pData;
                    g_object_set(
                        pXAMediaPlayer->mPlayer,
                        "audio-sink",
                        pCOutputMixData->pOutputMixBin,
                        NULL);
                }
            }
            g_object_get(pXAMediaPlayer->mPlayer, "flags", &flags, NULL);
            flags |= ((1 << 5) | (1 << 6)); //Native Audio and Video
            //flags |= (1 << 6);  //Only Native Video
            g_object_set(pXAMediaPlayer->mPlayer, "flags", flags, NULL);
            flags = 0;
            g_object_get(pXAMediaPlayer->mPlayer, "flags", &flags, NULL);

            // stream change and tags chnage call backs
            g_signal_connect(pXAMediaPlayer->mPlayer, "audio-tags-changed",
                G_CALLBACK(ALBackendMediaPlayerAudioTagsChangedCallback), pCMediaPlayer);
            g_signal_connect(pXAMediaPlayer->mPlayer, "video-tags-changed",
                G_CALLBACK(ALBackendMediaPlayerVideoTagsChangedCallback), pCMediaPlayer);
            g_signal_connect(pXAMediaPlayer->mPlayer, "text-tags-changed",
                G_CALLBACK(ALBackendMediaPlayerTextTagsChangedCallback), pCMediaPlayer);
            g_signal_connect(pXAMediaPlayer->mPlayer, "video-changed",
                G_CALLBACK(ALBackendMediaPlayerVideoChangedCallback), pCMediaPlayer);
            g_signal_connect(pXAMediaPlayer->mPlayer, "audio-changed",
                G_CALLBACK(ALBackendMediaPlayerAudioChangedCallback), pCMediaPlayer);
            g_signal_connect(pXAMediaPlayer->mPlayer, "text-changed",
                G_CALLBACK(ALBackendMediaPlayerTextChangedCallback), pCMediaPlayer);
        }
        else
        {
            XA_LOGE("UNSUPPORTED DATA SOURCE\n");
            result = XA_RESULT_PARAMETER_INVALID;
        }
    }

    XA_LEAVE_INTERFACE;
}

void ALBackendMediaPlayerDestroy(CMediaPlayer *pCMediaPlayer)
{
    XA_ENTER_INTERFACE_VOID

    if (pCMediaPlayer)
    {
        if (pCMediaPlayer->pData)
        {
            free(pCMediaPlayer->pData);
            pCMediaPlayer->pData = NULL;
        }
        if (pCMediaPlayer->pDataTaps)
        {
            //Free the memory
            free(pCMediaPlayer->pDataTaps);
            pCMediaPlayer->pDataTaps = NULL;
        }
    }

    XA_LEAVE_INTERFACE_VOID
}

XAresult
ALBackendMediaPlayerResume(CMediaPlayer *pCMediaPlayer, XAboolean bAsync)
{
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer);
    result = ALBackendMediaPlayerSetPlayState(
        pCMediaPlayer,
        XA_PLAYSTATE_PLAYING);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaPlayerGetDuration(
    CMediaPlayer *pCMediaPlayer,
    XAmillisecond *pDurMsec)
{
    GstElement *pPlayerGraph = NULL;
    GstFormat fmt = GST_FORMAT_TIME;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XAuint64 duration = XA_TIME_UNKNOWN;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pDurMsec);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    AL_CHK_ARG(pPlayerGraph);

    if (!gst_element_query_duration(
        pPlayerGraph,
        &fmt,
        (gint64*)&duration))
    {
        XA_LOGE("\n gst_element_query_duration Failed! \n");
        result = XA_RESULT_INTERNAL_ERROR;
    }

    if (duration < 0)
        *pDurMsec = XA_TIME_UNKNOWN;
    else
        *pDurMsec = (XAmillisecond)(duration / GST_MSECOND);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaPlayerGetPosition(
    CMediaPlayer *pCMediaPlayer,
    XAmillisecond *pPosMsec)
{
    GstElement *pPlayerGraph = NULL;
    GstFormat fmt = GST_FORMAT_TIME;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    gint64 position = 0;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pPosMsec);
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    AL_CHK_ARG(pPlayerGraph);

    if (pXAMediaPlayer->bImagePlayback)
    {
        position = 0;
    }
    else
    {
        if (!gst_element_query_position(
            pPlayerGraph,
            &fmt,
            &position))
        {
            XA_LOGE("\n gst_element_query_position Failed! \n");
            result = XA_RESULT_INTERNAL_ERROR;
        }
    }

    if (position < 0)
        *pPosMsec = XA_TIME_UNKNOWN;
    else
        *pPosMsec = (XAmillisecond)(position / GST_MSECOND);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaPlayerSetLoop(CMediaPlayer *pCMediaPlayer)
{
    GstElement *pPlayerGraph = NULL;
    SeekData *pSeekData = NULL;
    PlayRateData *pPlayRate = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    gdouble rate;
    gint64 startValue;
    gint64 stopValue;
    GstSeekType stopFlag = GST_SEEK_TYPE_SET;
    GstFormat fmt = GST_FORMAT_TIME;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    pSeekData = (SeekData *)pCMediaPlayer->mSeek.pData;
    pPlayRate = (PlayRateData *)pCMediaPlayer->mPlaybackRate.pData;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    AL_CHK_ARG(pPlayerGraph);
    if (pPlayRate->CurrentRate == 0)
        rate = ((gdouble)pPlayRate->PreviousRate) / 1000;
    else
        rate = ((gdouble)pPlayRate->CurrentRate) / 1000;
    startValue = ((gint64)pSeekData->startPos) * GST_MSECOND;
    stopValue = ((gint64)pSeekData->endPos) * GST_MSECOND;
    if (pSeekData->loopEnable)
    {
        if (pSeekData->endPos == XA_TIME_UNKNOWN)
        {
            stopValue = GST_CLOCK_TIME_NONE;
            stopFlag = GST_SEEK_TYPE_NONE;
        }
        else
        {
            stopFlag = GST_SEEK_TYPE_SET;
        }
    }

    if (!gst_element_query_position(
        pPlayerGraph,
        &fmt,
        &startValue))
    {
        XA_LOGE("\n gst_element_query_position Failed! \n");
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }
    XA_LOGD("start:%llu stop:%llu \n", startValue, stopValue);
    if (!gst_element_seek(pPlayerGraph, rate,
        GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
        GST_SEEK_TYPE_SET, startValue,
        GST_SEEK_TYPE_SET, stopValue))
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE;
}


XAresult
ALBackendMediaPlayerSetRate(CMediaPlayer *pCMediaPlayer, XApermille newRate)
{
    GstElement *pPlayerGraph = NULL;
    GstSeekFlags flags = GST_SEEK_FLAG_NONE;
    gdouble rate;
    gint64 startValue;
    gint64 stopValue;
    GstSeekType startFlag;
    GstSeekType stopFlag;
    gint64 positionNanosec = 0;
    GstFormat fmt = GST_FORMAT_TIME;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    SeekData *pSeekData = NULL;
    PlayRateData *pPlayRate = NULL;
    PlayerData *pPlayerData = NULL;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    pSeekData = (SeekData *)pCMediaPlayer->mSeek.pData;
    pPlayRate = (PlayRateData *)pCMediaPlayer->mPlaybackRate.pData;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    pPlayerData = (PlayerData *)pCMediaPlayer->mPlay.pData;
    AL_CHK_ARG(pPlayerGraph);

    if (newRate == 0)
    {
        if (pPlayRate->CurrentRate)
            pPlayRate->PreviousRate = pPlayRate->CurrentRate;

        result = ChangeGraphState(pPlayerGraph, GST_STATE_PAUSED);

        XA_LEAVE_INTERFACE;
    }
    rate = (gdouble)newRate / 1000;
    startValue = 0;
    flags = (GstSeekFlags)(GST_SEEK_FLAG_SKIP | GST_SEEK_FLAG_FLUSH);

    if (!gst_element_query_position(
        pPlayerGraph,
        &fmt,
        &positionNanosec))
    {
        XA_LOGE("\n gst_element_query_position Failed! \n");
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    if (rate > 0.0)
    {
        if (pSeekData->loopEnable)
        {
            if (pSeekData->endPos == XA_TIME_UNKNOWN)
            {
                stopValue = GST_CLOCK_TIME_NONE;
                stopFlag = GST_SEEK_TYPE_NONE;
            }
            else
            {
                stopFlag = GST_SEEK_TYPE_SET;
                stopValue = ((gint64) pSeekData->endPos) * GST_MSECOND;
            }
            startValue = positionNanosec;
            startFlag = GST_SEEK_TYPE_SET;
        }
        else
        {
            startFlag = GST_SEEK_TYPE_SET;
            startValue = positionNanosec;
            stopFlag = GST_SEEK_TYPE_NONE;
            stopValue = GST_CLOCK_TIME_NONE;
        }
    }
    else
    {
        pSeekData->loopEnable = XA_BOOLEAN_FALSE;
        startFlag = GST_SEEK_TYPE_SET;
        startValue = 0;
        stopFlag = GST_SEEK_TYPE_NONE;
        stopValue = GST_CLOCK_TIME_NONE;
    }

    if (!gst_element_seek(pPlayerGraph, rate,
        GST_FORMAT_TIME, flags,
        startFlag, startValue,
        stopFlag, stopValue))
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }
    else
    {
        if (newRate != 0 && pPlayRate->PreviousRate == 0 &&
            pPlayerData->mState == XA_PLAYSTATE_PLAYING)
        {
            result = ChangeGraphState(pPlayerGraph, GST_STATE_PLAYING);
            if (result != XA_RESULT_SUCCESS)
            {
               XA_LEAVE_INTERFACE;
            }
        }
        ALBackendMediaPlayerSignalThread(pCMediaPlayer);
    }
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaPlayerSeek(
    CMediaPlayer *pCMediaPlayer,
    XAmillisecond position,
    XAuint32 seekMode)
{
    GstElement *pPlayerGraph = NULL;
    GstFormat fmt = GST_FORMAT_TIME;
    GstSeekType startFlag;
    GstSeekType stopFlag;
    gdouble rate;
    gint64 startValue;
    gint64 stopValue;
    gint64 pos;
    GstSeekFlags flags = GST_SEEK_FLAG_NONE;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XAmillisecond duration = XA_TIME_UNKNOWN;
    SeekData *pSeekData = NULL;
    PlayRateData *pPlayRate = NULL;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    pSeekData = (SeekData *)pCMediaPlayer->mSeek.pData;
    pPlayRate = (PlayRateData *)pCMediaPlayer->mPlaybackRate.pData;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    AL_CHK_ARG(pPlayerGraph);

    result = ALBackendMediaPlayerGetDuration(
        pCMediaPlayer,
        &duration);
    if (result != XA_RESULT_SUCCESS)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }
    if (position < 0 || position > duration)
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    flags = GST_SEEK_FLAG_FLUSH;
    switch(seekMode)
    {
        case XA_SEEKMODE_ACCURATE:
            flags = (GstSeekFlags)(flags | GST_SEEK_FLAG_ACCURATE);
            break;
        case XA_SEEKMODE_FAST:
            flags = (GstSeekFlags)(flags | GST_SEEK_FLAG_KEY_UNIT);
            break;
        default:
            flags = (GstSeekFlags)(flags | GST_SEEK_FLAG_KEY_UNIT);
            break;
    }

    startValue = 0;
    stopValue = GST_CLOCK_TIME_NONE;
    startFlag = GST_SEEK_TYPE_SET;
    stopFlag = GST_SEEK_TYPE_NONE;
    if (pPlayRate->CurrentRate == 0)
        rate = (gdouble)(pPlayRate->PreviousRate / 1000);
    else
        rate = (gdouble)(pPlayRate->CurrentRate / 1000);

    if (pSeekData->loopEnable)
    {
        startValue = ((gint64)pSeekData->startPos) * GST_MSECOND;
        if (pSeekData->endPos == XA_TIME_UNKNOWN)
        {
            stopValue = GST_CLOCK_TIME_NONE;
        }
        else
        {
            stopFlag = GST_SEEK_TYPE_SET;
            stopValue = ((guint64)pSeekData->endPos) * GST_MSECOND;
        }
        if (position == pSeekData->endPos)
            position = startValue;
    }
    else
    {
        if (rate < 0.0)
        {
            startFlag = GST_SEEK_TYPE_NONE;
            stopFlag = GST_SEEK_TYPE_NONE;
        }
    }

    if (position > pSeekData->endPos)
    {
        XA_LOGD("position:%d start:%llu stop:%llu",
            position, startValue, stopValue);
        pSeekData->loopEnable = XA_BOOLEAN_FALSE;
        stopValue = GST_CLOCK_TIME_NONE;
    }
    XA_LOGD("rate:%f position:%d stop:%llu", rate, position, stopValue);
    pos = (gint64)(position * GST_MSECOND);
    if (!gst_element_seek(pPlayerGraph, rate,
        fmt, flags,
        startFlag, pos,
        stopFlag, stopValue))
    {
        XA_LOGE("Seek failed for position = %d\n", position);
        result = XA_RESULT_INTERNAL_ERROR;
    }
    else
    {
        ALBackendMediaPlayerSignalThread(pCMediaPlayer);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaPlayerSetPlayState(
    CMediaPlayer *pCMediaPlayer,
    XAuint32 playState)
{
    PlayerData *pPlayerData = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;
    GstElement *pPlayerGraph = NULL;
    PlayRateData *pPlayRate = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    pPlayerData = (PlayerData *)pCMediaPlayer->mPlay.pData;
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    pPlayerGraph = pXAMediaPlayer->mPlayer;
    AL_CHK_ARG(pPlayerGraph);
    pPlayRate = (PlayRateData *)pCMediaPlayer->mPlaybackRate.pData;

    switch (playState)
    {
        case XA_PLAYSTATE_STOPPED:
        {
            XA_LOGD("\n XA_PLAYSTATE_STOPPED: %s[%d]", __FUNCTION__, __LINE__);
            result = ChangeGraphState(pPlayerGraph, GST_STATE_NULL);
            MediaPlayerMainLoopQuit(pXAMediaPlayer);
            XA_LEAVE_INTERFACE;
        }
        case XA_PLAYSTATE_PAUSED:
        {
            XA_LOGD("\n XA_PLAYSTATE_PAUSED: %s[%d]", __FUNCTION__, __LINE__);
            result = ChangeGraphState(pPlayerGraph, GST_STATE_PAUSED);
            XA_LEAVE_INTERFACE;
        }
        case XA_PLAYSTATE_PLAYING:
        {
            XA_LOGD("\n XA_PLAYSTATE_PLAYING: %s[%d]", __FUNCTION__, __LINE__);
            if (pPlayRate && pPlayRate->CurrentRate == 0)
            {
                XA_LEAVE_INTERFACE;
            }

            if (g_object_class_find_property(G_OBJECT_GET_CLASS(
                pXAMediaPlayer->mPlayer), "viewfinder"))
            {
                g_object_set(
                    G_OBJECT(pXAMediaPlayer->pCameraSrc),
                    "viewfinder", TRUE, NULL);
            }
            if (GST_STATE(pPlayerGraph) == GST_STATE_NULL ||
                GST_STATE(pPlayerGraph) == GST_STATE_PAUSED ||
                GST_STATE(pPlayerGraph) == GST_STATE_READY)
            {
                if (pXAMediaPlayer->pVideoSinkBin)
                {
                    if (g_object_class_find_property(G_OBJECT_GET_CLASS(
                        pXAMediaPlayer->mPlayer), "viewfinder-sink"))
                    {
                        g_object_set(pXAMediaPlayer->mPlayer,
                            "viewfinder-sink",
                            pXAMediaPlayer->pVideoSinkBin,
                            NULL);
                    }
                    else if (g_object_class_find_property(G_OBJECT_GET_CLASS(
                        pXAMediaPlayer->mPlayer), "video-sink"))
                    {
                        g_object_set(pXAMediaPlayer->mPlayer,
                            "video-sink",
                            pXAMediaPlayer->pVideoSinkBin,
                            NULL);
                    }
                }
                result = ChangeGraphState(pPlayerGraph, GST_STATE_PLAYING);
                if (result != XA_RESULT_SUCCESS)
                {
                    XA_LEAVE_INTERFACE;
                }
                if (!pXAMediaPlayer->mLoopThread)
                {
                    pXAMediaPlayer->mLoopThread = g_thread_create(
                        (GThreadFunc)MediaPlayerMainLoopRun,
                        pXAMediaPlayer, TRUE, NULL);
                    if (!pXAMediaPlayer->mLoopThread)
                    {
                        XA_LOGE("\n Creating Main loop thread failed!!\n");
                        result = XA_RESULT_INTERNAL_ERROR;
                    }
                }
            }
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
    }
    XA_LEAVE_INTERFACE;
}

/* VideoPostProcessing Interface implementation */
XAresult
ALBackendVideoPostProcessingCommit(
   CMediaPlayer *pCMediaPlayer)
{
    XA_ENTER_INTERFACE
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE
}

/* Volume Interface implementation */
XAresult
ALBackendMediaPlayerSetVolume(
    CMediaPlayer *pCMediaPlayer,
    XAmillibel level)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData)
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;

    gst_stream_volume_set_volume(
        GST_STREAM_VOLUME(pXAMediaPlayer->mPlayer),
        GST_STREAM_VOLUME_FORMAT_DB,
        (gdouble)(level/100));

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetVolume(
    CMediaPlayer *pCMediaPlayer,
    XAmillibel *pLevel)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    gdouble level;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pLevel)
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;

    level = gst_stream_volume_get_volume(
        GST_STREAM_VOLUME(pXAMediaPlayer->mPlayer),
        GST_STREAM_VOLUME_FORMAT_DB);
    *pLevel = (XAmillibel)(level * 100);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetMaxVolume(
    CMediaPlayer *pCMediaPlayer,
    XAmillibel *pMaxLevel)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    gdouble level;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pMaxLevel)
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;

    level = gst_stream_volume_convert_volume(
        GST_STREAM_VOLUME_FORMAT_LINEAR,
        GST_STREAM_VOLUME_FORMAT_DB,
        1.0);
    *pMaxLevel = (XAmillibel)(level * 100);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerSetMute(
    CMediaPlayer *pCMediaPlayer,
    XAboolean mute)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData)
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;

    gst_stream_volume_set_mute(
        GST_STREAM_VOLUME(pXAMediaPlayer->mPlayer),
        mute);

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetMute(
    CMediaPlayer *pCMediaPlayer,
    XAboolean *pMute)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pMute)
    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;

    *pMute = gst_stream_volume_get_mute(
        GST_STREAM_VOLUME(pXAMediaPlayer->mPlayer));

    XA_LEAVE_INTERFACE
}

/* DynamicSource Interface implementation */
XAresult
ALBackendMediaPlayerSetSource(
    CMediaPlayer *pCMediaPlayer,
    XADataSource *pDataSource)
{
    XAMediaPlayer *pXAMediaPlayer = NULL;
    XADataLocator_URI *uri = NULL;
    XADataFormat_MIME *mime = NULL;
    XAint32 *locator = NULL;
    GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
    GstState stateCurrent;
    GstState statePending;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pDataSource)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->mPlayer)

    if (pDataSource->pLocator != NULL)
    {
        locator = (XAint32 *)pDataSource->pLocator;
        if (*locator == XA_DATALOCATOR_URI)
        {
            uri = (XADataLocator_URI *)pDataSource->pLocator;
            mime = (XADataFormat_MIME *)pDataSource->pFormat;
            if (uri->URI)
            {
                ret = gst_element_get_state(pXAMediaPlayer->mPlayer,
                    &stateCurrent, &statePending, GST_CLOCK_TIME_NONE);
                if (ret == GST_STATE_CHANGE_SUCCESS)
                {
                    result = ChangeGraphState(
                        pXAMediaPlayer->mPlayer, GST_STATE_NULL);
                    if (result != XA_RESULT_SUCCESS)
                    {
                        XA_LEAVE_INTERFACE
                    }

                    if (!strcmp((const char *)mime->mimeType, "image/jpeg"))
                    {
                        pXAMediaPlayer->bImagePlayback = XA_BOOLEAN_TRUE;
                    }
                    else
                    {
                        pXAMediaPlayer->bImagePlayback = XA_BOOLEAN_FALSE;
                    }

                    g_object_set(pXAMediaPlayer->mPlayer,
                        "uri", uri->URI, NULL);

                    result = ChangeGraphState(
                        pXAMediaPlayer->mPlayer, stateCurrent);
                    if (result != XA_RESULT_SUCCESS)
                    {
                        XA_LEAVE_INTERFACE
                    }
                    // Update mDataSrc
                    pCMediaPlayer->mDataSrc = *pDataSource;
                }
            }
        }
        else
        {
            XA_LOGE("UNSUPPORTED DATA SOURCE\n");
            result = XA_RESULT_PARAMETER_INVALID;
        }
    }

    XA_LEAVE_INTERFACE
}


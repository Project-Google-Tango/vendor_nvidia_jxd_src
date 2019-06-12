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

#include "linux_mediarecorder.h"
#include "linux_cameradevice.h"
#include <gst/interfaces/photography.h>
#include <unistd.h>

#define DUMP_MR_PIPELINE_GRAPH 1

static void TimedCallBackThread(void *arg)
{
    CMediaRecorder *pCMediaRecorder = NULL;
    XAMediaRecorder *pXAMediaRecorder = NULL;
    RecordData *pRecordData = NULL;
    GstElement *pRecorderGraph = NULL;
    XAresult result = XA_RESULT_SUCCESS;
    XARecordItf IRecordItf;
    XAtime posMsec = 0;
    XAmicrosecond delta = 40;
    XAint32 waitTime = 0;
    XAint32 sleepTime = 0;
    XAmicrosecond markePos = 0;
    XAmicrosecond periodic = 0;
    XAmicrosecond durationLimit = 0;
    XAmicrosecond updatePeriod = 0;
    GstState state;
    GstState pending;
    GTimeVal ts;

    pCMediaRecorder = (CMediaRecorder *)arg;
    if (!(pCMediaRecorder && pCMediaRecorder->pData))
        return;

    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecordData = (RecordData *)pCMediaRecorder->mRecord.pData;
    IRecordItf = (XARecordItf)pCMediaRecorder->mRecord.mItf;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    if (!(pRecordData && pRecorderGraph))
        return;

    markePos = pRecordData->mMarkerPosition;
    periodic = pRecordData->mPositionUpdatePeriod;
    durationLimit = pRecordData->mDurationLimit;
    updatePeriod = periodic;
    while (!pXAMediaRecorder->bStopThread)
    {
        if (pXAMediaRecorder->bReloadValues == XA_BOOLEAN_TRUE)
        {
            updatePeriod = pRecordData->mPositionUpdatePeriod;
            markePos = pRecordData->mMarkerPosition;
            durationLimit = pRecordData->mDurationLimit;
            pXAMediaRecorder->bReloadValues = XA_BOOLEAN_FALSE;
            result = ALBackendMediaRecorderGetClockTime(
                pCMediaRecorder, (XAmillisecond*)&posMsec);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LOGE("\n GetPosition failed in TimedCallbackThread \n");
                break;
            }

            if (updatePeriod)
                periodic = posMsec - (posMsec % updatePeriod) + updatePeriod;
            if (markePos != XA_TIME_UNKNOWN && posMsec > markePos)
                markePos = XA_TIME_UNKNOWN;
            if (durationLimit != XA_TIME_UNKNOWN && posMsec > durationLimit)
                durationLimit = XA_TIME_UNKNOWN;
            XA_LOGD("\n Reload Values WaitTime:%d Periodic:%u PosMsec:%llu"
                " marker:%u duration Limit:%u state:%u \n",
                waitTime, periodic, posMsec, markePos, durationLimit,
                pRecordData->mState);
        }
        waitTime = 0;
        sleepTime = 0;

        result = ALBackendMediaRecorderGetClockTime(
            pCMediaRecorder, (XAmillisecond *)&posMsec);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGE("\nGetPosition failed in TimedCallbackThread\n");
            break;
        }
        if (((periodic - delta) <= posMsec) && ((periodic + delta) >= posMsec))
        {
            sleepTime = (XAint32)(periodic - posMsec);
            if (sleepTime > 0)
                usleep(sleepTime * 1000);

            if ((pRecordData->mEventFlags & XA_PLAYEVENT_HEADATNEWPOS) &&
                pRecordData->mCallback)
            {
                pRecordData->mCallback(
                    IRecordItf,
                    pRecordData->pContext,
                    XA_PLAYEVENT_HEADATNEWPOS);
            }
            periodic += updatePeriod;
        }

        if (markePos != XA_TIME_UNKNOWN &&
            (((markePos - delta) <= posMsec) && ((markePos + delta) >= posMsec)))
        {
            result = ALBackendMediaRecorderGetClockTime(
                pCMediaRecorder, (XAmillisecond*)&posMsec);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LOGE("\nGetPosition failed in TimedCallbackThread\n");
                break;
            }
            sleepTime = (XAint32)(markePos - posMsec);
            if (sleepTime > 0)
                usleep(sleepTime * 1000);

            if ((pRecordData->mEventFlags & XA_PLAYEVENT_HEADATMARKER) &&
                  pRecordData->mCallback)
            {
                pRecordData->mCallback(
                    IRecordItf,
                    pRecordData->pContext,
                    XA_PLAYEVENT_HEADATMARKER);
            }
            markePos = XA_TIME_UNKNOWN;
        }

        if (durationLimit != XA_TIME_UNKNOWN &&
            (((durationLimit - delta) <= posMsec &&
              (durationLimit + delta) >= posMsec)))
        {
            result = ALBackendMediaRecorderGetClockTime(
                pCMediaRecorder, (XAmillisecond*)&posMsec);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LOGE("GetPosition failed in TimedCallbackThread\n");
                break;
            }
            sleepTime = (XAint32)(durationLimit - posMsec);
            if (sleepTime > 0)
                usleep(sleepTime * 1000);

            if (pRecordData->mCallback &&
                (pRecordData->mEventFlags &
                  XA_RECORDEVENT_HEADATLIMIT))
            {
                pRecordData->mCallback(
                    IRecordItf,
                    pRecordData->pContext,
                    XA_RECORDEVENT_HEADATLIMIT);
            }
            durationLimit = XA_TIME_UNKNOWN;
        }

        result = ALBackendMediaRecorderGetClockTime(
            pCMediaRecorder, (XAmillisecond*)&posMsec);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGE("\nGetPosition failed in TimedCallbackThread\n");
            break;
        }

        if ((posMsec > periodic) && updatePeriod)
        {
            periodic = posMsec - (posMsec % updatePeriod) + updatePeriod;
        }
        waitTime = (periodic >= markePos ?
            (durationLimit >= markePos ? markePos : durationLimit) :
            (periodic >= durationLimit ? durationLimit : periodic)) - posMsec;
        if (waitTime > 40)
        {
            if (sleepTime < 0)
                waitTime += sleepTime;

            g_get_current_time(&ts);
            g_time_val_add(&ts, (glong)waitTime * 1000);
            g_mutex_lock(pXAMediaRecorder->pRecordThreadMutex);
            g_cond_timed_wait(
                pXAMediaRecorder->pRecordThreadSema,
                pXAMediaRecorder->pRecordThreadMutex,
                (GTimeVal *)&ts);
            g_mutex_unlock(pXAMediaRecorder->pRecordThreadMutex);
        }

        if (gst_element_get_state(GST_ELEMENT(pRecorderGraph),
            &state, &pending, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS)
        {
            if ((pXAMediaRecorder->mRecordState != XA_RECORDSTATE_RECORDING) ||
                (state != GST_STATE_PLAYING) || (pending == GST_STATE_PAUSED))
            {
                XA_LOGD("\n ********Waiting for satate change******* \n");
                g_mutex_lock(pXAMediaRecorder->pStateChangeMutex);
                g_cond_wait(pXAMediaRecorder->pStateChangeSema,
                    pXAMediaRecorder->pStateChangeMutex);
                g_mutex_unlock(pXAMediaRecorder->pStateChangeMutex);
                pXAMediaRecorder->bReloadValues = XA_BOOLEAN_TRUE;
                XA_LOGD("\n ********satate has changed******* \n");
            }
        }
        else
        {
            XA_LOGE("\n state querying is failed in TimedCallbackThread\n");
            break;
        }
    }

    pXAMediaRecorder->pCallbackHandle = NULL;
}

static XAboolean
MediaRecorderViewFinderIsDisabled(XAMediaRecorder *pXAMediaRecorder)
{
    GstElement *pViewFinderSink = NULL;
    gchar *pViewFinderSinkName = NULL;
    XAboolean bResult = XA_BOOLEAN_FALSE;

    //Check the View finder is enbled or not
    g_object_get(G_OBJECT(pXAMediaRecorder->mRecorder),
        "viewfinder-sink", &pViewFinderSink, NULL);
    if (pViewFinderSink == NULL)
        return bResult;

    pViewFinderSinkName = gst_element_get_name(pViewFinderSink);
    g_object_unref(pViewFinderSink);
    if (pViewFinderSinkName == NULL)
        return bResult;

    if (!strcasecmp((const char*)pViewFinderSinkName, "OMXALFakesink"))
        bResult = XA_BOOLEAN_TRUE;
    g_free(pViewFinderSinkName);

    return bResult;
}

XAresult
ALBackendMediaRecorderSignalThread(CMediaRecorder *pCMediaRecorder)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;

    pXAMediaRecorder->bReloadValues = XA_BOOLEAN_TRUE;
    g_mutex_lock(pXAMediaRecorder->pRecordThreadMutex);
    g_cond_signal(pXAMediaRecorder->pRecordThreadSema);
    g_mutex_unlock(pXAMediaRecorder->pRecordThreadMutex);

    XA_LEAVE_INTERFACE;
}

static gboolean
MediaRecorderGstBusCallback(
    GstBus *pBus,
    GstMessage *pMessage,
    gpointer data)
{
    GstMessageType msgType = GST_MESSAGE_UNKNOWN;
    CMediaRecorder *pCMediaRecorder = (CMediaRecorder *)data;
    XAMediaRecorder *pXAMediaRecorder = NULL;
    RecordData *pRecordData = NULL;
    SnapshotData *pSnapShotData = NULL;
    XARecordItf IRecordItf;
    XASnapshotItf ISnapshotItf;

    XA_ENTER_INTERFACE_VOID;

    if (!(pCMediaRecorder && pCMediaRecorder->pData))
        return XA_BOOLEAN_FALSE;

    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecordData = (RecordData *)pCMediaRecorder->mRecord.pData;
    pSnapShotData = (SnapshotData *)pCMediaRecorder->mSnapshot.pData;
    IRecordItf = (XARecordItf)pCMediaRecorder->mRecord.mItf;
    ISnapshotItf = (XASnapshotItf)pCMediaRecorder->mSnapshot.mItf;

    if (!(pRecordData && pSnapShotData && IRecordItf && ISnapshotItf))
        return XA_BOOLEAN_FALSE;

    msgType = GST_MESSAGE_TYPE(pMessage);
    switch (msgType)
    {
        case GST_MESSAGE_ERROR:
        {
            GError *pErr = NULL;
            gst_message_parse_error(pMessage, &pErr, NULL);
            XA_LOGE(" ERROR from element %s: %s \n",
                GST_OBJECT_NAME(pMessage->src), pErr->message);
            /* some Recording failed */
            g_mutex_lock(pXAMediaRecorder->pExitMutex);
            g_cond_signal(pXAMediaRecorder->pExitSema);
            g_mutex_unlock(pXAMediaRecorder->pExitMutex);
            g_error_free(pErr);
            if (pRecordData->mCallback &&
                (pRecordData->mEventFlags &
                 XA_RECORDEVENT_HEADSTALLED))
            {
                pRecordData->mCallback(
                    IRecordItf,
                    pRecordData->pContext,
                    XA_RECORDEVENT_HEADSTALLED);
            }
            break;
        }
        case GST_MESSAGE_WARNING:
        {
            GError *pErr = NULL;
            gst_message_parse_warning(pMessage, &pErr, NULL);
            XA_LOGE(" WARNING from element %s: %s \n",
                GST_OBJECT_NAME(pMessage->src), pErr->message);
            if (pErr->domain == GST_RESOURCE_ERROR)
            {
                /* some Recording failed */
                g_mutex_lock(pXAMediaRecorder->pExitMutex);
                g_cond_signal(pXAMediaRecorder->pExitSema);
                g_mutex_unlock(pXAMediaRecorder->pExitMutex);
            }
            g_error_free(pErr);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState oldState;
            GstState newState;
            GstClock *pClock = NULL;

            gst_message_parse_state_changed(
                pMessage, &oldState, &newState, NULL);
            if (oldState == newState)
                break;

            // Check for toplevel bin state change
            if (GST_MESSAGE_SRC(pMessage) !=
                GST_OBJECT(pXAMediaRecorder->mRecorder))
                break;
            pClock = gst_system_clock_obtain();
            if (newState == GST_STATE_PLAYING)
            {
                if (pRecordData->mCallback &&
                    (pRecordData->mEventFlags &
                     XA_RECORDEVENT_HEADMOVING))
                {
                    pRecordData->mCallback(
                        IRecordItf,
                        pRecordData->pContext,
                        XA_RECORDEVENT_HEADMOVING);
                }
            }
            else if (newState == GST_STATE_PAUSED)
            {
                if (pXAMediaRecorder->mRecordState == XA_RECORDSTATE_PAUSED)
                {
                    pXAMediaRecorder->mRecordTime +=
                        (gst_clock_get_time(pClock) -
                        pXAMediaRecorder->mBaseTime);
                    pXAMediaRecorder->bPaused = XA_BOOLEAN_TRUE;
                    XA_LOGD("\n******record time is :%llu**********\n",
                        pXAMediaRecorder->mRecordTime);
                }

                if (pRecordData->mCallback &&
                    (pRecordData->mEventFlags &
                    XA_RECORDEVENT_HEADSTALLED))
                {
                    pRecordData->mCallback(
                        IRecordItf,
                        pRecordData->pContext,
                        XA_RECORDEVENT_HEADSTALLED);
                }
            }
            else if (newState == GST_STATE_NULL)
            {
                pXAMediaRecorder->mBaseTime = 0;
                pXAMediaRecorder->mRecordTime = 0;
            }
#if DUMP_MR_PIPELINE_GRAPH
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pXAMediaRecorder->mRecorder),
                GST_DEBUG_GRAPH_SHOW_ALL, "omxalmediarecordergraph");
#endif
            break;
        }
        case GST_MESSAGE_ELEMENT:
        {
            const GstStructure *pMsgStruct =
                gst_message_get_structure(pMessage);
            if (GST_MESSAGE_SRC(pMessage) ==
                GST_OBJECT(pXAMediaRecorder->mRecorder))
            {
                if (gst_structure_has_name(pMsgStruct, "image-done"))
                {
                    XAresult result = XA_RESULT_SUCCESS;
                    if (pSnapShotData->takenCallback)
                    {
                        pSnapShotData->takenCallback(
                            ISnapshotItf,
                            pSnapShotData->pContext,
                            1,
                            &pSnapShotData->mDataSink);
                    }
                    //Check the View finder is disabled
                    if (MediaRecorderViewFinderIsDisabled(pXAMediaRecorder))
                    {
                        result = ChangeGraphState(
                            pXAMediaRecorder->mRecorder, GST_STATE_NULL);
                        if (result != XA_RESULT_SUCCESS)
                            return XA_BOOLEAN_FALSE;
                    }
                }
            }
            else if (GST_MESSAGE_SRC(pMessage) ==
                GST_OBJECT(pXAMediaRecorder->pCameraSrc))
            {
                const GstStructure *pMsgStruct =
                    gst_message_get_structure(pMessage);
                if (gst_structure_has_name(pMsgStruct,
                    GST_PHOTOGRAPHY_AUTOFOCUS_DONE))
                {
                    CCameraDevice *pCameraDevice = NULL;
                    XADataLocator_IODevice *pIoDevice = NULL;
                    pIoDevice = (XADataLocator_IODevice *)
                        (pCMediaRecorder->mImageVideoSrc.pLocator);
                    if (pIoDevice)
                    {
                        pCameraDevice = (CCameraDevice *)(pIoDevice->device);
                        if (pCameraDevice)
                        {
                            return CameraDeviceAutoLocksCallback(pCameraDevice);
                        }
                    }
                }
            }
            break;
        }
        case GST_MESSAGE_APPLICATION:
        {
            const GstStructure *pMsgStruct =
                gst_message_get_structure(pMessage);
            if (gst_structure_has_name(pMsgStruct, "camera-is-ready"))
            {
                if (pSnapShotData->initiatedCallback)
                {
                    pSnapShotData->initiatedCallback(
                        ISnapshotItf, pSnapShotData->pContext);
                }
            }
            break;
        }
        case GST_MESSAGE_EOS:
        {
            GstElement *pSrcElement = GST_ELEMENT(GST_MESSAGE_SRC(pMessage));
            if (pSrcElement == pXAMediaRecorder->mRecorder)
            {
                XA_LOGD(" EOS from element %s: \n",
                    GST_OBJECT_NAME(pMessage->src));
                g_mutex_lock(pXAMediaRecorder->pExitMutex);
                g_cond_signal(pXAMediaRecorder->pExitSema);
                g_mutex_unlock(pXAMediaRecorder->pExitMutex);
            }
            break;
        }
        default:
            XA_LOGD("Unhandled message: %d", msgType);
            break;
    }

    return XA_BOOLEAN_TRUE;
}

static gpointer
MediaRecorderMainLoopRun(gpointer data)
{
    XAMediaRecorder *pXAMediaRecorder = (XAMediaRecorder *)data;
    pXAMediaRecorder->pLoop = g_main_loop_new(NULL, FALSE);
    XA_LOGD("*****gst_recorder_main_loop_run**** \n");
    if (pXAMediaRecorder->pLoop)
    {
        g_main_loop_run(pXAMediaRecorder->pLoop);
    }
    return pXAMediaRecorder->pLoopThread;
}

static void
MediaRecorderMainLoopQuit(gpointer data)
{
    XAMediaRecorder *pXAMediaRecorder = (XAMediaRecorder *)data;

    XA_ENTER_INTERFACE_VOID;
    if (g_main_loop_is_running(pXAMediaRecorder->pLoop))
    {
        g_main_loop_quit(pXAMediaRecorder->pLoop);
    }
    if (pXAMediaRecorder->pLoopThread)
        g_thread_join(pXAMediaRecorder->pLoopThread);
    pXAMediaRecorder->pLoopThread = NULL;
    XA_LOGD("****recorder main loop was quiting**** \n");

    XA_LEAVE_INTERFACE_VOID;
}

static XAresult
MediaRecorderFindElementInBin(
    CMediaRecorder *pCMediaRecorder,
    const XAchar *pElementName,
    const XAchar *pBinName,
    GstElement **ppElement)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstElement *pRecorderGraph = NULL;
    GstElement *pBin = NULL;
    gchar *pChildName = NULL;
    GstIterator *pIterator = NULL;
    XAboolean bFound = XA_BOOLEAN_FALSE;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    pBin = gst_bin_get_by_name((GstBin *)pRecorderGraph,
        (const gchar *)pBinName);
    if (pBin)
    {
        pIterator = gst_bin_iterate_elements((GstBin *)pBin);
        while (!bFound)
        {
            gpointer data;
            switch (gst_iterator_next(pIterator, &data))
            {
                case GST_ITERATOR_OK:
                {
                    GstElement *pChild = GST_ELEMENT_CAST(data);
                    pChildName = gst_element_get_name(pChild);
                    if (pChildName == NULL)
                    {
                        result = XA_RESULT_INTERNAL_ERROR;
                        XA_LEAVE_INTERFACE;
                    }

                    if (!strcasecmp(pChildName, (const char *)pElementName))
                    {
                        *ppElement = pChild;
                        bFound = XA_BOOLEAN_TRUE;
                    }
                    g_free(pChildName);
                    pChildName = NULL;
                    gst_object_unref(pChild);
                    break;
                }
                case GST_ITERATOR_RESYNC:
                    gst_iterator_resync(pIterator);
                    break;
                default:
                case GST_ITERATOR_DONE:
                    bFound = XA_BOOLEAN_TRUE;
                    break;
            }
        }
        gst_iterator_free(pIterator);
    }

    XA_LEAVE_INTERFACE;
}

static GstPadLinkReturn
MediaRecorderLinkEncodeBin(
    XAMediaRecorder *pXAMediaRecorder,
    GstElement *pElement)
{
    GstPadLinkReturn ret = GST_PAD_LINK_WRONG_HIERARCHY;
    GstPad *pSrcpad = NULL;
    GstPad *pSinkpad = NULL;
    GstElementClass *pElementClass = NULL;

    pSrcpad = gst_element_get_static_pad(pElement, "src");
    pElementClass = GST_ELEMENT_GET_CLASS(pXAMediaRecorder->pAudioEncodeBin);
    if (!pXAMediaRecorder->pPadTemplate)
    {
        pXAMediaRecorder->pPadTemplate =
            gst_element_class_get_pad_template(pElementClass, "audio_%d");
    }
    pSinkpad = gst_element_request_pad(
        pXAMediaRecorder->pAudioEncodeBin,
        pXAMediaRecorder->pPadTemplate,
        NULL,
        pXAMediaRecorder->pAudioCaps);
    if (pSrcpad && pSinkpad)
    {
        ret = gst_pad_link(pSrcpad, pSinkpad);
        gst_object_unref(pSinkpad);
        gst_object_unref(pSrcpad);
    }

    return ret;
}

static XAresult
MediaRecorderCreateProfile(XAMediaRecorder *pXAMediaRecorder)
{
    GstPadLinkReturn padLinkResult = GST_PAD_LINK_WRONG_HIERARCHY;
    gboolean res = FALSE;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pXAMediaRecorder);
    //create the container profile based on video and audio encoder id
    if (pXAMediaRecorder->pContainerCaps)
    {
        pXAMediaRecorder->pContainerProfile =
            gst_encoding_container_profile_new(
                (gchar *)"test-application-profile",
                NULL,
                pXAMediaRecorder->pContainerCaps,
                NULL);
    }

    if ((pXAMediaRecorder->pAudioEncodeBin == NULL)
        && (pXAMediaRecorder->pVideoProfile))
    {
        g_object_set(
            G_OBJECT(pXAMediaRecorder->mRecorder),
            "video-capture-caps",
            pXAMediaRecorder->pVideoCaps,
            NULL);
        res = gst_encoding_container_profile_add_profile(
            pXAMediaRecorder->pContainerProfile,
            (GstEncodingProfile *)pXAMediaRecorder->pVideoProfile);
        if (res == FALSE)
        {
            result = XA_RESULT_INTERNAL_ERROR;
            XA_LEAVE_INTERFACE;
        }
    }

    if (pXAMediaRecorder->pAudioProfile)
    {
        if (pXAMediaRecorder->pAudioEncodeBin)
        {
            g_object_set(
                G_OBJECT(pXAMediaRecorder->pAudioCapsfilterElem),
                "caps",
                pXAMediaRecorder->pAudioCaps,
                NULL);
        }
        else
        {
            g_object_set(
                G_OBJECT(pXAMediaRecorder->mRecorder),
                "audio-capture-caps",
                pXAMediaRecorder->pAudioCaps,
                NULL);
        }
        res = gst_encoding_container_profile_add_profile(
            pXAMediaRecorder->pContainerProfile,
            (GstEncodingProfile *)pXAMediaRecorder->pAudioProfile);
        if (res == FALSE)
        {
            result = XA_RESULT_INTERNAL_ERROR;
            XA_LEAVE_INTERFACE;
        }
    }

    //Set the profile for the graph
    if (pXAMediaRecorder->pAudioEncodeBin)
    {
        if (pXAMediaRecorder->bUpdateRequired)
        {
            g_object_set(
                G_OBJECT(pXAMediaRecorder->pAudioEncodeBin),
                "profile",
                (GstEncodingProfile *)pXAMediaRecorder->pContainerProfile,
                NULL);
            pXAMediaRecorder->bUpdateRequired = XA_BOOLEAN_FALSE;
        }

        padLinkResult = MediaRecorderLinkEncodeBin(
            pXAMediaRecorder,
            pXAMediaRecorder->pAudioConvertElem);
        if (padLinkResult != GST_PAD_LINK_OK)
        {
            result = XA_RESULT_INTERNAL_ERROR;
            XA_LEAVE_INTERFACE;
        }
    }
    else
    {
        g_object_set(
            G_OBJECT(pXAMediaRecorder->mRecorder),
            "video-profile",
            (GstEncodingProfile*)pXAMediaRecorder->pContainerProfile,
            NULL);
    }

    XA_LEAVE_INTERFACE;
}

static XAresult
MediaRecorderSetProperties(
    CMediaRecorder *pCMediaRecorder)
{
    GstElement *pElement = NULL;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntParamSpec = NULL;
    XAAudioEncoderSettings *pAudioSettings = NULL;
    XAVideoSettings *pVideoSettings = NULL;
    XAMediaRecorder *pXAMediaRecorder = NULL;
    XAuint16 rateControl = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pAudioSettings = (XAAudioEncoderSettings *)
        pCMediaRecorder->mAudioEncoder.pData;
    pVideoSettings = (XAVideoSettings *)pCMediaRecorder->mVideoEncoder.pData;
    AL_CHK_ARG(pAudioSettings && pVideoSettings);

    // set audio properties
    switch (pAudioSettings->encoderId)
    {
        case XA_AUDIOCODEC_AAC:
        {
            MediaRecorderFindElementInBin(pCMediaRecorder,
                (const XAchar *)"omxaacenc0",
                (const XAchar *)"encodebin0", &pElement);
            break;
        }
        case XA_AUDIOCODEC_AMR:
        {
            MediaRecorderFindElementInBin(pCMediaRecorder,
                (const XAchar *)"omxamrnbenc0",
                (const XAchar *)"encodebin0", &pElement);
            break;
        }
        case XA_AUDIOCODEC_AMRWB:
        {
            MediaRecorderFindElementInBin(pCMediaRecorder,
                (const XAchar *)"omxamrwbenc0",
                (const XAchar *)"encodebin0", &pElement);
            break;
        }
        default:
        {
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
        }
    }

    if (pElement)
    {
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pElement), "bitrate");
        if (pParamSpec)
        {
            pIntParamSpec = (GParamSpecInt *)pParamSpec;
            if ((pIntParamSpec->minimum < pAudioSettings->bitRate) &&
                (pIntParamSpec->maximum > pAudioSettings->bitRate))
            {
                g_object_set(G_OBJECT(pElement),
                    "bitrate", pAudioSettings->bitRate, NULL);
            }
        }
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pElement), "output-format");
        if (pParamSpec)
        {
            g_object_set(G_OBJECT(pElement),
                "output-format", pAudioSettings->streamFormat, NULL);
        }
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pElement), "profile");
        if (pParamSpec)
        {
            g_object_set(G_OBJECT(pElement),
                "profile", pAudioSettings->levelSetting, NULL);
        }
    }
    // set video properties
    pElement = NULL;
    // set video properties in video recording usecase and not in
    // audio only recoridng usecase
    if (pXAMediaRecorder->pAudioEncodeBin == NULL)
    {
        switch (pVideoSettings->encoderId)
        {
            case XA_VIDEOCODEC_H263:
            {
                MediaRecorderFindElementInBin(pCMediaRecorder,
                    (const XAchar *)"omxh263enc0",
                    (const XAchar *)"encodebin0", &pElement);
                break;
            }
            case XA_VIDEOCODEC_AVC:
            {
                MediaRecorderFindElementInBin(pCMediaRecorder,
                    (const XAchar *)"omxh264enc0",
                    (const XAchar *)"encodebin0", &pElement);
                break;
            }
            case XA_VIDEOCODEC_MPEG4:
            {
                MediaRecorderFindElementInBin(pCMediaRecorder,
                    (const XAchar *)"omxmpeg4enc0",
                    (const XAchar *)"encodebin0", &pElement);
                break;
            }
            default:
            {
                result = XA_RESULT_CONTENT_UNSUPPORTED;
                XA_LEAVE_INTERFACE;
            }
        }
    }
    if (pElement)
    {
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pElement), "bitrate");
        if (pParamSpec)
        {
            pIntParamSpec = (GParamSpecInt *)pParamSpec;
            if ((pIntParamSpec->minimum < pVideoSettings->bitRate) &&
                (pIntParamSpec->maximum > pVideoSettings->bitRate))
            {
                g_object_set(G_OBJECT(pElement),
                    "bitrate", pVideoSettings->bitRate, NULL);
            }
        }
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pElement), "rc-mode");
        if (pParamSpec)
        {
            if (XA_RATECONTROLMODE_CONSTANTBITRATE ==
                pVideoSettings->rateControl)
            {
                rateControl = 0;
            }
            else if (XA_RATECONTROLMODE_VARIABLEBITRATE ==
                pVideoSettings->rateControl)
            {
                rateControl = 1;
            }
            g_object_set(G_OBJECT(pElement),
                "rc-mode", rateControl, NULL);
        }
        pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pElement), "iframeinterval");
        if (pParamSpec)
        {
            pIntParamSpec = (GParamSpecInt *)pParamSpec;
            if ((pIntParamSpec->minimum <= pVideoSettings->keyFrameInterval) &&
                (pIntParamSpec->maximum >= pVideoSettings->keyFrameInterval))
            {
                g_object_set(G_OBJECT(pElement),
                    "iframeinterval", pVideoSettings->keyFrameInterval, NULL);
            }
        }
    }

    XA_LEAVE_INTERFACE;
}

static XAresult
MediaRecorderCreateAudioRecordBin(
    CMediaRecorder *pCMediaRecorder)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    gboolean success = FALSE;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;

    //Create Recorder Pipeline
    pXAMediaRecorder->mRecorder = gst_pipeline_new("OMXALAudioRecorder");
    if (pXAMediaRecorder->mRecorder == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create AudioSrc element
    pXAMediaRecorder->pAudioSrcElem =
        gst_element_factory_make("pulsesrc", "audiosrc");
    if (pXAMediaRecorder->pAudioSrcElem == NULL)
    {
        pXAMediaRecorder->pAudioSrcElem =
            gst_element_factory_make("autoaudiosrc", "audiosrc");
        if (pXAMediaRecorder->pAudioSrcElem == NULL)
        {
            pXAMediaRecorder->pAudioSrcElem =
                gst_element_factory_make("alsasrc", "audiosrc");
        }
    }
    if (pXAMediaRecorder->pAudioSrcElem == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create Audio Volume element
    pXAMediaRecorder->pAudioVolumeElem =
        gst_element_factory_make("volume", "audiovolume");
    if (pXAMediaRecorder->pAudioVolumeElem == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create Audio Convert element
    pXAMediaRecorder->pAudioConvertElem =
        gst_element_factory_make("audioconvert", "audioconvert");
    if (pXAMediaRecorder->pAudioConvertElem == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create Audio Caps filter element
    pXAMediaRecorder->pAudioCapsfilterElem =
        gst_element_factory_make("capsfilter", "audiocapsfilter");
    if (pXAMediaRecorder->pAudioCapsfilterElem == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create File Sink element
    pXAMediaRecorder->pFilesinkElem =
        gst_element_factory_make("filesink", "filesink");
    if (pXAMediaRecorder->pFilesinkElem == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Create EncodeBin element
    pXAMediaRecorder->pAudioEncodeBin = gst_element_factory_make(
        "encodebin", "encodebin0");
    if (pXAMediaRecorder->pAudioEncodeBin == NULL)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    //Add all elements to recorder Pipeline
    gst_bin_add_many(
        GST_BIN(pXAMediaRecorder->mRecorder),
        pXAMediaRecorder->pAudioSrcElem,
        pXAMediaRecorder->pAudioCapsfilterElem,
        pXAMediaRecorder->pAudioConvertElem,
        pXAMediaRecorder->pAudioVolumeElem,
        pXAMediaRecorder->pAudioEncodeBin,
        pXAMediaRecorder->pFilesinkElem,
        NULL);

    success = gst_element_link(
        pXAMediaRecorder->pAudioEncodeBin,
        pXAMediaRecorder->pFilesinkElem);
    if (success == FALSE)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    success = gst_element_link_many(
        pXAMediaRecorder->pAudioSrcElem,
        pXAMediaRecorder->pAudioVolumeElem,
        pXAMediaRecorder->pAudioCapsfilterElem,
        pXAMediaRecorder->pAudioConvertElem,
        NULL);
    if (success == FALSE)
    {
        result = XA_RESULT_INTERNAL_ERROR;
        XA_LEAVE_INTERFACE;
    }

    pXAMediaRecorder->pBus = gst_element_get_bus(pXAMediaRecorder->mRecorder);
    if (pXAMediaRecorder->pBus)
    {
        gst_bus_add_signal_watch(pXAMediaRecorder->pBus);
        pXAMediaRecorder->mSigBusAsync =
            g_signal_connect(pXAMediaRecorder->pBus, "message",
            G_CALLBACK(MediaRecorderGstBusCallback),
            pCMediaRecorder);
    }
    else
    {
        result = XA_RESULT_INTERNAL_ERROR;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderCreate(CMediaRecorder *pCMediaRecorder)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pCMediaRecorder);

    InitializeFramework();

    pXAMediaRecorder =
        (XAMediaRecorder *)malloc(sizeof(XAMediaRecorder));
    if (pXAMediaRecorder)
    {
        memset(pXAMediaRecorder, 0, sizeof(XAMediaRecorder));
        pXAMediaRecorder->pRecordThreadMutex = g_mutex_new();
        pXAMediaRecorder->pRecordThreadSema = g_cond_new();
        pXAMediaRecorder->pExitMutex = g_mutex_new();
        pXAMediaRecorder->pExitSema = g_cond_new();
        pXAMediaRecorder->pStateChangeMutex = g_mutex_new();
        pXAMediaRecorder->pStateChangeSema = g_cond_new();
        pCMediaRecorder->pData = pXAMediaRecorder;
    }
    else
    {
        result = XA_RESULT_MEMORY_FAILURE;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderRealize(
    CMediaRecorder *pCMediaRecorder,
    XAboolean async)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    CCameraDevice *pCameraDevice = NULL;
    CameraDeviceData *pCamData = NULL;
    XADataLocator_IODevice *pIoDevice = NULL;
    XADataLocator_URI *pUri = NULL;
    XADataFormat_MIME *pMime = NULL;
    RecordData *pRecordData = NULL;
    XAAudioEncoderSettings *pAudioSettings = NULL;
    XAVideoSettings *pVideoSettings = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);

    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecordData = (RecordData *)pCMediaRecorder->mRecord.pData;
    AL_CHK_ARG(pRecordData);
    pAudioSettings = (XAAudioEncoderSettings *)
        pCMediaRecorder->mAudioEncoder.pData;
    pVideoSettings = (XAVideoSettings *)pCMediaRecorder->mVideoEncoder.pData;
    AL_CHK_ARG(pAudioSettings && pVideoSettings);

    //set data sink
    pUri = (XADataLocator_URI *)(pCMediaRecorder->mDataSnk.pLocator);
    pMime = (XADataFormat_MIME *)(pCMediaRecorder->mDataSnk.pFormat);

    pXAMediaRecorder->bUpdateRequired = XA_BOOLEAN_TRUE;
    //set location and container format
    if (pXAMediaRecorder->pContainerCaps)
        gst_caps_unref(pXAMediaRecorder->pContainerCaps);

    if (pUri)
    {
        switch (pUri->locatorType) // locatorType
        {
            case XA_DATALOCATOR_URI:
                // check the mime type
                if ((pMime->formatType) != XA_DATAFORMAT_MIME) //formatType
                {
                    result = XA_RESULT_PARAMETER_INVALID;
                    XA_LEAVE_INTERFACE;
                }
                else
                {
                    // set the output format
                    switch (pMime->containerType)
                    {
                        case XA_CONTAINERTYPE_3GPP:
                        case XA_CONTAINERTYPE_MP4:
                        case XA_CONTAINERTYPE_M4A:
                        {
                            pXAMediaRecorder->pContainerCaps =
                                gst_caps_new_simple("video/quicktime",
                                "variant", G_TYPE_STRING, "iso",
                                NULL);
                            break;
                        }
                        case XA_CONTAINERTYPE_AMR:
                        {
                            pXAMediaRecorder->pContainerCaps =
                                gst_caps_new_simple("audio/x-amr-nb-sh",
                                NULL);
                            break;
                        }
                        case XA_CONTAINERTYPE_OGG:
                        {
                            pXAMediaRecorder->pContainerCaps =
                                gst_caps_new_simple("application/ogg",
                                NULL);
                            break;
                        }
                        default:
                            XA_LOGE("UNSUPPORTED FORMAT\n");
                            result = XA_RESULT_PARAMETER_INVALID;
                            XA_LEAVE_INTERFACE;
                    }
                }
                break;
            default :
                // for these data locator types, ignore the pFormat
                // as it might be uninitialized
                XA_LOGE("Error: Sink not suported\n");
                result = XA_RESULT_PARAMETER_INVALID;
                XA_LEAVE_INTERFACE;
        }
    }
    else
    {
        //set default container
        pXAMediaRecorder->pContainerCaps =
            gst_caps_new_simple("video/quicktime",
            "variant", G_TYPE_STRING, "iso",
            NULL);
    }

    //set video source
    if (pCMediaRecorder->mImageVideoSrc.pLocator != NULL)
    {
        pIoDevice = (XADataLocator_IODevice *)
            (pCMediaRecorder->mImageVideoSrc.pLocator);
        AL_CHK_ARG(pIoDevice);
        if (pIoDevice->locatorType == XA_DATALOCATOR_IODEVICE &&
            (pIoDevice->deviceType == XA_IODEVICE_CAMERA))
        {
            //check the device type
            pCameraDevice = (CCameraDevice *)(pIoDevice->device);
            AL_CHK_ARG(pCameraDevice);
            pCamData = (CameraDeviceData *)(pCameraDevice->pData);
            pXAMediaRecorder->mRecorder = pCamData->pCamerabin;
            pXAMediaRecorder->pCameraSrc = pCamData->pCameraSrc;
            pXAMediaRecorder->pBus = gst_element_get_bus(pCamData->pCamerabin);
            if (pXAMediaRecorder->pBus)
            {
                gst_bus_add_signal_watch(pXAMediaRecorder->pBus);
                pXAMediaRecorder->mSigBusAsync =
                    g_signal_connect(pXAMediaRecorder->pBus, "message",
                    G_CALLBACK(MediaRecorderGstBusCallback),
                    pCMediaRecorder);
            }
            else
            {
                result = XA_RESULT_INTERNAL_ERROR;
                XA_LEAVE_INTERFACE;
            }
            //Create Video Profile
            result = ALBackendMediaRecorderSetVideoSettings(
                pCMediaRecorder, pVideoSettings);

            //Set Mode to either IMAGE or VIDEO MODE
            if (pCMediaRecorder->mode == CMEDIARECORDER_SNAPSHOT)
            {
                g_object_set(G_OBJECT(pXAMediaRecorder->mRecorder), "mode",
                    CAPTURE_MODE, NULL);
            }
            else if (pCMediaRecorder->mode == CMEDIARECORDER_RECORD)
            {
                g_object_set(G_OBJECT(pXAMediaRecorder->mRecorder), "mode",
                    RECORDING_MODE, NULL);
            }
            else
            {
                XA_LOGE("UNSUPPORTED Capture Mode %d\n",
                    pCMediaRecorder->mode);
                result = XA_RESULT_PARAMETER_INVALID;
                XA_LEAVE_INTERFACE;
            }
        }
        else
        {
            XA_LOGE("UNSUPPORTED DATA SOURCE\n");
            result = XA_RESULT_PARAMETER_INVALID;
            XA_LEAVE_INTERFACE;
        }
    } //set video source

    if (pCMediaRecorder->mAudioSrc.pLocator != NULL)
    {
        if (pCMediaRecorder->mImageVideoSrc.pLocator == NULL)
        {
            result = MediaRecorderCreateAudioRecordBin(pCMediaRecorder);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LEAVE_INTERFACE;
            }
        }
        else
        {
            //Create AudioSrc element
            pXAMediaRecorder->pAudioSrcElem =
                gst_element_factory_make("pulsesrc", "audiosrc");
            if (pXAMediaRecorder->pAudioSrcElem == NULL)
            {
                pXAMediaRecorder->pAudioSrcElem =
                    gst_element_factory_make("autoaudiosrc", "audiosrc");
                if (pXAMediaRecorder->pAudioSrcElem == NULL)
                {
                    pXAMediaRecorder->pAudioSrcElem =
                        gst_element_factory_make("alsasrc", "audiosrc");
                }
            }
            if (pXAMediaRecorder->pAudioSrcElem == NULL)
            {
                result = XA_RESULT_INTERNAL_ERROR;
                XA_LEAVE_INTERFACE;
            }
            g_object_set(G_OBJECT(pXAMediaRecorder->mRecorder),
                "audio-source", pXAMediaRecorder->pAudioSrcElem, NULL);
        }
        //Create Audio Profile
        result = ALBackendMediaRecorderSetAudioSettings(
            pCMediaRecorder, pAudioSettings);
    }

    XA_LEAVE_INTERFACE;
}

void ALBackendMediaRecorderDestroy(CMediaRecorder *pCMediaRecorder)
{
    XA_ENTER_INTERFACE_VOID;

    if (pCMediaRecorder)
    {
        if (pCMediaRecorder->pData)
        {
            XAMediaRecorder *pXAMediaRecorder =
                (XAMediaRecorder *)pCMediaRecorder->pData;
            if (pXAMediaRecorder->pContainerProfile)
                gst_encoding_profile_unref(pXAMediaRecorder->pContainerProfile);
            if (pXAMediaRecorder->pAudioProfile)
                gst_encoding_profile_unref(pXAMediaRecorder->pAudioProfile);
            if (pXAMediaRecorder->pVideoProfile)
                gst_encoding_profile_unref(pXAMediaRecorder->pVideoProfile);
            //Destroy audiorecorderbin
            if (pXAMediaRecorder->pAudioEncodeBin)
            {
                gst_bin_remove_many(
                    (GstBin *)pXAMediaRecorder->mRecorder,
                    pXAMediaRecorder->pAudioCapsfilterElem,
                    pXAMediaRecorder->pAudioConvertElem,
                    pXAMediaRecorder->pAudioSrcElem,
                    pXAMediaRecorder->pAudioVolumeElem,
                    pXAMediaRecorder->pFilesinkElem,
                    pXAMediaRecorder->pAudioEncodeBin,
                    NULL);
                if (pXAMediaRecorder->mRecorder)
                    gst_object_unref(pXAMediaRecorder->mRecorder);
            }
            else if (pXAMediaRecorder->pAudioSrcElem)
            {
                gst_object_unref(pXAMediaRecorder->pAudioSrcElem);
            }

            //Destroy memory allocation like caps
            if (pXAMediaRecorder->pVideoCaps)
                gst_caps_unref(pXAMediaRecorder->pVideoCaps);
            if (pXAMediaRecorder->pAudioCaps)
                gst_caps_unref(pXAMediaRecorder->pAudioCaps);
            if (pXAMediaRecorder->pContainerCaps)
                gst_caps_unref(pXAMediaRecorder->pContainerCaps);
            if (pXAMediaRecorder->pPadTemplate)
                gst_object_unref(pXAMediaRecorder->pPadTemplate);
            if (pXAMediaRecorder->pBus)
                gst_object_unref(pXAMediaRecorder->pBus);

            //stop the periodic update thread
            pXAMediaRecorder->bStopThread = XA_BOOLEAN_TRUE;
            //signal semaphore to break thread
            if (pXAMediaRecorder->pRecordThreadSema)
            {
                g_mutex_lock(pXAMediaRecorder->pRecordThreadMutex);
                g_cond_signal(pXAMediaRecorder->pRecordThreadSema);
                g_mutex_unlock(pXAMediaRecorder->pRecordThreadMutex);
            }
            if (pXAMediaRecorder->pStateChangeSema)
            {
                g_mutex_lock(pXAMediaRecorder->pStateChangeMutex);
                g_cond_signal(pXAMediaRecorder->pStateChangeSema);
                g_mutex_unlock(pXAMediaRecorder->pStateChangeMutex);
            }
            if (pXAMediaRecorder->pCallbackHandle)
                g_thread_join(pXAMediaRecorder->pCallbackHandle);
            //Destroy semaphore
            if (pXAMediaRecorder->pRecordThreadMutex)
                g_mutex_free(pXAMediaRecorder->pRecordThreadMutex);
            if (pXAMediaRecorder->pRecordThreadSema)
                g_cond_free(pXAMediaRecorder->pRecordThreadSema);
            //Destroy semaphore
            if (pXAMediaRecorder->pExitSema)
                g_cond_free(pXAMediaRecorder->pExitSema);
            if (pXAMediaRecorder->pExitMutex)
                g_mutex_free(pXAMediaRecorder->pExitMutex);
            //Destroy semaphore
            if (pXAMediaRecorder->pStateChangeSema)
                g_cond_free(pXAMediaRecorder->pStateChangeSema);
            if (pXAMediaRecorder->pStateChangeMutex)
                g_mutex_free(pXAMediaRecorder->pStateChangeMutex);

            free(pCMediaRecorder->pData);
            pCMediaRecorder->pData = NULL;
        }
    }

    XA_LEAVE_INTERFACE_VOID;
}

XAresult
ALBackendMediaRecorderResume(
    CMediaRecorder *pCMediaRecorder,
    XAboolean async)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    if (GST_STATE(pXAMediaRecorder->mRecorder) != GST_STATE_PLAYING)
        result = ALBackendMediaRecorderSetRecordState(
            pCMediaRecorder, XA_RECORDSTATE_RECORDING);

    XA_LEAVE_INTERFACE;
}

XAresult ALBackendMediaRecorderGetClockTime(
    CMediaRecorder *pCMediaRecorder,
    XAmillisecond *pMsec)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstClock *pClock = NULL;
    gint64 position = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData && pMsec);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pClock = gst_system_clock_obtain();
    if (pXAMediaRecorder->mRecordTime)
    {
        if (pXAMediaRecorder->bPaused)
            position = pXAMediaRecorder->mRecordTime;
        else
            position = pXAMediaRecorder->mRecordTime +
                (gst_clock_get_time(pClock) - pXAMediaRecorder->mBaseTime);
    }
    else
    {
        position = gst_clock_get_time(pClock) - pXAMediaRecorder->mBaseTime;
    }

    if (position < 0)
        *pMsec = XA_TIME_UNKNOWN;
    else
        *pMsec = (XAmillisecond)(position / GST_MSECOND);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderSetRecordState(
    CMediaRecorder *pCMediaRecorder,
    XAuint32 recordState)
{
    RecordData *pRecordData = NULL;
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstElement *pRecorderGraph = NULL;
    GTimeVal ts;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pRecordData = (RecordData *)pCMediaRecorder->mRecord.pData;
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    AL_CHK_ARG(pRecorderGraph && pRecordData);

    pXAMediaRecorder->mRecordState = recordState;
    XA_LOGD("\n Move Record state to :%u \n", recordState);
    switch (recordState)
    {
        case XA_RECORDSTATE_STOPPED:
        {
            gboolean bIdle = FALSE;
            //Stop-Capture
            if (pXAMediaRecorder->pAudioEncodeBin)
            {
                if (pXAMediaRecorder->pAudioSrcElem)
                {
                    gst_element_send_event(
                        pXAMediaRecorder->pAudioSrcElem,
                        gst_event_new_eos());
                }
                //Waiting for EOS to be generated from sink elements
                g_get_current_time(&ts);
                g_time_val_add(&ts, (glong)5000000);
                g_mutex_lock(pXAMediaRecorder->pExitMutex);
                g_cond_timed_wait(
                    pXAMediaRecorder->pExitSema,
                    pXAMediaRecorder->pExitMutex,
                    (GTimeVal *)&ts);
                g_mutex_unlock(pXAMediaRecorder->pExitMutex);
            }
            else
            {
                g_signal_emit_by_name(
                    G_OBJECT(pRecorderGraph), "stop-capture", 0);
                do
                {
                    g_object_get(
                        G_OBJECT(pRecorderGraph), "idle", &bIdle, NULL);
                } while (bIdle == FALSE);
            }
            pXAMediaRecorder->mRecordTime = 0;

            //Check the View finder is disabled
            if (pXAMediaRecorder->pAudioEncodeBin ||
                MediaRecorderViewFinderIsDisabled(pXAMediaRecorder))
            {
                if (pXAMediaRecorder->pAudioEncodeBin)
                {
                    GstPad *pSrcPad = NULL;
                    GstPad *pSinkPad = NULL;

                    pSrcPad = gst_element_get_static_pad(
                        pXAMediaRecorder->pAudioConvertElem, "src");
                    pSinkPad = gst_pad_get_peer(pSrcPad);
                    if (pSinkPad)
                    {
                        /* Remove Encodebin request sink pad */
                        gst_pad_unlink(pSrcPad, pSinkPad);
                        gst_element_release_request_pad(
                            pXAMediaRecorder->pAudioEncodeBin, pSinkPad);
                        gst_object_unref(pSinkPad);
                    }
                    gst_object_unref(pSrcPad);
                }
                result = ChangeGraphState(pRecorderGraph, GST_STATE_NULL);
            }
            MediaRecorderMainLoopQuit(pXAMediaRecorder);
            break;
        }
        case XA_RECORDSTATE_PAUSED:
        {
            result = ChangeGraphState(pRecorderGraph, GST_STATE_PAUSED);
            break;
        }
        case XA_RECORDSTATE_RECORDING:
        {
            XADataLocator_URI *pUri = NULL;
            gchar *pFilename = NULL;
            gchar *pFilenameSuffix = (gchar *)".mp4";
            GstClock *pClock = NULL;

            // Set Record Mode for Recorder
            g_object_set(G_OBJECT(pRecorderGraph), "mode", RECORDING_MODE, NULL);

            if ((pXAMediaRecorder->bUpdateRequired) &&
                (GST_STATE(pRecorderGraph) == GST_STATE_PLAYING))
            {
                XA_LOGD("\n******Moving compnents state to NULL**** \n");
                result = ChangeGraphState(pRecorderGraph, GST_STATE_NULL);
                if (result != XA_RESULT_SUCCESS)
                {
                    XA_LEAVE_INTERFACE;
                }
                pXAMediaRecorder->bUpdateRequired = XA_BOOLEAN_FALSE;
             }
            if (GST_STATE(pRecorderGraph) == GST_STATE_NULL)
            {
                //set the location
                pUri = (XADataLocator_URI*)pCMediaRecorder->mDataSnk.pLocator;
                if (pUri)
                {
                    pFilename = g_strdup_printf("%s%04u%s", pUri->URI,
                        (unsigned int)pXAMediaRecorder->mFileCount, pFilenameSuffix);
                }
                else
                {
                    pFilename = g_strdup_printf("%s%04u%s", "temp",
                        (unsigned int)pXAMediaRecorder->mFileCount, pFilenameSuffix);
                }
                if (pXAMediaRecorder->pAudioEncodeBin)
                {
                    g_object_set(
                        G_OBJECT(pXAMediaRecorder->pFilesinkElem),
                        "location",
                        pFilename,
                        NULL);
                }
                else
                {
                    g_object_set(
                        G_OBJECT(pRecorderGraph), "location", pFilename, NULL);
                }
                g_free(pFilename);
                pXAMediaRecorder->mFileCount++;
            }
            if (!pXAMediaRecorder->pLoopThread)
            {
                pXAMediaRecorder->pLoopThread = g_thread_create(
                    (GThreadFunc)MediaRecorderMainLoopRun,
                    pXAMediaRecorder, TRUE, NULL);
                if (!pXAMediaRecorder->pLoopThread)
                {
                    XA_LOGE("\n Creating Main loop thread failed!!\n");
                    result = XA_RESULT_INTERNAL_ERROR;
                    XA_LEAVE_INTERFACE;
                }
            }
            if (GST_STATE(pRecorderGraph) == GST_STATE_NULL)
            {
                result = MediaRecorderCreateProfile(pXAMediaRecorder);
                if (result != XA_RESULT_SUCCESS)
                {
                    XA_LEAVE_INTERFACE;
                }
                 result = ChangeGraphState(pRecorderGraph, GST_STATE_READY);
                if (result != XA_RESULT_SUCCESS)
                {
                    XA_LEAVE_INTERFACE;
                }
                result = MediaRecorderSetProperties(pCMediaRecorder);
                if (result != XA_RESULT_SUCCESS)
                {
                    XA_LEAVE_INTERFACE;
                }
            }
            result = ChangeGraphState(pRecorderGraph, GST_STATE_PLAYING);
            if (result != XA_RESULT_SUCCESS)
            {
                XA_LEAVE_INTERFACE;
            }

            //Start-Capture
            if (pXAMediaRecorder->pAudioEncodeBin == NULL)
            {
                g_signal_emit_by_name(
                    G_OBJECT(pRecorderGraph), "start-capture", 0);
            }
            pClock = gst_system_clock_obtain();
            pXAMediaRecorder->mBaseTime = gst_clock_get_time(pClock);
            pXAMediaRecorder->bPaused = XA_BOOLEAN_FALSE;
            g_mutex_lock(pXAMediaRecorder->pStateChangeMutex);
            g_cond_signal(pXAMediaRecorder->pStateChangeSema);
            g_mutex_unlock(pXAMediaRecorder->pStateChangeMutex);
            if ((pXAMediaRecorder->pCallbackHandle == NULL) &&
                (pXAMediaRecorder->mRecordState ==
                XA_RECORDSTATE_RECORDING))
            {
                XA_LOGD("\n ********Marker and Periodic Update"
                    "Thread is created******* \n");
                pXAMediaRecorder->pCallbackHandle = g_thread_create(
                    (GThreadFunc)&TimedCallBackThread,
                    (void *)pCMediaRecorder, TRUE, NULL);
            }
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult ALBackendMediaRecorderSetVideoSettings(
    CMediaRecorder *pCMediaRecorder,
    XAVideoSettings *pVideoSettings)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstCaps *pVideoCaps = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData && pVideoSettings);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;

    switch (pVideoSettings->encoderId)
    {
        case XA_VIDEOCODEC_H263:
        {
            pVideoCaps = gst_caps_new_simple("video/x-h263",
                "framerate", GST_TYPE_FRACTION,
                (pVideoSettings->frameRate >> 16), 1,
                "width", G_TYPE_INT, pVideoSettings->width,
                "height", G_TYPE_INT, pVideoSettings->height,
                NULL);
            break;
        }
        case XA_VIDEOCODEC_AVC:
        {
            pVideoCaps = gst_caps_new_simple ("video/x-h264",
                "stream-format", G_TYPE_STRING, "avc",
                "alignment", G_TYPE_STRING, "au",
                "framerate", GST_TYPE_FRACTION,
                (pVideoSettings->frameRate >> 16), 1,
                "width", G_TYPE_INT, pVideoSettings->width,
                "height", G_TYPE_INT, pVideoSettings->height,
                NULL);
            break;
        }
        case XA_VIDEOCODEC_MPEG4:
        {
            pVideoCaps = gst_caps_new_simple("video/mpeg",
                "mpegversion", G_TYPE_INT, 4,
                "framerate", GST_TYPE_FRACTION,
                (pVideoSettings->frameRate >> 16), 1,
                "width", G_TYPE_INT, pVideoSettings->width,
                "height", G_TYPE_INT, pVideoSettings->height,
                NULL);
            break;
        }
        default:
            result = XA_RESULT_CONTENT_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
    }

    if (pXAMediaRecorder->pVideoCaps)
        gst_caps_unref(pXAMediaRecorder->pVideoCaps);

    pXAMediaRecorder->pVideoCaps = gst_caps_new_simple("video/x-nv-yuv",
        "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('I', '4', '2', '0'),
        "framerate", GST_TYPE_FRACTION, (pVideoSettings->frameRate >> 16), 1,
        "width", G_TYPE_INT, pVideoSettings->width,
        "height", G_TYPE_INT, pVideoSettings->height,
        NULL);

    if (pVideoCaps)
    {
        if (pXAMediaRecorder->pVideoProfile)
            gst_encoding_profile_unref(pXAMediaRecorder->pVideoProfile);

        pXAMediaRecorder->pVideoProfile = (GstEncodingVideoProfile *)
            gst_encoding_video_profile_new(pVideoCaps, NULL, NULL, 0);
        gst_encoding_video_profile_set_variableframerate(
            pXAMediaRecorder->pVideoProfile, TRUE);
        gst_caps_unref(pVideoCaps);
    }
    pXAMediaRecorder->bUpdateRequired = XA_BOOLEAN_TRUE;

    XA_LEAVE_INTERFACE;
}

XAresult ALBackendMediaRecorderSetAudioSettings(
    CMediaRecorder *pCMediaRecorder,
    XAAudioEncoderSettings *pAudioSettings)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstCaps *pAudioCaps = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData && pAudioSettings);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;

    switch (pAudioSettings->encoderId)
    {
        case XA_AUDIOCODEC_AAC:
        {
            pAudioCaps = gst_caps_new_simple("audio/mpeg",
                "mpegversion", G_TYPE_INT, 4,
                "channels", G_TYPE_INT, pAudioSettings->channelsIn,
                "rate", G_TYPE_INT, pAudioSettings->sampleRate,
                NULL);
            break;
        }
        case XA_AUDIOCODEC_AMR:
        {
            pAudioCaps = gst_caps_new_simple("audio/AMR",
                "channels", G_TYPE_INT, pAudioSettings->channelsIn,
                "rate", G_TYPE_INT, pAudioSettings->sampleRate,
                NULL);
            break;
        }
        case XA_AUDIOCODEC_AMRWB:
        {
            pAudioCaps = gst_caps_new_simple("audio/AMR-WB",
                "channels", G_TYPE_INT, pAudioSettings->channelsIn,
                "rate", G_TYPE_INT, pAudioSettings->sampleRate,
                NULL);
            break;
        }
        case XA_AUDIOCODEC_VORBIS:
        {
            pAudioCaps = gst_caps_new_simple("audio/x-vorbis",
                NULL);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            XA_LEAVE_INTERFACE;
    }

    if (pXAMediaRecorder->pAudioCaps)
        gst_caps_unref(pXAMediaRecorder->pAudioCaps);

    pXAMediaRecorder->pAudioCaps = gst_caps_new_simple("audio/x-raw-int",
         "width", G_TYPE_INT, pAudioSettings->bitsPerSample,
         "rate", G_TYPE_INT, pAudioSettings->sampleRate,
         "channels", G_TYPE_INT, pAudioSettings->channelsIn,
         NULL);

    if (pAudioCaps)
    {
        if (pXAMediaRecorder->pAudioProfile)
            gst_encoding_profile_unref(pXAMediaRecorder->pAudioProfile);

        pXAMediaRecorder->pAudioProfile = (GstEncodingAudioProfile *)
            gst_encoding_audio_profile_new(pAudioCaps, NULL, NULL, 0);
        gst_caps_unref(pAudioCaps);
    }
    pXAMediaRecorder->bUpdateRequired = XA_BOOLEAN_TRUE;

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderTakeSnapshot(
    CMediaRecorder *pCMediaRecorder)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    SnapshotData *pSnapShotData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)(pCMediaRecorder->pData);
    pSnapShotData = (SnapshotData *)pCMediaRecorder->mSnapshot.pData;
    AL_CHK_ARG(pXAMediaRecorder->mRecorder && pSnapShotData);

    //Check the view finder is enbled
    if (!MediaRecorderViewFinderIsDisabled(pXAMediaRecorder)
        && (pSnapShotData->bFreezeViewFinder))
    {
        if (g_object_class_find_property(
            G_OBJECT_GET_CLASS(pXAMediaRecorder->pCameraSrc),
            "pause-after-capture"))
        {
            g_object_set(
                G_OBJECT(pXAMediaRecorder->pCameraSrc),
                "pause-after-capture", TRUE, NULL);
        }
    }
    //Start-Capture
    g_signal_emit_by_name(
        G_OBJECT(pXAMediaRecorder->mRecorder), "start-capture", 0);
    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderInitiateSnapshot(
    CMediaRecorder *pCMediaRecorder,
    void *pData)
{
    GstCaps *pImageCaps = NULL;
    GstElement *pJpegEncoder = NULL;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntParamSpec = NULL;
    XAuint32 compressionLevel = 0;
    XAuint32 qualityLevel = 0;
    gchar *pFilename = NULL;
    gchar *pFilenameSuffix = (gchar *)".jpg";
    XAMediaRecorder *pXAMediaRecorder = NULL;
    SnapshotData *pSnapShotData = NULL;
    XAImageSettings *pImageSettings = NULL;
    GstElement *pRecorderGraph = NULL;
    GstMessage *pMsg = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pData && pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)(pCMediaRecorder->pData);
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    AL_CHK_ARG(pRecorderGraph);
    pSnapShotData = (SnapshotData *)pData;
    pImageSettings = (XAImageSettings *)pCMediaRecorder->mImageEncoder.pData;
    AL_CHK_ARG(pImageSettings);

    // Set Image Mode for Recorder
    g_object_set(G_OBJECT(pRecorderGraph), "mode", CAPTURE_MODE, NULL);

    pFilename = g_strdup_printf("%s%04u%s", pSnapShotData->pURI,
        (unsigned int)pSnapShotData->mPictureCount, pFilenameSuffix);
    XA_LOGD("Image Filename %s \n", pFilename);
    g_object_set(G_OBJECT(pRecorderGraph), "location", pFilename, NULL);
    g_free(pFilename);
    pSnapShotData->mPictureCount++;

    MediaRecorderFindElementInBin(
        pCMediaRecorder, (const XAchar *)"imagebin-encoder",
        (const XAchar *)"imagebin", &pJpegEncoder);
    if (pJpegEncoder)
    {
         pParamSpec = g_object_class_find_property(
            G_OBJECT_GET_CLASS(pJpegEncoder), "quality");
        if (pParamSpec)
        {
            //calculate new compression level based on encoder quality range
            pIntParamSpec = (GParamSpecInt *)pParamSpec;
            compressionLevel = pIntParamSpec->minimum +
                (pImageSettings->compressionLevel *
                (pIntParamSpec->maximum - pIntParamSpec->minimum)) / 1000;
            //compression scale to be reversed wrt quality
            qualityLevel = pIntParamSpec->maximum - compressionLevel;
            //If user-specific compressionlevel is zero, use default quality
            if (pImageSettings->compressionLevel == 0)
                qualityLevel = pIntParamSpec->default_value;
            g_object_set(
                G_OBJECT(pJpegEncoder), "quality", qualityLevel, NULL);
        }
    }

    pImageCaps = gst_caps_new_simple("image/jpeg", NULL);
    if (pImageCaps)
    {
        pXAMediaRecorder->pContainerProfile =
            gst_encoding_container_profile_new("jpeg", NULL, pImageCaps, NULL);
        pXAMediaRecorder->pVideoProfile = gst_encoding_video_profile_new(
            pImageCaps, NULL, NULL, 0);
        gst_encoding_video_profile_set_variableframerate(
            pXAMediaRecorder->pVideoProfile, TRUE);
        gst_encoding_container_profile_add_profile(
            pXAMediaRecorder->pContainerProfile,
            (GstEncodingProfile *)pXAMediaRecorder->pVideoProfile);
        gst_caps_unref(pImageCaps);
        g_object_set(G_OBJECT(pRecorderGraph), "image-profile",
            pXAMediaRecorder->pContainerProfile, NULL);
    }

    //Set image-capture-caps based on image settings
    pImageCaps = gst_caps_new_simple(
        "video/x-nv-yuv",
        "width", G_TYPE_INT, pImageSettings->width,
        "height", G_TYPE_INT, pImageSettings->height,
        NULL);
    if (pImageCaps)
    {
        g_object_set(
            G_OBJECT(pRecorderGraph), "image-capture-caps", pImageCaps, NULL);
        gst_caps_unref(pImageCaps);
    }

    if (GST_STATE(pRecorderGraph) == GST_STATE_NULL)
    {
        result = ChangeGraphState(pRecorderGraph, GST_STATE_PLAYING);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LEAVE_INTERFACE;
        }
    }
    if (!pXAMediaRecorder->pLoopThread)
    {
        pXAMediaRecorder->pLoopThread = g_thread_create(
            (GThreadFunc)MediaRecorderMainLoopRun,
            pXAMediaRecorder, TRUE, NULL);
        if (!pXAMediaRecorder->pLoopThread)
        {
            XA_LOGE("\n Creating Main loop thread failed!!\n");
            result = XA_RESULT_INTERNAL_ERROR;
        }
    }
    // post message on bus as camera is ready to take snapshot
    pMsg = gst_message_new_application(GST_OBJECT(pXAMediaRecorder->mRecorder),
        gst_structure_new("camera-is-ready", NULL));
    gst_element_post_message(pXAMediaRecorder->mRecorder, pMsg);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderSetVolume(
    CMediaRecorder *pCMediaRecorder,
    XAmillibel level)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstElement *pRecorderGraph = NULL;
    GstElement *pVolumeElem = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    AL_CHK_ARG(pRecorderGraph);

    if (pXAMediaRecorder->pAudioVolumeElem)
    {
        pVolumeElem = pXAMediaRecorder->pAudioVolumeElem;
    }
    else
    {
        pVolumeElem = gst_bin_get_by_name((GstBin *)pRecorderGraph,
            "audio-volume");
    }
    if (pVolumeElem)
    {
        gst_stream_volume_set_volume(
            GST_STREAM_VOLUME(pVolumeElem),
            GST_STREAM_VOLUME_FORMAT_DB,
            (gdouble)(level /100));
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderGetVolume(
    CMediaRecorder *pCMediaRecorder,
    XAmillibel *pLevel)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstElement *pRecorderGraph = NULL;
    GstElement *pVolumeElem = NULL;
    gdouble level;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData && pLevel);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    AL_CHK_ARG(pRecorderGraph);
    if (pXAMediaRecorder->pAudioVolumeElem)
    {
        pVolumeElem = pXAMediaRecorder->pAudioVolumeElem;
    }
    else
    {
        pVolumeElem = gst_bin_get_by_name((GstBin *)pRecorderGraph,
            "audio-volume");
    }
    if (pVolumeElem)
    {
        level = gst_stream_volume_get_volume(
            GST_STREAM_VOLUME(pVolumeElem),
            GST_STREAM_VOLUME_FORMAT_DB);
        *pLevel = (XAmillibel)(level * 100);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderGetMaxVolume(
    CMediaRecorder *pCMediaRecorder,
    XAmillibel *pMaxLevel)
{
    gdouble level = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pMaxLevel);
    level = gst_stream_volume_convert_volume(
        GST_STREAM_VOLUME_FORMAT_LINEAR,
        GST_STREAM_VOLUME_FORMAT_DB,
        1.0);
    *pMaxLevel = (XAmillibel)(level * 100);

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderSetMute(
    CMediaRecorder *pCMediaRecorder,
    XAboolean mute)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstElement *pRecorderGraph = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    AL_CHK_ARG(pRecorderGraph);

    if (pXAMediaRecorder->pAudioVolumeElem)
    {
        gst_stream_volume_set_mute(
            GST_STREAM_VOLUME(pXAMediaRecorder->pAudioVolumeElem),
            mute);
    }
    else
    {
        g_object_set(pRecorderGraph, "mute", mute, NULL);
    }

    XA_LEAVE_INTERFACE;
}

XAresult
ALBackendMediaRecorderGetMute(
    CMediaRecorder *pCMediaRecorder,
    XAboolean *pMute)
{
    XAMediaRecorder *pXAMediaRecorder = NULL;
    GstElement *pRecorderGraph = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData && pMute);
    pXAMediaRecorder = (XAMediaRecorder *)pCMediaRecorder->pData;
    pRecorderGraph = pXAMediaRecorder->mRecorder;
    AL_CHK_ARG(pRecorderGraph);

    if (pXAMediaRecorder->pAudioVolumeElem)
    {
        *pMute = gst_stream_volume_get_mute(
            GST_STREAM_VOLUME(pXAMediaRecorder->pAudioVolumeElem));
    }
    else
    {
        g_object_get(pRecorderGraph, "mute", pMute, NULL);
    }

    XA_LEAVE_INTERFACE;
}


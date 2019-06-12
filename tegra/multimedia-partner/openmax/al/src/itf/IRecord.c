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

/* IRecord interface implementation */
#include "linux_mediarecorder.h"

//***************************************************************************
// IRecord_SetRecordState
//***************************************************************************
static XAresult
IRecord_SetRecordState(
    XARecordItf self,
    XAuint32 state)
{
    IRecord *pRecordItf = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    RecordData *pRecordData = NULL;

    XA_ENTER_INTERFACE;
    pRecordItf = (IRecord *)self;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);

    interface_lock_poke(pRecordItf);
    result = ALBackendMediaRecorderSetRecordState(
        pCMediaRecorder, state);
    if (result == XA_RESULT_SUCCESS)
        pRecordData->mState = state;
    interface_unlock_poke(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_GetRecordState
//***************************************************************************
static XAresult
IRecord_GetRecordState(
    XARecordItf self,
    XAuint32 *pState)
{
    IRecord *pRecordItf = NULL;
    RecordData *pRecordData = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;

    XA_ENTER_INTERFACE;
    pRecordItf = (IRecord *)self;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData && pState);
    pRecordData = (RecordData *)pRecordItf->pData;
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);
    AL_CHK_ARG(pCMediaRecorder && pCMediaRecorder->pData);

    interface_lock_peek(pRecordItf);
    *pState = pRecordData->mState;
    interface_unlock_peek(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_SetDurationLimit
//***************************************************************************
static XAresult
IRecord_SetDurationLimit(
    XARecordItf self,
    XAmillisecond msec)
{
    RecordData *pRecordData = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    IRecord *pRecordItf = NULL;

    XA_ENTER_INTERFACE;
    pRecordItf = (IRecord *)self;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);

    if (pRecordData->mDurationLimit != msec)
    {
        interface_lock_poke(pRecordItf);
        pRecordData->mDurationLimit = msec;
        result = ALBackendMediaRecorderSignalThread(pCMediaRecorder);
        interface_unlock_poke(pRecordItf);
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_GetPosition
//***************************************************************************
static XAresult
IRecord_GetPosition(XARecordItf self, XAmillisecond *pMsec)
{
    CMediaRecorder *pCMediaRecorder = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData && pMsec);
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);

    interface_lock_poke(pRecordItf);
    result = ALBackendMediaRecorderGetClockTime(pCMediaRecorder, pMsec);
    interface_unlock_poke(pRecordItf);
    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_RegisterCallback
//***************************************************************************
static XAresult
IRecord_RegisterCallback(
    XARecordItf self,
    xaRecordCallback callback,
    void *pContext)
{
    RecordData *pRecordData;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;

    interface_lock_poke(pRecordItf);
    pRecordData->mCallback = callback;
    pRecordData->pContext = pContext;
    interface_unlock_poke(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_SetCallbackEventsMask
//***************************************************************************
static XAresult
IRecord_SetCallbackEventsMask(
    XARecordItf self,
    XAuint32 eventFlags)
{
    XA_ENTER_INTERFACE;

    if (eventFlags & ~(
        XA_RECORDEVENT_HEADATLIMIT  |
        XA_RECORDEVENT_HEADATMARKER |
        XA_RECORDEVENT_HEADATNEWPOS |
        XA_RECORDEVENT_HEADMOVING   |
        XA_RECORDEVENT_HEADSTALLED  |
        XA_RECORDEVENT_BUFFER_FULL))
    {
        result = XA_RESULT_PARAMETER_INVALID;
    }
    else
    {
        RecordData *pRecordData = NULL;
        IRecord *pRecordItf = (IRecord *)self;
        AL_CHK_ARG(pRecordItf && pRecordItf->pData);
        pRecordData = (RecordData *)pRecordItf->pData;
        interface_lock_poke(pRecordItf);
        pRecordData->mEventFlags = eventFlags;
        interface_unlock_poke(pRecordItf);
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_GetCallbackEventsMask
//***************************************************************************
static XAresult
IRecord_GetCallbackEventsMask(
    XARecordItf self,
    XAuint32 *pEventFlags)
{
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData && pEventFlags);
    pRecordData = (RecordData *)pRecordItf->pData;

    interface_lock_peek(pRecordItf);
    *pEventFlags = pRecordData->mEventFlags;
    interface_unlock_peek(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_SetMarkerPosition
//***************************************************************************
static XAresult
IRecord_SetMarkerPosition(
    XARecordItf self,
    XAmillisecond mSec)
{
    CMediaRecorder *pCMediaRecorder = NULL;
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);
    AL_CHK_ARG(pCMediaRecorder);

    if (pRecordData->mMarkerPosition != mSec)
    {
        interface_lock_poke(pRecordItf);
        pRecordData->mMarkerPosition = mSec;
        result = ALBackendMediaRecorderSignalThread(pCMediaRecorder);
        interface_unlock_poke(pRecordItf);
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_ClearMarkerPosition
//***************************************************************************
static XAresult
IRecord_ClearMarkerPosition(XARecordItf self)
{
    CMediaRecorder *pCMediaRecorder = NULL;
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);
    AL_CHK_ARG(pCMediaRecorder);

    interface_lock_poke(pRecordItf);
    pRecordData->mMarkerPosition = XA_TIME_UNKNOWN;
    result = ALBackendMediaRecorderSignalThread(pCMediaRecorder);
    interface_unlock_poke(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_GetMarkerPosition
//***************************************************************************
static XAresult
IRecord_GetMarkerPosition(
    XARecordItf self,
    XAmillisecond *pMsec)
{
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData && pMsec);
    pRecordData = (RecordData *)pRecordItf->pData;

    interface_lock_peek(pRecordItf);
    *pMsec = pRecordData->mMarkerPosition;
    interface_unlock_peek(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_SetPositionUpdatePeriod
//***************************************************************************
static XAresult
IRecord_SetPositionUpdatePeriod(
    XARecordItf self,
    XAmillisecond mSec)
{
    RecordData *pRecordData = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;
    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pRecordItf);
    AL_CHK_ARG(pCMediaRecorder);
    if (pRecordData->mPositionUpdatePeriod != mSec)
    {
        interface_lock_poke(pRecordItf);
        pRecordData->mPositionUpdatePeriod = mSec;
        result = ALBackendMediaRecorderSignalThread(pCMediaRecorder);
        interface_unlock_poke(pRecordItf);
    }

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_GetPositionUpdatePeriod
//***************************************************************************
static XAresult
IRecord_GetPositionUpdatePeriod(
    XARecordItf self,
    XAmillisecond *pMsec)
{
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pRecordItf && pRecordItf->pData);
    pRecordData = (RecordData *)pRecordItf->pData;
    AL_CHK_ARG(pRecordData && pMsec);

    interface_lock_peek(pRecordItf);
    *pMsec = pRecordData->mPositionUpdatePeriod;
    interface_unlock_peek(pRecordItf);

    XA_LEAVE_INTERFACE;
}

//***************************************************************************
// IRecord_Itf
//***************************************************************************
static const struct XARecordItf_ IRecord_Itf =
{
    IRecord_SetRecordState,
    IRecord_GetRecordState,
    IRecord_SetDurationLimit,
    IRecord_GetPosition,
    IRecord_RegisterCallback,
    IRecord_SetCallbackEventsMask,
    IRecord_GetCallbackEventsMask,
    IRecord_SetMarkerPosition,
    IRecord_ClearMarkerPosition,
    IRecord_GetMarkerPosition,
    IRecord_SetPositionUpdatePeriod,
    IRecord_GetPositionUpdatePeriod
};

//***************************************************************************
// IRecord_init
//***************************************************************************
void IRecord_init(void *self)
{
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE_VOID;
    if (pRecordItf)
    {
        pRecordItf->mItf = &IRecord_Itf;
        pRecordData = (RecordData *)malloc(sizeof(RecordData));
        if (pRecordData)
        {
            memset(pRecordData, 0, sizeof(RecordData));
            pRecordData->mState = XA_RECORDSTATE_STOPPED;
            pRecordData->mDurationLimit = XA_TIME_UNKNOWN;
            pRecordData->mCallback = NULL;
            pRecordData->pContext = NULL;
            pRecordData->mEventFlags = 0;
            pRecordData->mMarkerPosition = XA_TIME_UNKNOWN;
            pRecordData->mPositionUpdatePeriod = 1000;
        }
        pRecordItf->pData = (void *)pRecordData;
    }

    XA_LEAVE_INTERFACE_VOID;
}

//***************************************************************************
// IRecord_deinit
//***************************************************************************
void IRecord_deinit(void *self)
{
    RecordData *pRecordData = NULL;
    IRecord *pRecordItf = (IRecord *)self;

    XA_ENTER_INTERFACE_VOID;
    if (pRecordItf && pRecordItf->pData)
    {
        pRecordData = (RecordData *)pRecordItf->pData;
        free(pRecordData);
        pRecordItf->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID;
}

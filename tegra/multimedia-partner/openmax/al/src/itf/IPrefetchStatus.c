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

#include "linux_mediaplayer.h"

static XAresult
IPrefetchStatus_GetPrefetchStatus(
    XAPrefetchStatusItf self,
    XAuint32 *pStatus)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData && pStatus);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;

    if (pPrefetchData->mBufferLevel < XA_SUFFICIENTDATA_LOWERBOUND)
        *pStatus = XA_PREFETCHSTATUS_UNDERFLOW;
    else if (pPrefetchData->mBufferLevel <= XA_SUFFICIENTDATA_UPPERBOUND)
        *pStatus = XA_PREFETCHSTATUS_SUFFICIENTDATA;
    else
        *pStatus = XA_PREFETCHSTATUS_OVERFLOW;

    XA_LEAVE_INTERFACE;
}

static XAresult
IPrefetchStatus_GetFillLevel(
    XAPrefetchStatusItf self,
    XApermille *pLevel)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData && pLevel);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;
    *pLevel = pPrefetchData->mBufferLevel;
    XA_LEAVE_INTERFACE;
}

static XAresult
IPrefetchStatus_RegisterCallback(
    XAPrefetchStatusItf self,
    xaPrefetchCallback callback,
    void *pContext)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;

    interface_lock_poke(pPrefetchStat);
    pPrefetchData->mCallback = callback;
    pPrefetchData->pContext = pContext;
    interface_unlock_poke(pPrefetchStat);

    XA_LEAVE_INTERFACE;
}

static XAresult
IPrefetchStatus_SetCallbackEventsMask(
    XAPrefetchStatusItf self,
    XAuint32 eventFlags)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;

    if (eventFlags & ~(XA_PREFETCHEVENT_STATUSCHANGE |
        XA_PREFETCHEVENT_FILLLEVELCHANGE))
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    if (pPrefetchData->mEventFlags != eventFlags)
    {
        interface_lock_poke(pPrefetchStat);
        pPrefetchData->mEventFlags = eventFlags;
        interface_unlock_poke(pPrefetchStat);
    }

    XA_LEAVE_INTERFACE;
}

static XAresult
IPrefetchStatus_GetCallbackEventsMask(
    XAPrefetchStatusItf self,
    XAuint32 *pEventFlags)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData && pEventFlags);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;

    interface_lock_peek(pPrefetchStat);
    *pEventFlags = pPrefetchData->mEventFlags;
    interface_unlock_peek(pPrefetchStat);

    XA_LEAVE_INTERFACE;

}

static XAresult
IPrefetchStatus_SetFillUpdatePeriod(
    XAPrefetchStatusItf self,
    XApermille period)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;

    if (pPrefetchData->mFillUpdatePeriod != period)
    {
        interface_lock_peek(pPrefetchStat);
        pPrefetchData->mFillUpdatePeriod = period;
        interface_unlock_peek(pPrefetchStat);
    }
    XA_LEAVE_INTERFACE;
}

static XAresult
IPrefetchStatus_GetFillUpdatePeriod(
    XAPrefetchStatusItf self,
    XApermille *pPeriod)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    PrefetchData *pPrefetchData = NULL;

    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pPrefetchStat && pPrefetchStat->pData && pPeriod);
    pPrefetchData = (PrefetchData *)pPrefetchStat->pData;
    *pPeriod = pPrefetchData->mFillUpdatePeriod;

    XA_LEAVE_INTERFACE;
}

static const struct XAPrefetchStatusItf_ IPrefetchStatus_Itf = {
    IPrefetchStatus_GetPrefetchStatus,
    IPrefetchStatus_GetFillLevel,
    IPrefetchStatus_RegisterCallback,
    IPrefetchStatus_SetCallbackEventsMask,
    IPrefetchStatus_GetCallbackEventsMask,
    IPrefetchStatus_SetFillUpdatePeriod,
    IPrefetchStatus_GetFillUpdatePeriod
};

void IPrefetchStatus_init(void *self)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    XA_ENTER_INTERFACE_VOID;

    if (pPrefetchStat != NULL)
    {
        PrefetchData *pPrefetchData = (PrefetchData *)malloc(sizeof(PrefetchData));
        pPrefetchStat->mItf = &IPrefetchStatus_Itf;
        if (pPrefetchData != NULL)
        {
            memset(pPrefetchData, 0, sizeof(PrefetchData));
            pPrefetchData->mFillUpdatePeriod = 100;
            pPrefetchData->mBufferLevel = 0;
        }
        pPrefetchStat->pData = pPrefetchData;
    }
    XA_LEAVE_INTERFACE_VOID;
}

void IPrefetchStatus_deinit(void *self)
{
    IPrefetchStatus *pPrefetchStat = (IPrefetchStatus *)self;
    XA_ENTER_INTERFACE_VOID;
    if (pPrefetchStat != NULL && pPrefetchStat->pData != NULL)
    {
              free(pPrefetchStat->pData);
              pPrefetchStat->pData = NULL;
    }
    XA_LEAVE_INTERFACE_VOID;
}


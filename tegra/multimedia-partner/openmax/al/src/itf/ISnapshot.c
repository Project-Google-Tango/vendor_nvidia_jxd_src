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

/* Sanpshot implementation */

#include "linux_cameradevice.h"
#include "linux_mediarecorder.h"

static XAresult
ISnapshot_InitiateSnapshot(
    XASnapshotItf self,
    XAuint32 numberOfPictures,
    XAuint32 fps,
    XAboolean bFreezeViewFinder,
    XADataSink sink,
    xaSnapshotInitiatedCallback initiatedCallback,
    xaSnapshotTakenCallback takenCallback,
    void *pContext)
{
    XAint32 *pLocator = NULL;
    XAint32 *pMime = NULL;
    SnapshotData *pData = NULL;
    ISnapshot *pSnapshot = (ISnapshot *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pSnapshot && pSnapshot->pData);
    pData = (SnapshotData *)pSnapshot->pData;

    //only one picture is supported
    AL_CHK_ARG(numberOfPictures == 1);

    interface_lock_exclusive(pSnapshot);
    pData->initiatedCallback = initiatedCallback;
    pData->takenCallback = takenCallback;
    pData->mNumberOfPictures = numberOfPictures;
    pData->mFps = fps;
    pData->bFreezeViewFinder = bFreezeViewFinder;
    pData->pContext = pContext;
    pData->mDataSink = sink;

    //Decode data sink
    if (sink.pFormat == NULL || sink.pLocator == NULL)
    {
        XA_LOGE("TO DO: Image storage in memory provided by takenCallback "
                "with XA_DATALOCATOR_ADDRESS");
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        interface_unlock_exclusive(pSnapshot);
        XA_LEAVE_INTERFACE;
    }

    pLocator = (XAint32 *)sink.pLocator;
    pMime = (XAint32 *)sink.pFormat;

    switch (*pLocator)
    {
        case XA_DATALOCATOR_URI:
            // check the mime type
            if (*pMime != XA_DATAFORMAT_MIME)
            {
                XA_LOGE("Cannot create recorder with XA_DATALOCATOR_URI "
                        "data source without XA_DATAFORMAT_MIME format");
                result = XA_RESULT_FEATURE_UNSUPPORTED;
                interface_unlock_exclusive(pSnapshot);
                XA_LEAVE_INTERFACE;
            }
            else
            {
                XADataLocator_URI *pUri = (XADataLocator_URI *)(sink.pLocator);
                pData->pURI = pUri->URI;
            }
            break;
        default :
            // for these data locator types,
            // ignore the pFormat as it might be uninitialized
            XA_LOGE("Error: Sink not suported");
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            interface_unlock_exclusive(pSnapshot);
            XA_LEAVE_INTERFACE;
    }

    result = ALBackendMediaRecorderInitiateSnapshot(
        ((CMediaRecorder *)InterfaceToIObject(pSnapshot)), pData);

    pData->bInitiateSnapshotCalled = XA_BOOLEAN_TRUE;
    interface_unlock_exclusive(pSnapshot);

    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_TakeSnapshot(
    XASnapshotItf self)
{
    SnapshotData *pData = NULL;
    ISnapshot *pSnapshot = (ISnapshot *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pSnapshot && pSnapshot->pData);
    pData = (SnapshotData *)pSnapshot->pData;
    if (pData->bInitiateSnapshotCalled == XA_BOOLEAN_FALSE)
    {
        result = XA_RESULT_PRECONDITIONS_VIOLATED;
        XA_LEAVE_INTERFACE;
    }
    interface_lock_exclusive(pSnapshot);
    result = ALBackendMediaRecorderTakeSnapshot(
        ((CMediaRecorder *)InterfaceToIObject(pSnapshot)));
    interface_unlock_exclusive(pSnapshot);

    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_CancelSnapshot(
    XASnapshotItf self)
{
    //initiateSnapshotCalled should be reset here
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_ReleaseBuffers(
    XASnapshotItf self,
    XADataSink *pImage)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_GetMaxPicsPerBurst(
    XASnapshotItf self,
    XAuint32 * pMaxNumberOfPictures)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_GetBurstFPSRange(
    XASnapshotItf self,
    XAuint32 *pMinFPS,
    XAuint32 *pMaxFPS)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_SetShutterFeedback(
    XASnapshotItf self,
    XAboolean bEnabled)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

static XAresult
ISnapshot_GetShutterFeedback(
    XASnapshotItf self,
    XAboolean *pbEnabled)
{
    XA_ENTER_INTERFACE;
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE;
}

static const struct XASnapshotItf_ ISanpshot_Itf = {
    ISnapshot_InitiateSnapshot,
    ISnapshot_TakeSnapshot,
    ISnapshot_CancelSnapshot,
    ISnapshot_ReleaseBuffers,
    ISnapshot_GetMaxPicsPerBurst,
    ISnapshot_GetBurstFPSRange,
    ISnapshot_SetShutterFeedback,
    ISnapshot_GetShutterFeedback
};

void ISnapshot_init(void *self)
{
    SnapshotData *pData = NULL;
    ISnapshot *pSnapshot = (ISnapshot *)self;

    XA_ENTER_INTERFACE_VOID;

    if (pSnapshot)
    {
        pSnapshot->mItf = &ISanpshot_Itf;

        pData = (SnapshotData *)malloc(sizeof(SnapshotData));
        if (pData)
        {
            memset(pData, 0, sizeof(SnapshotData));
            //Do any intializations required for the Snapshot data
            pData->mFps = 1;
            pData->bFreezeViewFinder = 1;
            pData->bInitiateSnapshotCalled = XA_BOOLEAN_FALSE;
        }
        pSnapshot->pData = pData;
    }

    XA_LEAVE_INTERFACE_VOID;
}
//***************************************************************************
// ISnapshot_deinit
//***************************************************************************
void ISnapshot_deinit(void* self)
{
    ISnapshot *pSnapshot = (ISnapshot*)self;

    XA_ENTER_INTERFACE_VOID;

    if (pSnapshot && pSnapshot->pData)
    {
        free(pSnapshot->pData);
        pSnapshot->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID;
}


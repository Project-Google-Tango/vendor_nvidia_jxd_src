/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
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

/* Dynamic Source implementation */

#include "linux_mediaplayer.h"

static XAresult
IDynamicSource_SetSource(
    XADynamicSourceItf self,
    XADataSource *pDataSource)
{
    XAuint32 objectID = 0;
    IDynamicSource *pDynamicSource = (IDynamicSource *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pDynamicSource)

    objectID = InterfaceToObjectID(pDynamicSource);
    interface_lock_poke(pDynamicSource);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
            result = ALBackendMediaPlayerSetSource(
                (CMediaPlayer *)InterfaceToIObject(pDynamicSource),
                pDataSource);
            break;
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pDynamicSource);

    XA_LEAVE_INTERFACE
}

static const struct XADynamicSourceItf_ IDynamicSource_Itf = {
    IDynamicSource_SetSource
};

void IDynamicSource_init(void *self)
{
    IDynamicSource *pDynamicSource = (IDynamicSource *)self;

    XA_ENTER_INTERFACE_VOID
    if(pDynamicSource)
        pDynamicSource->mItf = &IDynamicSource_Itf;

    XA_LEAVE_INTERFACE_VOID
}

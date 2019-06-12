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

/** \file CNvDataTap.c MediaPlayer class */

#include "sles_allinclusive.h"
#ifdef ANDROID
#include "../android/android_datatap.h"
#endif

typedef struct
{
} NvDataTapData;

/** \brief Hook called by Object::Realize when a DataTap is realized */

SLresult CNvDataTap_Realize(void *self, SLboolean async)
{
#ifdef ANDROID
    LOGE("CNvDataTap_Realize ++\n");
    SLresult result = XA_RESULT_SUCCESS;
    CNvDataTap *pNvDataTap = (CNvDataTap *) self;
    XAObjectItf objectItf = pNvDataTap->mDataTapCreationParams.mediaObject;
    IObject* pObject = (IObject*)objectItf;

    if(IObjectToObjectID((IObject *)pObject) != XA_OBJECTID_MEDIAPLAYER)
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        LOGE("CNvDataTap_Create Failed");
     }
    else
    {
        LOGE("CNvDataTap_Create Passed");
        result = android_DataTapRealize(pNvDataTap);
    }

    LOGE("CNvDataTap_Realize --\n");
    return result;
#endif
    return XA_RESULT_FEATURE_UNSUPPORTED;
}


/** \brief Hook called by Object::Resume when a DataTap is
 *         resumed resumed */

SLresult CNvDataTap_Resume(void *self, SLboolean async)
{
    SLresult result = XA_RESULT_SUCCESS;
    return result;
}


/** \brief Hook called by Object::Destroy when a DataTap is destroyed */

void CNvDataTap_Destroy(void *self)
{
}


/** \brief Hook called by Object::Destroy before DataTap
 *         object is about to be destroyed */

predestroy_t CNvDataTap_PreDestroy(void *self)
{
    return predestroy_ok;

}
#if 0
/** \brief Hook called by Engine::CreateExtensionObject to create
 *         data tap */

XAresult CNvDataTap_Create(void *self)
{
    LOGE("CNvDataTap_Create ++\n");
    XAresult result = XA_RESULT_SUCCESS;
    InitializeFramework();
    CNvDataTap *pNvDataTap = (CNvDataTap *) self;
    XAObjectItf objectItf = pNvDataTap->mDataTapCreationParams.mediaObject;
    IObject* pObject = (IObject*)objectItf;
    const ClassTable* class__ = pObject->mClass;

    if( class__->mObjectID!=XA_OBJECTID_MEDIAPLAYER)
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        XA_LOGE("CNvDataTap_Create Failed");
     }
    else
        result = android_DataTapRealize(pNvDataTap);
    LOGE("CNvDataTap_Create --\n");
    return result;

}
#endif

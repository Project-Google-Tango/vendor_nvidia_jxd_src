/*
 * Copyright (C) 2011 NVIDIA Corporation
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

/* GetCapabilitiesOfInterface implementation */

#include "sles_allinclusive.h"

XAresult IGetCapabilitiesOfInterface_GetCapabilities(
    XAGetCapabilitiesOfInterfaceItf self,
    XAObjectItf pObject,
    const XAInterfaceID InterfaceID,
    XAuint32 CapabilityType,
    void * pCapabilities)
{
    XA_ENTER_INTERFACE

    IGetCapabilitiesOfInterface *thiz =
        (IGetCapabilitiesOfInterface *) self;

    interface_lock_exclusive(thiz);

    SLuint32 objectID;
    XAuint32 caps = 0;

    if ((pObject == NULL) || (InterfaceID == 0) || (pCapabilities == NULL))
    {
         result = XA_RESULT_PARAMETER_INVALID;
         goto done;
    }

    objectID = IObjectToObjectID((IObject *)pObject);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
             break;
        case XA_OBJECTID_MEDIARECORDER:
             break;

        default:
             SL_LOGE("%s object is not media player/recorder %x", __FUNCTION__, objectID);
             result = XA_RESULT_FEATURE_UNSUPPORTED;
             goto done;
    }

    if ((memcmp(InterfaceID, XA_IID_ANDROIDBUFFERQUEUESOURCE, sizeof(struct XAInterfaceID_))) &&
        (memcmp(InterfaceID, XA_IID_NVPUSHAPP, sizeof(struct XAInterfaceID_))) &&
        (memcmp(InterfaceID, XA_IID_VIDEOPOSTPROCESSING, sizeof(struct XAInterfaceID_))))
    {
        SL_LOGE("InterfaceID is not suported!!!");
        result = XA_RESULT_FEATURE_UNSUPPORTED;
        goto done;
    }

    if (CapabilityType == XA_GETCAPSOFINTERFACE_V1)
    {
        // MEDIAPLAYER and MEDIARECORDER both support RAW_H264_NAL
        caps = XA_GETCAPSOFINTERFACE_ES_RAW_H264_NAL;

        // MEDIAPLAYER and MEDIARECORDER both support RAW_PCM
        caps |= XA_GETCAPSOFINTERFACE_ES_RAW_PCM;

        // Only MEDIAPLAYER supports the rest
        if (objectID == XA_OBJECTID_MEDIAPLAYER)
        {
            caps |= XA_GETCAPSOFINTERFACE_ES_RAW_AAC_ADTS |
                    XA_GETCAPSOFINTERFACE_ES_MUXED |
                    XA_GETCAPSOFINTERFACE_ES_ENCRYPTED;
#ifndef TEGRA2
            caps |= XA_GETCAPSOFINTERFACE_SEAMLESS_FORMAT_CHANGE;
#endif
        }
    }
    else if(CapabilityType == XA_GETCAPSOFINTERFACE_V1_DATATAP)
    {

        if (objectID == XA_OBJECTID_MEDIAPLAYER)
        {
            CMediaPlayer *pPlayer = (CMediaPlayer *) pObject;
            if (NULL == pPlayer) {
                result = XA_RESULT_PARAMETER_INVALID;
                goto done;
            }

            NVXADataTapPointDescriptor *pDataTaps = (NVXADataTapPointDescriptor*)pCapabilities;
            int mNumDataTapPoints = (pPlayer->mNumDataTaps>MAX_DATATAPPOINTS)? MAX_DATATAPPOINTS : pPlayer->mNumDataTaps;
            for( int i = 0; i < mNumDataTapPoints; i++ ) {
                pDataTaps[i]. mType = pPlayer->pDataTaps[i]. mType;
                pDataTaps[i]. mNumFormats = pPlayer->pDataTaps[i]. mNumFormats;
                for(int j=0; j < MAX_DATATAPFORMATS_SUPPORTED; j++ ) {
                    NVXADataFormat *pDataFormat = &pPlayer->pDataTaps[i].mFormat[j];
                    XAuint32 formatType = *(XAuint32 *)pDataFormat;
                    switch (formatType) {
                    case XA_DATAFORMAT_MIME:
                        pDataTaps[i].mFormat[j].mMIME = pDataFormat->mMIME;
                        break;
                    case XA_DATAFORMAT_RAWIMAGE:
                        pDataTaps[i].mFormat[j].mRawImage = pDataFormat->mRawImage;
                        break;
                    case XA_DATAFORMAT_PCM:
                        pDataTaps[i].mFormat[j].mPCM = pDataFormat->mPCM;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    result = XA_RESULT_SUCCESS;
done:
    if (CapabilityType == XA_GETCAPSOFINTERFACE_V1) {
        *(XAuint32 *)pCapabilities = caps;

        SL_LOGI("IGetCapabilitiesOfInterface    : x%08x\n", caps);
        SL_LOGI("Seamless Format Change         : %s\n", (caps & XA_GETCAPSOFINTERFACE_SEAMLESS_FORMAT_CHANGE) ? "Yes" : "No");
        SL_LOGI("Raw H264 NAL Elementary Streams: %s\n", (caps & XA_GETCAPSOFINTERFACE_ES_RAW_H264_NAL) ? "Yes" : "No");
        SL_LOGI("Raw AAC ADTS Elementary Streams: %s\n", (caps & XA_GETCAPSOFINTERFACE_ES_RAW_AAC_ADTS) ? "Yes" : "No");
        SL_LOGI("Muxed Elementary Streams       : %s\n", (caps & XA_GETCAPSOFINTERFACE_ES_MUXED) ? "Yes" : "No");
        SL_LOGI("Encrypted Elementary Streams   : %s\n", (caps & XA_GETCAPSOFINTERFACE_ES_ENCRYPTED) ? "Yes" : "No");
        SL_LOGI("Raw PCM Elementary Streams     : %s\n", (caps & XA_GETCAPSOFINTERFACE_ES_RAW_PCM) ? "Yes" : "No");
    }

    interface_unlock_exclusive(thiz);

    XA_LEAVE_INTERFACE
}

static const struct XAGetCapabilitiesOfInterfaceItf_
    IGetCapabilitiesOfInterface_Itf = {
    IGetCapabilitiesOfInterface_GetCapabilities
};

void IGetCapabilitiesOfInterface_deinit(void *self)
{
    SL_LOGV("IGetCapabilitiesOfInterface_deinit\n");
    IGetCapabilitiesOfInterface *thiz =
        (IGetCapabilitiesOfInterface *) self;

    assert(thiz);
}

void IGetCapabilitiesOfInterface_init(void *self)
{
    SL_LOGV("IGetCapabilitiesOfInterface_init\n");
    IGetCapabilitiesOfInterface *thiz =
        (IGetCapabilitiesOfInterface *) self;

    assert(thiz);

    thiz->mItf = &IGetCapabilitiesOfInterface_Itf;
}


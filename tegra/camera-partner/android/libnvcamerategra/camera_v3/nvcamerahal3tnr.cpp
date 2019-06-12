/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <cutils/properties.h>
#include <cstdlib>
#include <utils/Log.h>

#include "nvcamerahal3tnr.h"

namespace android {

#define TNR_PROFILE (0)

#define NV_LOGE ALOGE

#define NV_LOGD(a, ...) ALOGD(__VA_ARGS__)

NvCameraTNR::NvCameraTNR():
    m_InitializeError(NvSuccess),
    m_hRm(NULL),
    m_pTVMRDevice(NULL),
    m_pTVMRMixer(NULL),
    m_fence(NULL),
    m_width(0),
    m_height(0),
    m_tnrStrength(1.0f),
    m_tnrAlgorithm(TVMR_NOISE_REDUCTION_ALGORITHM_INDOOR_MEDIUM_LIGHT),
    m_flush(false)
{
    NV_LOGD(HAL3_TNR_TAG, "%s: ++", __FUNCTION__);

    m_InitializeError = NvRmOpen(&m_hRm, 0);

    if (m_InitializeError != NvSuccess)
    {
        NV_LOGE("%s: failed to create Rm handle! [%d]\n",
                __FUNCTION__, m_InitializeError);
        goto fail;
    }

    m_pTVMRDevice = TVMRDeviceCreate(NULL);
    if (!m_pTVMRDevice)
    {
        NV_LOGE("%s: TVMR Device creation failed\n", __FUNCTION__);
        goto fail;
    }

    m_fence = TVMRFenceCreate();
    if (!m_fence)
    {
        NV_LOGE("%s Cannot create TVMR fence\n", __FUNCTION__);
        goto fail;
    }

    NV_LOGD(HAL3_TNR_TAG, "%s: --", __FUNCTION__);
    return;

fail:
    Release();
}

void
NvCameraTNR::Release(void)
{
    NV_LOGD(HAL3_TNR_TAG, "%s: ++", __FUNCTION__);

    if (m_hRm)
        NvRmClose(m_hRm);
    if (m_pTVMRMixer)
        TVMRVideoMixerDestroy(m_pTVMRMixer);
    if (m_pTVMRDevice)
        TVMRDeviceDestroy(m_pTVMRDevice);
    if (m_fence)
        TVMRFenceDestroy(m_fence);

    NV_LOGD(HAL3_TNR_TAG, "%s: --", __FUNCTION__);
}


NvCameraTNR::~NvCameraTNR()
{
    NV_LOGD(HAL3_TNR_TAG, "%s: ++", __FUNCTION__);
    Release();
    NV_LOGD(HAL3_TNR_TAG, "%s: --", __FUNCTION__);
}

// This function takes YV12 input buffer
// and applies temporal noise reduction
// on it.
NvError
NvCameraTNR::DoTNR(
    NvMMBuffer *pInputBuffer,
    NvMMBuffer *pOutputBuffer)
{
    Mutex::Autolock al(mMutex);
    NV_LOGD(HAL3_TNR_TAG, "%s: ++", __FUNCTION__);

    if (m_InitializeError != NvSuccess)
    {
        NV_LOGE("%s: TNR is not initialized properly! [%d]\n", __FUNCTION__,
                m_InitializeError);
        return NvError_NotInitialized;
    }

    if (m_flush)
    {
        NV_LOGD("%s: TNR flush --", __FUNCTION__);
        return NvSuccess;
    }

    NvError e = NvSuccess;
    NvRmSurface *pInRmSurfaces = NULL;
    NvRmSurface *pOutRmSurfaces = NULL;

    TVMRVideoSurface inputYUVFrame;
    TVMRVideoSurface outputYUVFrame;
    TVMRSurface inTVMRSurfaces[3];
    TVMRSurface outTVMRSurfaces[3];

    NvS32 surfaceCount = 0;
    NvS32 width = 0;
    NvS32 height = 0;
    NvS32 i;

    if (!pInputBuffer || !pOutputBuffer)
    {
        NV_LOGE("%s NULL input or output buffer\n", __FUNCTION__);
        return NvError_BadParameter;
    }

    surfaceCount   = pInputBuffer->Payload.Surfaces.SurfaceCount;
    width          = pInputBuffer->Payload.Surfaces.Surfaces[0].Width;
    height         = pInputBuffer->Payload.Surfaces.Surfaces[0].Height;
    pInRmSurfaces  = pInputBuffer->Payload.Surfaces.Surfaces;
    pOutRmSurfaces = pOutputBuffer->Payload.Surfaces.Surfaces;

    // Check for TVMR Mixer availiability
    if (m_pTVMRMixer == NULL  ||
            m_width  != width ||
            m_height != height)
    {
        TVMRVideoMixerAttributes attr;
        if (m_pTVMRMixer)
        {
            TVMRVideoMixerDestroy(m_pTVMRMixer);
        }

        m_pTVMRMixer =
            TVMRVideoMixerCreate(
                m_pTVMRDevice,
                TVMRSurfaceType_YV12,
                width,
                height,
                TVMR_VIDEO_MIXER_FEATURE_NOISE_REDUCTION
            );

        if (!m_pTVMRMixer)
        {
            NV_LOGE("%s TVMR Video Mixer Create Failed\n", __FUNCTION__);
            return NvError_InsufficientMemory;
        }
        m_width  = width;
        m_height = height;

        // Init the default TNR strength
        attr.noiseReduction = m_tnrStrength;
        attr.noiseReductionAlgorithm = m_tnrAlgorithm;
        TVMRVideoMixerSetAttributes(
            m_pTVMRMixer,
            TVMR_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION |
            TVMR_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_ALGORITHM,
            &attr);
        NV_LOGD(HAL3_TNR_TAG, "Created TVMR Mixer of size %dx%d\n", width, height);
    }

    // Check and update TNR strength
#if 1 // todo: disable this until proper debug switch support for yuv camera is done
    NvF32 tnrStrength;
    TVMRNoiseReductionAlgorithm algorithm;

    tnrStrength = GetTnrStrength();
    algorithm = GetTnrAlgorithm();
    if (tnrStrength != m_tnrStrength || algorithm != m_tnrAlgorithm)
    {
        TVMRVideoMixerAttributes attr;
        attr.noiseReduction = tnrStrength;
        attr.noiseReductionAlgorithm = algorithm;
        TVMRVideoMixerSetAttributes(
            m_pTVMRMixer,
            TVMR_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION |
            TVMR_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_ALGORITHM,
            &attr);
        m_tnrStrength = tnrStrength;
        m_tnrAlgorithm = algorithm;
    }
#endif

    // Setup TVMR surfaces
    inputYUVFrame.type    = TVMRSurfaceType_Y_V_U_420;
    inputYUVFrame.width   = width;
    inputYUVFrame.height  = height;
    outputYUVFrame.type   = TVMRSurfaceType_Y_V_U_420;
    outputYUVFrame.width  = width;
    outputYUVFrame.height = height;
    for (i = 0; i < 3; ++i)
    {
        inTVMRSurfaces[i].priv  = &pInRmSurfaces[i];
        outTVMRSurfaces[i].priv = &pOutRmSurfaces[i];
        inputYUVFrame.surfaces[i]  = &inTVMRSurfaces[i];
        outputYUVFrame.surfaces[i] = &outTVMRSurfaces[i];
    }

    // Perform TNR
    {
        TVMRStatus status;
#if TNR_PROFILE
        NvU32 startTime = NvOsGetTimeMS();
#endif
        status = TVMRVideoMixerRenderYUV(
                     m_pTVMRMixer,
                     &outputYUVFrame,
                     TVMR_PICTURE_STRUCTURE_FRAME,
                     NULL,
                     &inputYUVFrame,
                     NULL, NULL, NULL, NULL,
                     m_fence
                 );
        if (status != TVMR_STATUS_OK)
        {
            NV_LOGE("%s:TVMR Mixer render failed!\n", __FUNCTION__);
        }
        TVMRFenceBlock(m_pTVMRDevice, m_fence);

#if TNR_PROFILE
        NV_LOGD(HAL3_TNR_TAG, "TNR takes %d ms\n", NvOsGetTimeMS() - startTime);
#endif
    }

    NV_LOGD(HAL3_TNR_TAG, "%s: --", __FUNCTION__);
    return e;
}

NvF32
NvCameraTNR::GetTnrStrength()
{
    const char *key = "camera.debug.tnr.strength";
    const int PROP_VAL_MAX = 512;
    char property[PROP_VAL_MAX];
    if (property_get(key, property, "") > 0)
    {
        if (*property)
        {
            NvF32 str =  strtof(property, NULL);
            if (str > 1.0 || str < 0.0)
                return 0.0;
            return str;
        }
    }
    return 1.0;
}

TVMRNoiseReductionAlgorithm
NvCameraTNR::GetTnrAlgorithm()
{
    const char *key = "camera.debug.tnr.algorithm";
    const int PROP_VAL_MAX = 512;
    char property[PROP_VAL_MAX];
    if (property_get(key, property, "") > 0)
    {
        if (*property)
        {
            NvU32 algo =  atoi(property);
            if (algo <= (NvU32) TVMR_NOISE_REDUCTION_ALGORITHM_INDOOR_HIGH_LIGHT)
                return (TVMRNoiseReductionAlgorithm) algo;
            return TVMR_NOISE_REDUCTION_ALGORITHM_ORIGINAL;
        }
    }
    return TVMR_NOISE_REDUCTION_ALGORITHM_INDOOR_MEDIUM_LIGHT;
}


} //namespace android

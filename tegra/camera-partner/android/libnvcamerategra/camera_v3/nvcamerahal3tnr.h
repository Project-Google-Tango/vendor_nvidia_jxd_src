/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_TNR_H
#define NV_CAMERA_TNR_H

#include <nverror.h>
#include <nvassert.h>
#include <nvos.h>
#include <utils/Mutex.h>

#include "tvmr.h"
#include "nvmm.h"

namespace android {

class NvCameraTNR
{
public:
    NvCameraTNR();
    ~NvCameraTNR();

    void setBypass(bool pass){m_flush = pass;}
    NvError DoTNR(NvMMBuffer *pInputBuffer,
                  NvMMBuffer *pOutputBuffer);

private:
#if 1
    NvF32 GetTnrStrength();
    TVMRNoiseReductionAlgorithm GetTnrAlgorithm();
#endif

    void Release(void);

    NvError m_InitializeError;
    NvRmDeviceHandle m_hRm;
    TVMRDevice *m_pTVMRDevice;
    TVMRVideoMixer *m_pTVMRMixer;
    TVMRFence m_fence;
    NvS32 m_width;
    NvS32 m_height;
    NvF32 m_tnrStrength;
    TVMRNoiseReductionAlgorithm m_tnrAlgorithm;
    Mutex mMutex;
    bool m_flush;
};

} //namespace android

#endif

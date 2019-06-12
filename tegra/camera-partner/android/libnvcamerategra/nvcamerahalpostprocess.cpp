/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHalPostProcess"

#include "nvcamerahal.h"
#include "nvcamerahalpostprocess.h"

namespace android {

NvCameraPostProcessHalProxy::NvCameraPostProcessHalProxy(NvCameraHal *myHal)
{
    mHal = myHal;
}

void NvCameraPostProcessHalProxy::returnEmptyStillBuffer(NvMMBuffer *pBuffer)
{
    mHal->SendEmptyStillBufferToDZ(pBuffer);
}

void NvCameraPostProcessHalProxy::returnEmptyVideoBuffer(NvMMBuffer *pBuffer)
{
    mHal->SendEmptyVideoBufferToDZ(pBuffer);
}

void NvCameraPostProcessHalProxy::returnEmptyPreviewBuffer(NvMMBuffer *pBuffer)
{
    mHal->SendEmptyPreviewBufferToDZ(pBuffer);
}

NvError NvCameraPostProcessHalProxy::SetBracketCapture(NvMMBracketCaptureSettings &BracketCapture)
{
    return mHal->SetBracketCapture(BracketCapture);
}

void NvCameraPostProcessHalProxy::FeedJpegEncoder(NvMMBuffer *pBuffer)
{
    mHal->FeedJpegEncoder(pBuffer);
}

void NvCameraPostProcessHalProxy::WaitForJpegReturnSignal()
{
    mHal->WaitForCondition(mHal->mJpegBufferReturnedToPostProc);
}

void NvCameraPostProcessHalProxy::HandlePostviewCallback(NvMMBuffer *pBuffer)
{
    if (mHal->mPostviewFrameCopy->Enabled())
    {
        NvError e = NvSuccess;
        NV_CHECK_ERROR_CLEANUP(
            mHal->mPostviewFrameCopy->DoFrameCopyAndCallback(pBuffer, NV_FALSE)
        );
        mHal->mPostviewFrameCopy->CheckAndWaitWorkDone();
    }
    return;
fail:
    return;
}

NvCameraPostProcess::NvCameraPostProcess(void)
{
    mEnabled = NV_FALSE;
    mInitialized = NV_FALSE;
    mOutputReady = NV_FALSE;
    mHalProxy = NULL;
    NvOsMemset(&mOutputBuffer, 0, sizeof(mOutputBuffer));
}

NvCameraPostProcess::~NvCameraPostProcess(void)
{
    if (mHalProxy)
    {
        delete mHalProxy;
    }
}

NvError NvCameraPostProcess::MapSurface(NvRmSurface *pSurf, NvU8 **ptr)
{
    NvError e = NvSuccess;
    NvU32 sSize = 0;

    ALOGVV("%s++", __FUNCTION__);

    sSize = NvRmSurfaceComputeSize(pSurf);
    NV_CHECK_ERROR_CLEANUP(
                            NvRmMemMap(pSurf->hMem,
                            pSurf->Offset,
                            sSize,
                            NVOS_MEM_READ_WRITE | NVOS_MEM_MAP_INNER_WRITEBACK,
                            (void **)ptr)
    );

    NvRmMemCacheMaint(pSurf->hMem, *ptr, sSize, NV_TRUE, NV_TRUE);

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

void NvCameraPostProcess::UnmapSurface(NvRmSurface *pSurf, NvU8 *ptr)
{
    NvU32 sSize = 0;
    ALOGVV("%s++", __FUNCTION__);

    sSize = NvRmSurfaceComputeSize(pSurf);
    NvRmMemCacheMaint(pSurf->hMem, ptr, sSize, NV_TRUE, NV_TRUE);
    NvRmMemUnmap(pSurf->hMem, ptr, sSize);

    ALOGVV("%s--", __FUNCTION__);
    return;
}

NvError NvCameraPostProcess::Initialize(NvCameraHal *pCameraHal)
{
    if (!pCameraHal)
    {
        ALOGE("%s: Bad parameter", __FUNCTION__);
        return NvError_BadParameter;
    }
    mHalProxy = new NvCameraPostProcessHalProxy(pCameraHal);
    if (!mHalProxy)
    {
        ALOGE("%s: Out of memory", __FUNCTION__);
        return NvError_InsufficientMemory;
    }
    mInitialized = NV_TRUE;
    return NvSuccess;
}

NvError
NvCameraPostProcess::ProcessBuffer(
    NvMMBuffer *pInputBuffer,
    NvBool &OutputReady)
{
    // This is just a sample test and does a simple copy
    NvError e = NvSuccess;
    if (!mInitialized)
    {
        ALOGE("%s: Not initialized", __FUNCTION__);
        return NvError_NotInitialized;
    }

    if (!pInputBuffer)
    {
        ALOGE("%s: Bad parameter", __FUNCTION__);
        return NvError_BadParameter;
    }

    // always need to copy data in the NvMMBuffer
    NvOsMemcpy(&mOutputBuffer, pInputBuffer, sizeof(NvMMBuffer));

    // default just a pass through
    mOutputReady    = NV_TRUE;
    OutputReady     = mOutputReady;
    return e;
}

NvError NvCameraPostProcess::GetOutputBuffer(NvMMBuffer *pOutputBuffer)
{
    NvError e = NvSuccess;
    if (!mInitialized)
    {
        ALOGE("%s: Not initialized", __FUNCTION__);
        return NvError_NotInitialized;
    }

    if (!pOutputBuffer)
    {
        ALOGE("%s: Bad parameter", __FUNCTION__);
        return NvError_BadParameter;
    }

    if (!mOutputReady)
    {
        ALOGE("%s: Bad state", __FUNCTION__);
        return  NvError_InvalidState;
    }

    // always need to copy data in the NvMMBuffer
    NvOsMemcpy(pOutputBuffer, &mOutputBuffer, sizeof(NvMMBuffer));
    mOutputReady = NV_FALSE;
    return e;
}

NvBool NvCameraPostProcess::Enabled(void)
{
    return mEnabled;
}

NvError NvCameraPostProcess::Enable(NvBool enable)
{
    if (!mInitialized)
    {
        ALOGE("%s: Not initialized", __FUNCTION__);
        return NvError_NotInitialized;
    }
    mEnabled = enable;
    return NvSuccess;
}

NvCameraPostProcessStill::NvCameraPostProcessStill()
{
    mNumberOfInputImages = 1;
    mNumberOfAlgorithmInputImages = 1;
    mNumberOfOutputImages = 1;
    mInputFrameSequenceToEncode = NULL;
}

NvU32 NvCameraPostProcessStill::GetInputCount()
{
    return mNumberOfInputImages;
}

NvU32 NvCameraPostProcessStill::GetAlgorithmInputCount()
{
    return mNumberOfAlgorithmInputImages;
}

NvU32 NvCameraPostProcessStill::GetOutputCount()
{
    return mNumberOfOutputImages;
}

NvError NvCameraPostProcessStill::DoPreCaptureOperations()
{
    return NvSuccess;
}

NvError NvCameraPostProcessStill::DoPostCaptureOperations()
{
    return NvSuccess;
}

NvError
NvCameraPostProcessStill::ConfigureInputFrameEncoding(
    const NvBool *FramesToEncode,
    NvU32 NumInputArrayEntries)
{
    // base class doesn't support input frame encoding.
    // if a derived class doesn't support input frame encoding either, it should
    // simply return NvError_NotSupported like this:
    return NvError_NotSupported;

// example code to guide derived classes in how to implement their own versions
// of this function. we don't want to compile this in the base implementation,
// but we want to keep it around for reference.
#if 0

    // check error cases to make sure that the client is
    // sending sane inputs and configuring us in a way that we accept
    if (NumInputArrayEntries != mNumberOfAlgorithmInputImages)
    {
        // application passed in a bad array size
        return NvError_BadParameter;
    }

    // once we know the inputs are valid, a derived class's implementation would
    // likely just copy this state over so that it can reference it when input
    // frames begin to arrive
    NvOsMemcpy(
        mInputFrameSequenceToEncode,
        FramesToEncode,
        mNumberOfAlgorithmInputFrames * sizeof(NvBool));
    return NvSuccess;
#endif
}

NvBool NvCameraPostProcessStill::EncodesOutput()
{
    // default assumes client handles encoding
    return NV_FALSE;
}

void NvCameraPostProcessStill::ReturnBufferAfterEncoding(NvMMBuffer *Buffer)
{
    // this function is a no-op, because the default impl doesn't handle encode.
    return;
}

} // namespace android

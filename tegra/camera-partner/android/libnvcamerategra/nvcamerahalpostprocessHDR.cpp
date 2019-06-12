/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHDRStill"


#include "nvcamerahalpostprocessHDR.h"
#include "nvcamera_hdr.h"
#include <cutils/properties.h>


#define DO_TIMING_CHECK 0
#define OUTPUT_INDEX (mNumberOfInputImages - 2)
#define COMPOSE_INDEX (mNumberOfInputImages - 1)

#if DO_TIMING_CHECK

    NvS64 StartTime = 0;

    #define START_PERF_MEASURE(NAME) \
    do { \
        StartTime = NvOsGetTimeUS(); } while (0);

    #define END_PERF_MEASURE(NAME) \
        do { NvOsDebugPrintf("TIMING BLOCK = " #NAME \
            " total work time(us) = %lld", \
        NvOsGetTimeUS() - StartTime); } while(0);
#else
    #define END_PERF_MEASURE(NAME)
    #define START_PERF_MEASURE(NAME)
#endif

namespace android {

NvCameraHDRStill::NvCameraHDRStill()
{
    size_t allocSize = 0;
    NvCameraHdrAlloc(&mHdrProcessor);
    ClearCurrentBuffers();
    mNumberOfAlgorithmInputImages = DEFAULT_HDR_IMAGES;

    allocSize = mNumberOfAlgorithmInputImages * sizeof(NvBool);
    mInputFrameSequenceToEncode = (NvBool *)NvOsAlloc(allocSize);
    if (!mInputFrameSequenceToEncode)
    {
        ALOGE("%s: alloc failed!  HDR will not work properly.", __FUNCTION__);
        return;
    }
    NvOsMemset(mInputFrameSequenceToEncode, 0, allocSize);
}

NvCameraHDRStill::~NvCameraHDRStill()
{
    NvCameraHdrFree(mHdrProcessor);
    NvOsFree(mInputFrameSequenceToEncode);
    mInputFrameSequenceToEncode = NULL;
}

NvError NvCameraHDRStill::MapAndStoreBuffer(NvU32 index, NvMMBuffer *pBuffer)
{
    NvError e = NvSuccess;
    ALOGV("%s++", __FUNCTION__);

    NvOsMemcpy(&mCurrentBuffers[index].Buffer, pBuffer, sizeof(NvMMBuffer));

    NV_CHECK_ERROR_CLEANUP(
                            MapSurface(&pBuffer->Payload.Surfaces.Surfaces[0],
                            &(mCurrentBuffers[index].pY))
    );

    NV_CHECK_ERROR_CLEANUP(
                            MapSurface(&pBuffer->Payload.Surfaces.Surfaces[1],
                            &(mCurrentBuffers[index].pU))
    );

    NV_CHECK_ERROR_CLEANUP(
                            MapSurface(&pBuffer->Payload.Surfaces.Surfaces[2],
                            &(mCurrentBuffers[index].pV))
    );

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

void NvCameraHDRStill::UnmapStoredBuffer(NvU32 index)
{
    ALOGV("%s++", __FUNCTION__);

    UnmapSurface(&mCurrentBuffers[index].Buffer.Payload.Surfaces.Surfaces[0],
                     mCurrentBuffers[index].pY);

    UnmapSurface(&mCurrentBuffers[index].Buffer.Payload.Surfaces.Surfaces[1],
                     mCurrentBuffers[index].pU);

    UnmapSurface(&mCurrentBuffers[index].Buffer.Payload.Surfaces.Surfaces[2],
                     mCurrentBuffers[index].pV);

    ALOGV("%s--", __FUNCTION__);
    return;
}

NvError NvCameraHDRStill::Enable(NvBool enable)
{
    NvMMBracketCaptureSettings bracketCapSettings;
    NvError e = NvSuccess;

    ALOGV("%s++", __FUNCTION__);

    if (!mInitialized)
    {
        ALOGE("%s: Not initialized", __FUNCTION__);
        return NvError_NotInitialized;
    }

    if (mCurrentBufferNumber != 0)
    {
        // This is recoverable; however we should not get in this state
        // as capture should finish before trying to enable/disable HDR
        ALOGE("%s: Bad state", __FUNCTION__);
        return  NvError_InvalidState;
    }

    if (enable != mEnabled)
    {
        mNumberOfOutputImages   = 1;
        mCurrentBufferNumber    = 0;
        mEnabled = enable;
    }

    if (enable)
    {
        mNumberOfInputImages            = DEFAULT_HDR_IMAGES;
        bracketCapSettings.NumberImages = mNumberOfInputImages;
        bracketCapSettings.ExpAdj[0] = BRACKET_FSTOP0;
        bracketCapSettings.ExpAdj[1] = BRACKET_FSTOP1;
        bracketCapSettings.ExpAdj[2] = BRACKET_FSTOP2;
        bracketCapSettings.IspGainEnabled[0] = NV_TRUE;
        bracketCapSettings.IspGainEnabled[1] = NV_TRUE;
        bracketCapSettings.IspGainEnabled[2] = NV_TRUE;
    }
    else
    {
        mNumberOfInputImages            = 1;
        bracketCapSettings.NumberImages = 0;
    }

    NV_CHECK_ERROR_CLEANUP(
        mHalProxy->SetBracketCapture(bracketCapSettings)
    );

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHDRStill::ProcessBuffer(NvMMBuffer *pInputBuffer, NvBool &OutputReady)
{
    ALOGV("%s++", __FUNCTION__);

    NvError e = NvSuccess;

    // do some error checking
    if (!pInputBuffer)
    {
        ALOGE("%s: Bad parameter", __FUNCTION__);
        return NvError_BadParameter;
    }

    if (mCurrentBufferNumber == mNumberOfInputImages || mEnabled == NV_FALSE)
    {
        ALOGE("%s: Bad state", __FUNCTION__);
        return  NvError_InvalidState;
    }

    // store the buffer in our context, everything else after this may
    // reference it
    NV_CHECK_ERROR_CLEANUP(
        MapAndStoreBuffer(
            mCurrentBufferNumber,
            pInputBuffer)
    );

    if (mCurrentBufferNumber == 0)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartProcessingSequence()
        );
    }

    // feed the alg
    NV_CHECK_ERROR_CLEANUP(
        NvCameraHdrAddImageBuffer(
            mHdrProcessor,
            mCurrentBuffers[mCurrentBufferNumber].pY,
            mCurrentBuffers[mCurrentBufferNumber].pU,
            mCurrentBuffers[mCurrentBufferNumber].pV)
    );

    // if client told us we should encode this buffer, send it to the encoder
    if (mInputFrameSequenceToEncode[mCurrentBufferNumber] == NV_TRUE)
    {
        mCurrentBuffers[mCurrentBufferNumber].PendingEncode = NV_TRUE;
        mHalProxy->FeedJpegEncoder(
            &mCurrentBuffers[mCurrentBufferNumber].Buffer);
    }

    // keep track of how many we've fed to the alg so far
    mCurrentBufferNumber++;

    // once we've gotten all the input images, encode and clean up
    if (mCurrentBufferNumber == mNumberOfInputImages)
    {
        NV_CHECK_ERROR_CLEANUP(
            FinishProcessingSequence()
        );
    }

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHDRStill::StartProcessingSequence()
{
    ALOGV("%s++", __FUNCTION__);

    NvError e = NvSuccess;
    int registrationEnabled;
    char property[PROPERTY_VALUE_MAX];

    NV_CHECK_ERROR_CLEANUP(
        NvCameraHdrInit(
            mHdrProcessor,
            mCurrentBuffers[mCurrentBufferNumber].
                Buffer.Payload.Surfaces.Surfaces[0].Width,
            mCurrentBuffers[mCurrentBufferNumber].
                Buffer.Payload.Surfaces.Surfaces[0].Height,
            mCurrentBuffers[mCurrentBufferNumber].
                Buffer.Payload.Surfaces.Surfaces[0].Pitch,
            mCurrentBuffers[mCurrentBufferNumber].
                Buffer.Payload.Surfaces.Surfaces[1].Pitch,
            mNumberOfInputImages)
    );

    // enable registration if setprop on
    property_get("camera.debug.hdr.reg.enable", property, "1");
    registrationEnabled = atoi(property);

    if (registrationEnabled)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvCameraHdrEnableRegistration(mHdrProcessor)
        );
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            NvCameraHdrDisableRegistration(mHdrProcessor)
        );
    }

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;

}

NvError NvCameraHDRStill::FinishProcessingSequence()
{
    ALOGV("%s++", __FUNCTION__);
    NvError e = NvSuccess;
    NvRect srcRect;
    NvDdk2dFixedRect sfxSrcRect;
    NvRect destRect;

    // if we're still waiting for this input buffer to be encoded, we need
    // to let it finish before re-using it as the composition output buffer.
    // NOTE: if we spend a long time waiting here, we could try to use
    // a different buffer for the output, so that encoding will finish
    // sooner.
    WaitForJpegBufferToReturn(COMPOSE_INDEX);

    // produce the output
    NV_CHECK_ERROR_CLEANUP(
        NvCameraHdrCompose(
            mHdrProcessor,
            mCurrentBuffers[COMPOSE_INDEX].pY,
            mCurrentBuffers[COMPOSE_INDEX].pU,
            mCurrentBuffers[COMPOSE_INDEX].pV)
    );

    // unmap stored buffers. this does cache maintenance which makes
    // sure the data is good in the NvMMBuffer for the JPEG encoder
    // to use.
    for (NvU32 i = 0; i < mNumberOfInputImages; i++)
    {
        UnmapStoredBuffer(i);
    }

    // wait for the output's input buffer to finish encoding
    // before doing the blit from compose to output buffer
    WaitForJpegBufferToReturn(OUTPUT_INDEX);

    // crop and scale to get rid of the warping artifacts
    NvCameraGetCropParams(mHdrProcessor,&srcRect);
    sfxSrcRect.left = NV_SFX_WHOLE_TO_FX(srcRect.left);
    sfxSrcRect.right = NV_SFX_WHOLE_TO_FX(srcRect.right);
    sfxSrcRect.top = NV_SFX_WHOLE_TO_FX(srcRect.top);
    sfxSrcRect.bottom = NV_SFX_WHOLE_TO_FX(srcRect.bottom);

    destRect.left = 0;
    destRect.right = mCurrentBuffers[COMPOSE_INDEX].Buffer.Payload.Surfaces.Surfaces[0].Width;
    destRect.top = 0;
    destRect.bottom = mCurrentBuffers[COMPOSE_INDEX].Buffer.Payload.Surfaces.Surfaces[0].Height;

    mScaler.CropAndScale(
        &mCurrentBuffers[COMPOSE_INDEX].Buffer.Payload.Surfaces,
        &sfxSrcRect,
        &mCurrentBuffers[OUTPUT_INDEX].Buffer.Payload.Surfaces,
        &destRect);

    // feed the output to the encoder
    mCurrentBuffers[OUTPUT_INDEX].PendingEncode = NV_TRUE;
    mHalProxy->FeedJpegEncoder(
        &mCurrentBuffers[OUTPUT_INDEX].Buffer);

    // return all the images to DZ once they've returned
    // from the encoder
    for (NvU32 i = 0; i < mNumberOfInputImages; i++)
    {
        WaitForJpegBufferToReturn(i);

        // only send postview for output buffer
        if (i == OUTPUT_INDEX)
        {
            mHalProxy->HandlePostviewCallback(
                &mCurrentBuffers[i].Buffer);
        }

        mHalProxy->returnEmptyStillBuffer(
            &mCurrentBuffers[i].Buffer);
    }

    // finally, now that we're all done with the buffers, reinit this so
    // that we don't accidentally try to use them at an invalid time
    ClearCurrentBuffers();

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHDRStill::GetOutputBuffer(NvMMBuffer *pOutputBuffer)
{
    NvError e = NvSuccess;

    ALOGV("%s++", __FUNCTION__);

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
    NvOsMemcpy(pOutputBuffer, &mCurrentBuffers[OUTPUT_INDEX], sizeof(NvMMBuffer));
    // reset since output was retrived.
    mCurrentBufferNumber = 0;
    mOutputReady         = NV_FALSE;
    ALOGV("%s--", __FUNCTION__);
    return e;
}

void NvCameraHDRStill::GetHDRFrameSequence(NvF32 *FrameSequence)
{
    FrameSequence[0] = BRACKET_FSTOP0;
    FrameSequence[1] = BRACKET_FSTOP1;
    FrameSequence[2] = BRACKET_FSTOP2;
}

NvError
NvCameraHDRStill::ConfigureInputFrameEncoding(
    const NvBool *FramesToEncode,
    NvU32 NumInputArrayEntries)
{
    // check error cases to make sure that the client is
    // sending sane inputs and configuring us in a way that we accept
    if (NumInputArrayEntries != mNumberOfAlgorithmInputImages)
    {
        // application passed in a bad array size
        return NvError_BadParameter;
    }

    // once we know the inputs are valid, copy this state over so that we can
    // reference it when input frames begin to arrive
    NvOsMemcpy(
        mInputFrameSequenceToEncode,
        FramesToEncode,
        mNumberOfAlgorithmInputImages * sizeof(NvBool));

    return NvSuccess;
}

void NvCameraHDRStill::ReturnBufferAfterEncoding(NvMMBuffer *Buffer)
{
    for (int i = 0; i < MAX_HDR_IMAGES; i++)
    {
        if (mCurrentBuffers[i].Buffer.BufferID == Buffer->BufferID)
        {
            if (!mCurrentBuffers[i].PendingEncode)
            {
                ALOGE("%s: got a buffer back that wasn't being encoded?",
                    __FUNCTION__);
                return;
            }
            // found a match!
            mCurrentBuffers[i].PendingEncode = NV_FALSE;
        }
    }
}

NvBool NvCameraHDRStill::EncodesOutput()
{
    // HDR class handles encoding on its own
    return NV_TRUE;
}

void NvCameraHDRStill::ClearCurrentBuffers()
{
    for (int i = 0; i < MAX_HDR_IMAGES; i++)
    {
        mCurrentBuffers[i].PendingEncode = NV_FALSE;
        NvOsMemset(&mCurrentBuffers[i].Buffer, 0, sizeof(NvMMBuffer));
        mCurrentBuffers[i].pY = NULL;
        mCurrentBuffers[i].pU = NULL;
        mCurrentBuffers[i].pV = NULL;
    }
    mCurrentBufferNumber = 0;
    mOutputReady         = NV_FALSE;
}

void NvCameraHDRStill::WaitForJpegBufferToReturn(NvU32 Index)
{
    while (mCurrentBuffers[Index].PendingEncode)
    {
        mHalProxy->WaitForJpegReturnSignal();
    }
}

} // namespace android

/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_HAL_POSTPROCESS_HDR_H
#define NV_CAMERA_HAL_POSTPROCESS_HDR_H

#include "nvcamerahalpostprocess.h"
#include "nvimagescaler.h"

#define MAX_HDR_IMAGES 3
#define DEFAULT_HDR_IMAGES 3
#define BRACKET_FSTOP0  0.0f
#define BRACKET_FSTOP1 -2.0f
#define BRACKET_FSTOP2  2.0f

namespace android {

    class NvCameraHDRStill: public NvCameraPostProcessStill
    {
    public:
        NvCameraHDRStill();
        virtual ~NvCameraHDRStill();
        /**
        * Enables HDR processing overides NvCameraPostProcessStill::Enable()
        * @param enable, set to NV_TRUE to enable HDR
        * @return NV_Error Error possible if enabling before class is initialized
        */
        NvError Enable(NvBool enable);
        /**
        * Processes buffers for HDR, will signal OutputReady when an HDR frame is complete.
        * overrides NvCameraPostProcessStill::ProcessBuffer()
        * @param pInputBuffer pointer to a valid buffer
        * @param OutputReady will be set to true if post processing has an output ready
        * @return NV_Error, Can fail if parameters are invalid, or class is not initialized
        */
        NvError ProcessBuffer(NvMMBuffer *pInputBuffer,NvBool &OutputReady);
        /**
        * Copies a valid post processing output NvMMBuffer into the parameter.
        * overrides NvCameraPostProcessStill::GetOutputBuffer()
        * @param pOutputBuffer pointer to a valid buffer
        * @param OutputReady will be set to true if post processing has an output ready
        * @return NV_Error, Can fail if parameters are invalid, class not initialized or
        *                   no output ready.
        */
        NvError GetOutputBuffer(NvMMBuffer *pOutputBuffer);

        /**
        * Tells the postprocessing class to send input frames to the JPEG
        * encoder.  This is useful for applications that wish to allow the
        * user to keep some (or all) of the unprocessed frames.
        *
        * This method will update an internal copy of the state, which we will
        * check as input frames are passed in to decide whether they will be
        * encoded.
        *
        * Input frame encoding is supported by this HDR class.
        *
        * @param const NvBool *FramesToEncode -
        * Pointer to an array of NvBool values.  This array's size must
        * match the size returned by GetAlgorithmInputCount().  The order of
        * entries in this array corresponds to the order the input frames are
        * fed to the algorithm via ProcessBuffer(...).  For example, if index 1
        * in the array is true, the second input frame of a particular sequence
        * that is fed to ProcessBuffer(...) will be encoded.  Once this array
        * is set, the encoding will be done for every successive input sequence
        * that is fed to the postprocessing class, until a new array is sent
        * to specify some other encode configuration.
        *
        * @param NvU32 NumInputArrayEntries -
        * Number of entries the input array has.  Adds an extra sanity check
        * to force the client to double-check they're sending us something sane.
        *
        * @return NvError
        * NvError_BadParameter if input parameters are bad
        * NvSuccess if things go as planned
        */
        NvError ConfigureInputFrameEncoding(
            const NvBool *FramesToEncode,
            NvU32 NumInputArrayEntries);

        /**
        * If this class sends a buffer to the HAL to be encoded, the HAL must
        * return the buffer to this class when the encoding is done, so that
        * this class can either reuse it for the output or return it to the
        * source.
        *
        * @param NvMMBuffer *Buffer - pointer to the buffer being returned
        */
        void ReturnBufferAfterEncoding(NvMMBuffer *Buffer);

        NvBool EncodesOutput();

        /**
        * Obtains an array of the exposure compensation values that are used
        * as input frames to the HDR algorithm.  The order of the values in
        * the array matches the order of frames being fed into the alg.
        * @param NvF32 *, pointer to the first element of the array of values,
        *        This method will overwrite the first N values, where N is the
        *        number returned by NvCameraHDRStill::GetNumHDRImages().
        *        Because of this, the size of the input array MUST be at least
        *        as large as N.
        */
        static void GetHDRFrameSequence(NvF32 *FrameSequence);

        void ClearCurrentBuffers();

        void WaitForJpegBufferToReturn(NvU32 Index);

    private:

        void    UnmapStoredBuffer(NvU32 index);
        NvError MapAndStoreBuffer(NvU32 index, NvMMBuffer *pBuffer);
        NvError StartProcessingSequence();
        NvError FinishProcessingSequence();

        NvU32 mCurrentBufferNumber;
        NvU32 mHdrProcessor;

        NvImageScaler mScaler;

        struct
        {
            NvBool PendingEncode;
            NvMMBuffer Buffer;
            NvU8  *pY;
            NvU8  *pU;
            NvU8  *pV;
        } mCurrentBuffers[MAX_HDR_IMAGES];

    };

}; // namespace android

#endif // NV_CAMERA_HAL_POSTPROCESS_HDR_H

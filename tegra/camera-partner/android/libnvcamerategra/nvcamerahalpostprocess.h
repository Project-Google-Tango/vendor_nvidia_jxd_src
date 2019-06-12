/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_HAL_POSTPROCESS_H
#define NV_CAMERA_HAL_POSTPROCESS_H

/**
 * @file
 * <b>NVIDIA Camera Post-Processing Proxy APIs</b>
 *
 * @b Description: The NVIDIA camera post-processing classes are thin proxies
 * for the camera hardware abstraction layer (HAL), to facilitate extensions for
 * post-processing still or video images.
 *
 */

/**
 * @defgroup camera_postprocess_group Camera Post-Processing Interface
 *
 * The NvCameraHal class has a post-processing interface that allows developers
 * with access to the source code to develop YUV-level camera image processing
 * blocks.
 *
 * The interface provides hooks so you can post-process any still, video, or
 * preview data handled by the HAL. Frames are taken directly after the output
 * of the NvMM Digital Zoom (DZ) block. Extendable control of the HAL is
 * provided to any post-processing blocks.
 *
 * Post-processing occurs after the HAL receives frames from the DZ block but
 * before anything else is done with them in the HAL. After post-processing, the
 * HAL passes still image data to the JPEG encoder, video data to the Android
 * framework, and preview data to the application.
 *
 * To implement post-processing, you must create classes that inherit from
 * android::NvCameraPostProcessStill, android::NvCameraPostProcessVideo, or
 * android::NvCameraPostProcessPreview and override relevant functions (such as
 * processBuffer). After you create the classes, they can be instantiated
 * instead of the default classes in the HAL. A pointer to the custom class must
 * be set to one of the three HAL @c mPostProcessStill, @c mPostProcessVideo, or
 * @c mPostProcessPreview members.
 *
 * For example, if you build a preview post-processing class, replace
 * HAL::mPostProcessPeview (pointing to an NvCameraPostProcessPreview class)
 * with a pointer to the newly created class. To enable post-processing, you
 * would simply call @c HAL->mPostProcessPreview->Enable(NV_TRUE). If you
 * replace the mPostProcessPreview member with a custom implementation, you
 * could override the @c Enable function and have it run the custom code of your
 * choice.
 *
 * Where the @c Enable function is called is up to you. For example, you could
 * add a custom setting when a certain feature is enabled, such as due to a
 * state transition. For the HDR example, described later in this section, there
 * is a custom setting, @c stillHDR, that is called from the
 * application. Similarly, for any new features that are controlled by the app,
 * the setting should be added.
 *
 * If you want a post-processing algorithm to have access to multiple image data
 * sets, such as video and preview, you need to define a class that has two
 * helper classes that derive from the @c NvCameraPostProcessVideo and @c
 * NvCameraPostProcessPreview classes. It not advisable to have one class derive
 * from both the @c NvCameraPostProcessVideo and @c NvCameraPostProcessPreview
 * classes, because that inheritance will run into the star problem and
 * overridden functions might not be resolved correctly.
 *
 * @warning Any post-processing class you create should access the HAL using the
 * proxy only, due to the complexity of the HAL and its internal
 * interactions. Adding the needed function in the proxy allows you to test the
 * required functionality and ensure that it works correctly. The
 * post-processing class should not insert any additional code in the HAL. If
 * you need additional hooks, add them to the @c NvCameraPostProcessVideo, @c
 * NvCameraPostProcessStill, or @c NvCameraPostProcessPreview classes. This
 * allows NVIDIA to add additional post-processing blocks and allows easy
 * switching between blocks.
 *
 * @ingroup camera_modules
 * @{
 */

#include <utils/RefBase.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <utils/SortedVector.h>
#include "nvcommon.h"
#include "nverror.h"
#include "nvos.h"
#include "nvrm_surface.h"
#include "nvmm_buffertype.h"
#include "nvmm_camera_types.h"



namespace android {

    class NvCameraHal;

    /**
    * Class to use and extend as needed by applications performing image
    * post-processing.
    */
    class NvCameraPostProcessHalProxy
    {
    public:
        /**
        * Constructor for the post processing hal proxy. Stores a reference used
        * to pass commands to and from the HAL.
        *
        * @param myHal A pointer to a valid HAL object that includes post
        * processing functionality.
        */
        NvCameraPostProcessHalProxy(NvCameraHal *myHal);

        /**
        * Returns to the HAL a pointer to an NvMMBuffer used for a still
        * capture, which will be used to pass commands to and from the HAL.
        *
        * @param pBuffer A pointer to an NvMMBuffer object.
        */
        void returnEmptyStillBuffer(NvMMBuffer *pBuffer);

        /**
        * Returns to the HAL a pointer to an NvMMBuffer used for a video
        * capture, which will be used to pass commands to and from the HAL.
        *
        * @param pBuffer A pointer to an NvMMBuffer object.
        */
        void returnEmptyVideoBuffer(NvMMBuffer *pBuffer);

        /**
        * Returns to the HAL a pointer to an NvMMBuffer used for a preview
        * capture, which will be used to pass commands to and from the HAL.
        *
        * @param pBuffer A pointer to an NvMMBuffer object.
        */
        void returnEmptyPreviewBuffer(NvMMBuffer *pBuffer);

        /**
        * Sends a buffer to the JPEG encoder for encoding.
        */
        void FeedJpegEncoder(NvMMBuffer *pBuffer);

        /**
        * Waits for the HAL to tell us that it has returned a buffer. Must have
        * called \c Lock() before calling this, as it will wait on a condition
        * variable, and that operation must be done in a locked state.
        */
        void WaitForJpegReturnSignal();

        /**
        * Sends postview callback with the buffer that was passed in, if one
        * was requested.
        */
        void HandlePostviewCallback(NvMMBuffer *pBuffer);

        /**
        * Sets bracket capture as specified by settings in BracketCapture.
        *
        * @param BracketCapture Contains valid bracket capture settings.
        * @return NV_Error
        */
        NvError SetBracketCapture(NvMMBracketCaptureSettings &BracketCapture);

    private:
        NvCameraHal *mHal;
    };

    /**
    * Class used by video, still, and preview post processing.
    */
    class NvCameraPostProcess
    {
    public:

        /**
        * Base class constructor. Establishes state members.
        */
        NvCameraPostProcess();

        /**
        * Base class destructor. Releases any allocated members.
        */
        virtual ~NvCameraPostProcess();

        /**
        * Initializes the base class with the hosting HAL object.
        *
        * @param pCameraHal A pointer to a valid HAL object.
        *
        * @return NV_Error if the pointer is invalid or initialization has run
        * out of memory.
        */
        virtual NvError Initialize(NvCameraHal *pCameraHal);

        /**
        * Processes a single buffer and indicates if an output is ready.
        *
        * @param pInputBuffer A pointer to a valid input buffer.
        * @param OutputReady A handle set to NV_TRUE if post processing has an
        * output ready.
        *
        * @return NV_Error if parameters are invalid, or the class is not
        * initialized.
        */
        virtual NvError ProcessBuffer(NvMMBuffer *pInputBuffer,NvBool &OutputReady);

        /**
        * Copies a valid post processing output NvMMBuffer into the buffer
        * referenced by the parameter.
        *
        * @param pOutputBuffer A pointer to a valid buffer.
        *
        * @return NV_Error if the parameters are invalid, the class is not
        * initialized, or there is no ready output.
        */
        virtual NvError GetOutputBuffer(NvMMBuffer *pOutputBuffer);

        /**
        * Checks if post processing is in an enabled state
        *
        * @return NV_TRUE if successful, or NV_FALSE otherwise.
        *
        */
        virtual NvBool  Enabled();

        /**
        * Puts the class into an enabled state, meaning it will try to do
        * processing.
        *
        * @return NV_Error if the class is not initialized prior to being
        * enabled.
        */
        virtual NvError Enable(NvBool enable);

    protected:

        /**
        * Unmaps an NvRmSurface afer a @c MapSurface call and subsequent data
        * manipulation.
        *
        * @param pSurf A pointer to the surface.
        * @param ptr A uchar pointer previously mapped to the the surface.
        */
        static void    UnmapSurface(NvRmSurface *pSurf, NvU8 *ptr);
        /**
        * Used to map an NvRmSurface to a uchar pointer for direct data
        * access. This function configures caching for the data as well as
        * informs NvRm of software data access. Call @c UnmapSurface after
        * manipulating the data.
        *
        * @param pSurf A pointer to the surface.
        * @param ptr A pointer to the uchar pointer.
        */
        static NvError MapSurface(NvRmSurface *pSurf, NvU8 **ptr);
        /**
        * Temporary member used to store the output of the sample post
        * processing block.
        */
        NvMMBuffer mOutputBuffer;
        /**
        * Internal Boolean flag indicating output readiness.
        */
        NvBool     mOutputReady;
        /**
        * Internal flag indicating the component is enabled and can process
        * inputs.
        */
        NvBool     mEnabled;
        /**
        * Internal flag indicating the class is initialized and can be enabled.
        */
        NvBool     mInitialized;
        /**
        * A reference to the HAL proxy class for communicating with the HAL.
        */
        NvCameraPostProcessHalProxy *mHalProxy;
    };

    /**
    * Class for post processing of still images. Expands on NvCameraPostProcess by
    * adding functionality specific to still images.
    */
    class NvCameraPostProcessStill: public NvCameraPostProcess
    {
    public:
        NvCameraPostProcessStill();

        virtual ~NvCameraPostProcessStill() {  };

        /**
        * Allows post processing to issue commands to the HAL pre-capture. Call
        * prior to a still image capture request.
        *
        * @return NV_Error
        */
        virtual NvError DoPreCaptureOperations(void);

        /**
        * Call immediately after receiving the last image in a still
        * capture. Allows post processing to issue commands to the HAL
        * after capture.
        *
        * @return NV_Error
        */
        virtual NvError DoPostCaptureOperations(void);

        /**
        * Tells the postprocessing class to send input frames to the JPEG
        * encoder. This is useful for applications that wish to allow the
        * user to keep some (or all) of the unprocessed frames. The derived
        * class that implements the postprocessing algorithm is responsible for
        * using this information to feed the frames to FeedJpegEncoder() via
        * the HAL proxy. This design decision was made because the derived
        * class is the only one that will know how it is structuring its alg
        * to run most efficiently (e.g., reusing input frames as outputs), and
        * there could be data races if the processing is not synchronized
        * well with the encoding.
        *
        * This method updates an internal copy of the state, which the
        * derived class can check as it is processing input buffers to see
        * if they should be encoded.
        *
        * Applications should check the return value to make sure that input
        * frame encoding is supported before assuming that input frames will
        * be encoded.
        *
        * @param FramesToEncode A pointer to an array of NvBool values.
        * This array's size must
        * match the size returned by GetAlgorithmInputCount(). The order of
        * entries in this array corresponds to the order the input frames are
        * fed to the algorithm via ProcessBuffer(...). For example, if index 1
        * in the array is TRUE, the second input frame of a particular sequence
        * that is fed to ProcessBuffer(...) gets encoded. Once this array
        * is set, the encoding gets done for every successive input sequence
        * that is fed to the postprocessing class, until a new array is sent
        * to specify some other encode configuration.
        *
        * @param NumInputArrayEntries S[ecifies the number of entries the input
        *  array has. Adds an extra sanity check
        * to force the client to double-check they're sending us something sane.
        *
        * @retval NvError_NotSupported If the class does not support input frame encode.
        * @retval NvError_BadParameter If input parameters are bad.
        * @retval NvSuccess If things go as planned.
        */
        virtual NvError ConfigureInputFrameEncoding(
            const NvBool *FramesToEncode,
            NvU32 NumInputArrayEntries);

        /**
        * Returns the number of input images required to do post processing if
        * the class is enabled; otherwise, returns 1 as the class is in a
        * pass-through mode when disabled.
        * Call when setting up the capture request.
        *
        * @return The required number of input images.
        */
        NvU32 GetInputCount();

        /**
        * Returns the number of input images required to run the post-processing
        * algorithm, regardless of whether post-processing is enabled or
        * disabled at the time this method is called.
        *
        * @return The required number of input images to feed the algorithm.
        */
        virtual NvU32 GetAlgorithmInputCount();

        /**
        * Determines if a postproc class will handle its own encoding.
        * @retval NV_TRUE if the class does its own encoding.
        * @retval NV_FALSE if the client is expected to get the output from
        *                  the class and encode it.
        */
        virtual NvBool EncodesOutput();

        /**
        * Returns the number of output images per capture request. Call when
        * setting up the capture request.
        *
        * @return The number of output images.
        */
        NvU32 GetOutputCount();

        /**
        * If this class sends a buffer to the HAL to be encoded, the HAL must
        * return the buffer to this class when the encoding is done, so that
        * this class can either reuse it for the output or return it to the
        * source.
        *
        * @param Buffer A pointer to the buffer being returned.
        */
        virtual void ReturnBufferAfterEncoding(NvMMBuffer *Buffer);

    protected:

        /**
        * Holds the number of inputs required for one set of outputs, depending
        * on whether or not the algorithm is currently enabled.
        */
        NvU32 mNumberOfInputImages;
        /**
        * Holds the number of inputs required for one set of outputs assuming
        * the algorithm is active, regardless of whether or not it is currently
        * enabled.
        */
        NvU32 mNumberOfAlgorithmInputImages;
        /**
        * Holds the number of expected outputs per capture.
        */
        NvU32 mNumberOfOutputImages;
        /**
        * Holds an array the size of \a mNumberOfAlgorithmInputImages that
        * specifies whether the particular frame in the input frame sequence
        * should be encoded and delivered to the client.
        * Must be allocated by the algorithm's derived instance of this class
        * in the constructor, as that's the only code that will know exactly
        * how many input frames the algorithm requires.
        */
        NvBool *mInputFrameSequenceToEncode;
    };

    /**
     * Not currently implemented. This class may contain future extensions for
     * video post-processing.
     */
    class NvCameraPostProcessVideo: public NvCameraPostProcess
    {
    public:
        virtual ~NvCameraPostProcessVideo() {  };
    };

    /**
    * Not currently implemented. This class may contain future extensions for
    * preview post-processing.
    */
    class NvCameraPostProcessPreview: public NvCameraPostProcess
    {
    public:
        virtual ~NvCameraPostProcessPreview() {  };
    };

}; // namespace android

/** @} */

#endif // NV_CAMERA_HAL_POSTPROCESS_H

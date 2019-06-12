/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Graphics 2D Public API</b>
 *
 * @b Description: The JNI wrapper for the NvBlit interface.
 */

/**
 * @defgroup 2d_wrapper_group 2D Public APIs JNI Wrapper
 *
 * Implements 2D APIs for use by Android applications.
 *
 * <h3>Surface Representation</h3>
 *
 * The API itself does not define a type for a 2D surface, but instead operates
 * on the native surface type of the host platform. For Android this is the
 * <a class="el"
 * href="http://developer.android.com/reference/android/graphics/SurfaceTexture.html"
 * target="_blank">
 * SurfaceTexture</a> class.
 *
 * <h3>Design principles</h3>
 *
 * Internally the API manages a single global context for handling the
 * resources needed to submit commands to the hardware. This is for reasons of
 * simplicity and performance, but it also means that the API is not thread-safe.
 * Therefore, if using the API from multiple threads, the application is
 * responsible for arbitrating the calls to the API.
 *
 * The API itself is stateless, so the results of a particular 2D operation are
 * not dependent on any state left over from previous API calls.
 *
 * <h3>Synchronisation</h3>
 *
 * Synchronisation is handled implicitely inside the API, so any 2D operations
 * are correctly serialised with any other pending operations on a surface.
 * The API does not provide any methods for locking/unlocking surfaces, as these
 * are already implemented by the host platform and its native surface type.
 *
 * @ingroup graphics_modules
 * @{
 */

package com.nvidia.graphics;

import android.graphics.RectF;
import android.graphics.SurfaceTexture;

/**
 * Performs a hardware-accelerated 2D operation.
 */
public class NvBlit {

/** @name Transform
 *
 * Defines the transformation to apply during 2D operations.
 * @see NvBlit.State.setTransform
 */
/*@{*/
    public static final int Transform_None           = 0x0;
    public static final int Transform_Rotate90       = 0x1;
    public static final int Transform_Rotate180      = 0x2;
    public static final int Transform_Rotate270      = 0x3;
    public static final int Transform_FlipHorizontal = 0x4;
    public static final int Transform_InvTranspose   = 0x5;
    public static final int Transform_FlipVertical   = 0x6;
    public static final int Transform_Transpose      = 0x7;
/*@}*/

    /**
     * Disables filtering; hence, simply the nearest pixel to each source
     * coordinate is taken when reading pixels.
     * @see NvBlit.State.setFilter
     */
    public static final int Filter_Nearest = 0x1;

    /**
     * Enables filtering when reading pixels from the source surface. When
     * reading the color value of a source coordinate, the 2D operation
     * reads multiple pixels and computes the average color value. The exact
     * method for this calculation (i.e., how many pixels samples are taken) is
     * not specified, and may change depending on the scaling factor between the
     * source and destination rectangle, and the capabilites of the underlying
     * hardware.
     * @see NvBlit.State.setFilter
     */
    public static final int Filter_Linear  = 0x3;

    /**
     * @brief Absolute color space for source or destination surface.
     * @see NvBlit.State.setColorTransform
     */
    public static final int AbsColorSpace_DeviceRGB     = 0x1;
    public static final int AbsColorSpace_sRGB          = 0x2;
    public static final int AbsColorSpace_AdobeRGB      = 0x3;
    public static final int AbsColorSpace_WideGamutRGB  = 0x4;
    public static final int AbsColorSpace_Rec709        = 0x5;

    /**
     * @brief Link profile type used in color space transformation. The matrix
     * based method gives more accurate results, whereas the LUT based method
     * takes less time to compute. The matrix profile should be used only for
     * transformations where source and destination color spaces are decoded/
     * encoded with sRGB gamma (not e.g. Rec709).
     *
     * Recommended value: CmsLinkProfileType_LUT
     *
     * @see NvBlit.State.setColorTransform
     */
    public static final int CmsLinkProfileType_Matrix   = 0x1;
    public static final int CmsLinkProfileType_LUT      = 0x2;

    /**
     * @brief Initialises the NvBlit API.
     *
     * This method prepares the API for use. Internally it allocates and
     * initialises any resources that are required for using the underlying
     * hardware.
     *
     * Prior to calling any other methods, the application must call this
     * method and verify its return result is true.
     */
    public static boolean open() {
        return nativeOpen() == 0;
    }

    /**
     * @brief Closes the %NvBlit API.
     */
    public static void close() {
        nativeClose();
    }

    /**
     * @brief Prepares the destination surface for 2D operations.
     *
     * This method prepares the specified surface as the destination surface for
     * subsequent 2D operations. It must be used with the NvBlit.frameEnd method
     * as a begin/end pair, between which it is valid to call NvBlit.blit to
     * perform the actual 2D operations on the destination surface.
     *
     * The results of the 2D operation(s) are available after
     * \c %NvBlit.frameEnd has been called. To receive notification when the results
     * are available, the application must register a callback using the
     * \c OnFrameAvailableListener interface on the \c SurfaceTexture class.
     *
     * @pre NvBlit.frameBegin
     *
     * @see <a class="el"
     * href="http://developer.android.com/reference/android/graphics/SurfaceTexture.html"
     * target="_blank">android.graphics.SurfaceTexture</a>, 
     * <a class="el"
     * href="http://developer.android.com/reference/android/graphics/SurfaceTexture.OnFrameAvailableListener.html"
     * target="_blank">
     * SurfaceTexture.OnFrameAvailableListener</a>
     */
    public static boolean frameBegin(SurfaceTexture dstSurface) {
        return nativeFrameBegin(dstSurface) == 0;
    }

    /**
     * @brief Signals the end of 2D operations on a destination.
     *
     * The application must call this method to signal that all 2D operations
     * on the current destination surface have been requested. The %NvBlit API
     * will dispatch the operations to the hardware. The resulting frame will
     * be available some time later, so the application must register a callback
     * using the \c OnFrameAvailableListener interface on the
     * \c SurfaceTexture class.
     *
     * @see <a class="el"
     * href="http://developer.android.com/reference/android/graphics/SurfaceTexture.html"
     * target="_blank">android.graphics.SurfaceTexture</a>, 
     * <a class="el"
     * href="http://developer.android.com/reference/android/graphics/SurfaceTexture.OnFrameAvailableListener.html"
     * target="_blank">
     * SurfaceTexture.OnFrameAvailableListener</a>
     */
    public static void frameEnd() {
        nativeFrameEnd();
    }

    /**
     * @brief Performs hardware-accelerated 2D operations.
     *
     * This method takes the specified state object that describes a 2D
     * operation, and uses it to update the destination surface. Therefore the
     * blit operation is parameterised solely by the inputs to this method, and
     * the initial contents of the source and destination surfaces.
     *
     * @pre NvBlit.frameBegin
     *
     * This method can only be called between NvBlit.frameBegin and
     * NvBlit.frameEnd.
     *
     * Correct synchronisation of the hardware operation is managed internally
     * by the API. That is, any prior and pending operations on the surface(s)
     * are added as a precondition to the blit operation using hardware fences.
     * And likewise, fence synchronisation information for signalling the end of
     * the blit is stored back on the surface object(s).
     *
     * @see NvBlit.State
     *
     * @param state  %State object describing the input parameters for the 2D
     *               operation.
     *
     * @return Boolean indicating whether or not the requested operation was
     *         successful.
     */
    public static boolean blit(State state) {
        return nativeBlit(state) == 0;
    }

    private static native int  nativeOpen();
    private static native void nativeClose();
    private static native int  nativeFrameBegin(SurfaceTexture dstSurface);
    private static native void nativeFrameEnd();
    private static native int  nativeBlit(State state);
    private static native void nativeClassInit();
    static {
        System.loadLibrary("nvidia_graphics_jni");
        nativeClassInit();
    }

    /**
     * @brief Holds input state for 2D operation.
     *
     * This object encapsulates all input state to the 2D operation. This
     * includes:
     * - Source surface
     * - Source rectangle
     * - Destination rectangle
     * - Transformation
     * - Filter mode
     * - Source color
     *
     * This object should be considered opaque by the application. For managing
     * which parameters are set, the following helper methods are provided:
     * - NvBlit.State.clear
     * - NvBlit.State.setSrcSurface
     * - NvBlit.State.setSrcRect
     * - NvBlit.State.setDstRect
     * - NvBlit.State.setTransform
     * - NvBlit.State.setFilter
     * - NvBlit.State.setSrcColor
     */
    public static class State {

        public State() {
            nativeInit();
        }

        /**
         * @brief Clears the state object.
         *
         * This is a helper method for clearing the state object. After calling
         * this method, the parameters stored in the state object are all unset.
         */
        public void clear() {
            nativeClear();
        }

        /**
         * @brief Sets the source surface.
         *
         * Sets the specified surface as the source surface parameter for the 2D
         * operation.
         *
         * @see <a class="el"
         * href="http://developer.android.com/reference/android/graphics/SurfaceTexture.html"
         * target="_blank">android.graphics.SurfaceTexture</a>
         *
         * @param srcSurface  Source surface.
         */
        public void setSrcSurface(SurfaceTexture srcSurface) {
            nativeSetSrcSurface(srcSurface);
        }

        /**
         * @brief Sets the source rectangle.
         *
         * Sets the specified rectangle as the source rectangle parameter for
         * the 2D operation. This rectangle must conform to the following
         * requirements:
         * - Must not exceed the bounds of the source surface size.
         * - Must not be inverted.
         * - Must not be degenerate.
         *
         * If no source rectangle is set, then it defaults to a rectangle that
         * is the same size as the source surface.
         *
         * @see <a class="el"
         * href="http://developer.android.com/reference/android/graphics/RectF.html"
         * target="_blank">android.graphics.RectF</a>
         *
         * @param srcRect  Source rectangle.
         */
        public void setSrcRect(RectF srcRect) {
            nativeSetSrcRect(srcRect.left,
                             srcRect.top,
                             srcRect.right,
                             srcRect.bottom);
        }

        /**
         * @brief Sets the destination rectangle.
         *
         * Sets the specified rectangle as the destination rectangle parameter
         * for the 2D operation. This rectangle must conform to the following
         * requirements:
         * - Must not exceed the bounds of the destination surface size.
         * - Must specify the coordinates in whole numbers.
         * - Must not be inverted.
         * - Must not be degenerate.
         *
         * In addition, for YUV surfaces where the U and V planes may be a
         * different width/height to the Y plane, the rectangle has the
         * following extra requirements:
         * - For YUV 420, the rectangle bounds must all be a multiple of 2
         *   pixels.
         * - For YUV 422, the horizontal rectangle bounds must both be a
         *   multiple of 2 pixels.
         * - For YUV 422R, the vertical rectangle bounds must both be a
         *   multiple of 2 pixels.
         *
         * If no destination rectangle is set, then it defaults to a rectangle
         * that is the same size as the destination surface.
         *
         * @see <a class="el"
         * href="http://developer.android.com/reference/android/graphics/RectF.html"
         * target="_blank">android.graphics.RectF</a>
         *
         * @param dstRect  Destination rectangle.
         */
        public void setDstRect(RectF dstRect) {
            nativeSetDstRect(dstRect.left,
                             dstRect.top,
                             dstRect.right,
                             dstRect.bottom);
        }

        /**
         * @brief Sets the transformation.
         *
         * Sets the specified transformation for the 2D operation. This
         * transformation is applied to the source surface when blitting it to
         * the destination surface.
         *
         * @param transform  Transformation to apply.
         */
        public void setTransform(int transform) {
            nativeSetTransform(transform);
        }

        /**
         * @brief Sets the filter.
         *
         * Sets the filter to use when reading pixels from the source surface.
         * This defaults to NvBlit.Filter_Nearest.
         *
         * @param filter  Filter to use.
         */
        public void setFilter(int filter) {
            nativeSetFilter(filter);
        }

        /**
         * @brief Sets the source color.
         *
         * Sets the source color to use when doing a 2D fill operation. The
         * source color value must be in the same format as used by the standard
         * Android Color class.
         *
         * The source color parameter is mutually exclusive to all other
         * parameters in the state object, except for the destination rectangle.
         *
         * @see <a class="el"
         * href="http://developer.android.com/reference/android/graphics/Color.html"
         * target="_blank">android.graphics.Color</a>
         *
         * @param srcColor  Source color value.
         */
        public void setSrcColor(int srcColor) {
            nativeSetSrcColor(srcColor);
        }

        /**
         * @brief Sets a color space transformation.
         *
         * Sets the input and output color spaces, and link profile type for
         * color space transformation.
         *
         * @param srcColorSpace     Source color space.
         * @param dstColorSpace     Destination color space.
         * @param linkProfileType   Link profile type.
         */
        public void setColorTransform(int srcColorSpace, int dstColorSpace, int linkProfileType) {
            nativeSetColorTransform(srcColorSpace, dstColorSpace, linkProfileType);
        }

        protected void finalize() throws Throwable {
            try {
                nativeFinalize();
            } finally {
                super.finalize();
            }
        }

        private int mState;
        private native void nativeInit();
        private native void nativeFinalize();
        private native void nativeClear();
        private native void nativeSetSrcSurface(SurfaceTexture srcSurface);
        private native void nativeSetSrcRect(float left, float top, float right, float bottom);
        private native void nativeSetDstRect(float left, float top, float right, float bottom);
        private native void nativeSetTransform(int transform);
        private native void nativeSetFilter(int filter);
        private native void nativeSetSrcColor(int color);
        private native void nativeSetColorTransform(int srcColorSpace, int dstColorSpace, int linkProfileType);
    }
}
/** @} */

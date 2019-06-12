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
 * @b Description: The NVIDIA 2D Public API provides an interface for doing
 * hardware-accelerated 2D operations, such as filling, copying, rotating,
 * scaling, and format conversion of 2D surfaces.
 */

/**
 * @defgroup 2d_group 2D Public APIs
 *
 * Implements 2D APIs for use by applications.
 *
 * <h3>Surface Representation</h3>
 *
 * The API itself does not define a type for a 2D surface, but instead operates
 * on the native surface type of the host platform. For Android this is the
 * \a buffer_handle_t object, which is the same type used by the HAL gralloc and
 * hwcomposer modules, and also the internal buffer type used in the Android
 * GLConsumer object.
 *
 * The API does not provide any functions for locking/unlocking surfaces, as
 * these are already implemented by the host platform and its native surface
 * type. On Android these are provided by the HAL gralloc module.
 *
 * <h3>Design principles</h3>
 *
 * To use the API the client must maintain a NvBlit context object, which is
 * passed as a parameter to each API function call. The API is thread safe, so
 * the client can use the same context object concurrently from multiple
 * threads.
 *
 * The API itself is stateless with respect to hardware usage, so the results of
 * a particular 2D operation are not dependent on any state left over from
 * previous API calls.
 *
 * <h3>Synchronisation</h3>
 *
 * Syncronisation of blit operations must be considered by clients of the API,
 * to ensure any 2D operations are correctly serialised with any other pending
 * operations on a surface. The API itself exposes explicit parameters for
 * synchronisation, both for read access of source surfaces and for write access
 * of destination surfaces.
 *
 * The API does not define a type for these synchronisation parameters but
 * instead relies on the native sync object type of the host platform. On
 * Android this is implemented as a file descriptor, and can be used across
 * modules that conform to the Android sync framework.
 *
 * @ingroup graphics_modules
 * @{
 */

#ifndef INCLUDED_NVBLIT_H
#define INCLUDED_NVBLIT_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvcolor.h"
#include "nvcms.h"
#include "nvcms_context.h"
#include "nvcms_profile.h"

#ifdef ANDROID
#include <system/window.h>
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

#ifdef ANDROID
/**
 * @brief Definition of native surface type.
 */
typedef buffer_handle_t NvBlitSurface;

/**
 * @brief Definition of native synchronisation object.
 */
typedef int NvBlitSync;
#endif

#define NVBLIT_FORCE32(n) n##_Force32 = 0x7FFFFFFF

/**
 * @brief Defines the transformation to apply during 2D operations.
 * @see NvBlitSetTransform
 */
typedef enum
{
    NvBlitTransform_None = 0x0,
    NvBlitTransform_Rotate90,
    NvBlitTransform_Rotate180,
    NvBlitTransform_Rotate270,
    NvBlitTransform_FlipHorizontal,
    NvBlitTransform_InvTranspose,
    NvBlitTransform_FlipVertical,
    NvBlitTransform_Transpose,

    NVBLIT_FORCE32(NvBlitTransform)
} NvBlitTransform;

/**
 * @brief Defines the filter mode to use when reading source surface.
 * @see NvBlitSetFilter
 */
typedef enum
{
    /**
     * Disables filtering; hence, simply the nearest pixel to each source
     * coordinate is taken when reading pixels.
     */
    NvBlitFilter_Nearest = 0x1,

    /**
     * Enables filtering when reading pixels from the source surface. When
     * reading the color value of a source coordinate, the 2D operation
     * reads multiple pixels and computes the average color value. The exact
     * method for this calculation (i.e., how many pixels samples are taken) is
     * not specified, and may change depending on the scaling factor between the
     * source and destination rectangle, and the capabilites of the underlying
     * hardware.
     */
    NvBlitFilter_Linear  = 0x3,

    NVBLIT_FORCE32(NvBlitFilter)
} NvBlitFilter;

/**
 * @brief Defines the indicator for which state parameters are valid.
 *
 * This is used internally in the ::NvBlitState object for tracking which
 * parameters have been set and contain valid values.
 */
typedef enum
{
    NvBlitState_SrcSurface     = 1 << 0,
    NvBlitState_DstSurface     = 1 << 1,
    NvBlitState_SrcRect        = 1 << 2,
    NvBlitState_DstRect        = 1 << 3,
    NvBlitState_Transform      = 1 << 4,
    NvBlitState_Filter         = 1 << 5,
    NvBlitState_SrcColor       = 1 << 6,
    NvBlitState_ColorTransform = 1 << 7,

    NVBLIT_FORCE32(NvBlitState)
} NvBlitStateField;

/**
 * @brief Defines flags for controlling behaviour of the ::NvBlit function.
 * @see NvBlitSetFlag
 */
typedef enum
{
    /**
     * Modifies behaviour of the \c NvBlit function so that the commands for the
     * 2D operation are not immediately submitted to the hardware. The
     * application can then control when all pending commands are submitting by
     * calling ::NvBlitFlush. Refer to the \c NvBlit and \c NvBlitFlush
     * functions for details on how to use this flag.
     */
    NvBlitFlag_DeferFlush = 1 << 0,

    /**
     * Enables surface dumping for a single call to the \c NvBlit function.
     * See ::NVBLIT_CONFIG_DUMP for more information on surface dumping.
     */
    NvBlitFlag_DumpSurfaces = 1 << 1,

    /**
     * Enables trace logging for a single call to the \c NvBlit function.
     * See ::NVBLIT_CONFIG_TRACE for more information on this logging.
     */
    NvBlitFlag_Trace = 1 << 2,

    /**
     * Enables recording of profile information for a single call to the
     * \c NvBlit function.
     * See ::NVBLIT_CONFIG_PROFILE for more information on this profiling.
     */
    NvBlitFlag_Profile = 1 << 3,

    NVBLIT_FORCE32(NvBlitFlag)
} NvBlitFlag;

/**
 * @brief Holds rectangle coordinates.
 *
 * This object uses floats for storing the coordinates to allow sub-pixel
 * precision when specifying the source rectangle for a 2D operation. When used
 * as a destination rectangle, the float coordinates are internally rounded down
 * to the nearest whole number prior to the 2D operation.
 *
 * There are further restrictions on the rectangle coordinates, depending on
 * whether it is being used as a source or destination rectangle. See the
 * ::NvBlitSetSrcRect and ::NvBlitSetDstRect functions for these restrictions.
 */
typedef struct NvBlitRectRec
{
    float left;
    float top;
    float right;
    float bottom;
} NvBlitRect;

/**
 * @brief Holds input state for 2D operations.
 *
 * This object encapsulates all input states to the 2D operation. This includes:
 * - Source surface
 * - Source rectangle
 * - Destination rectangle
 * - Syncronisation object for source surface
 * - Syncronisation object for destination surface
 * - Transformation
 * - Filter mode
 * - Source color
 * - Color space transformation
 * - Flags for the ::NvBlit function
 *
 * @see NvBlitSurface, NvBlitRect, NvBlitTransform, NvBlitFilter
 *
 * This object should be considered opaque by the application. For managing
 * which parameters are set, the following helper functions are provided:
 * - ::NvBlitClearState
 * - ::NvBlitSetSrcSurface
 * - ::NvBlitSetSrcRect
 * - ::NvBlitSetDstRect
 * - ::NvBlitSetTransform
 * - ::NvBlitSetFilter
 * - ::NvBlitSetSrcColor
 * - ::NvBlitSetColorTransform
 * - ::NvBlitSetFlag
 */
typedef struct NvBlitStateRec
{
    NvU32              ValidFields;
    NvU32              Flags;
    NvBlitSurface      SrcSurface;
    NvBlitSurface      DstSurface;
    NvBlitSync         SrcSync;
    NvBlitSync         DstSync;
    NvBlitRect         SrcRect;
    NvBlitRect         DstRect;
    NvBlitTransform    Transform;
    NvBlitFilter       Filter;
    NvU32              SrcColor;
    NvColorFormat      SrcColorFormat;
    NvCmsAbsColorSpace SrcColorSpace;
    NvCmsAbsColorSpace DstColorSpace;
    NvCmsProfileType   ColorProfileType;
} NvBlitState;

/**
 * @brief Holds result information for a 2D operation.
 *
 * This object encapsulates all result information from the 2D operation. This
 * includes:
 * - Syncronisation object for blit operation
 * - Description of any error that occured
 *
 * This object should be considered opaque by the application. For querying
 * parameters from the results, the following helper functions are provided:
 * - ::NvBlitGetSync
 * - ::NvBlitGetErrorString
 */
typedef struct NvBlitResultRec
{
    NvBlitSync  ReleaseSync;
    NvPoint     DstOffset;
    const char *ErrorString;
} NvBlitResult;

/**
 * @brief Defines capabilites of the 2D hardware.
 *
 * This object encapsulates a subset of the capabilites and restrictions of the
 * 2D hardware. This includes:
 * - Supported features, as a bitmask of NvBlitStateField
 * - Maximum surface dimensions
 * - Minumum and maximum scaling factor
 *
 * @see NvBlitQuery, NvBlitStateField
 */
typedef struct NvBlitCapabilityRec
{
    NvU32 Features;
    NvU32 MaxWidth;
    NvU32 MaxHeight;
    float MinScale;
    float MaxScale;
} NvBlitCapability;

/**
 * @brief Opaque handle for the NvBlit API context object.
 *
 * @see NvBlitOpen
 */
typedef struct NvBlitContextRec NvBlitContext;

/**
 * @brief Initialises the NvBlit API.
 *
 * This function prepares the API for use. Internally it allocates and
 * initialises any resources that are required for using the underlying
 * hardware.
 *
 * Prior to calling any other functions, the application must call this function
 * and verify its return result is \c NvSuccess.
 *
 * @param ctx  Pointer to return the newly allocated NvBlitContext.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvBlitOpen(NvBlitContext **ctx);

/**
 * @brief Closes the NvBlit API.
 */
void
NvBlitClose(NvBlitContext *ctx);

/**
 * @brief Performs a hardware-accelerated 2D operation.
 *
 * This function takes the specified state object that describes a 2D operation,
 * and uses it to update the specified destination surface. Therefore the blit
 * operation is parameterised solely by the inputs to this function, and the
 * initial contents of the source and destination surfaces.
 *
 * Correct synchronisation of the hardware operation is managed internally by
 * the API. That is, any prior and pending operations on the surface(s) are
 * added as a precondition to the blit operation using hardware fences. And
 * likewise, fence synchronisation information for signalling the end of
 * the blit is returned to the client.
 *
 * The result object parameter is used to return information back to the client.
 * This includes a sync object that will be signalled once the 2D operation is
 * complete. The client takes ownership of this sync object and so is
 * responsible for closing/releasing it. If the client specifies a NULL pointer
 * for this parameter then a CPU wait will be done within the function, and once
 * the function returns the 2D operation will be completed.
 *
 * The default behaviour of this function is to both generate the commands for
 * doing the operation, and to submit/flush those commands to the hardware.
 *
 * Flushing the command stream involves a CPU cost, so to mitigate the cost,
 * this behaviour can be changed by using the ::NvBlitFlag_DeferFlush flag. When
 * this flag is used, the \c NvBlit function does not flush the commands
 * implicitely, but instead the application must take responsibility for doing
 * this later using the ::NvBlitFlush function. This allows the application to
 * do multiple blits and then a single flush after them, and pay only the CPU
 * cost of a single flush.
 *
 * When using the \a NvBlitFlag_DeferFlush flag, the application must ensure
 * that \c NvBlitFlush is called prior to the affected surfaces (both source and
 * destination) being used by any other software/hardware modules. In addition,
 * the surfaces cannot be deleted between the \c NvBlit and \c NvBlitFlush
 * calls, as the API will still attempt to reference them during the flush
 * operation.
 *
 * @see NvBlitState, NvBlitSurface, NvBlitFlag, NvBlitFlush
 *
 * @param ctx     API context object.
 * @param state   A pointer to the state object describing the input parameters
                  for the 2D operation.
 * @param result  A pointer to the result object where information about the 2D
 *                operation will be returned.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvBlit(NvBlitContext *ctx,
       const NvBlitState *state,
       NvBlitResult *result);

/**
 * @brief Submits any pending commands to the hardware.
 *
 * This function is only needed when using the ::NvBlitFlag_DeferFlush flag.
 *
 * Independent of the return result, after this function exits, all surfaces
 * previously used during ::NvBlit are no longer referenced by the NvBlit API.
 *
 * The result object parameter is used to return information back to the client.
 * This includes a sync object that will be signalled once all previous 2D
 * operations are complete. The client takes ownership of this sync object and
 * so is responsible for closing/releasing it. If the client specifies a NULL
 * pointer for this parameter then a CPU wait will be done within the function,
 * and once the function returns the 2D operation will be completed.
 *
 * @see NvBlit, NvBlitFlag
 *
 * @param ctx     API context object.
 * @param result  A pointer to the result object where information about the 2D
 *                operation will be returned.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvBlitFlush(NvBlitContext *ctx,
            NvBlitResult *result);

/**
 * @brief Returns the capabilities of the 2D hardware.
 *
 * This function returns a definition of the capabilities of the 2D hardware.
 * This allows the client to do their own validation before attempting a 2D
 * blit operation.
 *
 * @see NvBlitCapability
 *
 * @param ctx  API context object.
 *
 * @retval Pointer to an NvBlitCapability object.
 */
const NvBlitCapability *
NvBlitQuery(NvBlitContext *ctx);

/**
 * @brief Clears the specified state object.
 *
 * This is a helper function for clearing a state object. This must be called
 * before starting to set any parameters on the state object. After calling this
 * function, the parameters stored in the state object are all unset.
 *
 * @see NvBlitState
 *
 * @param state A pointer to the state object to clear.
 */
static NV_INLINE void
NvBlitClearState(NvBlitState *state)
{
    state->ValidFields = 0;
    state->Flags = 0;
}

/**
 * @brief Sets the source surface.
 *
 * Sets the specified surface as the source surface parameter for the 2D
 * operation.
 *
 * This function can also be used to unset the source surface by passing in a
 * NULL parameter.
 *
 * The acquire sync object is used to ensure the 2D operation does not start
 * reading from the surface until that sync object has been signalled. The
 * NvBlit API will take ownership of the sync object during the call to ::NvBlit
 * but only if that call returns with no error. Once the NvBlit API takes
 * ownership of the sync object, it will ensure it is closed/released when it is
 * no longer needed. If the call to ::NvBlit returns an error then the client
 * retains ownership of the sync object.
 *
 * @see NvBlitState, NvBlitSurface
 *
 * @param state       A pointer to the state object to update.
 * @param srcSurface  Source surface.
 * @param acquireSync Synchronisation object that the 2D operation must wait on
 *                    before reading from the source surface.
 */
static NV_INLINE void
NvBlitSetSrcSurface(NvBlitState *state,
                    NvBlitSurface srcSurface,
                    NvBlitSync acquireSync)
{
    if (srcSurface != NULL)
        state->ValidFields |= NvBlitState_SrcSurface;
    else
        state->ValidFields &= ~NvBlitState_SrcSurface;
    state->SrcSurface = srcSurface;
    state->SrcSync = acquireSync;
}

/**
 * @brief Sets the destination surface.
 *
 * Sets the specified surface as the destination surface parameter for the 2D
 * operation.
 *
 * This function can also be used to unset the destination surface by passing in
 * a NULL parameter.
 *
 * The acquire sync object is used to ensure the 2D operation does not start
 * writing to the surface until that sync object has been signalled. The NvBlit
 * API takes ownership of the sync object and will close/release it when no
 * longer needed.
 *
 * @see NvBlitState, NvBlitSurface
 *
 * @param state       A pointer to the state object to update.
 * @param dstSurface  Destination surface.
 * @param acquireSync Synchronisation object that the 2D operation must wait on
 *                    before writing to the destination surface.
 */
static NV_INLINE void
NvBlitSetDstSurface(NvBlitState *state,
                    NvBlitSurface dstSurface,
                    NvBlitSync acquireSync)
{
    if (dstSurface != NULL)
        state->ValidFields |= NvBlitState_DstSurface;
    else
        state->ValidFields &= ~NvBlitState_DstSurface;
    state->DstSurface = dstSurface;
    state->DstSync = acquireSync;
}

/**
 * @brief Sets the source rectangle.
 *
 * Sets the specified rectangle as the source rectangle parameter for the 2D
 * operation. This rectangle must conform to the following requirements:
 * - Must not exceed the bounds of the source surface size.
 * - Must not be inverted.
 * - Must not be degenerate.
 *
 * If no source rectangle is set, then it defaults to a rectangle that is the
 * same size as the source surface.
 *
 * This function can also be used to unset the source rectangle by passing in a
 * NULL parameter.
 *
 * @see NvBlitState, NvBlitRect
 *
 * @param state    A pointer to the state object to update.
 * @param srcRect  A pointer to the source rectangle.
 */
static NV_INLINE void
NvBlitSetSrcRect(NvBlitState *state,
                 const NvBlitRect *srcRect)
{
    if (srcRect != NULL)
    {
        state->ValidFields |= NvBlitState_SrcRect;
        state->SrcRect = *srcRect;
    }
    else
    {
        state->ValidFields &= ~NvBlitState_SrcRect;
    }
}

/**
 * @brief Sets the destination rectangle.
 *
 * Sets the specified rectangle as the destination rectangle parameter for the
 * 2D operation. This rectangle must conform to the following requirements:
 * - Must not exceed the bounds of the destination surface size.
 * - Must specify the coordinates in whole numbers.
 * - Must not be inverted.
 * - Must not be degenerate.
 *
 * In addition, for YUV surfaces where the U and V planes may be a different
 * width/height to the Y plane, the rectangle has the following extra
 * requirements:
 * - For YUV 420, the rectangle bounds must all be a multiple of 2 pixels
 * - For YUV 422, the horizontal rectangle bounds must both be a multiple of 2
 *   pixels
 * - For YUV 422R, the vertical rectangle bounds must both be a multiple of 2
 *   pixels.
 *
 * If no destination rectangle is set, then it defaults to a rectangle that is
 * the same size as the destination surface.
 *
 * This function can also be used to unset the source rectangle by passing in a
 * NULL parameter.
 *
 * @see NvBlitState, NvBlitRect
 *
 * @param state    A pointer to a state object to update.
 * @param dstRect  A pointer to a destination rectangle.
 */
static NV_INLINE void
NvBlitSetDstRect(NvBlitState *state,
                 const NvBlitRect *dstRect)
{
    if (dstRect != NULL)
    {
        state->ValidFields |= NvBlitState_DstRect;
        state->DstRect = *dstRect;
    }
    else
    {
        state->ValidFields &= ~NvBlitState_DstRect;
    }
}

/**
 * @brief Sets the transformation.
 *
 * Sets the specified transformation for the 2D operation. This transformation
 * is applied to the source surface when blitting it to the destination surface.
 *
 * @see NvBlitState, NvBlitTransform
 *
 * @param state      A pointer to a state object to update.
 * @param transform  Transformation to apply.
 */
static NV_INLINE void
NvBlitSetTransform(NvBlitState *state,
                   NvBlitTransform transform)
{
    state->ValidFields |= NvBlitState_Transform;
    state->Transform = transform;
}

/**
 * @brief Sets the filter.
 *
 * Sets the filter to use when reading pixels from the source surface. This
 * defaults to ::NvBlitFilter_Nearest.
 *
 * @see NvBlitState, NvBlitFilter
 *
 * @param state   A pointer to the state object to update.
 * @param filter  Filter to use.
 */
static NV_INLINE void
NvBlitSetFilter(NvBlitState *state,
                NvBlitFilter filter)
{
    state->ValidFields |= NvBlitState_Filter;
    state->Filter = filter;
}

/**
 * @brief Sets the source color.
 *
 * Sets the source color to use when doing a 2D fill operation. The source color
 * value is interpreted in the specified color format. Prior to filling a
 * surface, the color value is converted to match the same color format as the
 * destination surface.
 *
 * The source color parameter is mutually exclusive to all other parameters in
 * the state object, except for the destination rectangle.
 *
 * @see NvBlitState, NvColorFormat
 *
 * @param state           A pointer to the state object to update.
 * @param srcColor        Source color value.
 * @param srcColorFormat  Color format of source color value.
 */
static NV_INLINE void
NvBlitSetSrcColor(NvBlitState *state,
                  NvU32 srcColor,
                  NvColorFormat srcColorFormat)
{
    state->ValidFields |= NvBlitState_SrcColor;
    state->SrcColor = srcColor;
    state->SrcColorFormat = srcColorFormat;
}

/**
 * @brief Sets the color space transformation.
 *
 * Sets the color space for the source and destination surfaces, and enables a
 * color space transformation between the two during the 2d operation.
 *
 * The method of computing the color transformation is controlled by the
 * colorProfileType parameter, allowing the application to specify either a
 * matrix or LUT based approach. The matrix based method gives more accurate
 * results, whereas the LUT based method takes less time to compute.
 *
 * @see NvBlitState
 *
 * @param state             State object to update.
 * @param srcColorSpace     Color space of source surface.
 * @param dstColorSpace     Color space of destination surface.
 * @param colorProfileType  Color transformation method.
 */
static NV_INLINE void
NvBlitSetColorTransform(NvBlitState *state,
                        NvCmsAbsColorSpace srcColorSpace,
                        NvCmsAbsColorSpace dstColorSpace,
                        NvCmsProfileType colorProfileType)
{
    state->ValidFields |= NvBlitState_ColorTransform;
    state->SrcColorSpace = srcColorSpace;
    state->DstColorSpace = dstColorSpace;
    state->ColorProfileType = colorProfileType;
}

/**
 * @brief Sets a NvBlit flag.
 *
 * Sets the specified flag, allowing the behaviour of the \c NvBlit function to
 * be modified.
 *
 * @see NvBlitState, NvBlitFlag
 *
 * @param state  A pointer to the state object to update.
 * @param flag   Flag to set.
 */
static NV_INLINE void
NvBlitSetFlag(NvBlitState *state,
              NvBlitFlag flag)
{
    state->Flags |= flag;
}

/**
 * @brief Gets the synchronisation object for the blit.
 *
 * Gets the synchronisation object from the blit results, which can be used
 * to synchronisation against the completion of the 2D operation. See ::NvBlit
 * and ::NvBlitFlush for more information about this synchronisation.
 *
 * @see NvBlitResult
 *
 * @param result  A pointer to the result object to query.
 *
 * @retval Synchronisation object that will be signalled when the 2D operation
 *         is complete.
 */
static NV_INLINE NvBlitSync
NvBlitGetSync(const NvBlitResult *result)
{
    return result->ReleaseSync;
}

/**
 * @brief Gets an error description for the blit.
 *
 * If an error occured during the 2D operation then this function can be used
 * to retrieve a textual descrption of the error.
 *
 * @see NvBlitResult
 *
 * @param result  A pointer to the result object to query.
 *
 * @retval NULL if no error occured, otherwise an error description string.
 */
static NV_INLINE const char *
NvBlitGetErrorString(const NvBlitResult *result)
{
    return result->ErrorString;
}

/**
 * @defgroup config Configuration Variables
 *
 * Configuration variables provide a way of enabling and disabling debug
 * features of the NvBlit API.
 *
 * On Android, these use the standard system properties mechanism. The config
 * variables can be stored in the system properties, and should be prefixed with
 * the string "persist.tegra". For example, to enable surface dumping, use the
 * following command to set the correspnding system property:
 * @code
 *   adb shell setprop "persist.tegra.nvblit.dump" 1
 * @endcode
 */

/**
 * @brief Enables profile information.
 * @ingroup config
 *
 * A value of 0 or 1 disables/enables this option. When enabled, timings of
 * important ::NvBlit functions are recorded. When ::NvBlitClose is called, this
 * profile information is written to the log.
 */
#define NVBLIT_CONFIG_PROFILE    "nvblit.profile"

/**
 * @brief Enables function log tracing.
 * @ingroup config
 *
 * A value of 0 or 1 disables/enables this option. It is only supported for
 * debug builds of the NvBlit API. When enabled, entry and exit of important
 * NvBlit functions are written to the log.
 */
#define NVBLIT_CONFIG_TRACE      "nvblit.trace"

/**
 * @brief Enables surface dumping.
 * @ingroup config
 *
 * A value of 0 or 1 disables/enables this option. When enabled, source and
 * destination surfaces are dumped before and after each call to ::NvBlit.
 * The surface dump files are written to the directory stored in the
 * ::NVBLIT_CONFIG_DUMP_PATH config variable. If this directory does not exist,
 * no surface dumps will be written.
 */
#define NVBLIT_CONFIG_DUMP       "nvblit.dump"

/**
 * @brief Controls location of surface dumps.
 * @ingroup config
 *
 * This sets the file system directory into which surface dumps will be written.
 * The default path is:
 * <pre>
 *     /data/local/nvblit/
 * </pre>
 */
#define NVBLIT_CONFIG_DUMP_PATH  "nvblit.dump_path"

#if defined(__cplusplus)
}
#endif

/** @} */
#endif /* INCLUDED_NVBLIT_H */

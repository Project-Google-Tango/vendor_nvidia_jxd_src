/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * \file nvddk_2d_v2.h
 * \brief NvDdk 2d public API
 *
 * \mainpage
 * NvDdk2d is NVIDIA's 2D interface for hardware assisted 2D rendering
 * operations on embedded platforms. It provides an interface for drivers
 * to do 2d blit and primitive drawing operations and uses hardware
 * modules on the SOC to accelerate them when possible.
 *
 * The interface is part of the NVIDIA Driver Development Kit, namely NvDDK
 *
 * \section main_design Design principles
 *
 * - NvDdk 2d handles surface synchronization internally. This means
 *    that it is necessary to wrap NvRmSurfaces into NvDdk2dSurface
 *    objects.
 * - Primitive drawing and blitting are sufficiently different to
 *    warrant having to different entry points. However, trying to
 *    define different types of blits leads to API confusiuon, so
 *    there is only a single Blit entry point.
 * - NvDdk 2d is not a state machine, therefore using state setters
 *    complicates the API and/or requires large amounts of unnecessary
 *    state to be set.
 * - The NvDdk 2d blit entry point takes all the parameters necessary
 *    for 'every' blit as explicit arguments and everything else is
 *    passed in via a struct.
 * - A 'fields' mask is used to indicate which fields of the parameter
 *    struct contain useful information. Helper functions will be
 *    provided for setting values in the parameter struct that
 *    automatically handle the fields mask.
 *
 * \section api_notes API considerations
 *
 * All functions that can fail have NvError as their return type.  The
 * most typical error conditions you may see are:
 *
 * - NvSuccess: The operation was successful.
 * - NvError_NotSupported: The operation was not implemented/not
 *    supported.  Can easily happen with some types of ROPs, brush or
 *    surface color formats.
 * - NvError_InsufficientMemory: Ran out of memory.
 *
 * \section concurrency Concurrency
 *
 * Hardware access is arbitrated by RM. Access to a specific Ddk2d state
 * is protected by an internal mutex, i.e., only one thread can access
 * the 2D handle at the same time. Trivial getters are an exception, though.
 *
 * Internal mutex cannot guard against situations where resource destruction
 * is concurrent with other relevant API use. This means that, for example,
 * user must guarantee that before calling NvDdk2dClose() all other threads
 * have stopped making API calls. Same applies for specific surfaces when
 * destroying them.
 *
 * Please see the Modules tab or the File tab for more details on the API.
 */

#ifndef INCLUDED_NVDDK_2D_H
#define INCLUDED_NVDDK_2D_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"
#include "nvcolor.h"
#include "nvfxmath.h"
#include "nvrm_surface.h"
#include "nvrm_channel.h"

// Maximum supported surface size for overall surface creation in Ddk2d (pixel)
#define NVDDK_2D_MAX_SURFACE_WIDTH      5120
#define NVDDK_2D_MAX_SURFACE_HEIGHT     5120

/** Maximum number of pixel buffers in a surface */
#define NVDDK_2D_MAX_SURFACE_BUFFERS 3

/** Maximum number of fences in a fence list for a lock/unlock operation */
#define NVDDK_2D_MAX_FENCES 6

/*---------------------------------------------------------*/
/** \defgroup util Utility types */

/**
 * \brief Fixed point rectangle
 * \ingroup util
 */
typedef struct
{
    NvSFx left;
    NvSFx top;
    NvSFx right;
    NvSFx bottom;
} NvDdk2dFixedRect;

/**
 * \brief A single color sample in arbitrary color format.
 *
 * Usually accessed via nvddk2d utility funtions.
 * \ingroup util
 */
typedef struct
{
    NvColorFormat   Format;
    NvU8            Value[4*4];
} NvDdk2dColor;

/**
 * An opaque handle to the NvDdk2d context.
 */
typedef struct NvDdk2dRec *NvDdk2dHandle;

/*---------------------------------------------------------*/
/** \defgroup surface Synchronized surfaces
 *
 * TODO explain high-level usage of surfaces
 */

/*
 * TODO: [ahatala 2009-02-04] document surface abstraction
 */

/**
 * \brief An opaque handle to an NvDdk2d surface
 *
 * An NvDdk2d surface is a wrapper around a single or multiple
 * NvRmSurfaces.
 *
 * All NvDdk2d surface operations operate on NvDdk2dSurface's,
 * it's not possible to use NvRmSurface's directly.
 *
 * Examples single and multi surfaces:
 * - One surface: Single NvColorFormat_A8R8G8B8 surface
 * - Three surfaces: UUV multiplanar surface
 *
 * \ingroup surface
 */
typedef struct NvDdk2dSurfaceRec NvDdk2dSurface;



/**
 * \brief Surface type that describes meaning and number of
 *        sub-surfaces in an NvDdk2dSurface
 **/
typedef enum
{
    /** A single pixel buffer, of any color format */
    NvDdk2dSurfaceType_Single = 0x1,

    /**
     * Three buffers intepreted as values of Y, U and V respectively.
     * The U and V buffers are commonly subsampled (2x2 or 2x1) resulting
     * in the three buffers not necessarily being of the same dimensions.
     * The pixel format of the buffers must be NvColorFormat_Y8,
     * NvColorFormat_U8 and NvColorFormat V8, in that order.
     */
    NvDdk2dSurfaceType_Y_U_V,

    /**
     * Two buffers with the first one containing values for Y and the
     * second one U and V values packed together, in that order.
     */
    NvDdk2dSurfaceType_Y_UV,

    NvDdk2dSurfaceType_Last,
    NvDdk2dSurfaceType_Force32 = 0x7FFFFFFF
} NvDdk2dSurfaceType;

/** \brief Access mode for locking the surface memory */
typedef enum
{
    /** Read only */
    NvDdk2dSurfaceAccessMode_Read = 0x1,
    /** Write only */
    NvDdk2dSurfaceAccessMode_Write = 0x2,
    /** Read and write access */
    NvDdk2dSurfaceAccessMode_ReadWrite =
      ( NvDdk2dSurfaceAccessMode_Read | NvDdk2dSurfaceAccessMode_Write),
    NvDdk2dSurfaceAccessMode_Force32 = 0x7FFFFFFF,
} NvDdk2dSurfaceAccessMode;

/** \brief Surface repeat mode for Pixman/XRender compatible blits */
typedef enum
{
    NvDdk2dSurfaceRepeatMode_None = 1,
    NvDdk2dSurfaceRepeatMode_Pad,
    NvDdk2dSurfaceRepeatMode_Normal,
    NvDdk2dSurfaceRepeatMode_Reflect,

    NvDdk2dSurfaceRepeatMode_Force32 = 0x7FFFFFFF,
} NvDdk2dSurfaceRepeatMode;

/**
 * \brief Create a wrapper object for an NvRmSurface collection to use it with NvDdk2d API
 *
 * A surface handle allocated with NvDdk2dSurfaceCreate must be
 * destroyed with a matching call to NvDdk2dSurfaceDestroy.
 *
 * \ingroup surface
 */
NvError
NvDdk2dSurfaceCreate(
        NvDdk2dHandle Ddk2d,
        NvDdk2dSurfaceType Type,
        NvRmSurface* Buffers,
        NvDdk2dSurface** Surf);

/**
 * \brief Destroy an NvDdk2dSurface*
 *
 * After Destroy returns, user must deallocate any memory pointed to
 * by the wrapped NvRmSurface's.
 *
 * No external locking or synchronization of the surfaces needs to
 * happen, this is automatically handled in this destroy call.
 *
 * \note Cannot be called on NULL.
 * \ingroup surface
 */
void
NvDdk2dSurfaceDestroy(
        NvDdk2dSurface* Surf);

/** \brief Compute allocation parameters for NvDdk2dSurface buffer.
 *
 * Calculate NvDdk2d compatible alignment, pitch and size of a surface buffer.
 *
 * \param Buffer        Buffer must have valid Width, Height, ColorFormat and Layout fields.
 * \param SysmemHint    Preferred memory type. NvDdk2d may change the memory type for performance reasons.
 *                      The effective memory type is returned via this parameter.
 * \param Alignmen      The required alignment of the buffer. Can be ingnored by passing NULL.
 * \param Pitch         The required pitch of the buffer. Can be ingnored by passing NULL.
 * \param Size          The required size of the buffer. Can be ingnored by passing NULL.
 */
NvError
NvDdk2dSurfaceComputeBufferParams(NvDdk2dHandle Ddk2d,
                                  NvRmSurface* Buffer,
                                  NvBool* SysmemHint,
                                  int* Alignment,
                                  int* Pitch,
                                  int* Size);

/** \brief Get surface type */
NvDdk2dSurfaceType
NvDdk2dSurfaceGetType(
        const NvDdk2dSurface* Surf);

/** \brief Return the number of surface buffers */
NvU32
NvDdk2dSurfaceGetNumBuffers(
        const NvDdk2dSurface* Surf);

/**
 * \brief Get surface buffer
 */
void
NvDdk2dSurfaceGetBuffer(
        NvDdk2dSurface* Surf,
        NvU32 BufferIndex,
        NvRmSurface* Buf);

/**
 * \brief Lock a surface for reading and/or writing
 *
 * To be able to access the buffers of a surface outside of the nvddk2d
 * implementation the surface needs to be locked. This holds true both
 * for manipulating the buffer memory directly from the CPU as well as
 * accessing the buffer memory from other hardware units. Failure to
 * lock the surface properly will result in read/write operations
 * potentially happening out of order.
 *
 * NvDdk2dSurfaceLock returns a fence that the called must block on before
 * proceeding with accessing the surface. When the surface will be accessed
 * from hardware modules behind the graphics host, the blocking can be
 * implemented as doing a wait in the graphics host.
 *
 * When CPU access is required, a CPU wait on the fence must be done.
 * NvDdk2d provides a shortcut for doing this: by passing in NULL as the
 * fenceOut parameter the NvDdk2d implementation will do the CPU wait for
 * the user.
 *
 * If no blocking is necessary, the implementation will return a fence
 * with SyncPointID NVRM_INVALID_SYNCPOINT_ID.
 *
 * While the surface is locked, NvDdk2d operations involving the surface
 * are not allowed.
 *
 * Each call to NvDdk2dSurfaceLock must be matched by a call to
 * NvDdk2dSurfaceUnlock. A NvDdk2dSurfaceLock call followed by another
 * one without calling NvDdk2dSurfaceUnlock first is illegal.
 *
 * It is possible to only lock access to a subrectangle of the whole
 * surface, this is done by passing in a rectangle to
 * 'lockRect'. Passing in NULL will default to locking the whole
 * surface.  Locking only a subrectangle of the surface is better for
 * performance.
 */
void
NvDdk2dSurfaceLock(
        NvDdk2dSurface* Surf,
        NvDdk2dSurfaceAccessMode Access,
        const NvRect* LockRect,
        NvRmFence FencesOut[NVDDK_2D_MAX_FENCES],
        NvU32* NumFencesOut);

/**
 * \brief Get a pointer to the pixel data of a locked surface
 *
 * Calling this function for a surface that is not locked is an
 * error. The pixel pointer points to the first pixel in the first
 * scanline of the subrectangle 'lockRect' as passed into
 * NvDdk2dSurfaceLock, for access as specified in the 'access'
 * parameter. The locked rectangle is pitch linear with scanlines
 * separated by *strideOut bytes.
 *
 * The pointer to the pixel data becomes invalid when NvDdk2dSurfaceUnlock
 * is called. Using the pixel data pointer after that will cause undefined
 * behaviour.
 *
 * Getting pixel pointers again without unlockin in between will return
 * the same pointers as before.
 *
 */
NvError
NvDdk2dSurfaceGetPixels(
        NvDdk2dSurface* Surf,
        NvU32 BufferIndex,
        void** Pixels,
        NvU32* Stride);

/**
 * Unlock a surface and signal NvDdk2d to wait on a caller provided
 * fence before accessing the surface.
 *
 * Must be called on a surface handle that was previously locked using
 * NvDdk2dSurfaceLock.
 *
 * \param   Fences   Fences to wait for unlock.
 */
void
NvDdk2dSurfaceUnlock(
        NvDdk2dSurface* Surf,
        const NvRmFence Fences[NVDDK_2D_MAX_FENCES],
        NvU32 NumFences);

/**
 * Modifies color format of a surface buffer.
 *
 * Only such color formats which do not change pixel size can be specified,
 *
 * \param   bufferIndex Index of the buffer of which to change format
 * \param   ColorFormat New format
 *
 * Returns NvError_NotSupported if the buffer format cannot be changed
 *         because it would change pixel size.
 * Returns NvError_BadParameter if the bufferIndex is invalid.
 * Returns NvSuccess otherwise.
 */
NvError
NvDdk2dSurfaceResetPixelFormat(
        NvDdk2dSurface* hSurf,
        NvU32 bufferIndex,
        NvColorFormat ColorFormat);

/**
 * Defines repeat modes, which are used if a surface is used as a source or mask
 * operand to Pixman/XRender compatible blits.
 *
 * \param   Repeat
 */
void
NvDdk2dSurfaceSetRepeatMode(
        NvDdk2dSurface* Surf,
        NvDdk2dSurfaceRepeatMode Repeat);

/*---------------------------------------------------------*/
/** \defgroup rop ROPs (Raster OPerations) */

/**
 * \brief NvDdk2dRop defines the raster operations with two operands
 * \ingroup rop
 */
typedef enum
{
    /** resulting value = 0 */
    NvDdk2dRop_Clear = 0x00,

    /** resulting value = ~(src | dst) */
    NvDdk2dRop_NOR,

    /** resulting value = ~src & dst */
    NvDdk2dRop_AndInverted,

    /** resulting value = ~src */
    NvDdk2dRop_CopyInverted,

    /** resulting value = src & ~dst */
    NvDdk2dRop_AndReverse,

    /** resulting value = ~dst */
    NvDdk2dRop_Invert,

    /** resulting value = src ^ dst */
    NvDdk2dRop_XOR,

    /** resulting value = ~(src & dst) */
    NvDdk2dRop_Nand,

    /** resulting value = src & dst */
    NvDdk2dRop_And,

    /** resulting value = ~(src ^ dst) */
    NvDdk2dRop_Equiv,

    /** resulting value = dst */
    NvDdk2dRop_NoOp,

    /** resulting value = ~src | dst */
    NvDdk2dRop_ORInverted,

    /** resulting value = src */
    NvDdk2dRop_Copy,

    /** resulting value = src | ~dst */
    NvDdk2dRop_ORReverse,

    /** resulting value = src | dst */
    NvDdk2dRop_OR,

    /** resulting value = 1 */
    NvDdk2dRop_Set,

    NvDdk2dRop_Force32 = 0x7FFFFFFF
} NvDdk2dRop;

/** \brief Mask surface parameters */
typedef struct
{
    /** Mask surface must be an alpha-only format.
     *  Currently only NvColorFormat_A1 is supported.
     *  TODO A2, A4, A8
     */
    NvDdk2dSurface*    Surface;

    /** Top left offset of mask rect on mask surface
     *  Mask rect is same size as dest rect, and must be inside mask surface
     */
    NvPoint            Offset;
} NvDdk2dMaskParam;

/*---------------------------------------------------------*/
/** \defgroup brush Brushes */

/**
 * \brief Color/Monochrome Brush properties
 * \ingroup brush
 **/
typedef enum
{
    /** No Brush is used. This is the default. */
    NvDdk2dBrushType_None = 0x1,

    /** Brush is solid color, filling with Color1.
        No Brush surface associated with operation. */
    NvDdk2dBrushType_SolidColor,

    /**
     * Brush surface with color values. Must be in destination color
     * format.
     */
    NvDdk2dBrushType_Color,

    /**
     * Color1 when brush pixel has value 1
     * Color2 when brush pixel has value 0
     * Brush surface must be of format NvColorFormat_I1.
     */
    NvDdk2dBrushType_Mono,

    NvDdk2dBrushType_Force32 = 0x7FFFFFFF
} NvDdk2dBrushType;

/**
 * \brief Brush rendering parameters
 * \ingroup brush
 */
typedef struct
{
    /** Type of brush to use */
    NvDdk2dBrushType    Type;
    /** Brush surface */
    NvDdk2dSurface*     Surface;
    /** Source rectangle inside the brush surface used for tiling the pattern */
    NvRect              Rect;
    /** Source rectangle offset inside the source rectangle */
    NvPoint             Offset;
    /** Color #1 for 1-bit brush surfaces.  Used as fill color when
        the brush surface value is 1. */
    NvDdk2dColor        Color1;
    /** Color #2 for 1-bit brush surfaces.  Used as fill color when
        the brush surface value is 0. */
    NvDdk2dColor        Color2;
} NvDdk2dBrushParam;

/*---------------------------------------------------------*/
/** \defgroup blend Blending
 *
 * Blend equations define how source and destination pixels in a blit
 * are combined.
 */

/**
 * \brief Choose the blend function to use in blend operations.
 * \ingroup blend
 */
typedef enum
{
    /* Blending disabled. This is the default */
    NvDdk2dBlendFunc_Copy = 0x1,

    /**
     * A standard Src over Dst blend, with Src affected by an additional
     * constant alpha factor:
     * Out = CstAlpha * Src + (1 - CstAlpha * SrcAlpha) * Dst
     *
     * Where:
     * - CstAlpha = ConstantAlpha / 255
     * - SrcAlpha is Src.a if PerPixelAlpha is enabled, 1 otherwise.
     *
     * Note that this should only be used with premultiplied alpha
     * source and destination.
     */
    NvDdk2dBlendFunc_SrcOver,

    /**
     * Same as above for non-premultiplied source:
     * Out = CstAlpha * MultSrc + (1 - CstAlpha * SrcAlpha) * Dst
     *
     * Where:
     * - CstAlpha = ConstantAlpha / 255
     * - SrcAlpha is Src.a if PerPixelAlpha is enabled, 1 otherwise.
     * - MultSrc.rgb is Src.rgb * SrcAlpha, MultSrc.a is Src.a.
     *
     * Note that this should only be used with premultiplied alpha
     * destination.
     */
    NvDdk2dBlendFunc_SrcOverNonPre,

    /**
     * Blending operations defined in Pixman library and used by XRender
     * extension, for instance.
     */
    NvDdk2dBlendFunc_Clear,
    NvDdk2dBlendFunc_Src,
    NvDdk2dBlendFunc_Dst,
    NvDdk2dBlendFunc_Over,
    NvDdk2dBlendFunc_OverReverse,
    NvDdk2dBlendFunc_In,
    NvDdk2dBlendFunc_InReverse,
    NvDdk2dBlendFunc_Out,
    NvDdk2dBlendFunc_OutReverse,
    NvDdk2dBlendFunc_Atop,
    NvDdk2dBlendFunc_AtopReverse,
    NvDdk2dBlendFunc_Xor,
    NvDdk2dBlendFunc_Add,
    NvDdk2dBlendFunc_Saturate,

    NvDdk2dBlendFunc_DisjointClear,
    NvDdk2dBlendFunc_DisjointSrc,
    NvDdk2dBlendFunc_DisjointDst,
    NvDdk2dBlendFunc_DisjointOver,
    NvDdk2dBlendFunc_DisjointOverReverse,
    NvDdk2dBlendFunc_DisjointIn,
    NvDdk2dBlendFunc_DisjointInReverse,
    NvDdk2dBlendFunc_DisjointOut,
    NvDdk2dBlendFunc_DisjointOutReverse,
    NvDdk2dBlendFunc_DisjointATop,
    NvDdk2dBlendFunc_DisjointATopReverse,
    NvDdk2dBlendFunc_DisjointXor,

    NvDdk2dBlendFunc_ConjointClear,
    NvDdk2dBlendFunc_ConjointSrc,
    NvDdk2dBlendFunc_ConjointDst,
    NvDdk2dBlendFunc_ConjointOver,
    NvDdk2dBlendFunc_ConjointOverReverse,
    NvDdk2dBlendFunc_ConjointIn,
    NvDdk2dBlendFunc_ConjointInReverse,
    NvDdk2dBlendFunc_ConjointOut,
    NvDdk2dBlendFunc_ConjointOutReverse,
    NvDdk2dBlendFunc_ConjointATop,
    NvDdk2dBlendFunc_ConjointATopReverse,
    NvDdk2dBlendFunc_ConjointXor,

    NvDdk2dBlendFunc_Force32 = 0x7FFFFFFF,
} NvDdk2dBlendFunc;

/**
 * \brief Usage of the source pixels' alpha value in the blend operation.
 * \ingroup blend
 */
typedef enum
{
    NvDdk2dPerPixelAlpha_Disabled = 0x1,
    NvDdk2dPerPixelAlpha_Enabled,

    NvDdk2dPerPixelAlpha_Force32 = 0x7FFFFFFF,
} NvDdk2dPerPixelAlpha;

/**
 * \brief Description of the blend operation.
 * \ingroup blend
 */
typedef struct
{
    NvDdk2dBlendFunc        Func;
    NvDdk2dPerPixelAlpha    PerPixelAlpha;
    NvU8                    ConstantAlpha;
} NvDdk2dBlendParam;

/*---------------------------------------------------------*/
/** \defgroup srcoverride Source surface component override
 *
 * The source override provides a way to discard particular components of the
 * source surface pixels and replace them with a constant value. This override
 * operation is applied as a first stage of reading input pixels, before
 * applying any other blit operations.
 *
 * Currently the API only supports overriding the alpha component.
 */

/**
 * \brief Determines which pixel components of the source surface to override.
 * \ingroup srcoverride
 */
typedef enum
{
    NvDdk2dSrcOverride_Disabled = 0,
    NvDdk2dSrcOverride_Alpha    = 1 << 0,

    NvDdk2dSrcOverride_Force32 = 0x7FFFFFFF,
} NvDdk2dSrcOverride;

/**
 * \brief Description of the source surface component override parameter.
 * \ingroup srcoverride
 */
typedef struct
{
    NvDdk2dSrcOverride Override;
    NvU8               Alpha;
} NvDdk2dSrcOverrideParam;

/*---------------------------------------------------------*/
/** \defgroup colorkey Color keying */

/**
 * \brief Color comparison operation when performing a transparent blt.
 * \ingroup colorkey
 */
typedef enum
{
    /**
     * No color comparison operation is performed. This is the default.
     */
    NvDdk2dColorCompare_None = 0x1,

    /**
     * Color comparison is successful when the pixel color matches that
     * of the key color.
     */
    NvDdk2dColorCompare_Equal,

    /**
     * Color comparison is successful when the pixel color does not match
     * that of the key color.
     */
    NvDdk2dColorCompare_NotEqual,

    NvDdk2dColorCompare_Force32 = 0x7FFFFFFF,
} NvDdk2dColorCompare;

/**
 * \brief Color key parameters
 *
 * NvDdk2dColorKeyParam contains the parameters used for color
 * comparison operations.
 *
 * The color comparison operation specified is first applied against
 * each pixel from the source.  Any source pixels that pass the color
 * comparison operation are then use to perform the specified ROP.
 * The output from the ROP is applied to the destination only for
 * destination pixels that pass the color comparison operation.
 */
typedef struct
{
    /** color comparison operation performed against the source value */
    NvDdk2dColorCompare ColorCmpSrc;

    /** color comparison operation performed against the destination surface */
    NvDdk2dColorCompare ColorCmpDst;

    /**
     * key is the pointer to the color key used for both the source and
     * destination color comparison operations
     */
    NvDdk2dColor Key;

} NvDdk2dColorKeyParam;

/*---------------------------------------------------------*/
/** \defgroup filter Surface filtering */

/**
 * \brief Filtering mode used for stretched blits.
 */
typedef enum
{
    /** Disable the horizontal and vertical filtering */
    NvDdk2dStretchFilter_Nearest = 0x1,

    /**
     *  Enable the best quality filtering
     *  Both Horizontal and vertical filtering is turned on
     */
    NvDdk2dStretchFilter_Nicest,

    /**
     *  Enable intelligent filtering
     *  Horizontal or Vertical filtering is turned on only if needed
     */
    NvDdk2dStretchFilter_Smart,

    /** Enable 2 tap filtering in both horizontal and vertical direction */
    NvDdk2dStretchFilter_Bilinear,

    NvDdk2dStretchFilter_Force32 = 0x7FFFFFFF
} NvDdk2dStretchFilter;

/*---------------------------------------------------------*/
/** \defgroup transform Source transformations
 *
 * Transformations are used to rotate and mirror the source surface of
 * a blit operation.  The destination rectangle is not affected by any
 * transformation settings.
 *
 * NvDdk2dTransform identifies the transformations that can be applied
 * as a combination of rotation and mirroring
 *
 * Specifically, given a hypothetical 2x2 image, applying these operations
 * would yield the following results
 *
 * <PRE>
 *       INPUT                      OUTPUT
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |     IDENTITY     | 0 | 1 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 2 | 3 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |    ROTATE_90     | 1 | 3 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 0 | 2 |
 *     +---+---+                  +---+---+
 *
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |    ROTATE_180    | 3 | 2 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 1 | 0 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |     ROTATE_270   | 2 | 0 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 3 | 1 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |  FLIP_HORIZONTAL | 1 | 0 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 3 | 2 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |  INVTRANSPOSE    | 3 | 1 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 2 | 0 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |  FLIP_VERTICAL   | 2 | 3 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 0 | 1 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+
 *     | 0 | 1 |     TRANSPOSE    | 0 | 2 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 1 | 3 |
 *     +---+---+                  +---+---+
 * <PRE>
 */

/**
 * \brief See group \ref transform for an explanation.
 * \ingroup transform
 * The enumeration starts at 0 and is in the correct order, so don't change it!
 * The operation NvDdk2dMultiplyTransform() relies on the order for efficiency.
 * Also known as Dihedral Group D4 :-)
 **/
typedef enum
{
    /** No transformation */
    NvDdk2dTransform_None = 0x0,

    /** Rotation by 90 degrees */
    NvDdk2dTransform_Rotate90,

    /** Rotation by 180 degrees */
    NvDdk2dTransform_Rotate180,

    /** Rotation by 270 degrees */
    NvDdk2dTransform_Rotate270,

    /** Mirroring in the horizontal */
    NvDdk2dTransform_FlipHorizontal,

    /**
     * Mirroring along a diagonal axis from the top right to the bottom
     * left of the rectangular region
     */
    NvDdk2dTransform_InvTranspose,

    /** Mirroring in the vertical direction */
    NvDdk2dTransform_FlipVertical,

    /**
     * Mirroring along a diagonal axis from the top left to the bottom
     * right of the rectangular region
     */
    NvDdk2dTransform_Transpose,

    NvDdk2dTransform_Force32 = 0x7FFFFFFF
} NvDdk2dTransform;

/*---------------------------------------------------------*/
/** \defgroup cliprects Clip rectangles
 *
 * Clip rectangles are a blit optimization that can be used to split a
 * single src/dst surface blit into multiple sub-parts.  This happens
 * often when rendering windows on the screen.  Some parts of a blit
 * destination may be occluded by other windows, and thus it makes
 * sense to draw only the parts of the destination that is not
 * visible.  Clip rect lists are used to represent this clipping
 * operation.
 *
 * Clip rectangles are given in destination coordinate system.
 *
 * The rectangles must fulfil the following criteria:
 * - all dimensions must be positive
 * - rectangles may not intersect
 *   - unless NvDdk2dBlitFlags_AllowClipRectOverlap is set
 * - rectangles must be bound to the destination rectangle
 * - rectangles must be bound to the dimensions of the target surface
 *
 * Clip rectangles can be given either as a straight list or as a
 * rectangle iterator. If Rects is set, Iterator will not be used.
 *
 * If clip rectangles are specified, dstRect does not need to be
 * completely within the target surface.
 *
 * \ingroup cliprects
 */


/** \brief List of clip rects.
 *
 * \see Module  \ref cliprects
 * \ingroup cliprects
 **/
typedef struct
{
    NvRect* Rects;
    int     Count;
    void*   Iterator;
    int     (*NextN) (void* Iterator, NvRect* RectList, int MaxRects);
    void    (*Reset) (void* Iterator);
} NvDdk2dRectList;


/*---------------------------------------------------------*/
/** \defgroup antialias Anti-aliasing
 *
 * Anti-aliasing can be used to reduce aliasing artifacts near the boundaries
 * of graphical primitives. Anti-aliasing is often performed after drawing in
 * a separate step called resolving. The resolve is triggered if a matching
 * coverage surface is supplied. Anti-aliasing can be combined with other
 * operations such as format conversions and stretching etc.
 *
 * The supported methods for Full-screen anti-aliasing are:
 *  VCAA (Virtual-Coverage Anti-Aliasing)
 *
 * VCAA requires a separate coverage surface which is a standard single-buffer
 * NvDdk2dSurface. The only supported color format for a VCAA coverage
 * surface is NvColorFormat_X4C4 and the VCAA surface must have same dimensions
 * as the src color surface.
 *
 * \ingroup antialias
 */

/**
 * \brief Anti-aliasing options for the blit operation.
 * \ingroup antialias
 */
typedef struct
{
    NvDdk2dSurface* SrcCoverage;
} NvDdk2dAntiAliasParam;





/*---------------------------------------------------------*/
/** \defgroup palette Palettes
 *
 * When blitting to and from surfaces in paletted (ie. indexed) formats, a
 * palette is necessary to interpret the color indices that form the surface
 * data.
 *
 * Palettes are one-dimensional NvDdk2dSurfaces. They are as wide as the palette
 * contains colors, and map indices (x position in the surface) to color values.
 *
 * The color format of a palette surface can be any non-paletted format.
 *
 * Paletted formats are currently supported for source surfaces only. In the
 * future support could be extended to destination and brush.
 *
 * \ingroup palette
 */

/**
 * \brief Palettes for the blit operation.
 * \ingroup palette
 */
typedef struct
{
    NvDdk2dSurface* Src;
} NvDdk2dPaletteParam;


/*---------------------------------------------------------*/
/** \defgroup composite Composition parameters
 *
 * A special blit with multiple source layers composited together.
 * Needs a valid CompositeParam, no source surface and source rect.
 * The first layer in the list is at the bottom, and is the only one
 * allowed to be opaque or in YUV format.
 *
 * \ingroup composite
 */

typedef struct
{
    NvDdk2dSurface*                 Surface;
    const NvRect                    *DstRect;
    const NvDdk2dFixedRect          *SrcRect;
    NvDdk2dBlendParam               Blend;
    NvDdk2dTransform                Transform;
    NvDdk2dStretchFilter            Filter;
} NvDdk2dCompositeLayer;

/** \brief Parameters for a composite blit
 *
 * \see Module  \ref composite
 * \ingroup composite
 **/
typedef struct
{
    const NvDdk2dCompositeLayer*    Layers;
    NvU32                           Count;
} NvDdk2dCompositeParam;


/*---------------------------------------------------------*/
/** \defgroup colortransform Color Transformation parameters
 *
 * A blit for color space transformation with Nvcms. The
 * profile holds parameters for source-to-device gamut
 * mapping. The transformation is done either with per-pixel
 * matrix multiplication or with LUTs.
 *
 * \ingroup colortransform
 */
typedef struct {
    struct NvCmsDeviceLinkProfileRec *profile;
} NvDdk2dColorTransform;

/*---------------------------------------------------------*/
/** \defgroup Stereo formats */

/**
 * Stereo Formats supported by the blitter for the purposes of
 * interleaving.
 *
 * This will column interleave the src provided in the blit based
 * on the format provided below. The format shall dictate how to
 * read the source content and which half of the surface will be
 * shown to which eye.
 *
 * NOTE: Currently only Left-Right Stereo format is supported in the
 *       3D backend.
 *
 * \ingroup stereo
 **/
typedef enum
{
    /** Mono mode */
    NvDdk2dStereoFormat_Mono = 0,

    /** Left-Right Stereo format */
    NvDdk2dStereoFormat_LeftRight = 0x1,

    /** Right-Left Stereo format */
    NvDdk2dStereoFormat_RightLeft,

    /** Top-Bottom Stereo format */
    NvDdk2dStereoFormat_TopBottom,

    /** Botton-Top Stereo format */
    NvDdk2dStereoFormat_BottomTop,

    /** Seperate Frame Stereo format */
    NvDdk2dStereoFormat_SeparateFrame,

    NvDdk2dStereoFormat_Last = 0x7FFFFFFF
} NvDdk2dStereoFormat;

typedef enum
{
    /**
     * Operation flags
     * Affect blit behaviour
     */

    /*
     * If and only if doing a format conversion, set alpha values to 0.
     * This may cost a bit of perf so only enable this if the operation
     * absolutely requires it (e.g. non-alpha->alpha convert, pre WM7)
     */
    NvDdk2dBlitFlags_ConvertZeroAlpha       = 1 << 0,

    /*
     * Source rect is allowed to extend outside of source surface.
     * But if the rect is outside, some backends might reject it.
     * Costs a little bit of perf to check the rect;  It could be always on
     * except it's also a validation flag.  If it's not set, invalid rects
     * will be caught by validation so only turn it on when you need it.
     */
    NvDdk2dBlitFlags_AllowExtendedSrcRect   = 1 << 1,

    /*
     * By default the pixels in sub-byte color formats are ordered from LSB
     * to MSB. When this flag is set they are ordered from MSB to LSB.
     */
    NvDdk2dBlitFlags_ByteStartsAtMsb        = 1 << 2,

    /*
     * If the blit caused temporary memory to be allocated, hold on to it
     * until the next blit in anticipation of another similar blit. The temporary
     * memory is freed if the next blit didn't require it or upon an explicit
     * call to NvDdk2dFreeTempMemory().
     */
    NvDdk2dBlitFlags_KeepTempMemory         = 1 << 3,

    NvDdk2dBlitFlags_GlyphUnpacking         = 1 << 4,

    NvDdk2dBlitFlags_DumpSurfaces           = 1 << 5,

    /*
     * If, due to limitations of 2d engine, blit internally requires
     * destination rectangle larger then rectangle specified for the blit
     * operation, allow to skip a special blit pass if its only purpose would
     * be to crop the intermediate rectangle into a target size. The crop offset
     * is returned back to the caller via ParamsOut. This requires to use
     * NvDdk2dBlitExt() blit funtion.
     * Destination surface must be large enough to hold expanded destination
     * rectangle. If this condition is not met, flag NvDdk2dBlitFlags_ReturnDstCrop
     * is ignored, that is the special cropping blit pass is executed.
     * Care must be taken when enabling this flag, as part of the pixel data for
     * the destination surface will be stored outside of the bounds of the
     * surface width and height. It's therefore only possible to access the
     * correct pixel data by applying the crop offset that is returned.
     * This also means that the original memory allocation for the surface must
     * account for the possiblity of using this flag, as extra padding is
     * needed at the end of the surface. NvRmSurfaceComputeSize implicitely
     * considers this possibility, so this is the preferred method for computing
     * surface size requirements that conform to these restrictions.
     */
    NvDdk2dBlitFlags_ReturnDstCrop          = 1 << 6,

    /*
     * When blitting from one surface to another with the same color
     * space but higher precision (e.g. RGB 565 to 888), the hardware
     * may not be able to perfectly replicate the higher order bits
     * of each channel into the lower order bits. Without this
     * replication, the original RGB values will essentially be left
     * shifted, leaving zeros in the LSBs. This means, for instance,
     * that full intensity 565 (0xFFFF) doesn't map to full intensity
     * 888 (0xFFFFFF) as expected. Instead it maps to 0xF8FCF8.
     *
     * However, the hardware may be able to approximate the correct
     * low order bits by making use of the CSC logic to apply a
     * small gain. The downside to this operation is that, due to
     * rounding, the resulting values may not match the originals
     * if truncated back to their original precision.
     *
     * Setting this flag allows the blit unit to apply this
     * approximation, when supported by the hardware. It is otherwise
     * ignored. The formats supported and the equation used for the
     * converation are chip-dependent.
     */
    NvDdk2dBlitFlags_AllowApproxLSB         = 1 << 7,

    /**
     * Compute and return CRC value.
     *
     * Different backends may generate different CRC value even if blit
     * results are identical.
     *
     * The computed CRC is only guaranteed to take pixels within the clip
     * rectangles into account. It may not provide any information about
     * positioning of these clip rectangles on the destination surface.
     * */
    NvDdk2dBlitFlags_ReturnCrc              = 1 << 8,

    /**
     * Validation flags
     * Only affect validation (if it is turned on)
     */

    /** Clip rectangles are allowed to overlap */
    NvDdk2dBlitFlags_AllowClipRectOverlap   = 1 << 31,

    /** Blit may not turn into a stretch blit */
    NvDdk2dBlitFlags_DisallowStretchBlt     = 1 << 30,
} NvDdk2dBlitFlags;

/*---------------------------------------------------------*/
/** \defgroup blit Blits */

/**
 * \brief Bit-mask of valid fields in NvDdk2dBlitParameters
 * \ingroup blit
 */
typedef enum
{
    NvDdk2dBlitParamField_Rop4              = 1 << 0,
    NvDdk2dBlitParamField_Filter            = 1 << 1,
    NvDdk2dBlitParamField_Mask              = 1 << 2,
    NvDdk2dBlitParamField_Brush             = 1 << 3,
    NvDdk2dBlitParamField_Blend             = 1 << 4,
    NvDdk2dBlitParamField_ColorKey          = 1 << 5,
    NvDdk2dBlitParamField_Transform         = 1 << 6,
    NvDdk2dBlitParamField_ClipRects         = 1 << 7,
    NvDdk2dBlitParamField_VblankSyncPointId = 1 << 8,
    NvDdk2dBlitParamField_Flags             = 1 << 9,
    NvDdk2dBlitParamField_DstRotate         = 1 << 10,
    NvDdk2dBlitParamField_Palettes          = 1 << 11,
    NvDdk2dBlitParamField_SolidSrcColor     = 1 << 12,
    NvDdk2dBlitParamField_AntiAlias         = 1 << 13,
    NvDdk2dBlitParamField_SolidMaskColor    = 1 << 14,
    NvDdk2dBlitParamField_SeveralRects      = 1 << 15,
    NvDdk2dBlitParamField_MaskCompAlpha     = 1 << 16,
    NvDdk2dBlitParamField_OverrideFormat    = 1 << 17,
    NvDdk2dBlitParamField_Composite         = 1 << 18,
    NvDdk2dBlitParamField_StereoFormat      = 1 << 19,
    NvDdk2dBlitParamField_ColorTransform    = 1 << 20,
    NvDdk2dBlitParamField_SrcOverride       = 1 << 21,

    /* Make this always the last one */
    NvDdk2dBlitParamField_Invalid           = 1 << 22,
} NvDdk2dBlitParamField;

#define NVDDK2D_BLITPARAM_ALL_FIELDS (NvDdk2dBlitParamField_Invalid - 1)

/**
 * Struct for setting the additional parameters for a blit.
 * ValidFields is a mask which indicates which fields of the struct
 * should be read.
 *
 * \ingroup blit
 */
typedef struct
{
    NvU32                           ValidFields;
    NvU32                           Rop4;
    NvDdk2dMaskParam                Mask;
    NvDdk2dBrushParam               Brush;
    NvDdk2dBlendParam               Blend;
    NvDdk2dColorKeyParam            ColorKey;
    NvDdk2dStretchFilter            Filter;
    NvDdk2dTransform                Transform;
    NvDdk2dRectList                 ClipRects;
    NvU32                           VblankSyncPointId;
    NvU32                           Flags;
    NvDdk2dTransform                DstRotate;
    NvDdk2dPaletteParam             Palettes;
    NvDdk2dColor                    SolidSrcColor;
    NvDdk2dAntiAliasParam           AntiAlias;
    NvDdk2dColor                    SolidMaskColor;
    NvBool                          MaskCompAlpha;
    NvDdk2dRectList                 SrcRects;
    NvDdk2dRectList                 DstRects;
    NvColorFormat                   SrcFormat;
    NvColorFormat                   MaskFormat;
    NvColorFormat                   DstFormat;
    NvDdk2dCompositeParam           Composite;
    NvDdk2dStereoFormat             StereoFormat;
    NvDdk2dColorTransform           ColorTransform;
    NvDdk2dSrcOverrideParam         SrcOverride;

} NvDdk2dBlitParameters;

/**
 * Struct for returning additional values from a blit.
 *
 * \ingroup blit
 */
typedef struct
{
    /**
     * Coordinates of upper left corner of output cropping rectangle, see
     * NvDdk2dBlitFlags_ReturnDstCrop. Coordinates of bottom right corner
     * of the cropping rectangle can be computed from DstOffset and size
     * of DstRect.
     */
    NvPoint                     DstOffset;
    NvU32                       Crc;
} NvDdk2dBlitParametersOut;

NvError
NvDdk2dOpen(
        NvRmDeviceHandle Device,
        NvRmChannelHandle Channel,
        NvDdk2dHandle *Ddk2d);

void
NvDdk2dClose(
        NvDdk2dHandle Ddk2d);

/**
 * Perform a 2d blit operation with supplementary return values.
 *
 * This function behaves exactly as NvDdk2dBlit(), see below, with addition
 * that it allows to return supplementary values to caller.

 * If 'paramsOut' is not null, the blit operation stores supplementary return
 * values for the blit to the structure pointed to by paramsOut, if applicable.
 * If 'paramsOut' is null, the function becomes equivalent to NvDdk2dBlit().
 *
 *
 * Supplementary values are returned when using blit flag
 * NvDdk2dBlitFlags_ReturnDstCrop or NvDdk2dBlitFlags_ReturnCrc.
 *
 * NvDdk2dBlitFlags_ReturnDstCrop is used when padded destination rectangle for
 * internal operation is required and the final crop is skipped.
 *
 * NvDdk2dBlitFlags_ReturnCrc returns a crc value of blitted pixels.
 *
 *      NvDdk2dBlitExt(h2d, dst, NULL, src, NULL, NULL, NULL);
 *
 * @param h2d
 * @param hDstSurface
 * @param dstRect
 * @param hSrcSurface
 * @param srcRect
 * @param params
 * @param paramsOut
 *
 * \ingroup blit
 */
NvError
NvDdk2dBlitExt(
        NvDdk2dHandle                   Ddk2d,
        NvDdk2dSurface*                 DstSurface,
        const NvRect                    *DstRect,
        NvDdk2dSurface*                 SrcSurface,
        const NvDdk2dFixedRect          *SrcRect,
        const NvDdk2dBlitParameters     *Params,
        NvDdk2dBlitParametersOut        *ParamsOut);

/**
 * Perform a 2d blit operation.
 *
 * A blit transfers pixels from a source surface to a destination surface
 * applying a variety of transformations to the pixel values on the way.
 *
 * The interface aims at making the typical uses of normal pixel copy easy
 * by not mandating the setting of advanced blit parameters unless they are
 * actually required.
 *
 * Passing in NULL as 'params' invokes a standard pixel copy blit without
 * additional transformations. If the dimensions of the source rectangle do
 * not match the dimensions of the destination rectangle, pixels are scaled
 * to fit the destination rectangle. The filtering mode for the scale defaults
 * to NvDdk2dStretchFilter_Nearest, additional filtering modes are available
 * by setting the corresponding parameter in NvDdk2dBlitParameters.
 *
 * The source rectangle coordinates are fixed point values to give the caller
 * more control over the source sampling.
 *
 * Passing in NULL as srcRect defaults to a source rectangle the size of the
 * full source surface, likewise for dstRect and the destination surface.
 *
 * A straight pixel copy between surfaces of the same dimensions (but not
 * necessary the same bit depth or even color format) is issued by:
 *
 *      NvDdk2dBlit(h2d, dst, NULL, src, NULL, NULL);
 *
 * @param h2d
 * @param hDstSurface
 * @param dstRect
 * @param hSrcSurface
 * @param srcRect
 * @param params
 *
 * \ingroup blit
 */
static NV_INLINE NvError
NvDdk2dBlit(
        NvDdk2dHandle                   Ddk2d,
        NvDdk2dSurface*                 DstSurface,
        const NvRect                    *DstRect,
        NvDdk2dSurface*                 SrcSurface,
        const NvDdk2dFixedRect          *SrcRect,
        const NvDdk2dBlitParameters     *Params)
{
    return NvDdk2dBlitExt(Ddk2d,
                         DstSurface,
                         DstRect,
                         SrcSurface,
                         SrcRect,
                         Params,
                         NULL);
}

/**
 * Free any temporary memory used by Ddk2d as a result
 * of using the KeepTempMemory blit flag.
 */
void
NvDdk2dFreeTempMemory(
        NvDdk2dHandle                   Ddk2d);

/** \brief Print nvddk2d statistics into debug out */
void
NvDdk2dStatsPrint(
        NvDdk2dHandle Ddk2d);

/** \brief Reset any statistics gathered by the nvddk2d implementation */
void NvDdk2dStatsReset(
        NvDdk2dHandle Ddk2d);


/** Debugging functions. */
void
NvDdk2dEnumerateBackends(
        NvDdk2dHandle h2d,
        const char** backends,
        NvU32* numBackends);

/** Debugging functions. */
void
NvDdk2dSetActiveBackends(
        NvDdk2dHandle h2d,
        NvU32 activeMask);

/** Debugging functions. */
NvU32
NvDdk2dGetActiveBackends(
        NvDdk2dHandle h2d);

typedef enum
{
    /* Release: do not validate, debug: validate and assert */
    NvDdk2dValidateMode_Default = 0,

    /* Always validate */
    NvDdk2dValidateMode_Enable = 1,

    /* Do not validate */
    NvDdk2dValidateMode_Disable = 2,

} NvDdk2dValidateMode;

/** Debugging functions. */
void
NvDdk2dEnableValidate(
        NvDdk2dHandle h2d,
        NvDdk2dValidateMode Mode);

/** Debugging functions. */
void
NvDdk2dEnableDump(
        NvDdk2dHandle h2d,
        NvBool Enable);

/** Debugging functions. */
void
NvDdk2dSetDumpPath(
        NvDdk2dHandle Ddk2d,
        const char* path);

/** Debugging functions. */
NvError
NvDdk2dSurfaceDump(
        NvDdk2dSurface* s,
        const char* filename);

/*---------------------------------------------------------
 * Convenience utilities for setting up draw parameters
 */

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitRop4(NvDdk2dBlitParameters *Params, NvU32 Rop4)
{
    Params->Rop4 = Rop4;
    Params->ValidFields |= NvDdk2dBlitParamField_Rop4;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitRop3(NvDdk2dBlitParameters *Params, NvU32 Rop3)
{
    NvDdk2dSetBlitRop4(Params, (((Rop3 << 8) & 0xFF00) | (Rop3 & 0xFF)));
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitRop2(NvDdk2dBlitParameters *Params, NvU32 Rop2)
{
    NvDdk2dSetBlitRop3(Params, (((Rop2 << 4) & 0xF0) | (Rop2 & 0xF)));
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitFilter(NvDdk2dBlitParameters *Params, NvDdk2dStretchFilter Filter)
{
    Params->Filter = Filter;
    Params->ValidFields |= NvDdk2dBlitParamField_Filter;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitTransform(NvDdk2dBlitParameters *Params, NvDdk2dTransform Transform)
{
    Params->Transform = Transform;
    Params->ValidFields |= NvDdk2dBlitParamField_Transform;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitColorTransform(NvDdk2dBlitParameters *Params, struct NvCmsDeviceLinkProfileRec *Profile)
{
    Params->ColorTransform.profile = Profile;
    Params->ValidFields |= NvDdk2dBlitParamField_ColorTransform;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitVblankSyncPointId(NvDdk2dBlitParameters *Params, NvU32 Id)
{
    Params->VblankSyncPointId = Id;
    Params->ValidFields |= NvDdk2dBlitParamField_VblankSyncPointId;
}

/**
 * \ingroup blit
 * \brief Sets logical transform of destination surface buffer
 *
 * This attribute defines apparent origin and orientation of coordinate
 * system on destination surface. This is necessary in order to be able
 * to perform stretch blit operation consistently irrespective of the
 * logical transform of destination surface with respect to memory scan
 * line order, if the logical transform makes sense for the user.
 *
 * This attribute is relevant only for stretch operation, although it is
 * safe to specify it always. Default is NvDdk2dTransform_None.
*/
static NV_INLINE void
NvDdk2dSetBlitDstTransform(NvDdk2dBlitParameters *Params, NvDdk2dTransform Rotation)
{
    Params->DstRotate = Rotation;
    Params->ValidFields |= NvDdk2dBlitParamField_DstRotate;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitMask(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dSurface*         Surf,
        NvPoint*                Offset)
{
    Params->Mask.Surface = Surf;
    if (Offset)
        Params->Mask.Offset = *Offset;
    else
    {
        Params->Mask.Offset.x = 0;
        Params->Mask.Offset.y = 0;
    }
    Params->ValidFields |= NvDdk2dBlitParamField_Mask;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitSolidMaskColor(
                            NvDdk2dBlitParameters   *Params,
                            NvDdk2dColor*           SolidMaskColor)
{
    Params->SolidMaskColor = *SolidMaskColor;
    Params->ValidFields |= NvDdk2dBlitParamField_SolidMaskColor;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitMaskCompAlpha(
                            NvDdk2dBlitParameters   *Params,
                            NvBool                  UseComponentAlpha)
{
    Params->MaskCompAlpha = UseComponentAlpha;
    Params->ValidFields |= NvDdk2dBlitParamField_MaskCompAlpha;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitBrush(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dBrushType        Type,
        NvDdk2dSurface*         Surface,
        NvRect                  *Rect,
        NvPoint                 *Offset,
        NvDdk2dColor            *Color1,
        NvDdk2dColor            *Color2)
{
    Params->Brush.Surface = Surface;
    if (Rect)
    {
        Params->Brush.Rect = *Rect;
    }
    else if (Surface)
    {
        NvRmSurface s;
        NvDdk2dSurfaceGetBuffer(Surface, 0, &s);
        Params->Brush.Rect.top = Params->Brush.Rect.left = 0;
        Params->Brush.Rect.right = s.Width;
        Params->Brush.Rect.bottom = s.Height;
    }
    if (Offset)
    {
        Params->Brush.Offset = *Offset;
    }
    else
    {
        Params->Brush.Offset.x = 0;
        Params->Brush.Offset.y = 0;
    }
    if (Color1)
        Params->Brush.Color1 = *Color1;
    if (Color2)
        Params->Brush.Color2 = *Color2;
    Params->Brush.Type = Type;
    Params->ValidFields |= NvDdk2dBlitParamField_Brush;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitBlend(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dBlendFunc        Func,
        NvDdk2dPerPixelAlpha    PerPixelAlpha,
        NvU8                    ConstantAlpha)
{
    Params->Blend.Func = Func;
    Params->Blend.PerPixelAlpha = PerPixelAlpha;
    Params->Blend.ConstantAlpha = ConstantAlpha;
    Params->ValidFields |= NvDdk2dBlitParamField_Blend;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitSrcOverride(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dSrcOverride       Override,
        NvU8                     Constant)
{
    Params->SrcOverride.Override = Override;
    Params->SrcOverride.Alpha = Constant;
    Params->ValidFields |= NvDdk2dBlitParamField_SrcOverride;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitColorKey(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dColorCompare     SrcColorCmp,
        NvDdk2dColorCompare     DstColorCmp,
        NvDdk2dColor            *Key)
{
    Params->ColorKey.ColorCmpSrc = SrcColorCmp;
    Params->ColorKey.ColorCmpDst = DstColorCmp;
    if (Key)
        Params->ColorKey.Key = *Key;
    Params->ValidFields |= NvDdk2dBlitParamField_ColorKey;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitClipRects(
        NvDdk2dBlitParameters   *Params,
        NvRect                  *Rects,
        NvU32                   Count)
{
    Params->ClipRects.Rects = Rects;
    Params->ClipRects.Count = Count;
    Params->ClipRects.Iterator = NULL;
    Params->ClipRects.NextN = NULL;
    Params->ClipRects.Reset = NULL;
    Params->ValidFields |= NvDdk2dBlitParamField_ClipRects;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitClipRectIterator(
        NvDdk2dBlitParameters   *Params,
        void                    *Iterator,
        void                    (*Reset) (void* Iterator),
        int                     (*NextN) (void* Iterator, NvRect* RectList, int MaxRects))
{
    Params->ClipRects.Iterator = Iterator;
    Params->ClipRects.Reset = Reset;
    Params->ClipRects.NextN = NextN;
    Params->ClipRects.Rects = NULL;
    Params->ClipRects.Count = 0;
    Params->ValidFields |= NvDdk2dBlitParamField_ClipRects;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitSrcPalette(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dSurface*         SrcPalette)
{
    Params->Palettes.Src = SrcPalette;
    Params->ValidFields |= NvDdk2dBlitParamField_Palettes;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitSolidSrcColor(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dColor*           SolidSrcColor)
{
    Params->SolidSrcColor = *SolidSrcColor;
    Params->ValidFields |= NvDdk2dBlitParamField_SolidSrcColor;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitAntiAliasing(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dSurface*         SrcCoverage)
{
    Params->AntiAlias.SrcCoverage = SrcCoverage;
    Params->ValidFields |= NvDdk2dBlitParamField_AntiAlias;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetBlitComposite(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dCompositeLayer   *Layers,
        NvU32                   Count)
{
    Params->Composite.Layers = Layers;
    Params->Composite.Count = Count;
    Params->ValidFields |= NvDdk2dBlitParamField_Composite;
}

/** \ingroup blit */
static NV_INLINE void
NvDdk2dSetStereoFormat(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dStereoFormat     StereoFormat)
{
    Params->StereoFormat = StereoFormat;
    Params->ValidFields |= NvDdk2dBlitParamField_StereoFormat;
}

/** \ingroup blit */
// Set + Toggle => Clear
static NV_INLINE void
NvDdk2dModifyBlitFlags(
        NvDdk2dBlitParameters   *Params,
        NvU32                   Set,
        NvU32                   Toggle)
{
    NvU32 ValidFields = Params->ValidFields;
    NvU32 Flags;
    if (ValidFields & NvDdk2dBlitParamField_Flags)
        Flags = Params->Flags | Set;
    else
    {
        Flags = Set;
        Params->ValidFields = ValidFields | NvDdk2dBlitParamField_Flags;
    }
    Params->Flags = Flags ^ Toggle;
}

/**
 * \brief Sets Rop4 and Brush to implement a solid color fill
 * \ingroup blit
 */
static NV_INLINE void
NvDdk2dSetBlitFill(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dColor            *FillColor)
{
    NvDdk2dSetBlitBrush(Params,
                        NvDdk2dBrushType_SolidColor,
                        NULL, NULL, NULL,
                        FillColor,
                        NULL);
    NvDdk2dSetBlitRop3(Params, NvDdk2dRop_Set << 4);
}

/** \ingroup blit */
static NV_INLINE NvBool
NvDdk2dRop4NeedsDest(NvU32 Rop4)
{
    return ((Rop4 ^ (Rop4 >> 1)) & 0x5555) != 0;
}

/** \ingroup blit */
static NV_INLINE NvBool
NvDdk2dRop4NeedsSource(NvU32 Rop4)
{
    return ((Rop4 ^ (Rop4 >> 2)) & 0x3333) != 0;
}

/** \ingroup blit */
static NV_INLINE NvBool
NvDdk2dRop4NeedsBrush(NvU32 Rop4)
{
    return ((Rop4 ^ (Rop4 >> 4)) & 0x0F0F) != 0;
}

/** \ingroup blit */
static NV_INLINE NvBool
NvDdk2dRop4NeedsMask(NvU32 Rop4)
{
    return ((Rop4 ^ (Rop4 >> 8)) & 0x00FF) != 0;
}

/**
 * \brief Multiplies transform t1 with t2, in that order
 * \ingroup blit
 */
static NV_INLINE NvDdk2dTransform
NvDdk2dMultiplyTransform(
        NvDdk2dTransform t1,
        NvDdk2dTransform t2)
{
    NvDdk2dTransform f = (NvDdk2dTransform)(t1 & 4);
    return (NvDdk2dTransform)(((f ? (t1 - t2) : (t1 + t2)) & 3) | ((f ^ t2) & 4));
}

/**
 * \brief Gives the inverse t' of a transform t, such that:
 *   t*t' = t'*t = e
 * where e is the identity element.
 * \ingroup blit
 */
static NV_INLINE NvDdk2dTransform
NvDdk2dInverseTransform(
        NvDdk2dTransform t)
{
    return (NvDdk2dTransform)((t & 4) ? t : ((-t) & 3));
}

static NV_INLINE void
NvDdk2dSetCompositeLayer(
        NvDdk2dCompositeLayer   *Layer,
        NvDdk2dSurface          *Surface,
        const NvRect            *DstRect,
        const NvDdk2dFixedRect  *SrcRect,
        NvDdk2dBlendFunc        BlendFunc,
        NvDdk2dPerPixelAlpha    PerPixelAlpha,
        NvU8                    ConstantAlpha,
        NvDdk2dTransform        Transform,
        NvDdk2dStretchFilter    Filter)
{
    Layer->Surface = Surface;
    Layer->DstRect = DstRect;
    Layer->SrcRect = SrcRect;
    Layer->Blend.Func = BlendFunc;
    Layer->Blend.PerPixelAlpha = PerPixelAlpha;
    Layer->Blend.ConstantAlpha = ConstantAlpha;
    Layer->Transform = Transform;
    Layer->Filter = Filter;
}

static NV_INLINE void
NvDdk2dSetStereoComposite(
        NvDdk2dBlitParameters   *Params,
        NvDdk2dCompositeLayer   *Layers,
        NvDdk2dSurface          *LeftEyeSurface,
        NvDdk2dSurface          *RightEyeSurface,
        const NvDdk2dFixedRect  *SrcRect,
        const NvRect            *DstRect,
        NvDdk2dTransform        Transform)
{
    NvDdk2dSetCompositeLayer(&Layers[0],
                             LeftEyeSurface,
                             DstRect,
                             SrcRect,
                             NvDdk2dBlendFunc_Copy,
                             NvDdk2dPerPixelAlpha_Disabled,
                             255,
                             Transform,
                             NvDdk2dStretchFilter_Nearest);
    NvDdk2dSetCompositeLayer(&Layers[1],
                             RightEyeSurface,
                             DstRect,
                             SrcRect,
                             NvDdk2dBlendFunc_Copy,
                             NvDdk2dPerPixelAlpha_Disabled,
                             255,
                             Transform,
                             NvDdk2dStretchFilter_Nearest);

    NvDdk2dSetBlitComposite(Params, Layers, 2);
    NvDdk2dSetStereoFormat(Params, NvDdk2dStereoFormat_SeparateFrame);
}

#if defined(__cplusplus)
}
#endif
#endif /* INCLUDED_NVDDK_2D_H */

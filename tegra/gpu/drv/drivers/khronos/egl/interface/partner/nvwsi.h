/*
 * Copyright (c) 2006 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVWSI_H
#define NVWSI_H

/**
 * @file
 * <b>NVIDIA Windowing System Integration Interface</b>
 *
 * @b Description: Defines the interface for controlling the
 *     windowing system integration.
 */

/**
 * @defgroup nv_wsi_grp Windowing System Integration Interface
 *
 * For every supported platform, there is exactly one implementation of
 * the NVIDIA windowing system integration interface (formerly known as
 * the EGL windowing system integration interface, or EGL native backend
 * layer).
 *
 * If the platform has multiple notions of a window manager
 * (real or virtual) the native integration is expected to be able to deduce
 * which windowing system to target from the runtime environment or from the
 * native display handles passed to it.
 *
 * @par Includes
 *
 * The native integration interface may not depend on EGL types or concepts.
 * The communication between EGL and the native integration is handled via
 * NvOs and NvRm. Due to this generic nature, the usefulness of the native
 * integration is not limited to EGL, it may very well be used by other
 * modules that require access to rendering targets in a windowing system.
 *
 * @par NvWsiWindow
 *
 * As far as NvWsi clients are concerned, a native window is a rendering target
 * manager that hands our color and coverage buffers upon request and knows how
 * to display them in the context of the window manager once the client is done
 * with them.
 *
 * The client calls ::NvWsiWindowGetSurfaceType for getting the current buffers
 * and ::NvWsiWindowPostType for displaying them, always in pairs. This divides
 * the lifetime of the native window objects into two alternating periods:
 * - The time when the client has the buffers and is possibly rendering to them
 *   (between \c GetSurface and \c Post)
 * - The time when it doesn't.
 *
 * In the latter period, every aspect of the window buffers is allowed to change;
 * it may resize its buffers, reallocate them, get rid of them all together, etc.
 * Between \c GetSurface and \c Post the buffers are owned by the client and the
 * native integration is not allowed to modify them. This means that as the
 * actual displayed window changes in size during this period, for example,
 * the native integration still must make the best effort to display the buffers
 * owned by the client in the following call to \c Post.
 *
 * This model allows for a variety of buffer management strategies in the
 * native integration, still keeping the client operation simple and straight-
 * forward. In practice a typical integration will inspect the native window
 * state in the \c GetSurface call, decide what changes are needed to the buffers
 * and hand back updated buffers to the client. A simple example is a
 * multi-buffer swapping strategy (a.k.a. page flipping).
 *
 * Some clients (such as EGL) default to images in buffers being stored with
 * the first row of image pixels as the last row in memory, i.e. bottom-to-top.
 * Most windowing systems expect the image to be stored the other way around,
 * so the rendering APIs accomodate for that. Set the
 * ::NvWsiObjectFlag_TopToBottom flag if this is the case.
 *
 * @{
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvrm_surface.h"
#include "nvrm_channel.h"

#define NVWSI_MAX_FENCES 4

#ifdef __cplusplus
extern "C" {
#endif

//###########################################################################
//################################## TYPES ##################################
//###########################################################################

/**
 * Opaque handles to platform objects. These are passed as is to the NVWSI
 * backend that needs to know how to interpret these objects.
 */
typedef void* NvWsiNativeDisplay;
typedef void* NvWsiNativeWindow;
typedef void* NvWsiNativePixmap;
typedef NvU32 NvWsiNativeVisualType;
typedef NvUPtr NvWsiNativeVisualID;

/**
 * Flags describing the native object. These should be set upon object
 * creation and never modified dynamically.
 */
typedef enum
{
    NvWsiObjectFlag_TopToBottom     = 1 << 0,
    NvWsiObjectFlag_PostMayFlip     = 1 << 1,
    NvWsiObjectFlag_PostRectBlit    = 1 << 2,
    NvWsiObjectFlag_Force32         = 0x7FFFFFFF
} NvWsiObjectFlag;

/**
 * Return value from ::NvWsiWindowPostType, indicating the chosen strategy
 * for posting the results onto the target.
 */
typedef enum
{
    NvWsiPostResult_Failure        = 1,
    NvWsiPostResult_Blit,
    NvWsiPostResult_Flip,
    NvWsiPostResult_Force32        = 0x7FFFFFFF
} NvWsiPostResult;

/**
 * Filled in by NvWsiGetDisplayCaps, indicating features and limits
 * specific to the backend and/or display.
 */
typedef struct
{
    NvS32   minSwapInterval;    // Min/max supported swap intervals
    NvS32   maxSwapInterval;
    NvBool  stereo;             // Supports stereo/multiview windows
    NvBool  eglImageFromPixmap; // EGL image creation pixmap
    NvBool  eglImageFromANB;    // EGL image creation from Android native buffer
    NvBool  androidSyncExtensions; // Supports Android fence file syncs
    NvU32   maxWinBackBuffers;  // Maximum number of user requested back buffers
    NvU32   maxOutstandingSwaps; // Maximum number of outstanding swaps
    NvBool  absColorSpaces;     // Supports absolute color spaces
    NvBool  bufferWriteCount;   // Buffer write/modification tracking
    NvBool  vpr;                // Supports VPR heap (for EGL secure contexts)
    NvBool  needsFenceOnSwap;   // Whether explicit synchronization is needed after preSwap()
    NvBool  coreGLBufferAge;    // Supports core-GL implementation of buffer age tracking
} NvWsiDisplayCaps;

/**
 * Absolute color spaces available.
 */
typedef enum
{
    NvWsiAbsColorSpace_None           = 0,
    NvWsiAbsColorSpace_DeviceRGB,
    NvWsiAbsColorSpace_sRGB,
    NvWsiAbsColorSpace_AdobeRGB,
    NvWsiAbsColorSpace_WideGamutRGB,
    NvWsiAbsColorSpace_Rec709,
    NvWsiAbsColorSpace_CIEXYZ,
    NvWsiAbsColorSpace_CIELAB,

    NvWsiAbsColorSpace_Force32        = 0x7FFFFFFF
} NvWsiAbsColorSpace;

/**
 * A native context instance represents any per-device state that the
 * native integration may need to store. The caller is responsible for
 * tracking all opened objects and making sure they get destroyed before
 * the native context is torn down.
 */
typedef struct NvWsiContextRec NvWsiContext;

/**
 * For each native window or pixmap encountered, NvWsi creates a
 * corresponding native integration object, ::NvWsiWindow for
 * \c NvWsiNativeWindow and ::NvWsiPixmap for \c NvWsiNativePixmap.
 * The native integration is required to allocate the corresponding
 * object and set the members that NvWsi will use to interact with the
 * object (see struct definitions). The implementation may store
 * any private state associated with the objects by "subclassing" these
 * types, i.e. defining its own object type-specific struct with the
 * public information starting at offset 0.
 */
typedef struct NvWsiObjectRec  NvWsiObject;
typedef struct NvWsiWindowRec  NvWsiWindow;
typedef struct NvWsiPixmapRec  NvWsiPixmap;

/**
 * This represent a function that the caller can pass into the native
 * object creation functions to have control over what the memory layout
 * of the buffer will be. The native integration will reallocate window
 * and pixmap buffers to match the specified memory layout, if possible.
 */
typedef void
(*NvWsiMemAlignmentType)(
    const NvColorFormat format,
    const NvRmSurfaceLayout layout,
    const NvU32 width,
    const NvU32 height,
    NvU32* align,
    NvU32* pitch,
    NvU32* size);

/**
 * Callback types
 */
typedef void
(*NvWsiNativeCloseDisplayType)(NvWsiNativeDisplay *);

typedef void
(*NvWsiSuspendLocksType)(NvWsiContext *);

typedef void
(*NvWsiResumeLocksType)(NvWsiContext *);

typedef void
(*NvWsiColorManager)(NvWsiContext *);

typedef void
(*NvWsiApiIssueFenceType)(NvRmFence *);

typedef void
(*NvWsiApiWaitFenceType)(NvRmFence *);

//===========================================================================
// NvWsiObject
//===========================================================================

/**
 * Destroys a ::NvWsiObject. Releases all resources held by the object
 * and deallocates the object itself. Does Wb not imply closing the actual
 * native window or destroying the native pixmap.
 */
typedef void (*NvWsiDestroyType) (NvWsiObject* obj);

/**
 * Locks the object for read or write access.
 */
typedef void (*NvWsiLockType) (NvWsiObject* obj);

/**
 * Unlocks the object.
 */
typedef void (*NvWsiUnlockType) (NvWsiObject* obj);

/**
 * Gets fences from the native windowing system representing the last
 * native drawing operation(s) on this object. Used for \c eglWaitNative().
 */
typedef void (*NvWsiIssueFenceType) (NvWsiObject* obj,
                                     NvRmFence fence[NVWSI_MAX_FENCES],
                                     NvU32* numFences);
/**
 * Instructs the native system to wait on this fence before issuing
 * further native drawing operations on the object used for \c eglWaitClient().
 */
typedef void (*NvWsiWaitFenceType)  (NvWsiObject* obj,
                                     const NvRmFence* fence);
/**
 * Update object for use as a texture and return the appropriate memFd
 * to use, which may or not match the EGLimage. The returned memFd is owned
 * by nvwsi backend.
 */
typedef void (*NvWsiUpdateTextureType) (NvWsiObject* obj, int *memFd);

/**
 * Inform the native backend about the compression status of an object.
 * This allows the native backend to be smart about when it needs to decompress
 * the object.
 */
typedef void (*NvWsiSetCompressedType)  (NvWsiObject* obj,
                                         NvBool compressed);

/**
 * Ask from the native backend whether it wants a particular buffer to be
 * decompressed automatically after rendering.
 */
typedef NvBool (*NvWsiShouldDecompressType)  (NvWsiObject* obj);

/**
 * Get GPU mapping data that is stored in the native object. If the mapping is
 * stored in the native object, it can be shared between GLSI drawables/images.
 */
typedef void* (*NvWsiGetGpuMappingType)  (NvWsiObject* obj);

/**
 * Set GPU mapping data that is stored in the native object. If the mapping is
 * stored in the native object, it can be shared between GLSI drawables/images.
 * The native backend will call destroyCallback when the surface is destroyed
 * from the current process and the GPU mapping needs to be freed.
 */
typedef void (*NvWsiDestroyGpuMappingType)(void *);
typedef int (*NvWsiSetGpuMappingType)  (NvWsiObject* obj, void *mapping,
                                        NvWsiDestroyGpuMappingType destroyCallback);

/**
 * Indicate that the native object will be used as a render target.
 */
typedef void (*NvWsiSetRenderableType)(NvWsiObject* obj);

/**
 * The common header for native objects (windows and pixmaps).
 */
struct NvWsiObjectRec
{
    NvWsiDestroyType       Destroy;
    NvWsiLockType          Lock;
    NvWsiUnlockType        Unlock;
    NvWsiIssueFenceType    IssueFence;
    NvWsiWaitFenceType     WaitFence;
    NvWsiUpdateTextureType UpdateTexture;
    NvWsiSetCompressedType SetCompressed;
    NvWsiShouldDecompressType ShouldDecompress;
    NvWsiGetGpuMappingType GetGpuMapping;
    NvWsiSetGpuMappingType SetGpuMapping;
    NvWsiSetRenderableType SetRenderable;
    NvU32                  Flags;
};

//===========================================================================
// NvWsiWindow
//===========================================================================

/**
 * Gets and locks surface buffers for rendering. The color buffer format must
 * match the format requested in the constructor of this window; the
 * coverage buffer format must be ::NvColorFormat_X4C4. Window dimensions and the
 * memory handle of the buffers is allowed to change between every call to
 * this function.
 *
 * @note It is guaranteed that every call to \c GetSurface will be matched by a call
 *       to ::NvWsiWindowPostType, except in the event that the window is destroyed.
 *
 * This function cannot fail. If no surface is available (or couldn't be
 * allocated), the implementation should return a NULL buffer to disable
 * rendering.
 *
 * @param colorBuffer    A ::NvRmSurface structure to be filled with a
 *                       description of the color buffer. Non-existence of
 *                       a color buffer (for example due to allocation
 *                       failure or window not being visible) is signalled
 *                       by the \a hMem member being NULL. In any other case
 *                       the surface desciptor must represent a valid
 *                       existing surface.
 * @param coverageBuffer A ::NvRmSurface structure to be filled with a
 *                       description of the coverage buffer. If no coverage
 *                       buffer was requested or the buffer couldn't be
 *                       allocated the \a hMem member must be set to NULL.
 * @param preserveContent Force contents of the render buffer of the surface
 *                       to be preserved from a posted surface to the next surface
 *                       acquired via \c GetSurface. This may mean that the
 *                       implementation will have to do an additional blit.
 * @param fences         An array of fences to synchronize on before accessing
 *                       the surface.
 * @param preRotate      Whether or not the surface needs to be pre-rotated 90
 *                       degrees. If NULL, no pre-rotation will be applied.
 */

typedef NvError
(*NvWsiWindowGetSurfaceType)(
    NvWsiWindow* window,
    NvRmSurface* colorBuffer,
    NvRmSurface* colorBuffer2,
    NvRmSurface* coverageBuffer,
    NvBool       preserveContent,
    NvRmFence    fences[NVWSI_MAX_FENCES],
    NvU32*       numFences,
    NvBool*      preRotate);

/**
 * Posts the buffers obtained in the previous ::NvWsiWindowGetSurfaceType call
 * to the windowing system. Depending on the window environment, this could be,
 * for example, blitting the buffer contents onto a display primary, programming
 * the buffer as the primary surface to the display controller, or handing the buffer
 * over to a compositor to be composited.
 *
 * This function cannot fail. If posting the surface did not succeed for any
 * reason, it simply will not become visible; there is nothing the client can do
 * about it.
 *
 * @param fence          A syncpoint indicating when rendering to the buffers
 *                       is done. Any operation of displaying/posting the
 *                       buffers must be synchronized on this syncpoint.
 *                       For example, if the native backend posts the buffers
 *                       by reading the pixel data on the CPU, the integration
 *                       will have to do a CPU wait on the syncpoint. In most
 *                       cases, however, the post operation should consist
 *                       of a HW operation that can be synchronized in the
 *                       graphics host, leaving the CPU free to proceed.
 *                       A SyncPointID of \c NVRM_INVALID_SYNCPOINT_ID is used
 *                       to mean that no synchronization is necessary.
 * @param swapInterval   The number of refresh periods the posted surface
 *                       must be made visible (for example: as specified in the
 *                       EGL specification of \c eglSwapInterval()). The
 *                       integration may either accomplish this by blocking
 *                       this call as long as necessary or by returning an
 *                       appropriate fence in the next \c GetSurface() call for
 *                       this window.
 * @param absColorSpace  Absolute color space for the window.
 * @param preserve       If true, buffer contents will be preserved after the post.
 * @param skipResolve    If true, skip the coverage sample resolve blit even if
 *                       the drawable is allocated with a coverage sample buffer.
 */
typedef NvWsiPostResult
(*NvWsiWindowPostType)(
    NvWsiWindow* window,
    const NvRmFence* fence,
    NvU32 swapInterval,
    NvWsiAbsColorSpace absColorSpace,
    NvBool preserve,
    NvBool skipResolve);

typedef NvWsiPostResult
(*NvWsiWindowPostRectType)(
    NvWsiWindow* window,
    const NvRmFence* fence,
    NvU32 swapInterval,
    NvWsiAbsColorSpace absColorSpace,
    NvBool skipResolve,
    int x, int y,
    int width, int height);

/*
 * @param cropFactor     Scale factor used to set the crop rectangle on the window
 */
typedef void
(*NvWsiWindowSetCropType)(
    NvWsiWindow* window,
    NvRect *cropRect);

/**
 * Return number of times the window has been write locked.
 */
typedef NvU32
(*NvWsiWindowGetWriteCountType)(
    NvWsiWindow* window);

/**
 * Maps the buffers obtained in the previous ::NvWsiWindowGetSurfaceType call
 * for SW access and returns a pointer to the pixels.
 */
typedef void*
(*NvWsiWindowMapType)(
    NvWsiWindow* window);

typedef void
(*NvWsiWindowUnmapType)(
    NvWsiWindow* window);

/**
 * The public ::NvWsiWindow struct. The only information visible to
 * the user are the function pointers used to interact with the object.
 * These pointers may not be cached by the user, so it is legal for the
 * native integration to change the function pointers dynamically in the
 * calls themselves in response to a changed environment necessitating
 * a change in the buffer management strategy.
 */
struct NvWsiWindowRec
{
    NvWsiObject                  Obj;
    NvWsiWindowGetSurfaceType    GetSurface;
    NvWsiWindowPostType          Post;
    NvWsiWindowPostRectType      PostRect;
    NvWsiWindowSetCropType       SetCrop;
    NvWsiWindowGetWriteCountType GetWriteCount;
    NvWsiWindowMapType           Map;
    NvWsiWindowUnmapType         Unmap;
};

//===========================================================================
// NvWsiPixmap
//===========================================================================

/**
 * Maps the pixmap buffer for SW access and returns a pointer to the pixels.
 */
typedef void*
(*NvWsiPixmapMapType)(
    NvWsiPixmap* pixmap);

typedef void
(*NvWsiPixmapUnmapType)(
    NvWsiPixmap* pixmap);

/**
 * The public NvWsiPixmap struct.
 */
struct NvWsiPixmapRec
{
    NvWsiObject               Obj;
    NvRmSurface               ColorBuffer[NVRM_MAX_SURFACES];
    NvU32                     BufferCount;
    NvWsiPixmapMapType        Map;
    NvWsiPixmapUnmapType      Unmap;
};

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/**
 * Creates the native context for the RM device passed. A native context
 * may be used to store any global information such as connections to HW
 * modules or the window manager.
 */
NvError     NvWsiContextCreate  (NvWsiContext** ctx,
                                 NvRmDeviceHandle rmDev);

/**
 * Associate a native display with a context
 */
NvError NvWsiNativeDisplayRef   (NvWsiContext* Ctx,
                                 NvWsiNativeDisplay display);

/**
 * Retrieve native display associated with a context
 */
NvWsiNativeDisplay
NvWsiGetNativeDisplay           (NvWsiContext* Ctx);

/**
 * Retrieve capabilities for display associated with a context
 */
void
NvWsiGetDisplayCaps             (NvWsiContext* Ctx,
                                 NvWsiDisplayCaps* caps);

/**
 * Disassociate a native display from a context
 */
void NvWsiNativeDisplayUnref    (NvWsiContext* Ctx,
                                 NvWsiNativeDisplay display);

/**
 * Sets callbacks for a native context.
 * @param cndCallback   This function will be called when the native
 *                      display is closed by the application.
 *
 */
void        NvWsiSetCallbacks   (NvWsiContext* ctx,
                                 NvWsiNativeCloseDisplayType ncdCallback,
                                 NvWsiSuspendLocksType suspendLocksCallback,
                                 NvWsiResumeLocksType resumeLocksCallback,
                                 NvWsiApiIssueFenceType issueFenceCallback,
                                 NvWsiApiWaitFenceType waitFenceCallback);

/**
 * Destroys the native context, deallocates all memory, and closes all connections.
 * NvWsi guarantees that all objects created from this context are destroyed
 * before destroying the native context, so there is no need to track the
 * objects in the native integration.
 */
void        NvWsiContextDestroy (NvWsiContext* ctx);

/**
 * Allocates and initializes a new native window integration object. The client
 * imposes the color format and whether to have a coverage buffer or not onto
 * the native backend. If the actual native integration cannot service the
 * request, it can use the nvwsi_blit (see nvwsi_blit.h) wrapper to deal with
 * the format conversion and coverage resolve operation.
 *
 * Typically the native window passed in as 'native' is of the real native
 * window type as specified in eglplatform.h. We do currently overload the
 * window namespace to deal with supporting fullscreen window access and
 * compositor windows, these are case-by-case exceptions and are best
 * avoided.
 *
 * @param ctx       The native context this window is created in.
 * @param native    The native window object, i.e., a Win32 HWND.
 * @param format    The requested format for the window backbuffer. If it is
 *                  possible for the native integration to change the window
 *                  buffer format to accomodate this request, it should do so;
 *                  otherwise, the integration may be forced to do an additional
 *                  format conversion blit. It is not acceptable for the
 *                  native integration to return surfaces of different format
 *                  than what was originally requested. As it is, EGL and the
 *                  client APIs will actually be able to deal with this, but
 *                  it will break the EGL config selection rules.
 * @param absColorSpace Initial absolute color space for the window. Ignored
 *                  if enableCms is set to NV_FALSE.
 * @param enableCms NV_TRUE if color management is requested for the window.
 * @param coverage  NV_TRUE if the client expects the native integration to
 *                  return a coverage buffer in addition to the color buffer
 *                  and do the coverage resolve operation as a part of the
 *                  window post.
 * @param stereoInfo Stereo parameters for display. Check NV_STEREO_XX
 *                  in nvrm_surface.h
 * @param numBackBuffers Number of wanted back buffers, should be 1 for when using
 *                  EGL_BACK_BUFFER as EGL_RENDER_BUFFER. If EGL_TRIPLE_BUFFER_NV or
 *                  EGL_QUAD_BUFFER_NV is used, it should be set to 2/3 respectively.
 * @param window    The memory address to store the newly created window
 *                  integration object pointer to.
 */
NvError     NvWsiWindowCreate   (NvWsiContext*       ctx,
                                 NvWsiNativeWindow   native,
                                 NvColorFormat       format,
                                 NvWsiAbsColorSpace  absColorSpace,
                                 NvBool              enableCms,
                                 NvBool              coverage,
                                 NvU32               stereoInfo,
                                 NvU32               numBackBuffers,
                                 NvWsiWindow**       window);

/**
 *
 */
NvError     NvWsiPixmapCreate   (NvWsiContext*         ctx,
                                 NvWsiNativePixmap     native,
                                 NvWsiMemAlignmentType align,
                                 NvWsiPixmap**         pixmap);

/**
 * Ask the native windowing system to relinquish any device resources it may
 * currently be holding for use by the client.
 *
 * @param Ctx       The native context this window is created in.
 */
NvError NvWsiFreeResources      (NvWsiContext* Ctx);

/**
 * Returns the native visual type and ID for a color format.
 *
 * @param ctx       The native context to search for a visual
 * @param format    The color format for which to find a visual
 * @param type      Memory in which to store the type for the visual found
 * @param fast      Memory in which to store the ID for an optimized visual
 *                  It may not match the format bit counts exactly.
 * @param slow      Memory in which to optionally store the ID for a
 *                  non-optimized visual. Ignored if NULL.
 *                  If the fast visual does not match the format's bit counts
 *                  exactly, the backend may return an alternative visual ID
 *                  which does match, but for which performance is slower.
 *                  Otherwise, the value should match that returned in *fast.
 */
NvError     NvWsiGetNativeVisual(NvWsiContext* ctx,
                                 NvColorFormat format,
                                 NvWsiNativeVisualType* type,
                                 NvWsiNativeVisualID* fast,
                                 NvWsiNativeVisualID* slow);

#ifdef __cplusplus
}
#endif

/** @} */
#endif // NVWSI_H

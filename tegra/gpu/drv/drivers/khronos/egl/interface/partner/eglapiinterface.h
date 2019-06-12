/*
 * Copyright (c) 2006 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef EGLAPIINTERFACE_H
#define EGLAPIINTERFACE_H
/*
* eglapiinterface.h
*/

//###########################################################################
//############################### NOTES #####################################
//###########################################################################
//
// This file contains the internal interface between EGL and client APIs
//
// There are two parts in this interface: NvEglApiImports and NvEglApiExports
//
// - NvEglApiImports is a function table of all the functions that a client API
// needs to implement for EGL (EGL imports these functions from the client API)
//
// - NvEglApiExports is a function table of all the functions that EGL provides
// to a client API for it to implement some functionality (EGL exports these
// functions to the client APIs)
//
// - Client APIs must provide a function that initializes a NvEglApiImports
// function table. EGL will call this function with a table that is
// pre-initialized with default stub functions. The Client APIs need only
// provide valid implementations for those functionalities they support.
// Some functions always require a valid implementation. Example prototype
// for this initialization function:
//      extern int NvApiNameInit(NvEglApiImports *imports
//                                    , const NvEglApiExports *exports);
// The function will make a local copy of the "exports" table from EGL
//  and will fill in the "imports" table.
//
//  It should return the EGL_OPEN*_BIT that it supports.
//
// TODO [tkarras 09/Aug/2007] Using native types (e.g. int) or their direct
// derivatives (e.g. EGLint, EGLBoolean) in interface structs is pretty
// dangerous. The size of these types depends on compiler options, and
// compiling different modules with different compiler options can cause
// unexpected results. Use the Nv-types instead.

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvrm_channel.h"   // for NvRmFence & NVRM_INVALID_SYNCPOINT_ID
#include "nvrm_surface.h"   // for NvRmSurface
#include "nvwsi.h"
#include "pthread.h"

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// Forward declarations
//===========================================================================

typedef struct NvEglDisplayRec      NvEglDisplay;
typedef struct NvEglContextRec      NvEglContext;
typedef struct NvEglSurfaceRec      NvEglSurface;
typedef struct NvEglConfigRec       NvEglConfig;
typedef struct NvEglImageSyncObjRec NvEglImageSyncObj;

// Client API object handles owned by the Client API  and opaque to EGL.
typedef struct NvEglApiContextRec   NvEglApiContext;
typedef struct NvEglApiSurfaceRec   NvEglApiSurface;
typedef struct NvEglApiDisplayRec   NvEglApiDisplay;

typedef void* NvEglImageHandle;
typedef void* NvEglApiClientBuffer;

typedef void* NvEglStreamHandle;
typedef void* NvEglApiStreamInfo;

//===========================================================================
// NvEglBufferType
//===========================================================================
typedef enum
{
    NvEglApiBufferType_Color = 0,
    NvEglApiBufferType_Depth,
    NvEglApiBufferType_Stencil,
    NvEglApiBufferType_FsaaColor,
    NvEglApiBufferType_Coverage,
    NvEglApiBufferType_FsaaResolve,
    NvEglApiBufferType_ColorRight,
    NvEglApiBufferType_DepthRight,
    NvEglApiBufferType_StencilRight,
    NvEglApiBufferType_FsaaColorRight,
    NvEglApiBufferType_MSColorS01,  // Multisample color buffer for
                                    // sample 0 and 1
                                    // MSAA2X requires only
                                    // MSColorS01
    NvEglApiBufferType_MSColorS23,  // Multisample color buffer for
                                    // sample 2 and 3
                                    // MSAA4X requires MSColorS01 and
                                    // MSColorS23
    NvEglApiBufferType_MSColorTag,  // Multisample color tag buffer

    NvEglApiBufferType_MSDepthS01,  // Multisample depth buffer for
                                    // sample 0 and 1
                                    // MSAA2X requires only
                                    // MSDepthS01
    NvEglApiBufferType_MSDepthS23,  // Multisample depth buffer for
                                    // sample 2 and 3
                                    // MSAA4X requires MSDepthS01 and
                                    // MSDepthS23

    NvEglApiBufferType_CDepth,      // Compressed depth buffer for Multisample
                                    // or single sample depth buffer
    NvEglApiBufferType_DepthTag,    // Depth tag buffer for compressed depth or
                                    // multisample depth buffer

    NvEglApiBufferType_MSStencil,   // Multisample stencilbuffer, for both
                                    // MSAA2X and MSAA4X
    NvEglApiBufferType_MSColorS01Right,
    NvEglApiBufferType_MSColorS23Right,
    NvEglApiBufferType_MSColorTagRight,

    NvEglApiBufferType_MSDepthS01Right,
    NvEglApiBufferType_MSDepthS23Right,
    NvEglApiBufferType_CDepthRight,
    NvEglApiBufferType_DepthTagRight,
    NvEglApiBufferType_MSStencilRight,
    NvEglApiBufferType_Count,
    NvEglApiBufferType_Force32 = 0x7FFFFFFF
} NvEglApiBufferType;

//===========================================================================
// NvEglSurfaceDirty
//===========================================================================

typedef enum
{
    // the surface pointers are dirty (this happens alone in flipping)
    NV_EGLSURF_DIRTY_PTR       = 1 << 0,

    // the surface dimensions are dirty
    // note that surface being destroyed completely is signalled by
    // dimensions changing to 0, 0.
    NV_EGLSURF_DIRTY_DIM       = 1 << 1,

    // the prerotation state has changed
    NV_EGLSURF_DIRTY_PREROTATE = 1 << 2,

    // everything dirty
    NV_EGLSURF_DIRTY_ALL       = 0x7FFFFFFF
} NvEglApiSurfaceDirty;

//===========================================================================
// NvEglDownScaleMethodEnum - Downscale variant
//===========================================================================
typedef enum NvEglDownScaleMethodEnum {
    NV_EGL_DOWNSCALE_NONE        =   0x00000000,     // No downscale
    NV_EGL_DOWNSCALE_ONSCREEN    =   0x00000001,     // Downscale back buffer
    NV_EGL_DOWNSCALE_OFFSCREEN   =   0x00000002      // Downscale FBOs
} NvEglDownScaleMethod;

//===========================================================================
// NvEglApiSurfaceDesc
//===========================================================================
typedef struct NvEglApiSurfaceDescRec
{
    NvRmSurface                 buf[NvEglApiBufferType_Count];
    struct NvGlsiDrawableRec*   glsiDrawable;
    NvU32                       width;
    NvU32                       height;
    NvU32                       depthOverlapBufOffset;
    NvU32                       stencilOverlapBufOffset;
    NvU32                       fsaaSamples;
    NvF32                       fsaaScaleX;
    NvF32                       fsaaScaleY;
    NvBool                      colorNonLinear;
    NvBool                      depthNonLinear;
    NvBool                      preMultAlpha;
    NvBool                      topToBottom;
    NvBool                      postRectBlitSupported;
    NvBool                      hasOverlapBuf;
    NvU32                       stereoControl;
    NvU32                       downScaleBits;   // See NvEglDownScaleMethod
    float                       downScaleFactor;
    NvBool                      preRotate;
    NvU32                       samples;
    // Set to true if we detect that the backing native resource has become
    // invalid.
    NvBool                      badNative;
    // Dummy used for surfaceless contexts
    NvBool                      isSurfacelessDummy;
} NvEglApiSurfaceDesc;

//===========================================================================
// NvEglApiGenericProc - generic function pointer
//===========================================================================
typedef void (*NvEglApiGenericProc)(void);

//===========================================================================
// NvEglApiStatus
//===========================================================================
typedef enum
{
    NvEglApiStatus_Unsupported = 0,
    NvEglApiStatus_Uninitialized,
    NvEglApiStatus_Initialized,

    NvEglApiStatus_Force32 = 0x7fffffff
} NvEglApiStatus;

//===========================================================================
// NvEglApiIdxEnum
// Used to internally identify APIs and index into API arrays.
//===========================================================================
typedef enum NvEglApiIdxEnum
{
    // Real, EGL defined APIS.
    NV_EGL_API_GL_ES2 = 0,
    NV_EGL_API_GL_ES3,
    NV_EGL_API_GL_ES,
    NV_EGL_API_GL,
    NV_EGL_API_VG,
    NV_EGL_API_EXTERNAL_COUNT,

    // Internal APIs, used only for internal purposes such as EGLImages
    // or EGLStreams.
    NV_EGL_API_NVKD = NV_EGL_API_EXTERNAL_COUNT,
    NV_EGL_API_OMX_IL,
    NV_EGL_API_OMX_AL,
    NV_EGL_API_NVENC,
    NV_EGL_API_NVMEDIA,
    #if !defined(ANDROID)
    NV_EGL_API_TEGRA_V4L2,
    #endif
    NV_EGL_API_CUDA,

    NV_EGL_API_COUNT,
} NvEglApiIdx;

/* Mapping from api idx to api bit in supported apis bitmask */
#define NV_EGL_API_BIT(idx) (1 << idx)

//===========================================================================
// NvPrstntSettingType
// Indicates the type of value in the data store.
//===========================================================================
typedef enum {
    NvPrstntSettingType_Unknown = 0,
    NvPrstntSettingType_Uint32,
    NvPrstntSettingType_Uint64,
    NvPrstntSettingType_String,
    NvPrstntSettingType_Binary,
    // TBD: Float32, Double64
    NvPrstntSettingType_Last = NvPrstntSettingType_Binary,

    NvPrstntSettingType_Force32 = 0x7fffffffUL
} NvPrstntSettingType;

//===========================================================================
// NvEglApiExtnProcAddrTableRec - contains extension function pointers
//===========================================================================
typedef struct NvEglApiExtnProcAddrTableRec
{
    const char *name;
    NvEglApiGenericProc address;
} NvEglApiExtnProcAddrTable;

//===========================================================================
// NvEglApiClientBufferInfo - ClientBuffer (VGImage) info needed by EGL
//                            for eglCreatePbufferFromClientBuffer
//===========================================================================
typedef struct NvEglApiClientBufferInfoRec
{
    NvU32           width;
    NvU32           height;
    NvBool          preMultAlpha;
    NvBool          colorNonLinear;
    NvU8            redSize;
    NvU8            greenSize;
    NvU8            blueSize;
    NvU8            alphaSize;
    NvU8            luminanceSize;
} NvEglApiClientBufferInfo;

//===========================================================================
// TODO :: Document
// EGL_Image information
// TODO: [ahatala 2008-10-13] the various "image" descriptors for
//                            egl surface, image and clientpbuffer should
//                            be combined into a single type
//===========================================================================
typedef struct NvEglApiImageInfoRec
{
    // Note that buf.hMem should always be NULL and the actual memory handle
    // passed as a memory FD in memFd!
    NvRmSurface             buf[NVRM_MAX_SURFACES];
    NvU32                   bufCount;

    // TODO: Temporary #define to prevent build breaks with other modules.
    // To be removed.
#define EGLIMAGE_MEMFD_TRANSITION 1
    int                     memFd;
    NvU32                   alphaformat;
    NvU32                   colorspace;
    NvU32                   allowedquality;
    NvEglImageSyncObj*      syncObj;

    // Until GLSI is implemented and used for all platforms, the GLSI image
    // will be treated as a member here. A non-NULL value here implies
    // that the EGLImage is to be managed by GLSI.
    struct NvGlsiEglImageRec* glsiImage;
} NvEglApiImageInfo;

//===========================================================================
// EGL_Image underlying NvRmSurface's information
//===========================================================================
typedef struct NvEglApiNvRmSurfaceInfoRec
{
    NvRmSurface             buf[NVRM_MAX_SURFACES];
    int                     memFd[NVRM_MAX_SURFACES];
    NvU32                   bufCount;
} NvEglApiNvRmSurfaceInfo;

//===========================================================================
// NvEglApiCreateImageGlsiAttribs - Struct for passing attribs to
// createEglApiImageGlsi.
//===========================================================================
typedef struct NvEglApiCreateImageGlsiAttribsRec
{
    NvU32           level;
    NvU32           zOffset;
    NvBool          preserve;
} NvEglApiCreateImageGlsiAttribs;

//===========================================================================
// NvEglApiStreamFrame - Stream frame.
//===========================================================================
typedef struct NvEglApiStreamFrameRec
{
    NvEglApiClientBuffer native;
    NvEglApiImageInfo    info;
    NvRmFence            fence;
    NvU64                time;
} NvEglApiStreamFrame;

//===========================================================================
// NvGlContextPriority - Context priority level
// HIGH as the lowest value, while counter-intuitive, is how the
// spec orders them. This allows some shortcuts for conversion.
//===========================================================================
typedef enum NvGlContextPriorityEnum {
    NV_GL_CONTEXT_PRIORITY_HIGH   = 0,
    NV_GL_CONTEXT_PRIORITY_MEDIUM = 1,
    NV_GL_CONTEXT_PRIORITY_LOW    = 2,
} NvGlContextPriority;

//===========================================================================
// NvGlResetNotification - Reset notification setting
//===========================================================================
typedef enum NvGlResetNotificationEnum {
    NV_GL_NO_RESET_NOTIFICATION  = 0,
    NV_GL_LOSE_CONTEXT_ON_RESET = 1,
} NvGlResetNotification;

//===========================================================================
// NvEglApiCreateContextAttribs - Parameters for client API context creation
//===========================================================================
typedef struct NvEglApiCreateContextAttribsRec
{
    // Core EGL
    NvU32           versionMajor;

    // EGL_KHR_create_context
    NvU32           versionMinor;
    NvBool          coreProfile;
    NvBool          compatibilityProfile;
    NvBool          debug;
    NvBool          forwardCompatible;

    // EGL_NV_secure_context
    NvBool          secure;
    NvBool          validateSecure;

    // EGL_EXT_create_context_robustness
    NvBool                robustAccess;
    NvGlResetNotification resetStrategy;

    // EGL_IMG_context_priority
    NvGlContextPriority   priorityLevel;

    // Other
    NvEglApiIdx     apiIdx;
    NvU32           nativeConfigID;
} NvEglApiCreateContextAttribs;

//===========================================================================
// NvEglContextShare
//===========================================================================
typedef struct
{
    NvS32                       refs;
    NvS32                       currentCnt;
    NvOsMutexHandle             mutex;
    NvS32                       lockCount;
    pthread_t                   lockingThread;  // for debugging purpose.
} NvEglContextShare;

//===========================================================================
// NvEglApiImports - Client API functions called by EGL
//===========================================================================
typedef struct NvEglApiImportsRec
{
    // called by eglTerminate() when the implementation is shutting down the
    // client API.
    //
    void                      (*shutdown)(void);

    // Called when EGL is creating an EGLdisplay.  The client API should create
    // and return a NvEglApiDisplay handle which EGL will use thereafter to
    // identify the display to this client API.  Generally the NvEglApiDisplay
    // handle will actually be a pointer to a structure that the client API
    // creates to contain any display and device specific information that the
    // client API may need.  NULL is returned on failure.
    //
    NvEglApiDisplay*          (*displayCreate)(NvEglDisplay* pEglDisplay,
                                               void *glsiCtx,
                                               NvU32 deviceNumber);

    // Called when EGL is destroying an EGLdisplay.  The client API should
    // release all resources associated with the display and deallocate the
    // NvEglApiDisplay display handle.
    //
    void                      (*displayDestroy)(
                                  NvEglApiDisplay*          display);

    // EGL can call this to enable debug tracing via the tracePushbuf() Export
    // function.  The client API may call tracePushbuf() only if
    // displayTracePushbuf() is called with enable==NV_TRUE.  Returns the old
    // value of enable.  This function is probably not implemented yet.
    //
    NvError                   (*displayTracePushbuf)(
                                  NvEglApiDisplay*          display,
                                  NvBool                    enable);

    // Called when EGL is creating an EGLContext.  The client API should create
    // and return an NvEglApiContext handle which EGL will use thereafter to
    // identify the context to the client API.  Generally the NvEglApiContext
    // handle will actually be a pointer to a structure that the client API
    // creates to contain any context specific information that the client API
    // may need.  NULL is returned on failure.
    //
    NvEglApiContext*           (*contextCreate)(
                                  NvEglApiContext               *share,
                                  NvEglApiDisplay               *display,
                                  NvEglApiCreateContextAttribs  *attribs,
                                  NvEglContextShare             *eglShareState);

    // Called when EGL is destroying an EGLContext.  The client API should
    // release all resources associated with the context and deallocate the
    // NvEglApiContext handle.  Returns EGL_TRUE on success.
    //
    void                      (*contextDestroy)(
                                  NvEglApiContext           *apiCtx);

    // Called when EGL is creating an EGLSurface.  The client API should create
    // and return an NvEglApiSurface handle which EGL will use thereafter to
    // identify the surface to the client API.  Generally the NvEglApiSurface
    // handle will actually be a pointer to a structure that the client API
    // creates to contain any surface specific information that the client API
    // may need.  NULL is returned on failure.
    //
    //
    NvEglApiSurface*          (*surfaceCreate)(
                                  NvEglSurface               *pEglSurface,
                                  NvEglApiDisplay            *display);

    // Called when EGL is destroying an EGLSurface.  The client API should
    // release all resources associated with the surface and deallocate the
    // NvEglApiSurface handle.
    //
    void                      (*surfaceDestroy)(
                                  NvEglApiSurface           *surface);

    // Called when EGL is making context current. Checks if surface is
    // compatible with client API
    NvBool                    (*surfaceCompatible)(
                                  NvEglApiSurface           *surface);

    // Called before EGL posts a surface to resolve brute force antialiasing
    NvError                   (*surfaceResolve)(void);

    // Called when a context is bound to the current thread and surfaces
    // are associated to the context.
    //
    NvError                   (*makeContextCurrent)(
                                  NvEglContext              *eglCtx,
                                  NvEglApiContext           *apiCtx,
                                  NvEglApiSurface           *draw,
                                  NvEglApiSurface           *read);

    // Called when a context is unbound.
    //
    NvError                   (*loseCurrent)(
                                  NvEglApiContext           *apiCtx);

    // This is called by EGL when more video memory is needed and maybe when
    // allocating surfaces.  It is a request to the client API to free up any
    // unused video memory and to evict textures and other objects that can be
    // evicted from video memory to system memory.
    //
    void                      (*freeResources)(
                                  NvEglApiContext              *apiCtx,
                                  int nonActiveOnly);


    // Modify a client API resource to be used as a EGLImage source,
    // potentially reallocating memory. Fill in the descriptor of the
    // resource.
    //
    // Called by eglCreateImage().
    //
    NvError                  (*createEglApiImage)(
                                 NvEglApiDisplay        *pDisplay,
                                 NvEglApiContext        *pContext,
                                 NvEglApiClientBuffer    clientBuffer,
                                 NvU32                   target,
                                 NvU32                   lodLevel,
                                 NvBool                  preserveContent,
                                 NvEglApiImageInfo      *info);

    // Similar to createEglApiImage, above, but used when the EGLImage is
    // being managed by GLSI. Aside from using the attribs struct for
    // arguments, another difference is that the value passed to apiTarget
    // should be the target value as used by the API, not EGL.
    NvError                  (*createEglApiImageGlsi)(
                                 NvEglApiDisplay                *pDisplay,
                                 NvEglApiContext                *pContext,
                                 NvEglApiClientBuffer            clientBuffer,
                                 NvU32                           apiTarget,
                                 NvEglApiCreateImageGlsiAttribs *attribs,
                                 struct NvGlsiEglImageRec       *glsiImage);

    // Connects the currently bound TEXTURE_EXTERNAL texture to the given
    // stream. Returns an error is there is no non-zero texture bound.
    // On success, allocates and populates the opaque info struct that
    // will be used by EGL for any future calls.
    //
    // Called by eglStreamConsumerGLTextureKHR().
    //
    NvError                  (*streamGLTextureConsumerConnect)(
                                 NvEglApiContext     *pContext,
                                 NvEglStreamHandle    stream,
                                 NvEglApiStreamInfo  *info);

    // Acquires the given stream frame (only applicable to consumers).
    //
    // Called by eglStreamConsumerAcquireKHR().
    //
    NvError                  (*streamAcquire)(
                                 NvEglApiStreamInfo   info,
                                 NvEglApiImageInfo   *imageInfo);

    // Releases the last frame (as acquired by streamAcquire).
    //
    // Called by eglStreamConsumerReleaseKHR().
    //
    NvError                  (*streamRelease)(
                                 NvEglApiStreamInfo   info);

    // Broadcasts changes to stream attribs to producer/consumer.
    //
    // Called by eglStreamAttrib().
    //
    NvError                  (*streamSetAttrib)(
                                 NvEglApiStreamInfo   info,
                                 NvU32                attr,
                                 NvS32                val);

    // Returns a frame to the producer for future use.
    NvError                  (*streamReturnFrame)(
                                 NvEglApiStreamInfo   info,
                                 NvEglApiStreamFrame *frame);

    // Execute pre-swap operations. This lets the client API copy data
    // into the buffer provided by the OS (if it didn't render directly
    // into it), decompress the OS buffer etc.
    NvError                  (*preSwap)(
                                NvBool preserveBackbuffer);

    // Marks the end of the frame. This is a notification to the client API
    // to do any once periodic operations and cleanup and to refresh the
    // bound render surface info via exports->updateSurface().
    void                     (*frameEnd)(void);

    // Called by EGL for synchronizing the client APIs operation with an
    // external party. This is used by eglFenceKHR, for eglWaitClient and
    // for synchronizing the buffer swap in the native backend.
    //
    // The client API should insert a syncpt into the command stream and fill
    // in the fence structure with that syncpt's information.
    void                     (*issueFence)(NvEglApiContext *pContext,
                                           NvRmFence* fence);

    // Called by EGL for decompressing an image.
    // TODO: Decompress in the GLS channel.
    void                     (*decompressImage)(NvEglApiDisplay *pDisplay,
                                                struct NvGlsiEglImageRec *image,
                                                const NvRmFence *fencesIn,
                                                NvU32 numFencesIn,
                                                NvRmFence *fenceOut);

    // Wait for the syncpt given to have been reached before proceeding with
    // further rendering operations. EGL may call this multiple times between
    // client rendering, so the client API should maintain a list of syncpts
    // to wait upon before starting rendering or wait on the syncpts immediately
    // as they arrive.
    //
    void                     (*waitForFence)(NvEglApiContext *pContext,
                                             const NvRmFence* fence);

    // ClientBuffer to pbuffer mapping

    NvError                  (*bindClientBuffer)(
                               NvEglApiSurface* pbuffer,
                               NvEglApiClientBuffer clientBuffer,
                               NvEglApiClientBufferInfo* infoOut);

    NvError                  (*lockClientBuffer)(NvEglApiSurface* pbuffer);
    void                     (*unlockClientBuffer)(NvEglApiSurface* pbuffer);

    // Release current thread
    void                     (*releaseThread)(void);

    void                     (*eglCUDAInterOpFunc)(void *);


    // Called after initializing the API to get a pointer to API extension
    // functions. Needed for eglGetProcAddress to work correctly for extension
    // functions that are shared between ES2 and ES1.
    //
    NvEglApiExtnProcAddrTable* (*getApiExtensionFuncs)(void);

    // Called when more than one context in a sharing group becomes
    // simultaneously active. Turns on mutexes for the sharing group.
    //
    NvError                  (*becomeMultithreaded)(NvEglApiContext* context);

    // Flush dual core thread
    void                     (*finishDualCoreThread)(void);

    NvError                  (*attachThread)(void);
} NvEglApiImports;

//===========================================================================
// NvEglApiExports - EGL functions called by client API
//===========================================================================
typedef struct NvEglApiExportsRec
{
    // Called by the client API to log debug information.
    //
    void                        (*tracePushbuf)(
                                  NvEglDisplay *pEglDisplay,
                                  char                         *fmt, ...);

    NvError                     (*getEglImageInfo) (
                                    NvEglImageHandle            image,
                                    NvEglApiImageInfo          *info);

    void                        (*getEglImageAlignment) (
                                    const NvColorFormat         format,
                                    const NvRmSurfaceLayout     layout,
                                    const NvU32                 width,
                                    const NvU32                 height,
                                    NvU32*                      minAlign,
                                    NvU32*                      minSize,
                                    NvU32*                      pitch);


    // Called by one client API to get the display handle for a different
    // client API.  Used by gles1on2.
    //
    NvEglApiDisplay*            (*getApiDisplay)(
                                  NvEglApiIdx   apiIdx,
                                  NvEglDisplay* pEglDisplay);

    //
    // Retrieve the surface descriptor from EGL, potentially updating values
    // from the native backend.
    //
    // Returns bitmask of NvEglSurfaceDirty bits for values that have changed
    // since the last time the client API got the descriptor.
    //
    // EGL guarantees that the surface descriptor doesn't change between
    // imports->frameDone() calls. This function should be called between a
    // frameDone notification and render start.
    //
    // The descriptor point remains valid until the next call to this
    // function.
    //
    NvU32                       (*getSurfaceDesc)(
                                  NvEglSurface* pSurf,
                                  NvEglApiSurfaceDesc** desc);

    // Called by one client API to get the import functions of another client
    // API.  Returns NULL if the other client API is not initialized.
    // Used by opengles1on2 to get the opengles2 import functions.
    //
    NvEglApiImports*            (*getApiImports)(NvEglApiIdx      apiIdx);

    // Returns the API Index of the bound API for the calling thread.
    NvEglApiIdx                 (*getCurrentApi)(void);

    // Returns the API context of the bound API for the calling thread
    // Context is needed for calling some export functions
    NvEglApiContext*            (*getCurrentApiContext)(void);

    /* TODO: [ahatala 2008-08-08] document */

    void                        (*swapBuffers)(
                                  NvEglDisplay* pDisp,
                                  NvEglSurface* pSurf,
                                  NvRmFence* f);

    //
    // These functions are for client API under-the-hood synchronization
    // of native pixmap EGLImage siblings. This is not a lasting solution,
    // but needed for Android composition.
    //

    /* TODO: [ahatala 2009-10-08] actually document */

    void                        (*syncObjLock)(
                                  NvEglImageSyncObj* obj);

    void                        (*syncObjUnlock)(
                                  NvEglImageSyncObj* obj);

    void                        (*syncObjRef)(
                                  NvEglImageSyncObj* obj,
                                  NvWsiPixmap** pixmapOut);

    void                        (*syncObjUnref)(
                                  NvEglImageSyncObj* obj);

    void                        (*syncObjUpdate)(
                                  NvEglImageSyncObj* obj,
                                  int *memFd);

    // This asks the windowing system to free any optional device resources
    // it might hold so that GL can use it for textures
    void                        (*freeServerResources)(
                                  NvEglDisplay* pDisp);

    // Read persistent driver settings from data store
    NvError                     (*getPersistentSetting) (
                                  const char          *keyName,
                                  NvPrstntSettingType *type,
                                  void                *value,
                                  size_t              *valueSize);

    // Write persistent driver settings from data store
    NvError                     (*setPersistentSetting) (
                                  const char          *keyName,
                                  NvPrstntSettingType type,
                                  void                *value,
                                  size_t              valueSize);

    // These functions allow openkode to make sure the nvwsi backend for a
    // display is initialized and query it for the native display. In
    // particular, this allows it to obtain internally created displays
    // on window systems where there is no concept of a default display.
    NvError                     (*wsiDisplayRef)(
                                  NvUPtr      eglDpy);
    void                        (*wsiDisplayUnref)(
                                  NvUPtr      eglDpy);
    NvError                     (*wsiDisplayGetNative)(
                                  NvUPtr      eglDpy,
                                  void        *nativeDisplay);

    // Disconnects the consumer from the stream.
    NvError                     (*streamConsumerDisconnect) (
                                  NvEglStreamHandle    stream);

    // Disconnects the producer from the stream.
    NvError                     (*streamProducerDisconnect) (
                                  NvEglStreamHandle    stream);

    // Presents the next producer frame to the stream and returns
    // the last frame released by the consumer, if there is one.
    NvError                     (*streamProducerPresentFrame) (
                                  NvEglStreamHandle    stream,
                                  NvEglApiStreamFrame *newFrame);

    // Connects the stream to an OpenMAX AL data locator.
    NvError                     (*streamProducerALDataLocatorConnect) (
                                  NvEglStreamHandle    stream,
                                  NvEglApiStreamInfo   info);

    // Connects the stream to a producer.
    NvError                     (*streamProducerConnect) (
                                  NvEglStreamHandle    stream,
                                  NvEglApiStreamInfo   info,
                                  NvEglApiIdx          apiIdx);
    // Add to cache
    void                        (*setBlobFunc) (
                                  NvEglDisplay* pDisplay,
                                  const void *key,
                                  NvU32 keySize,
                                  const void *value,
                                  NvU32 valueSize);
    // Find in cache
    NvU32                       (*getBlobFunc) (
                                  NvEglDisplay* pDisplay,
                                  const void *key,
                                  NvU32 keySize,
                                  void *value,
                                  NvU32 valueSize);
    // Get the path for loading implementation module
    void                        (*getClientImplementationPath) (
                                  const char **path);

    // Connects the stream to an OpenMAX AL DataLocator as Consumer
    NvError                     (*streamConsumerALDataLocatorConnect)(
                                  NvEglStreamHandle stream,
                                  NvEglApiStreamInfo info);

    // Connects the stream as consumer
    NvError                     (*streamConsumerConnect)(
                                  NvEglStreamHandle stream,
                                  NvEglApiStreamInfo info,
                                  NvEglApiIdx        apiIdx);

    // Consumer calls this to Acquire Frame
    NvError                     (*streamConsumerAcquireFrame)(
                                  NvEglStreamHandle stream,
                                  NvEglApiStreamFrame *newFrame,
                                  NvU64  timeout);

    // Consumer calls this to Release Previously acquired Frame
    NvError                     (*streamConsumerReleaseFrame)(
                                  NvEglStreamHandle stream,
                                  NvEglApiStreamFrame *newFrame);

    // Lock/Unlock EGL's mutex
    NvError                     (*lockEgl)(void);
    void                        (*unlockEgl)(void);

    // Get EGL thread data attached to current thread
    void*                       (*getEglThreadData)(void);
    // Set EGL thread data to current thread (used for dualcore thread only)
    void                        (*setEglThreadData)(void *);

    // Get NvRmSurface information from EGL Image
    NvError                     (*fillNvRmSurfaceFromEglImage)(
                                  NvEglImageHandle image,
                                  NvEglApiNvRmSurfaceInfo *info);
    // Decompress EGLImage
    NvError                     (*decompressEglImage)(
                                  NvEglImageHandle image);
} NvEglApiExports;

//===========================================================================
// NvEglApiInitFnPtr - pointer to client API initialization function
//
// TODO [tkarras 09/Aug/2007] It would be good to allow the client APIs to
// create an internal global object during init. The object would get passed
// into the import functions that don't get any other internal object to
// operate on. The current design forces the client APIs to rely on global
// variables.
//===========================================================================
typedef NvError (*NvEglApiInitFnPtr)(NvEglApiImports* imports,
                                     const NvEglApiExports* exports);

//###########################################################################
//####################### FUNCTION PROTOTYPES ###############################
//###########################################################################

//
// This function is exported.
//
extern void NvEglRegClientApi(NvEglApiIdx apiId,
                              NvEglApiInitFnPtr apiInitFnct);

//
// One function prototype for each API that might be present.
//
extern NvError NvGlEsInit(
                NvEglApiImports *imports,
                const NvEglApiExports *exports);
extern NvError NvGlEs2Init(
                NvEglApiImports *imports,
                const NvEglApiExports *exports);
extern NvError NvVgInit(
                NvEglApiImports *imports,
                const NvEglApiExports *exports);
extern NvError NvxEglInit( /* OMX IL */
                NvEglApiImports *imports,
                const NvEglApiExports *exports);
extern NvError NvKdApiInit(
                NvEglApiImports *imports,
                const NvEglApiExports *exports);

//===========================================================================
// NvClientRedirInitFnPtr - Pointer to init function in client implementation
//===========================================================================
typedef void (*NvClientRedirInitFnPtr)(void *funcs);

//===========================================================================
// NvEglInitClientFunctions - Fills in the redir table in client shim
// library using functions in client implementation library.
//
// Returns NvSuccess if successfully initialized. NvError_AlreadyAllocated if
// already been initialized, NvError_NotInitialized for failure
//===========================================================================
typedef NvError (*NvEglInitClientFunctionsFnPtr)(NvU32 apiIdx,
                                                 NvOsLibraryHandle *handle,
                                                 void *functions);
NvError NvEglInitClientFunctions(NvU32 apiIdx, NvOsLibraryHandle *handle,
                                 void *functions);

//===========================================================================
// NvGetDriverVersion - Returns a string that encodes the release
// version and the graphics driver version
//===========================================================================
void NvEglGetDriverVersion(char ver[256]);

#endif // EGLAPIINTERFACE_H

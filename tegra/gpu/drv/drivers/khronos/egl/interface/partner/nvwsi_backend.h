/*
 * Copyright (c) 2009 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVWSI_BACKEND_H
#define NVWSI_BACKEND_H

#include "nvwsi.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Functions implemented by the backend
//

/**
 * Whether the display handle represents a valid display for the backend.
 * All backends are required to support a "default" display, so NV_TRUE must
 * be returned at least for value 0.
 */
typedef NvBool  (*NvWsiBackendIsDisplayType)      (NvWsiNativeDisplay Dpy);

/**
 * The backend's best guess on whether the window handle represents a valid window.
 * If the backend can't deduce this information it is always acceptable to return
 * NV_TRUE and fail the backend window object creation later.
 * If this function is left unimplemented, the backend rejects all windows.
 */
typedef NvBool  (*NvWsiBackendIsWindowType)       (NvWsiNativeWindow Win);

/**
 * The backend's best guess on whether the pixmap handle represents a valid pixmap.
 * See NvWsiBackendIsWindowType().
 */
typedef NvBool  (*NvWsiBackendIsPixmapType)       (NvWsiNativePixmap Type);

/**
 * Create a backend internal context. The context can be used to store state that
 * is common between backend integration objects, such as connections to the window
 * manager. The backend doesn't necessarily need a context, it is ok to return
 * a pointer to the NvWsi core context or NULL in Ctx. NvWsi maintains a reference
 * count on the backend internal context. The context is only kept alive while there
 * are active integration objects in existence.
 */
typedef NvError (*NvWsiBackendContextCreateType)  (NvWsiContext* Core, void** Ctx);

/**
 * Destroy a backend internal context.
 */
typedef void    (*NvWsiBackendContextDestroyType) (void* Ctx);

/**
 *
 */
typedef NvError (*NvWsiBackendWindowCreateType)   (void* Ctx, NvWsiNativeWindow Win,
                                                   NvColorFormat FormatReq, NvBool CoverageReq,
                                                   NvU32 StereoInfo,
                                                   NvU32 BackBuffers,
                                                   NvWsiWindow** out);

/**
 *
 */
typedef NvError (*NvWsiBackendPixmapCreateType)   (void* Ctx, NvWsiNativePixmap Pix,
                                                   NvWsiMemAlignmentType Layout,
                                                   NvWsiPixmap** out);

/**
 * Ask the backend to free any device resources it might hold
 */
typedef NvError (*NvWsiBackendFreeResourcesType)  (void* Ctx);

/**
 * Get the native visual type and ID(s) corresponding to a color format
 */
typedef NvError (*NvWsiBackendGetNativeVisualType)(NvWsiContext* ctx,
                                                   NvColorFormat format,
                                                   NvWsiNativeVisualType* type,
                                                   NvWsiNativeVisualID* fast,
                                                   NvWsiNativeVisualID* slow);

/**
 * Get any display capabilities which differ from the defaults. Only the
 * backend which owns the native display should apply its values.
 */
typedef NvError (*NvWsiBackendDisplayCaps)(void* ctx, NvWsiDisplayCaps* caps);

typedef struct
{
    int                             Version;
    const char*                     Name;
    const char*                     Vendor;

    NvWsiBackendIsDisplayType       IsDisplay;
    NvWsiBackendIsWindowType        IsWindow;
    NvWsiBackendIsPixmapType        IsPixmap;
    NvWsiBackendContextCreateType   ContextCreate;
    NvWsiBackendContextDestroyType  ContextDestroy;
    NvWsiBackendWindowCreateType    WindowCreate;
    NvWsiBackendPixmapCreateType    PixmapCreate;
    NvWsiBackendFreeResourcesType   FreeResources;
    NvWsiBackendGetNativeVisualType GetNativeVisual;
    NvWsiBackendDisplayCaps         DisplayCaps;
} NvWsiBackend;

const NvWsiBackend* NvWsiGetBackend (void);

//
// NvWsi core functions exported to backends
//

NvRmDeviceHandle
NvWsiGetRmDevice(
    NvWsiContext* Core);

void
NvWsiDelayedMemFree(
    NvWsiContext* Core,
    NvRmMemHandle Mem,
    const NvRmFence* Fence);

void
NvWsiCompositionDone(
    NvWsiContext* Core);

void
NvWsiSuspendClientLocks(
    NvWsiContext* Core);

void NvWsiResumeClientLocks(
    NvWsiContext* Core);

void NvWsiIssueFence(
    NvWsiContext* Ctx,
    NvRmFence* fence);

void NvWsiWaitFence(
    NvWsiContext* Ctx,
    NvRmFence* fence);

#ifdef __cplusplus
}
#endif
#endif


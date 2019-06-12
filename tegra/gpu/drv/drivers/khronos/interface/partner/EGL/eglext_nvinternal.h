#ifndef __eglext_nvinternal_h_
#define __eglext_nvinternal_h_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef EGL_NVX_stereo_layout
#define EGL_NVX_stereo_layout 1
/* Add surface attribute EGL_STEREO_LAYOUT_NVX:
 * The side-by-side format implies that the left and right sub-images
 * are of half width and placed next to each other in the image.
 * The top-bottom format implies that the left and right sub-images are
 * of half height and kept one above the other in the image.
 * "Left first" implies that the first subimage starting from the top
 * for top-bottom and starting from left for side-by-side is the left
 * image.
 */
#define EGL_STEREO_LAYOUT_NVX 0x3130
#define EGL_STEREO_LAYOUT_NONE_NVX                   0x000000
#define EGL_STEREO_LAYOUT_SIDEBYSIDE_LEFT_FIRST_NVX  0x0B8000
#define EGL_STEREO_LAYOUT_SIDEBYSIDE_RIGHT_FIRST_NVX 0x138000
#define EGL_STEREO_LAYOUT_TOPBOTTOM_LEFT_FIRST_NVX   0x0C8000
#define EGL_STEREO_LAYOUT_TOPBOTTOM_RIGHT_FIRST_NVX  0x148000
#define EGL_STEREO_LAYOUT_INVALID_NVX                0xFF7FFF
#endif /* EGL_NVX_stereo_layout */


#ifndef EGL_NVX_downscale
#define EGL_NVX_downscale 1
/* Add surface attribute EGL_DOWNSCALE_PERCENT_NVX:
 * Backdoor to set stereo downscale factor that is normally
 * read from the app stereo profile.
 */
#define EGL_DOWNSCALE_PERCENT_NVX 0x3137
#endif


#ifndef EGL_NVX_image_nvrmsurface
#define EGL_NVX_image_nvrmsurface 1
/* Add EGL_NVRM_SURFACE_NVX EGLImage target:
 * Create an EGLImage from an NvRmSurface.
 */
#define EGL_NVRM_SURFACE_NVX 0x3135

/* Add EGL_NVRM_SURFACE_COUNT_NVX EGLImage attribute:
 * Specify the number of NvRmSurface planes in an EGLImage.
 */
#define EGL_NVRM_SURFACE_COUNT_NVX 0x3144
#endif /* EGL_NVX_image_nvrmsurface */

#ifndef EGL_NVX_image_decompress
#define EGL_NVX_image_decompress 1
/* Fence synchronization is available only on Android */
#define EGL_WAIT_FENCE_FD_NVX   0x3228
#define EGL_ISSUE_FENCE_FD_NVX  0x3229
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglDecompressImageNVX (EGLDisplay dpy, EGLImageKHR image, EGLint *attrib_list);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDECOMPRESSIMAGENVXPROC) (EGLDisplay dpy, EGLImageKHR image, EGLint *attrib_list);
#endif

#if !defined(EGL_NV_perfmon_hw_counters) && defined(EGL_NV_perfmon)
#define EGL_NV_perfmon_hw_counters 1
EGLAPI EGLBoolean EGLAPIENTRY eglPerfGetCoreNumNV(EGLint * coreNum);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfGetCounterRegisterNV(EGLPerfMarkerNV marker, EGLint * size ,EGLint * registers);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfGetActiveCounterRegisterNV(EGLint *size, EGLint * activeList);
EGLAPI EGLBoolean EGLAPIENTRY eglReadCurrentPerfMarkerNV(void);
#endif

#ifdef __cplusplus
}
#endif

#endif

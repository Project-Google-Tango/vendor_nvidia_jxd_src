#ifndef __eglext_nv_h_
#define __eglext_nv_h_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (c) 2008 - 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <EGL/eglplatform.h>
#include <EGL/eglext.h>

/* EGL_NV_system_time
 */
#ifndef EGL_NV_system_time
#define EGL_NV_system_time 1
typedef khronos_int64_t EGLint64NV;
typedef khronos_uint64_t EGLuint64NV;
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLuint64NV EGLAPIENTRY eglGetSystemTimeFrequencyNV(void);
EGLAPI EGLuint64NV EGLAPIENTRY eglGetSystemTimeNV(void);
#endif
typedef EGLuint64NV (EGLAPIENTRYP PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC)(void);
typedef EGLuint64NV (EGLAPIENTRYP PFNEGLGETSYSTEMTIMENVPROC)(void);
#endif

/* EGL_NV_perfmon
 */
#ifndef EGL_NV_perfmon
#define EGL_NV_perfmon 1
#define EGL_PERFMONITOR_HARDWARE_COUNTERS_BIT_NV    0x00000001
#define EGL_PERFMONITOR_OPENGL_ES_API_BIT_NV        0x00000010
#define EGL_PERFMONITOR_OPENVG_API_BIT_NV           0x00000020
#define EGL_PERFMONITOR_OPENGL_ES2_API_BIT_NV       0x00000040
#define EGL_COUNTER_NAME_NV                         0x3220
#define EGL_COUNTER_DESCRIPTION_NV                  0x3221
#define EGL_IS_HARDWARE_COUNTER_NV                  0x3222
#define EGL_COUNTER_MAX_NV                          0x3223
#define EGL_COUNTER_VALUE_TYPE_NV                   0x3224
#define EGL_RAW_VALUE_NV                            0x3225
#define EGL_PERCENTAGE_VALUE_NV                     0x3226
#define EGL_BAD_CURRENT_PERFMONITOR_NV              0x3227
#define EGL_NO_PERFMONITOR_NV ((EGLPerfMonitorNV)0)
#define EGL_DEFAULT_PERFMARKER_NV ((EGLPerfMarkerNV)0)
typedef void *EGLPerfMonitorNV;
typedef void *EGLPerfCounterNV;
typedef void *EGLPerfMarkerNV;
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLPerfMonitorNV EGLAPIENTRY eglCreatePerfMonitorNV(EGLDisplay dpy, EGLint mask);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyPerfMonitorNV(EGLDisplay dpy, EGLPerfMonitorNV monitor);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrentPerfMonitorNV(EGLPerfMonitorNV monitor);
EGLAPI EGLPerfMonitorNV EGLAPIENTRY eglGetCurrentPerfMonitorNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfCountersNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV *counters, EGLint counter_size, EGLint *num_counter);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfCounterAttribNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname, EGLint *value);
EGLAPI const char * EGLAPIENTRY eglQueryPerfCounterStringNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorAddCountersNV(EGLint n, const EGLPerfCounterNV *counters);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorRemoveCountersNV(EGLint n, const EGLPerfCounterNV *counters);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorRemoveAllCountersNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorBeginExperimentNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorEndExperimentNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorBeginPassNV(EGLint n);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorEndPassNV(void);
EGLAPI EGLPerfMarkerNV EGLAPIENTRY eglCreatePerfMarkerNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyPerfMarkerNV(EGLPerfMarkerNV marker);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrentPerfMarkerNV(EGLPerfMarkerNV marker);
EGLAPI EGLPerfMarkerNV EGLAPIENTRY eglGetCurrentPerfMarkerNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfMarkerCounterNV(EGLPerfMarkerNV marker, EGLPerfCounterNV counter, EGLuint64NV *value, EGLuint64NV *cycles);
EGLAPI EGLBoolean EGLAPIENTRY eglValidatePerfMonitorNV(EGLint *num_passes);
#endif
typedef EGLPerfMonitorNV (EGLAPIENTRYP PFNEGLCREATEPERFMONITORNVPROC)(EGLDisplay dpy, EGLint mask);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYPERFMONITORNVPROC)(EGLDisplay dpy, EGLPerfMonitorNV monitor);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLMAKECURRENTPERFMONITORNVPROC)(EGLPerfMonitorNV monitor);
typedef EGLPerfMonitorNV (EGLAPIENTRYP PFNEGLGETCURRENTPERFMONITORNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFCOUNTERSNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV *counters, EGLint counter_size, EGLint *num_counter);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFCOUNTERATTRIBNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname, EGLint *value);
typedef const char * (EGLAPIENTRYP PFNEGLQUERYPERFCOUNTERSTRINGNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORADDCOUNTERSNVPROC)(EGLint n, const EGLPerfCounterNV *counters);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORREMOVECOUNTERSNVPROC)(EGLint n, const EGLPerfCounterNV *counters);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORREMOVEALLCOUNTERSNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORBEGINEXPERIMENTNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORENDEXPERIMENTNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORBEGINPASSNVPROC)(EGLint n);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORENDPASSNVPROC)(void);
typedef EGLPerfMarkerNV (EGLAPIENTRYP PFNEGLCREATEPERFMARKERNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYPERFMARKERNVPROC)(EGLPerfMarkerNV marker);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLMAKECURRENTPERFMARKERNVPROC)(EGLPerfMarkerNV marker);
typedef EGLPerfMarkerNV (EGLAPIENTRYP PFNEGLGETCURRENTPERFMARKERNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFMARKERCOUNTERNVPROC)(EGLPerfMarkerNV marker, EGLPerfCounterNV counter, EGLuint64NV *value, EGLuint64NV *cycles);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLVALIDATEPERFMONITORNVPROC)(EGLint *num_passes);
#endif

/* EGL_NV_coverage_sample
 */
#ifndef EGL_NV_coverage_sample
#define EGL_NV_coverage_sample 1
#define EGL_COVERAGE_BUFFERS_NV 0x30E0
#define EGL_COVERAGE_SAMPLES_NV 0x30E1
#endif

/* EGL_NV_depth_nonlinear
 */
#ifndef EGL_NV_depth_nonlinear
#define EGL_NV_depth_nonlinear 1
#define EGL_DEPTH_ENCODING_NV 0x30E2
#define EGL_DEPTH_ENCODING_NONE_NV 0
#define EGL_DEPTH_ENCODING_NONLINEAR_NV 0x30E3
#endif

/* EGL_NV_sync
 */
#ifndef EGL_NV_sync
#define EGL_NV_sync 1
typedef void* EGLSyncNV;
typedef unsigned long long EGLTimeNV;
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_NV     0x30E6
#define EGL_SYNC_STATUS_NV                      0x30E7
#define EGL_SIGNALED_NV                         0x30E8
#define EGL_UNSIGNALED_NV                       0x30E9
#define EGL_SYNC_FLUSH_COMMANDS_BIT_NV          0x0001
#define EGL_FOREVER_NV                          0xFFFFFFFFFFFFFFFFull
#define EGL_ALREADY_SIGNALED_NV                 0x30EA
#define EGL_TIMEOUT_EXPIRED_NV                  0x30EB
#define EGL_CONDITION_SATISFIED_NV              0x30EC
#define EGL_SYNC_TYPE_NV                        0x30ED
#define EGL_SYNC_CONDITION_NV                   0x30EE
#define EGL_SYNC_FENCE_NV                       0x30EF
#define EGL_NO_SYNC_NV                          ((EGLSyncNV)0)
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLSyncNV EGLAPIENTRY eglCreateFenceSyncNV( EGLDisplay dpy, EGLenum condition, const EGLint *attrib_list );
EGLAPI EGLBoolean EGLAPIENTRY eglDestroySyncNV( EGLSyncNV sync );
EGLAPI EGLBoolean EGLAPIENTRY eglFenceNV( EGLSyncNV sync );
EGLAPI EGLint EGLAPIENTRY eglClientWaitSyncNV( EGLSyncNV sync, EGLint flags, EGLTimeNV timeout );
EGLAPI EGLBoolean EGLAPIENTRY eglSignalSyncNV( EGLSyncNV sync, EGLenum mode );
EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncAttribNV( EGLSyncNV sync, EGLint attribute, EGLint *value );
#else
typedef EGLSyncNV (EGLAPIENTRYP PFNEGLCREATEFENCESYNCNVPROC)( EGLDisplay dpy, EGLenum condition, const EGLint *attrib_list );
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYSYNCNVPROC)( EGLSyncNV sync );
typedef EGLBoolean (EGLAPIENTRYP PFNEGLFENCENVPROC)( EGLSyncNV sync );
typedef EGLint (EGLAPIENTRYP PFNEGLCLIENTWAITSYNCNVPROC)( EGLSyncNV sync, EGLint flags, EGLTimeNV timeout );
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSIGNALSYNCNVPROC)( EGLSyncNV sync, EGLenum mode );
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETSYNCATTRIBNVPROC)( EGLSyncNV sync, EGLint attribute, EGLint *value );
#endif
#endif

/* EGL_NV_post_sub_buffer
 */
#ifndef EGL_NV_post_sub_buffer
#define EGL_NV_post_sub_buffer 1
#define EGL_POST_SUB_BUFFER_SUPPORTED_NV        0x30BE
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglPostSubBufferNV( EGLDisplay dpy, EGLSurface draw, EGLint x, EGLint y, EGLint width, EGLint height );
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPOSTSUBBUFFERNVPROC)( EGLDisplay dpy, EGLSurface draw, EGLint x, EGLint y, EGLint width, EGLint height);
#endif

/* EGL_NV_coverage_sample
 */
#ifndef EGL_NV_coverage_sample_resolve
#define EGL_NV_coverage_sample_resolve 1
#define EGL_COVERAGE_SAMPLE_RESOLVE_NV          0x3131
#define EGL_COVERAGE_SAMPLE_RESOLVE_DEFAULT_NV  0x3132
#define EGL_COVERAGE_SAMPLE_RESOLVE_NONE_NV     0x3133
#endif

/* EGL_EXT_multiview_window
*/
#ifndef EGL_EXT_multiview_window
#define EGL_EXT_multiview_window      1
#define EGL_MULTIVIEW_VIEW_COUNT_EXT             0x3134
#endif


/* EGL_EXT_create_context_robustness
*/
#ifndef EGL_EXT_create_context_robustness
#define EGL_EXT_create_context_robustness   1
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT        0x30BF
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define EGL_NO_RESET_NOTIFICATION_EXT               0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_EXT               0x31BF
#endif

/* EGL_KHR_create_context
*/
#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1
#define EGL_CONTEXT_MAJOR_VERSION_KHR                       EGL_CONTEXT_CLIENT_VERSION
#define EGL_CONTEXT_MINOR_VERSION_KHR                       0x30FB
#define EGL_CONTEXT_FLAGS_KHR                               0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR                 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR  0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR                       0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR                       0x31BF
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR                    0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR       0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR            0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR             0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR    0x00000002
#endif

#ifndef EGL_OPENGL_ES3_BIT_KHR
// XXX: this symbol was missing from older versions of the extension spec
//      temporarily add it here until Android eglext.h gets a version bump
#define EGL_OPENGL_ES3_BIT_KHR                              0x00000040
#endif

/* EGL_NV_native_query
 */
#ifndef EGL_NV_native_query
#define EGL_NV_native_query 1
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglQueryNativeDisplayNV(EGLDisplay dpy, EGLNativeDisplayType *display_id);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryNativeWindowNV(EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType *window);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryNativePixmapNV(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType *pixmap);
#else
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYNATIVEDISPLAYNVPROC)(EGLDisplay dpy, EGLNativeDisplayType *display_id);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYNATIVEWINDOWNVPROC)(EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType *window);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYNATIVEPIXMAPNVPROC)(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType *pixmap);
#endif
#endif

/* EGL_KHR_stream
*/
#ifndef EGL_KHR_stream
#define EGL_KHR_stream 1
typedef void* EGLStreamKHR;
typedef khronos_uint64_t EGLuint64KHR;
#define EGL_NO_STREAM_KHR                           ((EGLStreamKHR)0)
#define EGL_CONSUMER_LATENCY_USEC_KHR               0x3210
#define EGL_PRODUCER_FRAME_KHR                      0x3212
#define EGL_CONSUMER_FRAME_KHR                      0x3213
#define EGL_STREAM_STATE_KHR                        0x3214
#define EGL_STREAM_STATE_CREATED_KHR                0x3215
#define EGL_STREAM_STATE_CONNECTING_KHR             0x3216
#define EGL_STREAM_STATE_EMPTY_KHR                  0x3217
#define EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR    0x3218
#define EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR    0x3219
#define EGL_STREAM_STATE_DISCONNECTED_KHR           0x321A
#define EGL_BAD_STREAM_KHR                          0x321B
#define EGL_BAD_STATE_KHR                           0x321C
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLStreamKHR EGLAPIENTRY eglCreateStreamKHR(EGLDisplay dpy, const EGLint *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamAttribKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamu64KHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value);
#endif
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMKHRPROC)(EGLDisplay dpy, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYSTREAMKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMATTRIBKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMU64KHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value);
#endif

/* EGL_KHR_stream_fifo
*/
#ifndef EGL_KHR_stream_fifo
#define EGL_KHR_stream_fifo 1
// EGLTimeKHR also defined by EGL_KHR_reusable_sync
#ifndef EGL_KHR_reusable_sync
typedef khronos_utime_nanoseconds_t EGLTimeKHR;
#endif
#define EGL_STREAM_FIFO_LENGTH_KHR                  0x31FC
#define EGL_STREAM_TIME_NOW_KHR                     0x31FD
#define EGL_STREAM_TIME_CONSUMER_KHR                0x31FE
#define EGL_STREAM_TIME_PRODUCER_KHR                0x31FF
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamTimeKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR *value);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMTIMEKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR *value);
#endif

/* EGL_KHR_stream_producer_eglsurface
*/
#ifndef EGL_KHR_stream_producer_eglsurface
#define EGL_KHR_stream_producer_eglsurface 1
#define EGL_STREAM_BIT_KHR                          0x0800
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLSurface EGLAPIENTRY eglCreateStreamProducerSurfaceKHR(EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list);
#endif
typedef EGLSurface (EGLAPIENTRYP PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC)(EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list);
#endif

/* EGL_KHR_stream_consumer_gltexture
*/
#ifndef EGL_KHR_stream_consumer_gltexture
#define EGL_KHR_stream_consumer_gltexture 1
#define EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR       0x321E
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerGLTextureExternalKHR(EGLDisplay dpy, EGLStreamKHR stream);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
#endif

/* EGL_KHR_stream_cross_process_fd
*/
#ifndef EGL_KHR_stream_cross_process_fd
#define EGL_KHR_stream_cross_process_fd 1
typedef int EGLNativeFileDescriptorKHR;
#define EGL_NO_FILE_DESCRIPTOR_KHR                   ((EGLNativeFileDescriptorKHR)(-1))
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLNativeFileDescriptorKHR EGLAPIENTRY eglGetStreamFileDescriptorKHR(EGLDisplay dpy, EGLStreamKHR stream);
EGLAPI EGLStreamKHR EGLAPIENTRY eglCreateStreamFromFileDescriptorKHR(EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
#endif
typedef EGLNativeFileDescriptorKHR (EGLAPIENTRYP PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC)(EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
#endif

/* EGL_NV_3dvision_surface
 */
#ifndef EGL_NV_3dvision_surface
#define EGL_NV_3dvision_surface 1
#define EGL_AUTO_STEREO_NV              0x3136
#endif

/* EGL_NV_set_renderer
 */
#ifndef EGL_NV_set_renderer
#define EGL_NV_set_renderer 1
#define EGL_RENDERER_LOWEST_POWER_NV         0x313A
#define EGL_RENDERER_HIGHEST_PERFORMANCE_NV  0x313B
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglSetRendererNV(EGLenum renderer);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETRENDERERNVPROC)(EGLenum renderer);
#endif

/* EGL_NV_stream_sync
*/
#ifndef EGL_NV_stream_sync
#define EGL_NV_stream_sync 1
#define EGL_SYNC_NEW_FRAME_NV 0x321F
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLSyncKHR EGLAPIENTRY eglCreateStreamSyncNV(EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint *attrib_list);
#endif
typedef EGLSyncKHR (EGLAPIENTRYP PFNEGLCREATESTREAMSYNCNVPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint *attrib_list);
#endif

/* EGL_NV_displayless_extensions
 */
#ifndef EGL_NV_displayless_extensions
#define EGL_NV_displayless_extensions 1
#define EGL_DISPLAYLESS_EXTENSIONS_NV       0x313C
#endif

/* EGL_NV_triple_buffer and EGL_NV_quadruple_buffer */
#ifndef EGL_NV_triple_buffer
#define EGL_NV_triple_buffer 1
#define EGL_TRIPLE_BUFFER_NV                0x3230
#endif

#ifndef EGL_NV_quadruple_buffer
#define EGL_NV_quadruple_buffer 1
#define EGL_QUADRUPLE_BUFFER_NV             0x3231
#endif

/* EGL_NV_color_management
 */
#ifndef EGL_NV_color_management
#define EGL_NV_color_management 1
#define EGL_CMS_SUPPORTED_BIT_NV                     0x1000  /* EGL_SURFACE_TYPE mask bits */
#define EGL_ABSOLUTE_COLORSPACE_NV                   0x3228
#define EGL_ABSOLUTE_COLORSPACE_NONE_NV              0x0000
#define EGL_ABSOLUTE_COLORSPACE_sRGB_NV              0x0001
#define EGL_ABSOLUTE_COLORSPACE_Adobe_RGB_NV         0x0002
#endif // EGL_NV_color_management

/* EGL_EXT_buffer_age
 */
#ifndef EGL_EXT_buffer_age
#define EGL_EXT_buffer_age 1
#define EGL_BUFFER_AGE_EXT 0x313D
#endif

/* EGL_NV_swap_asynchrounous
 */
#ifndef EGL_NV_swap_asynchronous
#define EGL_NV_swap_asynchronous
#define EGL_ASYNCHRONOUS_SWAPS_NV 0x3232
#endif

/* EGL_NV_secure_context
 */
#ifndef EGL_NV_secure_context
#define EGL_NV_secure_context 1
#define EGL_SECURE_NV 0x313E
#endif

//
// Deprecated NV extensions. To be removed, do not use.
//

/* EGL_NV_swap_hint
 */
#ifndef EGL_NV_swap_hint
#define EGL_NV_swap_hint
#define EGL_SWAP_HINT_NV                0x30E4 /* eglCreateWindowSurface attribute name */
#define EGL_FASTEST_NV                  0x30E5 /* eglCreateWindowSurface attribute value */
#endif

//
// Disabled NV extensions. To be removed, do not use.
//

/* EGL_NV_texture_rectangle (reuse analagous WGL enum)
*/
#ifndef EGL_NV_texture_rectangle
#define EGL_NV_texture_rectangle 1
#define EGL_GL_TEXTURE_RECTANGLE_NV_KHR           0x30BB
#define EGL_TEXTURE_RECTANGLE_NV       0x20A2
#endif

#ifdef __cplusplus
}
#endif

#endif

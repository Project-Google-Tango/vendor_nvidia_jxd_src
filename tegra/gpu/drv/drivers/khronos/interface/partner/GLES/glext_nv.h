/*
 * Copyright (c) 2010 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __glext_nv_h_
#define __glext_nv_h_

#ifdef __cplusplus
extern "C" {
#endif

/* GL_NV_bgr */

#ifndef GL_NV_bgr
#define GL_NV_bgr 1
#define GL_BGR_NV                                               0x80E0
#endif

/* GL_OES_vertex_half_float */

#ifndef GL_OES_vertex_half_float
#define GL_OES_vertex_half_float 1
#define GL_HALF_FLOAT_OES                                       0x8D61
#endif

/* GL_EXT_debug_markers. */

#ifndef GL_EXT_debug_marker
#define GL_EXT_debug_marker 1
#ifdef GL_GLEXT_PROTOTYPES
GL_API void GL_APIENTRY glInsertEventMarkerEXT(GLsizei length, const char *marker);
GL_API void GL_APIENTRY glPushGroupMarkerEXT(GLsizei length, const char *marker);
GL_API void GL_APIENTRY glPopGroupMarkerEXT(void);
#endif /* GL_GLEXT_PROTOTYPES */
typedef void (GL_APIENTRYP PFNGLINSERTEVENTMARKEREXTPROC) (GLsizei length, const char *marker);
typedef void (GL_APIENTRYP PFNGLPUSHGROUPMARKEREXTPROC) (GLsizei length, const char *marker);
typedef void (GL_APIENTRYP PFNGLPOPGROUPMARKEREXTPROC) (void);
#endif

/* GL_EXT_debug_label */
#ifndef GL_EXT_debug_label
#define GL_EXT_debug_label 1
#define GL_BUFFER_OBJECT_EXT                                    0x9151
#define GL_SHADER_OBJECT_EXT                                    0x8B48
#define GL_PROGRAM_OBJECT_EXT                                   0x8B40
#define GL_VERTEX_ARRAY_OBJECT_EXT                              0x9154
#define GL_QUERY_OBJECT_EXT                                     0x9153
#define GL_PROGRAM_PIPELINE_OBJECT_EXT                          0x8A4F
#ifdef GL_GLEXT_PROTOTYPES
GL_API void GL_APIENTRY glLabelObjectEXT (GLenum type, GLuint object, GLsizei length,
                                               const char *label);
GL_API void GL_APIENTRY glGetObjectLabelEXT (GLenum type, GLuint object, GLsizei bufSize,
                                                   GLsizei *length, char *label);
#endif /* GL_GLEXT_PROTOTYPES */
typedef void (GL_APIENTRYP PFNGLLABELOBJECTEXTPROC) (GLenum type, GLuint object, GLsizei length, const char *label);
typedef void (GL_APIENTRYP PFNGLGETOBJECTLABELEXTPROC) (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, char *label);
#endif /* GL_EXT_debug_label */



/* GL_EXT_texture_compression_dxt1 */

#ifndef GL_EXT_texture_compression_dxt1
#define GL_EXT_texture_compression_dxt1 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                         0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                        0x83F1
#endif

/* GL_EXT_texture_compression_s3tc */

#ifndef GL_EXT_texture_compression_s3tc
#define GL_EXT_texture_compression_s3tc 1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                        0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                        0x83F3
#endif

/* GL_NV_texture_compression_s3tc */

#ifndef GL_NV_texture_compression_s3tc
#define GL_NV_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_NV                          0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_NV                         0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_NV                         0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_NV                         0x83F3
#endif

/* GL_EXT_unpack_subimage */

#ifndef GL_EXT_unpack_subimage
#define GL_EXT_unpack_subimage 1
#define GL_UNPACK_ROW_LENGTH_EXT                                0x0CF2
#define GL_UNPACK_SKIP_ROWS_EXT                                 0x0CF3
#define GL_UNPACK_SKIP_PIXELS_EXT                               0x0CF4
#endif

#ifdef __cplusplus
}
#endif

#endif /* __glext_nv_h_ */


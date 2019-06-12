/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __gl2ext_nv_h_
#define __gl2ext_nv_h_

#ifdef __cplusplus
extern "C" {
#endif

/* GL_NV_platform_binary */

#ifndef GL_NV_platform_binary
#define GL_NV_platform_binary 1
#define GL_NVIDIA_PLATFORM_BINARY_NV                            0x890B
#endif

/* GL_NV_packed_float */

#ifndef GL_NV_packed_float
#define GL_NV_packed_float 1
#define GL_R11F_G11F_B10F_NV                           0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV_NV             0x8C3B
#endif

/* GL_NV_texture_array */

#ifndef GL_NV_texture_array
#define GL_NV_texture_array 1
#define GL_UNPACK_SKIP_IMAGES_NV          0x806D
#define GL_UNPACK_IMAGE_HEIGHT_NV         0x806E
#define GL_TEXTURE_2D_ARRAY_NV            0x8C1A
#define GL_SAMPLER_2D_ARRAY_NV            0x8DC1
#define GL_TEXTURE_BINDING_2D_ARRAY_NV    0x8C1D
#define GL_MAX_ARRAY_TEXTURE_LAYERS_NV    0x88FF
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_NV 0x8CD4
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTexImage3DNV (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
GL_APICALL void GL_APIENTRY glTexSubImage3DNV (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
GL_APICALL void GL_APIENTRY glCopyTexSubImage3DNV (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL void GL_APIENTRY glCompressedTexImage3DNV (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
GL_APICALL void GL_APIENTRY glCompressedTexSubImage3DNV (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
GL_APICALL void GL_APIENTRY glFramebufferTextureLayerNV (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
#endif
typedef void (GL_APIENTRYP PFNGLTEXIMAGE3DNVPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
typedef void (GL_APIENTRYP PFNGLTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
typedef void (GL_APIENTRYP PFNGLCOPYTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLCOMPRESSEDTEXIMAGE3DNVPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GL_APIENTRYP PFNGLCOMPRESSEDTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTURELAYERNVPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
#endif

/* GL_EXT_texture_compression_latc */

#ifndef GL_EXT_texture_compression_latc
#define GL_EXT_texture_compression_latc 1
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT               0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT        0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT         0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT  0x8C73
#endif

/* GL_EXT_texture_compression_s3tc */

#ifndef GL_EXT_texture_compression_s3tc
#define GL_EXT_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

/* GL_NV_texture_compression_latc */

#ifndef GL_NV_texture_compression_latc
#define GL_NV_texture_compression_latc 1
#define GL_COMPRESSED_LUMINANCE_LATC1_NV                0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_NV         0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_NV          0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_NV   0x8C73
#endif

/* GL_NV_texture_compression_s3tc */

#ifndef GL_NV_texture_compression_s3tc
#define GL_NV_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_NV    0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_NV   0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_NV   0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_NV   0x83F3
#endif

/* GL_NV_copy_image */

#ifndef GL_NV_copy_image
#define GL_NV_copy_image 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glCopyImageSubDataNV(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);
#endif
typedef void (GL_APIENTRYP PFNGLCOPYIMAGESUBDATANVPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);
#endif

/* GL_NV_coverage_sample */

#ifndef GL_NV_coverage_sample
#define GL_NV_coverage_sample 1
#define GL_COVERAGE_COMPONENT_NV          0x8ED0
#define GL_COVERAGE_COMPONENT4_NV         0x8ED1
#define GL_COVERAGE_ATTACHMENT_NV         0x8ED2
#define GL_COVERAGE_BUFFERS_NV            0x8ED3
#define GL_COVERAGE_SAMPLES_NV            0x8ED4
#define GL_COVERAGE_ALL_FRAGMENTS_NV      0x8ED5
#define GL_COVERAGE_EDGE_FRAGMENTS_NV     0x8ED6
#define GL_COVERAGE_AUTOMATIC_NV          0x8ED7
#define GL_COVERAGE_BUFFER_BIT_NV         0x8000
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glCoverageMaskNV (GLboolean mask);
GL_APICALL void GL_APIENTRY glCoverageOperationNV (GLenum operation);
#endif
typedef void (GL_APIENTRYP PFNGLCOVERAGEMASKNVPROC) (GLboolean mask);
typedef void (GL_APIENTRYP PFNGLCOVERAGEOPERATIONNVPROC) (GLenum operation);
#endif

/* GL_NV_depth_nonlinear */

#ifndef GL_NV_depth_nonlinear
#define GL_NV_depth_nonlinear 1
#define GL_DEPTH_COMPONENT16_NONLINEAR_NV 0x8E2C
#endif

/* GL_NV_draw_path */

#ifndef GL_NV_draw_path
#define GL_NV_draw_path 1
#define GL_PATH_QUALITY_NV          0x8ED8
#define GL_FILL_RULE_NV             0x8ED9
#define GL_STROKE_CAP0_STYLE_NV     0x8EE0
#define GL_STROKE_CAP1_STYLE_NV     0x8EE1
#define GL_STROKE_CAP2_STYLE_NV     0x8EE2
#define GL_STROKE_CAP3_STYLE_NV     0x8EE3
#define GL_STROKE_JOIN_STYLE_NV     0x8EE8
#define GL_STROKE_MITER_LIMIT_NV    0x8EE9
#define GL_EVEN_ODD_NV              0x8EF0
#define GL_NON_ZERO_NV              0x8EF1
#define GL_CAP_BUTT_NV              0x8EF4
#define GL_CAP_ROUND_NV             0x8EF5
#define GL_CAP_SQUARE_NV            0x8EF6
#define GL_CAP_TRIANGLE_NV          0x8EF7
#define GL_JOIN_MITER_NV            0x8EFC
#define GL_JOIN_ROUND_NV            0x8EFD
#define GL_JOIN_BEVEL_NV            0x8EFE
#define GL_JOIN_CLIPPED_MITER_NV    0x8EFF
#define GL_MATRIX_PATH_TO_CLIP_NV   0x8F04
#define GL_MATRIX_STROKE_TO_PATH_NV 0x8F05
#define GL_MATRIX_PATH_COORD0_NV    0x8F08
#define GL_MATRIX_PATH_COORD1_NV    0x8F09
#define GL_MATRIX_PATH_COORD2_NV    0x8F0A
#define GL_MATRIX_PATH_COORD3_NV    0x8F0B
#define GL_FILL_PATH_NV             0x8F18
#define GL_STROKE_PATH_NV           0x8F19
#define GL_MOVE_TO_NV               0x00
#define GL_LINE_TO_NV               0x01
#define GL_QUADRATIC_BEZIER_TO_NV   0x02
#define GL_CUBIC_BEZIER_TO_NV       0x03
#define GL_START_MARKER_NV          0x20
#define GL_CLOSE_NV                 0x21
#define GL_CLOSE_FILL_NV            0x22
#define GL_STROKE_CAP0_NV           0x40
#define GL_STROKE_CAP1_NV           0x41
#define GL_STROKE_CAP2_NV           0x42
#define GL_STROKE_CAP3_NV           0x43
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GLuint GL_APIENTRY glCreatePathNV (GLenum datatype, GLsizei numCommands, const GLubyte* commands);
GL_APICALL void GL_APIENTRY glDeletePathNV (GLuint path);
GL_APICALL void GL_APIENTRY glPathVerticesNV (GLuint path, const void* vertices);
GL_APICALL void GL_APIENTRY glPathParameterfNV (GLuint path, GLenum paramType, GLfloat param);
GL_APICALL void GL_APIENTRY glPathParameteriNV (GLuint path, GLenum paramType, GLint param);
GL_APICALL GLuint GL_APIENTRY glCreatePathProgramNV (void);
GL_APICALL void GL_APIENTRY glPathMatrixNV (GLenum target, const GLfloat* value);
GL_APICALL void GL_APIENTRY glDrawPathNV (GLuint path, GLenum mode);
GL_APICALL GLuint GL_APIENTRY glCreatePathbufferNV (GLsizei capacity);
GL_APICALL void GL_APIENTRY glDeletePathbufferNV (GLuint buffer);
GL_APICALL void GL_APIENTRY glPathbufferPathNV (GLuint buffer, GLint index, GLuint path);
GL_APICALL void GL_APIENTRY glPathbufferPositionNV (GLuint buffer, GLint index, GLfloat x, GLfloat y);
GL_APICALL void GL_APIENTRY glDrawPathbufferNV (GLuint buffer, GLenum mode);
#endif
typedef GLuint (GL_APIENTRYP PFNGLCREATEPATHNVPROC) (GLenum datatype, GLsizei numCommands, const GLubyte* commands);
typedef void (GL_APIENTRYP PFNGLDELETEPATHNVPROC) (GLuint path);
typedef void (GL_APIENTRYP PFNGLPATHVERTICESNVPROC) (GLuint path, const void* vertices);
typedef void (GL_APIENTRYP PFNGLPATHPARAMETERFNVPROC) (GLuint path, GLenum paramType, GLfloat param);
typedef void (GL_APIENTRYP PFNGLPATHPARAMETERINVPROC) (GLuint path, GLenum paramType, GLint param);
typedef GLuint (GL_APIENTRYP PFNGLCREATEPATHPROGRAMNVPROC) (void);
typedef void (GL_APIENTRYP PFNGLPATHMATRIXNVPROC) (GLenum target, const GLfloat* value);
typedef void (GL_APIENTRYP PFNGLDRAWPATHNVPROC) (GLuint path, GLenum mode);
typedef GLuint (GL_APIENTRYP PFNGLCREATEPATHBUFFERNVPROC) (GLsizei capacity);
typedef void (GL_APIENTRYP PFNGLDELETEPATHBUFFERNVPROC) (GLuint buffer);
typedef void (GL_APIENTRYP PFNGLPATHBUFFERPATHNVPROC) (GLuint buffer, GLint index, GLuint path);
typedef void (GL_APIENTRYP PFNGLPATHBUFFERPOSITIONNVPROC) (GLuint buffer, GLint index, GLfloat x, GLfloat y);
typedef void (GL_APIENTRYP PFNGLDRAWPATHBUFFERPROC) (GLuint buffer, GLenum mode);
#endif

/* GL_NV_shader_framebuffer_fetch */

#ifndef GL_NV_shader_framebuffer_fetch
#define GL_NV_shader_framebuffer_fetch 1
#endif

/* GL_NV_get_tex_image */

#ifndef GL_NV_get_tex_image
#define GL_NV_get_tex_image 1
//those enums are the same as the big gl.h
#define GL_TEXTURE_WIDTH_NV                  0x1000
#define GL_TEXTURE_HEIGHT_NV                 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT_NV        0x1003
#define GL_TEXTURE_COMPONENTS_NV             GL_TEXTURE_INTERNAL_FORMAT_NV
#define GL_TEXTURE_BORDER_NV                 0x1005
#define GL_TEXTURE_RED_SIZE_NV               0x805C
#define GL_TEXTURE_GREEN_SIZE_NV             0x805D
#define GL_TEXTURE_BLUE_SIZE_NV              0x805E
#define GL_TEXTURE_ALPHA_SIZE_NV             0x805F
#define GL_TEXTURE_LUMINANCE_SIZE_NV         0x8060
#define GL_TEXTURE_INTENSITY_SIZE_NV         0x8061
#define GL_TEXTURE_DEPTH_NV                  0x8071
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE_NV  0x86A0
#define GL_TEXTURE_COMPRESSED_NV             0x86A1
#define GL_TEXTURE_DEPTH_SIZE_NV             0x884A
#define GL_PACK_SKIP_IMAGES_NV               0x806B
#define GL_PACK_IMAGE_HEIGHT_NV              0x806C
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGetTexImageNV (GLenum target, GLint level, GLenum format, GLenum type, GLvoid* img);
GL_APICALL void GL_APIENTRY glGetCompressedTexImageNV (GLenum target, GLint level, GLvoid* img);
GL_APICALL void GL_APIENTRY glGetTexLevelParameterfvNV (GLenum target, GLint level, GLenum pname, GLfloat* params);
GL_APICALL void GL_APIENTRY glGetTexLevelParameterivNV (GLenum target, GLint level, GLenum pname, GLint* params);
#endif
typedef void (GL_APIENTRYP PFNGLGETTEXIMAGENVPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLvoid* img);
typedef void (GL_APIENTRYP PFNGLGETCOMPRESSEDTEXIMAGENVPROC) (GLenum target, GLint level, GLvoid* img);
typedef void (GL_APIENTRYP PFNGLGETTEXLEVELPARAMETERFVNVPROC) (GLenum target, GLint level, GLenum pname, GLfloat* params);
typedef void (GL_APIENTRYP PFNGLGETTEXLEVELPARAMETERiVNVPROC) (GLenum target, GLint level, GLenum pname, GLint* params);
#endif

/* GL_NV_bgr */

#ifndef GL_NV_bgr
#define GL_NV_bgr 1
#define GL_BGR_NV                                               0x80E0
#endif


/* GL_NV_framebuffer_vertex_attrib_array */

#ifndef GL_NV_framebuffer_vertex_attrib_array
#define GL_NV_framebuffer_vertex_attrib_array 1
#define GL_FRAMEBUFFER_ATTACHABLE_NV                                  0x852A
#define GL_VERTEX_ATTRIB_ARRAY_NV                                     0x852B
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_SIZE_NV         0x852C
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_TYPE_NV         0x852D
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_NORMALIZED_NV   0x852E
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_OFFSET_NV       0x852F
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_WIDTH_NV        0x8530
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_STRIDE_NV       0x8531
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_HEIGHT_NV       0x8532
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glFramebufferVertexAttribArrayNV (GLenum target, GLenum attachment, GLenum buffertarget, GLuint bufferobject, GLint size, GLenum type, GLboolean normalized, GLintptr offset, GLsizeiptr width, GLsizeiptr height, GLsizei stride);
#endif
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERVERTEXATTRIBARRAYNVPROC) (GLenum target, GLenum attachment, GLenum buffertarget, GLuint bufferobject, GLint size, GLenum type, GLboolean normalized, GLintptr offset, GLsizeiptr width, GLsizeiptr height, GLsizei stride);
#endif


/* GL_NV_draw_buffers */

#ifndef GL_NV_draw_buffers
#define GL_NV_draw_buffers 1
#define GL_MAX_DRAW_BUFFERS_NV                     0x8824
#define GL_DRAW_BUFFER0_NV                         0x8825
#define GL_DRAW_BUFFER1_NV                         0x8826
#define GL_DRAW_BUFFER2_NV                         0x8827
#define GL_DRAW_BUFFER3_NV                         0x8828
#define GL_DRAW_BUFFER4_NV                         0x8829
#define GL_DRAW_BUFFER5_NV                         0x882A
#define GL_DRAW_BUFFER6_NV                         0x882B
#define GL_DRAW_BUFFER7_NV                         0x882C
#define GL_DRAW_BUFFER8_NV                         0x882D
#define GL_DRAW_BUFFER9_NV                         0x882E
#define GL_DRAW_BUFFER10_NV                        0x882F
#define GL_DRAW_BUFFER11_NV                        0x8830
#define GL_DRAW_BUFFER12_NV                        0x8831
#define GL_DRAW_BUFFER13_NV                        0x8832
#define GL_DRAW_BUFFER14_NV                        0x8833
#define GL_DRAW_BUFFER15_NV                        0x8834
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDrawBuffersNV (GLsizei n, const GLenum *bufs);
#endif
typedef void (GL_APIENTRYP PFNGLDRAWBUFFERSNVPROC) (GLsizei n, const GLenum *bufs);
#endif

/* GL_NV_draw_texture */

#ifndef GL_NV_draw_texture
#define GL_NV_draw_texture 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDrawTextureNV(GLuint texture, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);
#endif
typedef void (GL_APIENTRYP PFNGLDRAWTEXTURENVPROC) (GLuint texture, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);
#endif

/* GL_NV_EGL_image_YUV */

#ifndef GL_NV_EGL_image_YUV
#define GL_NV_EGL_image_YUV 1
#define GL_IMAGE_PLANE_Y_NV                        0x313D
#define GL_IMAGE_PLANE_UV_NV                       0x313E
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glEGLImageTargetTexture2DYUVNV(GLenum target, GLenum plane, GLeglImageOES image);
#endif
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DYUVNVPROC) (GLenum target, GLenum plane, GLeglImageOES image);
#endif

/* GL_EXT_multiview_draw_buffers */

#ifndef GL_EXT_multiview_draw_buffers
#define GL_EXT_multiview_draw_buffers 1
#define GL_MAX_MULTIVIEW_BUFFERS_EXT                   0x90F2
#define GL_COLOR_ATTACHMENT_EXT                        0x90F0
#define GL_MULTIVIEW_EXT                               0x90F1
#define GL_DRAW_BUFFER_EXT                             0x0C01
#define GL_READ_BUFFER_EXT                             0x0C02
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDrawBuffersIndexedEXT (GLsizei n, const GLenum *locations, const GLint *indices);
GL_APICALL void GL_APIENTRY glReadBufferIndexedEXT (GLenum location, GLint index);
GL_APICALL void GL_APIENTRY glGetIntegeri_vEXT (GLenum target, GLuint index, GLint *data);
#endif
typedef void (GL_APIENTRYP PFNGLDRAWBUFFERSINDEXEDEXTPROC) (GLsizei n, const GLenum *locations, const GLint *indices);
typedef void (GL_APIENTRYP PFNGLREADBUFFERINDEXEDEXTPROC) (GLenum location, GLint index);
typedef void (GL_APIENTRYP PFNGLGETINTEGERI_VEXTPROC) (GLenum target, GLuint index, GLint *data);
#endif


/* GL_NV_non_square_matrices */

#ifndef GL_NV_non_square_matrices
#define GL_NV_non_square_matrices 1
#define GL_FLOAT_MAT2x3_NV                            0x8B65
#define GL_FLOAT_MAT2x4_NV                            0x8B66
#define GL_FLOAT_MAT3x2_NV                            0x8B67
#define GL_FLOAT_MAT3x4_NV                            0x8B68
#define GL_FLOAT_MAT4x2_NV                            0x8B69
#define GL_FLOAT_MAT4x3_NV                            0x8B6A
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glUniformMatrix2x3fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glUniformMatrix3x2fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glUniformMatrix2x4fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glUniformMatrix4x2fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glUniformMatrix3x4fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glUniformMatrix4x3fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#endif
typedef void (GL_APIENTRYP PFNGLUNIFORMMATRIX2X3FVNVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLUNIFORMMATRIX3X2FVNVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLUNIFORMMATRIX2X4FVNVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLUNIFORMMATRIX4X2FVNVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLUNIFORMMATRIX3X4FVNVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLUNIFORMMATRIX4X3FVNVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#endif


/* GL_EXT_robustness */

#ifndef GL_EXT_robustness
#define GL_EXT_robustness 1
#define GL_CONTEXT_ROBUST_ACCESS_EXT               0x90F3
#define GL_GUILTY_CONTEXT_RESET_EXT                0x8253
#define GL_INNOCENT_CONTEXT_RESET_EXT              0x8254
#define GL_UNKNOWN_CONTEXT_RESET_EXT               0x8255
#define GL_RESET_NOTIFICATION_STRATEGY_EXT         0x8256
#define GL_LOSE_CONTEXT_ON_RESET_EXT               0x8252
#define GL_NO_RESET_NOTIFICATION_EXT               0x8261
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GLenum GL_APIENTRY glGetGraphicsResetStatusEXT (void);
GL_APICALL void GL_APIENTRY glGetnUniformfvEXT (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
GL_APICALL void GL_APIENTRY glGetnUniformivEXT (GLuint program, GLint location, GLsizei bufSize, GLint *params);
GL_APICALL void GL_APIENTRY glReadnPixelsEXT (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
#endif
typedef GLenum (GL_APIENTRYP PFNGLGETGRAPHICSRESETSTATUSEXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMFVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat *params);
typedef void (GL_APIENTRYP PFNGLGETNUNIFORMIVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, GLint *params);
typedef void (GL_APIENTRYP PFNGLREADNPIXELSEXTPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
#endif /* GL_EXT_robustness */

#ifndef GL_OES_surfaceless_context
#define GL_FRAMEBUFFER_UNDEFINED_OES                            0x8219
#endif

/* GL_NV_draw_instanced */

#ifndef GL_NV_draw_instanced
#define GL_NV_draw_instanced 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDrawArraysInstancedNV (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
GL_APICALL void GL_APIENTRY glDrawElementsInstancedNV (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
#endif
typedef void (GL_APIENTRYP PFNGLDRAWARRAYSINSTANCEDNVPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void (GL_APIENTRYP PFNGLDRAWELEMENTSINSTANCEDNVPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
#endif

/* GL_NV_instanced_arrays */

#ifndef GL_NV_instanced_arrays
#define GL_NV_instanced_arrays 1
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR_NV  0x88FE
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glVertexAttribDivisorNV (GLuint index, GLuint divisor);
#endif
typedef void (GL_APIENTRYP PFNGLVERTEXATTRIBDIVISORNVPROC) (GLuint index, GLuint divisor);
#endif

/* GL_EXT_occlusion_query_boolean */

#ifndef GL_EXT_occlusion_query_boolean
#define GL_EXT_occlusion_query_boolean

#define       GL_ANY_SAMPLES_PASSED_EXT                         0x8C2F
#define       GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT            0x8D6A

#define       GL_CURRENT_QUERY_EXT                              0x8865

#define       GL_QUERY_RESULT_EXT                               0x8866
#define       GL_QUERY_RESULT_AVAILABLE_EXT                     0x8867

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGenQueriesEXT(GLsizei n, GLuint *ids);
GL_APICALL void GL_APIENTRY glDeleteQueriesEXT(GLsizei n, const GLuint *ids);
GL_APICALL GLboolean GL_APIENTRY glIsQueryEXT(GLuint id);
GL_APICALL void GL_APIENTRY glBeginQueryEXT(GLenum target, GLuint id);
GL_APICALL void GL_APIENTRY glEndQueryEXT(GLenum target);
GL_APICALL void GL_APIENTRY glGetQueryivEXT(GLenum target, GLenum pname, GLint *params);
GL_APICALL void GL_APIENTRY glGetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint *params);
#endif

typedef void (GL_APIENTRYP PFNGLGENQUERIESEXTPROC) (GLsizei n, GLuint *ids);
typedef void (GL_APIENTRYP PFNGLDELETEQUERIESEXTPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (GL_APIENTRYP PFNGLISQUERYEXTPROC) (GLuint id);
typedef void (GL_APIENTRYP PFNGLBEGINQUERYEXTPROC) (GLenum target, GLuint id);
typedef void (GL_APIENTRYP PFNGLENDQUERYEXTPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLGETQUERYIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUIVEXTPROC) (GLuint id, GLenum pname, GLuint *params);

#endif /* GL_EXT_occlusion_query_boolean */

/* GL_NV_pack_subimage */

#ifndef GL_NV_pack_subimage
#define GL_NV_pack_subimage 1
#define GL_PACK_ROW_LENGTH_NV                0x0D02
#define GL_PACK_SKIP_ROWS_NV                 0x0D03
#define GL_PACK_SKIP_PIXELS_NV               0x0D04
#endif

/* GL_EXT_unpack_subimage -- TODO remove once gl2ext.h is updated */
#define GL_UNPACK_ROW_LENGTH_EXT                                0x0CF2
#define GL_UNPACK_SKIP_ROWS_EXT                                 0x0CF3
#define GL_UNPACK_SKIP_PIXELS_EXT                               0x0CF4

/* GL_NV_timer_query */
#ifndef GL_NV_timer_query
#define GL_NV_timer_query 1
typedef khronos_int64_t GLint64NV;
typedef khronos_uint64_t GLuint64NV;
#define GL_TIME_ELAPSED_NV                                     0x88BF
#define GL_TIMESTAMP_NV                                        0x8E28
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glQueryCounterNV(GLuint id, GLenum target);
GL_APICALL void GL_APIENTRY glGetQueryObjectui64vNV(GLuint id, GLenum pname, GLuint64NV *params);
#endif
typedef void (GL_APIENTRYP PFNGLQUERYCOUNTERNVPROC) (GLuint id, GLenum target);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUI64VNVPROC) (GLuint id, GLenum pname, GLuint64NV *params);
#endif /* GL_NV_timer_query */

/* GL_NV_framebuffer_multisample */
#ifndef GL_NV_framebuffer_multisample
#define GL_NV_framebuffer_multisample 1
#define GL_RENDERBUFFER_SAMPLES_NV                              0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_NV                0x8D56
#define GL_MAX_SAMPLES_NV                                       0x8D57
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glRenderbufferStorageMultisampleNV (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
#endif
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
#endif // GL_NV_framebuffer_multisample

/* GL_NV_uniform_buffer_object_es2 */
#ifndef GL_NV_uniform_buffer_object_es2
#define GL_NV_uniform_buffer_object_es2
#define GL_UNIFORM_BUFFER_NV                                  0x8A11
#define GL_UNIFORM_BUFFER_BINDING_NV                          0x8A28
#define GL_UNIFORM_BUFFER_START_NV                            0x8A29
#define GL_UNIFORM_BUFFER_SIZE_NV                             0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS_NV                       0x8A2B
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS_NV                     0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS_NV                     0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS_NV                     0x8A2F
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS_NV          0x8A31
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS_NV        0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT_NV                 0x8A34
#define GL_MAX_UNIFORM_BLOCK_SIZE_NV                          0x8A30
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH_NV            0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS_NV                           0x8A36
#define GL_UNIFORM_TYPE_NV                                    0x8A37
#define GL_UNIFORM_SIZE_NV                                    0x8A38
#define GL_UNIFORM_NAME_LENGTH_NV                             0x8A39
#define GL_UNIFORM_BLOCK_INDEX_NV                             0x8A3A
#define GL_UNIFORM_OFFSET_NV                                  0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE_NV                            0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE_NV                           0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR_NV                            0x8A3E
#define GL_UNIFORM_BLOCK_BINDING_NV                           0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE_NV                         0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH_NV                       0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS_NV                   0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES_NV            0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER_NV       0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER_NV     0x8A46
#define GL_INVALID_INDEX_NV                                   0xFFFFFFFFu
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGetUniformIndicesNV(GLuint program, GLsizei uniformCount, const GLchar * const *uniformNames, GLuint *uniformIndices);
GL_APICALL void GL_APIENTRY glGetActiveUniformsivNV(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
GL_APICALL void GL_APIENTRY glGetActiveUniformNameNV(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
GL_APICALL GLuint GL_APIENTRY glGetUniformBlockIndexNV(GLuint program, const GLchar* uniformBlockName);
GL_APICALL void GL_APIENTRY glGetActiveUniformBlockivNV(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
GL_APICALL void GL_APIENTRY glGetActiveUniformBlockNameNV(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
GL_APICALL void GL_APIENTRY glBindBufferRangeNV(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
GL_APICALL void GL_APIENTRY glBindBufferBaseNV(GLenum target, GLuint index, GLuint buffer);
GL_APICALL void GL_APIENTRY glGetIntegeri_vNV(GLenum target, GLuint index, GLint* data);
GL_APICALL void GL_APIENTRY glGetInteger64i_vNV(GLenum target, GLuint index, GLint64NV* data);
GL_APICALL void GL_APIENTRY glGetInteger64vNV(GLenum target, GLint64NV *data);
GL_APICALL void GL_APIENTRY glUniformBlockBindingNV(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
#endif
typedef void (GL_APIENTRYP PFNGLGETUNIFORMINDICESNVPROC) (GLuint program, GLsizei uniformCount, const GLchar * const *uniformNames, GLuint *uniformIndices);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMSIVNVPROC) (GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMNAMENVPROC) (GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
typedef GLuint (GL_APIENTRYP PFNGLGETUNIFORMBLOCKINDEXNVPROC) (GLuint program, const GLchar* uniformBlockName);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMBLOCKIVNVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMBLOCKNAMENVPROC) (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
typedef void (GL_APIENTRYP PFNGLBINDBUFFERRANGENVPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GL_APIENTRYP PFNGLBINDBUFFERBASENVPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (GL_APIENTRYP PFNGLGETINTEGERI_VNVPROC) (GLenum target, GLuint index, GLint* data);
typedef void (GL_APIENTRYP PFNGLGETINTEGER64I_VNVPROC) (GLenum target, GLuint index, GLint64NV* data);
typedef void (GL_APIENTRYP PFNGLGETINTEGER64VNVPROC) (GLenum target, GLint64NV* data);
typedef void (GL_APIENTRYP PFNGLUNIFORMBLOCKBINDINGNVPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
#endif

#ifndef GL_NV_3dvision_settings
#define GL_NV_3dvision_settings 1
#define GL_3DVISION_STEREO_NV                 0x90F4
#define GL_STEREO_SEPARATION_NV               0x90F5
#define GL_STEREO_CONVERGENCE_NV              0x90F6
#define GL_STEREO_CUTOFF_NV                   0x90F7
#define GL_STEREO_PROJECTION_NV               0x90F8
#define GL_STEREO_PROJECTION_PERSPECTIVE_NV   0x90F9
#define GL_STEREO_PROJECTION_ORTHO_NV         0x90FA

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glStereoParameterfNV(GLenum pname, GLfloat params);
GL_APICALL void GL_APIENTRY glStereoParameteriNV(GLenum pname, GLint params);
#endif
typedef void (GL_APIENTRYP PFNGLSTEREOPARAMETERFNVPROC) (GLenum pname, GLfloat params);
typedef void (GL_APIENTRYP PFNGLSTEREOPARAMETERINVPROC) (GLenum pname, GLint params);
#endif

/* GL_EXT_separate_shader_objects */
#ifndef GL_EXT_separate_shader_objects
#define GL_EXT_separate_shader_objects 1
#define GL_VERTEX_SHADER_BIT_EXT                                0x00000001
#define GL_FRAGMENT_SHADER_BIT_EXT                              0x00000002
#define GL_ALL_SHADER_BITS_EXT                                  0xFFFFFFFF
#define GL_PROGRAM_SEPARABLE_EXT                                0x8258
#define GL_ACTIVE_PROGRAM_EXT                                   0x8259
#define GL_PROGRAM_PIPELINE_BINDING_EXT                         0x825A
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glUseProgramStagesEXT(GLuint pipeline, GLbitfield stages, GLuint program);
GL_APICALL void GL_APIENTRY glActiveShaderProgramEXT(GLuint pipeline, GLuint program);
GL_APICALL GLuint GL_APIENTRY glCreateShaderProgramvEXT(GLenum type, GLsizei count, const GLchar **strings);
GL_APICALL void GL_APIENTRY glBindProgramPipelineEXT(GLuint pipeline);
GL_APICALL void GL_APIENTRY glDeleteProgramPipelinesEXT(GLsizei n, const GLuint *pipelines);
GL_APICALL void GL_APIENTRY glGenProgramPipelinesEXT(GLsizei n, GLuint *pipelines);
GL_APICALL GLboolean GL_APIENTRY glIsProgramPipelineEXT(GLuint pipeline);
GL_APICALL void GL_APIENTRY glProgramParameteriEXT(GLuint program, GLenum pname, GLint value);
GL_APICALL void GL_APIENTRY glGetProgramPipelineivEXT(GLuint pipeline, GLenum pname, GLint *params);
GL_APICALL void GL_APIENTRY glProgramUniform1iEXT(GLuint program, GLint location, GLint x);
GL_APICALL void GL_APIENTRY glProgramUniform2iEXT(GLuint program, GLint location, GLint x, GLint y);
GL_APICALL void GL_APIENTRY glProgramUniform3iEXT(GLuint program, GLint location, GLint x, GLint y, GLint z);
GL_APICALL void GL_APIENTRY glProgramUniform4iEXT(GLuint program, GLint location, GLint x, GLint y, GLint z, GLint w);
GL_APICALL void GL_APIENTRY glProgramUniform1fEXT(GLuint program, GLint location, GLfloat x);
GL_APICALL void GL_APIENTRY glProgramUniform2fEXT(GLuint program, GLint location, GLfloat x, GLfloat y);
GL_APICALL void GL_APIENTRY glProgramUniform3fEXT(GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z);
GL_APICALL void GL_APIENTRY glProgramUniform4fEXT(GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GL_APICALL void GL_APIENTRY glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value);
GL_APICALL void GL_APIENTRY glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value);
GL_APICALL void GL_APIENTRY glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value);
GL_APICALL void GL_APIENTRY glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value);
GL_APICALL void GL_APIENTRY glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value);
GL_APICALL void GL_APIENTRY glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value);
GL_APICALL void GL_APIENTRY glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value);
GL_APICALL void GL_APIENTRY glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value);
GL_APICALL void GL_APIENTRY glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GL_APICALL void GL_APIENTRY glValidateProgramPipelineEXT(GLuint pipeline );
GL_APICALL void GL_APIENTRY glGetProgramPipelineInfoLogEXT(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
#endif
typedef void (GL_APIENTRYP PFNGLUSEPROGRAMSTAGESEXTPROC) (GLuint pipeline, GLbitfield stages, GLuint program);
typedef void (GL_APIENTRYP PFNGLACTIVESHADERPROGRAMEXTPROC) (GLuint pipeline, GLuint program);
typedef GLuint (GL_APIENTRYP PFNGLCREATESHADERPROGRAMVEXTPROC) (GLenum type, GLsizei count, const GLchar **strings);
typedef void (GL_APIENTRYP PFNGLBINDPROGRAMPIPELINEEXTPROC) (GLuint pipeline);
typedef void (GL_APIENTRYP PFNGLDELETEPROGRAMPIPELINESEXTPROC) (GLsizei n, const GLuint *pipelines);
typedef void (GL_APIENTRYP PFNGLGENPROGRAMPIPELINESEXTPROC) (GLsizei n, GLuint *pipelines);
typedef GLboolean (GL_APIENTRYP PFNGLISPROGRAMPIPELINEEXTPROC) (GLuint pipeline);
typedef void (GL_APIENTRYP PFNGLPROGRAMPARAMETERIEXTPROC) (GLuint program, GLenum pname, GLint value);
typedef void (GL_APIENTRYP PFNGLGETPROGRAMPIPELINEIVEXTPROC) (GLuint pipeline, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM1IEXTPROC) (GLuint program, GLint location, GLint x);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM2IEXTPROC) (GLuint program, GLint location, GLint x, GLint y);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM3IEXTPROC) (GLuint program, GLint location, GLint x, GLint y, GLint z);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM4IEXTPROC) (GLuint program, GLint location, GLint x, GLint y, GLint z, GLint w);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM1FEXTPROC) (GLuint program, GLint location, GLfloat x);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM2FEXTPROC) (GLuint program, GLint location, GLfloat x, GLfloat y);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM3FEXTPROC) (GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM4FEXTPROC) (GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM1IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM2IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM3IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM4IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM1FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM2FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM3FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORM4FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose,const float *value);
typedef void (GL_APIENTRYP PFNGLVALIDATEPROGRAMPIPELINEEXTPROC) (GLuint pipeline );
typedef void (GL_APIENTRYP PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC) (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
#endif /* GL_EXT_separate_shader_objects */

/* Debug markers. */
#ifndef GL_EXT_debug_marker
#define GL_EXT_debug_marker 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glInsertEventMarkerEXT(GLsizei length, const GLchar *marker);
GL_APICALL void GL_APIENTRY glPushGroupMarkerEXT(GLsizei length, const GLchar *marker);
GL_APICALL void GL_APIENTRY glPopGroupMarkerEXT(void);
#endif
typedef void (GL_APIENTRYP PFNGLINSERTEVENTMARKEREXTPROC) (GLsizei length, const GLchar *marker);
typedef void (GL_APIENTRYP PFNGLPUSHGROUPMARKEREXTPROC) (GLsizei length, const GLchar *marker);
typedef void (GL_APIENTRYP PFNGLPOPGROUPMARKEREXTPROC) (void);
#endif /* GL_EXT_debug_marker */

//TODO remove once glext.h has been updated
/* GL_EXT_sRGB */
#ifndef GL_EXT_sRGB
#define GL_EXT_sRGB 1
#define GL_SRGB_EXT                                             0x8C40
#define GL_SRGB_ALPHA_EXT                                       0x8C42
#define GL_SRGB8_ALPHA8_EXT                                     0x8C43
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT            0x8210
#endif

#ifndef GL_NV_sRGB_formats
#define GL_NV_sRGB_formats 1
#define GL_SRGB8_NV                             0x8C41
#define GL_SLUMINANCE_ALPHA_NV                  0x8C44
#define GL_SLUMINANCE8_ALPHA8_NV                0x8C45
#define GL_SLUMINANCE_NV                        0x8C46
#define GL_SLUMINANCE8_NV                       0x8C47
#define GL_COMPRESSED_SRGB_NV                   0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_NV             0x8C49
#define GL_COMPRESSED_SLUMINANCE_NV             0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_NV       0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_NV         0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV   0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV   0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV   0x8C4F
#define GL_ETC1_SRGB8_NV                        0x88EE
#endif

#ifndef GL_NV_texture_border_clamp
#define GL_NV_texture_border_clamp 1
#ifndef GL_TEXTURE_BORDER_COLOR_NV
#define GL_TEXTURE_BORDER_COLOR_NV        0x1004
#endif
#define GL_CLAMP_TO_BORDER_NV             0x812D
#endif



#ifndef GL_NV_shadow_samplers_array
#define GL_NV_shadow_samplers_array 1
#define GL_SAMPLER_2D_ARRAY_SHADOW_NV           0x8DC4
#endif

#ifndef GL_NV_shadow_samplers_cube
#define GL_NV_shadow_samplers_cube 1
#define GL_SAMPLER_CUBE_SHADOW_NV               0x8DC5
#endif


/* GL_EXT_debug_label */
#ifndef GL_EXT_debug_label
#define GL_EXT_debug_label 1
#define GL_BUFFER_OBJECT_EXT                      0x9151
#define GL_SHADER_OBJECT_EXT                      0x8B48
#define GL_PROGRAM_OBJECT_EXT                     0x8B40
#define GL_VERTEX_ARRAY_OBJECT_EXT                0x9154
#define GL_QUERY_OBJECT_EXT                       0x9153
#define GL_PROGRAM_PIPELINE_OBJECT_EXT            0x8A4F
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glLabelObjectEXT (GLenum type, GLuint object, GLsizei length, const char *label);
GL_APICALL void GL_APIENTRY glGetObjectLabelEXT (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, char *label);
#endif /* GL_GLEXT_PROTOTYPES */
typedef void (GL_APIENTRYP PFNGLLABELOBJECTEXTPROC) (GLenum type, GLuint object, GLsizei length, const char *label);
typedef void (GL_APIENTRYP PFNGLGETOBJECTLABELEXTPROC) (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, char *label);
#endif /* GL_EXT_debug_label */


/* GL_KHR_debug */
#ifndef GL_KHR_debug
#define GL_KHR_debug 1

#define GL_STACK_OVERFLOW_OES                                   0x0503
#define GL_STACK_UNDERFLOW_OES                                  0x0504

#define GL_DEBUG_OUTPUT_OES                                     0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS_OES                         0x8242

#define GL_CONTEXT_FLAG_DEBUG_BIT_OES                           0x00000002

#define GL_MAX_DEBUG_MESSAGE_LENGTH_OES                         0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_OES                        0x9144
#define GL_DEBUG_LOGGED_MESSAGES_OES                            0x9145
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_OES                 0x8243
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH_OES                      0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH_OES                          0x826D
#define GL_MAX_LABEL_LENGTH_OES                                 0x82E8

#define GL_DEBUG_CALLBACK_FUNCTION_OES                          0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM_OES                        0x8245

#define GL_DEBUG_SOURCE_API_OES                                 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_OES                       0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_OES                     0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_OES                         0x8249
#define GL_DEBUG_SOURCE_APPLICATION_OES                         0x824A
#define GL_DEBUG_SOURCE_OTHER_OES                               0x824B

#define GL_DEBUG_TYPE_ERROR_OES                                 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_OES                   0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_OES                    0x824E
#define GL_DEBUG_TYPE_PORTABILITY_OES                           0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_OES                           0x8250
#define GL_DEBUG_TYPE_OTHER_OES                                 0x8251
#define GL_DEBUG_TYPE_MARKER_OES                                0x8268

#define GL_DEBUG_TYPE_PUSH_GROUP_OES                            0x8269
#define GL_DEBUG_TYPE_POP_GROUP_OES                             0x826A

#define GL_DEBUG_SEVERITY_HIGH_OES                              0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_OES                            0x9147
#define GL_DEBUG_SEVERITY_LOW_OES                               0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION_OES                      0x826B

#define GL_STACK_UNDERFLOW_OES                                  0x0504
#define GL_STACK_OVERFLOW_OES                                   0x0503

#define GL_BUFFER_OES                                           0x82E0
#define GL_SHADER_OES                                           0x82E1
#define GL_PROGRAM_OES                                          0x82E2
#define GL_QUERY_OES                                            0x82E3
#define GL_PROGRAM_PIPELINE_OES                                 0x82E4
#define GL_SAMPLER_OES                                          0x82E6
typedef void (GL_APIENTRY *GLDEBUGPROCOES)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDebugMessageControlOES (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
GL_APICALL void GL_APIENTRY glDebugMessageInsertOES (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
GL_APICALL void GL_APIENTRY glDebugMessageCallbackOES (GLDEBUGPROCOES callback, GLvoid* userParam);
GL_APICALL GLuint GL_APIENTRY glGetDebugMessageLogOES (GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
GL_APICALL void GL_APIENTRY glGetPointervOES (GLenum pname, GLvoid** params);
GL_APICALL void GL_APIENTRY glPushDebugGroupOES (GLenum source, GLuint id, GLsizei length, const GLchar * message);
GL_APICALL void GL_APIENTRY glPopDebugGroupOES (void);
GL_APICALL void GL_APIENTRY glObjectLabelOES (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
GL_APICALL void GL_APIENTRY glGetObjectLabelOES (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
GL_APICALL void GL_APIENTRY glObjectPtrLabelOES (GLvoid* ptr, GLsizei length, const GLchar *label);
GL_APICALL void GL_APIENTRY glGetObjectPtrLabelOES (GLvoid* ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
#endif
typedef void (GL_APIENTRYP PFNGLDEBUGMESSAGECONTROLOESPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
typedef void (GL_APIENTRYP PFNGLDEBUGMESSAGEINSERTOESPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
typedef void (GL_APIENTRYP PFNGLDEBUGMESSAGECALLBACKOESPROC) (GLDEBUGPROCOES callback, GLvoid* userParam);
typedef GLuint (GL_APIENTRYP PFNGLGETDEBUGMESSAGELOGOESPROC) (GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
typedef void (GL_APIENTRYP PFNGLGETPOINTERVOESPROC) (GLenum pname, GLvoid** params);
typedef void (GL_APIENTRYP PFNGLPUSHDEBUGGROUPOESPROC) (GLenum source, GLuint id, GLsizei length, const GLchar * message);
typedef void (GL_APIENTRYP PFNGLPOPDEBUGGROUPOESPROC) (void);
typedef void (GL_APIENTRYP PFNGLOBJECTLABELOESPROC) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP PFNGLGETOBJECTLABELOESPROC)(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP PFNGLOBJECTPTRLABELOESPROC) (GLvoid* ptr, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP PFNGLGETOBJECTPTRLABELOESPROC) (GLvoid* ptr, GLsizei bufSize, GLsizei *length, GLchar *label);
#endif /* GL_KHR_debug */

#ifndef GL_NV_pixel_buffer_object
#define GL_NV_pixel_buffer_object 1
#define GL_PIXEL_PACK_BUFFER_NV                   0x88EB
#define GL_PIXEL_UNPACK_BUFFER_NV                 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_NV           0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_NV         0x88EF
#endif /* GL_NV_pixel_buffer_object */

#ifndef GL_NV_copy_buffer
#define GL_NV_copy_buffer 1
#define GL_COPY_READ_BUFFER_NV                     0x8F36
#define GL_COPY_WRITE_BUFFER_NV                    0x8F37
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glCopyBufferSubDataNV(GLenum readtarget, GLenum writetarget, GLintptr readoffset, GLintptr writeoffset, GLsizeiptr size);
#endif
typedef void (GL_APIENTRYP PFNGLCOPYBUFFERSUBDATANVPROC) (GLenum readtarget, GLenum writetarget, GLintptr readoffset, GLintptr writeoffset, GLsizeiptr size);
#endif //GL_NV_copy_buffer

/* GL_NV_framebuffer_blit */
#ifndef GL_NV_framebuffer_blit
#define GL_NV_framebuffer_blit 1
#define GL_READ_FRAMEBUFFER_NV                    0x8CA8
#define GL_DRAW_FRAMEBUFFER_NV                    0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING_NV            0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING_NV            0x8CAA
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glBlitFramebufferNV (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#endif
typedef void (GL_APIENTRYP PFNGLBLITFRAMEBUFFERNVPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#endif /* GL_NV_framebuffer_blit */

/* GL_EXT_map_buffer_range */
#ifndef GL_EXT_map_buffer_range
#define GL_EXT_map_buffer_range 1
#define GL_MAP_READ_BIT_EXT                  0x0001
#define GL_MAP_WRITE_BIT_EXT                 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT_EXT      0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT_EXT     0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT_EXT        0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT_EXT        0x0020
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void* GL_APIENTRY glMapBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GL_APICALL void GL_APIENTRY glFlushMappedBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length);
#endif
typedef void* (GL_APIENTRYP PFNGLMAPBUFFERRANGEEXTPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (GL_APIENTRYP PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
#endif

/* GL_EXT_texture_storage */
#ifndef GL_EXT_texture_storage
#define GL_EXT_texture_storage 1
#define GL_TEXTURE_IMMUTABLE_FORMAT_EXT            0x912F
#define GL_ALPHA8_EXT                              0x803C
#define GL_LUMINANCE8_EXT                          0x8040
#define GL_LUMINANCE8_ALPHA8_EXT                   0x8045
#define GL_RGBA32F_EXT                             0x8814
#define GL_RGB32F_EXT                              0x8815
#define GL_ALPHA32F_EXT                            0x8816
#define GL_LUMINANCE32F_EXT                        0x8818
#define GL_LUMINANCE_ALPHA32F_EXT                  0x8819
#define GL_RGBA16F_EXT                             0x881A
#define GL_RGB16F_EXT                              0x881B
#define GL_ALPHA16F_EXT                            0x881C
#define GL_LUMINANCE16F_EXT                        0x881E
#define GL_LUMINANCE_ALPHA16F_EXT                  0x881F
#define GL_RGB10_A2_EXT                            0x8059
#define GL_RGB10_EXT                               0x8052
#define GL_BGRA8_EXT                               0x93A1
#define GL_R8_EXT                                  0x8229
#define GL_RG8_EXT                                 0x822B
#define GL_R32F_EXT                                0x822E
#define GL_RG32F_EXT                               0x8230
#define GL_R16F_EXT                                0x822D
#define GL_RG16F_EXT                               0x822F
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTexStorage2DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
GL_APICALL void GL_APIENTRY glTexStorage3DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
#endif
typedef void (GL_APIENTRYP PFNGLTEXSTORAGE2DEXTPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLTEXSTORAGE3DEXTPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
#endif /* GL_EXT_texture_storage */

/* GL_NV_occlusion_query_samples */
#ifndef GL_NV_occlusion_query_samples
#define GL_NV_occlusion_query_samples 1
#define GL_SAMPLES_PASSED_NV                 0x8914
#endif
/* GL_OES_vertex_array_object */
#ifndef GL_OES_vertex_array_object
#define GL_OES_vertex_array_object 1
#define GL_VERTEX_ARRAY_BINDING_OES                    0x85B5
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glBindVertexArrayOES(GLuint array);
GL_APICALL void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n, const GLuint *arrays);
GL_APICALL void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint *arrays);
GL_APICALL GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint array);
#endif
typedef void (GL_APIENTRYP PFNGLBINDVERTEXARRAYOES) (GLuint array);
typedef void (GL_APIENTRYP PFNGLDELETEVERTEXARRAYSOES) (GLsizei n, const GLuint *arrays);
typedef void (GL_APIENTRYP PFNGLGENVERTEXARRAYSOES) (GLsizei n, GLuint *arrays);
typedef GLboolean (GL_APIENTRYP PFNGLISVERTEXARRAYOES) (GLuint array);
#endif /* GL_OES_vertex_array_object */

/* GL_EXT_color_buffer_half_float */
#ifndef GL_EXT_color_buffer_half_float
#define GL_RGBA16F_EXT                                          0x881A
#define GL_RGB16F_EXT                                           0x881B
#define GL_RG16F_EXT                                            0x822F
#define GL_R16F_EXT                                             0x822D
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT            0x8211
#define GL_UNSIGNED_NORMALIZED_EXT                              0x8C17
#endif

/* EXT_texture_rg */
#ifndef GL_EXT_texture_rg
#define GL_EXT_texture_rg 1
#define GL_RED_EXT                                              0x1903
#define GL_RG_EXT                                               0x8227
#define GL_R8_EXT                                               0x8229
#define GL_RG8_EXT                                              0x822B
#endif

/* NV_sample_mask */
#ifndef GL_NV_sample_mask
#define GL_NV_sample_mask 1
#define GL_SAMPLE_POSITION_NV                                   0x8E50
#define GL_SAMPLE_MASK_NV                                       0x8E51
#define GL_SAMPLE_MASK_VALUE_NV                                 0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS_NV                             0x8E59
#ifdef GL_GLEXT_PROTOTYPES
// glGetIntegeri_vNV defined by NV_uniform_buffer_object
GL_APICALL void GL_APIENTRY glGetMultisamplefvNV (GLenum pname, GLuint index, GLfloat *value);
GL_APICALL void GL_APIENTRY glSampleMaskiNV (GLuint index, GLbitfield mask);
#endif
// PFNGLGETINTEGERI_VNVPROC defined by NV_uniform_buffer_object
typedef void (GL_APIENTRYP PFNGLGETMULTISAMPLEFVNVPROC) (GLenum pname, GLuint index, GLfloat *value);
typedef void (GL_APIENTRYP PFNGLSAMPLEMASKINVPROC) (GLuint index, GLbitfield mask);
#endif

/* GL_NV_blend_equation_advanced */
#ifndef GL_NV_blend_equation_advanced
#define GL_NV_blend_equation_advanced 1
#define GL_BLEND_PREMULTIPLIED_SRC_NV    0x9280
#define GL_BLEND_OVERLAP_NV              0x9281
#define GL_UNCORRELATED_NV               0x9282
#define GL_DISJOINT_NV                   0x9283
#define GL_CONJOINT_NV                   0x9284
/* GL_ZERO already defined in ES 2.0 */
#define GL_SRC_NV                        0x9286
#define GL_DST_NV                        0x9287
#define GL_SRC_OVER_NV                   0x9288
#define GL_DST_OVER_NV                   0x9289
#define GL_SRC_IN_NV                     0x928A
#define GL_DST_IN_NV                     0x928B
#define GL_SRC_OUT_NV                    0x928C
#define GL_DST_OUT_NV                    0x928D
#define GL_SRC_ATOP_NV                   0x928E
#define GL_DST_ATOP_NV                   0x928F
#define GL_XOR_NV                        0x1506
#define GL_MULTIPLY_NV                   0x9294
#define GL_SCREEN_NV                     0x9295
#define GL_OVERLAY_NV                    0x9296
#define GL_DARKEN_NV                     0x9297
#define GL_LIGHTEN_NV                    0x9298
#define GL_COLORDODGE_NV                 0x9299
#define GL_COLORBURN_NV                  0x929A
#define GL_HARDLIGHT_NV                  0x929B
#define GL_SOFTLIGHT_NV                  0x929C
#define GL_DIFFERENCE_NV                 0x929E
#define GL_EXCLUSION_NV                  0x92A0
/* GL_INVERT already defined in ES 2.0 */
#define GL_INVERT_RGB_NV                 0x92A3
#define GL_LINEARDODGE_NV                0x92A4
#define GL_LINEARBURN_NV                 0x92A5
#define GL_VIVIDLIGHT_NV                 0x92A6
#define GL_LINEARLIGHT_NV                0x92A7
#define GL_PINLIGHT_NV                   0x92A8
#define GL_HARDMIX_NV                    0x92A9
#define GL_HSL_HUE_NV                    0x92AD
#define GL_HSL_SATURATION_NV             0x92AE
#define GL_HSL_COLOR_NV                  0x92AF
#define GL_HSL_LUMINOSITY_NV             0x92B0
#define GL_PLUS_NV                       0x9291
#define GL_PLUS_CLAMPED_NV               0x92B1
#define GL_PLUS_CLAMPED_ALPHA_NV         0x92B2
#define GL_PLUS_DARKER_NV                0x9292
#define GL_MINUS_NV                      0x929F
#define GL_MINUS_CLAMPED_NV              0x92B3
#define GL_CONTRAST_NV                   0x92A1
#define GL_INVERT_OVG_NV                 0x92B4
#define GL_RED_NV                        0x1903
#define GL_GREEN_NV                      0x1904
#define GL_BLUE_NV                       0x1905
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glBlendParameteriNV(GLenum pname, GLint value);
GL_APICALL void GL_APIENTRY glBlendBarrierNV(void);
#endif
typedef void (GL_APIENTRYP PFNGLBLENDPARAMETERINVPROC) (GLenum pname, GLint value);
typedef void (GL_APIENTRYP PFNGLBLENDBARRIERNVPROC) (void);
#endif

/* GL_NV_blend_equation_advanced_coherent */
#ifndef GL_NV_blend_equation_advanced_coherent
#define GL_NV_blend_equation_advanced_coherent 1
#define GL_BLEND_ADVANCED_COHERENT_NV    0x9285
#endif

//
// Deprecated extensions. Do not use.
//

/* GL_NV_read_buffer */

#ifndef GL_NV_read_buffer
#define GL_NV_read_buffer 1
#define GL_READ_BUFFER_NV                          0x0C02
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_NV   0x8CDC
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glReadBufferNV (GLenum mode);
#endif
typedef void (GL_APIENTRYP PFNGLREADBUFFERNVPROC) (GLenum mode);
#endif

/* GL_NV_fbo_color_attachments */

#ifndef GL_NV_fbo_color_attachments
#define GL_NV_fbo_color_attachments 1
#define GL_MAX_COLOR_ATTACHMENTS_NV                     0x8CDF
#define GL_COLOR_ATTACHMENT0_NV                         0x8CE0
#define GL_COLOR_ATTACHMENT1_NV                         0x8CE1
#define GL_COLOR_ATTACHMENT2_NV                         0x8CE2
#define GL_COLOR_ATTACHMENT3_NV                         0x8CE3
#define GL_COLOR_ATTACHMENT4_NV                         0x8CE4
#define GL_COLOR_ATTACHMENT5_NV                         0x8CE5
#define GL_COLOR_ATTACHMENT6_NV                         0x8CE6
#define GL_COLOR_ATTACHMENT7_NV                         0x8CE7
#define GL_COLOR_ATTACHMENT8_NV                         0x8CE8
#define GL_COLOR_ATTACHMENT9_NV                         0x8CE9
#define GL_COLOR_ATTACHMENT10_NV                        0x8CEA
#define GL_COLOR_ATTACHMENT11_NV                        0x8CEB
#define GL_COLOR_ATTACHMENT12_NV                        0x8CEC
#define GL_COLOR_ATTACHMENT13_NV                        0x8CED
#define GL_COLOR_ATTACHMENT14_NV                        0x8CEE
#define GL_COLOR_ATTACHMENT15_NV                        0x8CEF
#endif

/* GL_EXT_texture_compression_dxt1 - use GL_NV_texture_compression_s3tc */

#ifndef GL_EXT_texture_compression_dxt1
#define GL_EXT_texture_compression_dxt1 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#endif

#ifndef GL_EXT_texture_sRGB_decode
#define GL_EXT_texture_sRGB_decode 1
#define GL_TEXTURE_SRGB_DECODE_EXT        0x8A48
#define GL_DECODE_EXT                     0x8A49
#define GL_SKIP_DECODE_EXT                0x8A4A
#endif

#ifndef GL_NV_framebuffer_sRGB
#define GL_NV_framebuffer_sRGB 1
#define GL_FRAMEBUFFER_SRGB_NV            0x8DB9
#endif

#ifndef GL_NV_sampler_objects
#define GL_NV_sampler_objects 1
#define GL_SAMPLER_BINDING_NV             0x8919
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGenSamplersNV (GLsizei count, GLuint *samplers);
GL_APICALL void GL_APIENTRY glDeleteSamplersNV (GLsizei count, const GLuint *samplers);
GL_APICALL GLboolean GL_APIENTRY glIsSamplerNV (GLuint sampler);
GL_APICALL void GL_APIENTRY glBindSamplerNV (GLuint unit, GLuint sampler);
GL_APICALL void GL_APIENTRY glSamplerParameteriNV (GLuint sampler, GLenum pname, GLint param);
GL_APICALL void GL_APIENTRY glSamplerParameterfNV (GLuint sampler, GLenum pname, GLfloat param);
GL_APICALL void GL_APIENTRY glSamplerParameterivNV (GLuint sampler, GLenum pname, const GLint *params);
GL_APICALL void GL_APIENTRY glSamplerParameterfvNV (GLuint sampler, GLenum pname, const GLfloat *params);
GL_APICALL void GL_APIENTRY glSamplerParameterIivNV (GLuint sampler, GLenum pname, const GLint *params);
GL_APICALL void GL_APIENTRY glSamplerParameterIuivNV (GLuint sampler, GLenum pname, const GLint *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterivNV (GLuint sampler, GLenum pname, GLint *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterfvNV (GLuint sampler, GLenum pname, GLfloat *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterIivNV (GLuint sampler, GLenum pname, GLint *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterIuivNV (GLuint sampler, GLenum pname, GLuint *params);
#endif
typedef void (GL_APIENTRYP PFNGLGENSAMPLERSNVPROC) (GLsizei count, GLuint *samplers);
typedef void (GL_APIENTRYP PFNGLDELETESAMPLERSNVPROC) (GLsizei count, const GLuint *samplers);
typedef GLboolean (GL_APIENTRYP PFNGLISSAMPLERNVPROC) (GLuint sampler);
typedef void (GL_APIENTRYP PFNGLBINDSAMPLERNVPROC) (GLuint unit, GLuint sampler);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERINVPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERFNVPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERIVNVPROC) (GLuint sampler, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERIIVNVPROC) (GLuint sampler, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERIUIVNVPROC) (GLuint sampler, GLenum pname, const GLuint *params);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERFVNVPROC) (GLuint sampler, GLenum pname, const GLfloat *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERIVNVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERFVNVPROC) (GLuint sampler, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERIIVNVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERIUIVNVPROC) (GLuint sampler, GLenum pname, GLuint *params);
#endif

//
// Disabled extensions. Do not use.
//

/* GL_ARB_half_float_pixel */

#ifndef GL_ARB_half_float_pixel
#define GL_ARB_half_float_pixel 1
#define GL_HALF_FLOAT_ARB                          0x140B
#endif

/* GL_ARB_texture_rectangle */

#ifndef GL_ARB_texture_rectangle
#define GL_ARB_texture_rectangle 1
#define GL_TEXTURE_RECTANGLE_ARB                0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_ARB        0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_ARB          0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB       0x84F8
#define GL_SAMPLER_2D_RECT_ARB                  0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB           0x8B64
#endif

/* GL_EXT_texture_lod_bias */

#ifndef GL_EXT_texture_lod_bias
#define GL_EXT_texture_lod_bias 1
#define GL_MAX_TEXTURE_LOD_BIAS_EXT       0x84FD
#define GL_TEXTURE_FILTER_CONTROL_EXT     0x8500
#define GL_TEXTURE_LOD_BIAS_EXT           0x8501
#endif

/* GL_SGIS_texture_lod */

#ifndef GL_SGIS_texture_lod
#define GL_SGIS_texture_lod 1
#define GL_TEXTURE_MIN_LOD_SGIS           0x813A
#define GL_TEXTURE_MAX_LOD_SGIS           0x813B
#define GL_TEXTURE_BASE_LEVEL_SGIS        0x813C
#define GL_TEXTURE_MAX_LEVEL_SGIS         0x813D
#endif

/* GL_NV_secure_context */
#ifndef GL_NV_secure_context
#define GL_NV_secure_context 1
#endif

#ifdef __cplusplus
}
#endif

#endif /* __gl2ext_nv_h_ */



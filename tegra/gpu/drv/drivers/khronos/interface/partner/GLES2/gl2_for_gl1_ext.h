/*
 * Copyright (c) 2005-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __gl2_for_gl1_ext_h_
#define __gl2_for_gl1_ext_h_

#ifndef __gl2_h_
#   include <GLES2/gl2.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.0 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
**
** http://oss.sgi.com/projects/FreeB
**
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
**
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
**
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
*/

#ifndef GL_APIENTRYP
#   define GL_APIENTRYP GL_APIENTRY*
#endif

/*------------------------------------------------------------------------*
 * NV extension tokens
 *------------------------------------------------------------------------*/

/* GL_NV_clip_coords */
#ifndef GL_NV_clip_coords
/* GetPName */
#define GL_MAX_CLIP_PLANES_NV             0x0D32
/* ClipPlaneName */
#define GL_CLIP_PLANE0_NV                 0x3000
#define GL_CLIP_PLANE1_NV                 0x3001
#define GL_CLIP_PLANE2_NV                 0x3002
#define GL_CLIP_PLANE3_NV                 0x3003
#define GL_CLIP_PLANE4_NV                 0x3004
#define GL_CLIP_PLANE5_NV                 0x3005
#endif

/* GL_NV_logic_op */
#ifndef GL_NV_logic_op
#define GL_COLOR_LOGIC_OP_NV              0x0BF2
#define GL_LOGIC_OP_MODE_NV               0x0BF0
/* LogicOp */
#define GL_CLEAR_NV                       0x1500
#define GL_AND_NV                         0x1501
#define GL_AND_REVERSE_NV                 0x1502
#define GL_COPY_NV                        0x1503
#define GL_AND_INVERTED_NV                0x1504
#define GL_NOOP_NV                        0x1505
#define GL_XOR_NV                         0x1506
#define GL_OR_NV                          0x1507
#define GL_NOR_NV                         0x1508
#define GL_EQUIV_NV                       0x1509
#define GL_INVERT_NV                      0x150A
#define GL_OR_REVERSE_NV                  0x150B
#define GL_COPY_INVERTED_NV               0x150C
#define GL_OR_INVERTED_NV                 0x150D
#define GL_NAND_NV                        0x150E
#define GL_SET_NV                         0x150F
#endif

/* GL_NV_pop_clipping */
#ifndef GL_NV_pop_clipping
// TODO: determine if need to allocate a value for this.  For now, we share
//       the GL_COMBINER_PROGRAM_NV value
#define GL_POP_CLIPPING_NV                0x890A
#endif


/*------------------------------------------------------------------------*
 * NV extension functions
 *------------------------------------------------------------------------*/

/* GL_NV_clip_coords */
#ifndef GL_NV_clip_coords
#define GL_NV_clip_coords 1
#endif

/* GL_NV_flat_shading */
#ifndef GL_NV_flat_shading
#define GL_NV_flat_shading 1
#endif

/* GL_NV_logic_op */
#ifndef GL_NV_logic_op
#define GL_NV_logic_op 1
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glLogicOpNV (GLenum opcode);
#endif
typedef void (GL_APIENTRYP PFNGLLOGICOPNVPROC) (GLenum opcode);
#endif

/* GL_NV_pop_clipping */
#ifndef GL_NV_pop_clipping
#define GL_NV_pop_clipping 1
#endif

/* GL_NV_smooth_points_lines */
#ifndef GL_NV_smooth_points_lines
#define GL_NV_smooth_points_lines 1
#define GL_POINT_SMOOTH_NV                0x0B10
#define GL_LINE_SMOOTH_NV                 0x0B20
#endif

#ifdef __cplusplus
}
#endif

#endif /* __gl2_for_gl1_ext_h_ */

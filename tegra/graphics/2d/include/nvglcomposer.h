/*
 * Copyright (c) 2012 - 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _NV_GLCOMPOSER_H
#define _NV_GLCOMPOSER_H

typedef enum {
    GLC_DRAW_ARRAYS,
    GLC_DRAW_TEXTURE
} glc_draw_method_t;

typedef struct glc_context glc_context_t;

glc_context_t *
glc_init(void);

struct nvcomposer *
glc_get(glc_context_t *glc,
        glc_draw_method_t method);

void
glc_destroy(glc_context_t *glc);

#endif // #ifndef _NV_GLCOMPOSER_H

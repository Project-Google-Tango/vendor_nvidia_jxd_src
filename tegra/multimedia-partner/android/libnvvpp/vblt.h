/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _VBLT_H
#define _VBLT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hardware/hwcomposer.h>

typedef struct VBLT vblt_t;

vblt_t* vblt_create();
void vblt_destroy(vblt_t*);
void vblt_blit(vblt_t* vblt,
               NvMMBuffer* src,
               NvMMBuffer* dst,
               const hwc_rect_t *sourceCrop,
               int transform,
               NvBool isProtected);

#ifdef __cplusplus
}
#endif

#endif // _VBLT_H

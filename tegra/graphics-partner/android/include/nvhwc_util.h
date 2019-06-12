/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVHWC_UTIL_H
#define NVHWC_UTIL_H

#include "nvproperty_util.h"

#define r_width(r) ((r)->right - (r)->left)
#define r_height(r) ((r)->bottom - (r)->top)

#define r_copy(dst, src) do {                                 \
    (dst)->left   = (src)->left;                              \
    (dst)->top    = (src)->top;                               \
    (dst)->right  = (src)->right;                             \
    (dst)->bottom = (src)->bottom;                            \
} while (0)

#define r_equal(r1, r2)                                       \
    (((r1)->left   == (r2)->left)     &&                      \
     ((r1)->top    == (r2)->top)      &&                      \
     ((r1)->right  == (r2)->right)    &&                      \
     ((r1)->bottom == (r2)->bottom))                          \

#define r_empty(r)                                            \
    (((r)->left   >= (r)->right)      ||                      \
     ((r)->top    >= (r)->bottom))                            \

#define r_intersect(r1, r2)                                   \
    (!(((r2)->left   >= (r1)->right)  ||                      \
      ((r2)->right  <= (r1)->left)    ||                      \
      ((r2)->top    >= (r1)->bottom)  ||                      \
      ((r2)->bottom <= (r1)->top)))                           \

/* we define intersection of non-intersecting rectangles to be
 * degenerate rectangle at (-1,-1) */
#define r_intersection(r, r0, r1) do {                        \
    if (!r_intersect((r0), (r1))) {                           \
        (r)->left = (r)->top = (r)->right = (r)->bottom = -1; \
    } else {                                                  \
        (r)->left = NV_MAX((r0)->left, (r1)->left);           \
        (r)->top = NV_MAX((r0)->top, (r1)->top);              \
        (r)->right = NV_MIN((r0)->right, (r1)->right);        \
        (r)->bottom = NV_MIN((r0)->bottom, (r1)->bottom);     \
    }                                                         \
} while (0)

#endif /* NVHWC_UTIL_H */

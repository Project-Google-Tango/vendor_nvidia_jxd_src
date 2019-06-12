/*
 * Copyright (c) 2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
*/

#ifndef __FOCUSER_COMMON_H__
#define __FOCUSER_COMMON_H__

#include "nvcommon.h"

#ifndef INT_MAX
#define INT_MAX    0x7fffffff
#endif

#define AF_POS_INVALID_VALUE INT_MAX

#define NV_FOCUSER_SET_MAX              10
#define NV_FOCUSER_SET_DISTANCE_PAIR    16

struct nv_focuser_set_dist_pairs {
    NvS32 fdn;
    NvS32 distance;
}  __attribute__((packed));

struct nv_focuser_set {
    NvS32 posture;
    NvS32 macro;
    NvS32 hyper;
    NvS32 inf;
    NvS32 hysteresis;
    NvU32 settle_time;
    NvS32 macro_offset;
    NvS32 inf_offset;
    NvU32 num_dist_pairs;
    struct nv_focuser_set_dist_pairs dist_pair[NV_FOCUSER_SET_DISTANCE_PAIR];
} __attribute__((packed));

struct nv_focuser_config {
    NvU32 focal_length;
    NvU32 fnumber;
    NvU32 max_aperture;
    NvU32 range_ends_reversed;
    NvS32 pos_working_low;
    NvS32 pos_working_high;
    NvS32 pos_actual_low;
    NvS32 pos_actual_high;
    NvU32 slew_rate;
    NvU32 circle_of_confusion;
    NvU32 num_focuser_sets;
    struct nv_focuser_set focuser_set[NV_FOCUSER_SET_MAX];
} __attribute__((packed));

#endif

/**
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVC_FOCUS_H__
#define __NVC_FOCUS_H__

enum nvc_focus_sts {
    NVC_FOCUS_STS_UNKNOWN           = 1,
    NVC_FOCUS_STS_NO_DEVICE,
    NVC_FOCUS_STS_INITIALIZING,
    NVC_FOCUS_STS_INIT_ERR,
    NVC_FOCUS_STS_WAIT_FOR_MOVE_END,
    NVC_FOCUS_STS_WAIT_FOR_SETTLE,
    NVC_FOCUS_STS_LENS_SETTLED,
    NVC_FOCUS_STS_FORCE32           = 0x7FFFFFFF
};

struct nvc_focus_nvc {
    __u32 focal_length;
    __u32 fnumber;
    __u32 max_aperature;
} __attribute__((packed));

struct nvc_focus_cap {
    __u32 version;
    __u32 actuator_range;
    __u32 settle_time;
    __u32 focus_macro;
    __u32 focus_hyper;
    __u32 focus_infinity;
} __attribute__((packed));

#endif /* __NVC_FOCUS_H__ */


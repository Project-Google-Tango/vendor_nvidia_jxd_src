/**
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVC_TORCH_H__
#define __NVC_TORCH_H__

struct nvc_torch_level_info {
    __s32 guidenum;
    __u32 sustaintime;
    __s32 rechargefactor;
} __attribute__((packed));

struct nvc_torch_pin_state {
    __u16 mask;
    __u16 values;
} __attribute__((packed));

struct nvc_torch_flash_capabilities {
    __u32 numberoflevels;
    struct nvc_torch_level_info levels[];
} __attribute__((packed));

struct nvc_torch_torch_capabilities {
    __u32 numberoflevels;
    __s32 guidenum[];
} __attribute__((packed));

/* advanced flash/torch capability settings */
/* use version number to distinguish between different capability structures */
#define NVC_TORCH_LED_ATTR_FLASH_SYNC	1
#define NVC_TORCH_LED_ATTR_IND_FTIMER	(1 << 1)
#define NVC_TORCH_LED_ATTR_TORCH_SYNC	(1 << 16)
#define NVC_TORCH_LED_ATTR_IND_TTIMER	(1 << 17)

struct nvc_torch_capability_query {
	__u8 version;
	__u8 flash_num;		/* number of flashes supported by this device */
	__u8 torch_num;		/* number of torches supported by this device */
	__u8 reserved;
	__u32 led_attr;
};

#define NVC_TORCH_CAPABILITY_LEGACY	0
#define NVC_TORCH_CAPABILITY_VER_1	1

struct nvc_torch_set_level_v1 {
	__u16 ledmask;
	__u16 timeout;
	/* flash/torch levels mapped to ledmask for lsb to msb respectively */
	__u16 levels[2];
};

struct nvc_torch_lumi_level_v1 {
	__u32 guidenum;
	__u32 luminance;
	__u32 reserved;
};

struct nvc_torch_timeout_v1 {
	__u32 timeout;
	__u32 reserved1;
};

struct nvc_torch_timer_capabilities_v1 {
	__u32 timeout_num;
	/* time out durations in uS */
	struct nvc_torch_timeout_v1 timeouts[];
};

struct nvc_torch_flash_capabilities_v1 {
	__u8 version;		/* fixed number 1 */
	__u8 led_idx;
	__u8 reserved1;
	__u8 reserved2;
	__u32 attribute;
	__u16 granularity;	/* 1, 10, 100, ... to carry float settings */
	__u16 reserved4;
	__u32 timeout_num;
	__u32 timeout_off;
	__u32 numberoflevels;
	struct nvc_torch_lumi_level_v1 levels[];
};

struct nvc_torch_torch_capabilities_v1 {
	__u8 version;		/* fixed number 1 */
	__u8 led_idx;
	__u8 reserved1;
	__u8 reserved2;
	__u32 attribute;
	__u16 granularity;	/* 1, 10, 100, ... to carry float settings */
	__u16 reserved4;
	__u32 timeout_num;
	__u32 timeout_off;
	__u32 numberoflevels;
	struct nvc_torch_lumi_level_v1 levels[];
};

#endif /* __NVC_TORCH_H__ */


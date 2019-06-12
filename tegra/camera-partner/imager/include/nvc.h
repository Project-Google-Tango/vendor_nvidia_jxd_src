/*
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVC_H__
#define __NVC_H__

#if (BUILD_FOR_AOS == 0)
#include <linux/ioctl.h>
#endif

#include "nvodm_imager.h"

#define NVC_INT2FLOAT_DIVISOR_1K        1000
#define NVC_INT2FLOAT_DIVISOR_1M        1000000
#define NVC_INT2FLOAT_DIVISOR           1000

#define NVC_INT2FLOAT_DIVISOR_1K        1000
#define NVC_INT2FLOAT_DIVISOR_1M        1000000
#define NVC_INT2FLOAT_DIVISOR           1000

struct nvc_param {
    NvU32 param;
    NvU32 sizeofvalue;
    NvU32 variant;
    NvU32 p_value;
} __attribute__((packed));

enum nvc_params {
    NVC_PARAM_EXPOSURE = 0,
    NVC_PARAM_GAIN,
    NVC_PARAM_FRAMERATE,
    NVC_PARAM_MAX_FRAMERATE,
    NVC_PARAM_INPUT_CLOCK,
    NVC_PARAM_LOCUS,
    NVC_PARAM_FLASH_CAPS,
    NVC_PARAM_FLASH_LEVEL,
    NVC_PARAM_FLASH_PIN_STATE,
    NVC_PARAM_TORCH_CAPS,
    NVC_PARAM_TORCH_LEVEL,
    NVC_PARAM_FOCAL_LEN,
    NVC_PARAM_MAX_APERTURE,
    NVC_PARAM_FNUMBER,
    NVC_PARAM_EXPOSURE_LIMITS,
    NVC_PARAM_GAIN_LIMITS,
    NVC_PARAM_FRAMERATE_LIMITS,
    NVC_PARAM_FRAME_RATES,
    NVC_PARAM_CLOCK_LIMITS,
    NVC_PARAM_EXP_LATCH_TIME,
    NVC_PARAM_REGION_USED,
    NVC_PARAM_CALIBRATION_DATA,
    NVC_PARAM_CALIBRATION_OVERRIDES,
    NVC_PARAM_SELF_TEST,
    NVC_PARAM_STS,
    NVC_PARAM_TESTMODE,
    NVC_PARAM_EXPECTED_VALUES,
    NVC_PARAM_RESET,
    NVC_PARAM_OPTIMIZE_RES,
    NVC_PARAM_DETECT_COLOR_TEMP,
    NVC_PARAM_LINES_PER_SEC,
    NVC_PARAM_CAPS,
    NVC_PARAM_CUSTOM_BLOCK_INFO,
    NVC_PARAM_STEREO_CAP,
    NVC_PARAM_FOCUS_STEREO,
    NVC_PARAM_STEREO,
    NVC_PARAM_INHERENT_GAIN,
    NVC_PARAM_VIEW_ANGLE_H,
    NVC_PARAM_VIEW_ANGLE_V,
    NVC_PARAM_ISP_SETTING,
    NVC_PARAM_OPERATION_MODE,
    NVC_PARAM_SUPPORT_ISP,
    NVC_PARAM_AWB_LOCK,
    NVC_PARAM_AE_LOCK,
    NVC_PARAM_RES_CHANGE_WAIT_TIME,
    NVC_PARAM_FACTORY_CALIBRATION_DATA,
    NVC_PARAM_DEV_ID,
    NVC_PARAM_GROUP_HOLD,
    NVC_PARAM_SET_SENSOR_FLASH_MODE,
    NVC_PARAM_TORCH_QUERY,
    NVC_PARAM_FLASH_EXT_CAPS,
    NVC_PARAM_TORCH_EXT_CAPS,
    NVC_PARAM_BEGIN_VENDOR_EXTENSIONS = 0x10000000,
    NVC_PARAM_CALIBRATION_STATUS,
    NVC_PARAM_TEST_PATTERN,
    NVC_PARAM_MODULE_INFO,
    NVC_PARAM_FLASH_MAX_POWER,
    NVC_PARAM_DIRECTION,
    NVC_PARAM_SENSOR_TYPE,
    NVC_PARAM_DLI_CHECK,
    NVC_PARAM_PARALLEL_DLI_CHECK,
    NVC_PARAM_BRACKET_CAPS,
    NVC_PARAM_NUM,
    NVC_PARAM_I2C,
    NVC_PARAM_FORCE32 = 0x7FFFFFFF
};

/* sync off */
#define NVC_SYNC_OFF                    0
/* use only this device (the one receiving the call) */
#define NVC_SYNC_MASTER                 1
/* use only the synced device (the "other" device) */
#define NVC_SYNC_SLAVE                  2
/* use both synced devices at the same time */
#define NVC_SYNC_STEREO                 3

#define NVC_RESET_HARD                  0
#define NVC_RESET_SOFT                  1

struct nvc_param_isp {
    int attr;
    void *p_data;
    NvU32 data_size;
} __attribute__((packed));

struct nvc_isp_focus_param {
    NvS32 min_pos;
    NvS32 max_pos;
    NvS32 hyperfocal;
    NvS32 macro;
    NvS32 powersave;
} __attribute__((packed));

struct nvc_isp_focus_pos {
    NvU32 is_auto;
    NvS32 value;
} __attribute__((packed));

struct nvc_isp_focus_region {
    NvU32 num_region;
    NvS32 value;
} __attribute__((packed));

struct nvc_fuseid {
    NvU32 size;
    NvU8 data[16];
};

enum nvc_params_isp {
    NVC_PARAM_ISP_FOCUS_CAF = 16389,
    NVC_PARAM_ISP_FOCUS_CAF_PAUSE,
    NVC_PARAM_ISP_FOCUS_CAF_STS,
    NVC_PARAM_ISP_FOCUS_POS = 16407,
    NVC_PARAM_ISP_FOCUS_RANGE,
    NVC_PARAM_ISP_FOCUS_AF_RGN = 16413,
    NVC_PARAM_ISP_FOCUS_AF_RGN_MASK,
    NVC_PARAM_ISP_FOCUS_AF_RGN_STS,
    NVC_PARAM_ISP_FOCUS_CTRL = 16424,
    NVC_PARAM_ISP_FOCUS_TRGR,
    NVC_PARAM_ISP_FOCUS_STS,
};

#define NVC_PARAM_ISP_FOCUS_STS_BUSY    0
#define NVC_PARAM_ISP_FOCUS_STS_LOCKD   1
#define NVC_PARAM_ISP_FOCUS_STS_FAILD   2
#define NVC_PARAM_ISP_FOCUS_STS_ERR     3

#define NVC_PARAM_ISP_FOCUS_CTRL_ON     0
#define NVC_PARAM_ISP_FOCUS_CTRL_OFF    1
#define NVC_PARAM_ISP_FOCUS_CTRL_AUTO   2
#define NVC_PARAM_ISP_FOCUS_CTRL_ALOCK  3

#define NVC_PARAM_ISP_FOCUS_CAF_CONVRG  1
#define NVC_PARAM_ISP_FOCUS_CAF_SEARCH  2

#define NVC_PARAM_ISP_FOCUS_POS_INF     0

#define NVC_IOCTL_PWR_WR                _IOW('o', 102, int)
#define NVC_IOCTL_PWR_RD                _IOW('o', 103, int)
#define NVC_IOCTL_PARAM_WR              _IOW('o', 104, struct nvc_param)
#define NVC_IOCTL_PARAM_RD              _IOWR('o', 105, struct nvc_param)
#define NVC_IOCTL_PARAM_ISP_RD          _IOWR('o', 200, struct nvc_param_isp)
#define NVC_IOCTL_PARAM_ISP_WR          _IOWR('o', 201, struct nvc_param_isp)
#define NVC_IOCTL_FUSE_ID               _IOWR('o', 202, struct nvc_fuseid)
#define NVC_IOCTL_GET_EEPROM_DATA       _IOWR('o', 255, __u8 *)

#endif /* __NVC_H__ */


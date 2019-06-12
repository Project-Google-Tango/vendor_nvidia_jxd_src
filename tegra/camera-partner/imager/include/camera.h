/**
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "nvc.h"

#define PCLLK_IOCTL_CHIP_REG    _IOW('o', 100, struct virtual_device)
#define PCLLK_IOCTL_DEV_REG     _IOW('o', 104, struct camera_device_info)
#define PCLLK_IOCTL_DEV_DEL     _IOW('o', 105, int)
#define PCLLK_IOCTL_DEV_FREE    _IOW('o', 106, int)
#define PCLLK_IOCTL_PWR_WR      _IOW('o', 108, int)
#define PCLLK_IOCTL_PWR_RD      _IOR('o', 109, int)
#define PCLLK_IOCTL_SEQ_WR      _IOWR('o', 112, struct nvc_param)
#define PCLLK_IOCTL_SEQ_RD      _IOWR('o', 113, struct nvc_param)
#define PCLLK_IOCTL_UPDATE      _IOW('o', 116, struct nvc_param)
#define PCLLK_IOCTL_LAYOUT_WR   _IOW('o', 120, struct nvc_param)
#define PCLLK_IOCTL_LAYOUT_RD   _IOWR('o', 121, struct nvc_param)
#define PCLLK_IOCTL_PARAM_WR    _IOWR('o', 140, struct nvc_param)
#define PCLLK_IOCTL_PARAM_RD    _IOWR('o', 141, struct nvc_param)
#define PCLLK_IOCTL_DRV_ADD     _IOW('o', 150, struct nvc_param)

/* Expected higher level power calls are:
 * 1 = OFF
 * 2 = STANDBY
 * 3 = ON
 * These will be multiplied by 2 before given to the driver's PM code that
 * uses the _PWR_ defines. This allows us to insert defines to give more power
 * granularity and still remain linear with regards to the power usage and
 * full power state transition latency for easy implementation of PM
 * algorithms.
 * The PM actions:
 * _PWR_ERR = Non-valid state.
 * _PWR_OFF_FORCE = _PWR_OFF is forced regardless of standby mechanisms.
 * _PWR_OFF = Device, regulators, clocks, etc is turned off.  The longest
 *            transition time to _PWR_ON is from this state.
 * _PWR_STDBY_OFF = Device is useless but powered.  No communication possible.
 *                  Device does not retain programming.  Main purpose is for
 *                  faster return to _PWR_ON without regulator delays.
 * _PWR_STDBY = Device is in standby.  Device retains programming.
 * _PWR_COMM = Device is powered enough to communicate with the device.
 * _PWR_ON = Device is at full power with active output.
 *
 * The kernel drivers treat these calls as Guaranteed Level Of Service.
 */

enum {
    NVC_PWR_ERR,
    NVC_PWR_OFF_FORCE,
    NVC_PWR_OFF,
    NVC_PWR_STDBY_OFF,
    NVC_PWR_STDBY,
    NVC_PWR_COMM,
    NVC_PWR_ON,
    NVC_PWR_FORCE32 = 0x7fffffff,
};

#define CAMERA_MAX_NAME_LENGTH  32
#define CAMERA_MAX_EDP_ENTRIES  16

#define PCL_KERNEL_NODE "/dev/camera.pcl"

enum {
    CAMERA_SEQ_EXEC,
    CAMERA_SEQ_REGISTER_EXEC,
    CAMERA_SEQ_REGISTER_ONLY,
    CAMERA_SEQ_EXIST,
    CAMERA_SEQ_FORCE32 = 0x7fffffff,
    CAMERA_SEQ_STATUS_MASK = 0xf000000,
};

struct camera_device_info {
    NvU8 name[CAMERA_MAX_NAME_LENGTH];
    NvU32 type;
    NvU8 bus;
    NvU8 addr;
};

struct camera_reg {
    NvU32 addr;
    NvU32 val;
};

struct regmap_cfg {
    int addr_bits;
    int val_bits;
    NvU32 cache_type;
};

struct gpio_cfg {
    int gpio;
    NvU8 own;
    NvU8 active_high;
    NvU8 flag;
    NvU8 reserved;
};

struct edp_cfg {
    uint estates[CAMERA_MAX_EDP_ENTRIES];
    uint num;
    uint e0_index;
    int priority;
};

#define VIRTUAL_DEV_MAX_REGULATORS      8
#define VIRTUAL_DEV_MAX_GPIOS           8
#define VIRTUAL_REGNAME_SIZE            (VIRTUAL_DEV_MAX_REGULATORS * CAMERA_MAX_NAME_LENGTH)

struct virtual_device {
    NvU32 power_on;
    NvU32 power_off;
    struct regmap_cfg regmap_cfg;
    NvU32 bus_type;
    NvU32 gpio_num;
    NvU32 reg_num;
    NvU32 pwr_on_size;
    NvU32 pwr_off_size;
    NvU32 clk_num;
    NvU8 name[32];
    NvU8 reg_names[VIRTUAL_REGNAME_SIZE];
};

enum {
    UPDATE_PINMUX,
    UPDATE_GPIO,
    UPDATE_POWER,
    UPDATE_CLOCK,
    UPDATE_EDP,
    UPDATE_MAX_NUM,
};

struct cam_update {
    NvU32 type;
    NvU32 index;
    NvU32 size;
    NvU32 arg;
};

enum {
    DEVICE_SENSOR,
    DEVICE_FOCUSER,
    DEVICE_FLASH,
    DEVICE_ROM,
    DEVICE_OTHER,
    DEVICE_OTHER2,
    DEVICE_OTHER3,
    DEVICE_OTHER4,
    DEVICE_MAX_NUM,
};

struct cam_device_layout {
    NvU64 guid;
    NvU8 name[CAMERA_MAX_NAME_LENGTH];
    NvU8 type;
    NvU8 alt_name[CAMERA_MAX_NAME_LENGTH];
    NvU8 pos;
    NvU8 bus;
    NvU8 addr;
    NvU8 addr_byte;
    NvU32 dev_id;
    NvU32 index;
    NvU32 reserved1;
    NvU32 reserved2;
};

#endif
/* __CAMERA_H__ */

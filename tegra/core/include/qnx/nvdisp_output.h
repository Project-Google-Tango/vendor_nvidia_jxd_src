/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __QNX_NVDISP_OUTPUT_H
#define __QNX_NVDISP_OUTPUT_H

#include <devctl.h>
#define NVDISP_MAX_OUTPUTS    (2)

enum nvdisp_display_type {
    NVDISP_LCD,
    NVDISP_HDMI,
};

/* Valid for NVDISP_LCD interface only */
enum nvdisp_pin_polarity {
    NVDISP_ACTIVE_HIGH,
    NVDISP_ACTIVE_LOW,
};

#define NVDISP_MAX_PINS             (4)
#define HDMI_HPD_DEV_NODE_LEN       (64)
#define DDC_I2C_DEV_NODE_LEN        (64)
enum nvdisp_pin_func {
    NVDISP_HSYNC = 1,
    NVDISP_VSYNC,
    NVDISP_PCLK,
    NVDISP_DATAEN,
};

struct nvdisp_pin {
    enum nvdisp_pin_func pin;
    enum nvdisp_pin_polarity polarity;
};

enum nvdisp_hotplug_level {
    NVDISP_HOTPLUG_LEVEL_HIGH = 1,
    NVDISP_HOTPLUG_LEVEL_LOW,
};

struct nvdisp_output {
    enum nvdisp_display_type disp;
    unsigned int hActive;
    unsigned int vActive;
    unsigned int hSyncWidth;
    unsigned int vSyncWidth;
    unsigned int hFrontPorch;
    unsigned int vFrontPorch;
    unsigned int hBackPorch;
    unsigned int vBackPorch;
    unsigned int pclkKHz;
    unsigned int bpp;
    struct nvdisp_pin pins[NVDISP_MAX_PINS];
    char hdmi_hpd[HDMI_HPD_DEV_NODE_LEN];
    char ddc_i2c[DDC_I2C_DEV_NODE_LEN];
    enum nvdisp_hotplug_level hotplug_level;
};

#define NVDISP_CTRL_REGISTER_OUTPUTS    \
    (int)__DIOT(_DCMD_MISC, 3, unsigned int)

#endif /* __QNX_NVDISP_OUTPUT_H */

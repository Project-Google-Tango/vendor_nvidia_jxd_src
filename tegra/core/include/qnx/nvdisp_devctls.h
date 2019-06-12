/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __QNX_NVDISP_DEVCTL_H
#define __QNX_NVDISP_DEVCTL_H

#include <nvdc.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <devctl.h>
#include <qnx/nvdisp_output.h>

#define NVDISP_GET_WINDOW            \
    (int)__DIOT(_DCMD_MISC, 1, unsigned int)

#define NVDISP_PUT_WINDOW            \
    (int)__DIOT(_DCMD_MISC, 2, unsigned int)

#define NVDISP_FLIP                \
    (int)__DIOTF(_DCMD_MISC, 3, struct nvdcFlipArgs)

#define NVDISP_GET_MODE                \
    (int)__DIOF(_DCMD_MISC, 4, struct nvdcMode)

#define NVDISP_SET_MODE                \
    (int)__DIOT(_DCMD_MISC, 5, struct nvdcMode)

#define NVDISP_SET_LUT                \
    (int)__DIOT(_DCMD_MISC, 6, struct nvdisp_lut)

#define NVDISP_CONFIG_CRC            \
    (int)__DIOT(_DCMD_MISC, 7, bool)

#define NVDISP_GET_CRC            \
    (int)__DIOF(_DCMD_MISC, 8, unsigned int)

#define NVDISP_GET_STATUS            \
    (int)__DIOF(_DCMD_MISC, 9, unsigned int)

#define NVDISP_VALIDATE_MODE            \
    (int)__DIOTF(_DCMD_MISC, 10, struct nvdcMode)

#define NVDISP_SET_CSC                \
    (int)__DIOT(_DCMD_MISC, 11, struct nvdisp_csc)

struct nvdisp_lut {
    int window;
    unsigned int start;
    unsigned int len;
    /*
     * Following arrays have type char since current hardware only supports
     * 8 bit color values (note "struct nvdc" uses bloated type NvU16).
     */
    char r[256];
    char g[256];
    char b[256];
};

struct nvdisp_csc {
    int window;
    uint16_t yof;
    uint16_t kyrgb;
    uint16_t kur;
    uint16_t kvr;
    uint16_t kug;
    uint16_t kvg;
    uint16_t kub;
    uint16_t kvb;
};

struct nvdisp_output_prop {
    enum nvdcDisplayType type;
    unsigned int handle;
    bool connected;
    int assosciated_head;
    unsigned int head_mask;
};

#define NVDISP_CTRL_GET_NUM_OUTPUTS        \
    (int)__DIOF(_DCMD_MISC, 1, unsigned int)

#define NVDISP_CTRL_GET_OUTPUT_PROPERTIES    \
    (int)__DIOTF(_DCMD_MISC, 2, struct nvdisp_output_prop)

#define NVDISP_CTRL_QUERY_EDID    \
    (int)__DIOTF(_DCMD_MISC, 3, struct nvdcEdid)

#endif /* __QNX_NVDISP_DEVCTL_H */

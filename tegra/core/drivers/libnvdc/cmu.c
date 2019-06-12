/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/tegra_dc_ext.h>
#include <nvdc.h>

#include "nvdc_priv.h"

int nvdcSetCmu(struct nvdcState *state, int head, struct nvdcCmu *cmu)
{
    int i;
    int ret;

    struct tegra_dc_ext_cmu args;

    args.cmu_enable = cmu->cmu_enable;
    for (i = 0; i < 256; i++) {
        args.lut1[i] = cmu->lut1[i];
    }

    for (i = 0; i < 9; i++) {
        args.csc[i] = cmu->csc[i];
    }

    for (i = 0; i < 960; i++) {
        args.lut2[i] = cmu->lut2[i];
    }

    ret = ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_SET_CMU, &args);
    if (ret) {
        return errno;
    }

    return 0;
}

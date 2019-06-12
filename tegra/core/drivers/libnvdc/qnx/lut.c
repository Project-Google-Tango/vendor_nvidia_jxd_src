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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <qnx/nvdisp_devctls.h>
#include <nvdc.h>
#include "nvdc_priv.h"

int nvdcAllocLut(struct nvdcLut *lut, unsigned int len)
{
    unsigned int i;

    if (lut == NULL) {
        return EINVAL;
    }

    if (len > 256) {
        nvdc_error("%s failed: too many (%u) LUT entries\n", __func__, len);
        return EINVAL;
    }

    memset(lut, 0, sizeof(*lut));
    lut->len = len;
    lut->r = malloc(len * sizeof(lut->r[0]));
    lut->g = malloc(len * sizeof(lut->g[0]));
    lut->b = malloc(len * sizeof(lut->b[0]));

    if (lut->r == NULL || lut->g == NULL || lut->b == NULL) {
        nvdcFreeLut(lut);
        nvdc_error("%s failed: could not allocate for LUT components\n", __func__);
        return ENOMEM;
    }

    /*
     * Initialize the newly allocated arrays with the identity lookup table (an
     * identity LUT maps color value C onto C itself; thereby leaving C unchanged).
     * The low-byte is replicated in the high-byte in case 16 bit will be supported
     * in future hardware (Tegra 2 and 3 hardware only supports 8 bit).
     */
    for (i = 0; i < len; i++) {
        lut->r[i] =
        lut->g[i] =
        lut->b[i] = i | (i << 8);
    }

    return 0;
}

void nvdcFreeLut(struct nvdcLut *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->b) {
        free(lut->b);
    }
    if (lut->g) {
        free(lut->g);
    }
    if (lut->r) {
        free(lut->r);
    }
    memset(lut, 0, sizeof(*lut));
}

int nvdcSetLut(struct nvdcState *state, int head, int window,
               const struct nvdcLut *lut)
{
    int ret = 0;
    unsigned int i = 0;
    struct nvdisp_lut nvdispLUT;

    if (state == NULL || lut == NULL) {
       return EINVAL;
    }

    if (!NVDC_VALID_HEAD(head)) {
        nvdc_error("%s failed: not a valid head\n", __func__);
        return EINVAL;
    }

    /*
     * Prepare an ad-hoc struct with a trimmed version of the data before
     * sending it to the display manager.
     *
     * Note lut->flags is not used in the QNX impl. It is ignored because there
     * is no Linux frame buffer device to override and NVDC_LUT_FLAGS_FBOVERRIDE
     * was the only valid flag when this was implemenetd.
     */
    nvdispLUT.window = window;
    nvdispLUT.start = lut->start;
    nvdispLUT.len = lut->len;

    /*
     * The LSB of the nvdc struct is copied into an ad-hoc struct that is
     * used to communicate the look-up table values to the display manager driver
     * (in process devg-nvrm on QNX).
     */
    for (i = 0; i < nvdispLUT.len; ++i) {
        /*
         * Note that only 8 bit color values are supported in current tegra hardware.
         * Make an attempt at asserting this assumption continues to hold.
         */
        assert((lut->r[i] & 0xFF) == (lut->r[i] >> 8));
        assert((lut->g[i] & 0xFF) == (lut->g[i] >> 8));
        assert((lut->b[i] & 0xFF) == (lut->b[i] >> 8));
        nvdispLUT.r[i] = (char)lut->r[i];
        nvdispLUT.g[i] = (char)lut->g[i];
        nvdispLUT.b[i] = (char)lut->b[i];
    }

    ret = devctl(state->dispFd[head], NVDISP_SET_LUT, (void*)&nvdispLUT, sizeof(nvdispLUT), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl NVDISP_SET_LUT failure\n", __func__);
        return ret;
    }

    return 0;
}

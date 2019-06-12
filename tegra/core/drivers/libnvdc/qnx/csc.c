/*
 * Copyright (c) 2012 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <nvdc.h>
#include <qnx/nvdisp_devctls.h>
#include <math.h>
#include "nvdc_priv.h"

static inline float clamp(float val, float min, float max)
{
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}
/*
 * Pack the floating-point value val into a fixed-point integer with one
 * optional signed bit and the specified integer and fractional component
 * widths.
 */
static inline uint16_t pack(float val, int sign, int intbits, int fracbits)
{
    const float scale_factor = (1 << fracbits);
    float max, min;
    float scaled;
    int16_t packed;

    scaled = roundf(val * scale_factor);

    /* clamp to valid values */
    max = (float)((1 << (intbits + fracbits)) - 1);
    if (sign) {
        min = (float)-(1 << (intbits + fracbits));
    } else {
        min = 0.0;
    }
    scaled = clamp(scaled, min, max);

    packed = (int16_t)scaled;

    packed &= ((1 << (!!sign + intbits + fracbits)) - 1);

    return (uint16_t)packed;
}

int nvdcSetCsc(struct nvdcState *state, int head, int window,
               const struct nvdcCsc *csc)
{
    int ret;
    float consat, coshue, sinhue, bricon;
    const float *matrix;
    static const float matrix_601[] =
        { 1.1644,  1.5960, -0.3918, -0.8130, 2.0172 };
    static const float matrix_709[] =
        { 1.1644,  1.7927, -0.2132, -0.5329, 2.1124 };
    float brightness, contrast, saturation, hue;
    float yof, kyrgb, kur, kvr, kug, kvg, kub, kvb;
    struct nvdisp_csc _csc;

    if (!NVDC_VALID_HEAD(head) || (csc == NULL) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    switch (csc->colorimetry) {
    case NVDC_COLORIMETRY_601:
        matrix = matrix_601;
        break;
    case NVDC_COLORIMETRY_709:
        matrix = matrix_709;
        break;
    default:
        return EINVAL;
    }

    brightness = clamp(csc->brightness, -0.5, 0.5);
    contrast =   clamp(csc->contrast,    0.1, 2.0);
    saturation = clamp(csc->saturation,  0.0, 2.0);
    hue        = clamp(csc->hue,       -M_PI, M_PI);

    /* Calculate ideal floating-point values for hardware coeffs */
    consat = contrast * saturation;
    coshue = cosf(hue) * consat;
    sinhue = sinf(hue) * consat;
    bricon = brightness / contrast;

    yof = -16.0 + (bricon * 255.0);
    kyrgb = contrast * matrix[0];

    kur = -matrix[1] * sinhue;
    kvr = matrix[1] * coshue;

    kug = (matrix[2] * coshue) - (matrix[3] * sinhue);
    kvg = (matrix[3] * coshue) + (matrix[2] * sinhue);

    kub = matrix[4] * coshue;
    kvb = matrix[4] * sinhue;

    /* Convert ideal floating-point values into fixed-point */
    _csc.window = window;
    _csc.yof    = pack(yof,   1, 7, 0);
    _csc.kyrgb  = pack(kyrgb, 0, 2, 8);
    _csc.kur    = pack(kur,   1, 2, 8);
    _csc.kvr    = pack(kvr,   1, 2, 8);
    _csc.kug    = pack(kug,   1, 1, 8);
    _csc.kvg    = pack(kvg,   1, 1, 8);
    _csc.kub    = pack(kub,   1, 2, 8);
    _csc.kvb    = pack(kvb,   1, 2, 8);

    ret = devctl(state->dispFd[head], NVDISP_SET_CSC, &_csc, sizeof(_csc), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n", __func__);
    }

    return ret;
}

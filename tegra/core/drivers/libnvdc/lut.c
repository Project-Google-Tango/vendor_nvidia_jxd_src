#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/tegra_dc_ext.h>
#include <nvdc.h>
#include "nvdc_priv.h"

int nvdcAllocLut(struct nvdcLut* lut, unsigned int len)
{
    unsigned int i;

    if (len > 256) {
        return EINVAL;
    }

    memset(lut, 0, sizeof(struct nvdcLut));
    lut->len = len;
    lut->r = malloc(len * sizeof(lut->r[0]));
    lut->g = malloc(len * sizeof(lut->g[0]));
    lut->b = malloc(len * sizeof(lut->b[0]));

    if (!lut->r || !lut->g || !lut->b) {
        nvdcFreeLut(lut);
        return ENOMEM;
    }

    for(i=0; i<len; i++)
    {
        lut->r[i] =
        lut->g[i] =
        lut->b[i] = i|(i<<8);
    }

    return 0;
}

void nvdcFreeLut(struct nvdcLut* lut)
{
    free(lut->b);
    free(lut->g);
    free(lut->r);
    memset(lut, 0, sizeof(struct nvdcLut));
}

int nvdcSetLut(struct nvdcState *state, int head, int window,
               const struct nvdcLut *lut)
{
    int ret;
    struct tegra_dc_ext_lut hwLut;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    memset(&hwLut, 0, sizeof(hwLut));
    hwLut.win_index = window;
    hwLut.start = lut->start;
    hwLut.len = lut->len;
    hwLut.r = lut->r;
    hwLut.g = lut->g;
    hwLut.b = lut->b;

    if (lut->flags & NVDC_LUT_FLAGS_FBOVERRIDE)
        hwLut.flags |= TEGRA_DC_EXT_LUT_FLAGS_FBOVERRIDE;

    ret = ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_SET_LUT, &hwLut);
    if (ret) {
        return errno;
    }

    return 0;
}


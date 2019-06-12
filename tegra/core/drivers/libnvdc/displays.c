#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/tegra_dc_ext.h>

#include <nvdc.h>

#include "nvdc_priv.h"

int nvdcQueryNumHeads(struct nvdcState *state)
{
    int i = 0;

    while (state->tegraExtFd[i] >= 0 && i < NVDC_MAX_HEADS) {
        i++;
    }

    return i;
}

static int kernelToLibType(int kernel, enum nvdcDisplayType *lib)
{
    enum nvdcDisplayType type;

    switch (kernel) {
    case TEGRA_DC_EXT_DSI:
        type = NVDC_DSI;
        break;
    case TEGRA_DC_EXT_LVDS:
        type = NVDC_LVDS;
        break;
    case TEGRA_DC_EXT_VGA:
        type = NVDC_VGA;
        break;
    case TEGRA_DC_EXT_HDMI:
        type = NVDC_HDMI;
        break;
    case TEGRA_DC_EXT_DVI:
        type = NVDC_DVI;
        break;
    case TEGRA_DC_EXT_DP:
        type = NVDC_DP;
        break;
    default:
        return EINVAL;
    }

    *lib = type;

    return 0;
}

/*
 * Initialize the displays array.  Called once at startup, outputs aren't
 * expected to change
 */
int _nvdcInitOutputInfo(struct nvdcState *state)
{
    unsigned int num, i;
    int ret;

    ret = ioctl(state->tegraCtrlFd, TEGRA_DC_EXT_CONTROL_GET_NUM_OUTPUTS, &num);
    if (ret) {
        return errno;
    }

    if (num > NVDC_MAX_OUTPUTS) {
        return E2BIG;
    }
    state->numOutputs = num;

    for (i = 0; i < num; i++) {
        struct tegra_dc_ext_control_output_properties prop = { };

        prop.handle = i;

        ret = ioctl(state->tegraCtrlFd,
                    TEGRA_DC_EXT_CONTROL_GET_OUTPUT_PROPERTIES,
                    &prop);
        if (ret) {
            return errno;
        }

        ret = kernelToLibType(prop.type, &state->displays[i].type);
        if (ret) {
            return ret;
        }
        state->displays[i].dcHandle = i;
    }

    return 0;
}

int nvdcQueryDisplays(struct nvdcState *state,
                      nvdcDisplayHandle **pDisplays,
                      int *nDisplays)
{
    nvdcDisplayHandle *displays;
    unsigned int num = state->numOutputs, i;

    displays = malloc(sizeof(*displays) * num);

    if (!displays) {
        return ENOMEM;
    }

    for (i = 0; i < num; i++) {
        /*
         * These are opaque handles to clients.  We just use a pointer into our
         * displays array for ease of fetching it back out later.
         */
        displays[i] = &state->displays[i];
    }

    *nDisplays = num;
    *pDisplays = displays;

    return 0;
}

int nvdcQueryDisplayInfo(struct nvdcState *state,
                         struct nvdcDisplay *display,
                         struct nvdcDisplayInfo *info)
{
    struct tegra_dc_ext_control_output_properties prop = { };
    int ret;

    prop.handle = display->dcHandle;
    ret = ioctl(state->tegraCtrlFd,
                TEGRA_DC_EXT_CONTROL_GET_OUTPUT_PROPERTIES,
                &prop);
    if (ret) {
        return errno;
    }

    ret = kernelToLibType(prop.type, &info->type);
    if (ret) {
        return ret;
    }
    info->boundHead = prop.associated_head;
    info->connected = prop.connected;
    info->headMask = prop.head_mask;

    return 0;
}

int nvdcQueryDisplayEdid(struct nvdcState *state,
                         struct nvdcDisplay *display,
                         void **data,
                         size_t *len)
{
    struct tegra_dc_ext_control_output_edid edid = { };
    char *malloc_data = NULL;
    int ret, size = 0;

retry:
    edid.handle = display->dcHandle;
    if (size) {
        char *new_data = realloc(malloc_data, size);
        if (!new_data) {
            free(malloc_data);
            return ENOMEM;
        }
        malloc_data = new_data;
        memset(malloc_data, 0, size);
    }
    edid.size = size;
    edid.data = malloc_data;

    ret = ioctl(state->tegraCtrlFd, TEGRA_DC_EXT_CONTROL_GET_OUTPUT_EDID,
                &edid);
    if (ret) {
        if (errno == EFBIG) {
            size = edid.size;
            goto retry;
        }
        free(malloc_data);
        return errno;
    }

    *data = malloc_data;
    *len = edid.size;

    return 0;
}

int nvdcDisplayBind(struct nvdcState *state,
                    struct nvdcDisplay *display,
                    int head)
{
    /*
     * XXX flesh this out with an actual implementation.  Right now we assume
     * that DC handle n is permanently bound to head n.
     */
    return (head == display->dcHandle) ? 0 : ENOANO;
}

int nvdcDisplayUnbind(struct nvdcState *state,
                      struct nvdcDisplay *display,
                      int head)
{
    /*
     * XXX flesh this out with an actual implementation.  Right now we assume
     * that DC handle n is permanently bound to head n, unbinding is not
     * supported.
     */
    return ENOANO;
}

int nvdcQueryVblankSyncpt(struct nvdcState *state, int head,
                          unsigned int *syncpt)
{
    unsigned int sp;
    int ret;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    ret = ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_GET_VBLANK_SYNCPT, &sp);
    if (ret) {
        return errno;
    }

    *syncpt = sp;

    return 0;
}

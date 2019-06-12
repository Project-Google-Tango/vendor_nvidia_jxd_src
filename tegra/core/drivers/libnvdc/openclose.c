#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/tegra_dc_ext.h>

#include <nvdc.h>

#include "nvdc_priv.h"

#define MAX_PATH_LEN 64
#define TEGRA_EXT_PATH_FMT "/dev/tegra_dc_%d"
#ifdef ANDROID
# define FB_PATH_FMT "/dev/graphics/fb%d"
#else
# define FB_PATH_FMT "/dev/fb%d"
#endif
static const char TEGRA_CTRL_PATH[] = "/dev/tegra_dc_ctrl";

static int setNvmapFd(struct nvdcState *state)
{
    int i;

    if (!state->nvmapUserFd) {
        return 0;
    }

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        int ret;

        if (state->tegraExtFd[i] < 0) {
            break;
        }

        ret = ioctl(state->tegraExtFd[i], TEGRA_DC_EXT_SET_NVMAP_FD,
                    state->nvmapUserFd);

        if (ret) {
            return ret;
        }
    }

    return 0;
}

nvdcHandle nvdcOpen(int nvmapFd)
{
    struct nvdcState *state;
    int i = 0, ret;

    state = calloc(sizeof(*state), 1);
    if (!state) {
        return NULL;
    }

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        state->tegraExtFd[i] =
        state->fbFd[i] = -1;
    }

    state->tegraCtrlFd = open(TEGRA_CTRL_PATH, O_RDWR);
    if (state->tegraCtrlFd < 0) {
        perror("nvdc: open");
        fprintf(stderr, "nvdc: failed to open '%s'.\n", TEGRA_CTRL_PATH);
        goto cleanup;
    }

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        char path[MAX_PATH_LEN];

        snprintf(path, MAX_PATH_LEN, TEGRA_EXT_PATH_FMT, i);
        path[MAX_PATH_LEN - 1] = '\0';

        state->tegraExtFd[i] = open(path, O_RDWR);
        if (state->tegraExtFd[i] < 0) {
            if (errno == ENOENT) {
                break;
            } else {
                perror("nvdc: open");
                fprintf(stderr, "nvdc: failed to open '%s'.\n", path);
                goto cleanup;
            }
        }

        snprintf(path, MAX_PATH_LEN, FB_PATH_FMT, i);
        path[MAX_PATH_LEN - 1] = '\0';

        state->fbFd[i] = open(path, O_RDWR);
        if (state->fbFd[i] < 0) {
            perror("nvdc: open");
            fprintf(stderr, "nvdc: failed to open '%s'.\n", path);
            goto cleanup;
        }
    }

    state->nvmapUserFd = nvmapFd;

    ret = setNvmapFd(state);
    if (ret) {
        fprintf(stderr, "nvdc: failed to set nvmap fd: %s\n", strerror(ret));
        goto cleanup;
    }

    ret = _nvdcInitOutputInfo(state);
    if (ret) {
        fprintf(stderr, "nvdc: failed to get output info: %s\n", strerror(ret));
        goto cleanup;
    }

    ret = _nvdcInitCaps(state);
    if (ret) {
        fprintf(stderr, "nvdc: failed to get capabilities: %s\n",
                strerror(ret));
        goto cleanup;
    }

    return state;

cleanup:
    nvdcClose(state);

    return NULL;
}

void nvdcClose(struct nvdcState *state)
{
    int i;

    if (!state) {
        return;
    }

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        if (state->tegraExtFd[i] >= 0) {
            close(state->tegraExtFd[i]);
        }
        if (state->fbFd[i] >= 0) {
            close(state->fbFd[i]);
        }
    }
    if (state->tegraCtrlFd >= 0) {
        close(state->tegraCtrlFd);
    }

    free(state);
}

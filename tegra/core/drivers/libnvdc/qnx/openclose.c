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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <qnx/nvdisp_devctls.h>
#include <nvdc.h>

#include "nvdc_priv.h"

#define MAX_PATH_LEN 64
#define NVDISP_CTRL_DEV_PATH NVDISP_DEV_PREFIX"ctrl"

#if 0 /* not used? */
static int setNvmapFd(struct nvdcState *state)
{
    /*
     * FIXME: QNX driver still relies on NvRmMem API for all operations. Once,
     * we switch driver to use native nvmap calls, this needs to be handled.
     */
    return 0;
}
#endif

nvdcHandle nvdcOpen(int nvmapFd)
{
    struct nvdcState *state = NULL;
    int i;

    state = calloc(sizeof(*state), 1);
    if (!state) {
        return NULL;
    }

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        state->dispFd[i] = -1;
    }

    state->ctrlFd = open(NVDISP_CTRL_DEV_PATH, O_RDWR);
    if (state->ctrlFd < 0) {
        nvdc_error("%s: failed to open %s\n", __func__, NVDISP_CTRL_DEV_PATH);
        goto cleanup;
    }
    nvdc_info("%s: opened (fd = %d)\n", NVDISP_CTRL_DEV_PATH, state->ctrlFd);

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        char path[MAX_PATH_LEN];
        snprintf(path, MAX_PATH_LEN, NVDISP_DEV_PREFIX"%s""%d", "disp", i);
        state->dispFd[i] = open(path, O_RDWR);
        if (state->dispFd[i] < 0) {
            if (errno == ENOENT) {
                break;
            } else {
                nvdc_error("%s: failed to open %s\n",__func__, path);
                goto cleanup;
            }
        }
        nvdc_info("%s: opened (fd = %d)\n", path, state->dispFd[i]);
    }

    if (_nvdcInitOutputInfo(state)) {
        nvdc_error("%s: failed to get ouput info\n",__func__);
        goto cleanup;
    }

    nvdc_info("%s passed\n",__func__);
    return state;

cleanup:
    if (state->ctrlFd >= 0)
        close(state->ctrlFd);

    for (i = 0; i < NVDC_MAX_HEADS; i++)
        if (state->dispFd[i] >= 0)
            close(state->dispFd[i]);

    free(state);

    return NULL;
}

void nvdcClose(struct nvdcState *state)
{
    int i;

    if (!state) {
        return;
    }

    for (i = 0; i < NVDC_MAX_HEADS; i++) {
        if (state->dispFd[i] >= 0) {
            close(state->dispFd[i]);
        }
    }

    if (state->ctrlFd >= 0) {
        close(state->ctrlFd);
    }

    free(state);
    nvdc_info("%s passed\n",__func__);
}
